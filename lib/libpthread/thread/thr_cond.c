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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
pthread_cond_init(pthread_cond_t * cond, const pthread_condattr_t * cond_attr)
{
	enum pthread_cond_type type;
	pthread_cond_t	pcond;
	int             rval = 0;

	if (cond == NULL) {
		rval = EINVAL;
	} else {
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
				_thread_queue_init(&pcond->c_queue);
				pcond->c_flags |= COND_FLAGS_INITED;
				pcond->c_type = type;
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

	if (cond == NULL || *cond == NULL) {
		rval = EINVAL;
	} else {
		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
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

		/* Check for errors: */
		if (rval == 0) {
			/* Destroy the contents of the condition structure: */
			_thread_queue_init(&(*cond)->c_queue);
			(*cond)->c_flags = 0;
			free(*cond);
			*cond = NULL;
		}
	}
	/* Return the completion status: */
	return (rval);
}

int
pthread_cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex)
{
	int             rval = 0;
	int             status;

	if (cond == NULL || *cond == NULL) {
		rval = EINVAL;
	} else {
		/* Block signals: */
		_thread_kern_sig_block(&status);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/*
			 * Queue the running thread for the condition
			 * variable:
			 */
			_thread_queue_enq(&(*cond)->c_queue, _thread_run);

			/* Unlock the mutex: */
			pthread_mutex_unlock(mutex);

			/* Wait forever: */
			_thread_run->wakeup_time.tv_sec = -1;

			/* Schedule the next thread: */
			_thread_kern_sched_state(PS_COND_WAIT,
			    __FILE__, __LINE__);

			/* Block signals: */
			_thread_kern_sig_block(NULL);

			/* Lock the mutex: */
			rval = pthread_mutex_lock(mutex);
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		/* Unblock signals: */
		_thread_kern_sig_unblock(status);
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

	if (cond == NULL || *cond == NULL) {
		rval = EINVAL;
	} else {
		/* Block signals: */
		_thread_kern_sig_block(&status);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/* Set the wakeup time: */
			_thread_run->wakeup_time.tv_sec = abstime->tv_sec;
			_thread_run->wakeup_time.tv_nsec = abstime->tv_nsec;

			/* Reset the timeout flag: */
			_thread_run->timeout = 0;

			/*
			 * Queue the running thread for the condition
			 * variable:
			 */
			_thread_queue_enq(&(*cond)->c_queue, _thread_run);

			/* Unlock the mutex: */
			if ((rval = pthread_mutex_unlock(mutex)) != 0) {
				/*
				 * Cannot unlock the mutex, so remove the
				 * running thread from the condition
				 * variable queue: 
				 */
				_thread_queue_deq(&(*cond)->c_queue);
			} else {
				/* Schedule the next thread: */
				_thread_kern_sched_state(PS_COND_WAIT,
				    __FILE__, __LINE__);

				/* Block signals: */
				_thread_kern_sig_block(NULL);

				/* Lock the mutex: */
				if ((rval = pthread_mutex_lock(mutex)) != 0) {
				}
				/* Check if the wait timed out: */
				else if (_thread_run->timeout) {
					/* Return a timeout error: */
					rval = ETIMEDOUT;
				}
			}
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		/* Unblock signals: */
		_thread_kern_sig_unblock(status);
	}

	/* Return the completion status: */
	return (rval);
}

int
pthread_cond_signal(pthread_cond_t * cond)
{
	int             rval = 0;
	int             status;
	pthread_t       pthread;

	if (cond == NULL || *cond == NULL) {
		rval = EINVAL;
	} else {
		/* Block signals: */
		_thread_kern_sig_block(&status);

		/* Process according to condition variable type: */
		switch ((*cond)->c_type) {
		/* Fast condition variable: */
		case COND_TYPE_FAST:
			/* Bring the next thread off the condition queue: */
			if ((pthread = _thread_queue_deq(&(*cond)->c_queue)) != NULL) {
				/* Allow the thread to run: */
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
			}
			break;

		/* Trap invalid condition variable types: */
		default:
			/* Return an invalid argument error: */
			rval = EINVAL;
			break;
		}

		/* Unblock signals: */
		_thread_kern_sig_unblock(status);
	}

	/* Return the completion status: */
	return (rval);
}

int
pthread_cond_broadcast(pthread_cond_t * cond)
{
	int             rval = 0;
	int             status;
	pthread_t       pthread;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	/* Process according to condition variable type: */
	switch ((*cond)->c_type) {
	/* Fast condition variable: */
	case COND_TYPE_FAST:
		/*
		 * Enter a loop to bring all threads off the
		 * condition queue:
		 */
		while ((pthread =
		    _thread_queue_deq(&(*cond)->c_queue)) != NULL) {
			/* Allow the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);
		}
		break;

	/* Trap invalid condition variable types: */
	default:
		/* Return an invalid argument error: */
		rval = EINVAL;
		break;
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);

	/* Return the completion status: */
	return (rval);
}
#endif
