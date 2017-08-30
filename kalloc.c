/* 
 * An implementation of freelist. 
 * Idea from xv6's src: kalloc.c
 * 
 * Author: Yitian Huang, huangyitian@gwu.edu, 2017
 */

#include "kalloc.h"

struct run {
  struct run *next;
  int len;
}*freelist;

void
kinit(void)
{
  char *start = (char*)malloc(PAGESIZE);
  assert(start != NULL);
  
  /* A special case, 
   * it leads to freelist starts with one element that has len = PAGESIZE 
   */ 
  kfree(start, PAGESIZE);
}

void
kfree(char *v, int len)
{
  struct run *r, *rend, **rp, *p, *pend;
  
  assert(len > 0 && len <= PAGESIZE);

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
  
  assert(n <= PAGESIZE && n > 0);

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
  
  /* out of memory, malloc new segement
   * But it shouldn't happen often
   */
  return (char*)malloc(n);
}