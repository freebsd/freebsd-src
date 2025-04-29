/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005, David Xu <davidxu@freebsd.org>
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
 */

#include "namespace.h"
#include <pthread.h>
#include "un-namespace.h"

#include "thr_private.h"

__weak_reference(_thr_cancel, pthread_cancel);
__weak_reference(_thr_cancel, _pthread_cancel);
__weak_reference(_thr_setcancelstate, pthread_setcancelstate);
__weak_reference(_thr_setcancelstate, _pthread_setcancelstate);
__weak_reference(_thr_setcanceltype, pthread_setcanceltype);
__weak_reference(_thr_setcanceltype, _pthread_setcanceltype);
__weak_reference(_Tthr_testcancel, pthread_testcancel);
__weak_reference(_Tthr_testcancel, _pthread_testcancel);
__weak_reference(_Tthr_cancel_enter, _pthread_cancel_enter);
__weak_reference(_Tthr_cancel_leave, _pthread_cancel_leave);

static inline void
testcancel(struct pthread *curthread)
{
	if (__predict_false(SHOULD_CANCEL(curthread) &&
	    !THR_IN_CRITICAL(curthread)))
		_pthread_exit(PTHREAD_CANCELED);
}

void
_thr_testcancel(struct pthread *curthread)
{
	testcancel(curthread);
}

int
_thr_cancel(pthread_t pthread)
{
	struct pthread *curthread = _get_curthread();
	int ret;

	/*
	 * POSIX says _pthread_cancel should be async cancellation safe.
	 * _thr_find_thread and THR_THREAD_UNLOCK will enter and leave critical
	 * region automatically.
	 */
	if ((ret = _thr_find_thread(curthread, pthread, 1)) == 0) {
		if (!pthread->cancel_pending) {
			pthread->cancel_pending = 1;
			if (pthread->state != PS_DEAD)
				_thr_send_sig(pthread, SIGCANCEL);
		}
		THR_THREAD_UNLOCK(curthread, pthread);
	}
	return (ret);
}

int
_thr_setcancelstate(int state, int *oldstate)
{
	struct pthread *curthread = _get_curthread();
	int oldval, val;

	switch (state) {
	case PTHREAD_CANCEL_DISABLE:
		val = 0;
		break;
	case PTHREAD_CANCEL_ENABLE:
		val = 1;
		break;
	default:
		return (EINVAL);
	}

	oldval = atomic_swap_int(&curthread->cancel_enable, val);
	if (state == PTHREAD_CANCEL_ENABLE && curthread->cancel_async)
		testcancel(curthread);
	if (oldstate != NULL) {
		*oldstate = oldval ? PTHREAD_CANCEL_ENABLE :
		    PTHREAD_CANCEL_DISABLE;
	}
	return (0);
}

int
_thr_setcanceltype(int type, int *oldtype)
{
	struct pthread	*curthread = _get_curthread();
	int oldval;

	oldval = curthread->cancel_async;
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		curthread->cancel_async = 1;
		testcancel(curthread);
		break;
	case PTHREAD_CANCEL_DEFERRED:
		curthread->cancel_async = 0;
		break;
	default:
		return (EINVAL);
	}

	if (oldtype != NULL) {
		*oldtype = oldval ? PTHREAD_CANCEL_ASYNCHRONOUS :
		    PTHREAD_CANCEL_DEFERRED;
	}
	return (0);
}

void
_Tthr_testcancel(void)
{
	struct pthread *curthread;

	_thr_check_init();
	curthread = _get_curthread();
	testcancel(curthread);
}

void
_thr_cancel_enter(struct pthread *curthread)
{
	curthread->cancel_point = 1;
	testcancel(curthread);
}

void
_thr_cancel_enter2(struct pthread *curthread, int maycancel)
{
	curthread->cancel_point = 1;
	if (__predict_false(SHOULD_CANCEL(curthread) &&
	    !THR_IN_CRITICAL(curthread))) {
		if (!maycancel)
			thr_wake(curthread->tid);
		else
			_pthread_exit(PTHREAD_CANCELED);
	}
}

void
_thr_cancel_leave(struct pthread *curthread, int maycancel)
{
	curthread->cancel_point = 0;
	if (maycancel)
		testcancel(curthread);
}

void
_Tthr_cancel_enter(int maycancel)
{
	_thr_cancel_enter2(_get_curthread(), maycancel);
}

void
_Tthr_cancel_leave(int maycancel)
{
	_thr_cancel_leave(_get_curthread(), maycancel);
}
