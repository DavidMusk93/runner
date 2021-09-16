#pragma once

typedef int (*compare)(const void*,const void*);

struct heap;

struct heap*heap_new(unsigned n,compare cmp);
void heap_release(struct heap*o);
void heap_push(struct heap*o,void*x);
void*heap_pop(struct heap*o);
void*heap_top(struct heap*o);
unsigned heap_size(struct heap*o);

#define __swap(x,y) \
do{\
    __auto_type __x=&(x);/*avoid side effect*/\
    __auto_type __y=&(y);\
    if(__x==__y)break;\
    __auto_type __t=*__x;\
    *__x=*__y;\
    *__y=__t;\
}while(0)
