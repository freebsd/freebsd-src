/*
 * Copyright (c) 2004 Michael Telahun Makonnen <mtm@FreeBSD.Org>
 * Copyright (c) 1997 John Birrell <jb@cimlogic.com.au>.
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
 *
 */

#include <sys/types.h>
#include <machine/atomic.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <pthread.h>
#include <unistd.h>

#include <libc_private.h>

#include "thr_private.h"

#define THR_SPIN_MAGIC		0xdadadada
#define THR_SPIN_UNOWNED	(void *)0
#define MAGIC_TEST_RETURN_ON_FAIL(l)					   \
	do {								   \
		if ((l) == NULL || (l)->s_magic != THR_SPIN_MAGIC)	   \
			return (EINVAL);				   \
	} while(0)

__weak_reference(_pthread_spin_destroy, pthread_spin_destroy);
__weak_reference(_pthread_spin_init, pthread_spin_init);
__weak_reference(_pthread_spin_lock, pthread_spin_lock);
__weak_reference(_pthread_spin_trylock, pthread_spin_trylock);
__weak_reference(_pthread_spin_unlock, pthread_spin_unlock);

int
_pthread_spin_destroy(pthread_spinlock_t *lock)
{
	MAGIC_TEST_RETURN_ON_FAIL((*lock));
	if ((*lock)->s_owner == THR_SPIN_UNOWNED) {
		(*lock)->s_magic = 0;
		free((*lock));
		*lock = NULL;
		return (0);
	}
	return (EBUSY);
}

int
_pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
	struct pthread_spinlock *s;

	s = (struct pthread_spinlock *)malloc(sizeof(struct pthread_spinlock));
	if (s == NULL)
		return (ENOMEM);
	s->s_magic = THR_SPIN_MAGIC;
	s->s_owner = THR_SPIN_UNOWNED;
	*lock = s;
	return (0);
}

/*
 * If the caller sets nonblocking to 1, this function will return
 * immediately without acquiring the lock it is owned by another thread.
 * If set to 0, it will keep spinning until it acquires the lock.
 */
int
_pthread_spin_lock(pthread_spinlock_t *lock)
{
	MAGIC_TEST_RETURN_ON_FAIL(*lock);
	if ((*lock)->s_owner == curthread)
		return (EDEADLK);
        while (atomic_cmpset_acq_ptr(&(*lock)->s_owner, THR_SPIN_UNOWNED,
            (void *)curthread) != 1)
		;	/* SPIN */
	return (0);
}

int
_pthread_spin_trylock(pthread_spinlock_t *lock)
{
	MAGIC_TEST_RETURN_ON_FAIL(*lock);
	if (atomic_cmpset_acq_ptr(&(*lock)->s_owner, THR_SPIN_UNOWNED,
	    (void *)curthread) == 1)
		return (0);
	return (EBUSY);
}

int
_pthread_spin_unlock(pthread_spinlock_t *lock)
{
	MAGIC_TEST_RETURN_ON_FAIL(*lock);
	if (atomic_cmpset_rel_ptr(&(*lock)->s_owner, (void *)curthread,
	    THR_SPIN_UNOWNED) == 1)
		return (0);
	return (EPERM);
}

void
_spinunlock(spinlock_t *lck)
{
	if (umtx_unlock((struct umtx *)lck, curthread->thr_id))
		abort();
}

/*
 * Lock a location for the running thread. Yield to allow other
 * threads to run if this thread is blocked because the lock is
 * not available. Note that this function does not sleep. It
 * assumes that the lock will be available very soon.
 */
void
_spinlock(spinlock_t *lck)
{
	if (umtx_lock((struct umtx *)lck, curthread->thr_id))
		abort();
}

int
_spintrylock(spinlock_t *lck)
{
	int error;

	error = umtx_lock((struct umtx *)lck, curthread->thr_id);
	if (error != 0 && error != EBUSY)
		abort();
	return (error);
}

/*
 * Lock a location for the running thread. Yield to allow other
 * threads to run if this thread is blocked because the lock is
 * not available. Note that this function does not sleep. It
 * assumes that the lock will be available very soon.
 *
 * This function checks if the running thread has already locked the
 * location, warns if this occurs and creates a thread dump before
 * returning.
 */
void
_spinlock_debug(spinlock_t *lck, char *fname, int lineno)
{
	if (umtx_lock((struct umtx *)lck, curthread->thr_id))
		abort();
}
