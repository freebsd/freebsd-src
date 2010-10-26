/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include "un-namespace.h"

#include "thr_private.h"

/*
 * Prototypes
 */
int	__pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int	__pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec * abstime);
static int cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
static int cond_wait_common(pthread_cond_t *cond, pthread_mutex_t *mutex,
		    const struct timespec *abstime, int cancel);
static int cond_signal_common(pthread_cond_t *cond, int broadcast);

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

static int
cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{
	pthread_cond_t	pcond;
	int             rval = 0;

	if ((pcond = (pthread_cond_t)
	    calloc(1, sizeof(struct pthread_cond))) == NULL) {
		rval = ENOMEM;
	} else {
		/*
		 * Initialise the condition variable structure:
		 */
		if (cond_attr == NULL || *cond_attr == NULL) {
			pcond->c_pshared = 0;
			pcond->c_clockid = CLOCK_REALTIME;
		} else {
			pcond->c_pshared = (*cond_attr)->c_pshared;
			pcond->c_clockid = (*cond_attr)->c_clockid;
		}
		_thr_umutex_init(&pcond->c_lock);
		*cond = pcond;
	}
	/* Return the completion status: */
	return (rval);
}

static int
init_static(struct pthread *thread, pthread_cond_t *cond)
{
	int ret;

	THR_LOCK_ACQUIRE(thread, &_cond_static_lock);

	if (*cond == NULL)
		ret = cond_init(cond, NULL);
	else
		ret = 0;

	THR_LOCK_RELEASE(thread, &_cond_static_lock);

	return (ret);
}

#define CHECK_AND_INIT_COND							\
	if (__predict_false((cv = (*cond)) <= THR_COND_DESTROYED)) {		\
		if (cv == THR_COND_INITIALIZER) {				\
			int ret;						\
			ret = init_static(_get_curthread(), cond);		\
			if (ret)						\
				return (ret);					\
		} else if (cv == THR_COND_DESTROYED) {				\
			return (EINVAL);					\
		}								\
		cv = *cond;							\
	}

int
_pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr)
{

	*cond = NULL;
	return (cond_init(cond, cond_attr));
}

int
_pthread_cond_destroy(pthread_cond_t *cond)
{
	struct pthread		*curthread = _get_curthread();
	struct pthread_cond	*cv;
	int			rval = 0;

	if ((cv = *cond) == THR_COND_INITIALIZER)
		rval = 0;
	else if (cv == THR_COND_DESTROYED)
		rval = EINVAL;
	else {
		cv = *cond;
		THR_UMUTEX_LOCK(curthread, &cv->c_lock);
		*cond = THR_COND_DESTROYED;
		THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);

		/*
		 * Free the memory allocated for the condition
		 * variable structure:
		 */
		free(cv);
	}
	return (rval);
}

struct cond_cancel_info
{
	pthread_mutex_t	*mutex;
	pthread_cond_t	*cond;
	int		count;
};

static void
cond_cancel_handler(void *arg)
{
	struct pthread *curthread = _get_curthread();
	struct cond_cancel_info *info = (struct cond_cancel_info *)arg;
	pthread_cond_t  cv;

	if (info->cond != NULL) {
		cv = *(info->cond);
		THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);
	}
	_mutex_cv_lock(info->mutex, info->count);
}

static int
cond_wait_common(pthread_cond_t *cond, pthread_mutex_t *mutex,
	const struct timespec *abstime, int cancel)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec ts, ts2, *tsp;
	struct cond_cancel_info info;
	pthread_cond_t  cv;
	int		ret;

	/*
	 * If the condition variable is statically initialized,
	 * perform the dynamic initialization:
	 */
	CHECK_AND_INIT_COND

	cv = *cond;
	THR_UMUTEX_LOCK(curthread, &cv->c_lock);
	ret = _mutex_cv_unlock(mutex, &info.count);
	if (__predict_false(ret != 0)) {
		THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);
		return (ret);
	}

	info.mutex = mutex;
	info.cond  = cond;

	if (abstime != NULL) {
		clock_gettime(cv->c_clockid, &ts);
		TIMESPEC_SUB(&ts2, abstime, &ts);
		tsp = &ts2;
	} else
		tsp = NULL;

	if (cancel) {
		THR_CLEANUP_PUSH(curthread, cond_cancel_handler, &info);
		_thr_cancel_enter_defer(curthread);
		ret = _thr_ucond_wait(&cv->c_kerncv, &cv->c_lock, tsp, 1);
		info.cond = NULL;
		_thr_cancel_leave_defer(curthread, ret);
		THR_CLEANUP_POP(curthread, 0);
	} else {
		ret = _thr_ucond_wait(&cv->c_kerncv, &cv->c_lock, tsp, 0);
	}
	if (ret == EINTR)
		ret = 0;
	_mutex_cv_lock(mutex, info.count);
	return (ret);
}

int
_pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

	return (cond_wait_common(cond, mutex, NULL, 0));
}

int
__pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{

	return (cond_wait_common(cond, mutex, NULL, 1));
}

int
_pthread_cond_timedwait(pthread_cond_t * cond, pthread_mutex_t * mutex,
		       const struct timespec * abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cond, mutex, abstime, 0));
}

int
__pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
		       const struct timespec *abstime)
{

	if (abstime == NULL || abstime->tv_sec < 0 || abstime->tv_nsec < 0 ||
	    abstime->tv_nsec >= 1000000000)
		return (EINVAL);

	return (cond_wait_common(cond, mutex, abstime, 1));
}

static int
cond_signal_common(pthread_cond_t *cond, int broadcast)
{
	struct pthread	*curthread = _get_curthread();
	pthread_cond_t	cv;
	int		ret = 0;

	/*
	 * If the condition variable is statically initialized, perform dynamic
	 * initialization.
	 */
	CHECK_AND_INIT_COND

	THR_UMUTEX_LOCK(curthread, &cv->c_lock);
	if (!broadcast)
		ret = _thr_ucond_signal(&cv->c_kerncv);
	else
		ret = _thr_ucond_broadcast(&cv->c_kerncv);
	THR_UMUTEX_UNLOCK(curthread, &cv->c_lock);
	return (ret);
}

int
_pthread_cond_signal(pthread_cond_t * cond)
{

	return (cond_signal_common(cond, 0));
}

int
_pthread_cond_broadcast(pthread_cond_t * cond)
{

	return (cond_signal_common(cond, 1));
}
