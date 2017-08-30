/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>

#include <sl.h>
#include <lwt.h>
#include <channel.h>
#include <kalloc.h>

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN(iters) do { if (iters > 0) { for (; iters > 0 ; iters -- ) ; } else { while (1) ; } } while (0)
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#define ITER 10000


extern int lwt_test_entry();

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

int
prints(char *s)
{
	int len = strlen(s);

	cos_llprint(s, len);

	return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_llprint(s, ret);

	  return ret;
}

#define N_TESTTHDS 8
#define WORKITERS  10000

void
test_thd_fn(void *data)
{	

	while (1) {
		int workiters = WORKITERS * ((int)data);

		printc("%d", (int)data);
		SPIN(workiters);
		sl_thd_yield(0);
	}

}

void
test_thd_zombie_fn(void){
	while(1){
		sl_thd_yield(0);
	}
}

void
test_yields(void)
{
	int                     i;
	struct sl_thd          *threads[N_TESTTHDS];
	union sched_param       sp    = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	for (i = 0 ; i < N_TESTTHDS ; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)(i + 1));
		assert(threads[i]);
		sl_thd_param_set(threads[i], sp.v);
	}
}

void
test_high(void *data)
{
	struct sl_thd *t = data;

	while (1) {
		sl_thd_yield(t->thdid);
		printc("h");
	}
}

void
test_low(void *data)
{
	while (1) {
		int workiters = WORKITERS * 10;
		SPIN(workiters);
		printc("l");
	}
}

void
test_blocking_directed_yield(void)
{
	struct sl_thd          *low, *high;
	union sched_param       sph = {.c = {.type = SCHEDP_PRIO, .value = 5}};
	union sched_param       spl = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	low  = sl_thd_alloc(test_low, NULL);
	high = sl_thd_alloc(test_high, low);
	sl_thd_param_set(low, spl.v);
	sl_thd_param_set(high, sph.v);
}

void
kthd_fn_kthd1(void)
{
	int i, pl = ITER;
	long long start = 0, end = 0;
	
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);
	for(i = 0; i < ITER; i++){
		pl--;
		lwt_yield(LWT_NULL);
	}
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);

	assert(pl == 0);
	
}

void
kthd_fn_kthd2(void)
{
	int i, pl = 0;
	long long start = 0, end = 0;

	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);
	rdtscll(start);
	for(i = 0; i < ITER; i++){
		pl++;
		lwt_yield(LWT_NULL);
	}
	rdtscll(end);
	lwt_yield(LWT_NULL);
	lwt_yield(LWT_NULL);

	assert(pl == ITER);
	
	/* to evaluate cpu cycles for each yield use: */
	printc("[PERF] %5lld <- each yield\n", (end-start)/(ITER*2));
	
	/* to evaluate cpu cycles for ITER times yields, use: */
	//printc("[PERF] %5lld <- yield %d times\n", (end-start)/2, ITER);


	lwt_create((lwt_fn_t)kthd_fn_kthd1,NULL,LWT_DEFAULT);
}




void
test_kthd_local_remote_yield(void)
{	
	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_kthd1, NULL));
	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_kthd2, NULL));
}

void*
kthd_fn_null(void *d)
{ return NULL; }

void
kthd_fn_snd_thd(void* d)
{
	lwt_chan_t c = (lwt_chan_t)d;
	
	int i, sum_tid = 0;
	long long start = 0, end = 0;

	lwt_t t;

	rdtscll(start);
	for(i = 0; i < ITER; i++){
		t = lwt_create(kthd_fn_null, NULL, LWT_NOJOIN);
		sum_tid += t->tid;
		lwt_snd_thd(c, t);
	}
	rdtscll(end);

	printc("[PERF] %5lld <- snd thd\n", (end-start)/ITER);

	lwt_chan_mark_set(c, (void*)sum_tid);
}	

void
kthd_fn_rcv_thd(void* d)
{	
	lwt_chan_t c = (lwt_chan_t)d;

	int i, sum_tid = 0;
	lwt_t t;
	long long start = 0, end = 0;

	rdtscll(start);
	for(i = 0; i < ITER; i++){
		t = lwt_rcv_thd(c);
		sum_tid += t->tid;
	}
	rdtscll(end);

	printc("[PERF] %5lld <- rcv thd\n", (end-start)/ITER);
	void *mark = lwt_chan_mark_get(c);

	assert(sum_tid == (int)mark);
}

void 
test_kthd_snd_rcv_thds()
{
	lwt_chan_t c = lwt_chan(0);	

	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_snd_thd, c));
	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_rcv_thd, c));
}

void *
kthd_fn_chan(lwt_chan_t to)
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


void*
kthd_fn_from_chan(lwt_chan_t from)
{	
	lwt_chan_t to;
	int i;
	unsigned long long start, end;

	assert(RUNNABLE == lwt_current()->state);
	to = (lwt_chan_t)lwt_rcv_chan(from);
	assert(to->snd_cnt);

	rdtscll(start);
	for (i = 0 ; i < ITER ; i++) {
		assert(1 == (int)lwt_rcv(from));
		lwt_snd(to, (void*)2);
	}
	lwt_chan_deref(to);
	rdtscll(end);
	printc("[PERF] %5lld <- snd+rcv remote channel(buffer size %d)\n", 
	       (end-start)/(ITER*2), from->size);
	lwt_yield(LWT_NULL);

	return NULL;
}



void
test_kthd_perf_remote_channels(int chsz)
{
	lwt_chan_t c = lwt_chan(chsz);
	
	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_chan,	 c));
	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_from_chan, c));
	
}

void*
kthd_fn_snder(void *d)
{
	assert(d);

	int i;

	for (i = 0 ; i < ITER ; i++) {
		if ((i % 7) == 0) {
			int j;

			for (j = 0 ; j < (i % 8) ; j++) lwt_yield(LWT_NULL);
		}
		lwt_snd((lwt_chan_t)d, ((lwt_chan_t)d)->id);
	}
}

#define GRPSIZE 30

void*
kthd_fn_multisend(lwt_chan_t c)
{
	int i;
	lwt_chan_t ct;

	for(i = 0; i < GRPSIZE; i++){
		ct = lwt_rcv_chan(c);

		lwt_create_chan((lwt_chan_fn_t)kthd_fn_snder,ct);
	}

	lwt_yield(LWT_NULL);

	return NULL;
}

void*
kthd_fn_multiwait(lwt_chan_t c)
{
	int i;
	
	lwt_chan_t *cs = (lwt_chan_t*)kalloc(sizeof(lwt_chan_t)*GRPSIZE);
	
	lwt_cgrp_t g;
	unsigned long long start, end;

	g = lwt_cgrp();
	assert(g);

	for(i = 0; i < GRPSIZE; i++){
		cs[i] = lwt_chan(0);
		lwt_chan_mark_set(cs[i], (cs[i])->id);
		lwt_snd_chan(c,cs[i]);
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
	printc("[PERF] %5lld <- multiwait (group size %d)\n", (end-start)/(ITER*GRPSIZE), GRPSIZE);

	for(i = 0 ; i < GRPSIZE ; i++) {
	 	lwt_cgrp_rem(g, cs[i]);
	 	lwt_chan_deref(cs[i]);
	}

	assert(0 == lwt_cgrp_free(g));

	return NULL;
}

void
test_kthd_multiwait()
{
	lwt_chan_t c = lwt_chan(0);
	
	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_multiwait, c));
	assert(0 == lwt_kthd_create((lwt_fn_t)kthd_fn_multisend, c));

}

void 
test_kthd_create()
{
	int i;
	unsigned long long start, end;

	rdtscll(start);
	for (i = 0 ; i < 50 ; i++){
		lwt_kthd_create((lwt_fn_t)kthd_fn_null, NULL);
	}
	rdtscll(end);


	printc("[PERF] %5lld <- create\n", (end-start)/50);
}


void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	kinit(ci);
	
	
	printc("\n[Unit-test for lwt library]\n");
	lwt_init();
	lwt_test_entry();

	printc("\n[Unit-test for kthd]\n");
	sl_init();

	//test_yields();
	//test_blocking_directed_yield();

	test_kthd_local_remote_yield();
	test_kthd_snd_rcv_thds();
	//test_kthd_create();
	int i;
	test_kthd_perf_remote_channels(0);
	for(i = 1; i <= 512; i = i<<2)
		test_kthd_perf_remote_channels(i);

	//test_kthd_multiwait();

	sl_sched_loop();
	
	assert(0);
	return;
}
