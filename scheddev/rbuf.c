/* 
 * An implementation of ring buffer.
 * 
 * Author: Yitian Huang, huangyitian@gwu.edu, 2017
 */
#include "rbuf.h"
#include "kalloc.h"

rbuf_t*
rbuf_init(unsigned int capacity)
{
	rbuf_t* rb = (rbuf_t*)kalloc(sizeof(struct rbuf));
	assert(rb);

	if(0 == capacity){
		rb->capacity = INFINITE_CAP + 1;
		rb->buf = (void**)kalloc(sizeof(void*) * rb->capacity);
	}else{
		/* The reason why use capacity+1 here is 
		 * for the convenience of rbuf_isfull() and rbuf_isempty()
		 */
		rb->capacity = capacity + 1;
		rb->buf = (void**)kalloc(sizeof(void*) * rb->capacity);
	}
	
	rb->head = 0;
	rb->tail = 0;
	assert(rb->buf);

	return rb;
}

void
rbuf_free(rbuf_t **rb)
{
	assert(rb && *rb);

	kfree((char*)((*rb)->buf), sizeof(void*) * (*rb)->capacity);
	kfree((char*)*rb, sizeof(struct rbuf));
	*rb = NULL;
}

unsigned int
rbuf_capacity(rbuf_t *rb)
{
	return rb->capacity - 1;
}

unsigned int
rbuf_size(rbuf_t *rb)
{
	return (rb->tail + rb->capacity - rb->head) % rb->capacity;
}

int
rbuf_isempty(rbuf_t *rb)
{
	return (rb->head == rb->tail);
}

int
rbuf_isfull(rbuf_t *rb)
{
	return ((rb->tail + 1) % rb->capacity) == rb->head;
}

void
rbuf_erase(rbuf_t *rb)
{
	rb->head = 0;
	rb->tail = 0;
}

int
rbuf_write(rbuf_t *rb, void* data)
{
	if(data == NULL){
		return WRITERBUF_ERROR;
	}

	int cas_tail;
	do{
		if(rbuf_isfull(rb)){
			return WRITERBUF_FULL;
		}
		cas_tail = rb->tail;
		rb->buf[cas_tail] = data;
	}while(!ps_cas(&(rb->tail), cas_tail, (cas_tail + 1)%rb->capacity));

	//rb->buf[rb->tail] = data;
	//rb->tail = (rb->tail + 1) % rb->capacity;
	
	return WRITERBUF_OK;
}

void*
rbuf_read(rbuf_t *rb)
{
	if(rbuf_isempty(rb)){
		return NULL;
	}

	void* ret;
	int cas_head;
	do{
		cas_head = rb->head;
		ret = rb->buf[cas_head];
	}while(!ps_cas(&(rb->head), cas_head, (cas_head + 1)%rb->capacity));

	//void* ret = rb->buf[rb->head];
	//rb->head = (rb->head + 1)% rb->capacity;

	return ret;
}				