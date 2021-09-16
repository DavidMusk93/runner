#include "runner.h"

#include <pthread.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>

#include "ipc.h"
#include "state.h"
#include "macro.h"
#include "misc.h"
#include "heap.h"

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

#define __ms() now_ms()

#define LOCK_GUARD(mutex) \
pthread_mutex_t*__lock __scoped_guard(mutex_unlock)=&(mutex);\
pthread_mutex_lock(__lock)

#define __internal static

#define GOTO_WORK(x) (x)^=STATE_COMPOUND
#define GOTO_IDLE(x) (x)^=STATE_COMPOUND

#define SLOT_SIZE 64
#define RUNNER_ID_MIN 0
#define RUNNER_ID_MAX 0x40000000
#ifndef RUNNER_WAITER_MAX
# define RUNNER_WAITER_MAX 32
#endif
#if RUNNER_WAITER_MAX>SLOT_SIZE
# error "invalid WAITER_MAX config for RUNNER"
#endif
#define DEFER_RUNNER_DURATION_MIN 10U /*ms*/

typedef struct{
    void*fixed_array[4096];
    void**a;
    unsigned i,n;
    compare cmp;
}sorted_array;

__internal sorted_array*sorted_array_new(compare cmp){
    sorted_array*o= malloc(sizeof(*o));
    o->a=&o->fixed_array[0];
    o->i=0;
    o->n= dimension_of(o->fixed_array);
    o->cmp=cmp;
    return o;
}

__internal void sorted_array_release(sorted_array*o){
    if(o){
        if(o->a!=&o->fixed_array[0])
            free(o->a);
        free(o);
    }
}

__internal void sorted_array_expand(sorted_array*o){
    unsigned i;
    void**a=&o->fixed_array[0];
    o->a= realloc(o->a==a?0:o->a,o->n*2);
    if(o->n== dimension_of(o->fixed_array)){
        for(i=0;i<o->n;i++){
            o->a[i]=a[i];
        }
    }
    o->n*=2;
}

__internal void sorted_array_put(sorted_array*o,void*e){
    unsigned i=o->i;
    if(i==o->n)
        sorted_array_expand(o);
    o->a[o->i++]=e;
    // bubble sort
    for(;i>0;i--){
        if(o->cmp(o->a[i],o->a[i-1])>=0)
            break;
        __swap(o->a[i],o->a[i-i]);
    }
}

__internal unsigned sorted_array_search(sorted_array*o,void*e){
    unsigned i,j,m; /*(i,j]*/
    int x;
    i=0,j=o->i,m=i+(j-i)/2;
    // binary search
    for(;;m=i+(j-i)/2){
        if(i==m)
            break;
        x=o->cmp(e,o->a[m]);
        if(x==0){
            break;
        }else if(x<0){
            j=m;
        }else{
            i=m+1;
        }
    }
    return m;
}

__internal void*sorted_array_erase(sorted_array*o,void*e){
    if(!o->i)
        return 0;
    unsigned i=sorted_array_search(o,e);
    if(o->cmp(e,o->a[i])!=0)
        return 0; /*not found*/
    for(;i+1<o->i;i++){
        __swap(o->a[i],o->a[i+1]);
    }
    o->i--;
    return e;
}

void array_init(sorted_array*o){
    o->a=&o->fixed_array[0];
    o->i=0;
    o->n= dimension_of(o->fixed_array);
    o->cmp=0;
}

void array_release(sorted_array*o){
    if(o&&o->a!=&o->fixed_array[0]){
        free(o->a);
    }
}

__internal void array_push(sorted_array*o,void*e){
    if(o->i==o->n)
        sorted_array_expand(o);
    o->a[o->i++]=e;
}

__internal void array_rewind(sorted_array*o){
    o->i=0;
}

typedef void*(*runnable)(void*);

typedef struct{
    unsigned state;
    event_fd fd;
    runnable run;
    pthread_t tid;
}base_runner;

__internal void base_runner_init(base_runner*o,runnable run){
    o->state=STATE_NOTHINGNESS;
    o->run=run;
}

__internal void base_runner_release(base_runner*o){
    if(o->state&STATE_INITIALIZED){
        pthread_join(o->tid,0);
        event_fd_release(o->fd);
    }
}

__internal void base_runner_start(base_runner*o){
    if(!(o->state&STATE_INITIALIZED)){
        o->state|=STATE_INITIALIZED;
        o->fd= event_fd_new(0,0);
        pthread_create(&o->tid,0,o->run,o);
        for(;__read_barrier,!(o->state&STATE_IDLE);__relax);
    }
}

__internal void base_runner_onstart(base_runner*o){
    o->state|=STATE_IDLE;
    __write_barrier;
}

__internal void base_runner_stop(base_runner*o){
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

__internal void runner_wait(struct runner*o,int id);
void task_wait(task*o,struct runner*r){
    if(o->id>=RUNNER_ID_MIN&&o->id<RUNNER_ID_MAX){
        runner_wait(r,o->id);
        o->id=-1; /*make sure wait once*/
    }
}

void defer_task_init(defer_task*o,job fn,void*arg,unsigned duration,int repeated){
    o->fn=fn;
    o->arg=arg;
    o->id=-1;
    o->run_at=0;
    o->duration=duration;
    o->repeated=repeated!=0;
}

__internal void defer_runner_cancel(struct defer_runner*o,void*e);
void defer_task_cancel(defer_task*o,struct defer_runner*r){
    if(o->id>=RUNNER_ID_MIN&&o->id<RUNNER_ID_MAX){
        defer_runner_cancel(r,o);
        o->id=-1;
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

struct defer_runner{
    base_runner base;
    pthread_mutex_t mutex,mutex2;
    struct heap*tasks;
    sorted_array*array;
    timer_fd fd;
    int id;
};

__internal void*runner_loop(void*arg){
    struct runner*o=arg;
    struct pollfd pfd={.fd=o->base.fd,.events=POLLIN};
    unsigned long n;
    int rc;
    queue tasks,*p;
    task*t;
    base_runner_onstart(&o->base);
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
    base_runner_release(&o->base);
#if RUNNER_WAITER_MAX
    event_fd_release(o->hub);
#endif
    pthread_mutex_destroy(&o->mutex);
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

__internal void runner_wait(struct runner*o,int id){
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

__internal void mutex_unlock(void*arg){
    pthread_mutex_t*mutex=*(pthread_mutex_t**)arg;
    pthread_mutex_unlock(mutex);
}

__internal void*defer_runner_loop(void*arg){
    struct defer_runner*o=arg;
    sorted_array ready_tasks;
    array_init(&ready_tasks);
    struct pollfd pfds[2];
    pfds[0]=(struct pollfd){.fd=o->base.fd,.events=POLLIN};
    pfds[1]=(struct pollfd){.fd=o->fd,.events=POLLIN};
    unsigned i;
    unsigned long n;
    long ms;
    defer_task*t;
    int rc;
    base_runner_onstart(&o->base);
    infinite_loop(){
        n=0;
        array_rewind(&ready_tasks);
        __poll(rc,poll,pfds,2,-1);
        if(rc>0&&(pfds[0].revents&POLLIN)){ /*new task arrival*/
            n= event_fd_wait(pfds[0].fd);
            if(n>=EVENT_QUIT)
                break;
            --rc;
        }
        if(rc>0&&(pfds[1].revents&POLLIN)){ /*timeout*/
            (void)event_fd_wait(pfds[1].fd);
        }
        {
            ms=__ms();
            LOCK_GUARD(o->mutex);
            while((t= heap_top(o->tasks))){
                if(t->run_at>=ms+DEFER_RUNNER_DURATION_MIN){ /*merge adjacent tasks*/
                    timer_fd_reset(o->fd,t->run_at-ms,0);
                    break;
                }
                heap_pop(o->tasks);
                array_push(&ready_tasks,t);
            }
        }
        for(i=0;i<ready_tasks.i;i++){
            t=ready_tasks.a[i];
            {
                LOCK_GUARD(o->mutex2);
                if(!sorted_array_erase(o->array,t)) /*canceled*/
                    continue;
            }
            t->fn(t->arg); /*may take a long time*/
            if(t->repeated){
                defer_runner_post(o,t); /*repost repeated task*/
            }
        }
    }
    array_release(&ready_tasks);
    return arg;
}

__internal int compare_ts(const void*l,const void*r){ /*minimal heap*/
    long a,b;
    a=((defer_task*)l)->run_at,b=((defer_task*)r)->run_at;
    return a==b?0:(a<b?1:-1);
}

__internal int compare_pointer(const void*l,const void*r){ /*ascend*/
    long a,b;
    a=(long)l,b=(long)r;
    return a==b?0:(a<b?-1:1);
}

struct defer_runner*defer_runner_new(){
    struct defer_runner*o= malloc(sizeof(*o));
    base_runner_init((base_runner*)o,&defer_runner_loop);
    pthread_mutex_init(&o->mutex,0);
    pthread_mutex_init(&o->mutex2,0);
    o->tasks= heap_new(dimension_of(o->array->fixed_array),&compare_ts);
    o->array= sorted_array_new(&compare_pointer);
    o->fd= timer_fd_new(O_NONBLOCK);
    o->id=RUNNER_ID_MIN;
    return o;
}

void defer_runner_release(struct defer_runner*o){
    if(o){
        base_runner_release((base_runner*)o);
        timer_fd_release(o->fd);
        sorted_array_release(o->array);
        heap_release(o->tasks);
        pthread_mutex_destroy(&o->mutex2);
        pthread_mutex_destroy(&o->mutex);
        free(o);
    }
}

void defer_runner_start(struct defer_runner*o){
    base_runner_start((base_runner*)o);
}

void defer_runner_stop(struct defer_runner*o){
    base_runner_stop((base_runner*)o);
}

int defer_runner_post(struct defer_runner*o,defer_task*t){
    if(t&&t->fn&&t->duration>=DEFER_RUNNER_DURATION_MIN){
        t->run_at=__ms()+t->duration;
        {
            LOCK_GUARD(o->mutex);
            heap_push(o->tasks,t);
            if(o->id==RUNNER_ID_MAX)
                o->id=RUNNER_ID_MIN;
            t->id=o->id++;
        }
        {
            LOCK_GUARD(o->mutex2);
            sorted_array_put(o->array,t);
        }
        event_fd_notify(o->base.fd,EVENT_MSG);
        return t->id;
    }
    return -1;
}

__internal void defer_runner_cancel(struct defer_runner*o,void*e){
    LOCK_GUARD(o->mutex2);
    sorted_array_erase(o->array,e);
}
