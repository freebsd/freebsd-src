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

	/* Ignored signals get dropped on the floor. */
	else if (_thread_sigact[sig - 1].sa_handler == SIG_IGN)
		ret = 0;

	/* Find the thread in the list of active threads: */
	else if ((ret = _find_thread(pthread)) == 0) {
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

		default:
			/* Increment the pending signal count: */
			sigaddset(&pthread->sigpend,sig);
			break;
		}
	}

	/* Return the completion status: */
	return (ret);
}
#endif
