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
#include <sys/param.h>
#include <sys/queue.h>
#include <pthread.h>
#include "thr_private.h"

#if defined(_PTHREADS_INVARIANTS)
#define _MUTEX_INIT_LINK(m) 		do {		\
	(m)->m_qe.tqe_prev = NULL;			\
	(m)->m_qe.tqe_next = NULL;			\
} while (0)
#define _MUTEX_ASSERT_IS_OWNED(m)	do {		\
	if ((m)->m_qe.tqe_prev == NULL)			\
		PANIC("mutex is not on list");		\
} while (0)
#define _MUTEX_ASSERT_NOT_OWNED(m)	do {		\
	if (((m)->m_qe.tqe_prev != NULL) ||		\
	    ((m)->m_qe.tqe_next != NULL))		\
		PANIC("mutex is on list");		\
} while (0)
#else
#define _MUTEX_INIT_LINK(m)
#define _MUTEX_ASSERT_IS_OWNED(m)
#define _MUTEX_ASSERT_NOT_OWNED(m)
#endif

/*
 * Prototypes
 */
static int		get_muncontested(pthread_mutex_t, int);
static void		get_mcontested(pthread_mutex_t);
static inline int	mutex_self_trylock(pthread_mutex_t);
static inline int	mutex_self_lock(pthread_mutex_t);
static inline int	mutex_unlock_common(pthread_mutex_t *, int);
static void		mutex_priority_adjust(pthread_mutex_t);
static void		mutex_rescan_owned (pthread_t, pthread_mutex_t);
static inline pthread_t	mutex_queue_deq(pthread_mutex_t);
static inline void	mutex_queue_remove(pthread_mutex_t, pthread_t);
static inline void	mutex_queue_enq(pthread_mutex_t, pthread_t);


static spinlock_t static_init_lock = _SPINLOCK_INITIALIZER;

static struct pthread_mutex_attr	static_mutex_attr =
    PTHREAD_MUTEXATTR_STATIC_INITIALIZER;
static pthread_mutexattr_t		static_mattr = &static_mutex_attr;

/* Single underscore versions provided for libc internal usage: */
__weak_reference(__pthread_mutex_trylock, pthread_mutex_trylock);
__weak_reference(__pthread_mutex_lock, pthread_mutex_lock);

/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_init, pthread_mutex_init);
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_unlock, pthread_mutex_unlock);


/*
 * Reinitialize a private mutex; this is only used for internal mutexes.
 */
int
_mutex_reinit(pthread_mutex_t * mutex)
{
	int	ret = 0;

	if (mutex == NULL)
		ret = EINVAL;
	else if (*mutex == NULL)
		ret = _pthread_mutex_init(mutex, NULL);
	else {
		/*
		 * Initialize the mutex structure:
		 */
		(*mutex)->m_type = PTHREAD_MUTEX_DEFAULT;
		(*mutex)->m_protocol = PTHREAD_PRIO_NONE;
		TAILQ_INIT(&(*mutex)->m_queue);
		(*mutex)->m_owner = NULL;
		(*mutex)->m_data.m_count = 0;
		(*mutex)->m_flags |= MUTEX_FLAGS_INITED | MUTEX_FLAGS_PRIVATE;
		(*mutex)->m_refcount = 0;
		(*mutex)->m_prio = 0;
		(*mutex)->m_saved_prio = 0;
		_MUTEX_INIT_LINK(*mutex);
		memset(&(*mutex)->lock, 0, sizeof((*mutex)->lock));
	}
	return (ret);
}

int
_pthread_mutex_init(pthread_mutex_t * mutex,
		   const pthread_mutexattr_t * mutex_attr)
{
	enum pthread_mutextype	type;
	int		protocol;
	int		ceiling;
	int		flags;
	pthread_mutex_t	pmutex;
	int		ret = 0;

	if (mutex == NULL)
		ret = EINVAL;

	/* Check if default mutex attributes: */
	if (mutex_attr == NULL || *mutex_attr == NULL) {
		/* Default to a (error checking) POSIX mutex: */
		type = PTHREAD_MUTEX_ERRORCHECK;
		protocol = PTHREAD_PRIO_NONE;
		ceiling = PTHREAD_MAX_PRIORITY;
		flags = 0;
	}

	/* Check mutex type: */
	else if (((*mutex_attr)->m_type < PTHREAD_MUTEX_ERRORCHECK) ||
	    ((*mutex_attr)->m_type >= MUTEX_TYPE_MAX))
		/* Return an invalid argument error: */
		ret = EINVAL;

	/* Check mutex protocol: */
	else if (((*mutex_attr)->m_protocol < PTHREAD_PRIO_NONE) ||
	    ((*mutex_attr)->m_protocol > PTHREAD_MUTEX_RECURSIVE))
		/* Return an invalid argument error: */
		ret = EINVAL;

	else {
		/* Use the requested mutex type and protocol: */
		type = (*mutex_attr)->m_type;
		protocol = (*mutex_attr)->m_protocol;
		ceiling = (*mutex_attr)->m_ceiling;
		flags = (*mutex_attr)->m_flags;
	}

	/* Check no errors so far: */
	if (ret == 0) {
		if ((pmutex = (pthread_mutex_t)
		    malloc(sizeof(struct pthread_mutex))) == NULL)
			ret = ENOMEM;
		else {
			/* Set the mutex flags: */
			pmutex->m_flags = flags;

			/* Process according to mutex type: */
			switch (type) {
			/* case PTHREAD_MUTEX_DEFAULT: */
			case PTHREAD_MUTEX_ERRORCHECK:
			case PTHREAD_MUTEX_NORMAL:
				/* Nothing to do here. */
				break;

			/* Single UNIX Spec 2 recursive mutex: */
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
				TAILQ_INIT(&pmutex->m_queue);
				pmutex->m_flags |= MUTEX_FLAGS_INITED;
				pmutex->m_owner = NULL;
				pmutex->m_type = type;
				pmutex->m_protocol = protocol;
				pmutex->m_refcount = 0;
				if (protocol == PTHREAD_PRIO_PROTECT)
					pmutex->m_prio = ceiling;
				else
					pmutex->m_prio = 0;
				pmutex->m_saved_prio = 0;
				_MUTEX_INIT_LINK(pmutex);
				memset(&pmutex->lock, 0, sizeof(pmutex->lock));
				*mutex = pmutex;
			} else {
				free(pmutex);
				*mutex = NULL;
			}
		}
	}
	/* Return the completion status: */
	return (ret);
}

int
_pthread_mutex_destroy(pthread_mutex_t * mutex)
{
	int	ret = 0;

	if (mutex == NULL || *mutex == NULL)
		ret = EINVAL;
	else {
		/* Lock the mutex structure: */
		_SPINLOCK(&(*mutex)->lock);

		/*
		 * Check to see if this mutex is in use:
		 */
		if (((*mutex)->m_owner != NULL) ||
		    (TAILQ_FIRST(&(*mutex)->m_queue) != NULL) ||
		    ((*mutex)->m_refcount != 0)) {
			ret = EBUSY;

			/* Unlock the mutex structure: */
			_SPINUNLOCK(&(*mutex)->lock);
		}
		else {
			/*
			 * Free the memory allocated for the mutex
			 * structure:
			 */
			_MUTEX_ASSERT_NOT_OWNED(*mutex);

			/* Unlock the mutex structure: */
			_SPINUNLOCK(&(*mutex)->lock);

			free(*mutex);

			/*
			 * Leave the caller's pointer NULL now that
			 * the mutex has been destroyed:
			 */
			*mutex = NULL;
		}
	}

	/* Return the completion status: */
	return (ret);
}

static int
init_static(pthread_mutex_t *mutex)
{
	int	ret;

	_SPINLOCK(&static_init_lock);

	if (*mutex == NULL)
		ret = _pthread_mutex_init(mutex, NULL);
	else
		ret = 0;

	_SPINUNLOCK(&static_init_lock);

	return (ret);
}

static int
init_static_private(pthread_mutex_t *mutex)
{
	int	ret;

	_SPINLOCK(&static_init_lock);

	if (*mutex == NULL)
		ret = _pthread_mutex_init(mutex, &static_mattr);
	else
		ret = 0;

	_SPINUNLOCK(&static_init_lock);

	return (ret);
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	if (mutex == NULL)
		ret = EINVAL;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	else if ((*mutex != NULL) || (ret = init_static(mutex)) == 0)
		ret = mutex_lock_common(mutex, 1);

	return (ret);
}

int
_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	if (mutex == NULL)
		ret = EINVAL;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking the mutex private (delete safe):
	 */
	else if ((*mutex != NULL) || (ret = init_static_private(mutex)) == 0)
		ret = mutex_lock_common(mutex, 1);

	return (ret);
}

static int
mutex_lock_common(pthread_mutex_t * mutex, int nonblock)
{
	int ret, error, inCancel;

	ret = error = inCancel = 0;

	PTHREAD_ASSERT((mutex != NULL) && (*mutex != NULL),
	    "Uninitialized mutex in mutex_lock_common");

	/*
	 * Enter a loop waiting to become the mutex owner.  We need a
	 * loop in case the waiting thread is interrupted by a signal
	 * to execute a signal handler.  It is not (currently) possible
	 * to remain in the waiting queue while running a handler.
	 * Instead, the thread is interrupted and backed out of the
	 * waiting queue prior to executing the signal handler.
	 */
	do {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		/* _thread_kern_sig_defer(); */

		/* Lock the mutex structure: */
		_SPINLOCK(&(*mutex)->lock);

		/*
		 * If the mutex was statically allocated, properly
		 * initialize the tail queue.
		 */
		if (((*mutex)->m_flags & MUTEX_FLAGS_INITED) == 0) {
			TAILQ_INIT(&(*mutex)->m_queue);
			(*mutex)->m_flags |= MUTEX_FLAGS_INITED;
			_MUTEX_INIT_LINK(*mutex);
		}

		/* Process according to mutex type: */
		switch ((*mutex)->m_protocol) {
		/* Default POSIX mutex: */
		case PTHREAD_PRIO_NONE:
			if ((error = get_muncontested(*mutex, nonblock)) == -1)
				if (nonblock) {
					ret = EBUSY;
					break;
				} else {
					get_mcontested(*mutex);
				}
			else
				ret = error;
			break;

		/* POSIX priority inheritence mutex: */
		case PTHREAD_PRIO_INHERIT:
			if ((error = get_muncontested(*mutex, nonblock)) == 0) {
				/* Track number of priority mutexes owned: */
				curthread->priority_mutex_count++;

				/*
				 * The mutex takes on attributes of the
				 * running thread when there are no waiters.
				 */
				(*mutex)->m_prio = curthread->active_priority;
				(*mutex)->m_saved_prio =
				    curthread->inherited_priority;
				curthread->inherited_priority =
				    (*mutex)->m_prio;
			} else if (error == -1) {
				if (nonblock) {
					ret = EBUSY;
					break;
				} else {
					get_mcontested(*mutex);
				}

				if (curthread->active_priority >
				    (*mutex)->m_prio)
					/* Adjust priorities: */
					mutex_priority_adjust(*mutex);
			} else {
				ret = error;
			}
			break;

		/* POSIX priority protection mutex: */
		case PTHREAD_PRIO_PROTECT:
			/* Check for a priority ceiling violation: */
			if (curthread->active_priority > (*mutex)->m_prio)
				ret = EINVAL;

			if ((error = get_muncontested(*mutex, nonblock)) == 0) {
				/* Track number of priority mutexes owned: */
				curthread->priority_mutex_count++;

				/*
				 * The running thread inherits the ceiling
				 * priority of the mutex and executes at that
				 * priority:
				 */
				curthread->active_priority = (*mutex)->m_prio;
				(*mutex)->m_saved_prio =
				    curthread->inherited_priority;
				curthread->inherited_priority =
				    (*mutex)->m_prio;
			} else if (error == -1) {
				if (nonblock) {
					ret = EBUSY;
					break;
				}

				/* Clear any previous error: */
				curthread->error = 0;

				get_mcontested(*mutex);

				/*
				 * The threads priority may have changed while
				 * waiting for the mutex causing a ceiling
				 * violation.
				 */
				ret = curthread->error;
				curthread->error = 0;
			} else {
				ret = error;
			}
			break;

		/* Trap invalid mutex types: */
		default:
			/* Return an invalid argument error: */
			ret = EINVAL;
			break;
		}

		/*
		 * Check to see if this thread was interrupted and
		 * is still in the mutex queue of waiting threads:
		 */
		if (curthread->cancelflags & PTHREAD_CANCELLING) {
			if (!nonblock)
				mutex_queue_remove(*mutex, curthread);
			inCancel=1;
		}

		/* Unlock the mutex structure: */
		_SPINUNLOCK(&(*mutex)->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		/* _thread_kern_sig_undefer(); */
		if (inCancel) {
			pthread_testcancel();
			PANIC("Canceled thread came back.\n");
		}
	} while ((*mutex)->m_owner != curthread && ret == 0);

	/* Return the completion status: */
	return (ret);
}

int
__pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	if (_thread_initial == NULL)
		_thread_init();

	if (mutex == NULL)
		ret = EINVAL;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	else if ((*mutex != NULL) || ((ret = init_static(mutex)) == 0))
		ret = mutex_lock_common(mutex, 0);

	return (ret);
}

int
_pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	if (_thread_initial == NULL)
		_thread_init();

	if (mutex == NULL)
		ret = EINVAL;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	else if ((*mutex != NULL) || ((ret = init_static_private(mutex)) == 0))
		ret = mutex_lock_common(mutex, 0);

	return (ret);
}

int
_pthread_mutex_unlock(pthread_mutex_t * mutex)
{
	return (mutex_unlock_common(mutex, /* add reference */ 0));
}

int
_mutex_cv_unlock(pthread_mutex_t * mutex)
{
	return (mutex_unlock_common(mutex, /* add reference */ 1));
}

int
_mutex_cv_lock(pthread_mutex_t * mutex)
{
	int	ret;
	if ((ret = _pthread_mutex_lock(mutex)) == 0)
		(*mutex)->m_refcount--;
	return (ret);
}

static inline int
mutex_self_trylock(pthread_mutex_t mutex)
{
	int	ret = 0;

	switch (mutex->m_type) {

	/* case PTHREAD_MUTEX_DEFAULT: */
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_NORMAL:
		/*
		 * POSIX specifies that mutexes should return EDEADLK if a
		 * recursive lock is detected.
		 */
		ret = EBUSY; 
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		mutex->m_data.m_count++;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

static inline int
mutex_self_lock(pthread_mutex_t mutex)
{
	int ret = 0;

	switch (mutex->m_type) {
	/* case PTHREAD_MUTEX_DEFAULT: */
	case PTHREAD_MUTEX_ERRORCHECK:
		/*
		 * POSIX specifies that mutexes should return EDEADLK if a
		 * recursive lock is detected.
		 */
		ret = EDEADLK; 
		break;

	case PTHREAD_MUTEX_NORMAL:
		/*
		 * What SS2 define as a 'normal' mutex.  Intentionally
		 * deadlock on attempts to get a lock you already own.
		 */
		/* XXX Sched lock. */
		PTHREAD_SET_STATE(curthread, PS_DEADLOCK);
		_SPINUNLOCK(&(mutex)->lock);
		_thread_suspend(curthread, NULL);
		PANIC("Shouldn't resume here?\n");
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		mutex->m_data.m_count++;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

static inline int
mutex_unlock_common(pthread_mutex_t * mutex, int add_reference)
{
	int	ret = 0;

	if (mutex == NULL || *mutex == NULL) {
		ret = EINVAL;
	} else {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		/* _thread_kern_sig_defer(); */

		/* Lock the mutex structure: */
		_SPINLOCK(&(*mutex)->lock);

		/* Process according to mutex type: */
		switch ((*mutex)->m_protocol) {
		/* Default POSIX mutex: */
		case PTHREAD_PRIO_NONE:
			/*
			 * Check if the running thread is not the owner of the
			 * mutex:
			 */
			if ((*mutex)->m_owner != curthread) {
				/*
				 * Return an invalid argument error for no
				 * owner and a permission error otherwise:
				 */
				ret = (*mutex)->m_owner == NULL ? EINVAL : EPERM;
			}
			else if (((*mutex)->m_type == PTHREAD_MUTEX_RECURSIVE) &&
			    ((*mutex)->m_data.m_count > 0)) {
				/* Decrement the count: */
				(*mutex)->m_data.m_count--;
			} else {
				/*
				 * Clear the count in case this is recursive
				 * mutex.
				 */
				(*mutex)->m_data.m_count = 0;

				/* Remove the mutex from the threads queue. */
				_MUTEX_ASSERT_IS_OWNED(*mutex);
				TAILQ_REMOVE(&(*mutex)->m_owner->mutexq,
				    (*mutex), m_qe);
				_MUTEX_INIT_LINK(*mutex);

				/*
				 * Get the next thread from the queue of
				 * threads waiting on the mutex. The deq
				 * function will have already locked it
				 * for us.
				 */
				if (((*mutex)->m_owner =
			  	    mutex_queue_deq(*mutex)) != NULL) {
					/* Make the new owner runnable: */
					/* XXXTHR sched lock. */
					PTHREAD_NEW_STATE((*mutex)->m_owner,
					    PS_RUNNING);

					/*
					 * Add the mutex to the threads list of
					 * owned mutexes:
					 */
					TAILQ_INSERT_TAIL(&(*mutex)->m_owner->mutexq,
					    (*mutex), m_qe);

					/*
					 * The owner is no longer waiting for
					 * this mutex:
					 */
					(*mutex)->m_owner->data.mutex = NULL;
					_thread_critical_exit((*mutex)->m_owner);
				}
			}
			break;

		/* POSIX priority inheritence mutex: */
		case PTHREAD_PRIO_INHERIT:
			/*
			 * Check if the running thread is not the owner of the
			 * mutex:
			 */
			if ((*mutex)->m_owner != curthread) {
				/*
				 * Return an invalid argument error for no
				 * owner and a permission error otherwise:
				 */
				ret = (*mutex)->m_owner == NULL ? EINVAL : EPERM;
			}
			else if (((*mutex)->m_type == PTHREAD_MUTEX_RECURSIVE) &&
			    ((*mutex)->m_data.m_count > 0)) {
				/* Decrement the count: */
				(*mutex)->m_data.m_count--;
			} else {
				/*
				 * Clear the count in case this is recursive
				 * mutex.
				 */
				(*mutex)->m_data.m_count = 0;

				/*
				 * Restore the threads inherited priority and
				 * recompute the active priority (being careful
				 * not to override changes in the threads base
				 * priority subsequent to locking the mutex).
				 */
				curthread->inherited_priority =
					(*mutex)->m_saved_prio;
				curthread->active_priority =
				    MAX(curthread->inherited_priority,
				    curthread->base_priority);

				/*
				 * This thread now owns one less priority mutex.
				 */
				curthread->priority_mutex_count--;

				/* Remove the mutex from the threads queue. */
				_MUTEX_ASSERT_IS_OWNED(*mutex);
				TAILQ_REMOVE(&(*mutex)->m_owner->mutexq,
				    (*mutex), m_qe);
				_MUTEX_INIT_LINK(*mutex);

				/*
				 * Get the next thread from the queue of threads
				 * waiting on the mutex. It will already be
				 * locked for us. 
				 */
				if (((*mutex)->m_owner = 
				    mutex_queue_deq(*mutex)) == NULL)
					/* This mutex has no priority. */
					(*mutex)->m_prio = 0;
				else {
					/*
					 * Track number of priority mutexes owned:
					 */
					(*mutex)->m_owner->priority_mutex_count++;

					/*
					 * Add the mutex to the threads list
					 * of owned mutexes:
					 */
					TAILQ_INSERT_TAIL(&(*mutex)->m_owner->mutexq,
					    (*mutex), m_qe);

					/*
					 * The owner is no longer waiting for
					 * this mutex:
					 */
					(*mutex)->m_owner->data.mutex = NULL;

					/*
					 * Set the priority of the mutex.  Since
					 * our waiting threads are in descending
					 * priority order, the priority of the
					 * mutex becomes the active priority of
					 * the thread we just dequeued.
					 */
					(*mutex)->m_prio =
					    (*mutex)->m_owner->active_priority;

					/*
					 * Save the owning threads inherited
					 * priority:
					 */
					(*mutex)->m_saved_prio =
						(*mutex)->m_owner->inherited_priority;

					/*
					 * The owning threads inherited priority
					 * now becomes his active priority (the
					 * priority of the mutex).
					 */
					(*mutex)->m_owner->inherited_priority =
						(*mutex)->m_prio;

					/*
					 * Make the new owner runnable:
					 */
					/* XXXTHR sched lock. */
					PTHREAD_NEW_STATE((*mutex)->m_owner,
					    PS_RUNNING);

					_thread_critical_exit((*mutex)->m_owner);
				}
			}
			break;

		/* POSIX priority ceiling mutex: */
		case PTHREAD_PRIO_PROTECT:
			/*
			 * Check if the running thread is not the owner of the
			 * mutex:
			 */
			if ((*mutex)->m_owner != curthread) {
				/*
				 * Return an invalid argument error for no
				 * owner and a permission error otherwise:
				 */
				ret = (*mutex)->m_owner == NULL ? EINVAL : EPERM;
			}
			else if (((*mutex)->m_type == PTHREAD_MUTEX_RECURSIVE) &&
			    ((*mutex)->m_data.m_count > 0)) {
				/* Decrement the count: */
				(*mutex)->m_data.m_count--;
			} else {
				/*
				 * Clear the count in case this is recursive
				 * mutex.
				 */
				(*mutex)->m_data.m_count = 0;

				/*
				 * Restore the threads inherited priority and
				 * recompute the active priority (being careful
				 * not to override changes in the threads base
				 * priority subsequent to locking the mutex).
				 */
				curthread->inherited_priority =
					(*mutex)->m_saved_prio;
				curthread->active_priority =
				    MAX(curthread->inherited_priority,
				    curthread->base_priority);

				/*
				 * This thread now owns one less priority mutex.
				 */
				curthread->priority_mutex_count--;

				/* Remove the mutex from the threads queue. */
				_MUTEX_ASSERT_IS_OWNED(*mutex);
				TAILQ_REMOVE(&(*mutex)->m_owner->mutexq,
				    (*mutex), m_qe);
				_MUTEX_INIT_LINK(*mutex);

				/*
				 * Enter a loop to find a waiting thread whose
				 * active priority will not cause a ceiling
				 * violation. It will already be locked for us.
				 */
				while ((((*mutex)->m_owner =
				    mutex_queue_deq(*mutex)) != NULL) &&
				    ((*mutex)->m_owner->active_priority >
				     (*mutex)->m_prio)) {
					/*
					 * Either the mutex ceiling priority
					 * been lowered and/or this threads
					 * priority has been raised subsequent
					 * to this thread being queued on the
					 * waiting list.
					 */
					(*mutex)->m_owner->error = EINVAL;
					PTHREAD_NEW_STATE((*mutex)->m_owner,
					    PS_RUNNING);
					/*
					 * The thread is no longer waiting for
					 * this mutex:
					 */
					(*mutex)->m_owner->data.mutex = NULL;

					_thread_critical_exit((*mutex)->m_owner);
				}

				/* Check for a new owner: */
				if ((*mutex)->m_owner != NULL) {
					/*
					 * Track number of priority mutexes owned:
					 */
					(*mutex)->m_owner->priority_mutex_count++;

					/*
					 * Add the mutex to the threads list
					 * of owned mutexes:
					 */
					TAILQ_INSERT_TAIL(&(*mutex)->m_owner->mutexq,
					    (*mutex), m_qe);

					/*
					 * The owner is no longer waiting for
					 * this mutex:
					 */
					(*mutex)->m_owner->data.mutex = NULL;

					/*
					 * Save the owning threads inherited
					 * priority:
					 */
					(*mutex)->m_saved_prio =
						(*mutex)->m_owner->inherited_priority;

					/*
					 * The owning thread inherits the
					 * ceiling priority of the mutex and
					 * executes at that priority:
					 */
					(*mutex)->m_owner->inherited_priority =
					    (*mutex)->m_prio;
					(*mutex)->m_owner->active_priority =
					    (*mutex)->m_prio;

					/*
					 * Make the new owner runnable:
					 */
					/* XXXTHR sched lock. */
					PTHREAD_NEW_STATE((*mutex)->m_owner,
					    PS_RUNNING);

					_thread_critical_exit((*mutex)->m_owner);
				}
			}
			break;

		/* Trap invalid mutex types: */
		default:
			/* Return an invalid argument error: */
			ret = EINVAL;
			break;
		}

		if ((ret == 0) && (add_reference != 0)) {
			/* Increment the reference count: */
			(*mutex)->m_refcount++;
		}

		/* Unlock the mutex structure: */
		_SPINUNLOCK(&(*mutex)->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		/* _thread_kern_sig_undefer(); */
	}

	/* Return the completion status: */
	return (ret);
}


/*
 * This function is called when a change in base priority occurs for
 * a thread that is holding or waiting for a priority protection or
 * inheritence mutex.  A change in a threads base priority can effect
 * changes to active priorities of other threads and to the ordering
 * of mutex locking by waiting threads.
 *
 * This must be called while thread scheduling is deferred.
 */
void
_mutex_notify_priochange(pthread_t pthread)
{
	/* Adjust the priorites of any owned priority mutexes: */
	if (pthread->priority_mutex_count > 0) {
		/*
		 * Rescan the mutexes owned by this thread and correct
		 * their priorities to account for this threads change
		 * in priority.  This has the side effect of changing
		 * the threads active priority.
		 */
		mutex_rescan_owned(pthread, /* rescan all owned */ NULL);
	}

	/*
	 * If this thread is waiting on a priority inheritence mutex,
	 * check for priority adjustments.  A change in priority can
	 * also effect a ceiling violation(*) for a thread waiting on
	 * a priority protection mutex; we don't perform the check here
	 * as it is done in pthread_mutex_unlock.
	 *
	 * (*) It should be noted that a priority change to a thread
	 *     _after_ taking and owning a priority ceiling mutex
	 *     does not affect ownership of that mutex; the ceiling
	 *     priority is only checked before mutex ownership occurs.
	 */
	if (pthread->state == PS_MUTEX_WAIT) {
		/* Lock the mutex structure: */
		_SPINLOCK(&pthread->data.mutex->lock);

		/*
		 * Check to make sure this thread is still in the same state
		 * (the spinlock above can yield the CPU to another thread):
		 */
		if (pthread->state == PS_MUTEX_WAIT) {
			/*
			 * Remove and reinsert this thread into the list of
			 * waiting threads to preserve decreasing priority
			 * order.
			 */
			mutex_queue_remove(pthread->data.mutex, pthread);
			mutex_queue_enq(pthread->data.mutex, pthread);

			if (pthread->data.mutex->m_protocol ==
			     PTHREAD_PRIO_INHERIT) {
				/* Adjust priorities: */
				mutex_priority_adjust(pthread->data.mutex);
			}
		}

		/* Unlock the mutex structure: */
		_SPINUNLOCK(&pthread->data.mutex->lock);
	}
}

/*
 * Called when a new thread is added to the mutex waiting queue or
 * when a threads priority changes that is already in the mutex
 * waiting queue.
 */
static void
mutex_priority_adjust(pthread_mutex_t mutex)
{
	pthread_t	pthread_next, pthread = mutex->m_owner;
	int		temp_prio;
	pthread_mutex_t	m = mutex;

	/*
	 * Calculate the mutex priority as the maximum of the highest
	 * active priority of any waiting threads and the owning threads
	 * active priority(*).
	 *
	 * (*) Because the owning threads current active priority may
	 *     reflect priority inherited from this mutex (and the mutex
	 *     priority may have changed) we must recalculate the active
	 *     priority based on the threads saved inherited priority
	 *     and its base priority.
	 */
	pthread_next = TAILQ_FIRST(&m->m_queue);  /* should never be NULL */
	temp_prio = MAX(pthread_next->active_priority,
	    MAX(m->m_saved_prio, pthread->base_priority));

	/* See if this mutex really needs adjusting: */
	if (temp_prio == m->m_prio)
		/* No need to propagate the priority: */
		return;

	/* Set new priority of the mutex: */
	m->m_prio = temp_prio;

	while (m != NULL) {
		/*
		 * Save the threads priority before rescanning the
		 * owned mutexes:
		 */
		temp_prio = pthread->active_priority;

		/*
		 * Fix the priorities for all the mutexes this thread has
		 * locked since taking this mutex.  This also has a
		 * potential side-effect of changing the threads priority.
		 */
		mutex_rescan_owned(pthread, m);

		/*
		 * If the thread is currently waiting on a mutex, check
		 * to see if the threads new priority has affected the
		 * priority of the mutex.
		 */
		if ((temp_prio != pthread->active_priority) &&
		    (pthread->state == PS_MUTEX_WAIT) &&
		    (pthread->data.mutex->m_protocol == PTHREAD_PRIO_INHERIT)) {
			/* Grab the mutex this thread is waiting on: */
			m = pthread->data.mutex;

			/*
			 * The priority for this thread has changed.  Remove
			 * and reinsert this thread into the list of waiting
			 * threads to preserve decreasing priority order.
			 */
			mutex_queue_remove(m, pthread);
			mutex_queue_enq(m, pthread);

			/* Grab the waiting thread with highest priority: */
			pthread_next = TAILQ_FIRST(&m->m_queue);

			/*
			 * Calculate the mutex priority as the maximum of the
			 * highest active priority of any waiting threads and
			 * the owning threads active priority.
			 */
			temp_prio = MAX(pthread_next->active_priority,
			    MAX(m->m_saved_prio, m->m_owner->base_priority));

			if (temp_prio != m->m_prio) {
				/*
				 * The priority needs to be propagated to the
				 * mutex this thread is waiting on and up to
				 * the owner of that mutex.
				 */
				m->m_prio = temp_prio;
				pthread = m->m_owner;
			}
			else
				/* We're done: */
				m = NULL;

		}
		else
			/* We're done: */
			m = NULL;
	}
}

static void
mutex_rescan_owned(pthread_t pthread, pthread_mutex_t mutex)
{
	int		active_prio, inherited_prio;
	pthread_mutex_t	m;
	pthread_t	pthread_next;

	/*
	 * Start walking the mutexes the thread has taken since
	 * taking this mutex.
	 */
	if (mutex == NULL) {
		/*
		 * A null mutex means start at the beginning of the owned
		 * mutex list.
		 */
		m = TAILQ_FIRST(&pthread->mutexq);

		/* There is no inherited priority yet. */
		inherited_prio = 0;
	}
	else {
		/*
		 * The caller wants to start after a specific mutex.  It
		 * is assumed that this mutex is a priority inheritence
		 * mutex and that its priority has been correctly
		 * calculated.
		 */
		m = TAILQ_NEXT(mutex, m_qe);

		/* Start inheriting priority from the specified mutex. */
		inherited_prio = mutex->m_prio;
	}
	active_prio = MAX(inherited_prio, pthread->base_priority);

	while (m != NULL) {
		/*
		 * We only want to deal with priority inheritence
		 * mutexes.  This might be optimized by only placing
		 * priority inheritence mutexes into the owned mutex
		 * list, but it may prove to be useful having all
		 * owned mutexes in this list.  Consider a thread
		 * exiting while holding mutexes...
		 */
		if (m->m_protocol == PTHREAD_PRIO_INHERIT) {
			/*
			 * Fix the owners saved (inherited) priority to
			 * reflect the priority of the previous mutex.
			 */
			m->m_saved_prio = inherited_prio;

			if ((pthread_next = TAILQ_FIRST(&m->m_queue)) != NULL)
				/* Recalculate the priority of the mutex: */
				m->m_prio = MAX(active_prio,
				     pthread_next->active_priority);
			else
				m->m_prio = active_prio;

			/* Recalculate new inherited and active priorities: */
			inherited_prio = m->m_prio;
			active_prio = MAX(m->m_prio, pthread->base_priority);
		}

		/* Advance to the next mutex owned by this thread: */
		m = TAILQ_NEXT(m, m_qe);
	}

	/*
	 * Fix the threads inherited priority and recalculate its
	 * active priority.
	 */
	pthread->inherited_priority = inherited_prio;
	active_prio = MAX(inherited_prio, pthread->base_priority);

	if (active_prio != pthread->active_priority) {
#if 0
		/*
		 * If this thread is in the priority queue, it must be
		 * removed and reinserted for its new priority.
	 	 */
		if (pthread->flags & PTHREAD_FLAGS_IN_PRIOQ) {
			/*
			 * Remove the thread from the priority queue
			 * before changing its priority:
			 */
			PTHREAD_PRIOQ_REMOVE(pthread);

			/*
			 * POSIX states that if the priority is being
			 * lowered, the thread must be inserted at the
			 * head of the queue for its priority if it owns
			 * any priority protection or inheritence mutexes.
			 */
			if ((active_prio < pthread->active_priority) &&
			    (pthread->priority_mutex_count > 0)) {
				/* Set the new active priority. */
				pthread->active_priority = active_prio;

				PTHREAD_PRIOQ_INSERT_HEAD(pthread);
			}
			else {
				/* Set the new active priority. */
				pthread->active_priority = active_prio;

				PTHREAD_PRIOQ_INSERT_TAIL(pthread);
			}
		}
		else {
			/* Set the new active priority. */
			pthread->active_priority = active_prio;
		}
#endif
		pthread->active_priority = active_prio;
	}
}

void
_mutex_unlock_private(pthread_t pthread)
{
	struct pthread_mutex	*m, *m_next;

	for (m = TAILQ_FIRST(&pthread->mutexq); m != NULL; m = m_next) {
		m_next = TAILQ_NEXT(m, m_qe);
		if ((m->m_flags & MUTEX_FLAGS_PRIVATE) != 0)
			_pthread_mutex_unlock(&m);
	}
}

void
_mutex_lock_backout(pthread_t pthread)
{
	struct pthread_mutex	*mutex;

	/*
	 * Defer signals to protect the scheduling queues from
	 * access by the signal handler:
	 */
	/* _thread_kern_sig_defer();*/

	/* XXX - Necessary to obey lock order */
	_SPINLOCK(&pthread->lock);
	mutex = pthread->data.mutex;
	_SPINUNLOCK(&pthread->lock);

	_SPINLOCK(&mutex->lock);

	_thread_critical_enter(pthread);
	if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0) {

		mutex_queue_remove(mutex, pthread);

		/* This thread is no longer waiting for the mutex: */
		pthread->data.mutex = NULL;

	}
	/*
	 * Undefer and handle pending signals, yielding if
	 * necessary:
	 */
	/* _thread_kern_sig_undefer(); */

	_thread_critical_exit(pthread);
	_SPINUNLOCK(&mutex->lock);
}

/*
 * Dequeue a waiting thread from the head of a mutex queue in descending
 * priority order. This funtion will return with the thread locked.
 */
static inline pthread_t
mutex_queue_deq(pthread_mutex_t mutex)
{
	pthread_t pthread;

	while ((pthread = TAILQ_FIRST(&mutex->m_queue)) != NULL) {
		_thread_critical_enter(pthread);
		TAILQ_REMOVE(&mutex->m_queue, pthread, sqe);
		pthread->flags &= ~PTHREAD_FLAGS_IN_MUTEXQ;

		/*
		 * Only exit the loop if the thread hasn't been
		 * cancelled.
		 */
		if ((pthread->cancelflags & PTHREAD_CANCELLING) == 0 &&
		    pthread->state == PS_MUTEX_WAIT)
			break;
		else
			_thread_critical_exit(pthread);
	}

	return (pthread);
}

/*
 * Remove a waiting thread from a mutex queue in descending priority order.
 */
static inline void
mutex_queue_remove(pthread_mutex_t mutex, pthread_t pthread)
{
	if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0) {
		TAILQ_REMOVE(&mutex->m_queue, pthread, sqe);
		pthread->flags &= ~PTHREAD_FLAGS_IN_MUTEXQ;
	}
}

/*
 * Enqueue a waiting thread to a queue in descending priority order.
 */
static inline void
mutex_queue_enq(pthread_mutex_t mutex, pthread_t pthread)
{
	pthread_t tid = TAILQ_LAST(&mutex->m_queue, mutex_head);
	char *name;

	name = pthread->name ? pthread->name : "unknown";
	if ((pthread->flags & PTHREAD_FLAGS_IN_CONDQ) != 0)
		_thread_printf(2, "Thread (%s:%u) already on condq\n",
		    pthread->name, pthread->uniqueid);
	if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0)
		_thread_printf(2, "Thread (%s:%u) already on mutexq\n",
		    pthread->name, pthread->uniqueid);
	PTHREAD_ASSERT_NOT_IN_SYNCQ(pthread);
	/*
	 * For the common case of all threads having equal priority,
	 * we perform a quick check against the priority of the thread
	 * at the tail of the queue.
	 */
	if ((tid == NULL) || (pthread->active_priority <= tid->active_priority))
		TAILQ_INSERT_TAIL(&mutex->m_queue, pthread, sqe);
	else {
		tid = TAILQ_FIRST(&mutex->m_queue);
		while (pthread->active_priority <= tid->active_priority)
			tid = TAILQ_NEXT(tid, sqe);
		TAILQ_INSERT_BEFORE(tid, pthread, sqe);
	}
	pthread->flags |= PTHREAD_FLAGS_IN_MUTEXQ;
}

/*
 * Returns with the lock owned and on the threads mutexq if
 * it is currently unowned. Returns 1, otherwise.
 */
static int
get_muncontested(pthread_mutex_t mutexp, int nonblock)
{
	if (mutexp->m_owner != NULL && mutexp->m_owner != curthread)
		return (-1);
	else if (mutexp->m_owner == curthread)
		if (nonblock)
			return (mutex_self_trylock(mutexp));
		else
			return (mutex_self_lock(mutexp));

	/*
	 * The mutex belongs to this thread now. Mark it as
	 * such. Add it to the list of mutexes owned by this
	 * thread.
	 */
	mutexp->m_owner = curthread;
	_MUTEX_ASSERT_NOT_OWNED(mutexp);
	TAILQ_INSERT_TAIL(&curthread->mutexq, mutexp, m_qe);
	return (0);
}

/*
 * Returns with the lock owned and on the thread's mutexq. If
 * the mutex is currently owned by another thread it will sleep
 * until it is available.
 */
static void
get_mcontested(pthread_mutex_t mutexp)
{
	_thread_critical_enter(curthread);

	/*
	 * Put this thread on the mutex's list of waiting threads.
	 * The lock on the thread ensures atomic (as far as other
	 * threads are concerned) setting of the thread state with
	 * it's status on the mutex queue.
	 */
	do {
		mutex_queue_enq(mutexp, curthread);
		PTHREAD_SET_STATE(curthread, PS_MUTEX_WAIT);
		curthread->data.mutex = mutexp;
		_thread_critical_exit(curthread);
		_SPINUNLOCK(&mutexp->lock);
		_thread_suspend(curthread, NULL);

		_SPINLOCK(&mutexp->lock);
		_thread_critical_enter(curthread);
	} while ((curthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0);

	_thread_critical_exit(curthread);
}
