/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
 * Copyright (c) 2006 David Xu <davidxu@freebsd.org>.
 * Copyright (c) 2015 The FreeBSD Foundation
 *
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdbool.h>
#include "namespace.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <pthread.h>
#include <pthread_np.h>
#include "un-namespace.h"

#include "thr_private.h"

_Static_assert(sizeof(struct pthread_mutex) <= PAGE_SIZE,
    "pthread_mutex is too large for off-page");

/*
 * For adaptive mutexes, how many times to spin doing trylock2
 * before entering the kernel to block
 */
#define MUTEX_ADAPTIVE_SPINS	2000

/*
 * Prototypes
 */
int	__pthread_mutex_init(pthread_mutex_t *mutex,
		const pthread_mutexattr_t *mutex_attr);
int	__pthread_mutex_trylock(pthread_mutex_t *mutex);
int	__pthread_mutex_lock(pthread_mutex_t *mutex);
int	__pthread_mutex_timedlock(pthread_mutex_t *mutex,
		const struct timespec *abstime);
int	_pthread_mutex_init_calloc_cb(pthread_mutex_t *mutex,
    		void *(calloc_cb)(size_t, size_t));
int	_pthread_mutex_getspinloops_np(pthread_mutex_t *mutex, int *count);
int	_pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count);
int	__pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count);
int	_pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count);
int	_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count);
int	__pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count);

static int	mutex_self_trylock(pthread_mutex_t);
static int	mutex_self_lock(pthread_mutex_t,
				const struct timespec *abstime);
static int	mutex_unlock_common(struct pthread_mutex *, int, int *);
static int	mutex_lock_sleep(struct pthread *, pthread_mutex_t,
				const struct timespec *);

__weak_reference(__pthread_mutex_init, pthread_mutex_init);
__strong_reference(__pthread_mutex_init, _pthread_mutex_init);
__weak_reference(__pthread_mutex_lock, pthread_mutex_lock);
__strong_reference(__pthread_mutex_lock, _pthread_mutex_lock);
__weak_reference(__pthread_mutex_timedlock, pthread_mutex_timedlock);
__strong_reference(__pthread_mutex_timedlock, _pthread_mutex_timedlock);
__weak_reference(__pthread_mutex_trylock, pthread_mutex_trylock);
__strong_reference(__pthread_mutex_trylock, _pthread_mutex_trylock);

/* Single underscore versions provided for libc internal usage: */
/* No difference between libc and application usage of these: */
__weak_reference(_pthread_mutex_destroy, pthread_mutex_destroy);
__weak_reference(_pthread_mutex_unlock, pthread_mutex_unlock);

__weak_reference(_pthread_mutex_getprioceiling, pthread_mutex_getprioceiling);
__weak_reference(_pthread_mutex_setprioceiling, pthread_mutex_setprioceiling);

__weak_reference(__pthread_mutex_setspinloops_np, pthread_mutex_setspinloops_np);
__strong_reference(__pthread_mutex_setspinloops_np, _pthread_mutex_setspinloops_np);
__weak_reference(_pthread_mutex_getspinloops_np, pthread_mutex_getspinloops_np);

__weak_reference(__pthread_mutex_setyieldloops_np, pthread_mutex_setyieldloops_np);
__strong_reference(__pthread_mutex_setyieldloops_np, _pthread_mutex_setyieldloops_np);
__weak_reference(_pthread_mutex_getyieldloops_np, pthread_mutex_getyieldloops_np);
__weak_reference(_pthread_mutex_isowned_np, pthread_mutex_isowned_np);

static void
mutex_init_link(struct pthread_mutex *m)
{

#if defined(_PTHREADS_INVARIANTS)
	m->m_qe.tqe_prev = NULL;
	m->m_qe.tqe_next = NULL;
	m->m_pqe.tqe_prev = NULL;
	m->m_pqe.tqe_next = NULL;
#endif
}

static void
mutex_assert_is_owned(struct pthread_mutex *m)
{

#if defined(_PTHREADS_INVARIANTS)
	if (__predict_false(m->m_qe.tqe_prev == NULL)) {
		char msg[128];
		snprintf(msg, sizeof(msg),
		    "mutex %p own %#x %#x is not on list %p %p",
		    m, m->m_lock.m_owner, m->m_owner, m->m_qe.tqe_prev,
		    m->m_qe.tqe_next);
		PANIC(msg);
	}
#endif
}

static void
mutex_assert_not_owned(struct pthread_mutex *m)
{

#if defined(_PTHREADS_INVARIANTS)
	if (__predict_false(m->m_qe.tqe_prev != NULL ||
	    m->m_qe.tqe_next != NULL)) {
		char msg[128];
		snprintf(msg, sizeof(msg),
		    "mutex %p own %#x %#x is on list %p %p",
		    m, m->m_lock.m_owner, m->m_owner, m->m_qe.tqe_prev,
		    m->m_qe.tqe_next);
		PANIC(msg);
	}
#endif
}

static int
is_pshared_mutex(struct pthread_mutex *m)
{

	return ((m->m_lock.m_flags & USYNC_PROCESS_SHARED) != 0);
}

static int
mutex_check_attr(const struct pthread_mutex_attr *attr)
{

	if (attr->m_type < PTHREAD_MUTEX_ERRORCHECK ||
	    attr->m_type >= PTHREAD_MUTEX_TYPE_MAX)
		return (EINVAL);
	if (attr->m_protocol < PTHREAD_PRIO_NONE ||
	    attr->m_protocol > PTHREAD_PRIO_PROTECT)
		return (EINVAL);
	return (0);
}

static void
mutex_init_body(struct pthread_mutex *pmutex,
    const struct pthread_mutex_attr *attr)
{

	pmutex->m_flags = attr->m_type;
	pmutex->m_owner = 0;
	pmutex->m_count = 0;
	pmutex->m_spinloops = 0;
	pmutex->m_yieldloops = 0;
	mutex_init_link(pmutex);
	switch (attr->m_protocol) {
	case PTHREAD_PRIO_NONE:
		pmutex->m_lock.m_owner = UMUTEX_UNOWNED;
		pmutex->m_lock.m_flags = 0;
		break;
	case PTHREAD_PRIO_INHERIT:
		pmutex->m_lock.m_owner = UMUTEX_UNOWNED;
		pmutex->m_lock.m_flags = UMUTEX_PRIO_INHERIT;
		break;
	case PTHREAD_PRIO_PROTECT:
		pmutex->m_lock.m_owner = UMUTEX_CONTESTED;
		pmutex->m_lock.m_flags = UMUTEX_PRIO_PROTECT;
		pmutex->m_lock.m_ceilings[0] = attr->m_ceiling;
		break;
	}
	if (attr->m_pshared == PTHREAD_PROCESS_SHARED)
		pmutex->m_lock.m_flags |= USYNC_PROCESS_SHARED;

	if (PMUTEX_TYPE(pmutex->m_flags) == PTHREAD_MUTEX_ADAPTIVE_NP) {
		pmutex->m_spinloops =
		    _thr_spinloops ? _thr_spinloops: MUTEX_ADAPTIVE_SPINS;
		pmutex->m_yieldloops = _thr_yieldloops;
	}
}

static int
mutex_init(pthread_mutex_t *mutex,
    const struct pthread_mutex_attr *mutex_attr,
    void *(calloc_cb)(size_t, size_t))
{
	const struct pthread_mutex_attr *attr;
	struct pthread_mutex *pmutex;
	int error;

	if (mutex_attr == NULL) {
		attr = &_pthread_mutexattr_default;
	} else {
		attr = mutex_attr;
		error = mutex_check_attr(attr);
		if (error != 0)
			return (error);
	}
	if ((pmutex = (pthread_mutex_t)
		calloc_cb(1, sizeof(struct pthread_mutex))) == NULL)
		return (ENOMEM);
	mutex_init_body(pmutex, attr);
	*mutex = pmutex;
	return (0);
}

static int
init_static(struct pthread *thread, pthread_mutex_t *mutex)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_mutex_static_lock);

	if (*mutex == THR_MUTEX_INITIALIZER)
		ret = mutex_init(mutex, &_pthread_mutexattr_default, calloc);
	else if (*mutex == THR_ADAPTIVE_MUTEX_INITIALIZER)
		ret = mutex_init(mutex, &_pthread_mutexattr_adaptive_default,
		    calloc);
	else
		ret = 0;
	THR_LOCK_RELEASE(thread, &_mutex_static_lock);

	return (ret);
}

static void
set_inherited_priority(struct pthread *curthread, struct pthread_mutex *m)
{
	struct pthread_mutex *m2;

	m2 = TAILQ_LAST(&curthread->mq[TMQ_NORM_PP], mutex_queue);
	if (m2 != NULL)
		m->m_lock.m_ceilings[1] = m2->m_lock.m_ceilings[0];
	else
		m->m_lock.m_ceilings[1] = -1;
}

static void
shared_mutex_init(struct pthread_mutex *pmtx, const struct
    pthread_mutex_attr *mutex_attr)
{
	static const struct pthread_mutex_attr foobar_mutex_attr = {
		.m_type = PTHREAD_MUTEX_DEFAULT,
		.m_protocol = PTHREAD_PRIO_NONE,
		.m_ceiling = 0,
		.m_pshared = PTHREAD_PROCESS_SHARED
	};
	bool done;

	/*
	 * Hack to allow multiple pthread_mutex_init() calls on the
	 * same process-shared mutex.  We rely on kernel allocating
	 * zeroed offpage for the mutex, i.e. the
	 * PMUTEX_INITSTAGE_ALLOC value must be zero.
	 */
	for (done = false; !done;) {
		switch (pmtx->m_ps) {
		case PMUTEX_INITSTAGE_DONE:
			atomic_thread_fence_acq();
			done = true;
			break;
		case PMUTEX_INITSTAGE_ALLOC:
			if (atomic_cmpset_int(&pmtx->m_ps,
			    PMUTEX_INITSTAGE_ALLOC, PMUTEX_INITSTAGE_BUSY)) {
				if (mutex_attr == NULL)
					mutex_attr = &foobar_mutex_attr;
				mutex_init_body(pmtx, mutex_attr);
				atomic_store_rel_int(&pmtx->m_ps,
				    PMUTEX_INITSTAGE_DONE);
				done = true;
			}
			break;
		case PMUTEX_INITSTAGE_BUSY:
			_pthread_yield();
			break;
		default:
			PANIC("corrupted offpage");
			break;
		}
	}
}

int
__pthread_mutex_init(pthread_mutex_t *mutex,
    const pthread_mutexattr_t *mutex_attr)
{
	struct pthread_mutex *pmtx;
	int ret;

	if (mutex_attr != NULL) {
		ret = mutex_check_attr(*mutex_attr);
		if (ret != 0)
			return (ret);
	}
	if (mutex_attr == NULL ||
	    (*mutex_attr)->m_pshared == PTHREAD_PROCESS_PRIVATE) {
		return (mutex_init(mutex, mutex_attr ? *mutex_attr : NULL,
		   calloc));
	}
	pmtx = __thr_pshared_offpage(mutex, 1);
	if (pmtx == NULL)
		return (EFAULT);
	*mutex = THR_PSHARED_PTR;
	shared_mutex_init(pmtx, *mutex_attr);
	return (0);
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
		.m_pshared = PTHREAD_PROCESS_PRIVATE,
	};
	int ret;

	ret = mutex_init(mutex, &attr, calloc_cb);
	if (ret == 0)
		(*mutex)->m_flags |= PMUTEX_FLAG_PRIVATE;
	return (ret);
}

/*
 * Fix mutex ownership for child process.
 *
 * Process private mutex ownership is transmitted from the forking
 * thread to the child process.
 *
 * Process shared mutex should not be inherited because owner is
 * forking thread which is in parent process, they are removed from
 * the owned mutex list.
 */
static void
queue_fork(struct pthread *curthread, struct mutex_queue *q,
    struct mutex_queue *qp, uint bit)
{
	struct pthread_mutex *m;

	TAILQ_INIT(q);
	TAILQ_FOREACH(m, qp, m_pqe) {
		TAILQ_INSERT_TAIL(q, m, m_qe);
		m->m_lock.m_owner = TID(curthread) | bit;
		m->m_owner = TID(curthread);
	}
}

void
_mutex_fork(struct pthread *curthread)
{

	queue_fork(curthread, &curthread->mq[TMQ_NORM],
	    &curthread->mq[TMQ_NORM_PRIV], 0);
	queue_fork(curthread, &curthread->mq[TMQ_NORM_PP],
	    &curthread->mq[TMQ_NORM_PP_PRIV], UMUTEX_CONTESTED);
}

int
_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	pthread_mutex_t m, m1;
	int ret;

	m = *mutex;
	if (m < THR_MUTEX_DESTROYED) {
		ret = 0;
	} else if (m == THR_MUTEX_DESTROYED) {
		ret = EINVAL;
	} else {
		if (m == THR_PSHARED_PTR) {
			m1 = __thr_pshared_offpage(mutex, 0);
			if (m1 != NULL) {
				mutex_assert_not_owned(m1);
				__thr_pshared_destroy(mutex);
			}
			*mutex = THR_MUTEX_DESTROYED;
			return (0);
		}
		if (m->m_owner != 0) {
			ret = EBUSY;
		} else {
			*mutex = THR_MUTEX_DESTROYED;
			mutex_assert_not_owned(m);
			free(m);
			ret = 0;
		}
	}

	return (ret);
}

static int
mutex_qidx(struct pthread_mutex *m)
{

	if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		return (TMQ_NORM);
	return (TMQ_NORM_PP);
}

static void
enqueue_mutex(struct pthread *curthread, struct pthread_mutex *m)
{
	int qidx;

	m->m_owner = TID(curthread);
	/* Add to the list of owned mutexes: */
	mutex_assert_not_owned(m);
	qidx = mutex_qidx(m);
	if (TAILQ_EMPTY(&curthread->mq[qidx]))
		TAILQ_INSERT_HEAD(&curthread->mq[qidx], m, m_qe);
	else
		TAILQ_INSERT_TAIL(&curthread->mq[qidx], m, m_qe);
	if (!is_pshared_mutex(m) && TAILQ_EMPTY(&curthread->mq[qidx + 1]))
		TAILQ_INSERT_HEAD(&curthread->mq[qidx + 1], m, m_pqe);
	else if (!is_pshared_mutex(m))
		TAILQ_INSERT_TAIL(&curthread->mq[qidx + 1], m, m_pqe);
}

static void
dequeue_mutex(struct pthread *curthread, struct pthread_mutex *m)
{
	int qidx;

	m->m_owner = 0;
	mutex_assert_is_owned(m);
	qidx = mutex_qidx(m);
	TAILQ_REMOVE(&curthread->mq[qidx], m, m_qe);
	if (!is_pshared_mutex(m))
		TAILQ_REMOVE(&curthread->mq[qidx + 1], m, m_pqe);
	if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) != 0)
		set_inherited_priority(curthread, m);
	mutex_init_link(m);
}

static int
check_and_init_mutex(pthread_mutex_t *mutex, struct pthread_mutex **m)
{
	int ret;

	*m = *mutex;
	ret = 0;
	if (*m == THR_PSHARED_PTR) {
		*m = __thr_pshared_offpage(mutex, 0);
		if (*m == NULL)
			ret = EINVAL;
		else
			shared_mutex_init(*m, NULL);
	} else if (__predict_false(*m <= THR_MUTEX_DESTROYED)) {
		if (*m == THR_MUTEX_DESTROYED) {
			ret = EINVAL;
		} else {
			ret = init_static(_get_curthread(), mutex);
			if (ret == 0)
				*m = *mutex;
		}
	}
	return (ret);
}

int
__pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	struct pthread *curthread;
	struct pthread_mutex *m;
	uint32_t id;
	int ret;

	ret = check_and_init_mutex(mutex, &m);
	if (ret != 0)
		return (ret);
	curthread = _get_curthread();
	id = TID(curthread);
	if (m->m_flags & PMUTEX_FLAG_PRIVATE)
		THR_CRITICAL_ENTER(curthread);
	ret = _thr_umutex_trylock(&m->m_lock, id);
	if (__predict_true(ret == 0)) {
		enqueue_mutex(curthread, m);
	} else if (m->m_owner == id) {
		ret = mutex_self_trylock(m);
	} /* else {} */
	if (ret && (m->m_flags & PMUTEX_FLAG_PRIVATE))
		THR_CRITICAL_LEAVE(curthread);
	return (ret);
}

static int
mutex_lock_sleep(struct pthread *curthread, struct pthread_mutex *m,
	const struct timespec *abstime)
{
	uint32_t	id, owner;
	int	count;
	int	ret;

	id = TID(curthread);
	if (m->m_owner == id)
		return (mutex_self_lock(m, abstime));

	/*
	 * For adaptive mutexes, spin for a bit in the expectation
	 * that if the application requests this mutex type then
	 * the lock is likely to be released quickly and it is
	 * faster than entering the kernel
	 */
	if (__predict_false(
		(m->m_lock.m_flags & 
		 (UMUTEX_PRIO_PROTECT | UMUTEX_PRIO_INHERIT)) != 0))
			goto sleep_in_kernel;

	if (!_thr_is_smp)
		goto yield_loop;

	count = m->m_spinloops;
	while (count--) {
		owner = m->m_lock.m_owner;
		if ((owner & ~UMUTEX_CONTESTED) == 0) {
			if (atomic_cmpset_acq_32(&m->m_lock.m_owner, owner, id|owner)) {
				ret = 0;
				goto done;
			}
		}
		CPU_SPINWAIT;
	}

yield_loop:
	count = m->m_yieldloops;
	while (count--) {
		_sched_yield();
		owner = m->m_lock.m_owner;
		if ((owner & ~UMUTEX_CONTESTED) == 0) {
			if (atomic_cmpset_acq_32(&m->m_lock.m_owner, owner, id|owner)) {
				ret = 0;
				goto done;
			}
		}
	}

sleep_in_kernel:
	if (abstime == NULL) {
		ret = __thr_umutex_lock(&m->m_lock, id);
	} else if (__predict_false(
		   abstime->tv_nsec < 0 ||
		   abstime->tv_nsec >= 1000000000)) {
		ret = EINVAL;
	} else {
		ret = __thr_umutex_timedlock(&m->m_lock, id, abstime);
	}
done:
	if (ret == 0)
		enqueue_mutex(curthread, m);

	return (ret);
}

static inline int
mutex_lock_common(struct pthread_mutex *m,
	const struct timespec *abstime, int cvattach)
{
	struct pthread *curthread  = _get_curthread();
	int ret;

	if (!cvattach && m->m_flags & PMUTEX_FLAG_PRIVATE)
		THR_CRITICAL_ENTER(curthread);
	if (_thr_umutex_trylock2(&m->m_lock, TID(curthread)) == 0) {
		enqueue_mutex(curthread, m);
		ret = 0;
	} else {
		ret = mutex_lock_sleep(curthread, m, abstime);
	}
	if (ret && (m->m_flags & PMUTEX_FLAG_PRIVATE) && !cvattach)
		THR_CRITICAL_LEAVE(curthread);
	return (ret);
}

int
__pthread_mutex_lock(pthread_mutex_t *mutex)
{
	struct pthread_mutex *m;
	int ret;

	_thr_check_init();
	ret = check_and_init_mutex(mutex, &m);
	if (ret == 0)
		ret = mutex_lock_common(m, NULL, 0);
	return (ret);
}

int
__pthread_mutex_timedlock(pthread_mutex_t *mutex,
    const struct timespec *abstime)
{
	struct pthread_mutex *m;
	int ret;

	_thr_check_init();
	ret = check_and_init_mutex(mutex, &m);
	if (ret == 0)
		ret = mutex_lock_common(m, abstime, 0);
	return (ret);
}

int
_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	struct pthread_mutex *mp;

	if (*mutex == THR_PSHARED_PTR) {
		mp = __thr_pshared_offpage(mutex, 0);
		if (mp == NULL)
			return (EINVAL);
		shared_mutex_init(mp, NULL);
	} else {
		mp = *mutex;
	}
	return (mutex_unlock_common(mp, 0, NULL));
}

int
_mutex_cv_lock(struct pthread_mutex *m, int count)
{
	int	error;

	error = mutex_lock_common(m, NULL, 1);
	if (error == 0)
		m->m_count = count;
	return (error);
}

int
_mutex_cv_unlock(struct pthread_mutex *m, int *count, int *defer)
{

	/*
	 * Clear the count in case this is a recursive mutex.
	 */
	*count = m->m_count;
	m->m_count = 0;
	(void)mutex_unlock_common(m, 1, defer);
        return (0);
}

int
_mutex_cv_attach(struct pthread_mutex *m, int count)
{
	struct pthread *curthread = _get_curthread();

	enqueue_mutex(curthread, m);
	m->m_count = count;
	return (0);
}

int
_mutex_cv_detach(struct pthread_mutex *mp, int *recurse)
{
	struct pthread *curthread = _get_curthread();
	int     defered;
	int     error;

	if ((error = _mutex_owned(curthread, mp)) != 0)
                return (error);

	/*
	 * Clear the count in case this is a recursive mutex.
	 */
	*recurse = mp->m_count;
	mp->m_count = 0;
	dequeue_mutex(curthread, mp);

	/* Will this happen in real-world ? */
        if ((mp->m_flags & PMUTEX_FLAG_DEFERED) != 0) {
		defered = 1;
		mp->m_flags &= ~PMUTEX_FLAG_DEFERED;
	} else
		defered = 0;

	if (defered)  {
		_thr_wake_all(curthread->defer_waiters,
				curthread->nwaiter_defer);
		curthread->nwaiter_defer = 0;
	}
	return (0);
}

static int
mutex_self_trylock(struct pthread_mutex *m)
{
	int	ret;

	switch (PMUTEX_TYPE(m->m_flags)) {
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_NORMAL:
	case PTHREAD_MUTEX_ADAPTIVE_NP:
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
mutex_self_lock(struct pthread_mutex *m, const struct timespec *abstime)
{
	struct timespec	ts1, ts2;
	int	ret;

	switch (PMUTEX_TYPE(m->m_flags)) {
	case PTHREAD_MUTEX_ERRORCHECK:
	case PTHREAD_MUTEX_ADAPTIVE_NP:
		if (abstime) {
			if (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			    abstime->tv_nsec >= 1000000000) {
				ret = EINVAL;
			} else {
				clock_gettime(CLOCK_REALTIME, &ts1);
				TIMESPEC_SUB(&ts2, abstime, &ts1);
				__sys_nanosleep(&ts2, NULL);
				ret = ETIMEDOUT;
			}
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
			if (abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
			    abstime->tv_nsec >= 1000000000) {
				ret = EINVAL;
			} else {
				clock_gettime(CLOCK_REALTIME, &ts1);
				TIMESPEC_SUB(&ts2, abstime, &ts1);
				__sys_nanosleep(&ts2, NULL);
				ret = ETIMEDOUT;
			}
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
mutex_unlock_common(struct pthread_mutex *m, int cv, int *mtx_defer)
{
	struct pthread *curthread = _get_curthread();
	uint32_t id;
	int defered, error;

	if (__predict_false(m <= THR_MUTEX_DESTROYED)) {
		if (m == THR_MUTEX_DESTROYED)
			return (EINVAL);
		return (EPERM);
	}

	id = TID(curthread);

	/*
	 * Check if the running thread is not the owner of the mutex.
	 */
	if (__predict_false(m->m_owner != id))
		return (EPERM);

	error = 0;
	if (__predict_false(
		PMUTEX_TYPE(m->m_flags) == PTHREAD_MUTEX_RECURSIVE &&
		m->m_count > 0)) {
		m->m_count--;
	} else {
		if ((m->m_flags & PMUTEX_FLAG_DEFERED) != 0) {
			defered = 1;
			m->m_flags &= ~PMUTEX_FLAG_DEFERED;
        	} else
			defered = 0;

		dequeue_mutex(curthread, m);
		error = _thr_umutex_unlock2(&m->m_lock, id, mtx_defer);

		if (mtx_defer == NULL && defered)  {
			_thr_wake_all(curthread->defer_waiters,
				curthread->nwaiter_defer);
			curthread->nwaiter_defer = 0;
		}
	}
	if (!cv && m->m_flags & PMUTEX_FLAG_PRIVATE)
		THR_CRITICAL_LEAVE(curthread);
	return (error);
}

int
_pthread_mutex_getprioceiling(pthread_mutex_t *mutex,
    int *prioceiling)
{
	struct pthread_mutex *m;

	if (*mutex == THR_PSHARED_PTR) {
		m = __thr_pshared_offpage(mutex, 0);
		if (m == NULL)
			return (EINVAL);
		shared_mutex_init(m, NULL);
	} else {
		m = *mutex;
		if (m <= THR_MUTEX_DESTROYED)
			return (EINVAL);
	}
	if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		return (EINVAL);
	*prioceiling = m->m_lock.m_ceilings[0];
	return (0);
}

int
_pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
    int ceiling, int *old_ceiling)
{
	struct pthread *curthread;
	struct pthread_mutex *m, *m1, *m2;
	struct mutex_queue *q, *qp;
	int ret;

	if (*mutex == THR_PSHARED_PTR) {
		m = __thr_pshared_offpage(mutex, 0);
		if (m == NULL)
			return (EINVAL);
		shared_mutex_init(m, NULL);
	} else {
		m = *mutex;
		if (m <= THR_MUTEX_DESTROYED)
			return (EINVAL);
	}
	if ((m->m_lock.m_flags & UMUTEX_PRIO_PROTECT) == 0)
		return (EINVAL);

	ret = __thr_umutex_set_ceiling(&m->m_lock, ceiling, old_ceiling);
	if (ret != 0)
		return (ret);

	curthread = _get_curthread();
	if (m->m_owner == TID(curthread)) {
		mutex_assert_is_owned(m);
		m1 = TAILQ_PREV(m, mutex_queue, m_qe);
		m2 = TAILQ_NEXT(m, m_qe);
		if ((m1 != NULL && m1->m_lock.m_ceilings[0] > (u_int)ceiling) ||
		    (m2 != NULL && m2->m_lock.m_ceilings[0] < (u_int)ceiling)) {
			q = &curthread->mq[TMQ_NORM_PP];
			qp = &curthread->mq[TMQ_NORM_PP_PRIV];
			TAILQ_REMOVE(q, m, m_qe);
			if (!is_pshared_mutex(m))
				TAILQ_REMOVE(qp, m, m_pqe);
			TAILQ_FOREACH(m2, q, m_qe) {
				if (m2->m_lock.m_ceilings[0] > (u_int)ceiling) {
					TAILQ_INSERT_BEFORE(m2, m, m_qe);
					if (!is_pshared_mutex(m)) {
						while (m2 != NULL &&
						    is_pshared_mutex(m2)) {
							m2 = TAILQ_PREV(m2,
							    mutex_queue, m_qe);
						}
						if (m2 == NULL) {
							TAILQ_INSERT_HEAD(qp,
							    m, m_pqe);
						} else {
							TAILQ_INSERT_BEFORE(m2,
							    m, m_pqe);
						}
					}
					return (0);
				}
			}
			TAILQ_INSERT_TAIL(q, m, m_qe);
			if (!is_pshared_mutex(m))
				TAILQ_INSERT_TAIL(qp, m, m_pqe);
		}
	}
	return (0);
}

int
_pthread_mutex_getspinloops_np(pthread_mutex_t *mutex, int *count)
{
	struct pthread_mutex *m;
	int ret;

	ret = check_and_init_mutex(mutex, &m);
	if (ret == 0)
		*count = m->m_spinloops;
	return (ret);
}

int
__pthread_mutex_setspinloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread_mutex *m;
	int ret;

	ret = check_and_init_mutex(mutex, &m);
	if (ret == 0)
		m->m_spinloops = count;
	return (ret);
}

int
_pthread_mutex_getyieldloops_np(pthread_mutex_t *mutex, int *count)
{
	struct pthread_mutex *m;
	int ret;

	ret = check_and_init_mutex(mutex, &m);
	if (ret == 0)
		*count = m->m_yieldloops;
	return (ret);
}

int
__pthread_mutex_setyieldloops_np(pthread_mutex_t *mutex, int count)
{
	struct pthread_mutex *m;
	int ret;

	ret = check_and_init_mutex(mutex, &m);
	if (ret == 0)
		m->m_yieldloops = count;
	return (0);
}

int
_pthread_mutex_isowned_np(pthread_mutex_t *mutex)
{
	struct pthread_mutex *m;

	if (*mutex == THR_PSHARED_PTR) {
		m = __thr_pshared_offpage(mutex, 0);
		if (m == NULL)
			return (0);
		shared_mutex_init(m, NULL);
	} else {
		m = *mutex;
		if (m <= THR_MUTEX_DESTROYED)
			return (0);
	}
	return (m->m_owner == TID(_get_curthread()));
}

int
_mutex_owned(struct pthread *curthread, const struct pthread_mutex *mp)
{
	if (__predict_false(mp <= THR_MUTEX_DESTROYED)) {
		if (mp == THR_MUTEX_DESTROYED)
			return (EINVAL);
		return (EPERM);
	}
	if (mp->m_owner != TID(curthread))
		return (EPERM);
	return (0);                  
}
