#ifndef LWT_H
#define LWT_H

#include <sys/types.h>
#include <sys/queue.h>
#include "ps_list.h"


#define likely(x)	__builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

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
	ulong ip, sp;
};

typedef struct lwt {
	int  				tid;				/* thread id	*/
	struct lwt_context	ct;					/* ip, sp		*/
	char 				stack[STACK_SIZE];	/* stack		*/
	int  				state;				/* thread state	*/
	void*				ret;				/* return value	*/
	
	struct ps_list 		list;
	
	struct lwt*			joinedfrom;			/* tracking joinning relations */
	struct lwt*			joiningto;

	lwt_fn_t			fn;					/* function pointer */
	void*				data;				/* data				*/

	lwt_flags_t			flag;
}lwt, *lwt_t;	


/* see in csci_6411_lwt.pdf */
lwt_t 	lwt_create	(lwt_fn_t fn, void* data, lwt_flags_t flags);
void*	lwt_join	(lwt_t);
void	lwt_die  	(void*);
int 	lwt_yield 	(lwt_t);
lwt_t 	lwt_current (void);
int 	lwt_id  	(lwt_t);
int 	lwt_info 	(lwt_info_t t);

#endif	/* LWT_H */
