#include "ipc.h"

#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

event_fd event_fd_new(int count, int flags) {
    return eventfd(count, flags);
}

void event_fd_release(event_fd fd) {
    close(fd);
}

void event_fd_notify(event_fd fd, unsigned long e) {
    (void) write(fd, &e, sizeof e);
}

unsigned long event_fd_wait(event_fd fd) {
    unsigned long e = 0;
    (void) read(fd, &e, sizeof e);
    return e;
}

timer_fd timer_fd_new(int flags) {
    return timerfd_create(CLOCK_MONOTONIC, flags);
}

void timer_fd_release(timer_fd fd) {
    close(fd);
}

int timer_fd_reset(timer_fd fd, unsigned ms, int repeated) {
#define THOUSAND 1000
    struct itimerspec its = {};
    its.it_value.tv_sec = ms / THOUSAND;
    its.it_value.tv_nsec = (ms % THOUSAND) * THOUSAND * THOUSAND;
    if (repeated)
        its.it_interval = its.it_value;
    return timerfd_settime(fd, 0, &its, 0);
}

#if TEST_IPC
#include <stdio.h>
#include "macro.h"

MAIN(){
    log1("%ld,%ld",sizeof(EVENT_MSG),sizeof(EVENT_QUIT));
}
#endif
