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
#include <stdlib.h>
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
		_thread_critical_enter(curthread);

		if ((thread->flags & PTHREAD_FLAGS_SUSPENDED) != 0)
			resume_common(thread);

		_thread_critical_exit(curthread);
	}
	return (ret);
}

void
_pthread_resume_all_np(void)
{
	struct pthread	*thread;

	THREAD_LIST_LOCK;
	TAILQ_FOREACH(thread, &_thread_list, tle) {
		if ((thread != curthread) &&
		    ((thread->flags & PTHREAD_FLAGS_SUSPENDED) != 0)) {
			_thread_critical_enter(thread);
			resume_common(thread);
			_thread_critical_exit(thread);
		}
	}
	THREAD_LIST_UNLOCK;
}

/*
 * The caller is required to have locked the thread before
 * calling this function.
 */
static void
resume_common(struct pthread *thread)
{
	thread->flags &= ~PTHREAD_FLAGS_SUSPENDED;
	if (thr_kill(thread->thr_id, SIGTHR))
		abort();
}
