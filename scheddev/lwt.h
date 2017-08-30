#ifndef LWT_H
#define LWT_H

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>

#include <sys/types.h>
#include <queue.h>
#include <ps_list.h>
#include <sl.h>

//#define likely(x)	__builtin_expect((x),1)
//#define unlikely(x) __builtin_expect((x),0)

/* Thread States */
typedef enum {
	RUNNABLE	= 0,
	DEAD		= 1,
	BLOCKED 	= 2,
	WAITING		= 3
}lwt_state;

/* (no-)join state */
typedef enum {
	LWT_DEFAULT = 0,
	LWT_NOJOIN	= 1
}lwt_flags_t;

#ifndef STACK_SIZE
	#define STACK_SIZE	512
#endif

#define LWT_NULL  	(NULL)

typedef void*         (*lwt_fn_t)(void*);


/* Ethan:
 * I know I shouldn't use up-case characters as variable names.
 * The reason I insist to define variables as below is for the 
 * convenience of implementation of lwt_info().
 * 
 * By doing this, when you call lwt_info(LWT_INFO_NTHD_RUNNABLE),
 * you can simply return LWT_INFO_NTHD_RUNNABLE inside the function,
 * instead a bunch of switch-cases statements.
 */
typedef int lwt_info_t;
lwt_info_t LWT_INFO_NTHD_RUNNABLE;	
lwt_info_t LWT_INFO_NTHD_ZOMBIES;	
lwt_info_t LWT_INFO_NTHD_BLOCKED;	
lwt_info_t LWT_INFO_NCHAN;			
lwt_info_t LWT_INFO_NSNDING;		
lwt_info_t LWT_INFO_NRCVING;		

struct lwt_context {
	unsigned long ip;
	unsigned long sp;
};

typedef struct lwt {
	/* FIXME:
	 * here is how this bug works: if you put member tid at the first position, then the value of tid will
	 * be modified during execution. 
	 * I'm not able to figure out why, but the tid CANNOT be the first element here. So as other elements, so I 
	 * a fixfix here...And its values is changeable as I said 
	 */
	char 				fix[128];
	
	char 				stack[STACK_SIZE];	/* stack		*/
	struct lwt_context	ct;					/* ip, sp		*/

	void*				ret;				/* return value	*/
	
	struct ps_list 		list;
	
	struct lwt*			joinedfrom;			/* tracking joinning relations */
	struct lwt*			joiningto;

	lwt_fn_t			fn;					/* function pointer */
	void*				data;				/* data				*/

	lwt_flags_t			flag;
	int  				tid;				/* thread id	*/
	int 				kid;				/* kernel thread id */
	int  				state;				/* thread state	*/
}lwt, *lwt_t;	


/* see in csci_6411_lwt.pdf */
lwt_t 	lwt_create	(lwt_fn_t fn, void* data, lwt_flags_t flags);
void*	lwt_join	(lwt_t);
void	lwt_die  	(void*);
int 	lwt_yield 	(lwt_t);
lwt_t 	lwt_current (void);
int 	lwt_id  	(lwt_t);
int 	lwt_info 	(lwt_info_t t);

void lwt_init();
void lwt_schedule(void);

#endif	/* LWT_H */
