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
 * $Id$
 */
#include <errno.h>
#include <signal.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
pthread_kill(pthread_t pthread, int sig)
{
	int ret;

	/* Check for invalid signal numbers: */
	if (sig < 0 || sig >= NSIG)
		/* Invalid signal: */
		ret = EINVAL;

	/*
	 * Ensure the thread is in the list of active threads, and the
	 * signal is valid (signal 0 specifies error checking only) and
	 * not being ignored:
	 */
	else if (((ret = _find_thread(pthread)) == 0) && (sig > 0) &&
	    (_thread_sigact[sig - 1].sa_handler != SIG_IGN)) {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		switch (pthread->state) {
		case PS_SIGSUSPEND:
			/*
			 * Only wake up the thread if the signal is unblocked
			 * and there is a handler installed for the signal.
			 */
			if (!sigismember(&pthread->sigmask, sig) &&
			    _thread_sigact[sig - 1].sa_handler != SIG_DFL) {
				/* Change the state of the thread to run: */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);

				/* Return the signal number: */
				pthread->signo = sig;
			}
			/* Increment the pending signal count: */
			sigaddset(&pthread->sigpend,sig);
			break;

		case PS_SIGWAIT:
			/* Wake up the thread if the signal is blocked. */
			if (sigismember(pthread->data.sigwait, sig)) {
				/* Change the state of the thread to run: */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);

				/* Return the signal number: */
				pthread->signo = sig;
			} else
				/* Increment the pending signal count. */
				sigaddset(&pthread->sigpend,sig);
			break;

		case PS_FDR_WAIT:
		case PS_FDW_WAIT:
		case PS_POLL_WAIT:
		case PS_SLEEP_WAIT:
		case PS_SELECT_WAIT:
			if (!sigismember(&pthread->sigmask, sig) &&
			    (_thread_sigact[sig - 1].sa_handler != SIG_IGN)) {
				/* Flag the operation as interrupted: */
				pthread->interrupted = 1;

				if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
					PTHREAD_WORKQ_REMOVE(pthread);

				/* Change the state of the thread to run: */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);

				/* Return the signal number: */
				pthread->signo = sig;
			} else {
				/* Increment the pending signal count: */
				sigaddset(&pthread->sigpend,sig);
			}
			break;

		default:
			/* Increment the pending signal count: */
			sigaddset(&pthread->sigpend,sig);
			break;
		}


		/*
		 * Check that a custom handler is installed
		 * and if the signal is not blocked:
		 */
		if (_thread_sigact[sig - 1].sa_handler != SIG_DFL &&
		    _thread_sigact[sig - 1].sa_handler != SIG_IGN &&
		    sigismember(&pthread->sigpend, sig) &&
		    !sigismember(&pthread->sigmask, sig)) {
			pthread_t pthread_saved = _thread_run;

			/* Current thread inside critical region? */
			if (_thread_run->sig_defer_count > 0)
				pthread->sig_defer_count++;

			_thread_run = pthread;

			/* Clear the pending signal: */
			sigdelset(&pthread->sigpend, sig);

			/*
			 * Dispatch the signal via the custom signal
			 * handler:
			 */
			(*(_thread_sigact[sig - 1].sa_handler))(sig);

			_thread_run = pthread_saved;

			if (_thread_run->sig_defer_count > 0)
				pthread->sig_defer_count--;
		}

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}

	/* Return the completion status: */
	return (ret);
}
#endif
