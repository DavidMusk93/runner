#pragma once

#include "queue.h"

typedef void (*job)(void*);

typedef struct{
    job fn;
    void*arg;
    int wait;
    int id;
    queue q;
}task;

typedef struct{
    job fn;
    void*arg;
    long run_at;
    int id;
    unsigned duration:31;
    unsigned repeated:1;
}defer_task;

struct runner;
struct defer_runner;

void task_init(task*o,job fn,void*arg,int wait);
void task_wait(task*o,struct runner*r);

void defer_task_init(defer_task*o,job fn,void*arg,unsigned duration,int repeated);
void defer_task_cancel(defer_task*o,struct defer_runner*r);

struct runner*runner_new();
void runner_release(struct runner*o);
void runner_start(struct runner*o);
void runner_stop(struct runner*o);
int runner_post(struct runner*o,task*t);

struct defer_runner*defer_runner_new();
void defer_runner_release(struct defer_runner*o);
void defer_runner_start(struct defer_runner*o);
void defer_runner_stop(struct defer_runner*o);
int defer_runner_post(struct defer_runner*o,defer_task*t);
