/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>.
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
#include <errno.h>
#include <pthread.h>
#include "thr_private.h"

static void	suspend_common(struct pthread *thread);

__weak_reference(_pthread_suspend_np, pthread_suspend_np);
__weak_reference(_pthread_suspend_all_np, pthread_suspend_all_np);

/* Suspend a thread: */
int
_pthread_suspend_np(pthread_t thread)
{
	int ret;

	/* Suspending the current thread doesn't make sense. */
	if (thread == _get_curthread())
		ret = EDEADLK;

	/* Find the thread in the list of active threads: */
	else if ((ret = _find_thread(thread)) == 0) {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		suspend_common(thread);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}
	return (ret);
}

void
_pthread_suspend_all_np(void)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread	*thread;

	/*
	 * Defer signals to protect the scheduling queues from
	 * access by the signal handler:
	 */
	_thread_kern_sig_defer();

	TAILQ_FOREACH(thread, &_thread_list, tle) {
		if (thread != curthread)
			suspend_common(thread);
	}

	/*
	 * Undefer and handle pending signals, yielding if
	 * necessary:
	 */
	_thread_kern_sig_undefer();
}

void
suspend_common(struct pthread *thread)
{
	thread->flags |= PTHREAD_FLAGS_SUSPENDED;
	if (thread->flags & PTHREAD_FLAGS_IN_PRIOQ) {
		PTHREAD_PRIOQ_REMOVE(thread);
		PTHREAD_SET_STATE(thread, PS_SUSPENDED);
	}
}
