/* syscompat.c — make musl-linked binaries run on Linux 2.6.24 (DS207).
 *
 * musl always uses the "modern" syscall variants (signalfd4, eventfd2, pipe2,
 * accept4, epoll_create1, dup3, inotify_init1 — all Linux 2.6.27+). The DS207
 * kernel is 2.6.24, so those return ENOSYS and Samba's tevent (epoll backend +
 * eventfd wakeup + signalfd) panics ("failed to setup SIGTERM handler").
 *
 * We override each with a version that tries the modern syscall and, on ENOSYS,
 * falls back to the pre-2.6.27 syscall and applies CLOEXEC/NONBLOCK via fcntl.
 *
 * For functions that musl groups into one object (epoll.o, eventfd.o,
 * inotify.o) we must provide EVERY symbol in that object, otherwise the linker
 * pulls musl's object for a sibling symbol and we get a duplicate-symbol error.
 * These are strong definitions, force-linked ahead of libc.a so ours win.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>

/* flag bits are identical to O_CLOEXEC / O_NONBLOCK across these interfaces */
static void apply_flags(int fd, int flags)
{
    if (fd < 0) return;
    if (flags & O_CLOEXEC)  fcntl(fd, F_SETFD, FD_CLOEXEC);
    if (flags & O_NONBLOCK) fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

/* ---- standalone musl objects ---- */

int signalfd(int fd, const sigset_t *mask, int flags)
{
    long r = syscall(SYS_signalfd4, fd, mask, 8, flags);
    if (r >= 0 || errno != ENOSYS) return (int)r;
    r = syscall(SYS_signalfd, fd, mask, 8);
    if (r >= 0) apply_flags((int)r, flags);
    return (int)r;
}

int pipe2(int pipefd[2], int flags)
{
    long r = syscall(SYS_pipe2, pipefd, flags);
    if (r >= 0 || errno != ENOSYS) return (int)r;
    if (pipe(pipefd) < 0) return -1;
    apply_flags(pipefd[0], flags);
    apply_flags(pipefd[1], flags);
    return 0;
}

int accept4(int sfd, struct sockaddr *addr, socklen_t *len, int flags)
{
    long r = syscall(SYS_accept4, sfd, addr, len, flags);
    if (r >= 0 || errno != ENOSYS) return (int)r;
    r = accept(sfd, addr, len);
    if (r >= 0) apply_flags((int)r, flags);
    return (int)r;
}

int dup3(int oldfd, int newfd, int flags)
{
    long r = syscall(SYS_dup3, oldfd, newfd, flags);
    if (r >= 0 || errno != ENOSYS) return (int)r;
    if (oldfd == newfd) { errno = EINVAL; return -1; }
    r = syscall(SYS_dup2, oldfd, newfd);
    if (r >= 0 && (flags & O_CLOEXEC)) fcntl((int)r, F_SETFD, FD_CLOEXEC);
    return (int)r;
}

/* ---- musl epoll.o: epoll_create, epoll_create1, epoll_ctl, epoll_wait, epoll_pwait ---- */

int epoll_create1(int flags)
{
    long r = syscall(SYS_epoll_create1, flags);
    if (r >= 0 || errno != ENOSYS) return (int)r;
    r = syscall(SYS_epoll_create, 1);
    if (r >= 0) apply_flags((int)r, flags);   /* EPOLL_CLOEXEC == O_CLOEXEC */
    return (int)r;
}
int epoll_create(int size)
{
    if (size <= 0) size = 1;
    return (int)syscall(SYS_epoll_create, size);
}
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *ev)
{
    return (int)syscall(SYS_epoll_ctl, epfd, op, fd, ev);
}
int epoll_pwait(int epfd, struct epoll_event *ev, int maxevents, int timeout,
                const sigset_t *sigmask)
{
    return (int)syscall(SYS_epoll_pwait, epfd, ev, maxevents, timeout, sigmask, 8);
}
int epoll_wait(int epfd, struct epoll_event *ev, int maxevents, int timeout)
{
    return (int)syscall(SYS_epoll_wait, epfd, ev, maxevents, timeout);
}

/* ---- musl eventfd.o: eventfd, eventfd_read, eventfd_write ---- */

int eventfd(unsigned int count, int flags)
{
    long r = syscall(SYS_eventfd2, count, flags);
    if (r >= 0 || errno != ENOSYS) return (int)r;
    r = syscall(SYS_eventfd, count);
    if (r >= 0) apply_flags((int)r, flags);
    return (int)r;
}
int eventfd_read(int fd, uint64_t *value)
{
    return (read(fd, value, 8) == 8) ? 0 : -1;
}
int eventfd_write(int fd, uint64_t value)
{
    return (write(fd, &value, 8) == 8) ? 0 : -1;
}

/* ---- musl inotify.o: inotify_init, inotify_init1, inotify_add_watch, inotify_rm_watch ---- */

int inotify_init1(int flags)
{
    long r = syscall(SYS_inotify_init1, flags);
    if (r >= 0 || errno != ENOSYS) return (int)r;
    r = syscall(SYS_inotify_init);
    if (r >= 0) apply_flags((int)r, flags);
    return (int)r;
}
int inotify_init(void)
{
    return (int)syscall(SYS_inotify_init);
}
int inotify_add_watch(int fd, const char *path, uint32_t mask)
{
    return (int)syscall(SYS_inotify_add_watch, fd, path, mask);
}
int inotify_rm_watch(int fd, int wd)
{
    return (int)syscall(SYS_inotify_rm_watch, fd, wd);
}

/* ---- time on Linux 2.6.24 ----
 * musl (32-bit, 64-bit time_t) uses clock_gettime64/gettimeofday via the Linux
 * 5.1 time64 syscalls, which ENOSYS on 2.6.24 (clock_gettime returned 0, time()
 * returned garbage) -> NTLMSSP auth fails with "challenge expired". We provide
 * the time64 symbols Samba calls, implemented with the 32-bit kernel syscalls
 * (ppc: clock_gettime=246, gettimeofday=78) and widen the result to 64-bit. */
#include <time.h>
struct __kts32 { long s, n; };
struct __ktv32 { long s, u; };

int __clock_gettime64(clockid_t clk, struct timespec *ts)
{
    struct __kts32 k;
    long r = syscall(246 /* SYS_clock_gettime (ppc, 32-bit) */, clk, &k);
    if (r < 0) return (int)r;
    ts->tv_sec = (time_t)k.s;
    ts->tv_nsec = k.n;
    return 0;
}

int __gettimeofday_time64(struct timeval *tv, void *tz)
{
    struct __ktv32 k;
    long r = syscall(78 /* SYS_gettimeofday (ppc) */, &k, tz);
    if (r < 0) return (int)r;
    if (tv) { tv->tv_sec = (time_t)k.s; tv->tv_usec = k.u; }
    return 0;
}

time_t __time64(time_t *t)
{
    struct __kts32 k;
    syscall(246, 0 /* CLOCK_REALTIME */, &k);
    time_t r = (time_t)k.s;
    if (t) *t = r;
    return r;
}
