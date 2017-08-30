/* 
 * An implementation of light-weight thread library.
 * 
 * Author: Yitian Huang, huangyitian@gwu.edu, 2017
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

 
#include "lwt.h"
#include "kalloc.h"

lwt_t	current_thread;				/* tracking current active thread					*/
uint	lwt_tid_counter 	= 1;	/* global threadId tracker							*/

/* see in csci_6411_lwt.pdf */
static inline void __lwt_schedule (void);
static inline void __lwt_trampoline();
static inline void __lwt_dispatch(lwt_t n, lwt_t c);


__attribute__((constructor)) static void
lwt_init(void)
{
	kinit();
	
	current_thread = (lwt_t)kalloc(sizeof(struct lwt));
	ps_list_init_d(current_thread);

	current_thread->state = RUNNABLE;
	current_thread->ct.ip = (ulong)0;
	current_thread->ct.sp = (ulong)NULL;
	current_thread->tid = lwt_tid_counter++;

	LWT_INFO_NTHD_RUNNABLE++;	
}


lwt_t
lwt_create(lwt_fn_t fn, void* data, lwt_flags_t flags)
{
	lwt_t t = (lwt_t)kalloc(sizeof(struct lwt));
	assert(t != NULL);

	t->tid = ++lwt_tid_counter;
	t->ct.ip = (ulong)&__lwt_trampoline;
	t->ct.sp = (ulong)(t->stack + STACK_SIZE);
	t->state = RUNNABLE;
	t->fn = fn;
	t->data = data;
	t->flag = flags;

	LWT_INFO_NTHD_RUNNABLE++;
	LWT_INFO_NTHD_ZOMBIES++;

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
	
	/* Set up join relation */
	current_thread->joiningto = t;
	current_thread->joiningto->joinedfrom = current_thread;
		
	/* !important
	 * It's possible that the child is already died before the parent is executed.
	 */
	if(t->state == DEAD){
		LWT_INFO_NTHD_ZOMBIES--;
		LWT_INFO_NTHD_RUNNABLE--;
		tmp = t->ret;
		ps_list_rem_d(t);
		kfree((char*)t,sizeof(struct lwt));
		return tmp;
	}
	
	current_thread->state = BLOCKED;
	
	while(current_thread->state == BLOCKED){
		LWT_INFO_NTHD_RUNNABLE--;
		LWT_INFO_NTHD_BLOCKED++;
		lwt_yield(t);
	}
	
	LWT_INFO_NTHD_RUNNABLE++;
	LWT_INFO_NTHD_BLOCKED--;

	LWT_INFO_NTHD_RUNNABLE--;
	LWT_INFO_NTHD_ZOMBIES--;
	
	tmp = t->ret;
	ps_list_rem_d(t);
	kfree((char*)t,sizeof(struct lwt));
	return tmp;
}

void
lwt_die (void* data)
{
	if(current_thread->flag == LWT_NOJOIN){
		lwt_t tmp = current_thread;

		do{
			current_thread = ps_list_next_d(current_thread);
		}while(current_thread->state != RUNNABLE);

		ps_list_rem_d(tmp);
		//kfree((char*)tmp,sizeof(struct lwt));
	
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

static inline void
__lwt_schedule(void){
	lwt_t temp; 
	
	/* No Runnable threads */
	if(unlikely(lwt_info(LWT_INFO_NTHD_RUNNABLE) == 0)){ 
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
	
	//current_thread = next_thread;
	return;	
}

static inline void __lwt_trampoline()
{
	void* ret;
	//current_thread = next_thread;


	ret = current_thread->fn(current_thread->data);
	
	lwt_die(ret);
}
