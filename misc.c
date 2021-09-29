#include "misc.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/syscall.h>

#include "ipc.h"

const char *default_datetime_format(char buf[DATETIME_BUF_LEN], int sec, int us) {
    snprintf(buf, DATETIME_BUF_LEN, "%d.%06d", sec, us);
    return buf;
}

const char *now_string(char buf[DATETIME_BUF_LEN], datetime_format format) {
    static __thread char __buf[DATETIME_BUF_LEN];
    char *p = buf ?: __buf;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (format ?: default_datetime_format)(p, (int) ts.tv_sec, (int) ts.tv_nsec / 1000);
}

long now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long) ts.tv_sec * 1000 + (long) ts.tv_nsec / 1000000;
}

int __pid() {
    static int pid;
    if (!pid) {
        pid = getpid();
    }
    return pid;
}

int __tid() {
    static __thread int tid;
    if (!tid) {
        tid = (int) syscall(SYS_gettid);
    }
    return tid;
}

int nap(int ms) {
    static event_fd fd;
    if (!fd) {
        fd = event_fd_new(0, 0);
    }
    struct pollfd pfd = {.fd=fd, .events=POLLIN};
    return poll(&pfd, 1, ms);
}
