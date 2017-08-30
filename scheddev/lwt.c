/* 
 * An implementation of light-weight thread library.
 * 
 * Author: Yitian Huang, huangyitian@gwu.edu, 2017
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

 
#include <lwt.h>
#include <kalloc.h>


lwt_t			current_thread;				/* tracking current active thread, also know as head thread */
unsigned int	lwt_tid_counter 	= 1;	/* global threadId tracker									*/

/* see in csci_6411_lwt.pdf */
static inline void __lwt_schedule (void);
static inline void __lwt_trampoline();
static inline void __lwt_dispatch(lwt_t n, lwt_t c);


#define PS_CAS(t,d,v) do {		\
	t = d;						\
}while(!ps_cas(&d,t,t+v))



void
lwt_init()
{	
	int cas_t;
	current_thread = (lwt_t)kalloc(sizeof(struct lwt) + 100);
	ps_list_init_d(current_thread);

	current_thread->state = RUNNABLE;
	current_thread->ct.ip = (unsigned long)0;
	current_thread->ct.sp = (unsigned long)NULL;
	current_thread->kid = cos_thdid();

	current_thread->tid = lwt_tid_counter;
	PS_CAS(cas_t,lwt_tid_counter,1);

	
	PS_CAS(cas_t,LWT_INFO_NTHD_RUNNABLE,1);
}


lwt_t
lwt_create(lwt_fn_t fn, void* data, lwt_flags_t flags)
{	
	int cas_t;

	lwt_t t = (lwt_t)kalloc(sizeof(struct lwt)+ 100);
	assert(t != NULL);

	t->tid = lwt_tid_counter;
	PS_CAS(cas_t,lwt_tid_counter,1);

	t->ct.ip = (unsigned long)&__lwt_trampoline;
	t->ct.sp = (unsigned long)(t->stack + STACK_SIZE);
	t->state = RUNNABLE;
	t->fn = fn;
	t->data = data;
	t->flag = flags;
	t->kid = current_thread->kid;


	PS_CAS(cas_t,LWT_INFO_NTHD_RUNNABLE,1);
	PS_CAS(cas_t,LWT_INFO_NTHD_ZOMBIES,1);

	ps_list_add_d(ps_list_prev_d(current_thread),t);
	

	return t;
}

void*
lwt_join (lwt_t t)
{	
	assert(t);

	if(t->flag == LWT_NOJOIN){
		return NULL;
	}

	void* tmp;
	int cas_t;
	
	/* Set up join relation */

	current_thread->joiningto = t;
	current_thread->joiningto->joinedfrom = current_thread;
		
	/* !important
	 * It's possible that the child is already died before the parent is executed.
	 */
	if(t->state == DEAD){
		PS_CAS(cas_t,LWT_INFO_NTHD_ZOMBIES,-1);
		PS_CAS(cas_t,LWT_INFO_NTHD_RUNNABLE,-1);

		tmp = t->ret;
		ps_list_rem_d(t);
		kfree((char*)t,sizeof(struct lwt));
		return tmp;
	}
	
	current_thread->state = BLOCKED;
	
	while(current_thread->state == BLOCKED){
		PS_CAS(cas_t,LWT_INFO_NTHD_RUNNABLE,-1);
		PS_CAS(cas_t,LWT_INFO_NTHD_BLOCKED,1);
		lwt_yield(t);
	}
	
	PS_CAS(cas_t,LWT_INFO_NTHD_RUNNABLE,1);
	PS_CAS(cas_t,LWT_INFO_NTHD_BLOCKED,-1);
	PS_CAS(cas_t,LWT_INFO_NTHD_RUNNABLE,-1);
	PS_CAS(cas_t,LWT_INFO_NTHD_ZOMBIES,-1);

	
	tmp = t->ret;
	ps_list_rem_d(t);
	kfree((char*)t,sizeof(struct lwt));
	return tmp;
}

void
lwt_die(void* data)
{	
	if(current_thread->flag == LWT_NOJOIN){
		lwt_t tmp = current_thread;

		do{
			current_thread = ps_list_next_d(current_thread);
		}while(current_thread->state != RUNNABLE);

		ps_list_rem_d(tmp);
		kfree((char*)tmp,sizeof(struct lwt));
		
		//lwt_t trash;

		__lwt_dispatch(current_thread, tmp);

		goto done;

	}

	current_thread->state = DEAD;
	/* cause Zombie thread when current_thread->joinedFrom == NULL */
	if(current_thread->joinedfrom != NULL){
		current_thread->joinedfrom->state = RUNNABLE;
	}
	
	/* track return value */
	current_thread->ret = (void*)data;

	lwt_yield(LWT_NULL);

done:
	return;
}

int
lwt_yield (lwt_t t)
{
	if(t == LWT_NULL){
		__lwt_schedule();
	}
	else if(t->state == RUNNABLE){
		/* directed yield */
		lwt_t tmp = current_thread;
		current_thread = t;

		__lwt_dispatch(current_thread, tmp);
	}
	
	return 0;
}

lwt_t
lwt_current (void)
{
	return current_thread;
}

int
lwt_id (lwt_t t)
{
	assert(t != NULL);
	return t->tid; 
}

int
lwt_info(lwt_info_t t)
{
	return t;
} 


void
lwt_schedule(void){
	__lwt_schedule();
}

static inline void
__lwt_schedule(void){
	lwt_t temp; 
	
	/* No Runnable threads */
	if(lwt_info(LWT_INFO_NTHD_RUNNABLE) == 0){ 
		return;
	}
		
	/* Find next RUNNABLE thread */
	temp = current_thread;

	do{
		current_thread = ps_list_next_d(current_thread);
	}while(current_thread->state != RUNNABLE);

	if (temp != current_thread){
		
		__lwt_dispatch(current_thread, temp);

	}

	return;	
}

static inline void
__lwt_dispatch(lwt_t next, lwt_t curr)	
{
	__asm__ __volatile__(
		"cld\n\t"							/* clearing the direction bit of the flags register */
		"pushal\n\t"						/* save all current registers to stack */
		"movl $0f, (%%eax)\n\t"				/* save return address to curr->ip */
		"movl %%esp, 0x4(%%eax)\n\t"		/* save esp to curr->ip, refer to struct lwt_context */
		"movl 0x4(%%ebx), %%esp\n\t"		/* load next->sp to esp */
		"jmp *(%%ebx)\n\t"					/* jump to the next->ip */
		"0:\t"								/* addr where $0f refers to */
		"popal"								/* restore stack */
		:: "a"(&curr->ct), "b"(&next->ct)
		: "cc", "memory","%esp"				/* clobber list */
	);

	return;	
}

static inline void __lwt_trampoline()
{
	void* ret;

	ret = current_thread->fn(current_thread->data);

	lwt_die(ret);
}
