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
	 * Find the thread in the list of active threads or in the
	 * list of dead threads:
	 */
	if ((_find_thread(pthread) != 0) && (_find_dead_thread(pthread) != 0))
		/* Return an error: */
		ret = ESRCH;

	/* Check if this thread has been detached: */
	else if ((pthread->attr.flags & PTHREAD_DETACHED) != 0)
		/* Return an error: */
		ret = ESRCH;

	/* Check if the thread is not dead: */
	else if (pthread->state != PS_DEAD) {
		PTHREAD_ASSERT_NOT_IN_SYNCQ(_thread_run);

		/*
		 * Enter a loop in case this thread is woken prematurely
		 * in order to invoke a signal handler:
		 */
		for (;;) {
			/* Clear the interrupted flag: */
			_thread_run->interrupted = 0;

			/*
			 * Protect against being context switched out while
			 * adding this thread to the join queue.
			 */
			_thread_kern_sig_defer();

			/* Add the running thread to the join queue: */
			TAILQ_INSERT_TAIL(&(pthread->join_queue),
			    _thread_run, sqe);
			_thread_run->flags |= PTHREAD_FLAGS_IN_JOINQ;
			_thread_run->data.thread = pthread;

			/* Schedule the next thread: */
			_thread_kern_sched_state(PS_JOIN, __FILE__, __LINE__);

			if ((_thread_run->flags & PTHREAD_FLAGS_IN_JOINQ) != 0) {
				TAILQ_REMOVE(&(pthread->join_queue),
				    _thread_run, sqe);
				_thread_run->flags &= ~PTHREAD_FLAGS_IN_JOINQ;
			}
			_thread_run->data.thread = NULL;

			_thread_kern_sig_undefer();

			if (_thread_run->interrupted != 0) {
				if (_thread_run->continuation != NULL)
					_thread_run->continuation(_thread_run);
				/*
				 * This thread was interrupted, probably to
				 * invoke a signal handler.  Make sure the
				 * target thread is still joinable.
				 */
				if (((_find_thread(pthread) != 0) &&
				    (_find_dead_thread(pthread) != 0)) ||
				    ((pthread->attr.flags &
				    PTHREAD_DETACHED) != 0)) {
					/* Return an error: */
					ret = ESRCH;

					/* We're done; break out of the loop. */
					break;
				}
				else if (pthread->state == PS_DEAD) {
					/* We're done; break out of the loop. */
					break;
				}
			} else {
				/*
				 * The thread return value and error are set
				 * by the thread we're joining to when it
				 * exits or detaches:
				 */
				ret = _thread_run->error;
				if ((ret == 0) && (thread_return != NULL))
					*thread_return = _thread_run->ret;

				/* We're done; break out of the loop. */
				break;
			}
		}
	/* Check if the return value is required: */
	} else if (thread_return != NULL)
		/* Return the thread's return value: */
		*thread_return = pthread->ret;

	_thread_leave_cancellation_point();

	/* Return the completion status: */
	return (ret);
}

void
_join_backout(pthread_t pthread)
{
	_thread_kern_sig_defer();
	if ((pthread->flags & PTHREAD_FLAGS_IN_JOINQ) != 0) {
		TAILQ_REMOVE(&pthread->data.thread->join_queue, pthread, sqe);
		_thread_run->flags &= ~PTHREAD_FLAGS_IN_JOINQ;
	}
	_thread_kern_sig_undefer();
}
#endif
