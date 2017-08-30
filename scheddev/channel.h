#ifndef CHANNEL_H
#define CHANNEL_H

#include <sys/types.h>
#include <queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <lwt.h>
#include <channel.h>
#include <kalloc.h>
#include <rbuf.h>

#include <sl.h>

typedef void*         (*lwt_chan_fn_t)(void*);

typedef struct clist_head
{
	void* data;
	lwt_t thd;						/* sender */
}clist_head, *clist_head_t;

typedef struct lwt_channel
{
	int id;
	int snd_cnt;			/* number of senders */ 
	
	int size;				/* rbuf's size, 0 refers infinte */
	rbuf_t *rbuf;

	/* receiver’s data */ 
	lwt_t rcv_thd;			

	/* group */
	void* mark;
	struct lwt_cgrp* cgrp;
	TAILQ_ENTRY(lwt_channel) clist_chans;
	
}lwt_channel, *lwt_chan_t;

typedef struct lwt_cgrp
{
	unsigned int chan_cnt;
	TAILQ_HEAD(tailhead, clist_chans) clist_cgrp_chan_head;

}*lwt_cgrp_t;

typedef struct kthd_param
{
	struct sl_thd* 	kthd;
	lwt_t          	lwt;
	lwt_fn_t 		fn;
	lwt_chan_t 		c;
	void* 			d;
}*kthd_param_t;

/* See csci_6411_lwt_schan.pdf */
lwt_chan_t 	lwt_chan 			(int sz);
void 		lwt_chan_deref 		(lwt_chan_t c);
int 		lwt_snd 			(lwt_chan_t c, void *data);
void* 		lwt_rcv 			(lwt_chan_t c);
int 		lwt_snd_chan		(lwt_chan_t c, lwt_chan_t sending);
lwt_chan_t 	lwt_rcv_chan 		(lwt_chan_t c);
lwt_t 		lwt_create_chan 	(lwt_chan_fn_t fn, lwt_chan_t c);

/* See csci_6411_lwt_async_grp.pdf */
lwt_cgrp_t	lwt_cgrp 			(void);
int 		lwt_cgrp_free		(lwt_cgrp_t cg);
int 		lwt_cgrp_add		(lwt_cgrp_t cg, lwt_chan_t c);
int 		lwt_cgrp_rem		(lwt_cgrp_t cg, lwt_chan_t c);
lwt_chan_t 	lwt_cgrp_wait		(lwt_cgrp_t cg);
void 		lwt_chan_mark_set	(lwt_chan_t c, void* mark);
void*		lwt_chan_mark_get	(lwt_chan_t c);		


/* See csci_6411_lwt_kthds.pdf */
int 		lwt_kthd_create		(lwt_fn_t fn, lwt_chan_t c);
int 		lwt_snd_thd			(lwt_chan_t c, lwt_t sending);
lwt_t 		lwt_rcv_thd			(lwt_chan_t c);

#endif /* CHANNEL_H */