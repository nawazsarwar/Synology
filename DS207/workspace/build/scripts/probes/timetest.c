#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/syscall.h>
int main(void){
    time_t t = time(NULL);
    struct timeval tv; gettimeofday(&tv,NULL);
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    printf("time()        = %lld\n", (long long)t);
    printf("gettimeofday  = %lld.%06ld\n", (long long)tv.tv_sec, (long)tv.tv_usec);
    printf("clock_gettime = %lld.%09ld\n", (long long)ts.tv_sec, (long)ts.tv_nsec);
#ifdef SYS_clock_gettime64
    printf("SYS_clock_gettime64=%d ", (int)SYS_clock_gettime64);
#endif
#ifdef SYS_clock_gettime
    printf("SYS_clock_gettime=%d ", (int)SYS_clock_gettime);
#endif
#ifdef SYS_gettimeofday
    printf("SYS_gettimeofday=%d ", (int)SYS_gettimeofday);
#endif
    printf("\n(correct ~1.79e9 for 2026)\n");
    return 0;
}
