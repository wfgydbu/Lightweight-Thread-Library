#ifndef KALLOC_H
#define KALLOC_H

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* Modify to meet your requirement, if necessary */
#ifndef NPAGES
	#define NPAGES (1024 / 4 * 16)
#endif


void	kinit(struct cos_compinfo *ci);					/* init the pool, necessary*/
char*	kalloc(int n);
void	kfree(char *v, int len);

#endif	/* KALLOC_H */
