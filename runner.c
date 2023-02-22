#include "runner.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/eventfd.h>

#include "array.h"
#include "heap.h"
#include "ipc.h"
#include "macro.h"
#include "misc.h"
#include "queue.h"
#include "state.h"

#define __relax ({ __asm__ volatile("pause" ::: "memory"); })
#if __USE_GNU
#undef __relax
#define __relax pthread_yield()
#endif

#define __read_barrier ({ __asm__ volatile("" ::: "memory"); })
#define __write_barrier \
  ({ __asm__ volatile("lock;addq $0,0(%%rsp)" ::: "cc", "memory"); })

#define __poll(rc, fn, ...) \
  rc = fn(__VA_ARGS__);     \
  if (rc == -1 && errno == EINTR) break

#define __ms() now_ms()

#define LOCK_GUARD(mutex)                                          \
  pthread_mutex_t* __lock __scoped_guard(mutex_unlock) = &(mutex); \
  pthread_mutex_lock(__lock)

#define MUTEX_INIT(x) pthread_mutex_init(&o->x, 0)
#define MUTEX_DESTROY(x) pthread_mutex_destroy(&o->x)

#define __internal static

#define GOTO_WORK(x) (x) ^= STATE_COMPOUND
#define GOTO_IDLE(x) (x) ^= STATE_COMPOUND

#define SLOT_SIZE 64
#define RUNNER_ID_MIN 0
#define RUNNER_ID_MAX 0x40000000
#ifndef RUNNER_WAITER_MAX
#define RUNNER_WAITER_MAX 32
#endif
#if RUNNER_WAITER_MAX > SLOT_SIZE
#error "invalid WAITER_MAX config for RUNNER"
#endif
#define DEFER_RUNNER_DURATION_MIN 10U /*ms*/

typedef void* (*runnable)(void*);

typedef void (*job)(void*);

typedef struct {
  job fn;
  void* arg;
  int wait;
  int id;
  queue q;
} task;

typedef struct {
  job fn;
  void* arg;
  long run_at;
  int id;
  unsigned duration : 31;
  int repeated : 1;
} defer_task;

typedef struct {
  unsigned state;
  event_fd fd;
  runnable run;
  pthread_t tid;
} base_runner;

#define MUTEX_TASK mutex
#define MUTEX_POOL mutex2
#define MUTEX_ARRAY mutex3
#define GUARD_TASK __guard(TASK)
#define GUARD_POOL __guard(POOL)
#define GUARD_ARRAY __guard(ARRAY)
#define __mutex(x) MUTEX_##x
#define __guard(x) LOCK_GUARD(o->__mutex(x))

struct runner {
  base_runner base;
  pthread_mutex_t OP_LIST(__mutex, TASK, POOL);
  queue tasks;
  int id;
  int last_id;
  unsigned long done;
#if RUNNER_WAITER_MAX
  event_fd hub;
#endif
  obj_pool* pool;
};

struct defer_runner {
  base_runner base;
  pthread_mutex_t OP_LIST(__mutex, TASK, POOL, ARRAY);
  struct heap* tasks;
  sorted_array* array;
  timer_fd fd;
  int id;
  obj_pool* pool;
};

/* private APIs */
__internal void base_runner_init(base_runner* o, runnable run) {
  o->state = STATE_NOTHINGNESS;
  o->run = run;
}

__internal void base_runner_stop(base_runner*);

__internal void base_runner_release(base_runner* o) {
  if (o->state & STATE_INITIALIZED) {
    base_runner_stop(o);
    pthread_join(o->tid, 0);
    event_fd_release(o->fd);
    o->state = STATE_NOTHINGNESS;
  }
}

__internal void base_runner_start(base_runner* o) {
  if (!(o->state & STATE_INITIALIZED)) {
    o->state |= STATE_INITIALIZED;
    o->fd = event_fd_new(0, 0);
    pthread_create(&o->tid, 0, o->run, o);
    for (; __read_barrier, !(o->state & STATE_IDLE); __relax)
      ;
  }
}

__internal void base_runner_onstart(base_runner* o) {
  o->state |= STATE_IDLE;
  __write_barrier;
}

__internal void base_runner_stop(base_runner* o) {
  if ((o->state & STATE_INITIALIZED) && !(o->state & STATE_TERMINATED)) {
    event_fd_notify(o->fd, EVENT_QUIT);
    o->state |= STATE_TERMINATED;
    __write_barrier;
  }
}

__internal void mutex_unlock(void* arg) {
  pthread_mutex_t* mutex = *(pthread_mutex_t**)arg;
  pthread_mutex_unlock(mutex);
}

__internal int compare_ts(const void* l, const void* r) { /*minimal heap*/
  long a, b;
  a = ((defer_task*)l)->run_at, b = ((defer_task*)r)->run_at;
  return a == b ? 0 : (a < b ? 1 : -1);
}

__internal int compare_id(const void* l, const void* r) { /*ascend*/
  return ((const defer_task*)l)->id - ((const defer_task*)r)->id;
}

__internal void* runner_loop(void* arg) {
  runner* o = arg;
  struct pollfd pfd = {.fd = o->base.fd, .events = POLLIN};
  unsigned long n;
  int rc;
  queue ready_tasks, *p;
  task* t;
  base_runner_onstart(&o->base);
  infinite_loop() {
    __poll(rc, poll, &pfd, 1, -1);
    n = event_fd_wait(o->base.fd);
    if (n >= EVENT_QUIT) break;
    queue_init(&ready_tasks);
    {
      GUARD_TASK;
      while (n--) {
        queue_move(&o->tasks, &ready_tasks, p);
      }
    }
    queue_foreach(&ready_tasks, p) {
      GOTO_WORK(o->base.state);
      t = queue_data(p, task, q);
      t->fn(t->arg);
      if (t->wait) {
        o->done |= 1L << (t->id % SLOT_SIZE);
        __write_barrier;
        o->last_id = t->id;
#if RUNNER_WAITER_MAX
        event_fd_notify(o->hub, EVENT_MSG); /*notify task done*/
#endif
      }
      {
        GUARD_POOL;
        obj_pool_put(o->pool, t);
      }
      GOTO_IDLE(o->base.state);
    }
  }
  return arg;
}

__internal int defer_runner_repost(defer_runner* o, defer_task* t) {
  if (t->id != -1) {
    t->run_at = __ms() + t->duration;
    {
      GUARD_TASK;
      heap_push(o->tasks, t);
    }
    event_fd_notify(o->base.fd, EVENT_MSG);
    return t->id;
  }
  return -1; /*already canceled*/
}

__internal void* defer_runner_loop(void* arg) {
  defer_runner* o = arg;
  array ready_tasks;
  array_init(&ready_tasks);
  struct pollfd pfds[2];
  pfds[0] = (struct pollfd){.fd = o->base.fd, .events = POLLIN};
  pfds[1] = (struct pollfd){.fd = o->fd, .events = POLLIN};
  unsigned i;
  unsigned long n;
  long ms;
  defer_task* t;
  int rc;
  base_runner_onstart(&o->base);
  infinite_loop() {
    array_rewind(&ready_tasks);
    __poll(rc, poll, pfds, 2, -1);
    if (rc > 0 && (pfds[0].revents & POLLIN)) { /*new task arrival*/
      n = event_fd_wait(pfds[0].fd);
      if (n >= EVENT_QUIT) break;
      --rc;
    }
    if (rc > 0 && (pfds[1].revents & POLLIN)) { /*timeout*/
      (void)event_fd_wait(pfds[1].fd);
    }
    ms = __ms();
    {
      GUARD_TASK;
      while ((t = heap_top(o->tasks))) {
        if (t->run_at >= ms + DEFER_RUNNER_DURATION_MIN) { /*merge adjacent tasks*/
          timer_fd_reset(o->fd, t->run_at - ms, 0);
          break;
        }
        heap_pop(o->tasks);
        array_push(&ready_tasks, t);
      }
    }
    for (i = 0; i < ready_tasks.i; i++) {
      t = ready_tasks.a[i];
      if (t->id != -1) {
        t->fn(t->arg); /*may take a long time*/
        if (t->repeated && defer_runner_repost(o, t) != -1) {
          continue;
        }
      }
      if (t->id != -1) {
        GUARD_ARRAY;
        sorted_array_erase(o->array, t);
      }
      {
        GUARD_POOL;
        obj_pool_put(o->pool, t);
      }
    }
  }
  array_release(&ready_tasks);
  return arg;
}

/* public APIs */
runner* runner_new() {
  runner* o = malloc(sizeof(*o));
  base_runner_init(&o->base, &runner_loop);
  OP_LIST(MUTEX_INIT, OP_LIST(__mutex, TASK, POOL));
  queue_init(&o->tasks);
  o->id = RUNNER_ID_MIN;
  o->last_id = -1;
  o->done = 0;
#if RUNNER_WAITER_MAX
  o->hub = event_fd_new(0, EFD_SEMAPHORE);
#endif
  o->pool = obj_pool_new(sizeof(task), 256);
  return o;
}

void runner_release(runner* o) {
  base_runner_release(&o->base);
  obj_pool_release(o->pool);
#if RUNNER_WAITER_MAX
  event_fd_release(o->hub);
#endif
  OP_LIST(MUTEX_DESTROY, OP_LIST_R(__mutex, TASK, POOL));
  free(o);
}

void runner_start(runner* o) { base_runner_start((base_runner*)o); }

void runner_stop(runner* o) {
  base_runner_stop((base_runner*)o);
#if RUNNER_WAITER_MAX
  event_fd_notify(o->hub, EVENT_QUIT);
#endif
}

int runner_post(runner* o, void (*fn)(void*), void* arg, int wait) {
  if (o->base.state & STATE_TERMINATED) {
    return -2;
  }
  if (fn) {
    task* t;
    {
      GUARD_POOL;
      t = obj_pool_get(o->pool);
      t->fn = fn;
      t->arg = arg;
      t->wait = wait;
      t->id = -1;
    }
    {
      GUARD_TASK;
      if (wait) {
        if (o->id == RUNNER_ID_MAX) o->id = RUNNER_ID_MIN;
        t->id = o->id++; /*ID is valuable*/
      }
      queue_push(&o->tasks, &t->q);
    }
    event_fd_notify(o->base.fd, EVENT_MSG); /*notify task arrival*/
    return t->id;
  }
  return -1;
}

void runner_wait(runner* o, int id) {
  if (id < RUNNER_ID_MIN || id >= RUNNER_ID_MAX) {
    return;
  }
  unsigned long m = 1L << (id % SLOT_SIZE);
  for (; !(__read_barrier, o->base.state & STATE_TERMINATED);) {
    if (id >= o->last_id && id <= o->last_id + RUNNER_WAITER_MAX) {
#if RUNNER_WAITER_MAX
      (void)event_fd_wait(o->hub);
#endif
      if (__read_barrier, (o->base.state & STATE_TERMINATED) || (o->done & m)) {
        o->done &= ~m;
        __write_barrier;
        break;
      }
#if RUNNER_WAITER_MAX
      event_fd_notify(o->hub, EVENT_MSG); /*false consume, supply one*/
#endif
    }
    __relax;
  }
}

defer_runner* defer_runner_new() {
  defer_runner* o = malloc(sizeof(*o));
  base_runner_init((base_runner*)o, &defer_runner_loop);
  OP_LIST(MUTEX_INIT, OP_LIST(__mutex, TASK, POOL, ARRAY));
  o->tasks = heap_new(ARRAY_FIXED_SIZE, &compare_ts);
  o->array = sorted_array_new(&compare_id);
  o->fd = timer_fd_new(O_NONBLOCK);
  o->id = RUNNER_ID_MIN;
  o->pool = obj_pool_new(sizeof(defer_task), 256);
  return o;
}

void defer_runner_release(defer_runner* o) {
  if (o) {
    base_runner_release((base_runner*)o);
    obj_pool_release(o->pool);
    timer_fd_release(o->fd);
    sorted_array_release(o->array);
    heap_release(o->tasks);
    OP_LIST(MUTEX_DESTROY, OP_LIST_R(__mutex, TASK, POOL, ARRAY));
    free(o);
  }
}

void defer_runner_start(defer_runner* o) { base_runner_start((base_runner*)o); }

void defer_runner_stop(defer_runner* o) { base_runner_stop((base_runner*)o); }

int defer_runner_post(defer_runner* o, void (*fn)(void*), void* arg, unsigned duration,
                      int repeated) {
  if (fn && duration >= DEFER_RUNNER_DURATION_MIN) {
    defer_task* t;
    {
      GUARD_POOL;
      t = obj_pool_get(o->pool);
    }
    t->fn = fn;
    t->arg = arg;
    t->duration = duration;
    t->repeated = repeated != 0;
    t->run_at = __ms() + t->duration;
    {
      GUARD_ARRAY;
      if (o->id == RUNNER_ID_MAX) { /*slow id alloc*/
        array* p = (array*)o->array;
        if (!p->i) {
          o->id = RUNNER_ID_MIN; /*switch to fast version*/
          t->id = o->id++;
        } else {
          unsigned i, avail = RUNNER_ID_MIN;
          defer_task* x;
          for (i = 0; i < p->i; i++) {
            x = p->a[i];
            if (avail != x->id) {
              break;
            }
            avail++;
          }
          /* TODO how about avail reach to ID_MAX? */
          t->id = (int)avail;
        }
      } else { /*fast id alloc*/
        t->id = o->id++;
      }
      sorted_array_insert(o->array, t);
    }
    {
      GUARD_TASK;
      heap_push(o->tasks, t);
    }
    event_fd_notify(o->base.fd, EVENT_MSG);
    return t->id;
  }
  return -1;
}

void defer_runner_cancel(defer_runner* o, int id) {
  if (id < RUNNER_ID_MIN || id >= RUNNER_ID_MAX) {
    return;
  }
  defer_task t, *p;
  t.id = id;
  GUARD_ARRAY;
  p = sorted_array_erase(o->array, &t);
  if (p) p->id = -1;
}

#undef __guard
#undef __mutex
