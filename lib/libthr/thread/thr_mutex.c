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
#include <time.h>
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
static void		acquire_mutex(struct pthread_mutex *, struct pthread *);
static int		get_mcontested(pthread_mutex_t,
			    const struct timespec *);
static void		mutex_attach_to_next_pthread(struct pthread_mutex *);
static int		mutex_init(pthread_mutex_t *, int);
static int		mutex_lock_common(pthread_mutex_t *, int,
			    const struct timespec *);
static inline int	mutex_self_lock(pthread_mutex_t, int);
static inline int	mutex_unlock_common(pthread_mutex_t *, int);
static inline pthread_t	mutex_queue_deq(pthread_mutex_t);
static inline void	mutex_queue_remove(pthread_mutex_t, pthread_t);
static inline void	mutex_queue_enq(pthread_mutex_t, pthread_t);
static void		restore_prio_inheritance(struct pthread *);
static void		restore_prio_protection(struct pthread *);


static spinlock_t static_init_lock = _SPINLOCK_INITIALIZER;

static struct pthread_mutex_attr	static_mutex_attr =
    PTHREAD_MUTEXATTR_STATIC_INITIALIZER;
static pthread_mutexattr_t		static_mattr = &static_mutex_attr;

/* Single underscore versions provided for libc internal usage: */
__weak_reference(__pthread_mutex_trylock, pthread_mutex_trylock);
__weak_reference(__pthread_mutex_lock, pthread_mutex_lock);
__weak_reference(__pthread_mutex_unlock, pthread_mutex_unlock);

/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_init, pthread_mutex_init);
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_timedlock, pthread_mutex_timedlock);


/*
 * Reinitialize a private mutex; this is only used for internal mutexes.
 */
int
_mutex_reinit(pthread_mutex_t * mutex)
{
	int	ret = 0;

	if (*mutex == PTHREAD_MUTEX_INITIALIZER)
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
	struct pthread_mutex_attr default_attr = {PTHREAD_MUTEX_ERRORCHECK,
	    PTHREAD_PRIO_NONE, PTHREAD_MAX_PRIORITY, 0 };
	struct pthread_mutex_attr *attr;

	if (mutex_attr == NULL) {
		attr = &default_attr;
	} else {
		/*
		 * Check that the given mutex attribute is valid.
		 */
		if (((*mutex_attr)->m_type < PTHREAD_MUTEX_ERRORCHECK) ||
		    ((*mutex_attr)->m_type >= MUTEX_TYPE_MAX))
			return (EINVAL);
		else if (((*mutex_attr)->m_protocol < PTHREAD_PRIO_NONE) ||
		    ((*mutex_attr)->m_protocol > PTHREAD_MUTEX_RECURSIVE))
			return (EINVAL);
		attr = *mutex_attr;
	}
	if ((*mutex =
	    (pthread_mutex_t)malloc(sizeof(struct pthread_mutex))) == NULL)
		return (ENOMEM);
	memset((void *)(*mutex), 0, sizeof(struct pthread_mutex));

	/* Initialise the rest of the mutex: */
	TAILQ_INIT(&(*mutex)->m_queue);
	_MUTEX_INIT_LINK(*mutex);
	(*mutex)->m_protocol = attr->m_protocol;
	(*mutex)->m_flags = (attr->m_flags | MUTEX_FLAGS_INITED);
	(*mutex)->m_type = attr->m_type;
	if ((*mutex)->m_protocol == PTHREAD_PRIO_PROTECT)
		(*mutex)->m_prio = attr->m_ceiling;
	return (0);
}

int
_pthread_mutex_destroy(pthread_mutex_t * mutex)
{
	/*
	 * If this mutex was statically initialized, don't bother
	 * initializing it in order to destroy it immediately.
	 */
	if (*mutex == PTHREAD_MUTEX_INITIALIZER)
		return (0);

	/* Lock the mutex structure: */
	_SPINLOCK(&(*mutex)->lock);

	/*
	 * Check to see if this mutex is in use:
	 */
	if (((*mutex)->m_owner != NULL) ||
	    (TAILQ_FIRST(&(*mutex)->m_queue) != NULL) ||
	    ((*mutex)->m_refcount != 0)) {
		/* Unlock the mutex structure: */
		_SPINUNLOCK(&(*mutex)->lock);
		return (EBUSY);
	}

	/*
	 * Free the memory allocated for the mutex
	 * structure:
	 */
	_MUTEX_ASSERT_NOT_OWNED(*mutex);
	_SPINUNLOCK(&(*mutex)->lock);
	free(*mutex);

	/*
	 * Leave the caller's pointer NULL now that
	 * the mutex has been destroyed:
	 */
	*mutex = NULL;

	return (0);
}

static int
mutex_init(pthread_mutex_t *mutex, int private)
{
	pthread_mutexattr_t *pma;
	int error;

	error = 0;
	pma = private ? &static_mattr : NULL;
	_SPINLOCK(&static_init_lock);
	if (*mutex == PTHREAD_MUTEX_INITIALIZER)
		error = _pthread_mutex_init(mutex, pma);
	_SPINUNLOCK(&static_init_lock);
	return (error);
}

/*
 * Acquires a mutex for the current thread. The caller must
 * lock the mutex before calling this function.
 */
static void
acquire_mutex(struct pthread_mutex *mtx, struct pthread *ptd)
{
	mtx->m_owner = ptd;
	_MUTEX_ASSERT_NOT_OWNED(mtx);
	PTHREAD_LOCK(ptd);
	TAILQ_INSERT_TAIL(&ptd->mutexq, mtx, m_qe);
	PTHREAD_UNLOCK(ptd);
}

/*
 * Releases a mutex from the current thread. The owner must
 * lock the mutex. The next thread on the queue will be returned
 * locked by the current thread. The caller must take care to
 * unlock it.
 */
static void
mutex_attach_to_next_pthread(struct pthread_mutex *mtx)
{
	struct pthread *ptd;

	_MUTEX_ASSERT_IS_OWNED(mtx);
	TAILQ_REMOVE(&mtx->m_owner->mutexq, (mtx), m_qe);
	_MUTEX_INIT_LINK(mtx);

	/*
	 * Deque next thread waiting for this mutex and attach
	 * the mutex to it. The thread will already be locked.
	 */
	if ((ptd = mutex_queue_deq(mtx)) != NULL) {
		TAILQ_INSERT_TAIL(&ptd->mutexq, mtx, m_qe);
		ptd->data.mutex = NULL;
		PTHREAD_WAKE(ptd);
	}
	mtx->m_owner = ptd;
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if ((*mutex != PTHREAD_MUTEX_INITIALIZER) ||
	    (ret = mutex_init(mutex, 0)) == 0)
		ret = mutex_lock_common(mutex, 1, NULL);

	return (ret);
}

/*
 * Libc internal.
 */
int
_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking the mutex private (delete safe):
	 */
	if ((*mutex != PTHREAD_MUTEX_INITIALIZER) ||
	    (ret = mutex_init(mutex, 1)) == 0)
		ret = mutex_lock_common(mutex, 1, NULL);

	return (ret);
}

static int
mutex_lock_common(pthread_mutex_t * mutex, int nonblock,
    const struct timespec *abstime)
{
	int error;

	error = 0;
	PTHREAD_ASSERT((mutex != NULL) && (*mutex != NULL),
	    "Uninitialized mutex in mutex_lock_common");
	PTHREAD_ASSERT(((*mutex)->m_protocol >= PTHREAD_PRIO_NONE &&
	    (*mutex)->m_protocol <= PTHREAD_PRIO_PROTECT),
	    "Invalid mutex protocol"); 
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

retry:
	/*
	 * If the mutex is a priority protected mutex the thread's
	 * priority may not be higher than that of the mutex.
	 */
	if ((*mutex)->m_protocol == PTHREAD_PRIO_PROTECT &&
	    curthread->active_priority > (*mutex)->m_prio) {
		_SPINUNLOCK(&(*mutex)->lock);
		return (EINVAL);
	}
	if ((*mutex)->m_owner == NULL) {
		/*
		 * Mutex is currently unowned.
		 */
		acquire_mutex(*mutex, curthread);
	} else if ((*mutex)->m_owner == curthread) {
		/*
		 * Mutex is owned by curthread. We must test against
		 * certain conditions in such a case.
		 */
		if ((error = mutex_self_lock((*mutex), nonblock)) != 0) {
			_SPINUNLOCK(&(*mutex)->lock);
			return (error);
		}
	} else {
		if (nonblock) {
			error = EBUSY;
			goto out;
		}

		/*
		 * Another thread owns the mutex. This thread must
		 * wait for that thread to unlock the mutex. This
		 * thread must not return to the caller if it was
		 * interrupted by a signal.
		 */
		error = get_mcontested(*mutex, abstime);
		if (error == EINTR)
			goto retry;
		else if (error == ETIMEDOUT)
			goto out;
	}

	if ((*mutex)->m_type == PTHREAD_MUTEX_RECURSIVE)
		(*mutex)->m_data.m_count++;

	/*
	 * The mutex is now owned by curthread.
	 */
	PTHREAD_LOCK(curthread);

	/*
	 * The mutex's priority may have changed while waiting for it.
 	 */
	if ((*mutex)->m_protocol == PTHREAD_PRIO_PROTECT &&
	    curthread->active_priority > (*mutex)->m_prio) {
		mutex_attach_to_next_pthread(*mutex);
		if ((*mutex)->m_owner != NULL)
			PTHREAD_UNLOCK((*mutex)->m_owner);
		PTHREAD_UNLOCK(curthread);
		_SPINUNLOCK(&(*mutex)->lock);
		return (EINVAL);
	}

	switch ((*mutex)->m_protocol) {
	case PTHREAD_PRIO_INHERIT:
		curthread->prio_inherit_count++;
		break;
	case PTHREAD_PRIO_PROTECT:
		PTHREAD_ASSERT((curthread->active_priority <=
		    (*mutex)->m_prio), "priority protection violation");
		curthread->prio_protect_count++;
		if ((*mutex)->m_prio > curthread->active_priority) {
			curthread->inherited_priority = (*mutex)->m_prio;
			curthread->active_priority = (*mutex)->m_prio;
		}
		break;
	default:
		/* Nothing */
		break;
	}
	PTHREAD_UNLOCK(curthread);
out:
	_SPINUNLOCK(&(*mutex)->lock);
	return (error);
}

/*
 * Caller must lock thread.
 */
void
adjust_prio_inheritance(struct pthread *ptd)
{
	struct pthread_mutex *tempMtx;
	struct pthread	     *tempTd;

	/*
	 * Scan owned mutexes's wait queue and execute at the
	 * higher of thread's current priority or the priority of
	 * the highest priority thread waiting on any of the the
	 * mutexes the thread owns. Note: the highest priority thread
	 * on a queue is always at the head of the queue.
	 */
	TAILQ_FOREACH(tempMtx, &ptd->mutexq, m_qe) {
		if (tempMtx->m_protocol != PTHREAD_PRIO_INHERIT)
			continue;

		/*
		 * XXX LOR with respect to tempMtx and ptd.
		 * Order should be: 1. mutex
		 *		    2. pthread
		 */
		_SPINLOCK(&tempMtx->lock);

		tempTd = TAILQ_FIRST(&tempMtx->m_queue);
		if (tempTd != NULL) {
			PTHREAD_LOCK(tempTd);
			if (tempTd->active_priority > ptd->active_priority) {
				ptd->inherited_priority =
				    tempTd->active_priority;
				ptd->active_priority =
				    tempTd->active_priority;
			}
			PTHREAD_UNLOCK(tempTd);
		}
		_SPINUNLOCK(&tempMtx->lock);
	}
}

/*
 * Caller must lock thread.
 */
static void
restore_prio_inheritance(struct pthread *ptd)
{
	ptd->inherited_priority = PTHREAD_MIN_PRIORITY;
	ptd->active_priority = ptd->base_priority;
	adjust_prio_inheritance(ptd);
}

/*
 * Caller must lock thread.
 */
void
adjust_prio_protection(struct pthread *ptd)
{
	struct pthread_mutex *tempMtx;

	/*
	 * The thread shall execute at the higher of its priority or
	 * the highest priority ceiling of all the priority protection
	 * mutexes it owns.
	 */
	TAILQ_FOREACH(tempMtx, &ptd->mutexq, m_qe) {
		if (tempMtx->m_protocol != PTHREAD_PRIO_PROTECT)
			continue;
		if (ptd->active_priority < tempMtx->m_prio) {
			ptd->inherited_priority = tempMtx->m_prio;
			ptd->active_priority = tempMtx->m_prio;
		}
	}
}

/*
 * Caller must lock thread.
 */
static void
restore_prio_protection(struct pthread *ptd)
{
	ptd->inherited_priority = PTHREAD_MIN_PRIORITY;
	ptd->active_priority = ptd->base_priority;
	adjust_prio_protection(ptd);
}

int
__pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	if (_thread_initial == NULL)
		_thread_init();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if ((*mutex != PTHREAD_MUTEX_INITIALIZER) ||
	    ((ret = mutex_init(mutex, 0)) == 0))
		ret = mutex_lock_common(mutex, 0, NULL);

	return (ret);
}

/*
 * Libc internal.
 */
int
_pthread_mutex_lock(pthread_mutex_t *mutex)
{
	int	ret = 0;

	if (_thread_initial == NULL)
		_thread_init();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	if ((*mutex != PTHREAD_MUTEX_INITIALIZER) ||
	    ((ret = mutex_init(mutex, 1)) == 0))
		ret = mutex_lock_common(mutex, 0, NULL);

	return (ret);
}

int
_pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime)
{
	int error;

	error = 0;
	if (_thread_initial == NULL)
		_thread_init();

	/*
	 * Initialize it if it's a valid statically inited mutex.
	 */
	if ((*mutex != PTHREAD_MUTEX_INITIALIZER) ||
	    ((error = mutex_init(mutex, 0)) == 0))
		error = mutex_lock_common(mutex, 0, abstime);

	PTHREAD_ASSERT(error != EINTR, "According to SUSv3 this function shall not return an error code of EINTR");
	return (error);
}

int
__pthread_mutex_unlock(pthread_mutex_t * mutex)
{
	return (mutex_unlock_common(mutex, /* add reference */ 0));
}

/*
 * Libc internal
 */
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

/*
 * Caller must lock mutex and then disable signals and lock curthread.
 */
static inline int
mutex_self_lock(pthread_mutex_t mutex, int noblock)
{
	switch (mutex->m_type) {
	case PTHREAD_MUTEX_ERRORCHECK:
		/*
		 * POSIX specifies that mutexes should return EDEADLK if a
		 * recursive lock is detected.
		 */
		if (noblock)
			return (EBUSY);
		return (EDEADLK); 
		break;

	case PTHREAD_MUTEX_NORMAL:
		/*
		 * What SS2 define as a 'normal' mutex.  Intentionally
		 * deadlock on attempts to get a lock you already own.
		 */
		if (noblock)
			return (EBUSY);
		curthread->isdeadlocked = 1;
		_SPINUNLOCK(&(mutex)->lock);
		_thread_suspend(curthread, NULL);
		PANIC("Shouldn't resume here?\n");
		break;

	default:
		/* Do Nothing */
		break;
	}
	return (0);
}

static inline int
mutex_unlock_common(pthread_mutex_t * mutex, int add_reference)
{
	/*
	 * Error checking.
	 */
	if ((*mutex)->m_owner != curthread)
		return (EPERM);
	PTHREAD_ASSERT(((*mutex)->m_protocol >= PTHREAD_PRIO_NONE &&
	    (*mutex)->m_protocol <= PTHREAD_PRIO_PROTECT),
	    "Invalid mutex protocol"); 

	_SPINLOCK(&(*mutex)->lock);
	if ((*mutex)->m_type == PTHREAD_MUTEX_RECURSIVE) {
		(*mutex)->m_data.m_count--;
		PTHREAD_ASSERT((*mutex)->m_data.m_count >= 0,
		    "The mutex recurse count cannot be less than zero");
		if ((*mutex)->m_data.m_count > 0) {
			_SPINUNLOCK(&(*mutex)->lock);
			return (0);
		}
	}

	/*
	 * Release the mutex from this thread and attach it to
	 * the next thread in the queue, if there is one waiting.
	 */
	PTHREAD_LOCK(curthread);
	mutex_attach_to_next_pthread(*mutex);
	if ((*mutex)->m_owner != NULL)
		PTHREAD_UNLOCK((*mutex)->m_owner);
	if (add_reference != 0) {
		/* Increment the reference count: */
		(*mutex)->m_refcount++;
	}
	_SPINUNLOCK(&(*mutex)->lock);

	/*
	 * Fix priority of the thread that just released the mutex.
	 */
	switch ((*mutex)->m_protocol) {
	case PTHREAD_PRIO_INHERIT:
		curthread->prio_inherit_count--;
		PTHREAD_ASSERT(curthread->prio_inherit_count >= 0,
		    "priority inheritance counter cannot be less than zero");
		restore_prio_inheritance(curthread);
		if (curthread->prio_protect_count > 0)
			restore_prio_protection(curthread);
		break;
	case PTHREAD_PRIO_PROTECT:
		curthread->prio_protect_count--;
		PTHREAD_ASSERT(curthread->prio_protect_count >= 0,
		    "priority protection counter cannot be less than zero");
		restore_prio_protection(curthread);
		if (curthread->prio_inherit_count > 0)
			restore_prio_inheritance(curthread);
		break;
	default:
		/* Nothing */
		break;
	}
	PTHREAD_UNLOCK(curthread);
	return (0);
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

	mutex = pthread->data.mutex;
	if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0) {

		mutex_queue_remove(mutex, pthread);

		/* This thread is no longer waiting for the mutex: */
		pthread->data.mutex = NULL;

	}
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
		PTHREAD_LOCK(pthread);
		TAILQ_REMOVE(&mutex->m_queue, pthread, sqe);
		pthread->flags &= ~PTHREAD_FLAGS_IN_MUTEXQ;

		/*
		 * Only exit the loop if the thread hasn't been
		 * asynchronously cancelled.
		 */
		if (pthread->cancelmode == M_ASYNC &&
		    pthread->cancellation != CS_NULL)
			continue;
		else
			break;
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
		_thread_printf(2, "Thread (%s:%ld) already on condq\n",
		    pthread->name, pthread->thr_id);
	if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0)
		_thread_printf(2, "Thread (%s:%ld) already on mutexq\n",
		    pthread->name, pthread->thr_id);
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
	if (mutex->m_protocol == PTHREAD_PRIO_INHERIT &&
	    pthread == TAILQ_FIRST(&mutex->m_queue)) {
		PTHREAD_LOCK(mutex->m_owner);
		if (pthread->active_priority >
		    mutex->m_owner->active_priority) {
			mutex->m_owner->inherited_priority =
			    pthread->active_priority;
			mutex->m_owner->active_priority =
			    pthread->active_priority;
		}
		PTHREAD_UNLOCK(mutex->m_owner);
	}
	pthread->flags |= PTHREAD_FLAGS_IN_MUTEXQ;
}

/*
 * Caller must lock mutex and pthread.
 */
void
readjust_priorities(struct pthread *pthread, struct pthread_mutex *mtx)
{
	if ((pthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0) {
		PTHREAD_ASSERT(mtx != NULL,
		    "mutex is NULL when it should not be");
		mutex_queue_remove(mtx, pthread);
		mutex_queue_enq(mtx, pthread);
		PTHREAD_LOCK(mtx->m_owner);
		adjust_prio_inheritance(mtx->m_owner);
		if (mtx->m_owner->prio_protect_count > 0)
			adjust_prio_protection(mtx->m_owner);
		PTHREAD_UNLOCK(mtx->m_owner);
	}
	if (pthread->prio_inherit_count > 0)
		adjust_prio_inheritance(pthread);
	if (pthread->prio_protect_count > 0)
		adjust_prio_protection(pthread);
}

/*
 * Returns with the lock owned and on the thread's mutexq. If
 * the mutex is currently owned by another thread it will sleep
 * until it is available.
 */
static int
get_mcontested(pthread_mutex_t mutexp, const struct timespec *abstime)
{
	int error;

	/*
	 * If the timeout is invalid this thread is not allowed
	 * to block;
	 */
	if (abstime != NULL) {
		if (abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000)
			return (EINVAL);
	}

	/*
	 * Put this thread on the mutex's list of waiting threads.
	 * The lock on the thread ensures atomic (as far as other
	 * threads are concerned) setting of the thread state with
	 * it's status on the mutex queue.
	 */
	PTHREAD_LOCK(curthread);
	mutex_queue_enq(mutexp, curthread);
	do {
		if (curthread->cancelmode == M_ASYNC &&
		    curthread->cancellation != CS_NULL) {
			mutex_queue_remove(mutexp, curthread);
			PTHREAD_UNLOCK(curthread);
			_SPINUNLOCK(&mutexp->lock);
			pthread_testcancel();
		}
		curthread->data.mutex = mutexp;
		PTHREAD_UNLOCK(curthread);
		_SPINUNLOCK(&mutexp->lock);
		error = _thread_suspend(curthread, abstime);
		if (error != 0 && error != ETIMEDOUT && error != EINTR)
			PANIC("Cannot suspend on mutex.");
		_SPINLOCK(&mutexp->lock);
		PTHREAD_LOCK(curthread);
		if (error == ETIMEDOUT) {
			/*
			 * Between the timeout and when the mutex was
			 * locked the previous owner may have released
			 * the mutex to this thread. Or not.
			 */
			if (mutexp->m_owner == curthread)
				error = 0;
			else
				_mutex_lock_backout(curthread);
		}
	} while ((curthread->flags & PTHREAD_FLAGS_IN_MUTEXQ) != 0);
	PTHREAD_UNLOCK(curthread);
	return (error);
}
