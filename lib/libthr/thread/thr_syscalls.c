/*
 * Copyright (c) 2000 Jason Evans <jasone@freebsd.org>.
 * Copyright (c) 2002 Daniel M. Eischen <deischen@freebsd.org>
 * Copyright (c) 2003 Jeff Roberson <jeff@freebsd.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <aio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "thr_private.h"

extern spinlock_t *__malloc_lock;

extern int __creat(const char *, mode_t);
extern int __sleep(unsigned int);
extern int __sys_nanosleep(const struct timespec *, struct timespec *);
extern int __sys_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern int __sys_sigaction(int, const struct sigaction *, struct sigaction *);
extern int __system(const char *);
extern int __tcdrain(int);
extern pid_t __wait(int *);
extern pid_t __sys_wait4(pid_t, int *, int, struct rusage *);
extern pid_t __waitpid(pid_t, int *, int);

__weak_reference(_accept, accept);

int
_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_accept(s, addr, addrlen);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_aio_suspend, aio_suspend);

int
_aio_suspend(const struct aiocb * const iocbs[], int niocb, const struct
    timespec *timeout)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = __sys_aio_suspend(iocbs, niocb, timeout);
	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_close, close);

int
_close(int fd)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = __sys_close(fd);
	_thread_leave_cancellation_point();
	
	return ret;
}

__weak_reference(_connect, connect);

int
_connect(int s, const struct sockaddr *n, socklen_t l)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_connect(s, n, l);
	_thread_leave_cancellation_point();
	return ret;
}
	
__weak_reference(_creat, creat);

int
_creat(const char *path, mode_t mode)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = __creat(path, mode);
	_thread_leave_cancellation_point();
	
	return ret;
}

__weak_reference(_fcntl, fcntl);

int
_fcntl(int fd, int cmd,...)
{
	int	ret;
	va_list	ap;
	
	_thread_enter_cancellation_point();

	va_start(ap, cmd);
	switch (cmd) {
		case F_DUPFD:
		case F_SETFD:
		case F_SETFL:
			ret = __sys_fcntl(fd, cmd, va_arg(ap, int));
			break;
		case F_GETFD:
		case F_GETFL:
			ret = __sys_fcntl(fd, cmd);
			break;
		default:
			ret = __sys_fcntl(fd, cmd, va_arg(ap, void *));
	}
	va_end(ap);

	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_fork, fork);

int
_fork(int fd)
{
	int	ret;
	struct pthread_atfork *af;

	_pthread_mutex_lock(&_atfork_mutex);

	/* Run down atfork prepare handlers. */
	TAILQ_FOREACH_REVERSE(af, &_atfork_list, atfork_head, qe) {
		if (af->prepare != NULL)
			af->prepare();
	}

 	/*
	 * Fork a new process.
	 * XXX - The correct way to handle __malloc_lock is to have
	 *	 the threads libraries (or libc) install fork handlers for it
	 *	 in their initialization routine. We should probably
	 *	 do that for all the locks in libc.
	 */
	if (__isthreaded && __malloc_lock != NULL)
		_SPINLOCK(__malloc_lock);
	ret = __sys_fork();
 	if (ret == 0) {
		__isthreaded = 0;
		if (__malloc_lock != NULL)
			memset(__malloc_lock, 0, sizeof(spinlock_t));
		init_tdlist(curthread, 1);
		init_td_common(curthread, NULL, 1);
		_mutex_reinit(&_atfork_mutex);

		/* Run down atfork child handlers. */
		TAILQ_FOREACH(af, &_atfork_list, qe) {
			if (af->child != NULL)
				af->child();
		}
 	} else if (ret != -1) {
		/* Run down atfork parent handlers. */
		TAILQ_FOREACH(af, &_atfork_list, qe) {
			if (af->parent != NULL)
			af->parent();
		}
	}

	if (ret != 0) {
		if (__isthreaded && __malloc_lock != NULL)
			_SPINUNLOCK(__malloc_lock);
		_pthread_mutex_unlock(&_atfork_mutex);
	}
	return ret;
}


__weak_reference(_fsync, fsync);

int
_fsync(int fd)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = __sys_fsync(fd);
	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_msgrcv, msgrcv);

int
_msgrcv(int id, void *p, size_t sz, long t, int f)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_msgrcv(id, p, sz, t, f);
	_thread_leave_cancellation_point();
	return ret;
}

__weak_reference(_msgsnd, msgsnd);

int
_msgsnd(int id, const void *p, size_t sz, int f)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_msgsnd(id, p, sz, f);
	_thread_leave_cancellation_point();
	return ret;
}

__weak_reference(_msync, msync);

int
_msync(void *addr, size_t len, int flags)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = __sys_msync(addr, len, flags);
	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_nanosleep, nanosleep);

int
_nanosleep(const struct timespec * time_to_sleep, struct timespec *
    time_remaining)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = __sys_nanosleep(time_to_sleep, time_remaining);
	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_open, open);

int
_open(const char *path, int flags,...)
{
	int	ret;
	int	mode = 0;
	va_list	ap;

	_thread_enter_cancellation_point();
	
	/* Check if the file is being created: */
	if (flags & O_CREAT) {
		/* Get the creation mode: */
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	
	ret = __sys_open(path, flags, mode);
	_thread_leave_cancellation_point();

	return ret;
}

/*
 * The implementation in libc calls sigpause(), which is also
 * a cancellation point.
 */
#if 0
__weak_reference(_pause, pause);

int
_pause(void)
{
	_thread_enter_cancellation_point();
	__pause();
	_thread_leave_cancellation_point();
}
#endif

__weak_reference(_poll, poll);

int
_poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_poll(fds, nfds, timeout);
	_thread_leave_cancellation_point();

	return ret;
}

/* XXXFix */
#if 0
__weak_reference(_pread, pread);

ssize_t
_pread(int d, void *b, size_t n, off_t o)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_pread(d, b, n, o);
	_thread_leave_cancellation_point();
	return (ret);
}
#endif

/* The libc version calls select(), which is also a cancellation point. */
#if 0
extern int __pselect(int count, fd_set *rfds, fd_set *wfds, fd_set *efds, 
		const struct timespec *timo, const sigset_t *mask);

int 
pselect(int count, fd_set *rfds, fd_set *wfds, fd_set *efds, 
	const struct timespec *timo, const sigset_t *mask)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __pselect(count, rfds, wfds, efds, timo, mask);
	_thread_leave_cancellation_point();

	return (ret);
}
#endif

/* XXXFix */
#if 0
__weak_reference(_pwrite, pwrite);

ssize_t
_pwrite(int d, const void *b, size_t n, off_t o)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_pwrite(d, b, n, o);
	_thread_leave_cancellation_point();
	return (ret);
}
#endif

__weak_reference(_raise, raise);

int
_raise(int sig)
{
	int error;

	error = pthread_kill(pthread_self(), sig);
	if (error != 0) {
		errno = error;
		error = -1;
	}
	return (error);
}

__weak_reference(_read, read);

ssize_t
_read(int fd, void *buf, size_t nbytes)
{
	ssize_t	ret;

	_thread_enter_cancellation_point();
	ret = __sys_read(fd, buf, nbytes);
	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_readv, readv);

ssize_t
_readv(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_readv(fd, iov, iovcnt);
	_thread_leave_cancellation_point();

	return ret;
}

/*
 * The libc implementation of recv() calls recvfrom, which
 * is also a cancellation point.
 */
#if 0
__weak_reference(_recv, recv);

ssize_t
_recv(int s, void *b, size_t l, int f)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_recv(s, b, l, f);
	_thread_leave_cancellation_point();
	return (ret);
}
#endif

__weak_reference(_recvfrom, recvfrom);

ssize_t
_recvfrom(int s, void *b, size_t l, int f, struct sockaddr *from,
    socklen_t *fl)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_recvfrom(s, b, l, f, from, fl);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_recvmsg, recvmsg);

ssize_t
_recvmsg(int s, struct msghdr *m, int f)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_recvmsg(s, m, f);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_select, select);

int 
_select(int numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	struct timeval *timeout)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_select(numfds, readfds, writefds, exceptfds, timeout);
	_thread_leave_cancellation_point();

	return ret;
}

/*
 * Libc implements this by calling _sendto(), which is also a
 * cancellation point.
 */
#if 0
__weak_reference(_send, send);

ssize_t
_send(int s, const void *m, size_t l, int f)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = _sendto(s, m, l, f, NULL, 0);
	_thread_leave_cancellation_point();
	return (ret);
}
#endif

__weak_reference(_sendmsg, sendmsg);

ssize_t
_sendmsg(int s, const struct msghdr *m, int f)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_sendmsg(s, m, f);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_sendto, sendto);

ssize_t
_sendto(int s, const void *m, size_t l, int f, const struct sockaddr *t,
    socklen_t tl)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_sendto(s, m, l, f, t, tl);
	_thread_leave_cancellation_point();
	return (ret);
}

/*
 * The implementation in libc calls sigsuspend(), which is also
 * a cancellation point.
 */
#if 0
__weak_reference(_sigpause, sigpause);

int
_sigpause(int m)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_sigpause(m);
	_thread_leave_cancellation_point();
	return (ret);
}
#endif

__weak_reference(_sigsuspend, sigsuspend);

int
_sigsuspend(const sigset_t *m)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_sigsuspend(m);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_sigtimedwait, sigtimedwait);

int
_sigtimedwait(const sigset_t *s, siginfo_t *i, const struct timespec *t)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_sigtimedwait(s, i, t);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_sigwait, sigwait);

int
_sigwait(const sigset_t *s, int *i)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_sigwait(s, i);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_sigwaitinfo, sigwaitinfo);

int
_sigwaitinfo(const sigset_t *s, siginfo_t *i)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_sigwaitinfo(s, i);
	_thread_leave_cancellation_point();
	return (ret);
}

__weak_reference(_sleep, sleep);

unsigned int
_sleep(unsigned int seconds)
{
	unsigned int	ret;

	_thread_enter_cancellation_point();
	ret = __sleep(seconds);
	_thread_leave_cancellation_point();
	
	return ret;
}

__weak_reference(_system, system);

int
_system(const char *string)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = __system(string);
	_thread_leave_cancellation_point();
	
	return ret;
}


__weak_reference(_tcdrain, tcdrain);

int
_tcdrain(int fd)
{
	int	ret;
	
	_thread_enter_cancellation_point();
	ret = __tcdrain(fd);
	_thread_leave_cancellation_point();

	return ret;
}

/*
 * The usleep() implementation calls nanosleep(), which is also
 * a cancellation point.
 */
#if 0
__weak_reference(_usleep, usleep);

int
_usleep(useconds_t u)
{
	int ret;

	_thread_enter_cancellation_point();
	ret = __sys_usleep(u);
	_thread_leave_cancellation_point();
	return (ret);
}
#endif

__weak_reference(_wait, wait);

pid_t
_wait(int *istat)
{
	pid_t	ret;

	_thread_enter_cancellation_point();
	ret = __wait(istat);
	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_wait4, wait4);

pid_t
_wait4(pid_t pid, int *istat, int options, struct rusage *rusage)
{
	pid_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_wait4(pid, istat, options, rusage);
	_thread_leave_cancellation_point();

	return ret;
}

/*
 * The libc implementation of waitpid calls wait4().
 */
#if 0
__weak_reference(_waitpid, waitpid);

pid_t
_waitpid(pid_t wpid, int *status, int options)
{
	pid_t	ret;

	_thread_enter_cancellation_point();
	ret = __waitpid(wpid, status, options);
	_thread_leave_cancellation_point();
	
	return ret;
}
#endif

__weak_reference(_write, write);

ssize_t
_write(int fd, const void *buf, size_t nbytes)
{
	ssize_t	ret;

	_thread_enter_cancellation_point();
	ret = __sys_write(fd, buf, nbytes);
	_thread_leave_cancellation_point();

	return ret;
}

__weak_reference(_writev, writev);

ssize_t
_writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret;

	_thread_enter_cancellation_point();
	ret = __sys_writev(fd, iov, iovcnt);
	_thread_leave_cancellation_point();

	return ret;
}
