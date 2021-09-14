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

MAIN(){
    ALWAYS_FLUSH_OUTPUT();
#define TASK_SIZE 20
    task tasks[TASK_SIZE+1];
    int i;
    struct runner*r=runner_new();
    runner_start(r);
    for(i=0;i<TASK_SIZE;i++){
        task_init(&tasks[i],&foo,(void*)(unsigned long)i,0);
        runner_post(r,&tasks[i]);
    }
    task_init(&tasks[TASK_SIZE],&bar,0,1);
    runner_post(r,&tasks[TASK_SIZE]);
    task_wait(&tasks[TASK_SIZE],r);
    runner_stop(r);
    runner_release(r);
    return 0;
}