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
 */
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/*
 * Prototypes
 */
static inline pthread_t	cond_queue_deq(pthread_cond_t);
static inline void	cond_queue_remove(pthread_cond_t, pthread_t);
static inline void	cond_queue_enq(pthread_cond_t, pthread_t);


int
pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t * cond_attr)
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
				memset(&pcond->lock,0,sizeof(pcond->lock));
				*cond = pcond;
			}
		}
	}
	/* Return the completion status: */
	return (rval);
}

int
pthread_cond_destroy(pthread_cond_t * cond)
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
pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex)
{
	int             rval = 0;
	int             status;

	if (cond == NULL)
		rval = EINVAL;

	/*
	 * If the condition variable is statically initialized,
	 * perform the dynamic initialization:
	 */
	else if (*cond != NULL ||
	    (rval = pthread_cond_init(cond,NULL)) == 0) {
		/* Lock the condition variable structure: */
		_SPINLOCK(&(*cond)->lock);

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
				/* Reset the timeout flag: */
				_thread_run->timeout = 0;

				/*
				 * Queue the running thread for the condition
				 * variable:
				 */
				cond_queue_enq(*cond, _thread_run);

				/* Remember the mutex that is being used: */
				(*cond)->c_mutex = *mutex;

				/* Wait forever: */
				_thread_run->wakeup_time.tv_sec = -1;

				/* Unlock the mutex: */
				if ((rval = _mutex_cv_unlock(mutex)) != 0) {
					/*
					 * Cannot unlock the mutex, so remove
					 * the running thread from the condition
					 * variable queue:
					 */
					cond_queue_remove(*cond, _thread_run);

					/* Check for no more waiters: */
					if (TAILQ_FIRST(&(*cond)->c_queue) ==
					    NULL)
						(*cond)->c_mutex = NULL;

					/* Unlock the condition variable structure: */
					_SPINUNLOCK(&(*cond)->lock);
				}
				else {
					/*
					 * Schedule the next thread and unlock
					 * the condition variable structure:
					 */
					_thread_kern_sched_state_unlock(PS_COND_WAIT,
				    	    &(*cond)->lock, __FILE__, __LINE__);

					/* Lock the mutex: */
					rval = _mutex_cv_lock(mutex);
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
	}

	/* Return the completion status: */
	return (rval);
}

int
pthread_cond_timedwait(pthread_cond_t * cond, pthread_mutex_t * mutex,
		       const struct timespec * abstime)
{
	int             rval = 0;
	int             status;

	if (cond == NULL)
		rval = EINVAL;

	/*
	 * If the condition variable is statically initialized,
	 * perform the dynamic initialization:
	 */
	else if (*cond != NULL ||
	    (rval = pthread_cond_init(cond,NULL)) == 0) {
		/* Lock the condition variable structure: */
		_SPINLOCK(&(*cond)->lock);

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
				_thread_run->wakeup_time.tv_sec =
				    abstime->tv_sec;
				_thread_run->wakeup_time.tv_nsec =
				    abstime->tv_nsec;

				/* Reset the timeout flag: */
				_thread_run->timeout = 0;

				/*
				 * Queue the running thread for the condition
				 * variable:
				 */
				cond_queue_enq(*cond, _thread_run);

				/* Remember the mutex that is being used: */
				(*cond)->c_mutex = *mutex;

				/* Unlock the mutex: */
				if ((rval = _mutex_cv_unlock(mutex)) != 0) {
					/*
					 * Cannot unlock the mutex, so remove
					 * the running thread from the condition
					 * variable queue: 
					 */
					cond_queue_remove(*cond, _thread_run);

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

					/* Check if the wait timedout: */
					if (_thread_run->timeout == 0) {
						/* Lock the mutex: */
						rval = _mutex_cv_lock(mutex);
					}
					else {
						/* Lock the condition variable structure: */
						_SPINLOCK(&(*cond)->lock);

						/*
						 * The wait timed out; remove
						 * the thread from the condition
					 	 * variable queue:
						 */
						cond_queue_remove(*cond,
						    _thread_run);

						/* Check for no more waiters: */
						if (TAILQ_FIRST(&(*cond)->c_queue) == NULL)
							(*cond)->c_mutex = NULL;

						/* Unock the condition variable structure: */
						_SPINUNLOCK(&(*cond)->lock);

						/* Return a timeout error: */
						rval = ETIMEDOUT;

						/*
						 * Lock the mutex and ignore
						 * any errors:
						 */
						(void)_mutex_cv_lock(mutex);
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

	}

	/* Return the completion status: */
	return (rval);
}

int
pthread_cond_signal(pthread_cond_t * cond)
{
	int             rval = 0;
	pthread_t       pthread;

	if (cond == NULL || *cond == NULL)
		rval = EINVAL;
	else {
		/* Lock the condition variable structure: */
		_SPINLOCK(&(*cond)->lock);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/*
			 * Enter a loop to dequeue threads from the condition
			 * queue until we find one that hasn't previously
			 * timed out.
			 */
			while (((pthread = cond_queue_deq(*cond)) != NULL) &&
			    (pthread->timeout != 0)) {
			}

			if (pthread != NULL)
				/* Allow the thread to run: */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);

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
	}

	/* Return the completion status: */
	return (rval);
}

int
pthread_cond_broadcast(pthread_cond_t * cond)
{
	int             rval = 0;
	pthread_t       pthread;

	if (cond == NULL || *cond == NULL)
		rval = EINVAL;
	else {
		/*
		 * Guard against preemption by a scheduling signal.
		 * A change of thread state modifies the waiting
		 * and priority queues.  In addition, we must assure
		 * that all threads currently waiting on the condition
		 * variable are signaled and are not timedout by a
		 * scheduling signal that causes a preemption.
		 */
		_thread_kern_sched_defer();

		/* Lock the condition variable structure: */
		_SPINLOCK(&(*cond)->lock);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/*
			 * Enter a loop to bring all threads off the
			 * condition queue:
			 */
			while ((pthread = cond_queue_deq(*cond)) != NULL) {
				/*
				 * The thread is already running if the
				 * timeout flag is set.
				 */
				if (pthread->timeout == 0)
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
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

		/* Reenable preemption and yield if necessary.
		 */
		_thread_kern_sched_undefer();
	}

	/* Return the completion status: */
	return (rval);
}

/*
 * Dequeue a waiting thread from the head of a condition queue in
 * descending priority order.
 */
static inline pthread_t
cond_queue_deq(pthread_cond_t cond)
{
	pthread_t pthread;

	if ((pthread = TAILQ_FIRST(&cond->c_queue)) != NULL) {
		TAILQ_REMOVE(&cond->c_queue, pthread, qe);
		pthread->flags &= ~PTHREAD_FLAGS_QUEUED;
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
	if (pthread->flags & PTHREAD_FLAGS_QUEUED) {
		TAILQ_REMOVE(&cond->c_queue, pthread, qe);
		pthread->flags &= ~PTHREAD_FLAGS_QUEUED;
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

	/*
	 * For the common case of all threads having equal priority,
	 * we perform a quick check against the priority of the thread
	 * at the tail of the queue.
	 */
	if ((tid == NULL) || (pthread->active_priority <= tid->active_priority))
		TAILQ_INSERT_TAIL(&cond->c_queue, pthread, qe);
	else {
		tid = TAILQ_FIRST(&cond->c_queue);
		while (pthread->active_priority <= tid->active_priority)
			tid = TAILQ_NEXT(tid, qe);
		TAILQ_INSERT_BEFORE(tid, pthread, qe);
	}
	pthread->flags |= PTHREAD_FLAGS_QUEUED;
}
#endif
