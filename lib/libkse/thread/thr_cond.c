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
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "thr_private.h"

#define	THR_IN_CONDQ(thr)	(((thr)->sflags & THR_FLAGS_IN_SYNCQ) != 0)
#define	THR_IN_CONDQ(thr)	(((thr)->sflags & THR_FLAGS_IN_SYNCQ) != 0)
#define	THR_CONDQ_SET(thr)	(thr)->sflags |= THR_FLAGS_IN_SYNCQ
#define	THR_CONDQ_CLEAR(thr)	(thr)->sflags &= ~THR_FLAGS_IN_SYNCQ

/*
 * Prototypes
 */
static inline struct pthread	*cond_queue_deq(pthread_cond_t);
static inline void		cond_queue_remove(pthread_cond_t, pthread_t);
static inline void		cond_queue_enq(pthread_cond_t, pthread_t);

/*
 * Double underscore versions are cancellation points.  Single underscore
 * versions are not and are provided for libc internal usage (which
 * shouldn't introduce cancellation points).
 */
__weak_reference(__pthread_cond_wait, pthread_cond_wait);
__weak_reference(__pthread_cond_timedwait, pthread_cond_timedwait);

__weak_reference(_pthread_cond_init, pthread_cond_init);
__weak_reference(_pthread_cond_destroy, pthread_cond_destroy);
__weak_reference(_pthread_cond_signal, pthread_cond_signal);
__weak_reference(_pthread_cond_broadcast, pthread_cond_broadcast);


int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
	enum pthread_cond_type type;
	pthread_cond_t	pcond;
	int		flags;
	int             rval = 0;

	if (cond == NULL)
		rval = EINVAL;
	else {
		/*
		 * Check if a pointer to a condition variable attribute
		 * structure was passed by the caller: 
		 */
		if (cond_attr != NULL && *cond_attr != NULL) {
			/* Default to a fast condition variable: */
			type = (*cond_attr)->c_type;
			flags = (*cond_attr)->c_flags;
		} else {
			/* Default to a fast condition variable: */
			type = COND_TYPE_FAST;
			flags = 0;
		}

		/* Process according to condition variable type: */
		switch (type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/* Nothing to do here. */
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		/* Check for no errors: */
		if (rval == 0) {
			if ((pcond = (pthread_cond_t)
			    malloc(sizeof(struct pthread_cond))) == NULL) {
				rval = ENOMEM;
			} else if (_lock_init(&pcond->c_lock, LCK_ADAPTIVE,
			    _thr_lock_wait, _thr_lock_wakeup) != 0) {
				free(pcond);
				rval = ENOMEM;
			} else {
				/*
				 * Initialise the condition variable
				 * structure:
				 */
				TAILQ_INIT(&pcond->c_queue);
				pcond->c_flags |= COND_FLAGS_INITED;
				pcond->c_type = type;
				pcond->c_mutex = NULL;
				pcond->c_seqno = 0;
				*cond = pcond;
			}
		}
	}
	/* Return the completion status: */
	return (rval);
}

int
_pthread_cond_destroy(pthread_cond_t *cond)
{
	struct pthread_cond	*cv;
	struct pthread		*curthread = _get_curthread();
	int			rval = 0;

	if (cond == NULL || *cond == NULL)
		rval = EINVAL;
	else {
		/* Lock the condition variable structure: */
		THR_LOCK_ACQUIRE(curthread, &(*cond)->c_lock);

		/*
		 * NULL the caller's pointer now that the condition
		 * variable has been destroyed:
		 */
		cv = *cond;
		*cond = NULL;

		/* Unlock the condition variable structure: */
		THR_LOCK_RELEASE(curthread, &cv->c_lock);

		/* Free the cond lock structure: */
		_lock_destroy(&cv->c_lock);

		/*
		 * Free the memory allocated for the condition
		 * variable structure:
		 */
		free(cv);

	}
	/* Return the completion status: */
	return (rval);
}

int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	struct pthread	*curthread = _get_curthread();
	int	rval = 0;
	int	done = 0;
	int	interrupted = 0;
	int	unlock_mutex = 1;
	int	seqno;

	if (cond == NULL)
		return (EINVAL);

	/*
	 * If the condition variable is statically initialized,
	 * perform the dynamic initialization:
	 */
	if (*cond == NULL &&
	    (rval = pthread_cond_init(cond, NULL)) != 0)
		return (rval);

	if (!_kse_isthreaded())
		_kse_setthreaded(1);

	/*
	 * Enter a loop waiting for a condition signal or broadcast
	 * to wake up this thread.  A loop is needed in case the waiting
	 * thread is interrupted by a signal to execute a signal handler.
	 * It is not (currently) possible to remain in the waiting queue
	 * while running a handler.  Instead, the thread is interrupted
	 * and backed out of the waiting queue prior to executing the
	 * signal handler.
	 */
	do {
		/* Lock the condition variable structure: */
		THR_LOCK_ACQUIRE(curthread, &(*cond)->c_lock);

		/*
		 * If the condvar was statically allocated, properly
		 * initialize the tail queue.
		 */
		if (((*cond)->c_flags & COND_FLAGS_INITED) == 0) {
			TAILQ_INIT(&(*cond)->c_queue);
			(*cond)->c_flags |= COND_FLAGS_INITED;
		}

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			if ((mutex == NULL) || (((*cond)->c_mutex != NULL) &&
			    ((*cond)->c_mutex != *mutex))) {
				/* Unlock the condition variable structure: */
				THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);

				/* Return invalid argument error: */
				rval = EINVAL;
			} else {
				/* Reset the timeout and interrupted flags: */
				curthread->timeout = 0;
				curthread->interrupted = 0;

				/*
				 * Queue the running thread for the condition
				 * variable:
				 */
				cond_queue_enq(*cond, curthread);

				/* Remember the mutex and sequence number: */
				(*cond)->c_mutex = *mutex;
				seqno = (*cond)->c_seqno;

				/* Wait forever: */
				curthread->wakeup_time.tv_sec = -1;

				/* Unlock the mutex: */
				if ((unlock_mutex != 0) &&
				    ((rval = _mutex_cv_unlock(mutex)) != 0)) {
					/*
					 * Cannot unlock the mutex, so remove
					 * the running thread from the condition
					 * variable queue:
					 */
					cond_queue_remove(*cond, curthread);

					/* Check for no more waiters: */
					if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
						(*cond)->c_mutex = NULL;

					/* Unlock the condition variable structure: */
					THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);
				}
				else {
					/*
					 * Don't unlock the mutex the next
					 * time through the loop (if the
					 * thread has to be requeued after
					 * handling a signal).
					 */
					unlock_mutex = 0;

					/*
					 * This thread is active and is in a
					 * critical region (holding the cv
					 * lock); we should be able to safely
					 * set the state.
					 */
					THR_SCHED_LOCK(curthread, curthread);
					THR_SET_STATE(curthread, PS_COND_WAIT);

					/* Remember the CV: */
					curthread->data.cond = *cond;
					THR_SCHED_UNLOCK(curthread, curthread);

					/* Unlock the CV structure: */
					THR_LOCK_RELEASE(curthread,
					    &(*cond)->c_lock);

					/* Schedule the next thread: */
					_thr_sched_switch(curthread);

					curthread->data.cond = NULL;

					/*
					 * XXX - This really isn't a good check
					 * since there can be more than one
					 * thread waiting on the CV.  Signals
					 * sent to threads waiting on mutexes
					 * or CVs should really be deferred
					 * until the threads are no longer
					 * waiting, but POSIX says that signals
					 * should be sent "as soon as possible".
					 */
					done = (seqno != (*cond)->c_seqno);

					if (THR_IN_SYNCQ(curthread)) {
						/*
						 * Lock the condition variable
						 * while removing the thread.
						 */
						THR_LOCK_ACQUIRE(curthread,
						    &(*cond)->c_lock);

						cond_queue_remove(*cond,
						    curthread);

						/* Check for no more waiters: */
						if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
							(*cond)->c_mutex = NULL;

						THR_LOCK_RELEASE(curthread,
						    &(*cond)->c_lock);
					}

					/*
					 * Save the interrupted flag; locking
					 * the mutex may destroy it.
					 */
					interrupted = curthread->interrupted;

					/*
					 * Note that even though this thread may
					 * have been canceled, POSIX requires
					 * that the mutex be reaquired prior to
					 * cancellation.
					 */
					if (done || interrupted) {
						rval = _mutex_cv_lock(mutex);
						unlock_mutex = 1;
					}
				}
			}
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Unlock the condition variable structure: */
			THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);

			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		if ((interrupted != 0) && (curthread->continuation != NULL))
			curthread->continuation((void *) curthread);
	} while ((done == 0) && (rval == 0));

	/* Return the completion status: */
	return (rval);
}

__strong_reference(_pthread_cond_wait, _thr_cond_wait);

int
__pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	_thr_enter_cancellation_point(curthread);
	ret = _pthread_cond_wait(cond, mutex);
	_thr_leave_cancellation_point(curthread);
	return (ret);
}

int
_pthread_cond_timedwait(pthread_cond_t * cond, pthread_mutex_t * mutex,
		       const struct timespec * abstime)
{
	struct pthread	*curthread = _get_curthread();
	int	rval = 0;
	int	done = 0;
	int	interrupted = 0;
	int	unlock_mutex = 1;
	int	seqno;

	THR_ASSERT(curthread->locklevel == 0,
	    "cv_timedwait: locklevel is not zero!");

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);
	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	if (*cond == NULL && (rval = pthread_cond_init(cond, NULL)) != 0)
		return (rval);

	if (!_kse_isthreaded())
		_kse_setthreaded(1);

	/*
	 * Enter a loop waiting for a condition signal or broadcast
	 * to wake up this thread.  A loop is needed in case the waiting
	 * thread is interrupted by a signal to execute a signal handler.
	 * It is not (currently) possible to remain in the waiting queue
	 * while running a handler.  Instead, the thread is interrupted
	 * and backed out of the waiting queue prior to executing the
	 * signal handler.
	 */
	do {
		/* Lock the condition variable structure: */
		THR_LOCK_ACQUIRE(curthread, &(*cond)->c_lock);

		/*
		 * If the condvar was statically allocated, properly
		 * initialize the tail queue.
		 */
		if (((*cond)->c_flags & COND_FLAGS_INITED) == 0) {
			TAILQ_INIT(&(*cond)->c_queue);
			(*cond)->c_flags |= COND_FLAGS_INITED;
		}

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			if ((mutex == NULL) || (((*cond)->c_mutex != NULL) &&
			    ((*cond)->c_mutex != *mutex))) {
				/* Return invalid argument error: */
				rval = EINVAL;

				/* Unlock the condition variable structure: */
				THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);
			} else {
				/* Set the wakeup time: */
				curthread->wakeup_time.tv_sec = abstime->tv_sec;
				curthread->wakeup_time.tv_nsec =
				    abstime->tv_nsec;

				/* Reset the timeout and interrupted flags: */
				curthread->timeout = 0;
				curthread->interrupted = 0;

				/*
				 * Queue the running thread for the condition
				 * variable:
				 */
				cond_queue_enq(*cond, curthread);

				/* Remember the mutex and sequence number: */
				(*cond)->c_mutex = *mutex;
				seqno = (*cond)->c_seqno;

				/* Unlock the mutex: */
				if ((unlock_mutex != 0) &&
				   ((rval = _mutex_cv_unlock(mutex)) != 0)) {
					/*
					 * Cannot unlock the mutex; remove the
					 * running thread from the condition
					 * variable queue: 
					 */
					cond_queue_remove(*cond, curthread);

					/* Check for no more waiters: */
					if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
						(*cond)->c_mutex = NULL;

					/* Unlock the condition variable structure: */
					THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);
				} else {
					/*
					 * Don't unlock the mutex the next
					 * time through the loop (if the
					 * thread has to be requeued after
					 * handling a signal).
					 */
					unlock_mutex = 0;

					/*
					 * This thread is active and is in a
					 * critical region (holding the cv
					 * lock); we should be able to safely
					 * set the state.
					 */
					THR_SCHED_LOCK(curthread, curthread);
					THR_SET_STATE(curthread, PS_COND_WAIT);

					/* Remember the CV: */
					curthread->data.cond = *cond;
					THR_SCHED_UNLOCK(curthread, curthread);

					/* Unlock the CV structure: */
					THR_LOCK_RELEASE(curthread,
					    &(*cond)->c_lock);

					/* Schedule the next thread: */
					_thr_sched_switch(curthread);

					curthread->data.cond = NULL;

					/*
					 * XXX - This really isn't a good check
					 * since there can be more than one
					 * thread waiting on the CV.  Signals
					 * sent to threads waiting on mutexes
					 * or CVs should really be deferred
					 * until the threads are no longer
					 * waiting, but POSIX says that signals
					 * should be sent "as soon as possible".
					 */
					done = (seqno != (*cond)->c_seqno);

					if (THR_IN_CONDQ(curthread)) {
						/*
						 * Lock the condition variable
						 * while removing the thread.
						 */
						THR_LOCK_ACQUIRE(curthread,
						    &(*cond)->c_lock);

						cond_queue_remove(*cond,
						    curthread);

						/* Check for no more waiters: */
						if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
							(*cond)->c_mutex = NULL;

						THR_LOCK_RELEASE(curthread,
						    &(*cond)->c_lock);
					}

					/*
					 * Save the interrupted flag; locking
					 * the mutex may destroy it.
					 */
					interrupted = curthread->interrupted;
					if (curthread->timeout != 0) {
						/* The wait timedout. */
						rval = ETIMEDOUT;
						(void)_mutex_cv_lock(mutex);
					} else if (interrupted || done) {
						rval = _mutex_cv_lock(mutex);
						unlock_mutex = 1;
					}
				}
			}
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Unlock the condition variable structure: */
			THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);

			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		if ((interrupted != 0) && (curthread->continuation != NULL))
			curthread->continuation((void *)curthread);
	} while ((done == 0) && (rval == 0));

	/* Return the completion status: */
	return (rval);
}

int
__pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	_thr_enter_cancellation_point(curthread);
	ret = _pthread_cond_timedwait(cond, mutex, abstime);
	_thr_leave_cancellation_point(curthread);
	return (ret);
}


int
_pthread_cond_signal(pthread_cond_t * cond)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread	*pthread;
	struct kse_mailbox *kmbx;
	int		rval = 0;

	THR_ASSERT(curthread->locklevel == 0,
	    "cv_timedwait: locklevel is not zero!");
	if (cond == NULL)
		rval = EINVAL;
       /*
        * If the condition variable is statically initialized, perform dynamic
        * initialization.
        */
	else if (*cond != NULL || (rval = pthread_cond_init(cond, NULL)) == 0) {
		/* Lock the condition variable structure: */
		THR_LOCK_ACQUIRE(curthread, &(*cond)->c_lock);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/* Increment the sequence number: */
			(*cond)->c_seqno++;

			/*
			 * Wakeups have to be done with the CV lock held;
			 * otherwise there is a race condition where the
			 * thread can timeout, run on another KSE, and enter
			 * another blocking state (including blocking on a CV).
			 */
			if ((pthread = TAILQ_FIRST(&(*cond)->c_queue))
			    != NULL) {
				THR_SCHED_LOCK(curthread, pthread);
				cond_queue_remove(*cond, pthread);
				if ((pthread->kseg == curthread->kseg) &&
				    (pthread->active_priority >
				    curthread->active_priority))
					curthread->critical_yield = 1;
				kmbx = _thr_setrunnable_unlocked(pthread);
				THR_SCHED_UNLOCK(curthread, pthread);
				if (kmbx != NULL)
					kse_wakeup(kmbx);
			}
			/* Check for no more waiters: */
			if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
				(*cond)->c_mutex = NULL;
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		/* Unlock the condition variable structure: */
		THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);
	}

	/* Return the completion status: */
	return (rval);
}

__strong_reference(_pthread_cond_signal, _thr_cond_signal);

int
_pthread_cond_broadcast(pthread_cond_t * cond)
{
	struct pthread	*curthread = _get_curthread();
	struct pthread	*pthread;
	struct kse_mailbox *kmbx;
	int		rval = 0;

	THR_ASSERT(curthread->locklevel == 0,
	    "cv_timedwait: locklevel is not zero!");
	if (cond == NULL)
		rval = EINVAL;
       /*
        * If the condition variable is statically initialized, perform dynamic
        * initialization.
        */
	else if (*cond != NULL || (rval = pthread_cond_init(cond, NULL)) == 0) {
		/* Lock the condition variable structure: */
		THR_LOCK_ACQUIRE(curthread, &(*cond)->c_lock);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/* Increment the sequence number: */
			(*cond)->c_seqno++;

			/*
			 * Enter a loop to bring all threads off the
			 * condition queue:
			 */
			while ((pthread = TAILQ_FIRST(&(*cond)->c_queue))
			    != NULL) {
				THR_SCHED_LOCK(curthread, pthread);
				cond_queue_remove(*cond, pthread);
				if ((pthread->kseg == curthread->kseg) &&
				    (pthread->active_priority >
				    curthread->active_priority))
					curthread->critical_yield = 1;
				kmbx = _thr_setrunnable_unlocked(pthread);
				THR_SCHED_UNLOCK(curthread, pthread);
				if (kmbx != NULL)
					kse_wakeup(kmbx);
			}

			/* There are no more waiting threads: */
			(*cond)->c_mutex = NULL;
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		/* Unlock the condition variable structure: */
		THR_LOCK_RELEASE(curthread, &(*cond)->c_lock);
	}

	/* Return the completion status: */
	return (rval);
}

__strong_reference(_pthread_cond_broadcast, _thr_cond_broadcast);

void
_cond_wait_backout(struct pthread *curthread)
{
	pthread_cond_t	cond;

	cond = curthread->data.cond;
	if (cond != NULL) {
		/* Lock the condition variable structure: */
		THR_LOCK_ACQUIRE(curthread, &cond->c_lock);

		/* Process according to condition variable type: */
		switch (cond->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			cond_queue_remove(cond, curthread);

			/* Check for no more waiters: */
			if (TAILQ_FIRST(&cond->c_queue) == NULL)
				cond->c_mutex = NULL;
			break;

		default:
			break;
		}

		/* Unlock the condition variable structure: */
		THR_LOCK_RELEASE(curthread, &cond->c_lock);
	}
}

/*
 * Dequeue a waiting thread from the head of a condition queue in
 * descending priority order.
 */
static inline struct pthread *
cond_queue_deq(pthread_cond_t cond)
{
	struct pthread	*pthread;

	while ((pthread = TAILQ_FIRST(&cond->c_queue)) != NULL) {
		TAILQ_REMOVE(&cond->c_queue, pthread, sqe);
		THR_CONDQ_CLEAR(pthread);
		if ((pthread->timeout == 0) && (pthread->interrupted == 0))
			/*
			 * Only exit the loop when we find a thread
			 * that hasn't timed out or been canceled;
			 * those threads are already running and don't
			 * need their run state changed.
			 */
			break;
	}

	return (pthread);
}

/*
 * Remove a waiting thread from a condition queue in descending priority
 * order.
 */
static inline void
cond_queue_remove(pthread_cond_t cond, struct pthread *pthread)
{
	/*
	 * Because pthread_cond_timedwait() can timeout as well
	 * as be signaled by another thread, it is necessary to
	 * guard against removing the thread from the queue if
	 * it isn't in the queue.
	 */
	if (THR_IN_CONDQ(pthread)) {
		TAILQ_REMOVE(&cond->c_queue, pthread, sqe);
		THR_CONDQ_CLEAR(pthread);
	}
}

/*
 * Enqueue a waiting thread to a condition queue in descending priority
 * order.
 */
static inline void
cond_queue_enq(pthread_cond_t cond, struct pthread *pthread)
{
	struct pthread *tid = TAILQ_LAST(&cond->c_queue, cond_head);

	THR_ASSERT(!THR_IN_SYNCQ(pthread),
	    "cond_queue_enq: thread already queued!");

	/*
	 * For the common case of all threads having equal priority,
	 * we perform a quick check against the priority of the thread
	 * at the tail of the queue.
	 */
	if ((tid == NULL) || (pthread->active_priority <= tid->active_priority))
		TAILQ_INSERT_TAIL(&cond->c_queue, pthread, sqe);
	else {
		tid = TAILQ_FIRST(&cond->c_queue);
		while (pthread->active_priority <= tid->active_priority)
			tid = TAILQ_NEXT(tid, sqe);
		TAILQ_INSERT_BEFORE(tid, pthread, sqe);
	}
	THR_CONDQ_SET(pthread);
	pthread->data.cond = cond;
}
