/*-
 * Copyright (c) 2004 Michael Telahun Makonnen <mtm@FreeBSD.Org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "thr_private.h"

__weak_reference(_pthread_barrier_destroy, pthread_barrier_destroy);
__weak_reference(_pthread_barrier_init, pthread_barrier_init);
__weak_reference(_pthread_barrier_wait, pthread_barrier_wait);

int
_pthread_barrier_destroy(pthread_barrier_t *barrier)
{
	if (*barrier == NULL)
		return (EINVAL);
	if ((*barrier)->b_subtotal > 0)
		return (EBUSY);
	PTHREAD_ASSERT((*barrier)->b_subtotal == 0,
	    "barrier count must be zero when destroyed");
	free(*barrier);
	*barrier = NULL;
	return (0);
}

int
_pthread_barrier_init(pthread_barrier_t *barrier,
    const pthread_barrierattr_t attr, unsigned int count)
{
	if (count < 1)
		return (EINVAL);
	*barrier =
	    (struct pthread_barrier *)malloc(sizeof(struct pthread_barrier));
	if (*barrier == NULL)
		return (ENOMEM);
	memset((void *)*barrier, 0, sizeof(struct pthread_barrier));
	(*barrier)->b_total = count;
	TAILQ_INIT(&(*barrier)->b_barrq);
	return (0);
}

int
_pthread_barrier_wait(pthread_barrier_t *barrier)
{
	struct pthread_barrier *b;
	struct pthread	       *ptd;
	int error;

	if (*barrier == NULL)
		return (EINVAL);

	/*
	 * Check if threads waiting on the barrier can be released. If
	 * so, release them and make this last thread the special thread.
	 */
	error = 0;
	b = *barrier;
	UMTX_LOCK(&b->b_lock);
	if (b->b_subtotal == (b->b_total - 1)) {
		TAILQ_FOREACH(ptd, &b->b_barrq, sqe) {
			_thread_critical_enter(ptd);
			PTHREAD_NEW_STATE(ptd, PS_RUNNING);
			TAILQ_REMOVE(&b->b_barrq, ptd, sqe);
			ptd->flags |= PTHREAD_FLAGS_BARR_REL;
			_thread_critical_exit(ptd);
		}
		b->b_subtotal = 0;
		UMTX_UNLOCK(&b->b_lock);
		return (PTHREAD_BARRIER_SERIAL_THREAD);
	}

	/*
	 * More threads need to reach the barrier. Suspend this thread.
	 */
	_thread_critical_enter(curthread);
	TAILQ_INSERT_HEAD(&b->b_barrq, curthread, sqe);
	PTHREAD_NEW_STATE(curthread, PS_BARRIER_WAIT);
	_thread_critical_exit(curthread);
	b->b_subtotal++;
	PTHREAD_ASSERT(b->b_subtotal < b->b_total,
	    "the number of threads waiting at a barrier is too large");
	UMTX_UNLOCK(&b->b_lock);
	do {
		error = _thread_suspend(curthread, NULL);
		if (error == EINTR) {
			/*
			 * Make sure this thread wasn't released from
			 * the barrier while it was handling the signal.
			 */
			_thread_critical_enter(curthread);
			if ((curthread->flags & PTHREAD_FLAGS_BARR_REL) != 0) {
				curthread->flags &= ~PTHREAD_FLAGS_BARR_REL;
				_thread_critical_exit(curthread);
				error = 0;
				break;
			}
			PTHREAD_NEW_STATE(curthread, PS_BARRIER_WAIT);
			_thread_critical_exit(curthread);
		}
	} while (error == EINTR);
	return (error);
}
