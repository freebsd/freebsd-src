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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_sigmask.c,v 1.5 1999/09/29 15:18:40 marcel Exp $
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <errno.h>
#include <signal.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
pthread_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
	int ret = 0;

	/* Check if the existing signal process mask is to be returned: */
	if (oset != NULL) {
		/* Return the current mask: */
		*oset = _thread_run->sigmask;
	}
	/* Check if a new signal set was provided by the caller: */
	if (set != NULL) {
		/* Process according to what to do: */
		switch (how) {
		/* Block signals: */
		case SIG_BLOCK:
			/* Add signals to the existing mask: */
			SIGSETOR(_thread_run->sigmask, *set);
			break;

		/* Unblock signals: */
		case SIG_UNBLOCK:
			/* Clear signals from the existing mask: */
			SIGSETNAND(_thread_run->sigmask, *set);
			break;

		/* Set the signal process mask: */
		case SIG_SETMASK:
			/* Set the new mask: */
			_thread_run->sigmask = *set;
			break;

		/* Trap invalid actions: */
		default:
			/* Return an invalid argument: */
			errno = EINVAL;
			ret = -1;
			break;
		}

		/*
		 * Dispatch signals to the running thread that are pending
		 * and now unblocked:
		 */
		_dispatch_signals();
	}

	/* Return the completion status: */
	return (ret);
}
#endif
