#include "array.h"

#include <stdlib.h>
#include <string.h>

#include "macro.h"

#define __swap SWAP_POINTER
#define OBJ_POOL_BLOCK_SIZE 4096
#define PTR_SIZE 8
#define PTR_MASK (PTR_SIZE-1)

struct sorted_array {
    array array;
    compare cmp;
};

struct stack {
    unsigned i, n;
    void *a[];
};

struct obj_pool {
    stack *stack;
    void **a;
    unsigned i, n;
    unsigned sz;
    unsigned block_sz;
};
/* private APIs */
#define __p (&o->fixed_array[0])

static void array_try_expand(array *o) {
    if (o->i < o->n)
        return;
    unsigned i, n = o->n;
    void **a = __p;
    o->a = realloc(o->a == a ? 0 : o->a, NEXT_ALLOC(o->n) * sizeof(void *));
    if (n == ARRAY_FIXED_SIZE) {
        for (i = 0; i < ARRAY_FIXED_SIZE; i++) {
            o->a[i] = a[i];
        }
    }
}

static void array_try_shrink(array *o) {
    void **a = __p;
    unsigned i = o->i;
    if (a != o->a && i < o->n / 3) {
        if (i < ARRAY_FIXED_SIZE / 4 * 3) {
            for (i = 0; i < o->i; i++) {
                a[i] = o->a[i];
            }
            free(o->a);
            o->a = a;
            o->n = ARRAY_FIXED_SIZE;
        } else {
            o->n = o->n / 3 * 2;
            o->a = realloc(o->a, o->n * sizeof(void *));
        }
    }
}

static __deprecated int array_search(array *o, void *e, compare cmp) {
#define __m(i, j) ((i)+((j)-(i))/2)
    unsigned i, j, m; /*(i,j]*/
    int x;
    i = 0;
    j = o->i, m = __m(i, j);
    /* binary search */
    for (;; m = __m(i, j)) {
        if (i >= j) { /*not found*/
            break;
        }
        x = cmp(e, o->a[m]);
        if (x == 0) {
            return (int) m;
        } else if (x < 0) {
            j = m;
        } else {
            i = m + 1;
        }
    }
    return -1;
}

static unsigned array_search2(array *o, void *e, compare cmp, int *success) {
    unsigned i, j, m; /*(i,j)*/
    int x;
    if (success)
        *success = 0;
    if (!o->i || cmp(e, o->a[0]) < 0) {
        return 0;
    }
    if (cmp(e, o->a[o->i - 1]) > 0) {
        return o->i;
    }
    for (i = 0, j = o->i - 1, m = __m(i, j);; m = __m(i, j)) {
        if (i > j) {
            return i;
        }
        x = cmp(e, o->a[i]);
        if (x == 0) {
            break;
        } else if (x < 0) {
            j = m - 1;
        } else {
            i = m + 1;
        }
    }
    if (success)
        *success = 1;
    return i;
}

#undef __m

static void array_step_move(array *o, unsigned from, unsigned to) {
    unsigned n = (o->i - from) * sizeof(void *);
    if (from + 1 == to) {
        memmove(&o->a[to], &o->a[from], n);
    } else if (from == to + 1) {
        memcpy(&o->a[to], &o->a[from], n);
    }
}

/* public APIs */
void array_init(array *o) {
    o->a = __p;
    o->i = 0;
    o->n = ARRAY_FIXED_SIZE;
}

void array_release(array *o) {
    if (o->a != __p) {
        free(o->a);
    }
}

void array_push(array *o, void *e) {
    array_try_expand(o);
    o->a[o->i++] = e;
}

void *array_pop(array *o) {
    if (!o->i)
        return 0;
    void *e = o->a[--o->i];
    array_try_shrink(o);
    return e;
}

void array_rewind(array *o) {
    void **a = __p;
    if (a != o->a) {
        free(o->a);
        o->a = a;
        o->n = ARRAY_FIXED_SIZE;
    }
    o->i = 0;
}

sorted_array *sorted_array_new(compare cmp) {
    sorted_array *o = malloc(sizeof(*o));
    array_init(&o->array);
    o->cmp = cmp;
    return o;
}

void sorted_array_release(sorted_array *o) {
    array_release(&o->array);
    free(o);
}

#define __a(i) o->array.a[i]
#define __i o->array.i

void sorted_array_insert(sorted_array *o, void *e) {
    int success;
    unsigned i = array_search2(&o->array, e, o->cmp, &success);
    if (success)
        return; /*already exist*/
    array_try_expand(&o->array);
    if (!__i || i == __i) {
        __a(__i) = e;
    } else {
        array_step_move(&o->array, i, i + 1);
        __a(i) = e;
    }
    __i++;
//    unsigned i=o->array.i;
//    array_push(&o->array,e);
//    /* bubble sort */
//    for(;i>0;i--){
//        if(o->cmp(__a(i),__a(i-1))>=0)
//            break;
//        __swap(__a(i),__a(i-1));
//    }
}

void *sorted_array_erase(sorted_array *o, void *e) {
    int success;
    unsigned i = array_search2(&o->array, e, o->cmp, &success);
    if (!success)
        return 0; /*not exist*/
    if (__i == 1 || i + 1 == __i) {
        return array_pop(&o->array);
    }
    e = __a(i);
    array_step_move(&o->array, i + 1, i);
//    unsigned n,i=array_search(&o->array,e,o->cmp);
//    if(i>>31)
//        return 0;
//    n=o->array.i;
//    for(;i+1<n;i++){
//        __swap(__a(i),__a(i+1));
//    }
    o->array.i--;
    array_try_shrink(&o->array);
    return e;
}

stack *stack_new(unsigned init_capacity) {
    AT_LEAST(init_capacity, 16);
    stack *o = malloc(sizeof(*o) + init_capacity * sizeof(void *));
    o->i = 0;
    o->n = init_capacity;
    return o;
}

void stack_release(stack *o) {
    free(o);
}

stack *stack_push(stack *o, void *e) {
    if (o->i == o->n) {
        o = realloc(o, sizeof(*o) + NEXT_ALLOC(o->n) * sizeof(void *));
    }
    o->a[o->i++] = e;
    return o;
}

void *stack_pop(stack *o) {
    if (o->i) {
        return o->a[--o->i];
    }
    return 0;
}

void *stack_top(stack *o) {
    if (o->i) {
        return o->a[o->i - 1];
    }
    return 0;
}

obj_pool *obj_pool_new(unsigned szobj, unsigned numobj) {
    obj_pool *o = malloc(sizeof(*o));
    o->stack = stack_new(16);
    o->i = 0;
    o->n = numobj ?: 16;
    o->sz = PTR_SIZE + ((szobj & PTR_MASK) ? (szobj & ~PTR_MASK) + PTR_SIZE : szobj);
    o->block_sz = o->sz > OBJ_POOL_BLOCK_SIZE ? o->sz : OBJ_POOL_BLOCK_SIZE;
    unsigned i, n = o->n / (o->block_sz / o->sz) + 1;
    o->a = malloc((n + 1) * sizeof(void *));
    for (i = 0; i <= n; i++)
        o->a[i] = 0;
    return o;
}

void obj_pool_release(obj_pool *o) {
    unsigned i;
    for (i = 0; o->a[i]; i++)
        free(o->a[i]);
    free(o->a);
    stack_release(o->stack);
    free(o);
}

void obj_pool_put(obj_pool *o, void *e) {
    long *p = e - 1;
    if (*p == -1) {
        free(p);
    } else {
        o->stack = stack_push(o->stack, e);
    }
}

void *obj_pool_get(obj_pool *o) {
    void *e;
    if ((e = stack_pop(o->stack))) {
        return e;
    }
    if (o->i < o->n) {
        unsigned i, j;
        i = o->i / (o->block_sz / o->sz);
        j = o->i % (o->block_sz / o->sz);
        if (!j) {
            o->a[i] = malloc(o->block_sz);
        }
        e = (char *) o->a[i] + j * o->sz;
        *(unsigned long *) e = o->i++;
        return e + 1;
    }
    e = malloc(o->sz);
    *(long *) e = -1;
    return e + 1;
}

#undef __i
#undef __a
#undef __p
#undef __swap
