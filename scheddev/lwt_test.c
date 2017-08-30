#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <lwt.h>
#include <channel.h>
#include <rbuf.h>
#include <kalloc.h>

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))


#define ITER 10000
extern void run_lm_tests();

/* 
 * My output on an Intel Core i5-2520M CPU @ 2.50GHz:
 *
 * [PERF] 120 <- fork/join
 * [PERF] 13 <- yield
 * [TEST] thread creation/join/scheduling
 */

void *
fn_bounce(void *d) 
{
	int i;
	unsigned long long start, end;

	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) lwt_yield(LWT_NULL);
	rdtscll(end);
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);

	if (!d) printc("[PERF] %5lld <- yield\n", (end-start)/(ITER*2));

	return NULL;
}

void *
fn_null(void *d)
{ return NULL; }

#define IS_RESET()						\
        assert( lwt_info(LWT_INFO_NTHD_RUNNABLE) == 1 &&	\
		lwt_info(LWT_INFO_NTHD_ZOMBIES) == 0 &&		\
		lwt_info(LWT_INFO_NTHD_BLOCKED) == 0)

void
test_perf(void)
{
	lwt_t chld1, chld2;
	int i;
	unsigned long long start, end;

	/* Performance tests */
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		chld1 = lwt_create(fn_null, NULL, LWT_DEFAULT);
		lwt_join(chld1);
	}
	rdtscll(end);
	printc("[PERF] %5lld <- fork/join\n", (end-start)/ITER);
	IS_RESET();

	chld1 = lwt_create(fn_bounce, (void*)1, LWT_DEFAULT);
	chld2 = lwt_create(fn_bounce, NULL, LWT_DEFAULT);
	lwt_join(chld1);
	lwt_join(chld2);
	IS_RESET();
}

void *
fn_identity(void *d)
{ return d; }

void *
fn_nested_joins(void *d)
{
	lwt_t chld;

	if (d) {
		lwt_yield(LWT_NULL);
		lwt_yield(LWT_NULL);
		assert(lwt_info(LWT_INFO_NTHD_RUNNABLE) == 1);
		lwt_die(NULL);
	}
	chld = lwt_create(fn_nested_joins, (void*)1, LWT_DEFAULT);
	lwt_join(chld);
}

volatile int sched[2] = {0, 0};
volatile int curr = 0;

void *
fn_sequence(void *d)
{
	int i, other, val = (int)d;

	for (i = 0 ; i < ITER ; i++) {
		other = curr;
		curr  = (curr + 1) % 2;
		sched[curr] = val;
		assert(sched[other] != val);
		lwt_yield(LWT_NULL);
	}

	return NULL;
}

void *
fn_join(void *d)
{
	lwt_t t = (lwt_t)d;
	void *r;

	r = lwt_join(d);
	assert(r == (void*)0x37337);
}

void
test_crt_join_sched(void)
{
	lwt_t chld1, chld2;

	printc("[TEST] thread creation/join/scheduling\n");

	/* functional tests: scheduling */
	lwt_yield(LWT_NULL);
	
	

	chld1 = lwt_create(fn_sequence, (void*)1, LWT_DEFAULT);
	chld2 = lwt_create(fn_sequence, (void*)2, LWT_DEFAULT);
		
	lwt_join(chld2);
	lwt_join(chld1);
	
	IS_RESET();	
	
	
	/* functional tests: join */
	chld1 = lwt_create(fn_null, NULL, LWT_DEFAULT);
	lwt_join(chld1);
	IS_RESET();

	chld1 = lwt_create(fn_null, NULL, LWT_DEFAULT);
	lwt_yield(LWT_NULL);
	lwt_join(chld1);
	IS_RESET();

	chld1 = lwt_create(fn_nested_joins, NULL, LWT_DEFAULT);
	lwt_join(chld1);
	IS_RESET();

	/* functional tests: join only from parents */
	chld1 = lwt_create(fn_identity, (void*)0x37337, LWT_DEFAULT);
	chld2 = lwt_create(fn_join, chld1, LWT_DEFAULT);
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);
	lwt_join(chld2);
	//lwt_join(chld1);
	IS_RESET();

	/* functional tests: passing data between threads */
	chld1 = lwt_create(fn_identity, (void*)0x37337, LWT_DEFAULT);
	assert((void*)0x37337 == lwt_join(chld1));
	IS_RESET();

	/* functional tests: directed yield */
	chld1 = lwt_create(fn_null, NULL, LWT_DEFAULT);
	lwt_yield(chld1);
	assert(lwt_info(LWT_INFO_NTHD_ZOMBIES) == 1);
	lwt_join(chld1);
	IS_RESET();
}

void *
fn_chan(lwt_chan_t to)
{
	lwt_chan_t from;
	int i;
	from = (lwt_chan_t)lwt_chan(0);
	
	lwt_snd_chan(to, from);

	

	assert(from->snd_cnt);  
	for (i = 0 ; i < ITER ; i++) {
		lwt_snd(to, (void*)1);
		assert(2 == (int)lwt_rcv(from));
	}
	lwt_chan_deref(from);
	
	return NULL;
}

void
test_perf_channels(int chsz)
{
	lwt_chan_t from, to;
	lwt_t t;
	int i;
	unsigned long long start, end;

	assert(RUNNABLE == lwt_current()->state);
	from = (lwt_chan_t)lwt_chan(chsz);

	assert(from);
	t    = lwt_create_chan((lwt_chan_fn_t)fn_chan, from); 
	to   = (lwt_chan_t)lwt_rcv_chan(from);
	
	assert(to->snd_cnt);  
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		assert(1 == (int)lwt_rcv(from));
		lwt_snd(to, (void*)2);
	}
	lwt_chan_deref(to);
	rdtscll(end);
	printc("[PERF] %5lld <- snd+rcv (buffer size %d)\n", 
	       (end-start)/(ITER*2), chsz);
	lwt_join(t);
}

void
test_ringbuffer()
{
	int capacity = 7, i;
	char res[7] = "SUCCESS";

	printc("[TEST] ring buffer...");

	rbuf_t* rbuf = rbuf_init(capacity);
	assert(rbuf_isempty(rbuf) == 1 && rbuf_isfull(rbuf)  == 0);
	assert(rbuf_capacity(rbuf) == capacity);
	assert(rbuf_size(rbuf) == 0);

	for(i = 0; i < capacity; i++){
		assert(WRITERBUF_OK == rbuf_write(rbuf,(void*)(res+i)));
	}

	assert(rbuf_isempty(rbuf) == 0 && rbuf_isfull(rbuf)  == 1);
	assert(rbuf_capacity(rbuf) == capacity);
	assert(rbuf_size(rbuf) == capacity);

	void* ret;
	for(i = 0; i < capacity; i++){
		ret = rbuf_read(rbuf);
		printc("%c", *((char*)ret));
		
	}
	printc("\n");

	assert(rbuf_isempty(rbuf) == 1 && rbuf_isfull(rbuf)  == 0);
	assert(rbuf_capacity(rbuf) == capacity);
	assert(rbuf_size(rbuf) == 0);

	rbuf_free(&rbuf);


	/* test infinite cap*/
	rbuf = rbuf_init(0);
	for(i = 0; i < 1000; i++){
		assert(WRITERBUF_OK == rbuf_write(rbuf,(void*)(i+1)));
	}

	for(i = 0; i < 1000; i++){
		ret = rbuf_read(rbuf);
		assert((int)ret == i+1);
		
	}
	rbuf_free(&rbuf);
}

static int sndrcv_cnt = 0;

void *
fn_snder(lwt_chan_t c, int v)
{
	int i;

	for (i = 0 ; i < ITER ; i++) {
		lwt_snd(c, (void*)v);
		sndrcv_cnt++;
	}

	return NULL;
}

void *fn_snder_1(lwt_chan_t c) { return fn_snder(c, 1); }
void *fn_snder_2(lwt_chan_t c) { return fn_snder(c, 2); }

void
test_multisend(int chsz)
{
	lwt_chan_t c;
	lwt_t t1, t2;
	int i, sum = 0, maxcnt = 0;
	
	//ret[ITER*2], for <4K stack 
	int *ret = (int*)kalloc(sizeof(int)*ITER*2);
	memset(ret, 0, sizeof(int)*ITER*2);
	
	printc("[TEST] multisend (channel buffer size %d)\n", chsz);

	c  = lwt_chan(chsz);
	assert(c);
	t1 = lwt_create_chan((lwt_chan_fn_t)fn_snder_2, c);
	t2 = lwt_create_chan((lwt_chan_fn_t)fn_snder_1, c);
	for (i = 0 ; i < ITER*2 ; i++) {
		//if (i % 5 == 0) lwt_yield(LWT_NULL);
		ret[i] = (int)lwt_rcv(c);
		if (sndrcv_cnt > maxcnt) maxcnt = sndrcv_cnt;
		sndrcv_cnt--;
	}
	lwt_join(t1);
	lwt_join(t2);
	
	for (i = 0 ; i < ITER*2 ; i++) {
		sum += ret[i];
		assert(ret[i] == 1 || ret[i] == 2);
	}
	assert(sum == (ITER * 1) + (ITER*2));
	/* 
	 * This is important: Asynchronous means that the buffer
	 * should really fill up here as the senders never block until
	 * the buffer is full.  Thus the difference in the number of
	 * sends and the number of receives should vary by the size of
	 * the buffer.  If your implementation doesn't do this, it is
	 * doubtful you are really doing asynchronous communication.
	 */
	assert(maxcnt >= chsz);

	kfree((char*)ret,sizeof(int)*ITER*2);

	return;
}

static int async_sz = 0;

void *
fn_async_steam(lwt_chan_t to)
{
	int i;
	
	for (i = 0 ; i < ITER ; i++) lwt_snd(to, (void*)(i+1));
	lwt_chan_deref(to);
	
	return NULL;
}

void
test_perf_async_steam(int chsz)
{
	lwt_chan_t from;
	lwt_t t;
	int i;
	unsigned long long start, end;

	async_sz = chsz;
	assert(RUNNABLE == lwt_current()->state);
	from = lwt_chan(chsz);
	assert(from);
	t = lwt_create_chan((lwt_chan_fn_t)fn_async_steam, from);
	assert(lwt_info(LWT_INFO_NTHD_RUNNABLE) == 2);
	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) assert(i+1 == (int)lwt_rcv(from));
	rdtscll(end);
	printc("[PERF] %5lld <- asynchronous snd->rcv (buffer size %d)\n",
	       (end-start)/(ITER*2), chsz);
	lwt_join(t);
}


void*
fn_multiwait(void *d)
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

void
test_multiwait(int cgrpsz)
{
	lwt_chan_t cs[cgrpsz];
	lwt_t ts[cgrpsz];
	int i;
	lwt_cgrp_t g;
	unsigned long long start, end;

	
	g = lwt_cgrp();
	assert(g);
	
	for(i = 0 ; i < cgrpsz ; i++) {
		cs[i] = lwt_chan(0);
		assert(cs[i]);
		
		ts[i] = lwt_create_chan(fn_multiwait, cs[i]);
		assert(ts[i]);

		lwt_chan_mark_set(cs[i], (void*)lwt_id(ts[i]));
		assert(0 == lwt_cgrp_add(g, cs[i]));
	}

	assert(lwt_cgrp_free(g) == -1);

	rdtscll(start);
	for(i = 0 ; i < ITER * cgrpsz; i++) {
		lwt_chan_t c;
		int r;
		c = lwt_cgrp_wait(g);
		assert(c);
		
		r = (int)lwt_rcv(c);
		assert(r == (int)lwt_chan_mark_get(c));
	}
	rdtscll(end);
	printc("[PERF] %5lld <- multiwait (group size %d)\n", (end-start)/(ITER*2), cgrpsz);

	for(i = 0 ; i < cgrpsz ; i++) {
	 	lwt_cgrp_rem(g, cs[i]);
	 	lwt_chan_deref(cs[i]);
	}

	assert(0 == lwt_cgrp_free(g));
	
	
	return;
}

int
lwt_test_entry(void)
{
	//run_lm_tests();
	test_perf();
	test_crt_join_sched();

	int i;
	for(i = 1; i <= 512; i = i<<2)
		test_perf_channels(i);
	
	test_perf_async_steam(ITER/10 < 100 ? ITER/10 : 100);
	test_multisend(0);
	test_multisend(ITER/10 < 100 ? ITER/10 : 100);

	test_ringbuffer();


	//test_multiwait(200);

	return 0;
}