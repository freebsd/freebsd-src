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
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
pthread_join(pthread_t pthread, void **thread_return)
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
	if (pthread == _thread_run) {
		/* Avoid a deadlock condition: */
		_thread_leave_cancellation_point();
		return(EDEADLK);
	}

	/*
	 * Lock the garbage collector mutex to ensure that the garbage
	 * collector is not using the dead thread list.
	 */
	if (pthread_mutex_lock(&_gc_mutex) != 0)
		PANIC("Cannot lock gc mutex");

	/*
	 * Defer signals to protect the thread list from access
	 * by the signal handler:
	 */
	_thread_kern_sig_defer();

	/*
	 * Unlock the garbage collector mutex, now that the garbage collector
	 * can't be:
	 */
	if (pthread_mutex_unlock(&_gc_mutex) != 0)
		PANIC("Cannot lock gc mutex");

	/*
	 * Search for the specified thread in the list of active threads.  This
	 * is done manually here rather than calling _find_thread() because
	 * the searches in _thread_list and _dead_list (as well as setting up
	 * join/detach state) have to be done atomically.
	 */
	TAILQ_FOREACH(thread, &_thread_list, tle) {
		if (thread == pthread)
			break;
	}
	if (thread == NULL) {
		/*
		 * Search for the specified thread in the list of dead threads:
		 */
		TAILQ_FOREACH(thread, &_dead_list, dle) {
			if (thread == pthread)
				break;
		}
	}

	/* Check if the thread was not found or has been detached: */
	if (thread == NULL ||
	    ((pthread->attr.flags & PTHREAD_DETACHED) != 0)) {
		/* Undefer and handle pending signals, yielding if necessary: */
		_thread_kern_sig_undefer();

		/* Return an error: */
		ret = ESRCH;

	} else if (pthread->joiner != NULL) {
		/* Undefer and handle pending signals, yielding if necessary: */
		_thread_kern_sig_undefer();

		/* Multiple joiners are not supported. */
		ret = ENOTSUP;

	/* Check if the thread is not dead: */
	} else if (pthread->state != PS_DEAD) {
		/* Set the running thread to be the joiner: */
		pthread->joiner = _thread_run;

		/* Keep track of which thread we're joining to: */
		_thread_run->data.thread = pthread;

		/* Schedule the next thread: */
		_thread_kern_sched_state(PS_JOIN, __FILE__, __LINE__);

 		/*
		 * The thread return value and error are set by the thread we're
		 * joining to when it exits or detaches:
 		 */
		ret = _thread_run->error;
		if ((ret == 0) && (thread_return != NULL))
			*thread_return = _thread_run->ret;
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

		/* Undefer and handle pending signals, yielding if necessary: */
		_thread_kern_sig_undefer();
	}

	_thread_leave_cancellation_point();

	/* Return the completion status: */
	return (ret);
}
#endif
