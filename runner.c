#include "runner.h"

#include <pthread.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <sys/eventfd.h>

#include "ipc.h"
#include "state.h"
#include "macro.h"

#define __relax ({__asm__ volatile("pause":::"memory");})
#if __USE_GNU
# undef __relax
# define __relax pthread_yield()
#endif

#define __read_barrier ({__asm__ volatile("":::"memory");})
#define __write_barrier ({__asm__ volatile("lock;addq $0,0(%%rsp)":::"cc","memory");})

#define __poll(rc,fn,...) \
rc=fn(__VA_ARGS__);\
if(rc==-1&&errno==EINTR)break

#define GOTO_WORK(x) (x)^=STATE_COMPOUND
#define GOTO_IDLE(x) (x)^=STATE_COMPOUND

#define SLOT_SIZE 64
#define RUNNER_ID_MIN 0
#define RUNNER_ID_MAX 0x40000000
#ifndef RUNNER_WAITER_MAX
# define RUNNER_WAITER_MAX 32
#endif
#if RUNNER_WAITER_MAX>SLOT_SIZE
# error "invalid MAX_WAITERS config for RUNNER"
#endif

typedef void*(*runnable)(void*);

typedef struct{
    unsigned state;
    event_fd fd;
    runnable run;
    pthread_t tid;
}base_runner;

void base_runner_init(base_runner*o,runnable run){
    o->state=STATE_NOTHINGNESS;
    o->run=run;
}

void base_runner_release(base_runner*o){
    if(o->state&STATE_INITIALIZED){
        pthread_join(o->tid,0);
        event_fd_release(o->fd);
    }
}

void base_runner_start(base_runner*o){
    if(!(o->state&STATE_INITIALIZED)){
        o->state|=STATE_INITIALIZED;
        o->fd= event_fd_new(0,0);
        pthread_create(&o->tid,0,o->run,o);
        for(;__read_barrier,!(o->state&STATE_IDLE);__relax);
    }
}

void base_runner_stop(base_runner*o){
    if(!(o->state&STATE_TERMINATED)){
        event_fd_notify(o->fd,EVENT_QUIT);
        o->state|=STATE_TERMINATED;
        __write_barrier;
    }
}

void task_init(task*o,job fn,void*arg,int wait){
    o->fn=fn;
    o->arg=arg;
    o->wait=wait;
    o->id=-1;
}

void runner_wait(struct runner*o,int id);
void task_wait(task*o,struct runner*r){
    if(o->id>=RUNNER_ID_MIN&&o->id<=RUNNER_ID_MAX){
        runner_wait(r,o->id);
        o->id=-1; /*make sure wait once*/
    }
}

struct runner{
    base_runner base;
    pthread_mutex_t mutex;
    queue tasks;
    int id;
    int last_id;
    unsigned long done;
#if RUNNER_WAITER_MAX
    event_fd hub;
#endif
};

void*runner_loop(void*arg){
    struct runner*o=arg;
    struct pollfd pfd={.fd=o->base.fd,.events=POLLIN};
    unsigned long n;
    int rc;
    queue tasks,*p;
    task*t;
    o->base.state|=STATE_IDLE;
    __write_barrier;
    infinite_loop(){
        __poll(rc,poll,&pfd,1,-1);
        n= event_fd_wait(o->base.fd);
        if(n>=EVENT_QUIT)
            break;
        queue_init(&tasks);
        pthread_mutex_lock(&o->mutex);
        while(n--){
            queue_move(&o->tasks,&tasks,p);
        }
        pthread_mutex_unlock(&o->mutex);
        queue_foreach(&tasks,p){
            GOTO_WORK(o->base.state);
            t= queue_data(p,task,q);
            t->fn(t->arg);
            if(t->wait){
                o->done|=1L<<(t->id%SLOT_SIZE);
                __write_barrier;
                o->last_id=t->id;
#if RUNNER_WAITER_MAX
                event_fd_notify(o->hub,EVENT_MSG); /*notify task done*/
#endif
            }
            GOTO_IDLE(o->base.state);
        }
    }
    return arg;
}

struct runner*runner_new(){
    struct runner*o= malloc(sizeof(*o));
    base_runner_init(&o->base,&runner_loop);
    pthread_mutex_init(&o->mutex,0);
    queue_init(&o->tasks);
    o->id=RUNNER_ID_MIN;
    o->last_id=-1;
    o->done=0;
#if RUNNER_WAITER_MAX
    o->hub= event_fd_new(0,EFD_SEMAPHORE);
#endif
    return o;
}

void runner_release(struct runner*o){
#if RUNNER_WAITER_MAX
    event_fd_release(o->hub);
#endif
    pthread_mutex_destroy(&o->mutex);
    base_runner_release(&o->base);
    free(o);
}

void runner_start(struct runner*o){
    base_runner_start((base_runner*)o);
}

void runner_stop(struct runner*o){
    base_runner_stop((base_runner*)o);
#if RUNNER_WAITER_MAX
    event_fd_notify(o->hub,EVENT_QUIT);
#endif
}

int runner_post(struct runner*o,task*t){
    if(o->base.state&STATE_TERMINATED){
        return -2;
    }
    if(t->fn){
        pthread_mutex_lock(&o->mutex);
        if(t->wait){
            if(o->id==RUNNER_ID_MAX)
                o->id=RUNNER_ID_MIN;
            t->id=o->id++; /*ID is valuable*/
        }
        queue_push(&o->tasks,&t->q);
        pthread_mutex_unlock(&o->mutex);
        event_fd_notify(o->base.fd,EVENT_MSG); /*notify task arrival*/
        return t->id;
    }
    return -1;
}

void runner_wait(struct runner*o,int id){
    unsigned long m=1L<<(id%SLOT_SIZE);
    for(;!(__read_barrier,o->base.state&STATE_TERMINATED);){
        if(id>=o->last_id&&id<=o->last_id+RUNNER_WAITER_MAX){
#if RUNNER_WAITER_MAX
            (void) event_fd_wait(o->hub);
#endif
            if(__read_barrier,(o->base.state&STATE_TERMINATED)||(o->done&m)){
                o->done&=~m;
                __write_barrier;
                break;
            }
#if RUNNER_WAITER_MAX
            event_fd_notify(o->hub,EVENT_MSG); /*false consume, supply one*/
#endif
        }
        __relax;
    }
}
