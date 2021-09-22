#pragma once

#include "st.h"

#ifndef ARRAY_FIXED_SIZE
# define ARRAY_FIXED_SIZE 4096
#endif

typedef struct{
    void*fixed_array[ARRAY_FIXED_SIZE];
    void**a;
    unsigned i,n;
}array;

typedef struct sorted_array sorted_array;
typedef struct stack stack;
typedef struct obj_pool obj_pool;

void array_init(array*o);
void array_release(array*o);
void array_push(array*o,void*e);
void*array_pop(array*o);
void array_rewind(array*o);

sorted_array*sorted_array_new(compare cmp);
void sorted_array_release(sorted_array*o);
void sorted_array_insert(sorted_array*o,void*e);
void*sorted_array_erase(sorted_array*o,void*e);

stack*stack_new(unsigned init_capacity);
void stack_release(stack*o);
stack*stack_push(stack*o,void*e);
void*stack_pop(stack*o);
void*stack_top(stack*o);

obj_pool*obj_pool_new(unsigned obj_sz,unsigned n_objs);
void obj_pool_release(obj_pool*o);
void obj_pool_put(obj_pool*o,void*e);
void*obj_pool_get(obj_pool*o);
