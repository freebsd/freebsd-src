/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int 
execve(const char *name, char *const * argv, char *const * envp)
{
	int             i;
	int             ret;
	struct sigaction act;
	struct sigaction oact;

	/* Close the pthread kernel pipe: */
	_thread_sys_close(_thread_kern_pipe[0]);
	_thread_sys_close(_thread_kern_pipe[1]);

	/* Enter a loop to adopt the signal actions for the running thread: */
	for (i = 1; i < NSIG; i++) {
		/* Check for signals which cannot be caught: */
		if (i == SIGKILL || i == SIGSTOP) {
			/* Don't do anything with these signals. */
		} else {
			/*
			 * Check if the running thread is ignoring this
			 * signal: 
			 */
			if (_thread_run->act[i - 1].sa_handler == SIG_IGN) {
				/* Continue to ignore this signal: */
				act.sa_handler = SIG_IGN;
			} else {
				/* Use the default handler for this signal: */
				act.sa_handler = SIG_DFL;
			}

			/* Copy the mask and flags for this signal: */
			act.sa_mask = _thread_run->act[i - 1].sa_mask;
			act.sa_flags = _thread_run->act[i - 1].sa_flags;

			/* Change the signal action for the process: */
			_thread_sys_sigaction(i, &act, &oact);
		}
	}

	/* Execute the process: */
	_thread_sys_sigprocmask(SIG_SETMASK, &_thread_run->sigmask, NULL);

	/* Execute the process: */
	ret = _thread_sys_execve(name, argv, envp);

	/* Return the completion status: */
	return (ret);
}
#endif
