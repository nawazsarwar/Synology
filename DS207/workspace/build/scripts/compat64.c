/* compat64.c — provide glibc _TIME_BITS=64 symbol names that Samba references
 * but zig's musl/powerpc libc does not export. musl is already 64-bit time,
 * so each is a thin passthrough. We bind the REAL musl entry points via asm
 * labels to bypass any header-level time64 redirection. */
#define _GNU_SOURCE
#include <time.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

extern time_t        r_time(time_t*)                                 __asm__("time");
extern int           r_utime(const char*,const struct utimbuf*)      __asm__("utime");
extern int           r_utimes(const char*,const struct timeval*)     __asm__("utimes");
extern int           r_stat(const char*,struct stat*)                __asm__("stat");
extern int           r_fstat(int,struct stat*)                       __asm__("fstat");
extern int           r_lstat(const char*,struct stat*)               __asm__("lstat");
extern int           r_fstatat(int,const char*,struct stat*,int)     __asm__("fstatat");
extern int           r_gettimeofday(struct timeval*,void*)           __asm__("gettimeofday");
extern int           r_clock_gettime(clockid_t,struct timespec*)     __asm__("clock_gettime");
extern int           r_clock_settime(clockid_t,const struct timespec*) __asm__("clock_settime");
extern int           r_nanosleep(const struct timespec*,struct timespec*) __asm__("nanosleep");
extern struct tm*    r_localtime_r(const time_t*,struct tm*)         __asm__("localtime_r");
extern struct tm*    r_gmtime_r(const time_t*,struct tm*)            __asm__("gmtime_r");
extern time_t        r_mktime(struct tm*)                            __asm__("mktime");
extern time_t        r_timegm(struct tm*)                            __asm__("timegm");
extern int           r_futimens(int,const struct timespec*)          __asm__("futimens");
extern int           r_utimensat(int,const char*,const struct timespec*,int) __asm__("utimensat");

__attribute__((weak)) time_t     __time64(time_t*t){ return r_time(t); }
__attribute__((weak)) int        __utime64(const char*p,const struct utimbuf*t){ return r_utime(p,t); }
__attribute__((weak)) int        __utimes_time64(const char*p,const struct timeval t[2]){ return r_utimes(p,t); }
__attribute__((weak)) int        __stat_time64(const char*p,struct stat*b){ return r_stat(p,b); }
__attribute__((weak)) int        __fstat_time64(int fd,struct stat*b){ return r_fstat(fd,b); }
__attribute__((weak)) int        __lstat_time64(const char*p,struct stat*b){ return r_lstat(p,b); }
__attribute__((weak)) int        __fstatat_time64(int d,const char*p,struct stat*b,int f){ return r_fstatat(d,p,b,f); }
__attribute__((weak)) int        __gettimeofday64(struct timeval*tv,void*tz){ return r_gettimeofday(tv,tz); }
__attribute__((weak)) int        __clock_gettime64(clockid_t c,struct timespec*t){ return r_clock_gettime(c,t); }
__attribute__((weak)) int        __clock_settime64(clockid_t c,const struct timespec*t){ return r_clock_settime(c,t); }
__attribute__((weak)) int        __nanosleep_time64(const struct timespec*rq,struct timespec*rm){ return r_nanosleep(rq,rm); }
__attribute__((weak)) struct tm* __localtime64_r(const time_t*t,struct tm*r){ return r_localtime_r(t,r); }
__attribute__((weak)) struct tm* __gmtime64_r(const time_t*t,struct tm*r){ return r_gmtime_r(t,r); }
__attribute__((weak)) time_t     __mktime64(struct tm*t){ return r_mktime(t); }
__attribute__((weak)) time_t     __timegm_time64(struct tm*t){ return r_timegm(t); }
__attribute__((weak)) int        __futimens_time64(int fd,const struct timespec t[2]){ return r_futimens(fd,t); }
__attribute__((weak)) int        __utimensat_time64(int d,const char*p,const struct timespec t[2],int f){ return r_utimensat(d,p,t,f); }

/* non-_r time conversion variants (nmbd) */
extern struct tm* r_localtime(const time_t*) __asm__("localtime");
extern struct tm* r_gmtime(const time_t*)    __asm__("gmtime");
extern char*      r_ctime(const time_t*)     __asm__("ctime");
extern char*      r_asctime(const struct tm*) __asm__("asctime");
extern double     r_difftime(time_t,time_t)  __asm__("difftime");
__attribute__((weak)) struct tm* __localtime64(const time_t*t){ return r_localtime(t); }
__attribute__((weak)) struct tm* __gmtime64(const time_t*t){ return r_gmtime(t); }
__attribute__((weak)) char*      __ctime64(const time_t*t){ return r_ctime(t); }
__attribute__((weak)) char*      __asctime64(const struct tm*t){ return r_asctime(t); }
__attribute__((weak)) double     __difftime64(time_t a,time_t b){ return r_difftime(a,b); }

/* more time64 names referenced by heimdal/pthreadpool */
#include <sys/select.h>
#include <pthread.h>
extern int r_setitimer(int,const struct itimerval*,struct itimerval*) __asm__("setitimer");
extern int r_select(int,fd_set*,fd_set*,fd_set*,struct timeval*)       __asm__("select");
extern int r_pthread_cond_timedwait(pthread_cond_t*,pthread_mutex_t*,const struct timespec*) __asm__("pthread_cond_timedwait");
__attribute__((weak)) int __setitimer_time64(int w,const struct itimerval*n,struct itimerval*o){ return r_setitimer(w,n,o); }
__attribute__((weak)) int __select_time64(int n,fd_set*r,fd_set*w,fd_set*e,struct timeval*t){ return r_select(n,r,w,e,t); }
__attribute__((weak)) int __pthread_cond_timedwait_time64(pthread_cond_t*c,pthread_mutex_t*m,const struct timespec*t){ return r_pthread_cond_timedwait(c,m,t); }
