//depot/projects/kse/lib/libpthread/thread/thr_sigwait.c#1 - branch change 15154 (text+ko)
/*
 * Copyright (c) 1997 John Birrell <jb@cimlogic.com.au>.
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
 * $FreeBSD$
 */
#include <signal.h>
#include <sys/param.h>
#include <sys/signalvar.h>
#include <errno.h>
#include <pthread.h>
#include "thr_private.h"

__weak_reference(__sigwait, sigwait);
__weak_reference(__sigtimedwait, sigtimedwait);
__weak_reference(__sigwaitinfo, sigwaitinfo);

static int
lib_sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec * timeout)
{
	struct pthread	*curthread = _get_curthread();
	int		ret = 0;
	int		i;
	sigset_t	tempset, waitset;
	struct sigaction act;
	kse_critical_t  crit;
	siginfo_t	siginfo;

	if (!_kse_isthreaded()) {
		if (info == NULL)
			info = &siginfo;
		return __sys_sigtimedwait((sigset_t *)set, info,
			(struct timespec *)timeout);
	}

	/*
	 * Specify the thread kernel signal handler.
	 */
	act.sa_handler = (void (*) ()) _thr_sig_handler;
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	/* Ensure the signal handler cannot be interrupted by other signals: */
	SIGFILLSET(act.sa_mask);

	/*
	 * Initialize the set of signals that will be waited on:
	 */
	waitset = *set;

	/* These signals can't be waited on. */
	SIGDELSET(waitset, SIGKILL);
	SIGDELSET(waitset, SIGSTOP);

	crit = _kse_critical_enter();
	KSE_LOCK_ACQUIRE(curthread->kse, &_thread_signal_lock);

	/*
	 * Enter a loop to find the signals that are SIG_DFL. For
	 * these signals we must install a dummy signal handler in
	 * order for the kernel to pass them in to us.  POSIX says
	 * that the _application_ must explicitly install a dummy
	 * handler for signals that are SIG_IGN in order to sigwait
	 * on them.  Note that SIG_IGN signals are left in the
	 * mask because a subsequent sigaction could enable an
	 * ignored signal.
	 */
	SIGEMPTYSET(tempset);
	for (i = 1; i <= _SIG_MAXSIG; i++) {
		if (SIGISMEMBER(waitset, i) &&
		    (_thread_sigact[i - 1].sa_handler == SIG_DFL)) {
			_thread_dfl_count[i - 1]++;
			SIGADDSET(tempset, i);
			if (_thread_dfl_count[i - 1] == 1) {
				if (__sys_sigaction(i, &act, NULL) != 0)
					/* ret = -1 */;
			}
		}
	}

	if (ret == 0) {
		/* Done accessing _thread_dfl_count for now. */
		KSE_LOCK_RELEASE(curthread->kse, &_thread_signal_lock);
		KSE_SCHED_LOCK(curthread->kse, curthread->kseg);
		for (i = 1; i <= _SIG_MAXSIG; ++i) {
			if (SIGISMEMBER(waitset, i) &&
			    SIGISMEMBER(curthread->sigpend, i)) {
				SIGDELSET(curthread->sigpend, i);
				siginfo = curthread->siginfo[i - 1];
				KSE_SCHED_UNLOCK(curthread->kse,
					curthread->kseg);
				KSE_LOCK_ACQUIRE(curthread->kse,
					&_thread_signal_lock);
				ret = i;
				goto OUT;
			}
		}
		curthread->timeout = 0;
		curthread->interrupted = 0;
		_thr_set_timeout(timeout);
		/* Wait for a signal: */
		curthread->oldsigmask = curthread->sigmask;
		siginfo.si_signo = 0;
		curthread->data.sigwaitinfo = &siginfo;
		SIGFILLSET(curthread->sigmask);
		SIGSETNAND(curthread->sigmask, waitset);
		THR_SET_STATE(curthread, PS_SIGWAIT);
		_thr_sched_switch_unlocked(curthread);
		/*
		 * Return the signal number to the caller:
		 */
		if (siginfo.si_signo > 0) {
			ret = siginfo.si_signo;
		} else {
			if (curthread->interrupted)
				errno = EINTR;
			else if (curthread->timeout)
				errno = EAGAIN;
			ret = -1;
		}
		curthread->timeout = 0;
		curthread->interrupted = 0;
		/*
		 * Probably unnecessary, but since it's in a union struct
		 * we don't know how it could be used in the future.
		 */
		crit = _kse_critical_enter();
		curthread->data.sigwaitinfo = NULL;
		/*
		 * Relock the array of SIG_DFL wait counts.
		 */
		KSE_LOCK_ACQUIRE(curthread->kse, &_thread_signal_lock);
	}

OUT:
	/* Restore the sigactions: */
	act.sa_handler = SIG_DFL;
	for (i = 1; i <= _SIG_MAXSIG; i++) {
		if (SIGISMEMBER(tempset, i)) {
			_thread_dfl_count[i - 1]--;
			if ((_thread_sigact[i - 1].sa_handler == SIG_DFL) &&
			    (_thread_dfl_count[i - 1] == 0)) {
				if (__sys_sigaction(i, &act, NULL) != 0)
					/* ret = -1 */ ;
			}
		}
	}
	/* Done accessing _thread_dfl_count. */
	KSE_LOCK_RELEASE(curthread->kse, &_thread_signal_lock);
	_kse_critical_leave(crit);

	if (ret > 0 && info != NULL)
		*info = siginfo;

	return (ret);
}

int
__sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec * timeout)
{
	struct pthread	*curthread = _get_curthread();
	int ret;

	_thr_enter_cancellation_point(curthread);
	ret = lib_sigtimedwait(set, info, timeout);
	_thr_leave_cancellation_point(curthread);
	return (ret);
}

int _sigtimedwait(const sigset_t *set, siginfo_t *info,
	const struct timespec * timeout)
{
	return lib_sigtimedwait(set, info, timeout);
}

int
__sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	struct pthread	*curthread = _get_curthread();
	int ret;

	_thr_enter_cancellation_point(curthread);
	ret = lib_sigtimedwait(set, info, NULL);
	_thr_leave_cancellation_point(curthread);
	return (ret);
}

int
_sigwaitinfo(const sigset_t *set, siginfo_t *info)
{
	return lib_sigtimedwait(set, info, NULL);
}

int
__sigwait(const sigset_t *set, int *sig)
{
	struct pthread	*curthread = _get_curthread();
	int ret;

	_thr_enter_cancellation_point(curthread);
	ret = lib_sigtimedwait(set, NULL, NULL);
	if (ret > 0) {
		*sig = ret;
		ret = 0;
	}
	else
		ret = -1;
	_thr_leave_cancellation_point(curthread);
	return (ret);
}

int
_sigwait(const sigset_t *set, int *sig)
{
	int ret;

	ret = lib_sigtimedwait(set, NULL, NULL);
	if (ret > 0) {
		*sig = ret;
		ret = 0;
	} else {
		ret = -1;
	}
	return (ret);
}

