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

struct runner;

void task_init(task*o,job fn,void*arg,int wait);
void task_wait(task*o,struct runner*r);

struct runner*runner_new();
void runner_release(struct runner*o);
void runner_start(struct runner*o);
void runner_stop(struct runner*o);
int runner_post(struct runner*o,task*t);
