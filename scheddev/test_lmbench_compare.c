#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <lwt.h>
#include <channel.h>
#include <rbuf.h>
#include <queue.h>

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#define ITER 10000

void *
lm_fn_null(void *d)
{ return NULL; }


void
test_lat_ctx(void)
{
	int i;
	unsigned long long start, end;

	for (i = 0 ; i < 1000 ; i++){
		lwt_create(lm_fn_null, NULL, LWT_NOJOIN);
	}

	rdtscll(start);
	lwt_yield(LWT_NULL);
	rdtscll(end);


	printc("[PERF] %5lld <- lat_ctx in lwt\n", (end-start));


}


void *
lm_fn_chan(lwt_chan_t to)
{
	lwt_chan_t from;
	int i;
	from = (lwt_chan_t)lwt_chan(0);
	
	lwt_snd_chan(to, from);


	for (i = 0 ; i < ITER ; i++) {
		lwt_snd(to, (void*)1);
		lwt_rcv(from);
	}
	lwt_chan_deref(from);
	
	return NULL;
}

void
test_lat_pipe()
{
	lwt_chan_t from, to;
	lwt_t t;
	int i;
	unsigned long long start, end;

	from = (lwt_chan_t)lwt_chan(0);

	t    = lwt_create_chan((lwt_chan_fn_t)lm_fn_chan, from); 
	to   = (lwt_chan_t)lwt_rcv_chan(from);
	
	
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		lwt_rcv(from);
		lwt_snd(to, (void*)2);
	}
	lwt_chan_deref(to);
	rdtscll(end);
	printc("[PERF] %5lld <- lat_pipe in lwt\n", 
	       (end-start)/ITER);
	lwt_join(t);
}

void *
lm_fn_identity(void *d)
{ return d; }

void
test_lat_proc(void)
{
	int i;
	unsigned long long start, end;

	rdtscll(start);
	for (i = 0 ; i < ITER ; i++){
		lwt_create(lm_fn_identity, NULL, LWT_NOJOIN);
	}
	rdtscll(end);


	printc("[PERF] %5lld <- lat_proc in lwt\n", (end-start)/ITER);
}


void*
lm_fn_multiwait(void *d)
{
	assert(d);

	int i;

	for (i = 0 ; i < ITER ; i++) {
		if ((i % 7) == 0) {
			int j;

			for (j = 0 ; j < (i % 8) ; j++) lwt_yield(LWT_NULL);
		}
		lwt_snd((lwt_chan_t)d, (void*)lwt_id(lwt_current()));
	}
}

#define GRPSIZE 30

void
test_lat_select()
{
	lwt_chan_t cs[GRPSIZE];
	lwt_t ts[GRPSIZE];
	int i;
	lwt_cgrp_t g;
	unsigned long long start, end;

	g = lwt_cgrp();
	assert(g);
	
	for(i = 0 ; i < GRPSIZE ; i++) {
		cs[i] = lwt_chan(0);
		assert(cs[i]);
		
		ts[i] = lwt_create_chan(lm_fn_multiwait, cs[i]);
		assert(ts[i]);

		lwt_chan_mark_set(cs[i], (void*)lwt_id(ts[i]));
		assert(0 == lwt_cgrp_add(g, cs[i]));
	}

	assert(lwt_cgrp_free(g) == -1);

	rdtscll(start);
	for(i = 0 ; i < ITER * GRPSIZE; i++) {
		lwt_chan_t c;
		int r;
		c = lwt_cgrp_wait(g);
		assert(c);
		
		r = (int)lwt_rcv(c);
		assert(r == (int)lwt_chan_mark_get(c));
	}
	rdtscll(end);
	printc("[PERF] %5lld <- lat_select in lwt, size %d\n", (end-start)/(ITER*GRPSIZE),GRPSIZE);
	
	for(i = 0 ; i < GRPSIZE ; i++) {
	 	lwt_cgrp_rem(g, cs[i]);
	 	lwt_chan_deref(cs[i]);
	}

	assert(0 == lwt_cgrp_free(g));
	
	
	return;
}


void
run_lm_tests()
{
	test_lat_ctx();
	test_lat_pipe();
	test_lat_proc();
	test_lat_select();
}