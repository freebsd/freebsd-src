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

__weak_reference(_pthread_join, pthread_join);

int
_pthread_join(pthread_t pthread, void **thread_return)
{
	int ret = 0;
	pthread_t thread;
 
	_thread_enter_cancellation_point();

	/* Check if the caller has specified an invalid thread: */
	if (pthread == NULL || pthread->magic != PTHREAD_MAGIC) {
		/* Invalid thread: */
		_thread_leave_cancellation_point();
		return(EINVAL);
	}

	/* Check if the caller has specified itself: */
	if (pthread == curthread) {
		/* Avoid a deadlock condition: */
		_thread_leave_cancellation_point();
		return(EDEADLK);
	}

	/*
	 * Search for the specified thread in the list of active threads.  This
	 * is done manually here rather than calling _find_thread() because
	 * the searches in _thread_list and _dead_list (as well as setting up
	 * join/detach state) have to be done atomically.
	 */
	DEAD_LIST_LOCK;
	THREAD_LIST_LOCK;
	TAILQ_FOREACH(thread, &_thread_list, tle)
		if (thread == pthread) {
			_SPINLOCK(&pthread->lock);
			break;
		}

	if (thread == NULL)
		/*
		 * Search for the specified thread in the list of dead threads:
		 */
		TAILQ_FOREACH(thread, &_dead_list, dle)
			if (thread == pthread) {
				_SPINLOCK(&pthread->lock);
				break;
			}

	/* Check if the thread was not found or has been detached: */
	if (thread == NULL ||
	    ((pthread->attr.flags & PTHREAD_DETACHED) != 0)) {
		if (thread != NULL)
			_SPINUNLOCK(&pthread->lock);
		THREAD_LIST_UNLOCK;
		DEAD_LIST_UNLOCK;
		ret = ESRCH;
		goto out;

	}
	if (pthread->joiner != NULL) {
		/* Multiple joiners are not supported. */
		/* XXXTHR - support multiple joiners. */
		_SPINUNLOCK(&pthread->lock);
		THREAD_LIST_UNLOCK;
		DEAD_LIST_UNLOCK;
		ret = ENOTSUP;
		goto out;

	}

	/* Check if the thread is not dead: */
	if (pthread->state != PS_DEAD) {
		/* Set the running thread to be the joiner: */
		pthread->joiner = curthread;
		_SPINUNLOCK(&pthread->lock);
		_thread_critical_enter(curthread);

		/* Keep track of which thread we're joining to: */
		curthread->join_status.thread = pthread;

		while (curthread->join_status.thread == pthread) {
			PTHREAD_SET_STATE(curthread, PS_JOIN);
			/* Wait for our signal to wake up. */
			_thread_critical_exit(curthread);
			THREAD_LIST_UNLOCK;
			DEAD_LIST_UNLOCK;
			_thread_suspend(curthread, NULL);

			/*
			 * XXX - For correctness reasons.
			 * We must aquire these in the same order and also
			 * importantly, release in the same order, order because
			 * otherwise we might deadlock with the joined thread
			 * when we attempt to release one of these locks.
			 */
			DEAD_LIST_LOCK;
			THREAD_LIST_LOCK;
			_thread_critical_enter(curthread);
		}

		/*
		 * The thread return value and error are set by the thread we're
		 * joining to when it exits or detaches:
		 */
		ret = curthread->join_status.error;
		if ((ret == 0) && (thread_return != NULL))
			*thread_return = curthread->join_status.ret;
		_thread_critical_exit(curthread);
		THREAD_LIST_UNLOCK;
		DEAD_LIST_UNLOCK;
	} else {
		/*
		 * The thread exited (is dead) without being detached, and no
		 * thread has joined it.
		 */

		/* Check if the return value is required: */
		if (thread_return != NULL) {
			/* Return the thread's return value: */
			*thread_return = pthread->ret;
		}

		/* Make the thread collectable by the garbage collector. */
		pthread->attr.flags |= PTHREAD_DETACHED;
		_SPINUNLOCK(&pthread->lock);
		if (pthread_cond_signal(&_gc_cond) != 0)
			PANIC("Cannot signal gc cond");
		THREAD_LIST_UNLOCK;
		DEAD_LIST_UNLOCK;
	}

out:
	_thread_leave_cancellation_point();

	/* Return the completion status: */
	return (ret);
}
