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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <signal.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
sigwait(const sigset_t * set, int *sig)
{
	int		ret = 0;
	int		i;
	sigset_t	oset;
	struct sigaction act;
	
	/*
	 * Specify the thread kernel signal handler.
	 */
	act.sa_handler = (void (*) ()) _thread_sig_handler;
	act.sa_flags = SA_RESTART;
	act.sa_mask = *set;

	/*
	 * These signals can't be waited on.
	 */
	sigdelset(&act.sa_mask, SIGKILL);
	sigdelset(&act.sa_mask, SIGSTOP);
	sigdelset(&act.sa_mask, SIGVTALRM);
	sigdelset(&act.sa_mask, SIGCHLD);
	sigdelset(&act.sa_mask, SIGINFO);

	/*
	 * Enter a loop to find the signals that are SIG_DFL.  For
	 * these signals we must install a dummy signal handler in
	 * order for the kernel to pass them in to us.  POSIX says
	 * that the application must explicitly install a dummy
	 * handler for signals that are SIG_IGN in order to sigwait
	 * on them, so we ignore SIG_IGN signals.
	 */
	for (i = 1; i < NSIG; i++) {
		if (sigismember(&act.sa_mask, i)) {
			if (_thread_sigact[i - 1].sa_handler == SIG_DFL) {
				if (_thread_sys_sigaction(i,&act,NULL) != 0)
					ret = -1;
			}
			else if (_thread_sigact[i - 1].sa_handler == SIG_IGN)
				sigdelset(&act.sa_mask, i);
		}
	}
	if (ret == 0) {

		/* Save the current signal mask: */
		oset = _thread_run->sigmask;

		/* Combine the caller's mask with the current one: */
		_thread_run->sigmask |= act.sa_mask;

		/* Wait for a signal: */
		_thread_kern_sched_state(PS_SIGWAIT, __FILE__, __LINE__);

		/* Return the signal number to the caller: */
		*sig = _thread_run->signo;

		/* Restore the signal mask: */
		_thread_run->sigmask = oset;
	}

	/* Restore the sigactions: */
	act.sa_handler = SIG_DFL;
	for (i = 1; i < NSIG; i++) {
		if (sigismember(&act.sa_mask, i) &&
		    (_thread_sigact[i - 1].sa_handler == SIG_DFL)) {
			if (_thread_sys_sigaction(i,&act,NULL) != 0)
				ret = -1;
		}
	}

	/* Return the completion status: */
	return (ret);
}
#endif
