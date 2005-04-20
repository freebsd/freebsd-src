/*-
 * Copyright (c) 2003 David Xu <davidxu@freebsd.org>
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

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <atomic_ops.h>
#include "thr_private.h"

#define SPIN_COUNT 10000

__weak_reference(_pthread_spin_init, pthread_spin_init);
__weak_reference(_pthread_spin_destroy, pthread_spin_destroy);
__weak_reference(_pthread_spin_trylock, pthread_spin_trylock);
__weak_reference(_pthread_spin_lock, pthread_spin_lock);
__weak_reference(_pthread_spin_unlock, pthread_spin_unlock);

int
_pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
	struct pthread_spinlock	*lck;
	int ret;

	if (lock == NULL || pshared != PTHREAD_PROCESS_PRIVATE)
		ret = EINVAL;
	else if ((lck = malloc(sizeof(struct pthread_spinlock))) == NULL)
		ret = ENOMEM;
	else {
		lck->s_lock = 0;
		lck->s_owner= NULL;
		*lock = lck;
		ret = 0;
	}

	return (ret);
}

int
_pthread_spin_destroy(pthread_spinlock_t *lock)
{
	int ret;

	if (lock == NULL || *lock == NULL)
		ret = EINVAL;
	else if ((*lock)->s_owner != NULL)
		ret = EBUSY;
	else {
		free(*lock);
		*lock = NULL;
		ret = 0;
	}

	return (ret);
}

int
_pthread_spin_trylock(pthread_spinlock_t *lock)
{
	struct pthread_spinlock	*lck;
	struct pthread *self = _pthread_self();
	int oldval, ret;

	if (lock == NULL || (lck = *lock) == NULL)
		ret = EINVAL;
	else if (lck->s_owner == self)
		ret = EDEADLK;
	else if (lck->s_lock != 0)
		ret = EBUSY;
	else {
		atomic_swap_int((int *)&(lck)->s_lock, 1, &oldval);
		if (oldval)
			ret = EBUSY;
		else {
			lck->s_owner = _pthread_self();
			ret = 0;
		}
	}
	return (ret);
}

int
_pthread_spin_lock(pthread_spinlock_t *lock)
{
	struct pthread_spinlock	*lck;
	struct pthread *self = _pthread_self();
	int count, oldval, ret;

	if (lock == NULL || (lck = *lock) == NULL)
		ret = EINVAL;
	else if (lck->s_owner == self)
		ret = EDEADLK;
	else {
		do {
			count = SPIN_COUNT;
			while (lck->s_lock) {
#ifdef __i386__
				/* tell cpu we are spinning */
				__asm __volatile("pause");
#endif
				if (--count <= 0) {
					count = SPIN_COUNT;
					_pthread_yield();
				}
			}
			atomic_swap_int((int *)&(lck)->s_lock, 1, &oldval);
		} while (oldval);

		lck->s_owner = self;
		ret = 0;
	}

	return (ret);
}

int
_pthread_spin_unlock(pthread_spinlock_t *lock)
{
	struct pthread_spinlock	*lck;
	int ret;

	if (lock == NULL || (lck = *lock) == NULL)
		ret = EINVAL;
	else {
		if (lck->s_owner != _pthread_self())
			ret = EPERM;
		else {
			lck->s_owner = NULL;
			atomic_swap_int((int *)&lck->s_lock, 0, &ret);
			ret = 0;
		}
	}

	return (ret);
}

