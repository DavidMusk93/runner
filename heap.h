#pragma once

#include "st.h"

struct heap;

struct heap*heap_new(unsigned n,compare cmp);
void heap_release(struct heap*o);
void heap_push(struct heap*o,void*x);
void*heap_pop(struct heap*o);
void*heap_top(struct heap*o);
unsigned heap_size(struct heap*o);
