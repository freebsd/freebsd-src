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
#include "pthread_private.h"

static void	finish_suspension(void *arg);

__weak_reference(_pthread_suspend_np, pthread_suspend_np);

/* Suspend a thread: */
int
_pthread_suspend_np(pthread_t thread)
{
	int ret;

	/* Find the thread in the list of active threads: */
	if ((ret = _find_thread(thread)) == 0) {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		switch (thread->state) {
		case PS_RUNNING:
			/*
			 * Remove the thread from the priority queue and
			 * set the state to suspended:
			 */
			PTHREAD_PRIOQ_REMOVE(thread);
			PTHREAD_SET_STATE(thread, PS_SUSPENDED);
			break;

		case PS_SPINBLOCK:
		case PS_FDR_WAIT:
		case PS_FDW_WAIT:
		case PS_POLL_WAIT:
		case PS_SELECT_WAIT:
			/*
			 * Remove these threads from the work queue
			 * and mark the operation as interrupted:
			 */
			if ((thread->flags & PTHREAD_FLAGS_IN_WORKQ) != 0)
				PTHREAD_WORKQ_REMOVE(thread);
			_thread_seterrno(thread,EINTR);

			/* FALLTHROUGH */
		case PS_SLEEP_WAIT:
			thread->interrupted = 1;

			/* FALLTHROUGH */
		case PS_SIGTHREAD:
		case PS_WAIT_WAIT:
		case PS_SIGSUSPEND:
		case PS_SIGWAIT:
			/*
			 * Remove these threads from the waiting queue and
			 * set their state to suspended:
			 */
			PTHREAD_WAITQ_REMOVE(thread);
			PTHREAD_SET_STATE(thread, PS_SUSPENDED);
			break;

		case PS_MUTEX_WAIT:
			/* Mark the thread as suspended and still in a queue. */
			thread->suspended = SUSP_MUTEX_WAIT;

			PTHREAD_SET_STATE(thread, PS_SUSPENDED);
			break;
		case PS_COND_WAIT:
			/* Mark the thread as suspended and still in a queue. */
			thread->suspended = SUSP_COND_WAIT;

			PTHREAD_SET_STATE(thread, PS_SUSPENDED);
			break;
		case PS_FDLR_WAIT:
		case PS_FDLW_WAIT:
		case PS_FILE_WAIT:
		case PS_JOIN:
			/* Mark the thread as suspended: */
			thread->suspended = SUSP_YES;

			/*
			 * Threads in these states may be in queues.
			 * In order to preserve queue integrity, the
			 * cancelled thread must remove itself from the
			 * queue.  Mark the thread as interrupted and
			 * set the state to running.  When the thread
			 * resumes, it will remove itself from the queue
			 * and call the suspension completion routine.
			 */
			thread->interrupted = 1;
			_thread_seterrno(thread, EINTR);
			PTHREAD_NEW_STATE(thread, PS_RUNNING);
			thread->continuation = finish_suspension;
			break;

		case PS_DEAD:
		case PS_DEADLOCK:
		case PS_STATE_MAX:
		case PS_SUSPENDED:
			/* Nothing needs to be done: */
			break;
		}

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}
	return(ret);
}

static void
finish_suspension(void *arg)
{
	struct pthread	*curthread = _get_curthread();

	if (curthread->suspended != SUSP_NO)
		_thread_kern_sched_state(PS_SUSPENDED, __FILE__, __LINE__);
}


