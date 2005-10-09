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

__weak_reference(__sigsuspend, sigsuspend);

int
_sigsuspend(const sigset_t * set)
{
	struct pthread	*curthread = _get_curthread();
	int             ret = -1;
	sigset_t        oset, sigset;

	/* Check if a new signal set was provided by the caller: */
	if (set != NULL) {
		/* Save the current signal mask: */
		oset = curthread->sigmask;

		/* Change the caller's mask: */
		curthread->sigmask = *set;

		/*
		 * Check if there are pending signals for the running
		 * thread or process that aren't blocked:
		 */
		sigset = curthread->sigpend;
		SIGSETOR(sigset, _process_sigpending);
		SIGSETNAND(sigset, curthread->sigmask);
		if (SIGNOTEMPTY(sigset)) {
			/*
			 * Call the kernel scheduler which will safely
			 * install a signal frame for the running thread:
			 */
			_thread_kern_sched_sig();
		} else {
			/* Wait for a signal: */
			_thread_kern_sched_state(PS_SIGSUSPEND,
			    __FILE__, __LINE__);
		}

		/* Always return an interrupted error: */
		errno = EINTR;

		/* Restore the signal mask: */
		curthread->sigmask = oset;
	} else {
		/* Return an invalid argument error: */
		errno = EINVAL;
	}

	/* Return the completion status: */
	return (ret);
}

int
__sigsuspend(const sigset_t * set)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = _sigsuspend(set);
	_thread_leave_cancellation_point();

	return ret;
}
