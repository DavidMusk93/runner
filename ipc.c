#include "ipc.h"

#include <sys/eventfd.h>
#include <unistd.h>

event_fd event_fd_new(int count,int flags){
    return eventfd(count,flags);
}

void event_fd_release(event_fd fd){
    close(fd);
}

void event_fd_notify(event_fd fd,unsigned long e){
    (void)write(fd,&e,sizeof e);
}

unsigned long event_fd_wait(event_fd fd){
    unsigned long e=0;
    (void)read(fd,&e,sizeof e);
    return e;
}

#if TEST_IPC
#include <stdio.h>
#include "macro.h"

MAIN(){
    log1("%ld,%ld",sizeof(EVENT_MSG),sizeof(EVENT_QUIT));
}
#endif
