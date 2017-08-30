#ifndef RBUF_H
#define RBUF_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#define INFINITE_CAP 1024

typedef struct rbuf
{
	void**	buf;
	uint	head;
	uint	tail;
	uint	capacity;
}rbuf_t;

typedef enum {
	WRITERBUF_OK		= 0,
	WRITERBUF_FULL		= 1,
	WRITERBUF_ERROR 	= 2,
}rbuf_write_state;

rbuf_t*	rbuf_init(uint capacity);   		/* necessary*/
void	rbuf_free(rbuf_t **rb);				/* free the whole buffer */
uint	rbuf_capacity(rbuf_t *rb);			/* return capacity of this buffer */
uint	rbuf_size(rbuf_t *rb);				/* return num of current existing elements in this buffer */

int 	rbuf_isempty(rbuf_t *rb);			/* return 1 as empty, otherwise 0 */
int 	rbuf_isfull(rbuf_t *rb);			/* return 1 as full, otherwise 0 */

void 	rbuf_erase(rbuf_t *rb);				/* erase all elements in this buffer. **NOT** free */	
int 	rbuf_write(rbuf_t *rb, void* data);	/* write data into buffer, return value refers to rbuf_write_state */
void*	rbuf_read(rbuf_t *rb);				/* read data from buffer */

#endif /* RBUF_H */
