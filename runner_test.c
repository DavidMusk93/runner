#include "test.h"

#include "runner.h"

void foo(void*arg){
    int n=(int)(unsigned long)arg;
    log("#%d start",n);
    nap((n%10)*100+100);
    log("end");
}

void bar(void*arg){
    (void)arg;
    log("last job");
}

void baz(void*arg){
    (void)arg;
    log("repeated job");
}

#define RUNNER_TEST 1
#define DEFER_RUNNER_TEST 1

MAIN(){
    ALWAYS_FLUSH_OUTPUT();
#define vp_cast(x) (void*)(unsigned long)(x)
    int i;
#if RUNNER_TEST
# define TASK_SIZE 20
    // runner test
    task tasks[TASK_SIZE+1];
    struct runner*r=runner_new();
    runner_start(r);
    for(i=0;i<TASK_SIZE;i++){
        task_init(&tasks[i],&foo, vp_cast(i),0);
        runner_post(r,&tasks[i]);
    }
    task_init(&tasks[TASK_SIZE],&bar,0,1);
    runner_post(r,&tasks[TASK_SIZE]);
    task_wait(&tasks[TASK_SIZE],r);
    runner_stop(r);
    runner_release(r);
#endif
#if DEFER_RUNNER_TEST
    // defer_runner test
# define DEFER_TASK_SIZE 2
    struct defer_runner*r2=defer_runner_new();
    defer_runner_start(r2);
    defer_task t2[DEFER_TASK_SIZE+1];
    for(i=0;i<DEFER_TASK_SIZE;i++){
        defer_task_init(&t2[i],&foo, vp_cast(i),i*500+500,0);
        defer_runner_post(r2,&t2[i]);
    }
    defer_task_init(&t2[DEFER_TASK_SIZE],&baz,0,1000,1);
    defer_runner_post(r2,&t2[DEFER_TASK_SIZE]);
    nap(5000);
    defer_task_cancel(&t2[DEFER_TASK_SIZE],r2);
    nap(2000);
    defer_runner_stop(r2);
    defer_runner_release(r2);
#endif
    return 0;
}
