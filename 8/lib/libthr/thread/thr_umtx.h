/*-
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
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

#ifndef _THR_FBSD_UMTX_H_
#define _THR_FBSD_UMTX_H_

#include <strings.h>
#include <sys/umtx.h>

#define DEFAULT_UMUTEX	{0,0, {0,0},{0,0,0,0}}

int __thr_umutex_lock(struct umutex *mtx, uint32_t id) __hidden;
int __thr_umutex_timedlock(struct umutex *mtx, uint32_t id,
	const struct timespec *timeout) __hidden;
int __thr_umutex_unlock(struct umutex *mtx, uint32_t id) __hidden;
int __thr_umutex_trylock(struct umutex *mtx) __hidden;
int __thr_umutex_set_ceiling(struct umutex *mtx, uint32_t ceiling,
	uint32_t *oldceiling) __hidden;

void _thr_umutex_init(struct umutex *mtx) __hidden;
int _thr_umtx_wait(volatile long *mtx, long exp,
	const struct timespec *timeout) __hidden;
int _thr_umtx_wait_uint(volatile u_int *mtx, u_int exp,
	const struct timespec *timeout, int shared) __hidden;
int _thr_umtx_wake(volatile void *mtx, int count, int shared) __hidden;
int _thr_ucond_wait(struct ucond *cv, struct umutex *m,
        const struct timespec *timeout, int check_unpaking) __hidden;
void _thr_ucond_init(struct ucond *cv) __hidden;
int _thr_ucond_signal(struct ucond *cv) __hidden;
int _thr_ucond_broadcast(struct ucond *cv) __hidden;

int __thr_rwlock_rdlock(struct urwlock *rwlock, int flags, struct timespec *tsp) __hidden;
int __thr_rwlock_wrlock(struct urwlock *rwlock, struct timespec *tsp) __hidden;
int __thr_rwlock_unlock(struct urwlock *rwlock) __hidden;

static inline int
_thr_umutex_trylock(struct umutex *mtx, uint32_t id)
{
    if (atomic_cmpset_acq_32(&mtx->m_owner, UMUTEX_UNOWNED, id))
	return (0);
    if ((mtx->m_flags & UMUTEX_PRIO_PROTECT) == 0)
    	return (EBUSY);
    return (__thr_umutex_trylock(mtx));
}

static inline int
_thr_umutex_trylock2(struct umutex *mtx, uint32_t id)
{
    if (atomic_cmpset_acq_32(&mtx->m_owner, UMUTEX_UNOWNED, id) != 0)
	return (0);
    if ((uint32_t)mtx->m_owner == UMUTEX_CONTESTED &&
        __predict_true((mtx->m_flags & (UMUTEX_PRIO_PROTECT | UMUTEX_PRIO_INHERIT)) == 0))
    	if (atomic_cmpset_acq_32(&mtx->m_owner, UMUTEX_CONTESTED, id | UMUTEX_CONTESTED))
		return (0);
    return (EBUSY);
}

static inline int
_thr_umutex_lock(struct umutex *mtx, uint32_t id)
{
    if (_thr_umutex_trylock2(mtx, id) == 0)
	return (0);
    return (__thr_umutex_lock(mtx, id));
}

static inline int
_thr_umutex_timedlock(struct umutex *mtx, uint32_t id,
	const struct timespec *timeout)
{
    if (_thr_umutex_trylock2(mtx, id) == 0)
	return (0);
    return (__thr_umutex_timedlock(mtx, id, timeout));
}

static inline int
_thr_umutex_unlock(struct umutex *mtx, uint32_t id)
{
    if (atomic_cmpset_rel_32(&mtx->m_owner, id, UMUTEX_UNOWNED))
	return (0);
    return (__thr_umutex_unlock(mtx, id));
}

static inline int
_thr_rwlock_tryrdlock(struct urwlock *rwlock, int flags)
{
	int32_t state;
	int32_t wrflags;

	if (flags & URWLOCK_PREFER_READER || rwlock->rw_flags & URWLOCK_PREFER_READER)
		wrflags = URWLOCK_WRITE_OWNER;
	else
		wrflags = URWLOCK_WRITE_OWNER | URWLOCK_WRITE_WAITERS;
	state = rwlock->rw_state;
	while (!(state & wrflags)) {
		if (__predict_false(URWLOCK_READER_COUNT(state) == URWLOCK_MAX_READERS))
			return (EAGAIN);
		if (atomic_cmpset_acq_32(&rwlock->rw_state, state, state + 1))
			return (0);
		state = rwlock->rw_state;
	}

	return (EBUSY);
}

static inline int
_thr_rwlock_trywrlock(struct urwlock *rwlock)
{
	int32_t state;

	state = rwlock->rw_state;
	while (!(state & URWLOCK_WRITE_OWNER) && URWLOCK_READER_COUNT(state) == 0) {
		if (atomic_cmpset_acq_32(&rwlock->rw_state, state, state | URWLOCK_WRITE_OWNER))
			return (0);
		state = rwlock->rw_state;
	}

	return (EBUSY);
}

static inline int
_thr_rwlock_rdlock(struct urwlock *rwlock, int flags, struct timespec *tsp)
{
	if (_thr_rwlock_tryrdlock(rwlock, flags) == 0)
		return (0);
	return (__thr_rwlock_rdlock(rwlock, flags, tsp));
}

static inline int
_thr_rwlock_wrlock(struct urwlock *rwlock, struct timespec *tsp)
{
	if (_thr_rwlock_trywrlock(rwlock) == 0)
		return (0);
	return (__thr_rwlock_wrlock(rwlock, tsp));
}

static inline int
_thr_rwlock_unlock(struct urwlock *rwlock)
{
	int32_t state;

	state = rwlock->rw_state;
	if (state & URWLOCK_WRITE_OWNER) {
		if (atomic_cmpset_rel_32(&rwlock->rw_state, URWLOCK_WRITE_OWNER, 0))
			return (0);
	} else {
		for (;;) {
			if (__predict_false(URWLOCK_READER_COUNT(state) == 0))
				return (EPERM);
			if (!((state & (URWLOCK_WRITE_WAITERS |
			    URWLOCK_READ_WAITERS)) &&
			    URWLOCK_READER_COUNT(state) == 1)) {
				if (atomic_cmpset_rel_32(&rwlock->rw_state,
				    state, state-1))
					return (0);
				state = rwlock->rw_state;
			} else {
				break;
			}
		}
    	}
    	return (__thr_rwlock_unlock(rwlock));
}
#endif
