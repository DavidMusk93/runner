#pragma once

typedef struct runner runner;
typedef struct defer_runner defer_runner;

runner* runner_new();
void runner_release(runner* o);
void runner_start(runner* o);
void runner_stop(runner* o);
int runner_post(runner* o, void (*fn)(void*), void* arg, int wait);
void runner_wait(runner* o, int id);

defer_runner* defer_runner_new();
void defer_runner_release(defer_runner* o);
void defer_runner_start(defer_runner* o);
void defer_runner_stop(defer_runner* o);
int defer_runner_post(defer_runner* o, void (*fn)(void*), void* arg, unsigned duration,
                      int repeated);
void defer_runner_cancel(defer_runner* o, int id);
