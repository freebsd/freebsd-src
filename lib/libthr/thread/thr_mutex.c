/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
 * Copyright (c) 2006 David Xu <davidxu@freebsd.org>.
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

#include "namespace.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <pthread.h>
#include "un-namespace.h"

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
#else
#define MUTEX_INIT_LINK(m)
#define MUTEX_ASSERT_IS_OWNED(m)
#define MUTEX_ASSERT_NOT_OWNED(m)
#endif

/*
 * For adaptive mutexes, how many times to spin doing trylock2
 * before entering the kernel to block
 */
#define MUTEX_ADAPTIVE_SPINS	200

/*
 * Prototypes
 */
int	__pthread_mutex_init(pthread_mutex_t *mutex,
		const pthread_mutexattr_t *mutex_attr);
int	__pthread_mutex_trylock(pthread_mutex_t *mutex);
int	__pthread_mutex_lock(pthread_mutex_t *mutex);
int	__pthread_mutex_timedlock(pthread_mutex_t *mutex,
		const struct timespec *abstime);
int	__pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count);
int	_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count);

static int	mutex_self_trylock(pthread_mutex_t);
static int	mutex_self_lock(pthread_mutex_t,
				const struct timespec *abstime);
static int	mutex_unlock_common(pthread_mutex_t *);

__weak_reference(__pthread_mutex_init, pthread_mutex_init);
__weak_reference(__pthread_mutex_lock, pthread_mutex_lock);
__weak_reference(__pthread_mutex_timedlock, pthread_mutex_timedlock);
__weak_reference(__pthread_mutex_trylock, pthread_mutex_trylock);

/* Single underscore versions provided for libc internal usage: */
/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_unlock, pthread_mutex_unlock);

__weak_reference(_pthread_mutex_getprioceiling, pthread_mutex_getprioceiling);
__weak_reference(_pthread_mutex_setprioceiling, pthread_mutex_setprioceiling);

__weak_reference(__pthread_mutex_setspinloops_np, pthread_mutex_setspinloops_np);
__weak_reference(_pthread_mutex_getspinloops_np, pthread_mutex_getspinloops_np);

__weak_reference(__pthread_mutex_setyieldloops_np, pthread_mutex_setyieldloops_np);
__weak_reference(_pthread_mutex_getyieldloops_np, pthread_mutex_getyieldloops_np);

static int
mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr, int private,
    void *(calloc_cb)(size_t, size_t))
{
	const struct pthread_mutex_attr *attr;
	struct pthread_mutex *pmutex;

	if (mutex_attr == NULL) {
		attr = &_pthread_mutexattr_default;
	} else {
		attr = *mutex_attr;
		if (attr->m_type < PTHREAD_MUTEX_ERRORCHECK ||
		    attr->m_type >= PTHREAD_MUTEX_TYPE_MAX)
			return (EINVAL);
		if (attr->m_protocol < PTHREAD_PRIO_NONE ||
		    attr->m_protocol > PTHREAD_PRIO_PROTECT)
			return (EINVAL);
	}
	if ((pmutex = (pthread_mutex_t)
		calloc_cb(1, sizeof(struct pthread_mutex))) == NULL)
		return (ENOMEM);

	pmutex->m_type = attr->m_type;
	pmutex->m_owner = NULL;
	pmutex->m_flags = attr->m_flags | MUTEX_FLAGS_INITED;
	if (private)
		pmutex->m_flags |= MUTEX_FLAGS_PRIVATE;
	pmutex->m_count = 0;
	pmutex->m_refcount = 0;
	pmutex->m_spinloops = 0;
	pmutex->m_yieldloops = 0;
	MUTEX_INIT_LINK(pmutex);
	switch(attr->m_protocol) {
	case PTHREAD_PRIO_INHERIT:
		pmutex->m_lock.m_owner = UMUTEX_UNOWNED;
		pmutex->m_lock.m_flags = UMUTEX_PRIO_INHERIT;
		break;
	case PTHREAD_PRIO_PROTECT:
		pmutex->m_lock.m_owner = UMUTEX_CONTESTED;
		pmutex->m_lock.m_flags = UMUTEX_PRIO_PROTECT;
		pmutex->m_lock.m_ceilings[0] = attr->m_ceiling;
		break;
	case PTHREAD_PRIO_NONE:
		pmutex->m_lock.m_owner = UMUTEX_UNOWNED;
		pmutex->m_lock.m_flags = 0;
	}

	if (pmutex->m_type == PTHREAD_MUTEX_ADAPTIVE_NP) {
		pmutex->m_spinloops =
		    _thr_spinloops ? _thr_spinloops: MUTEX_ADAPTIVE_SPINS;
		pmutex->m_yieldloops = _thr_yieldloops;
	}

	*mutex = pmutex;
	return (0);
}

static int
init_static(struct pthread *thread, pthread_mutex_t *mutex)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);

	if (*mutex == NULL)
		ret = mutex_init(mutex, NULL, 0, calloc);
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
		ret = mutex_init(mutex, NULL, 1, calloc);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_mutex_static_lock);

	return (ret);
}

static void
set_inherited_priority(struct pthread *curthread, struct pthread_mutex *m)
{
	struct pthread_mutex *m2;

	m2 = TAILQ_LAST(&curthread->pp_mutexq, mutex_queue);
	if (m2 != NULL)
		m->m_lock.m_ceilings[1] = m2->m_lock.m_ceilings[0];
	else
		m->m_lock.m_ceilings[1] = -1;
}

int
_pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init(mutex, mutex_attr, 1, calloc);
}

int
__pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	return mutex_init(mutex, mutex_attr, 0, calloc);
}

/* This function is used internally by malloc. */
int
_pthread_mutex_init_calloc_cb(pthread_mutex_t *mutex,
    void *(calloc_cb)(size_t, size_t))
{
	static const struct pthread_mutex_attr attr = {
		.m_type = PTHREAD_MUTEX_NORMAL,
		.m_protocol = PTHREAD_PRIO_NONE,
		.m_ceiling = 0,
		.m_flags = 0
	};
	static const struct pthread_mutex_attr *pattr = &attr;

	return mutex_init(mutex, (pthread_mutexattr_t *)&pattr, 0, calloc_cb);
}

void
_mutex_fork(struct pthread *curthread)
{
	struct pthread_mutex *m;

	/*
	 * Fix mutex ownership for child process.
	 * note that process shared mutex should not
	 * be inherited because owner is forking thread
	 * which is in parent process, they should be
	 * removed from the owned mutex list, current,
	 * process shared mutex is not supported, so I
	 * am not worried.
	 */

	TAILQ_FOREACH(m, &curthread->mutexq, m_qe)
		m->m_lock.m_owner = TID(curthread);
	TAILQ_FOREACH(m, &curthread->pp_mutexq, m_qe)
		m->m_lock.m_owner = TID(curthread) | UMUTEX_CONTESTED;
}

int
_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	pthread_mutex_t m;
	uint32_t id;
	int ret = 0;

	if (__predict_false(*mutex == NULL))
		ret = EINVAL;
	else {
		id = TID(curthread);

		/*
		 * Try to lock the mutex structure, we only need to
		 * try once, if failed, the mutex is in used.
		 */
		ret = _thr_umutex_trylock(&(*mutex)->m_lock, id);
		if (ret)
			return (ret);
		m  = *mutex;
		/*
		 * Check mutex other fields to see if this mutex is
		 * in use. Mostly for prority mutex types, or there
		 * are condition variables referencing it.
		 */
		if (m->m_owner != NULL || m->m_refcount != 0) {
			if (m->m_lock.m_flags & UMUTEX_PRIO_PROTECT)
				set_inherited_priority(curthread, m);
			_thr_umutex_unlock(&m->m_lock, id);
			ret = EBUSY;
		} else {
			/*
			 * Save a pointer to the mutex so it can be free'd
			 * and set the caller's pointer to NULL.
			 */
			*mutex = NULL;

			if (m->m_lock.m_flags & UMUTEX_PRIO_PROTECT)
				set_inherited_priority(curthread, m);
			_thr_umutex_unlock(&m->m_lock, id);

			MUTEX_ASSERT_NOT_OWNED(m);
			free(m);
		}
	}

	return (ret);
}


#define ENQUEUE_MUTEX(curthread, m)  					\
	do {								\
		(m)->m_owner = curthread;				\
		/* Add to the list of owned mutexes: */			\
		MUTEX_ASSERT_NOT_OWNED((m));				\
		if (((m)->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)	\
			TAILQ_INSERT_TAIL(&curthread->mutexq, (m), m_qe);\
		else							\
			TAILQ_INSERT_TAIL(&curthread->pp_mutexq, (m), m_qe);\
	} while (0)

static int
mutex_trylock_common(struct pthread *curthread, pthread_mutex_t *mutex)
{
	struct pthread_mutex *m;
	uint32_t id;
	int ret;

	id = TID(curthread);
	m = *mutex;
	ret = _thr_umutex_trylock(&m->m_lock, id);
	if (ret == 0) {
		ENQUEUE_MUTEX(curthread, m);
	} else if (m->m_owner == curthread) {
		ret = mutex_self_trylock(m);
	} /* else {} */

	return (ret);
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if (__predict_false(*mutex == NULL)) {
		ret = init_static(curthread, mutex);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_trylock_common(curthread, mutex));
}

int
_pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct pthread	*curthread = _get_curthread();
	int	ret;

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking the mutex private (delete safe):
	 */
	if (__predict_false(*mutex == NULL)) {
		ret = init_static_private(curthread, mutex);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_trylock_common(curthread, mutex));
}

static int
mutex_lock_common(struct pthread *curthread, pthread_mutex_t *mutex,
	const struct timespec * abstime)
{
	struct  timespec ts, ts2;
	struct	pthread_mutex *m;
	uint32_t	id;
	int	ret;
	int	count;

	id = TID(curthread);
	m = *mutex;
	ret = _thr_umutex_trylock2(&m->m_lock, id);
	if (ret == 0) {
		ENQUEUE_MUTEX(curthread, m);
	} else if (m->m_owner == curthread) {
		ret = mutex_self_lock(m, abstime);
	} else {
		/*
		 * For adaptive mutexes, spin for a bit in the expectation
		 * that if the application requests this mutex type then
		 * the lock is likely to be released quickly and it is
		 * faster than entering the kernel
		 */
		if (m->m_lock.m_flags & UMUTEX_PRIO_PROTECT)
			goto sleep_in_kernel;

		if (!_thr_is_smp)
			goto yield_loop;

		count = m->m_spinloops;
		while (count--) {
			if (m->m_lock.m_owner == UMUTEX_UNOWNED) {
				ret = _thr_umutex_trylock2(&m->m_lock, id);
				if (ret == 0)
					goto done;
			}
			CPU_SPINWAIT;
		}

yield_loop:
		count = m->m_yieldloops;
		while (count--) {
			_sched_yield();
			ret = _thr_umutex_trylock2(&m->m_lock, id);
			if (ret == 0)
				goto done;
		}

sleep_in_kernel:
		if (abstime == NULL) {
			ret = __thr_umutex_lock(&m->m_lock);
		} else if (__predict_false(
			   abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			   abstime->tv_nsec >= 1000000000)) {
			ret = EINVAL;
		} else {
			clock_gettime(CLOCK_REALTIME, &ts);
			TIMESPEC_SUB(&ts2, abstime, &ts);
			ret = __thr_umutex_timedlock(&m->m_lock, &ts2);
			/*
			 * Timed out wait is not restarted if
			 * it was interrupted, not worth to do it.
			 */
			if (ret == EINTR)
				ret = ETIMEDOUT;
		}
done:
		if (ret == 0)
			ENQUEUE_MUTEX(curthread, m);
	}
	return (ret);
}

int
__pthread_mutex_lock(pthread_mutex_t *m)
{
	struct pthread *curthread;
	int	ret;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if (__predict_false(*m == NULL)) {
		ret = init_static(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, NULL));
}

int
_pthread_mutex_lock(pthread_mutex_t *m)
{
	struct pthread *curthread;
	int	ret;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	if (__predict_false(*m == NULL)) {
		ret = init_static_private(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, NULL));
}

int
__pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abstime)
{
	struct pthread *curthread;
	int	ret;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization:
	 */
	if (__predict_false(*m == NULL)) {
		ret = init_static(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, abstime));
}

int
_pthread_mutex_timedlock(pthread_mutex_t *m, const struct timespec *abstime)
{
	struct pthread	*curthread;
	int	ret;

	_thr_check_init();

	curthread = _get_curthread();

	/*
	 * If the mutex is statically initialized, perform the dynamic
	 * initialization marking it private (delete safe):
	 */
	if (__predict_false(*m == NULL)) {
		ret = init_static_private(curthread, m);
		if (__predict_false(ret))
			return (ret);
	}
	return (mutex_lock_common(curthread, m, abstime));
}

int
_pthread_mutex_unlock(pthread_mutex_t *m)
{
	return (mutex_unlock_common(m));
}

int
_mutex_cv_lock(pthread_mutex_t *m, int count)
{
	int	ret;

	ret = mutex_lock_common(_get_curthread(), m, NULL);
	if (ret == 0) {
		(*m)->m_refcount--;
		(*m)->m_count += count;
	}
	return (ret);
}

static int
mutex_self_trylock(pthread_mutex_t m)
{
	int	ret;

	switch (m->m_type) {
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
mutex_self_lock(pthread_mutex_t m, const struct timespec *abstime)
{
	struct timespec	ts1, ts2;
	int	ret;

	switch (m->m_type) {
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_ADAPTIVE_NP:
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
mutex_unlock_common(pthread_mutex_t *mutex)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *m;
	uint32_t id;

	if (__predict_false((m = *mutex) == NULL))
		return (EINVAL);

	/*
	 * Check if the running thread is not the owner of the mutex.
	 */
	if (__predict_false(m->m_owner != curthread))
		return (EPERM);

	id = TID(curthread);
	if (__predict_false(
		m->m_type == PTHREAD_MUTEX_RECURSIVE &&
		m->m_count > 0)) {
		m->m_count--;
	} else {
		m->m_owner = NULL;
		/* Remove the mutex from the threads queue. */
		MUTEX_ASSERT_IS_OWNED(m);
		if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
			TAILQ_REMOVE(&curthread->mutexq, m, m_qe);
		else {
			TAILQ_REMOVE(&curthread->pp_mutexq, m, m_qe);
			set_inherited_priority(curthread, m);
		}
		MUTEX_INIT_LINK(m);
		_thr_umutex_unlock(&m->m_lock, id);
	}
	return (0);
}

int
_mutex_cv_unlock(pthread_mutex_t *mutex, int *count)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *m;

	if (__predict_false((m = *mutex) == NULL))
		return (EINVAL);

	/*
	 * Check if the running thread is not the owner of the mutex.
	 */
	if (__predict_false(m->m_owner != curthread))
		return (EPERM);

	/*
	 * Clear the count in case this is a recursive mutex.
	 */
	*count = m->m_count;
	m->m_refcount++;
	m->m_count = 0;
	m->m_owner = NULL;
	/* Remove the mutex from the threads queue. */
	MUTEX_ASSERT_IS_OWNED(m);
	if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		TAILQ_REMOVE(&curthread->mutexq, m, m_qe);
	else {
		TAILQ_REMOVE(&curthread->pp_mutexq, m, m_qe);
		set_inherited_priority(curthread, m);
	}
	MUTEX_INIT_LINK(m);
	_thr_umutex_unlock(&m->m_lock, TID(curthread));
	return (0);
}

void
_mutex_unlock_private(pthread_t pthread)
{
	struct pthread_mutex	*m, *m_next;

	TAILQ_FOREACH_SAFE(m, &pthread->mutexq, m_qe, m_next) {
		if ((m->m_flags & MUTEX_FLAGS_PRIVATE) != 0)
			_pthread_mutex_unlock(&m);
	}
}

int
_pthread_mutex_getprioceiling(pthread_mutex_t *mutex,
			      int *prioceiling)
{
	int ret;

	if (*mutex == NULL)
		ret = EINVAL;
	else if (((*mutex)->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		ret = EINVAL;
	else {
		*prioceiling = (*mutex)->m_lock.m_ceilings[0];
		ret = 0;
	}

	return(ret);
}

int
_pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
			      int ceiling, int *old_ceiling)
{
	struct pthread *curthread = _get_curthread();
	struct pthread_mutex *m, *m1, *m2;
	int ret;

	m = *mutex;
	if (m == NULL || (m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		return (EINVAL);

	ret = __thr_umutex_set_ceiling(&m->m_lock, ceiling, old_ceiling);
	if (ret != 0)
		return (ret);

	if (m->m_owner == curthread) {
		MUTEX_ASSERT_IS_OWNED(m);
		m1 = TAILQ_PREV(m, mutex_queue, m_qe);
		m2 = TAILQ_NEXT(m, m_qe);
		if ((m1 != NULL && m1->m_lock.m_ceilings[0] > (u_int)ceiling) ||
		    (m2 != NULL && m2->m_lock.m_ceilings[0] < (u_int)ceiling)) {
			TAILQ_REMOVE(&curthread->pp_mutexq, m, m_qe);
			TAILQ_FOREACH(m2, &curthread->pp_mutexq, m_qe) {
				if (m2->m_lock.m_ceilings[0] > (u_int)ceiling) {
					TAILQ_INSERT_BEFORE(m2, m, m_qe);
					return (0);
				}
			}
			TAILQ_INSERT_TAIL(&curthread->pp_mutexq, m, m_qe);
		}
	}
	return (0);
}

int
_pthread_mutex_getspinloops_np(pthread_mutex_t *mutex, int *count)
{
	if (*mutex == NULL)
		return (0);
	return (*mutex)->m_spinloops;
}

int
_pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	if (__predict_false(*mutex == NULL)) {
		ret = init_static_private(curthread, mutex);
		if (__predict_false(ret))
			return (ret);
	}
	(*mutex)->m_spinloops = count;
	return (0);
}

int
__pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	if (__predict_false(*mutex == NULL)) {
		ret = init_static(curthread, mutex);
		if (__predict_false(ret))
			return (ret);
	}
	(*mutex)->m_spinloops = count;
	return (0);
}

int
_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count)
{
	if (*mutex == NULL)
		return (0);
	return (*mutex)->m_yieldloops;
}

int
_pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	if (__predict_false(*mutex == NULL)) {
		ret = init_static_private(curthread, mutex);
		if (__predict_false(ret))
			return (ret);
	}
	(*mutex)->m_yieldloops = count;
	return (0);
}

int
__pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	if (__predict_false(*mutex == NULL)) {
		ret = init_static(curthread, mutex);
		if (__predict_false(ret))
			return (ret);
	}
	(*mutex)->m_yieldloops = count;
	return (0);
}
