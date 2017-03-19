/*
 * Copyright (c) 2014 The FreeBSD Foundation.
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include "libc_private.h"

#ifdef __CHERI_PURE_CAPABILITY__
#include <sys/wait.h>
/*
 * __wrap_<sys> functions to work around CTSRD-CHERI/llvm#182
 */
int __wrap_accept(int s, struct sockaddr * restrict addr,
    __socklen_t * restrict addrlen);
int __wrap_accept4(int s, struct sockaddr * restrict addr,
    __socklen_t * restrict addrlen, int flags);
int __wrap_aio_suspend(const struct aiocb *const iocbs[], int niocb,
     const struct timespec *timeout);
int __wrap_close(int fd);
int __wrap_clock_nanosleep(clockid_t clock_id, int flags,
    const struct timespec *rqtp, struct timespec *rmtp)
int __wrap_connect(int s, const struct sockaddr *name, __socklen_t namelen);
int __wrap_fcntl(int fd, int cmd, intptr_t arg);
int __wrap_fdatasync(int fd);
int __wrap_fork(void);
int __wrap_fsync(int fd);
int __wrap_kevent(int fd, struct kevent *changelist, int nchanges,
    struct kevent *eventlist, int nevents, const struct timespec *timeout);
int __wrap_msync(void *addr, size_t len, int flags);
int __wrap_nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
int __wrap_openat(int fd, const char *path, int flags, mode_t mode);
int __wrap_poll(struct pollfd *fds, u_int nfds, int timeout);
int __wrap_ppoll(struct pollfd *fds, u_int nfds, const struct timespec *ts,
    const sigset_t *set);
int __wrap_pselect(int nd, fd_set *in, fd_set *ou, fd_set *ex,
    const struct timespec *ts, const sigset_t *sm);
int __wrap_read(int fd, void *buf, size_t nbyte);
int __wrap_readv(int fd, struct iovec *iovp, u_int iovcnt);
int __wrap_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from,
    int *fromlenaddr);
int __wrap_recvmsg(int s, struct msghdr *msg, int flags);
int __wrap_select(int nd, fd_set *in, fd_set *ou, fd_set *ex, struct timeval *tv);
int __wrap_sendmsg(int s, struct msghdr *msg, int flags);
int __wrap_sendto(int s, void *buf, size_t len, int flags,
    const struct sockaddr *to, int tolen);
int __wrap_setcontext(const struct __ucontext *ucp);
int __wrap_sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
int __wrap_sigprocmask(int how, const sigset_t *set, sigset_t *oset);
int __wrap_sigsuspend(const sigset_t *sigmask);
int __wrap_sigtimedwait(const sigset_t *set, struct __siginfo *info,
    const struct timespec *timeout);
int __wrap_sigwaitinfo(const sigset_t *set, struct __siginfo *info);
int __wrap_swapcontext(struct __ucontext *oucp, const struct __ucontext *ucp);
int __wrap_wait4(int pid, int *status, int options, struct rusage *rusage);
int __wrap_wait6(enum idtype idtype, id_t id, int *status, int options,
    struct __wrusage *wrusage, struct __siginfo *info);
int __wrap_write(int fd, const void *buf, size_t nbyte);
int __wrap_writev(int fd, struct iovec *iovp, u_int iovcnt);

int
__wrap_accept(int s, struct sockaddr * restrict addr,
    __socklen_t * restrict addrlen)
{

	return (__sys_accept(s, addr, addrlen));
}

int
__wrap_accept4(int s, struct sockaddr * restrict addr,
    __socklen_t * restrict addrlen, int flags)
{

	return (__sys_accept4(s, addr, addrlen, flags));
}

int
__wrap_aio_suspend(const struct aiocb *const iocbs[], int niocb,
    const struct timespec *timeout)
{

	return (__sys_aio_suspend(iocbs, niocb, timeout));
}

int
__wrap_close(int fd)
{

	return (__sys_close(fd));
}

int
__wrap_clock_nanosleep(clockid_t clock_id, int flags,
    const struct timespec *rqtp, struct timespec *rmtp)
{

	return (__sys_clock_nanosleep(clock_id, flags, rqtp, rmtp));
}

int
__wrap_connect(int s, const struct sockaddr *name, __socklen_t namelen)
{

	return(__sys_connect(s, name, namelen));
}


int
__wrap_fcntl(int fd, int cmd, intptr_t arg)
{

	return(__sys_fcntl(fd, cmd, arg));
}


int
__wrap_fdatasync(int fd)
{

	return(__sys_fdatasync(fd));
}


int
__wrap_fork(void)
{

	return(__sys_fork());
}


int
__wrap_fsync(int fd)
{

	return(__sys_fsync(fd));
}


int
__wrap_kevent(int fd, struct kevent *changelist, int nchanges,
    struct kevent *eventlist, int nevents, const struct timespec *timeout)
{

	return(__sys_kevent(fd, changelist, nchanges, eventlist, nevents,
	    timeout));
}


int
__wrap_msync(void *addr, size_t len, int flags)
{

	return(__sys_msync(addr, len, flags));
}


int
__wrap_nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{

	return(__sys_nanosleep(rqtp, rmtp));
}


int
__wrap_openat(int
fd, const char *path, int flags, mode_t mode)
{

	return (__sys_openat(fd, path, flags, mode));
}
int
__wrap_poll(struct pollfd *fds, u_int nfds, int timeout)
{

	return(__sys_poll(fds, nfds, timeout));
}


int
__wrap_ppoll(struct pollfd *fds, u_int nfds, const struct timespec *ts,
    const sigset_t *set)
{

	return(__sys_ppoll(fds, nfds, ts, set));
}


int
__wrap_pselect(int nd, fd_set *in, fd_set *ou, fd_set *ex,
    const struct timespec *ts, const sigset_t *sm)
{

	return(__sys_pselect(nd, in, ou, ex, ts, sm));
}


int
__wrap_read(int fd, void *buf, size_t nbyte)
{

	return(__sys_read(fd, buf, nbyte));
}


int
__wrap_readv(int fd, struct iovec *iovp, u_int iovcnt)
{

	return(__sys_readv(fd, iovp, iovcnt));
}


int
__wrap_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from,
    int *fromlenaddr)
{

	return(__sys_recvfrom(s, buf, len, flags, from, fromlenaddr));
}


int
__wrap_recvmsg(int s, struct msghdr *msg, int flags)
{

	return(__sys_recvmsg(s, msg, flags));
}


int
__wrap_select(int nd, fd_set *in, fd_set *ou, fd_set *ex, struct timeval *tv)
{

	return(__sys_select(nd, in, ou, ex, tv));
}


int
__wrap_sendmsg(int s, struct msghdr *msg, int flags)
{

	return(__sys_sendmsg(s, msg, flags));
}


int
__wrap_sendto(int s, void *buf, size_t len, int flags,
    const struct sockaddr *to, int tolen)
{

	return(__sys_sendto(s, buf, len, flags, to, tolen));
}


int
__wrap_setcontext(const struct __ucontext *ucp)
{

	return(__sys_setcontext(ucp));
}


int
__wrap_sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{

	return(__sys_sigaction(sig, act, oact));
}


int
__wrap_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{

	return(__sys_sigprocmask(how, set, oset));
}


int
__wrap_sigsuspend(const sigset_t *sigmask)
{

	return(__sys_sigsuspend(sigmask));
}


int
__wrap_sigtimedwait(const sigset_t *set, struct __siginfo *info,
    const struct timespec *timeout)
{

	return(__sys_sigtimedwait(set, info, timeout));
}


int
__wrap_sigwaitinfo(const sigset_t *set, struct __siginfo *info)
{

	return(__sys_sigwaitinfo(set, info));
}


int
__wrap_swapcontext(struct __ucontext *oucp, const struct __ucontext *ucp)
{

	return(__sys_swapcontext(oucp, ucp));
}


int
__wrap_wait4(int pid, int *status, int options, struct rusage *rusage)
{

	return(__sys_wait4(pid, status, options, rusage));
}


int
__wrap_wait6(enum idtype idtype, id_t id, int *status, int options,
    struct __wrusage *wrusage, struct __siginfo *info)
{

	return(__sys_wait6(idtype, id, status, options, wrusage, info));
}


int
__wrap_write(int fd, const void *buf, size_t nbyte)
{

	return(__sys_write(fd, buf, nbyte));
}


int
__wrap_writev(int fd, struct iovec *iovp, u_int iovcnt)
{

	return(__sys_writev(fd, iovp, iovcnt));
}
#endif

#define	SLOT(a, b) \
	[INTERPOS_##a] = (interpos_func_t)b,
#ifndef NO_SYSCALLS
#ifndef __CHERI_PURE_CAPABILITY__
#define SLOT_SYS(s) \
	[INTERPOS_##s] = (interpos_func_t)__sys_##s,
#else
#define SLOT_SYS(s) \
	[INTERPOS_##s] = (interpos_func_t)__wrap_##s,
#endif
#else
#define SLOT_SYS(s)
#endif
#define SLOT_LIBC(s) \
	[INTERPOS_##s] = (interpos_func_t)__libc_##s,
interpos_func_t __libc_interposing[INTERPOS_MAX] = {
	SLOT_SYS(accept)
	SLOT_SYS(accept4)
	SLOT_SYS(aio_suspend)
	SLOT_SYS(close)
	SLOT_SYS(connect)
	SLOT_SYS(fcntl)
	SLOT_SYS(fsync)
	SLOT_SYS(fork)
	SLOT_SYS(msync)
	SLOT_SYS(nanosleep)
	SLOT_SYS(openat)
	SLOT_SYS(poll)
	SLOT_SYS(pselect)
	SLOT_SYS(read)
	SLOT_SYS(readv)
	SLOT_SYS(recvfrom)
	SLOT_SYS(recvmsg)
	SLOT_SYS(select)
	SLOT_SYS(sendmsg)
	SLOT_SYS(sendto)
	SLOT_SYS(setcontext)
	SLOT_SYS(sigaction)
	SLOT_SYS(sigprocmask)
	SLOT_SYS(sigsuspend)
	SLOT_LIBC(sigwait)
	SLOT_SYS(sigtimedwait)
	SLOT_SYS(sigwaitinfo)
	SLOT_SYS(swapcontext)
	SLOT_LIBC(system)
	SLOT_LIBC(tcdrain)
	SLOT_SYS(wait4)
	SLOT_SYS(write)
	SLOT_SYS(writev)
	SLOT(_pthread_mutex_init_calloc_cb, _pthread_mutex_init_calloc_cb_stub)
	SLOT(spinlock, __libc_spinlock_stub)
	SLOT(spinunlock, __libc_spinunlock_stub)
	SLOT_SYS(kevent)
	SLOT_SYS(wait6)
	SLOT_SYS(ppoll)
	SLOT_LIBC(map_stacks_exec)
	SLOT_SYS(fdatasync)
	SLOT_SYS(clock_nanosleep)
};
#undef SLOT
#undef SLOT_SYS
#undef SLOT_LIBC

interpos_func_t *
__libc_interposing_slot(int interposno)
{

	return (&__libc_interposing[interposno]);
}
