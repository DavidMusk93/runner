#include "heap.h"

#include <stdlib.h>

struct heap {
    void **a;
    unsigned i, n;
    compare cmp;
};

heap *heap_new(unsigned n, compare cmp) {
    heap *o = malloc(sizeof(*o));
    o->a = malloc(n * sizeof(void *));
    o->i = 0, o->n = n;
    o->cmp = cmp;
    return o;
}

void heap_release(heap *o) {
    if (o) {
        free(o->a);
        free(o);
    }
}

//static inline void swap(void**x,void**y){
//    if(x!=y){
//        void*t=*x;
//        *x=*y;
//        *y=t;
//    }
//}

#define __swap SWAP_POINTER
#define __a(i) o->a[i]

static void __up(heap *o, unsigned i) {
    unsigned j;
    if (i > 0) {
        j = (i - 1) / 2;
        if (o->cmp(__a(j), __a(i)) < 0) {
            __swap(__a(i), __a(j));
            __up(o, j);
        }
    }
}

static void __down(heap *o, unsigned i) {
    unsigned j, n;
    for (j = i * 2 + 1, n = j + 2; j <= n && j < o->i; j++) {
        if (o->cmp(__a(i), __a(j)) < 0) {
            __swap(__a(i), __a(j));
            __down(o, j);
        }
    }
}

void heap_push(heap *o, void *x) {
    if (o->i == o->n) {
        o->a = realloc(o->a, NEXT_ALLOC(o->n));
    }
    o->a[o->i] = x;
    __up(o, o->i++);
}

void *heap_pop(heap *o) {
    if (o->i == 0) return 0;
    __swap(__a(0), __a(--o->i));
    __down(o, 0);
    return __a(o->i);
}

void *heap_top(heap *o) {
    return o->i ? __a(0) : 0;
}

unsigned heap_size(heap *o) {
    return o->i;
}

#undef __a
#undef __swap
