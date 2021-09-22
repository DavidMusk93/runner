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
# ifndef TASK_SIZE
#  define TASK_SIZE 10
# endif
    // runner test
    runner*r=runner_new();
    runner_start(r);
    for(i=0;i<TASK_SIZE;i++){
        runner_post(r,&foo, vp_cast(i),0);
    }
    int id=runner_post(r,&bar,0,1);
    runner_wait(r,id);
    runner_stop(r);
    runner_release(r);
#endif
#if DEFER_RUNNER_TEST
    // defer_runner test
# define DEFER_TASK_SIZE 2
    defer_runner*r2=defer_runner_new();
    defer_runner_start(r2);
    for(i=0;i<DEFER_TASK_SIZE;i++){
        defer_runner_post(r2,&foo, vp_cast(i),i*500+500,0);
    }
    id=defer_runner_post(r2,&baz,0,1000,1);
    nap(5000);
    defer_runner_cancel(r2,id);
    nap(2000);
    defer_runner_stop(r2);
    defer_runner_release(r2);
#endif
    return 0;
}
