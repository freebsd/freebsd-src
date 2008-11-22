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
	struct pthread *curthread = _get_curthread();
	void *tmp;
	kse_critical_t crit;
	int ret = 0;
 
	_thr_cancel_enter(curthread);

	/* Check if the caller has specified an invalid thread: */
	if (pthread == NULL || pthread->magic != THR_MAGIC) {
		/* Invalid thread: */
		_thr_cancel_leave(curthread, 1);
		return (EINVAL);
	}

	/* Check if the caller has specified itself: */
	if (pthread == curthread) {
		/* Avoid a deadlock condition: */
		_thr_cancel_leave(curthread, 1);
		return (EDEADLK);
	}

	/*
	 * Find the thread in the list of active threads or in the
	 * list of dead threads:
	 */
	if ((ret = _thr_ref_add(curthread, pthread, /*include dead*/1)) != 0) {
		/* Return an error: */
		_thr_cancel_leave(curthread, 1);
		return (ESRCH);
	}

	THR_SCHED_LOCK(curthread, pthread);
	/* Check if this thread has been detached: */
	if ((pthread->attr.flags & PTHREAD_DETACHED) != 0) {
		THR_SCHED_UNLOCK(curthread, pthread);
		/* Remove the reference and return an error: */
		_thr_ref_delete(curthread, pthread);
		ret = EINVAL;
	} else {
		/* Lock the target thread while checking its state. */
		if (pthread->state == PS_DEAD) {
			/* Return the thread's return value: */
			tmp = pthread->ret;

			/* Detach the thread. */
			pthread->attr.flags |= PTHREAD_DETACHED;

			/* Unlock the thread. */
			THR_SCHED_UNLOCK(curthread, pthread);

			/*
			 * Remove the thread from the list of active
			 * threads and add it to the GC list.
			 */
			crit = _kse_critical_enter();
			KSE_LOCK_ACQUIRE(curthread->kse, &_thread_list_lock);
			THR_LIST_REMOVE(pthread);
			THR_GCLIST_ADD(pthread);
			KSE_LOCK_RELEASE(curthread->kse, &_thread_list_lock);
			_kse_critical_leave(crit);

			/* Remove the reference. */
			_thr_ref_delete(curthread, pthread);
			if (thread_return != NULL)
				*thread_return = tmp;
		}
		else if (pthread->joiner != NULL) {
			/* Unlock the thread and remove the reference. */
			THR_SCHED_UNLOCK(curthread, pthread);
			_thr_ref_delete(curthread, pthread);

			/* Multiple joiners are not supported. */
			ret = ENOTSUP;
		}
		else {
			/* Set the running thread to be the joiner: */
			pthread->joiner = curthread;

			/* Keep track of which thread we're joining to: */
			curthread->join_status.thread = pthread;

			/* Unlock the thread and remove the reference. */
			THR_SCHED_UNLOCK(curthread, pthread);
			_thr_ref_delete(curthread, pthread);

			THR_SCHED_LOCK(curthread, curthread);
			while (curthread->join_status.thread == pthread) {
				THR_SET_STATE(curthread, PS_JOIN);
				THR_SCHED_UNLOCK(curthread, curthread);
				/* Schedule the next thread: */
				_thr_sched_switch(curthread);
				THR_SCHED_LOCK(curthread, curthread);
			}
			THR_SCHED_UNLOCK(curthread, curthread);

			if ((curthread->cancelflags & THR_CANCELLING) &&
			   !(curthread->cancelflags & PTHREAD_CANCEL_DISABLE)) {
				if (_thr_ref_add(curthread, pthread, 1) == 0) {
					THR_SCHED_LOCK(curthread, pthread);
					pthread->joiner = NULL;
					THR_SCHED_UNLOCK(curthread, pthread);
					_thr_ref_delete(curthread, pthread);
				}
				pthread_exit(PTHREAD_CANCELED);
			}

			/*
			 * The thread return value and error are set by the
			 * thread we're joining to when it exits or detaches:
			 */
			ret = curthread->join_status.error;
			if ((ret == 0) && (thread_return != NULL))
				*thread_return = curthread->join_status.ret;
		}
	}
	_thr_cancel_leave(curthread, 1);

	/* Return the completion status: */
	return (ret);
}
