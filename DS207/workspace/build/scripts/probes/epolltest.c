#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
int main(void){
    int ep = epoll_create1(0);
    printf("epoll_create1=%d errno=%d(%s)\n", ep, errno, strerror(errno));
    int p[2]; if(pipe(p)){perror("pipe");return 1;}
    struct epoll_event ev; memset(&ev,0,sizeof ev); ev.events=EPOLLIN; ev.data.fd=p[0];
    int r = epoll_ctl(ep, EPOLL_CTL_ADD, p[0], &ev);
    printf("epoll_ctl(ADD)=%d errno=%d(%s)\n", r, errno, strerror(errno));
    write(p[1], "x", 1);
    struct epoll_event out[4]; memset(out,0,sizeof out);
    r = epoll_wait(ep, out, 4, 2000);
    printf("epoll_wait=%d (EXPECT 1) errno=%d(%s)\n", r, errno, strerror(errno));
    if(r>0) printf("  returned fd=%d events=0x%x (EPOLLIN=0x%x)\n", out[0].data.fd, out[0].events, EPOLLIN);
#ifdef SYS_epoll_wait
    printf("SYS_epoll_wait=%d ",(int)SYS_epoll_wait);
#endif
#ifdef SYS_epoll_create
    printf("SYS_epoll_create=%d ",(int)SYS_epoll_create);
#endif
    printf("\n");
    return 0;
}
