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

void array_init(array*o);
void array_release(array*o);
void array_push(array*o,void*e);
void*array_pop(array*o);
void array_rewind(array*o);

sorted_array*sorted_array_new(compare cmp);
void sorted_array_release(sorted_array*o);
void sorted_array_put(sorted_array*o,void*e);
void*sorted_array_erase(sorted_array*o,void*e);
