/*
 * Copyright (c) 2005, David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

/* #define DEBUG_SIGNAL */
#ifdef DEBUG_SIGNAL
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

extern int	__pause(void);
int	___pause(void);
int	_raise(int);
int	__sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec * timeout);
int	__sigwaitinfo(const sigset_t *set, siginfo_t *info);
int	__sigwait(const sigset_t *set, int *sig);

static void
sigcancel_handler(int sig __unused,
	siginfo_t *info __unused, ucontext_t *ucp __unused)
{
	struct pthread *curthread = _get_curthread();

	if (curthread->cancel_defer && curthread->cancel_pending)
		thr_wake(curthread->tid);
	curthread->in_sigcancel_handler++;
	_thr_ast(curthread);
	curthread->in_sigcancel_handler--;
}

void
_thr_ast(struct pthread *curthread)
{
	if (!THR_IN_CRITICAL(curthread)) {
		_thr_testcancel(curthread);
		if (__predict_false((curthread->flags &
		    (THR_FLAGS_NEED_SUSPEND | THR_FLAGS_SUSPENDED))
			== THR_FLAGS_NEED_SUSPEND))
			_thr_suspend_check(curthread);
	}
}

void
_thr_suspend_check(struct pthread *curthread)
{
	long cycle;
	int err;

	err = errno;
	/* 
	 * Blocks SIGCANCEL which other threads must send.
	 */
	_thr_signal_block(curthread);

	/*
	 * Increase critical_count, here we don't use THR_LOCK/UNLOCK
	 * because we are leaf code, we don't want to recursively call
	 * ourself.
	 */
	curthread->critical_count++;
	THR_UMUTEX_LOCK(curthread, &(curthread)->lock);
	while ((curthread->flags & (THR_FLAGS_NEED_SUSPEND |
		THR_FLAGS_SUSPENDED)) == THR_FLAGS_NEED_SUSPEND) {
		curthread->cycle++;
		cycle = curthread->cycle;

		/* Wake the thread suspending us. */
		_thr_umtx_wake(&curthread->cycle, INT_MAX);

		/*
		 * if we are from pthread_exit, we don't want to
		 * suspend, just go and die.
		 */
		if (curthread->state == PS_DEAD)
			break;
		curthread->flags |= THR_FLAGS_SUSPENDED;
		THR_UMUTEX_UNLOCK(curthread, &(curthread)->lock);
		_thr_umtx_wait(&curthread->cycle, cycle, NULL);
		THR_UMUTEX_LOCK(curthread, &(curthread)->lock);
		curthread->flags &= ~THR_FLAGS_SUSPENDED;
	}
	THR_UMUTEX_UNLOCK(curthread, &(curthread)->lock);
	curthread->critical_count--;

	/* 
	 * Unblocks SIGCANCEL, it is possible a new SIGCANCEL is ready and
	 * a new signal frame will nest us, this seems a problem because 
	 * stack will grow and overflow, but because kernel will automatically
	 * mask the SIGCANCEL when delivering the signal, so we at most only
	 * have one nesting signal frame, this should be fine.
	 */
	_thr_signal_unblock(curthread);
	errno = err;
}

void
_thr_signal_init(void)
{
	struct sigaction act;

	/* Install cancel handler. */
	SIGEMPTYSET(act.sa_mask);
	act.sa_flags = SA_SIGINFO | SA_RESTART;
	act.sa_sigaction = (__siginfohandler_t *)&sigcancel_handler;
	__sys_sigaction(SIGCANCEL, &act, NULL);
}

void
_thr_signal_deinit(void)
{
}

__weak_reference(___pause, pause);

int
___pause(void)
{
	struct pthread *curthread = _get_curthread();
	int	ret;

	_thr_cancel_enter(curthread);
	ret = __pause();
	_thr_cancel_leave(curthread);
	
	return ret;
}

__weak_reference(_raise, raise);

int
_raise(int sig)
{
	int ret;

	if (!_thr_isthreaded())
		ret = kill(getpid(), sig);
	else
		ret = _thr_send_sig(_get_curthread(), sig);
	return (ret);
}

__weak_reference(_sigaction, sigaction);

int
_sigaction(int sig, const struct sigaction * act, struct sigaction * oact)
{
	/* Check if the signal number is out of range: */
	if (sig < 1 || sig > _SIG_MAXSIG || sig == SIGCANCEL) {
		/* Return an invalid argument: */
		errno = EINVAL;
		return (-1);
	}

	return __sys_sigaction(sig, act, oact);
}

__weak_reference(_sigprocmask, sigprocmask);

int
_sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	const sigset_t *p = set;
	sigset_t newset;

	if (how != SIG_UNBLOCK) {
		if (set != NULL) {
			newset = *set;
			SIGDELSET(newset, SIGCANCEL);
			p = &newset;
		}
	}
	return (__sys_sigprocmask(how, p, oset));
}

__weak_reference(_pthread_sigmask, pthread_sigmask);

int
_pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	if (_sigprocmask(how, set, oset))
		return (errno);
	return (0);
}

__weak_reference(__sigsuspend, sigsuspend);

int
_sigsuspend(const sigset_t * set)
{
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;

	ret = __sys_sigsuspend(pset);

	return (ret);
}

int
__sigsuspend(const sigset_t * set)
{
	struct pthread *curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;

	_thr_cancel_enter(curthread);
	ret = __sys_sigsuspend(pset);
	_thr_cancel_leave(curthread);

	return (ret);
}

__weak_reference(__sigwait, sigwait);
__weak_reference(__sigtimedwait, sigtimedwait);
__weak_reference(__sigwaitinfo, sigwaitinfo);

int
_sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec * timeout)
{
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;
	ret = __sys_sigtimedwait(pset, info, timeout);
	return (ret);
}

int
__sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec * timeout)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;
	_thr_cancel_enter(curthread);
	ret = __sys_sigtimedwait(pset, info, timeout);
	_thr_cancel_leave(curthread);
	return (ret);
}

int
_sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;

	ret = __sys_sigwaitinfo(pset, info);
	return (ret);
}

int
__sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else
		pset = set;

	_thr_cancel_enter(curthread);
	ret = __sys_sigwaitinfo(pset, info);
	_thr_cancel_leave(curthread);
	return (ret);
}

int
_sigwait(const sigset_t *set, int *sig)
{
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else 
		pset = set;

	ret = __sys_sigwait(pset, sig);
	return (ret);
}

int
__sigwait(const sigset_t *set, int *sig)
{
	struct pthread	*curthread = _get_curthread();
	sigset_t newset;
	const sigset_t *pset;
	int ret;

	if (SIGISMEMBER(*set, SIGCANCEL)) {
		newset = *set;
		SIGDELSET(newset, SIGCANCEL);
		pset = &newset;
	} else 
		pset = set;

	_thr_cancel_enter(curthread);
	ret = __sys_sigwait(pset, sig);
	_thr_cancel_leave(curthread);
	return (ret);
}
