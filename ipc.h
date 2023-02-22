#pragma once

enum {
#define __event(t, x) EVENT_##t = x
  __event(MSG, 1UL),
  __event(TASK, 1UL << 31),
  __event(QUIT, 1UL << 63),
#undef __event
};

typedef int event_fd;
typedef int timer_fd;

event_fd event_fd_new(int count, int flags);
void event_fd_release(event_fd fd);
void event_fd_notify(event_fd fd, unsigned long e);
unsigned long event_fd_wait(event_fd fd);

timer_fd timer_fd_new(int flags);
void timer_fd_release(timer_fd fd);
int timer_fd_reset(timer_fd fd, unsigned ms, int repeated);
