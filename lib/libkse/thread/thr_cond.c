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

/*
 * Prototypes
 */
static inline pthread_t	cond_queue_deq(pthread_cond_t);
static inline void	cond_queue_remove(pthread_cond_t, pthread_t);
static inline void	cond_queue_enq(pthread_cond_t, pthread_t);

__weak_reference(_pthread_cond_init, pthread_cond_init);
__weak_reference(_pthread_cond_destroy, pthread_cond_destroy);
__weak_reference(_pthread_cond_wait, pthread_cond_wait);
__weak_reference(_pthread_cond_timedwait, pthread_cond_timedwait);
__weak_reference(_pthread_cond_signal, pthread_cond_signal);
__weak_reference(_pthread_cond_broadcast, pthread_cond_broadcast);


/* Reinitialize a condition variable to defaults. */
int
_cond_reinit(pthread_cond_t *cond)
{
	int ret = 0;

	if (cond == NULL)
		ret = EINVAL;
	else if (*cond == NULL)
		ret = pthread_cond_init(cond, NULL);
	else {
		/*
		 * Initialize the condition variable structure:
		 */
		TAILQ_INIT(&(*cond)->c_queue);
		(*cond)->c_flags = COND_FLAGS_INITED;
		(*cond)->c_type = COND_TYPE_FAST;
		(*cond)->c_mutex = NULL;
		(*cond)->c_seqno = 0;
		memset(&(*cond)->lock, 0, sizeof((*cond)->lock));
	}
	return (ret);
}

int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
	enum pthread_cond_type type;
	pthread_cond_t	pcond;
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
		} else {
			/* Default to a fast condition variable: */
			type = COND_TYPE_FAST;
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
				memset(&pcond->lock,0,sizeof(pcond->lock));
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
	int             rval = 0;

	if (cond == NULL || *cond == NULL)
		rval = EINVAL;
	else {
		/* Lock the condition variable structure: */
		_SPINLOCK(&(*cond)->lock);

		/*
		 * Free the memory allocated for the condition
		 * variable structure:
		 */
		free(*cond);

		/*
		 * NULL the caller's pointer now that the condition
		 * variable has been destroyed:
		 */
		*cond = NULL;
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
	int	seqno;

	_thread_enter_cancellation_point();
	
	if (cond == NULL)
		return (EINVAL);

	/*
	 * If the condition variable is statically initialized,
	 * perform the dynamic initialization:
	 */
	if (*cond == NULL &&
	    (rval = pthread_cond_init(cond, NULL)) != 0)
		return (rval);

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
		_SPINLOCK(&(*cond)->lock);

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
				_SPINUNLOCK(&(*cond)->lock);

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
				if ((rval = _mutex_cv_unlock(mutex)) != 0) {
					/*
					 * Cannot unlock the mutex, so remove
					 * the running thread from the condition
					 * variable queue:
					 */
					cond_queue_remove(*cond, curthread);

					/* Check for no more waiters: */
					if (TAILQ_FIRST(&(*cond)->c_queue) ==
					    NULL)
						(*cond)->c_mutex = NULL;

					/* Unlock the condition variable structure: */
					_SPINUNLOCK(&(*cond)->lock);
				} else {
					/*
					 * Schedule the next thread and unlock
					 * the condition variable structure:
					 */
					_thread_kern_sched_state_unlock(PS_COND_WAIT,
				    	    &(*cond)->lock, __FILE__, __LINE__);

					done = (seqno != (*cond)->c_seqno);

					interrupted = curthread->interrupted;

					/*
					 * Check if the wait was interrupted
					 * (canceled) or needs to be resumed
					 * after handling a signal.
					 */
					if (interrupted != 0) {
						/*
						 * Lock the mutex and ignore any
						 * errors.  Note that even
						 * though this thread may have
						 * been canceled, POSIX requires
						 * that the mutex be reaquired
						 * prior to cancellation.
						 */
						(void)_mutex_cv_lock(mutex);
					} else {
						/*
						 * Lock the condition variable
						 * while removing the thread.
						 */
						_SPINLOCK(&(*cond)->lock);

						cond_queue_remove(*cond,
						    curthread);

						/* Check for no more waiters: */
						if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
							(*cond)->c_mutex = NULL;

						_SPINUNLOCK(&(*cond)->lock);

						/* Lock the mutex: */
						rval = _mutex_cv_lock(mutex);
					}
				}
			}
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Unlock the condition variable structure: */
			_SPINUNLOCK(&(*cond)->lock);

			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		if ((interrupted != 0) && (curthread->continuation != NULL))
			curthread->continuation((void *) curthread);
	} while ((done == 0) && (rval == 0));

	_thread_leave_cancellation_point();

	/* Return the completion status: */
	return (rval);
}

int
_pthread_cond_timedwait(pthread_cond_t * cond, pthread_mutex_t * mutex,
		       const struct timespec * abstime)
{
	struct pthread	*curthread = _get_curthread();
	int	rval = 0;
	int	done = 0;
	int	interrupted = 0;
	int	seqno;

	_thread_enter_cancellation_point();
	
	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);
	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	if (*cond == NULL && (rval = pthread_cond_init(cond, NULL)) != 0)
		return (rval);

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
		_SPINLOCK(&(*cond)->lock);

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
				_SPINUNLOCK(&(*cond)->lock);
			} else {
				/* Set the wakeup time: */
				curthread->wakeup_time.tv_sec =
				    abstime->tv_sec;
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
				if ((rval = _mutex_cv_unlock(mutex)) != 0) {
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
					_SPINUNLOCK(&(*cond)->lock);
				} else {
					/*
					 * Schedule the next thread and unlock
					 * the condition variable structure:
					 */
					_thread_kern_sched_state_unlock(PS_COND_WAIT,
				  	     &(*cond)->lock, __FILE__, __LINE__);

					done = (seqno != (*cond)->c_seqno);

					interrupted = curthread->interrupted;

					/*
					 * Check if the wait was interrupted
					 * (canceled) or needs to be resumed
					 * after handling a signal.
					 */
					if (interrupted != 0) {
						/*
						 * Lock the mutex and ignore any
						 * errors.  Note that even
						 * though this thread may have
						 * been canceled, POSIX requires
						 * that the mutex be reaquired
						 * prior to cancellation.
						 */
						(void)_mutex_cv_lock(mutex);
					} else {
						/*
						 * Lock the condition variable
						 * while removing the thread.
						 */
						_SPINLOCK(&(*cond)->lock);

						cond_queue_remove(*cond,
						    curthread);

						/* Check for no more waiters: */
						if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
							(*cond)->c_mutex = NULL;

						_SPINUNLOCK(&(*cond)->lock);

						/* Lock the mutex: */
						rval = _mutex_cv_lock(mutex);

						/*
						 * Return ETIMEDOUT if the wait
						 * timed out and there wasn't an
						 * error locking the mutex:
						 */
						if ((curthread->timeout != 0)
						    && rval == 0)
							rval = ETIMEDOUT;

					}
				}
			}
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Unlock the condition variable structure: */
			_SPINUNLOCK(&(*cond)->lock);

			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		if ((interrupted != 0) && (curthread->continuation != NULL))
			curthread->continuation((void *) curthread);
	} while ((done == 0) && (rval == 0));

	_thread_leave_cancellation_point();

	/* Return the completion status: */
	return (rval);
}

int
_pthread_cond_signal(pthread_cond_t * cond)
{
	int             rval = 0;
	pthread_t       pthread;

	if (cond == NULL)
		rval = EINVAL;
       /*
        * If the condition variable is statically initialized, perform dynamic
        * initialization.
        */
	else if (*cond != NULL || (rval = pthread_cond_init(cond, NULL)) == 0) {
		/*
		 * Defer signals to protect the scheduling queues
		 * from access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/* Lock the condition variable structure: */
		_SPINLOCK(&(*cond)->lock);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/* Increment the sequence number: */
			(*cond)->c_seqno++;

			if ((pthread = cond_queue_deq(*cond)) != NULL) {
				/*
				 * Wake up the signaled thread:
				 */
				PTHREAD_NEW_STATE(pthread, PS_RUNNING);
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
		_SPINUNLOCK(&(*cond)->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}

	/* Return the completion status: */
	return (rval);
}

int
_pthread_cond_broadcast(pthread_cond_t * cond)
{
	int             rval = 0;
	pthread_t       pthread;

	if (cond == NULL)
		rval = EINVAL;
       /*
        * If the condition variable is statically initialized, perform dynamic
        * initialization.
        */
	else if (*cond != NULL || (rval = pthread_cond_init(cond, NULL)) == 0) {
		/*
		 * Defer signals to protect the scheduling queues
		 * from access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/* Lock the condition variable structure: */
		_SPINLOCK(&(*cond)->lock);

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
			while ((pthread = cond_queue_deq(*cond)) != NULL) {
				/*
				 * Wake up the signaled thread:
				 */
				PTHREAD_NEW_STATE(pthread, PS_RUNNING);
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
		_SPINUNLOCK(&(*cond)->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}

	/* Return the completion status: */
	return (rval);
}

void
_cond_wait_backout(pthread_t pthread)
{
	pthread_cond_t	cond;

	cond = pthread->data.cond;
	if (cond != NULL) {
		/*
		 * Defer signals to protect the scheduling queues
		 * from access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/* Lock the condition variable structure: */
		_SPINLOCK(&cond->lock);

		/* Process according to condition variable type: */
		switch (cond->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			cond_queue_remove(cond, pthread);

			/* Check for no more waiters: */
			if (TAILQ_FIRST(&cond->c_queue) == NULL)
				cond->c_mutex = NULL;
			break;

		default:
			break;
		}

		/* Unlock the condition variable structure: */
		_SPINUNLOCK(&cond->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}
}

/*
 * Dequeue a waiting thread from the head of a condition queue in
 * descending priority order.
 */
static inline pthread_t
cond_queue_deq(pthread_cond_t cond)
{
	pthread_t pthread;

	while ((pthread = TAILQ_FIRST(&cond->c_queue)) != NULL) {
		TAILQ_REMOVE(&cond->c_queue, pthread, sqe);
		pthread->flags &= ~PTHREAD_FLAGS_IN_CONDQ;
		if ((pthread->timeout == 0) && (pthread->interrupted == 0))
			/*
			 * Only exit the loop when we find a thread
			 * that hasn't timed out or been canceled;
			 * those threads are already running and don't
			 * need their run state changed.
			 */
			break;
	}

	return(pthread);
}

/*
 * Remove a waiting thread from a condition queue in descending priority
 * order.
 */
static inline void
cond_queue_remove(pthread_cond_t cond, pthread_t pthread)
{
	/*
	 * Because pthread_cond_timedwait() can timeout as well
	 * as be signaled by another thread, it is necessary to
	 * guard against removing the thread from the queue if
	 * it isn't in the queue.
	 */
	if (pthread->flags & PTHREAD_FLAGS_IN_CONDQ) {
		TAILQ_REMOVE(&cond->c_queue, pthread, sqe);
		pthread->flags &= ~PTHREAD_FLAGS_IN_CONDQ;
	}
}

/*
 * Enqueue a waiting thread to a condition queue in descending priority
 * order.
 */
static inline void
cond_queue_enq(pthread_cond_t cond, pthread_t pthread)
{
	pthread_t tid = TAILQ_LAST(&cond->c_queue, cond_head);

	PTHREAD_ASSERT_NOT_IN_SYNCQ(pthread);

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
	pthread->flags |= PTHREAD_FLAGS_IN_CONDQ;
	pthread->data.cond = cond;
}
