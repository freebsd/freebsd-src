/*
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
 *
 * $FreeBSD$
 *
 */

#include <pthread.h>

#include "thr_private.h"

__weak_reference(_pthread_cancel, pthread_cancel);
__weak_reference(_pthread_setcancelstate, pthread_setcancelstate);
__weak_reference(_pthread_setcanceltype, pthread_setcanceltype);
__weak_reference(_pthread_testcancel, pthread_testcancel);

int _pthread_setcanceltype(int type, int *oldtype);

int
_pthread_cancel(pthread_t pthread)
{
	struct pthread *curthread = _get_curthread();
	int oldval, newval = 0;
	int oldtype;
	int ret;

	/*
	 * POSIX says _pthread_cancel should be async cancellation safe,
	 * so we temporarily disable async cancellation.
	 */
	_pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);
	if ((ret = _thr_ref_add(curthread, pthread, 0)) != 0) {
		_pthread_setcanceltype(oldtype, NULL);
		return (ret);
	}

	do {
		oldval = pthread->cancelflags;
		if (oldval & THR_CANCEL_NEEDED)
			break;
		newval = oldval | THR_CANCEL_NEEDED;
	} while (!atomic_cmpset_acq_int(&pthread->cancelflags, oldval, newval));

	if (!(oldval & THR_CANCEL_NEEDED) && SHOULD_ASYNC_CANCEL(newval))
		_thr_send_sig(pthread, SIGCANCEL);

	_thr_ref_delete(curthread, pthread);
	_pthread_setcanceltype(oldtype, NULL);
	return (0);
}

static inline void
testcancel(struct pthread *curthread)
{
	int newval;

	newval = curthread->cancelflags;
	if (SHOULD_CANCEL(newval))
		pthread_exit(PTHREAD_CANCELED);
}

int
_pthread_setcancelstate(int state, int *oldstate)
{
	struct pthread *curthread = _get_curthread();
	int oldval, ret;

	oldval = curthread->cancelflags;
	if (oldstate != NULL)
		*oldstate = ((oldval & THR_CANCEL_DISABLE) ?
		    PTHREAD_CANCEL_DISABLE : PTHREAD_CANCEL_ENABLE);
	switch (state) {
	case PTHREAD_CANCEL_DISABLE:
		atomic_set_int(&curthread->cancelflags, THR_CANCEL_DISABLE);
		ret = 0;
		break;
	case PTHREAD_CANCEL_ENABLE:
		atomic_clear_int(&curthread->cancelflags, THR_CANCEL_DISABLE);
		testcancel(curthread);
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	return (ret);
}

int
_pthread_setcanceltype(int type, int *oldtype)
{
	struct pthread	*curthread = _get_curthread();
	int oldval, ret;

	oldval = curthread->cancelflags;
	if (oldtype != NULL)
		*oldtype = ((oldval & THR_CANCEL_AT_POINT) ?
				 PTHREAD_CANCEL_ASYNCHRONOUS :
				 PTHREAD_CANCEL_DEFERRED);
	switch (type) {
	case PTHREAD_CANCEL_ASYNCHRONOUS:
		atomic_set_int(&curthread->cancelflags, THR_CANCEL_AT_POINT);
		testcancel(curthread);
		ret = 0;
		break;
	case PTHREAD_CANCEL_DEFERRED:
		atomic_clear_int(&curthread->cancelflags, THR_CANCEL_AT_POINT);
		ret = 0;
		break;
	default:
		ret = EINVAL;
	}

	return (ret);
}

void
_pthread_testcancel(void)
{
	testcancel(_get_curthread());
}

int
_thr_cancel_enter(struct pthread *curthread)
{
	int oldval;

	oldval = curthread->cancelflags;
	if (!(oldval & THR_CANCEL_AT_POINT)) {
		atomic_set_int(&curthread->cancelflags, THR_CANCEL_AT_POINT);
		testcancel(curthread);
	}
	return (oldval);
}

void
_thr_cancel_leave(struct pthread *curthread, int previous)
{
	if (!(previous & THR_CANCEL_AT_POINT))
		atomic_clear_int(&curthread->cancelflags, THR_CANCEL_AT_POINT);
}
