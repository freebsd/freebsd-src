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
#include "pthread_private.h"

__weak_reference(_sigwait, sigwait);

int
_sigwait(const sigset_t *set, int *sig)
{
	struct pthread	*curthread = _get_curthread();
	int		ret = 0;
	int		i;
	sigset_t	tempset, waitset;
	struct sigaction act;
	
	_thread_enter_cancellation_point();
	/*
	 * Specify the thread kernel signal handler.
	 */
	act.sa_handler = (void (*) ()) _thread_sig_handler;
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	/* Ensure the signal handler cannot be interrupted by other signals: */
	sigfillset(&act.sa_mask);

	/*
	 * Initialize the set of signals that will be waited on:
	 */
	waitset = *set;

	/* These signals can't be waited on. */
	sigdelset(&waitset, SIGKILL);
	sigdelset(&waitset, SIGSTOP);
	sigdelset(&waitset, _SCHED_SIGNAL);
	sigdelset(&waitset, SIGCHLD);
	sigdelset(&waitset, SIGINFO);

	/* Check to see if a pending signal is in the wait mask. */
	tempset = curthread->sigpend;
	SIGSETOR(tempset, _process_sigpending);
	SIGSETAND(tempset, waitset);
	if (SIGNOTEMPTY(tempset)) {
		/* Enter a loop to find a pending signal: */
		for (i = 1; i < NSIG; i++) {
			if (sigismember (&tempset, i))
				break;
		}

		/* Clear the pending signal: */
		if (sigismember(&curthread->sigpend,i))
			sigdelset(&curthread->sigpend,i);
		else
			sigdelset(&_process_sigpending,i);

		/* Return the signal number to the caller: */
		*sig = i;

		_thread_leave_cancellation_point();
		return (0);
	}

	/*
	 * Access the _thread_dfl_count array under the protection of signal
	 * deferral.
	 */
	_thread_kern_sig_defer();

	/*
	 * Enter a loop to find the signals that are SIG_DFL.  For
	 * these signals we must install a dummy signal handler in
	 * order for the kernel to pass them in to us.  POSIX says
	 * that the _application_ must explicitly install a dummy
	 * handler for signals that are SIG_IGN in order to sigwait
	 * on them.  Note that SIG_IGN signals are left in the
	 * mask because a subsequent sigaction could enable an
	 * ignored signal.
	 */
	sigemptyset(&tempset);
	for (i = 1; i < NSIG; i++) {
		if (sigismember(&waitset, i) &&
		    (_thread_sigact[i - 1].sa_handler == SIG_DFL)) {
			_thread_dfl_count[i]++;
			sigaddset(&tempset, i);
			if (_thread_dfl_count[i] == 1) {
				if (__sys_sigaction(i,&act,NULL) != 0)
					ret = -1;
			}
		}
	}
	/* Done accessing _thread_dfl_count for now. */
	_thread_kern_sig_undefer();

	if (ret == 0) {
		/*
		 * Save the wait signal mask.  The wait signal
		 * mask is independent of the threads signal mask
		 * and requires separate storage.
		 */
		curthread->data.sigwait = &waitset;

		/* Wait for a signal: */
		_thread_kern_sched_state(PS_SIGWAIT, __FILE__, __LINE__);

		/* Return the signal number to the caller: */
		*sig = curthread->signo;

		/*
		 * Probably unnecessary, but since it's in a union struct
		 * we don't know how it could be used in the future.
		 */
		curthread->data.sigwait = NULL;
	}

	/*
	 * Access the _thread_dfl_count array under the protection of signal
	 * deferral.
	 */
	_thread_kern_sig_defer();

	/* Restore the sigactions: */
	act.sa_handler = SIG_DFL;
	for (i = 1; i < NSIG; i++) {
		if (sigismember(&tempset, i)) {
			_thread_dfl_count[i]--;
			if ((_thread_sigact[i - 1].sa_handler == SIG_DFL) &&
			    (_thread_dfl_count[i] == 0)) {
				if (__sys_sigaction(i,&act,NULL) != 0)
					ret = -1;
			}
		}
	}
	/* Done accessing _thread_dfl_count. */
	_thread_kern_sig_undefer();

	_thread_leave_cancellation_point();
	
	/* Return the completion status: */
	return (ret);
}
