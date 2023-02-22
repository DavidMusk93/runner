#pragma once

#include "st.h"

typedef struct heap heap;

heap* heap_new(unsigned n, fCompare cmp);
void heap_release(heap* o);
void heap_push(heap* o, void* x);
void* heap_pop(heap* o);
void* heap_top(heap* o);
unsigned heap_size(heap* o);
