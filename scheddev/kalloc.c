/* 
 * An implementation of freelist. 
 * Idea from xv6's src: kalloc.c
 * 
 * Author: Yitian Huang, huangyitian@gwu.edu, 2017
 */

#include <kalloc.h>

struct run {
	struct run *next;
	int len;
}*freelist;

void
kinit(struct cos_compinfo *ci)
{
	printc("[KALLOC INIT]\n");

	char* start;

	/* ethan: allocate 16MB mem */
	char *s, *t, *prev, *t1;

	int i;

	s = cos_page_bump_alloc(ci);
	assert(s);

	prev = s;
	for (i = 0; i < NPAGES; i++)
	{
		t = cos_page_bump_alloc(ci);
		assert(t && t == prev + 4096);

		prev = t;
	}

	memset(s, 0, NPAGES * 4096);
	printc("\tAllocated and zeroed %d pages = %d MB memory\n", NPAGES, NPAGES * 4096 / 1024 / 1024);
	printc("\tFirst address: 0x%x, Last address: 0x%x", (unsigned int)s, (unsigned int)t);

	
	start = s;
	
	kfree(start, NPAGES * 4096);
	printc("...SUCCESS\n");
}

void
kfree(char *v, int len)
{
  struct run *r, *rend, **rp, *p, *pend;
  
  assert(len > 0 && len <= NPAGES * 4096);

  /* It's supposed to reset memory segement here
   * But perf reduces quite a lot like 60 => 600.
   * So I gave up.
   */
  //memset(v, 0, len);

  p = (struct run*)v;
  pend = (struct run*)(v + len);


  for(rp=&freelist; (r=*rp) != 0 && r <= pend; rp=&r->next){
    rend = (struct run*)((char*)r + r->len);
    
    /* p next to r: replace r with p */
    if(pend == r){  
      p->len = len + r->len;
      p->next = r->next;
      *rp = p;
      goto ret;
    }
    
    /* r next to p: replace p with r */
    if(rend == p){  
      r->len += len;
      
      if(r->next && r->next == pend){
        r->len += r->next->len;
        r->next = r->next->next;
      }
      
      goto ret;
    }
  }
  
  /* Not in the list 
   * Suppose to be return by malloc in kalloc()
   * Append it to the list
   */
  p->len = len;
  p->next = r;
  *rp = p;

ret:
  return;
}

char*
kalloc(int n)
{
  char *p;
  struct run *r, **rp;
  
  assert(n <= NPAGES * 4096 && n > 0);

  for(rp=&freelist; (r=*rp) != 0; rp=&r->next){
    if(r->len == n){
      *rp = r->next;
      return (char*)r;
    }
    
    if(r->len > n){
      r->len -= n;
      p = (char*)r + r->len;
      return p;
    }
  }
  
  /* out of memory */
  return (char*)0;
}