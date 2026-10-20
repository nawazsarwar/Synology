#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>
int main(void){
    int s = socket(AF_INET, SOCK_STREAM, 0);
    printf("socket(AF_INET,SOCK_STREAM,0)=%d errno=%d (%s)\n", s, errno, strerror(errno));
    int s2 = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC, 0);
    printf("socket(...|SOCK_CLOEXEC)=%d errno=%d (%s)\n", s2, errno, strerror(errno));
#ifdef SYS_socket
    printf("SYS_socket=%d ", (int)SYS_socket);
#endif
#ifdef SYS_socketcall
    printf("SYS_socketcall=%d", (int)SYS_socketcall);
#endif
    printf("\n");
    if (s>=0){
        struct sockaddr_in a; memset(&a,0,sizeof a);
        a.sin_family=AF_INET; a.sin_port=htons(4499); a.sin_addr.s_addr=INADDR_ANY;
        int one=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&one,sizeof one);
        int r=bind(s,(struct sockaddr*)&a,sizeof a);
        printf("bind(4499)=%d errno=%d (%s)\n", r, errno, strerror(errno));
        r=listen(s,5); printf("listen=%d errno=%d (%s)\n", r, errno, strerror(errno));
    }
    return 0;
}
