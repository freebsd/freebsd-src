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
#include <errno.h>
#include <pthread.h>
#include "thr_private.h"

static void	resume_common(struct pthread *);

__weak_reference(_pthread_resume_np, pthread_resume_np);
__weak_reference(_pthread_resume_all_np, pthread_resume_all_np);

/* Resume a thread: */
int
_pthread_resume_np(pthread_t thread)
{
	int ret;

	/* Find the thread in the list of active threads: */
	if ((ret = _find_thread(thread)) == 0) {
		/*
		 * Defer signals to protect the scheduling queues
		 * from access by the signal handler:
		 */
		_thread_kern_sig_defer();

		if ((thread->flags & PTHREAD_FLAGS_SUSPENDED) != 0)
			resume_common(thread);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}
	return (ret);
}

void
_pthread_resume_all_np(void)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread	*thread;

	/*
	 * Defer signals to protect the scheduling queues from access
	 * by the signal handler:
	 */
	_thread_kern_sig_defer();

	TAILQ_FOREACH(thread, &_thread_list, tle) {
		if ((thread != curthread) &&
		    ((thread->flags & PTHREAD_FLAGS_SUSPENDED) != 0))
			resume_common(thread);
	}

	/*
	 * Undefer and handle pending signals, yielding if necessary:
	 */
	_thread_kern_sig_undefer();
}

static void
resume_common(struct pthread *thread)
{
	/* Clear the suspend flag: */
	thread->flags &= ~PTHREAD_FLAGS_SUSPENDED;

	/*
	 * If the thread's state is suspended, that means it is
	 * now runnable but not in any scheduling queue.  Set the
	 * state to running and insert it into the run queue.
	 */
	if (thread->state == PS_SUSPENDED) {
		PTHREAD_SET_STATE(thread, PS_RUNNING);
		if (thread->priority_mutex_count > 0)
			PTHREAD_PRIOQ_INSERT_HEAD(thread);
		else
			PTHREAD_PRIOQ_INSERT_TAIL(thread);
	}
}
