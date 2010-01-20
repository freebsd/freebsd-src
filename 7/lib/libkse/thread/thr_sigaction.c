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
 * 3. Neither the name of the author nor the names of any co-contributors
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

#include "namespace.h"
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

LT10_COMPAT_PRIVATE(_sigaction);
LT10_COMPAT_DEFAULT(sigaction);

__weak_reference(_sigaction, sigaction);

int
_sigaction(int sig, const struct sigaction * act, struct sigaction * oact)
{
	int ret = 0;
	int err = 0;
	struct sigaction newact, oldact;
	struct pthread *curthread;
	kse_critical_t crit;

	/* Check if the signal number is out of range: */
	if (sig < 1 || sig > _SIG_MAXSIG) {
		/* Return an invalid argument: */
		errno = EINVAL;
		ret = -1;
	} else {
		if (act)
			newact = *act;

		crit = _kse_critical_enter();
		curthread = _get_curthread();
		KSE_LOCK_ACQUIRE(curthread->kse, &_thread_signal_lock);

		oldact = _thread_sigact[sig - 1];

		/* Check if a signal action was supplied: */
		if (act != NULL) {
			/* Set the new signal handler: */
			_thread_sigact[sig - 1] = newact;
		}

		/*
		 * Check if the kernel needs to be advised of a change
		 * in signal action:
		 */
		if (act != NULL) {

			newact.sa_flags |= SA_SIGINFO;

			/*
			 * Check if the signal handler is being set to
			 * the default or ignore handlers:
			 */
			if (newact.sa_handler != SIG_DFL &&
			    newact.sa_handler != SIG_IGN) {
				/*
				 * Specify the thread kernel signal
				 * handler:
				 */
				newact.sa_sigaction = _thr_sig_handler;
			}
			/*
			 * Install libpthread signal handler wrapper
			 * for SIGINFO signal if threads dump enabled
			 * even if a user set the signal handler to
			 * SIG_DFL or SIG_IGN.
			 */
			if (sig == SIGINFO && _thr_dump_enabled()) {
				newact.sa_sigaction = _thr_sig_handler;
			}
			/* Change the signal action in the kernel: */
			if (__sys_sigaction(sig, &newact, NULL) != 0) {
				_thread_sigact[sig - 1] = oldact;
				/* errno is in kse, will copy it to thread */
				err = errno;
				ret = -1;
			}
		}
		KSE_LOCK_RELEASE(curthread->kse, &_thread_signal_lock);
		_kse_critical_leave(crit);
		/*
		 * Check if the existing signal action structure contents are
		 * to be returned: 
		*/
		if (oact != NULL) {
			/* Return the existing signal action contents: */
			*oact = oldact;
		}
		if (ret != 0) {
			/* Return errno to thread */
			errno = err;
		}
	}

	/* Return the completion status: */
	return (ret);
}
