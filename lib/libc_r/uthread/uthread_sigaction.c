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
sigaction(int sig, const struct sigaction * act, struct sigaction * oact)
{
	int ret = 0;
	struct sigaction gact;

	/* Check if the signal number is out of range: */
	if (sig < 1 || sig > NSIG) {
		/* Return an invalid argument: */
		errno = EINVAL;
		ret = -1;
	} else {
		/*
		 * Check if the existing signal action structure contents are
		 * to be returned: 
		 */
		if (oact != NULL) {
			/* Return the existing signal action contents: */
			oact->sa_handler = _thread_sigact[sig - 1].sa_handler;
			oact->sa_mask = _thread_sigact[sig - 1].sa_mask;
			oact->sa_flags = _thread_sigact[sig - 1].sa_flags;
		}

		/* Check if a signal action was supplied: */
		if (act != NULL) {
			/* Set the new signal handler: */
			_thread_sigact[sig - 1].sa_mask = act->sa_mask;
			_thread_sigact[sig - 1].sa_flags = act->sa_flags;
			_thread_sigact[sig - 1].sa_handler = act->sa_handler;
		}

		/*
		 * Check if the kernel needs to be advised of a change
		 * in signal action:
		 */
		if (act != NULL && sig != SIGVTALRM && sig != SIGCHLD &&
		    sig != SIGINFO) {
			/* Initialise the global signal action structure: */
			gact.sa_mask = act->sa_mask;
			gact.sa_flags = 0;

			/*
			 * Check if the signal handler is being set to
			 * the default or ignore handlers:
			 */
			if (act->sa_handler == SIG_DFL ||
			    act->sa_handler == SIG_IGN)
				/* Specify the built in handler: */
				gact.sa_handler = act->sa_handler;
			else
				/*
				 * Specify the thread kernel signal
				 * handler:
				 */
				gact.sa_handler = (void (*) ()) _thread_sig_handler;

			/* Change the signal action in the kernel: */
		    	if (_thread_sys_sigaction(sig,&gact,NULL) != 0)
				ret = -1;
		}
	}

	/* Return the completion status: */
	return (ret);
}
#endif
