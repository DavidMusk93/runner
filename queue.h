#pragma once

#include <stddef.h>

typedef void* queue[2];

// rvalue
#define queue_next_r(q) ((queue*)(q)[0][0])
#define queue_prev_r(q) ((queue*)(q)[0][1])
// lvalue
#define queue_next(q) (*(queue**)&(q)[0][0])
#define queue_prev(q) (*(queue**)&(q)[0][1])

#define queue_prev_next(q) (queue_next(queue_prev_r(q)))
#define queue_next_prev(q) (queue_prev(queue_next_r(q)))

#define queue_init(q)  \
  queue_next(q) = (q); \
  queue_prev(q) = (q)

#define queue_is_empty(q) ((q) == queue_next(q))

// insert an element at the back of a queue
#define queue_push(q, e)           \
  queue_next(e) = (q);             \
  queue_prev(e) = queue_prev_r(q); \
  queue_prev_next(e) = (e);        \
  queue_prev(q) = (e)

#define queue_remove(e)                 \
  queue_prev_next(e) = queue_next_r(e); \
  queue_next_prev(e) = queue_prev_r(e)

// #define queue_head(q) queue_next(q)
// #define queue_tail(q) queue_prev(q)

#define queue_foreach(q, e) for ((e) = queue_next_r(q); (e) != (q); (e) = queue_next_r(e))
#define queue_data(e, type, field) ((type*)((char*)(e)-offsetof(type, field)))

// move the front entry of @from to back of @to
#define queue_move(from, to, e) \
  (e) = queue_next_r(from);     \
  queue_remove(e);              \
  queue_push(to, e)
