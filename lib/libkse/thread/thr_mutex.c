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
#include <string.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

static spinlock_t static_init_lock = _SPINLOCK_INITIALIZER;

int
pthread_mutex_init(pthread_mutex_t * mutex,
		   const pthread_mutexattr_t * mutex_attr)
{
	enum pthread_mutextype type;
	pthread_mutex_t	pmutex;
	int             ret = 0;

	if (mutex == NULL) {
		ret = EINVAL;
	} else {
		/* Check if default mutex attributes: */
		if (mutex_attr == NULL || *mutex_attr == NULL)
			/* Default to a fast mutex: */
			type = PTHREAD_MUTEX_DEFAULT;

		else if ((*mutex_attr)->m_type >= MUTEX_TYPE_MAX)
			/* Return an invalid argument error: */
			ret = EINVAL;
		else
			/* Use the requested mutex type: */
			type = (*mutex_attr)->m_type;

		/* Check no errors so far: */
		if (ret == 0) {
			if ((pmutex = (pthread_mutex_t)
			    malloc(sizeof(struct pthread_mutex))) == NULL)
				ret = ENOMEM;
			else {
				/* Reset the mutex flags: */
				pmutex->m_flags = 0;

				/* Process according to mutex type: */
				switch (type) {
				/* Fast mutex: */
				case PTHREAD_MUTEX_DEFAULT:
				case PTHREAD_MUTEX_NORMAL:
				case PTHREAD_MUTEX_ERRORCHECK:
					/* Nothing to do here. */
					break;

				/* Counting mutex: */
				case PTHREAD_MUTEX_RECURSIVE:
					/* Reset the mutex count: */
					pmutex->m_data.m_count = 0;
					break;

				/* Trap invalid mutex types: */
				default:
					/* Return an invalid argument error: */
					ret = EINVAL;
					break;
				}
				if (ret == 0) {
					/* Initialise the rest of the mutex: */
					_thread_queue_init(&pmutex->m_queue);
					pmutex->m_flags |= MUTEX_FLAGS_INITED;
					pmutex->m_owner = NULL;
					pmutex->m_type = type;
					memset(&pmutex->lock, 0,
					    sizeof(pmutex->lock));
					*mutex = pmutex;
				} else {
					free(pmutex);
					*mutex = NULL;
				}
			}
		}
	}
	/* Return the completion status: */
	return (ret);
}

int
pthread_mutex_destroy(pthread_mutex_t * mutex)
{
	int ret = 0;

	if (mutex == NULL || *mutex == NULL)
		ret = EINVAL;
	else {
		/* Lock the mutex structure: */
		_SPINLOCK(&(*mutex)->lock);

		/*
		 * Free the memory allocated for the mutex
		 * structure:
		 */
		free(*mutex);

		/*
		 * Leave the caller's pointer NULL now that
		 * the mutex has been destroyed:
		 */
		*mutex = NULL;
	}

	/* Return the completion status: */
	return (ret);
}

static int
init_static (pthread_mutex_t *mutex)
{
	int ret;

	_SPINLOCK(&static_init_lock);

	if (*mutex == NULL)
		ret = pthread_mutex_init(mutex, NULL);
	else
		ret = 0;

	_SPINUNLOCK(&static_init_lock);

	return(ret);
}

int
pthread_mutex_trylock(pthread_mutex_t * mutex)
{
	int             ret = 0;

	if (mutex == NULL)
		ret = EINVAL;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	else if (*mutex != NULL || (ret = init_static(mutex)) == 0) {
		/* Lock the mutex structure: */
		_SPINLOCK(&(*mutex)->lock);

		/* Process according to mutex type: */
		switch ((*mutex)->m_type) {
		/* Fast mutex: */
		case PTHREAD_MUTEX_NORMAL:
		case PTHREAD_MUTEX_DEFAULT:
		case PTHREAD_MUTEX_ERRORCHECK:
			/* Check if this mutex is not locked: */
			if ((*mutex)->m_owner == NULL) {
				/* Lock the mutex for the running thread: */
				(*mutex)->m_owner = _thread_run;
			} else {
				/* Return a busy error: */
				ret = EBUSY;
			}
			break;

		/* Counting mutex: */
		case PTHREAD_MUTEX_RECURSIVE:
			/* Check if this mutex is locked: */
			if ((*mutex)->m_owner != NULL) {
				/*
				 * Check if the mutex is locked by the running
				 * thread: 
				 */
				if ((*mutex)->m_owner == _thread_run) {
					/* Increment the lock count: */
					(*mutex)->m_data.m_count++;
				} else {
					/* Return a busy error: */
					ret = EBUSY;
				}
			} else {
				/* Lock the mutex for the running thread: */
				(*mutex)->m_owner = _thread_run;
			}
			break;

		/* Trap invalid mutex types: */
		default:
			/* Return an invalid argument error: */
			ret = EINVAL;
			break;
		}

		/* Unlock the mutex structure: */
		_SPINUNLOCK(&(*mutex)->lock);
	}

	/* Return the completion status: */
	return (ret);
}

int
pthread_mutex_lock(pthread_mutex_t * mutex)
{
	int             ret = 0;

	if (mutex == NULL)
		ret = EINVAL;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	else if (*mutex != NULL || (ret = init_static(mutex)) == 0) {
		/* Lock the mutex structure: */
		_SPINLOCK(&(*mutex)->lock);

		/* Process according to mutex type: */
		switch ((*mutex)->m_type) {
		/* What SS2 define as a 'normal' mutex.  This has to deadlock
		   on attempts to get a lock you already own. */
		case PTHREAD_MUTEX_NORMAL:
			if ((*mutex)->m_owner == _thread_run) {
				/* Intetionally deadlock */
				for (;;)
					_thread_kern_sched_state(PS_MUTEX_WAIT, __FILE__, __LINE__);
			}
			goto COMMON_LOCK;
			
		 /* Return error (not OK) on attempting to re-lock */
		case PTHREAD_MUTEX_ERRORCHECK:
			if ((*mutex)->m_owner == _thread_run) {
				ret = EDEADLK;
				break;
			}
			
		/* Fast mutexes do not check for any error conditions: */
		case PTHREAD_MUTEX_DEFAULT:
		COMMON_LOCK:
			/*
			 * Enter a loop to wait for the mutex to be locked by the
			 * current thread: 
			 */
			while ((*mutex)->m_owner != _thread_run) {
				/* Check if the mutex is not locked: */
				if ((*mutex)->m_owner == NULL) {
					/* Lock the mutex for this thread: */
					(*mutex)->m_owner = _thread_run;
				} else {
					/*
					 * Join the queue of threads waiting to lock
					 * the mutex: 
					 */
					_thread_queue_enq(&(*mutex)->m_queue, _thread_run);

					/* Wait for the mutex: */
					_thread_kern_sched_state_unlock(
					    PS_MUTEX_WAIT, &(*mutex)->lock,
					    __FILE__, __LINE__);

					/* Lock the mutex again: */
					_SPINLOCK(&(*mutex)->lock);
				}
			}
			break;

		/* Counting mutex: */
		case PTHREAD_MUTEX_RECURSIVE:
			/*
			 * Enter a loop to wait for the mutex to be locked by the
			 * current thread: 
			 */
			while ((*mutex)->m_owner != _thread_run) {
				/* Check if the mutex is not locked: */
				if ((*mutex)->m_owner == NULL) {
					/* Lock the mutex for this thread: */
					(*mutex)->m_owner = _thread_run;

					/* Reset the lock count for this mutex: */
					(*mutex)->m_data.m_count = 0;
				} else {
					/*
					 * Join the queue of threads waiting to lock
					 * the mutex: 
					 */
					_thread_queue_enq(&(*mutex)->m_queue, _thread_run);

					/* Wait for the mutex: */
					_thread_kern_sched_state_unlock(
					    PS_MUTEX_WAIT, &(*mutex)->lock,
					    __FILE__, __LINE__);

					/* Lock the mutex again: */
					_SPINLOCK(&(*mutex)->lock);
				}
			}

			/* Increment the lock count for this mutex: */
			(*mutex)->m_data.m_count++;
			break;

		/* Trap invalid mutex types: */
		default:
			/* Return an invalid argument error: */
			ret = EINVAL;
			break;
		}

		/* Unlock the mutex structure: */
		_SPINUNLOCK(&(*mutex)->lock);
	}

	/* Return the completion status: */
	return (ret);
}

int
pthread_mutex_unlock(pthread_mutex_t * mutex)
{
	int             ret = 0;

	if (mutex == NULL || *mutex == NULL) {
		ret = EINVAL;
	} else {
		/* Lock the mutex structure: */
		_SPINLOCK(&(*mutex)->lock);

		/* Process according to mutex type: */
		switch ((*mutex)->m_type) {
		/* Default & normal mutexes do not really need to check for
		   any error conditions: */
		case PTHREAD_MUTEX_NORMAL:
		case PTHREAD_MUTEX_DEFAULT:
		case PTHREAD_MUTEX_ERRORCHECK:
			/* Check if the running thread is not the owner of the mutex: */
			if ((*mutex)->m_owner != _thread_run) {
				/* Return an invalid argument error: */
				ret = (*mutex)->m_owner ? EPERM : EINVAL;
			}
			/*
			 * Get the next thread from the queue of threads waiting on
			 * the mutex: 
			 */
			else if (((*mutex)->m_owner = _thread_queue_deq(&(*mutex)->m_queue)) != NULL) {
				/* Allow the new owner of the mutex to run: */
				PTHREAD_NEW_STATE((*mutex)->m_owner,PS_RUNNING);
			}
			break;

		/* Counting mutex: */
		case PTHREAD_MUTEX_RECURSIVE:
			/* Check if the running thread is not the owner of the mutex: */
			if ((*mutex)->m_owner != _thread_run) {
				/* Return an invalid argument error: */
				ret = EINVAL;
			}
			/* Check if there are still counts: */
			else if ((*mutex)->m_data.m_count > 1) {
				/* Decrement the count: */
				(*mutex)->m_data.m_count--;
			} else {
				(*mutex)->m_data.m_count = 0;
				/*
				 * Get the next thread from the queue of threads waiting on
				 * the mutex: 
				 */
				if (((*mutex)->m_owner = _thread_queue_deq(&(*mutex)->m_queue)) != NULL) {
					/* Allow the new owner of the mutex to run: */
					PTHREAD_NEW_STATE((*mutex)->m_owner,PS_RUNNING);
				}
			}
			break;

		/* Trap invalid mutex types: */
		default:
			/* Return an invalid argument error: */
			ret = EINVAL;
			break;
		}

		/* Unlock the mutex structure: */
		_SPINUNLOCK(&(*mutex)->lock);
	}

	/* Return the completion status: */
	return (ret);
}
#endif
