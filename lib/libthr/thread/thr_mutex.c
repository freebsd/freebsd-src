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
#define MUTEX_INIT_LINK(m) 		do {		\
	(m)->m_qe.tqe_prev = NULL;			\
	(m)->m_qe.tqe_next = NULL;			\
} while (0)
#define MUTEX_ASSERT_IS_OWNED(m)	do {		\
	if ((m)->m_qe.tqe_prev == NULL)			\
		PANIC("mutex is not on list");		\
} while (0)
#define MUTEX_ASSERT_NOT_OWNED(m)	do {		\
	if (((m)->m_qe.tqe_prev != NULL) ||		\
	    ((m)->m_qe.tqe_next != NULL))		\
		PANIC("mutex is on list");		\
} while (0)
#define	THR_ASSERT_NOT_IN_SYNCQ(thr)	do {		\
	THR_ASSERT(((thr)->sflags & THR_FLAGS_IN_SYNCQ) == 0, \
	    "thread in syncq when it shouldn't be.");	\
} while (0);
#else
#define MUTEX_INIT_LINK(m)
#define MUTEX_ASSERT_IS_OWNED(m)
#define MUTEX_ASSERT_NOT_OWNED(m)
#define	THR_ASSERT_NOT_IN_SYNCQ(thr)
#endif

#define THR_IN_MUTEXQ(thr)	(((thr)->sflags & THR_FLAGS_IN_SYNCQ) != 0)
#define	MUTEX_DESTROY(m) do {		\
	free(m);			\
} while (0)


/*
 * Prototypes
 */
static long		mutex_handoff(struct pthread *, struct pthread_mutex *);
static int		mutex_self_trylock(struct pthread *, pthread_mutex_t);
static int		mutex_self_lock(struct pthread *, pthread_mutex_t,
				const struct timespec *abstime);
static int		mutex_unlock_common(pthread_mutex_t *, int);
static void		mutex_priority_adjust(struct pthread *, pthread_mutex_t);
static void		mutex_rescan_owned (struct pthread *, struct pthread *,
			    struct pthread_mutex *);
#if 0
static pthread_t	mutex_queue_deq(pthread_mutex_t);
#endif
static void		mutex_queue_remove(pthread_mutex_t, pthread_t);
static void		mutex_queue_enq(pthread_mutex_t, pthread_t);

__weak_reference(__pthread_mutex_init, pthread_mutex_init);
__weak_reference(__pthread_mutex_lock, pthread_mutex_lock);
__weak_reference(__pthread_mutex_timedlock, pthread_mutex_timedlock);
__weak_reference(__pthread_mutex_trylock, pthread_mutex_trylock);

/* Single underscore versions provided for libc internal usage: */
/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_unlock, pthread_mutex_unlock);

static int
mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr, int private)
{
	struct pthread_mutex *pmutex;
	enum pthread_mutextype type;
	int		protocol;
	int		ceiling;
	int		flags;
	int		ret = 0;

	/* Check if default mutex attributes: */
	if (mutex_attr == NULL || *mutex_attr == NULL) {
		/* Default to a (error checking) POSIX mutex: */
		type = PTHREAD_MUTEX_ERRORCHECK;
		protocol = PTHREAD_PRIO_NONE;
		ceiling = THR_MAX_PRIORITY;
		flags = 0;
	}

	/* Check mutex type: */
	else if (((*mutex_attr)->m_type < PTHREAD_MUTEX_ERRORCHECK) ||
	    ((*mutex_attr)->m_type >= PTHREAD_MUTEX_TYPE_MAX))
		/* Return an invalid argument error: */
		ret = EINVAL;

	/* Check mutex protocol: */
	else if (((*mutex_attr)->m_protocol < PTHREAD_PRIO_NONE) ||
	    ((*mutex_attr)->m_protocol > PTHREAD_PRIO_PROTECT))
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
		    malloc(sizeof(struct pthread_mutex))) == NULL) {
			ret = ENOMEM;
		} else {
			_thr_umtx_init(&pmutex->m_lock);
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
				pmutex->m_count = 0;
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
				if (private)
					pmutex->m_flags |= MUTEX_FLAGS_PRIVATE;
				pmutex->m_owner = NULL;
				pmutex->m_type = type;
				pmutex->m_protocol = protocol;
				pmutex->m_refcount = 0;
				if (protocol == PTHREAD_PRIO_PROTECT)
					pmutex->m_prio = ceiling;
				else
					pmutex->m_prio = -1;
				pmutex->m_saved_prio = 0;
				MUTEX_INIT_LINK(pmutex);
				*mutex = pmutex;
			} else {
				/* Free the mutex lock structure: */
				MUTEX_DESTROY(pmutex);
				*mutex = NULL;
			}
		}
	}
	/* Return the completion status: */
	return (ret);
}

static int
init_static(struct pthread *thread, pthread_mutex_t *mutex)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);

	if (*mutex == NULL)
		ret = mutex_init(mutex, NULL, 0);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_mutex_static_lock);

	return (ret);
}

static int
init_static_private(struct pthread *thread, pthread_mutex_t *mutex)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);

	if (*mutex == NULL)
		ret = mutex_init(mutex, NULL, 1);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_mutex_static_lock);

	return (ret);
}

int
_pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init(mutex, mutex_attr, 1);
}

int
__pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init(mutex, mutex_attr, 0);
}

int
_mutex_reinit(pthread_mutex_t *mutex)
{
	_thr_umtx_init(&(*mutex)->m_lock);
	TAILQ_INIT(&(*mutex)->m_queue);
	MUTEX_INIT_LINK(*mutex);
	(*mutex)->m_owner = NULL;
	(*mutex)->m_count = 0;
	(*mutex)->m_refcount = 0;
	(*mutex)->m_prio = 0;
	(*mutex)->m_saved_prio = 0;
	return (0);
}

void
_mutex_fork(struct pthread *curthread)
{
	TAILQ_INIT(&curthread->mutexq);
	TAILQ_INIT(&curthread->pri_mutexq);
	curthread->priority_mutex_count = 0;
#if 0
	struct pthread_mutex *m;

	TAILQ_FOREACH(m, &curthread->mutexq, m_qe) {
		m->m_lock = (umtx_t)curthread->tid;
	}

	/* Clear contender for priority mutexes */
	TAILQ_FOREACH(m, &curthread->pri_mutexq, m_qe) {
		/* clear another thread locked us */
		_thr_umtx_init(&m->m_lock);
		TAILQ_INIT(&m->m_queue);
	}
#endif
}

int
_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	pthread_mutex_t m;
	int ret = 0;

	if (mutex == NULL || *mutex == NULL)
		ret = EINVAL;
	else {
		/*
		 * Try to lock the mutex structure, we only need to
		 * try once, if failed, the mutex is in used.
		 */
		ret = THR_UMTX_TRYLOCK(curthread, &(*mutex)->m_lock);
		if (ret)
			return (ret);

		/*
		 * Check mutex other fields to see if this mutex is
		 * in use. Mostly for prority mutex types, or there
		 * are condition variables referencing it.
		 */
		if (((*mutex)->m_owner != NULL) ||
		    (TAILQ_FIRST(&(*mutex)->m_queue) != NULL) ||
		    ((*mutex)->m_refcount != 0)) {
			THR_UMTX_UNLOCK(curthread, &(*mutex)->m_lock);
			ret = EBUSY;
		} else {
			/*
			 * Save a pointer to the mutex so it can be free'd
			 * and set the caller's pointer to NULL:
			 */
			m = *mutex;
			*mutex = NULL;

			/* Unlock the mutex structure: */
			_thr_umtx_unlock(&m->m_lock, curthread->tid);

			/*
			 * Free the memory allocated for the mutex
			 * structure:
			 */
			MUTEX_ASSERT_NOT_OWNED(m);
			MUTEX_DESTROY(m);
		}
	}

	/* Return the completion status: */
	return (ret);
}

static int
mutex_trylock_common(struct pthread *curthread, pthread_mutex_t *mutex)
{
	int ret = 0;

	THR_ASSERT((mutex != NULL) && (*mutex != NULL),
	    "Uninitialized mutex in mutex_trylock_common");

	/* Short cut for simple mutex. */
	if ((*mutex)->m_protocol == PTHREAD_PRIO_NONE) {
		ret = THR_UMTX_TRYLOCK(curthread, &(*mutex)->m_lock);
		if (ret == 0) {
			(*mutex)->m_owner = curthread;
			/* Add to the list of owned mutexes: */
			MUTEX_ASSERT_NOT_OWNED(*mutex);
			TAILQ_INSERT_TAIL(&curthread->mutexq,
			    (*mutex), m_qe);
		} else if ((*mutex)->m_owner == curthread) {
			ret = mutex_self_trylock(curthread, *mutex);
		} /* else {} */

		return (ret);
	}

	/* Code for priority mutex */

	/* Lock the mutex structure: */
	THR_LOCK_ACQUIRE(curthread, &(*mutex)->m_lock);

	/*
	 * If the mutex was statically allocated, properly
	 * initialize the tail queue.
	 */
	if (((*mutex)->m_flags & MUTEX_FLAGS_INITED) == 0) {
		TAILQ_INIT(&(*mutex)->m_queue);
		MUTEX_INIT_LINK(*mutex);
		(*mutex)->m_flags |= MUTEX_FLAGS_INITED;
	}

	/* Process according to mutex type: */
	switch ((*mutex)->m_protocol) {
	/* POSIX priority inheritence mutex: */
	case PTHREAD_PRIO_INHERIT:
		/* Check if this mutex is not locked: */
		if ((*mutex)->m_owner == NULL) {
			/* Lock the mutex for the running thread: */
			(*mutex)->m_owner = curthread;

			THR_LOCK(curthread);
			/* Track number of priority mutexes owned: */
			curthread->priority_mutex_count++;

			/*
			 * The mutex takes on the attributes of the
			 * running thread when there are no waiters.
			 */
			(*mutex)->m_prio = curthread->active_priority;
			(*mutex)->m_saved_prio =
			    curthread->inherited_priority;
			curthread->inherited_priority = (*mutex)->m_prio;
			THR_UNLOCK(curthread);

			/* Add to the list of owned mutexes: */
			MUTEX_ASSERT_NOT_OWNED(*mutex);
			TAILQ_INSERT_TAIL(&curthread->pri_mutexq,
			    (*mutex), m_qe);
		} else if ((*mutex)->m_owner == curthread)
			ret = mutex_self_trylock(curthread, *mutex);
		else
			/* Return a busy error: */
			ret = EBUSY;
		break;

	/* POSIX priority protection mutex: */
	case PTHREAD_PRIO_PROTECT:
		/* Check for a priority ceiling violation: */
		if (curthread->active_priority > (*mutex)->m_prio)
			ret = EINVAL;

		/* Check if this mutex is not locked: */
		else if ((*mutex)->m_owner == NULL) {
			/* Lock the mutex for the running thread: */
			(*mutex)->m_owner = curthread;

			THR_LOCK(curthread);
			/* Track number of priority mutexes owned: */
			curthread->priority_mutex_count++;

			/*
			 * The running thread inherits the ceiling
			 * priority of the mutex and executes at that
			 * priority.
			 */
			curthread->active_priority = (*mutex)->m_prio;
			(*mutex)->m_saved_prio =
			    curthread->inherited_priority;
			curthread->inherited_priority =
			    (*mutex)->m_prio;
			THR_UNLOCK(curthread);
			/* Add to the list of owned mutexes: */
			MUTEX_ASSERT_NOT_OWNED(*mutex);
			TAILQ_INSERT_TAIL(&curthread->pri_mutexq,
			    (*mutex), m_qe);
		} else if ((*mutex)->m_owner == curthread)
			ret = mutex_self_trylock(curthread, *mutex);
		else
			/* Return a busy error: */
			ret = EBUSY;
		break;

	/* Trap invalid mutex types: */
	default:
		/* Return an invalid argument error: */
		ret = EINVAL;
		break;
	}

	/* Unlock the mutex structure: */
	THR_LOCK_RELEASE(curthread, &(*mutex)->m_lock);

	/* Return the completion status: */
	return (ret);
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	int ret = 0;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if ((*mutex != NULL) ||
	    ((ret = init_static(curthread, mutex)) == 0))
		ret = mutex_trylock_common(curthread, mutex);

	return (ret);
}

int
_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct pthread	*curthread = _get_curthread();
	int	ret = 0;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking the mutex private (delete safe):
	 */
	if ((*mutex != NULL) ||
	    ((ret = init_static_private(curthread, mutex)) == 0))
		ret = mutex_trylock_common(curthread, mutex);

	return (ret);
}

static int
mutex_lock_common(struct pthread *curthread, pthread_mutex_t *m,
	const struct timespec * abstime)
{
	struct  timespec ts, ts2;
	long	cycle;
	int	ret = 0;

	THR_ASSERT((m != NULL) && (*m != NULL),
	    "Uninitialized mutex in mutex_lock_common");

	if (abstime != NULL && (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000))
		return (EINVAL);

	/* Short cut for simple mutex. */

	if ((*m)->m_protocol == PTHREAD_PRIO_NONE) {
		/* Default POSIX mutex: */
		ret = THR_UMTX_TRYLOCK(curthread, &(*m)->m_lock);
		if (ret == 0) {
			(*m)->m_owner = curthread;
			/* Add to the list of owned mutexes: */
			MUTEX_ASSERT_NOT_OWNED(*m);
			TAILQ_INSERT_TAIL(&curthread->mutexq,
			    (*m), m_qe);
		} else if ((*m)->m_owner == curthread) {
			ret = mutex_self_lock(curthread, *m, abstime);
		} else {
			if (abstime == NULL) {
				THR_UMTX_LOCK(curthread, &(*m)->m_lock);
				ret = 0;
			} else {
				clock_gettime(CLOCK_REALTIME, &ts);
				TIMESPEC_SUB(&ts2, abstime, &ts);
				ret = THR_UMTX_TIMEDLOCK(curthread,
					&(*m)->m_lock, &ts2);
				/*
				 * Timed out wait is not restarted if
				 * it was interrupted, not worth to do it.
				 */
				if (ret == EINTR)
					ret = ETIMEDOUT;
			}
			if (ret == 0) {
				(*m)->m_owner = curthread;
				/* Add to the list of owned mutexes: */
				MUTEX_ASSERT_NOT_OWNED(*m);
				TAILQ_INSERT_TAIL(&curthread->mutexq,
				    (*m), m_qe);
			}
		}
		return (ret);
	}

	/* Code for priority mutex */

	/*
	 * Enter a loop waiting to become the mutex owner.  We need a
	 * loop in case the waiting thread is interrupted by a signal
	 * to execute a signal handler.  It is not (currently) possible
	 * to remain in the waiting queue while running a handler.
	 * Instead, the thread is interrupted and backed out of the
	 * waiting queue prior to executing the signal handler.
	 */
	do {
		/* Lock the mutex structure: */
		THR_LOCK_ACQUIRE(curthread, &(*m)->m_lock);

		/*
		 * If the mutex was statically allocated, properly
		 * initialize the tail queue.
		 */
		if (((*m)->m_flags & MUTEX_FLAGS_INITED) == 0) {
			TAILQ_INIT(&(*m)->m_queue);
			(*m)->m_flags |= MUTEX_FLAGS_INITED;
			MUTEX_INIT_LINK(*m);
		}

		/* Process according to mutex type: */
		switch ((*m)->m_protocol) {
		/* POSIX priority inheritence mutex: */
		case PTHREAD_PRIO_INHERIT:
			/* Check if this mutex is not locked: */
			if ((*m)->m_owner == NULL) {
				/* Lock the mutex for this thread: */
				(*m)->m_owner = curthread;

				THR_LOCK(curthread);
				/* Track number of priority mutexes owned: */
				curthread->priority_mutex_count++;

				/*
				 * The mutex takes on attributes of the
				 * running thread when there are no waiters.
				 * Make sure the thread's scheduling lock is
				 * held while priorities are adjusted.
				 */
				(*m)->m_prio = curthread->active_priority;
				(*m)->m_saved_prio =
				    curthread->inherited_priority;
				curthread->inherited_priority = (*m)->m_prio;
				THR_UNLOCK(curthread);

				/* Add to the list of owned mutexes: */
				MUTEX_ASSERT_NOT_OWNED(*m);
				TAILQ_INSERT_TAIL(&curthread->pri_mutexq,
				    (*m), m_qe);

				/* Unlock the mutex structure: */
				THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
			} else if ((*m)->m_owner == curthread) {
				ret = mutex_self_lock(curthread, *m, abstime);

				/* Unlock the mutex structure: */
				THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
			} else {
				/*
				 * Join the queue of threads waiting to lock
				 * the mutex and save a pointer to the mutex.
				 */
				mutex_queue_enq(*m, curthread);
				curthread->data.mutex = *m;

				if (curthread->active_priority > (*m)->m_prio)
					/* Adjust priorities: */
					mutex_priority_adjust(curthread, *m);

				THR_LOCK(curthread);
				cycle = curthread->cycle;
				THR_UNLOCK(curthread);

				/* Unlock the mutex structure: */
				THR_LOCK_RELEASE(curthread, &(*m)->m_lock);

				clock_gettime(CLOCK_REALTIME, &ts);
				TIMESPEC_SUB(&ts2, abstime, &ts);
				ret = _thr_umtx_wait(&curthread->cycle, cycle,
					 &ts2);
				if (ret == EINTR)
					ret = 0;

				if (THR_IN_MUTEXQ(curthread)) {
					THR_LOCK_ACQUIRE(curthread, &(*m)->m_lock);
					mutex_queue_remove(*m, curthread);
					THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
				}
				/*
				 * Only clear these after assuring the
				 * thread is dequeued.
				 */
				curthread->data.mutex = NULL;
			}
			break;

		/* POSIX priority protection mutex: */
		case PTHREAD_PRIO_PROTECT:
			/* Check for a priority ceiling violation: */
			if (curthread->active_priority > (*m)->m_prio) {
				/* Unlock the mutex structure: */
				THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
				ret = EINVAL;
			}
			/* Check if this mutex is not locked: */
			else if ((*m)->m_owner == NULL) {
				/*
				 * Lock the mutex for the running
				 * thread:
				 */
				(*m)->m_owner = curthread;

				THR_LOCK(curthread);
				/* Track number of priority mutexes owned: */
				curthread->priority_mutex_count++;

				/*
				 * The running thread inherits the ceiling
				 * priority of the mutex and executes at that
				 * priority.  Make sure the thread's
				 * scheduling lock is held while priorities
				 * are adjusted.
				 */
				curthread->active_priority = (*m)->m_prio;
				(*m)->m_saved_prio =
				    curthread->inherited_priority;
				curthread->inherited_priority = (*m)->m_prio;
				THR_UNLOCK(curthread);

				/* Add to the list of owned mutexes: */
				MUTEX_ASSERT_NOT_OWNED(*m);
				TAILQ_INSERT_TAIL(&curthread->pri_mutexq,
				    (*m), m_qe);

				/* Unlock the mutex structure: */
				THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
			} else if ((*m)->m_owner == curthread) {
				ret = mutex_self_lock(curthread, *m, abstime);

				/* Unlock the mutex structure: */
				THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
			} else {
				/*
				 * Join the queue of threads waiting to lock
				 * the mutex and save a pointer to the mutex.
				 */
				mutex_queue_enq(*m, curthread);
				curthread->data.mutex = *m;

				/* Clear any previous error: */
				curthread->error = 0;

				THR_LOCK(curthread);
				cycle = curthread->cycle;
				THR_UNLOCK(curthread);

				/* Unlock the mutex structure: */
				THR_LOCK_RELEASE(curthread, &(*m)->m_lock);

				clock_gettime(CLOCK_REALTIME, &ts);
				TIMESPEC_SUB(&ts2, abstime, &ts);
				ret = _thr_umtx_wait(&curthread->cycle, cycle,
					&ts2);
				if (ret == EINTR)
					ret = 0;

				curthread->data.mutex = NULL;
				if (THR_IN_MUTEXQ(curthread)) {
					THR_LOCK_ACQUIRE(curthread, &(*m)->m_lock);
					mutex_queue_remove(*m, curthread);
					THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
				}
				/*
				 * Only clear these after assuring the
				 * thread is dequeued.
				 */
				curthread->data.mutex = NULL;

				/*
				 * The threads priority may have changed while
				 * waiting for the mutex causing a ceiling
				 * violation.
				 */
				ret = curthread->error;
				curthread->error = 0;
			}
			break;

		/* Trap invalid mutex types: */
		default:
			/* Unlock the mutex structure: */
			THR_LOCK_RELEASE(curthread, &(*m)->m_lock);

			/* Return an invalid argument error: */
			ret = EINVAL;
			break;
		}

	} while (((*m)->m_owner != curthread) && (ret == 0));

	/* Return the completion status: */
	return (ret);
}

int
__pthread_mutex_lock(pthread_mutex_t *m)
{
	struct pthread *curthread;
	int	ret = 0;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if ((*m != NULL) || ((ret = init_static(curthread, m)) == 0))
		ret = mutex_lock_common(curthread, m, NULL);

	return (ret);
}

int
_pthread_mutex_lock(pthread_mutex_t *m)
{
	struct pthread *curthread;
	int	ret = 0;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	if ((*m != NULL) ||
	    ((ret = init_static_private(curthread, m)) == 0))
		ret = mutex_lock_common(curthread, m, NULL);

	return (ret);
}

int
__pthread_mutex_timedlock(pthread_mutex_t *m,
	const struct timespec *abs_timeout)
{
	struct pthread *curthread;
	int	ret = 0;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if ((*m != NULL) || ((ret = init_static(curthread, m)) == 0))
		ret = mutex_lock_common(curthread, m, abs_timeout);

	return (ret);
}

int
_pthread_mutex_timedlock(pthread_mutex_t *m,
	const struct timespec *abs_timeout)
{
	struct pthread *curthread;
	int	ret = 0;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	if ((*m != NULL) ||
	    ((ret = init_static_private(curthread, m)) == 0))
		ret = mutex_lock_common(curthread, m, abs_timeout);

	return (ret);
}

int
_pthread_mutex_unlock(pthread_mutex_t *m)
{
	return (mutex_unlock_common(m, /* add reference */ 0));
}

int
_mutex_cv_unlock(pthread_mutex_t *m)
{
	return (mutex_unlock_common(m, /* add reference */ 1));
}

int
_mutex_cv_lock(pthread_mutex_t *m)
{
	int	ret;

	if ((ret = _pthread_mutex_lock(m)) == 0)
		(*m)->m_refcount--;
	return (ret);
}

static int
mutex_self_trylock(struct pthread *curthread, pthread_mutex_t m)
{
	int	ret;

	switch (m->m_type) {
	/* case PTHREAD_MUTEX_DEFAULT: */
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_NORMAL:
		ret = EBUSY; 
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		if (m->m_count + 1 > 0) {
			m->m_count++;
			ret = 0;
		} else
			ret = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

static int
mutex_self_lock(struct pthread *curthread, pthread_mutex_t m,
	const struct timespec *abstime)
{
	struct timespec ts1, ts2;
	int ret;

	switch (m->m_type) {
	/* case PTHREAD_MUTEX_DEFAULT: */
	case PTHREAD_MUTEX_ERRORCHECK:
		if (abstime) {
			clock_gettime(CLOCK_REALTIME, &ts1);
			TIMESPEC_SUB(&ts2, abstime, &ts1);
			__sys_nanosleep(&ts2, NULL);
			ret = ETIMEDOUT;
		} else {
			/*
			 * POSIX specifies that mutexes should return
			 * EDEADLK if a recursive lock is detected.
			 */
			ret = EDEADLK; 
		}
		break;

	case PTHREAD_MUTEX_NORMAL:
		/*
		 * What SS2 define as a 'normal' mutex.  Intentionally
		 * deadlock on attempts to get a lock you already own.
		 */
		ret = 0;
		if (m->m_protocol != PTHREAD_PRIO_NONE) {
			/* Unlock the mutex structure: */
			THR_LOCK_RELEASE(curthread, &m->m_lock);
		}
		if (abstime) {
			clock_gettime(CLOCK_REALTIME, &ts1);
			TIMESPEC_SUB(&ts2, abstime, &ts1);
			__sys_nanosleep(&ts2, NULL);
			ret = ETIMEDOUT;
		} else {
			ts1.tv_sec = 30;
			ts1.tv_nsec = 0;
			for (;;)
				__sys_nanosleep(&ts1, NULL);
		}
		break;

	case PTHREAD_MUTEX_RECURSIVE:
		/* Increment the lock count: */
		if (m->m_count + 1 > 0) {
			m->m_count++;
			ret = 0;
		} else
			ret = EAGAIN;
		break;

	default:
		/* Trap invalid mutex types; */
		ret = EINVAL;
	}

	return (ret);
}

static int
mutex_unlock_common(pthread_mutex_t *m, int add_reference)
{
	struct pthread *curthread = _get_curthread();
	long tid = -1;
	int ret = 0;

	if (m == NULL || *m == NULL)
		ret = EINVAL;
	else {
		/* Short cut for simple mutex. */

		if ((*m)->m_protocol == PTHREAD_PRIO_NONE) {
			/*
			 * Check if the running thread is not the owner of the
			 * mutex:
			 */
			if (__predict_false((*m)->m_owner != curthread)) {
				ret = EPERM;
			} else if (__predict_false(
				  (*m)->m_type == PTHREAD_MUTEX_RECURSIVE &&
			          (*m)->m_count > 0)) {
				/* Decrement the count: */
				(*m)->m_count--;
				if (add_reference)
					(*m)->m_refcount++;
			} else {
				/*
				 * Clear the count in case this is a recursive
				 * mutex.
				 */
				(*m)->m_count = 0;
				(*m)->m_owner = NULL;
				/* Remove the mutex from the threads queue. */
				MUTEX_ASSERT_IS_OWNED(*m);
				TAILQ_REMOVE(&curthread->mutexq, (*m), m_qe);
				MUTEX_INIT_LINK(*m);
				if (add_reference)
					(*m)->m_refcount++;
				/*
				 * Hand off the mutex to the next waiting
				 * thread.
				 */
				_thr_umtx_unlock(&(*m)->m_lock, curthread->tid);
			}
			return (ret);
		}

		/* Code for priority mutex */

		/* Lock the mutex structure: */
		THR_LOCK_ACQUIRE(curthread, &(*m)->m_lock);

		/* Process according to mutex type: */
		switch ((*m)->m_protocol) {
		/* POSIX priority inheritence mutex: */
		case PTHREAD_PRIO_INHERIT:
			/*
			 * Check if the running thread is not the owner of the
			 * mutex:
			 */
			if ((*m)->m_owner != curthread)
				ret = EPERM;
			else if (((*m)->m_type == PTHREAD_MUTEX_RECURSIVE) &&
			    ((*m)->m_count > 0))
				/* Decrement the count: */
				(*m)->m_count--;
			else {
				/*
				 * Clear the count in case this is recursive
				 * mutex.
				 */
				(*m)->m_count = 0;

				/*
				 * Restore the threads inherited priority and
				 * recompute the active priority (being careful
				 * not to override changes in the threads base
				 * priority subsequent to locking the mutex).
				 */
				THR_LOCK(curthread);
				curthread->inherited_priority =
					(*m)->m_saved_prio;
				curthread->active_priority =
				    MAX(curthread->inherited_priority,
				    curthread->base_priority);

				/*
				 * This thread now owns one less priority mutex.
				 */
				curthread->priority_mutex_count--;
				THR_UNLOCK(curthread);

				/* Remove the mutex from the threads queue. */
				MUTEX_ASSERT_IS_OWNED(*m);
				TAILQ_REMOVE(&(*m)->m_owner->pri_mutexq,
				    (*m), m_qe);
				MUTEX_INIT_LINK(*m);

				/*
				 * Hand off the mutex to the next waiting
				 * thread:
				 */
				tid = mutex_handoff(curthread, *m);
			}
			break;

		/* POSIX priority ceiling mutex: */
		case PTHREAD_PRIO_PROTECT:
			/*
			 * Check if the running thread is not the owner of the
			 * mutex:
			 */
			if ((*m)->m_owner != curthread)
				ret = EPERM;
			else if (((*m)->m_type == PTHREAD_MUTEX_RECURSIVE) &&
			    ((*m)->m_count > 0))
				/* Decrement the count: */
				(*m)->m_count--;
			else {
				/*
				 * Clear the count in case this is a recursive
				 * mutex.
				 */
				(*m)->m_count = 0;

				/*
				 * Restore the threads inherited priority and
				 * recompute the active priority (being careful
				 * not to override changes in the threads base
				 * priority subsequent to locking the mutex).
				 */
				THR_LOCK(curthread);
				curthread->inherited_priority =
					(*m)->m_saved_prio;
				curthread->active_priority =
				    MAX(curthread->inherited_priority,
				    curthread->base_priority);

				/*
				 * This thread now owns one less priority mutex.
				 */
				curthread->priority_mutex_count--;
				THR_UNLOCK(curthread);

				/* Remove the mutex from the threads queue. */
				MUTEX_ASSERT_IS_OWNED(*m);
				TAILQ_REMOVE(&(*m)->m_owner->pri_mutexq,
				    (*m), m_qe);
				MUTEX_INIT_LINK(*m);

				/*
				 * Hand off the mutex to the next waiting
				 * thread:
				 */
				tid = mutex_handoff(curthread, *m);
			}
			break;

		/* Trap invalid mutex types: */
		default:
			/* Return an invalid argument error: */
			ret = EINVAL;
			break;
		}

		if ((ret == 0) && (add_reference != 0))
			/* Increment the reference count: */
			(*m)->m_refcount++;

		/* Unlock the mutex structure: */
		THR_LOCK_RELEASE(curthread, &(*m)->m_lock);
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
 * This must be called without the target thread's scheduling lock held.
 */
void
_mutex_notify_priochange(struct pthread *curthread, struct pthread *pthread,
    int propagate_prio)
{
	struct pthread_mutex *m;

	/* Adjust the priorites of any owned priority mutexes: */
	if (pthread->priority_mutex_count > 0) {
		/*
		 * Rescan the mutexes owned by this thread and correct
		 * their priorities to account for this threads change
		 * in priority.  This has the side effect of changing
		 * the threads active priority.
		 *
		 * Be sure to lock the first mutex in the list of owned
		 * mutexes.  This acts as a barrier against another
		 * simultaneous call to change the threads priority
		 * and from the owning thread releasing the mutex.
		 */
		m = TAILQ_FIRST(&pthread->pri_mutexq);
		if (m != NULL) {
			THR_LOCK_ACQUIRE(curthread, &m->m_lock);
			/*
			 * Make sure the thread still owns the lock.
			 */
			if (m == TAILQ_FIRST(&pthread->pri_mutexq))
				mutex_rescan_owned(curthread, pthread,
				    /* rescan all owned */ NULL);
			THR_LOCK_RELEASE(curthread, &m->m_lock);
		}
	}

	/*
	 * If this thread is waiting on a priority inheritence mutex,
	 * check for priority adjustments.  A change in priority can
	 * also cause a ceiling violation(*) for a thread waiting on
	 * a priority protection mutex; we don't perform the check here
	 * as it is done in pthread_mutex_unlock.
	 *
	 * (*) It should be noted that a priority change to a thread
	 *     _after_ taking and owning a priority ceiling mutex
	 *     does not affect ownership of that mutex; the ceiling
	 *     priority is only checked before mutex ownership occurs.
	 */
	if (propagate_prio != 0) {
		/*
		 * Lock the thread's scheduling queue.  This is a bit
		 * convoluted; the "in synchronization queue flag" can
		 * only be cleared with both the thread's scheduling and
		 * mutex locks held.  The thread's pointer to the wanted
		 * mutex is guaranteed to be valid during this time.
		 */
		THR_THREAD_LOCK(curthread, pthread);

		if (((pthread->sflags & THR_FLAGS_IN_SYNCQ) == 0) ||
		    ((m = pthread->data.mutex) == NULL))
			THR_THREAD_UNLOCK(curthread, pthread);
		else {
			/*
			 * This thread is currently waiting on a mutex; unlock
			 * the scheduling queue lock and lock the mutex.  We
			 * can't hold both at the same time because the locking
			 * order could cause a deadlock.
			 */
			THR_THREAD_UNLOCK(curthread, pthread);
			THR_LOCK_ACQUIRE(curthread, &m->m_lock);

			/*
			 * Check to make sure this thread is still in the
			 * same state (the lock above can yield the CPU to
			 * another thread or the thread may be running on
			 * another CPU).
			 */
			if (((pthread->sflags & THR_FLAGS_IN_SYNCQ) != 0) &&
			    (pthread->data.mutex == m)) {
				/*
				 * Remove and reinsert this thread into
				 * the list of waiting threads to preserve
				 * decreasing priority order.
				 */
				mutex_queue_remove(m, pthread);
				mutex_queue_enq(m, pthread);

				if (m->m_protocol == PTHREAD_PRIO_INHERIT)
					/* Adjust priorities: */
					mutex_priority_adjust(curthread, m);
			}

			/* Unlock the mutex structure: */
			THR_LOCK_RELEASE(curthread, &m->m_lock);
		}
	}
}

/*
 * Called when a new thread is added to the mutex waiting queue or
 * when a threads priority changes that is already in the mutex
 * waiting queue.
 *
 * This must be called with the mutex locked by the current thread.
 */
static void
mutex_priority_adjust(struct pthread *curthread, pthread_mutex_t mutex)
{
	pthread_mutex_t	m = mutex;
	struct pthread	*pthread_next, *pthread = mutex->m_owner;
	int		done, temp_prio;

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

	/*
	 * Don't unlock the mutex passed in as an argument.  It is
	 * expected to be locked and unlocked by the caller.
	 */
	done = 1;
	do {
		/*
		 * Save the threads priority before rescanning the
		 * owned mutexes:
		 */
		temp_prio = pthread->active_priority;

		/*
		 * Fix the priorities for all mutexes held by the owning
		 * thread since taking this mutex.  This also has a
		 * potential side-effect of changing the threads priority.
		 *
		 * At this point the mutex is locked by the current thread.
		 * The owning thread can't release the mutex until it is
		 * unlocked, so we should be able to safely walk its list
		 * of owned mutexes.
		 */
		mutex_rescan_owned(curthread, pthread, m);

		/*
		 * If this isn't the first time through the loop,
		 * the current mutex needs to be unlocked.
		 */
		if (done == 0)
			THR_LOCK_RELEASE(curthread, &m->m_lock);

		/* Assume we're done unless told otherwise: */
		done = 1;

		/*
		 * If the thread is currently waiting on a mutex, check
		 * to see if the threads new priority has affected the
		 * priority of the mutex.
		 */
		if ((temp_prio != pthread->active_priority) &&
		    ((pthread->sflags & THR_FLAGS_IN_SYNCQ) != 0) &&
		    ((m = pthread->data.mutex) != NULL) &&
		    (m->m_protocol == PTHREAD_PRIO_INHERIT)) {
			/* Lock the mutex structure: */
			THR_LOCK_ACQUIRE(curthread, &m->m_lock);

			/*
			 * Make sure the thread is still waiting on the
			 * mutex:
			 */
			if (((pthread->sflags & THR_FLAGS_IN_SYNCQ) != 0) &&
			    (m == pthread->data.mutex)) {
				/*
				 * The priority for this thread has changed.
				 * Remove and reinsert this thread into the
				 * list of waiting threads to preserve
				 * decreasing priority order.
				 */
				mutex_queue_remove(m, pthread);
				mutex_queue_enq(m, pthread);

				/*
				 * Grab the waiting thread with highest
				 * priority:
				 */
				pthread_next = TAILQ_FIRST(&m->m_queue);

				/*
				 * Calculate the mutex priority as the maximum
				 * of the highest active priority of any
				 * waiting threads and the owning threads
				 * active priority.
				 */
				temp_prio = MAX(pthread_next->active_priority,
				    MAX(m->m_saved_prio,
				    m->m_owner->base_priority));

				if (temp_prio != m->m_prio) {
					/*
					 * The priority needs to be propagated
					 * to the mutex this thread is waiting
					 * on and up to the owner of that mutex.
					 */
					m->m_prio = temp_prio;
					pthread = m->m_owner;

					/* We're not done yet: */
					done = 0;
				}
			}
			/* Only release the mutex if we're done: */
			if (done != 0)
				THR_LOCK_RELEASE(curthread, &m->m_lock);
		}
	} while (done == 0);
}

static void
mutex_rescan_owned(struct pthread *curthread, struct pthread *pthread,
    struct pthread_mutex *mutex)
{
	struct pthread_mutex	*m;
	struct pthread		*pthread_next;
	int			active_prio, inherited_prio;

	/*
	 * Start walking the mutexes the thread has taken since
	 * taking this mutex.
	 */
	if (mutex == NULL) {
		/*
		 * A null mutex means start at the beginning of the owned
		 * mutex list.
		 */
		m = TAILQ_FIRST(&pthread->pri_mutexq);

		/* There is no inherited priority yet. */
		inherited_prio = 0;
	} else {
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

	for (; m != NULL; m = TAILQ_NEXT(m, m_qe)) {
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
	}

	/*
	 * Fix the threads inherited priority and recalculate its
	 * active priority.
	 */
	pthread->inherited_priority = inherited_prio;
	active_prio = MAX(inherited_prio, pthread->base_priority);

	if (active_prio != pthread->active_priority) {
		/* Lock the thread's scheduling queue: */
		THR_THREAD_LOCK(curthread, pthread);

		/* if ((pthread->flags & THR_FLAGS_IN_RUNQ) == 0) */
		if (1) {
			/*
			 * This thread is not in a run queue.  Just set
			 * its active priority.
			 */
			pthread->active_priority = active_prio;
		}
		else {
			/*
			 * This thread is in a run queue.  Remove it from
			 * the queue before changing its priority:
			 */
			/* THR_RUNQ_REMOVE(pthread);*/
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
				/* THR_RUNQ_INSERT_HEAD(pthread); */
			} else {
				/* Set the new active priority. */
				pthread->active_priority = active_prio;
				/* THR_RUNQ_INSERT_TAIL(pthread);*/
			}
		}
		THR_THREAD_UNLOCK(curthread, pthread);
	}
}

void
_mutex_unlock_private(pthread_t pthread)
{
	struct pthread_mutex	*m, *m_next;

	for (m = TAILQ_FIRST(&pthread->pri_mutexq); m != NULL; m = m_next) {
		m_next = TAILQ_NEXT(m, m_qe);
		if ((m->m_flags & MUTEX_FLAGS_PRIVATE) != 0)
			pthread_mutex_unlock(&m);
	}
}

/*
 * Dequeue a waiting thread from the head of a mutex queue in descending
 * priority order.
 *
 * In order to properly dequeue a thread from the mutex queue and
 * make it runnable without the possibility of errant wakeups, it
 * is necessary to lock the thread's scheduling queue while also
 * holding the mutex lock.
 */
static long
mutex_handoff(struct pthread *curthread, struct pthread_mutex *mutex)
{
	struct pthread *pthread;
	long tid = -1;

	/* Keep dequeueing until we find a valid thread: */
	mutex->m_owner = NULL;
	pthread = TAILQ_FIRST(&mutex->m_queue);
	while (pthread != NULL) {
		/* Take the thread's scheduling lock: */
		THR_THREAD_LOCK(curthread, pthread);

		/* Remove the thread from the mutex queue: */
		TAILQ_REMOVE(&mutex->m_queue, pthread, sqe);
		pthread->sflags &= ~THR_FLAGS_IN_SYNCQ;

		/*
		 * Only exit the loop if the thread hasn't been
		 * cancelled.
		 */
		switch (mutex->m_protocol) {
		case PTHREAD_PRIO_NONE:
			/*
			 * Assign the new owner and add the mutex to the
			 * thread's list of owned mutexes.
			 */
			mutex->m_owner = pthread;
			TAILQ_INSERT_TAIL(&pthread->pri_mutexq, mutex, m_qe);
			break;

		case PTHREAD_PRIO_INHERIT:
			/*
			 * Assign the new owner and add the mutex to the
			 * thread's list of owned mutexes.
			 */
			mutex->m_owner = pthread;
			TAILQ_INSERT_TAIL(&pthread->pri_mutexq, mutex, m_qe);

			/* Track number of priority mutexes owned: */
			pthread->priority_mutex_count++;

			/*
			 * Set the priority of the mutex.  Since our waiting
			 * threads are in descending priority order, the
			 * priority of the mutex becomes the active priority
			 * of the thread we just dequeued.
			 */
			mutex->m_prio = pthread->active_priority;

			/* Save the owning threads inherited priority: */
			mutex->m_saved_prio = pthread->inherited_priority;

			/*
			 * The owning threads inherited priority now becomes
			 * his active priority (the priority of the mutex).
			 */
			pthread->inherited_priority = mutex->m_prio;
			break;

		case PTHREAD_PRIO_PROTECT:
			if (pthread->active_priority > mutex->m_prio) {
				/*
				 * Either the mutex ceiling priority has
				 * been lowered and/or this threads priority
			 	 * has been raised subsequent to the thread
				 * being queued on the waiting list.
				 */
				pthread->error = EINVAL;
			}
			else {
				/*
				 * Assign the new owner and add the mutex
				 * to the thread's list of owned mutexes.
				 */
				mutex->m_owner = pthread;
				TAILQ_INSERT_TAIL(&pthread->pri_mutexq,
				    mutex, m_qe);

				/* Track number of priority mutexes owned: */
				pthread->priority_mutex_count++;

				/*
				 * Save the owning threads inherited
				 * priority:
				 */
				mutex->m_saved_prio =
				    pthread->inherited_priority;

				/*
				 * The owning thread inherits the ceiling
				 * priority of the mutex and executes at
				 * that priority:
				 */
				pthread->inherited_priority = mutex->m_prio;
				pthread->active_priority = mutex->m_prio;

			}
			break;
		}

		/* Make the thread runnable and unlock the scheduling queue: */
		pthread->cycle++;
		_thr_umtx_wake(&pthread->cycle, 1);

		THR_THREAD_UNLOCK(curthread, pthread);
		if (mutex->m_owner == pthread)
			/* We're done; a valid owner was found. */
			break;
		else
			/* Get the next thread from the waiting queue: */
			pthread = TAILQ_NEXT(pthread, sqe);
	}

	if ((pthread == NULL) && (mutex->m_protocol == PTHREAD_PRIO_INHERIT))
		/* This mutex has no priority: */
		mutex->m_prio = 0;
	return (tid);
}

#if 0
/*
 * Dequeue a waiting thread from the head of a mutex queue in descending
 * priority order.
 */
static pthread_t
mutex_queue_deq(struct pthread_mutex *mutex)
{
	pthread_t pthread;

	while ((pthread = TAILQ_FIRST(&mutex->m_queue)) != NULL) {
		TAILQ_REMOVE(&mutex->m_queue, pthread, sqe);
		pthread->sflags &= ~THR_FLAGS_IN_SYNCQ;
	}

	return (pthread);
}
#endif

/*
 * Remove a waiting thread from a mutex queue in descending priority order.
 */
static void
mutex_queue_remove(pthread_mutex_t mutex, pthread_t pthread)
{
	if ((pthread->sflags & THR_FLAGS_IN_SYNCQ) != 0) {
		TAILQ_REMOVE(&mutex->m_queue, pthread, sqe);
		pthread->sflags &= ~THR_FLAGS_IN_SYNCQ;
	}
}

/*
 * Enqueue a waiting thread to a queue in descending priority order.
 */
static void
mutex_queue_enq(pthread_mutex_t mutex, pthread_t pthread)
{
	pthread_t tid = TAILQ_LAST(&mutex->m_queue, mutex_head);

	THR_ASSERT_NOT_IN_SYNCQ(pthread);
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
	pthread->sflags |= THR_FLAGS_IN_SYNCQ;
}
