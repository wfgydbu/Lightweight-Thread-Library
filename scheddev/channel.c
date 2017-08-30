/* 
 * An implementation of Synchronous LWT Channels.
 * 
 * Author: Yitian Huang, huangyitian@gwu.edu, 2017
 */
#include <channel.h>

unsigned int	lwt_chanid_counter 		= 1;	/* global channelId tracker */
extern  lwt_t	current_thread;					/* current active thread, first defined in lwt.c */
extern 	lwt_t 	head_thread;

#define PS_CAS(t,d,v) do {		\
	t = d;						\
}while(!ps_cas(&d,t,t+v))


lwt_chan_t
lwt_chan(int sz)
{
	assert(sz >= 0);

	lwt_chan_t c = (lwt_chan_t)kalloc(sizeof(struct lwt_channel));
	assert(c != NULL);

	c->size = sz;
	c->snd_cnt = 0;   
	c->id = lwt_chanid_counter++;	
	
	c->rbuf = rbuf_init(sz);

	c->rcv_thd = current_thread;
	
	c->cgrp = NULL;
	c->mark = NULL;

	int cas_t;
	PS_CAS(cas_t, LWT_INFO_NCHAN, 1);

	return c;
}


void
lwt_chan_deref(lwt_chan_t c)
{
	assert(c != NULL);


	if(c->rcv_thd == current_thread){
		/* This means the receiver is the only thread has reference with this channel
		 * and this the only case causes channel freed.
		 */
		if(c->snd_cnt == 0){
			c->rcv_thd = NULL;
			rbuf_free(&c->rbuf);
			kfree((char*)c, sizeof(struct lwt_channel));
			goto done;
		}
	}

	/* Otherwise, simply decrement the snd_cnt
	 * This is is the case when sender call deref()
	 */
	c->snd_cnt--;

done:
	return;
}



int
lwt_snd(lwt_chan_t c, void *data)
{	
	/* data cannot be NULL */
	assert(data != NULL && c != NULL);

	/* this channel calls lwt_chan_deref() before */
	if(c->rcv_thd == NULL){
		return -1;
	}

	/* block sender if channel's rbuf is full */
	while(rbuf_isfull(c->rbuf)){
		lwt_yield(LWT_NULL);
	}
	
	/* write to rbuf */
	int cas_t;
	PS_CAS(cas_t,LWT_INFO_NSNDING,1);


	clist_head_t t = (clist_head_t)kalloc(sizeof(struct clist_head));
	t->thd = current_thread;
	t->data = data;	

	assert(WRITERBUF_OK == rbuf_write(c->rbuf,(void*)t));
	
	c->snd_cnt++;
	
	PS_CAS(cas_t,LWT_INFO_NSNDING,-1);
	
	/* wake up if the rcv kthd has new lwt coming */
	struct sl_thd *tt = sl_thd_lkup(c->rcv_thd->kid);

	if(tt){
		if(tt->state == SL_THD_BLOCKED){
			sl_thd_wakeup(c->rcv_thd->kid);
		}
	}

	return 0;
}

void*
lwt_rcv(lwt_chan_t c)
{
	assert(c != NULL);

	void *tmp, *ret;
	clist_head_t t;
	int cas_t;


	if (c->rcv_thd == NULL) {
		return NULL;
	}
	
	/* block receiver until queue is not empty */
	PS_CAS(cas_t, LWT_INFO_NRCVING,1);

	while(rbuf_isempty(c->rbuf)) {
		lwt_yield(LWT_NULL);
	}

	PS_CAS(cas_t, LWT_INFO_NRCVING,-1);

	/* read from rbuf, receive data */
	ret = rbuf_read(c->rbuf);
	tmp = ((clist_head_t)ret)->data;

	kfree((char*)ret,sizeof(struct clist_head));

	return tmp;
}

int
lwt_snd_chan(lwt_chan_t c, lwt_chan_t sending)
{	
	sending->snd_cnt++;

	return lwt_snd(c,(void*)sending);
}


lwt_chan_t
lwt_rcv_chan(lwt_chan_t c)
{
	return (lwt_chan_t)lwt_rcv(c);
}

lwt_t
lwt_create_chan(lwt_chan_fn_t fn, lwt_chan_t c)
{	
	return lwt_create(fn,(void*)c, LWT_DEFAULT);;
}

lwt_cgrp_t
lwt_cgrp(void)
{
	lwt_cgrp_t cg = (lwt_cgrp_t)kalloc(sizeof(struct lwt_cgrp));

	if(!cg){
		return LWT_NULL;
	}

	cg->chan_cnt = 0;
	TAILQ_INIT(&cg->clist_cgrp_chan_head);

	return cg;
}

int
lwt_cgrp_free(lwt_cgrp_t cg)
{
	assert(cg);

	if(cg->chan_cnt){
		return -1;
	}

	kfree((char*)cg, sizeof(struct lwt_cgrp));
	return 0;
}

int
lwt_cgrp_add(lwt_cgrp_t cg, lwt_chan_t c)
{
	assert(cg);
	assert(c);

	/* the c has not been set to any groups */
	if(c->cgrp == NULL){
		c->cgrp = cg;
		cg->chan_cnt++;
		TAILQ_INSERT_TAIL(&cg->clist_cgrp_chan_head, c, clist_chans);	
		
		return 0;
	}

	/* This means someone try to set c to cg, twice.*/
	if(c->cgrp == cg){
		return 0;
	}

	/* Otherwise, c has been to another group */
	return -1;
}

int
lwt_cgrp_rem(lwt_cgrp_t cg, lwt_chan_t c)
{
	assert(cg);
	assert(c);

	/* the chan does not belong to this group */
	if(c->cgrp != cg){
		return -1;
	}


	if(rbuf_isempty(c->rbuf)){
		/* Not pending */
		TAILQ_REMOVE(&cg->clist_cgrp_chan_head, c, clist_chans);
		cg->chan_cnt--;
		return 0;
	}

	return 1;
}

lwt_chan_t
lwt_cgrp_wait(lwt_cgrp_t cg)
{
	assert(cg);

	lwt_chan_t p;

	while(1){
		for(p = (lwt_chan_t)cg->clist_cgrp_chan_head.tqh_first; p != NULL; p = p->clist_chans.tqe_next){
			if(!rbuf_isempty(p->rbuf)){
				return p;
			}
		}

		lwt_yield(LWT_NULL);
	}       

}

void
lwt_chan_mark_set(lwt_chan_t c, void* mark)
{
	assert(mark != NULL);
	c->mark = mark;	
}

void*
lwt_chan_mark_get(lwt_chan_t c)
{	
	assert(c);

	if(c->mark){
		return c->mark;
	}

	return NULL;
}	

int
__ps_list_nelements(lwt_t t)
{
	lwt_t tmp = t;
	int cnt = 0;

	do{
		tmp = ps_list_next_d(tmp);
		
		if(tmp->state == RUNNABLE)
			cnt++;
	}while(tmp != t);

	return cnt;
}

void
__kthd_entry(void* data)
{
	lwt_init();
	kthd_param_t kp = (kthd_param_t)data;
	kp->lwt = current_thread;


	lwt_t t = lwt_create((lwt_chan_fn_t)kp->fn,kp->c, LWT_NOJOIN);
	
	while(1){
		current_thread = kp->lwt;
		lwt_schedule();

		int remain_nthds = __ps_list_nelements(current_thread);		

		if(remain_nthds == 1){
			sl_thd_block(0);
		}		 	
		else{
			sl_thd_yield(0);
		}
	}


}

int 		
lwt_kthd_create(lwt_fn_t fn, lwt_chan_t c)
{
	kthd_param_t kp = (kthd_param_t)kalloc(sizeof(struct kthd_param));
	assert(kp);

	kp->fn = fn;
	kp->c  = c;

	struct sl_thd* s = sl_thd_alloc(__kthd_entry, (void*)kp);
	union sched_param spl = {.c = {.type = SCHEDP_PRIO, .value = 10}};
	sl_thd_param_set(s, spl.v);
	
	return (s != NULL)-1;
	
}

int
lwt_snd_thd(lwt_chan_t c, lwt_t sending)
{
	
	if(sending == current_thread){
		return -1;
	}

	
	sending->state = BLOCKED;
	

	return lwt_snd(c, (void*)sending);
}


lwt_t
lwt_rcv_thd(lwt_chan_t c)
{
	lwt_t tmp = (lwt_t)lwt_rcv(c);


	/* move tmp from old thd to current kthd, and wake it up */
	tmp->kid = current_thread->kid;

	ps_list_rem_d(tmp);
	ps_list_add_d(ps_list_prev_d(current_thread), tmp);

	tmp->state = RUNNABLE;

	return tmp;
}