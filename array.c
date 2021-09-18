#include "array.h"

#include <stdlib.h>

#define __swap SWAP_POINTER

typedef struct sorted_array{
    array array;
    compare cmp;
}sorted_array;

/* private APIs */
#define __p (&o->fixed_array[0])
static void array_try_expand(array*o){
    if(o->i<o->n)
        return;
    unsigned i;
    void**a=__p;
    o->a=realloc(o->a==a?0:o->a,o->n*2);
    if(o->n==ARRAY_FIXED_SIZE){
        for(i=0;i<ARRAY_FIXED_SIZE;i++){
            o->a[i]=a[i];
        }
    }
    o->n*=2;
}

static void array_try_shrink(array*o){
    void**a=__p;
    unsigned i=o->i;
    if(a!=o->a&&i<o->n/3){
        if(i<ARRAY_FIXED_SIZE/4*3){
            for(i=0;i<o->i;i++){
                a[i]=o->a[i];
            }
            free(o->a);
            o->a=a;
            o->n=ARRAY_FIXED_SIZE;
        }else{
            o->n=o->n/3*2;
            o->a=realloc(o->a,o->n);
        }
    }
}

static int array_search(array*o,void*e,compare cmp){
#define __m(i,j) ((i)+((j)-(i))/2)
    unsigned i,j,m; /*(i,j]*/
    int x;
    i=0;j=o->i,m=__m(i,j);
    /* binary search */
    for(;;m=__m(i,j)){
        if(i>=j){ /*not found*/
            break;
        }
        x=cmp(e,o->a[m]);
        if(x==0){
            return (int)m;
        }else if(x<0){
            j=m;
        }else{
            i=m+1;
        }
    }
    return -1;
#undef __m
}

/* public APIs */
void array_init(array*o){
    o->a=__p;
    o->i=0;
    o->n=ARRAY_FIXED_SIZE;
}

void array_release(array*o){
    if(o->a!=__p){
        free(o->a);
    }
}

void array_push(array*o,void*e){
    array_try_expand(o);
    o->a[o->i++]=e;
}

void*array_pop(array*o){
    if(!o->i)
        return 0;
    void*e=o->a[--o->i];
    array_try_shrink(o);
    return e;
}

void array_rewind(array*o){
    void**a=__p;
    if(a!=o->a){
        free(o->a);
        o->a=a;
        o->n=ARRAY_FIXED_SIZE;
    }
    o->i=0;
}

sorted_array*sorted_array_new(compare cmp){
    sorted_array*o=malloc(sizeof(*o));
    array_init(&o->array);
    o->cmp=cmp;
    return o;
}

void sorted_array_release(sorted_array*o){
    array_release(&o->array);
    free(o);
}

#define __a(i) o->array.a[i]
void sorted_array_put(sorted_array*o,void*e){
    unsigned i=o->array.i;
    array_push(&o->array,e);
    /* bubble sort */
    for(;i>0;i--){
        if(o->cmp(__a(i),__a(i-1))>=0)
            break;
        __swap(__a(i),__a(i-1));
    }
}

void*sorted_array_erase(sorted_array*o,void*e){
    unsigned n,i=array_search(&o->array,e,o->cmp);
    if(i>>31)
        return 0;
    n=o->array.i;
    for(;i+1<n;i++){
        __swap(__a(i),__a(i+1));
    }
    o->array.i--;
    array_try_shrink(&o->array);
    return e;
}

#undef __a
#undef __p
#undef __swap
