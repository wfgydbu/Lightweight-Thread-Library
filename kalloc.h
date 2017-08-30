#ifndef KALLOC_H
#define KALLOC_H

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* Modify to meet your requirement, if necessary */
#ifndef PAGESIZE
	#define PAGESIZE (10 * 1024 * 1024)
#endif


void	kinit(void);					/* init the pool, necessary*/
char*	kalloc(int n);
void	kfree(char *v, int len);

#endif	/* KALLOC_H */
