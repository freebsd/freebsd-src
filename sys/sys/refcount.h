/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2005 John Baldwin <jhb@FreeBSD.org>
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

#ifndef __SYS_REFCOUNT_H__
#define __SYS_REFCOUNT_H__

#include <machine/atomic.h>

#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <stdbool.h>
#define	KASSERT(exp, msg)	/* */
#endif

#define	REFCOUNT_WAITER			(1U << 31) /* Refcount has waiter. */
#define	REFCOUNT_SATURATION_VALUE	(3U << 29)

#define	REFCOUNT_SATURATED(val)		(((val) & (1U << 30)) != 0)
#define	REFCOUNT_COUNT(x)		((x) & ~REFCOUNT_WAITER)

bool refcount_release_last(volatile u_int *count, u_int n, u_int old);
void refcount_sleep(volatile u_int *count, const char *wmesg, int prio);

/*
 * Attempt to handle reference count overflow and underflow.  Force the counter
 * to stay at the saturation value so that a counter overflow cannot trigger
 * destruction of the containing object and instead leads to a less harmful
 * memory leak.
 */
static __inline void
_refcount_update_saturated(volatile u_int *count)
{
#ifdef INVARIANTS
	panic("refcount %p wraparound", count);
#else
	atomic_store_int(count, REFCOUNT_SATURATION_VALUE);
#endif
}

static __inline void
refcount_init(volatile u_int *count, u_int value)
{
	KASSERT(!REFCOUNT_SATURATED(value),
	    ("invalid initial refcount value %u", value));
	*count = value;
}

static __inline void
refcount_acquire(volatile u_int *count)
{
	u_int old;

	old = atomic_fetchadd_int(count, 1);
	if (__predict_false(REFCOUNT_SATURATED(old)))
		_refcount_update_saturated(count);
}

static __inline void
refcount_acquiren(volatile u_int *count, u_int n)
{
	u_int old;

	KASSERT(n < REFCOUNT_SATURATION_VALUE / 2,
	    ("refcount_acquiren: n=%u too large", n));
	old = atomic_fetchadd_int(count, n);
	if (__predict_false(REFCOUNT_SATURATED(old)))
		_refcount_update_saturated(count);
}

static __inline __result_use_check bool
refcount_acquire_checked(volatile u_int *count)
{
	u_int lcount;

	for (lcount = *count;;) {
		if (__predict_false(REFCOUNT_SATURATED(lcount + 1)))
			return (false);
		if (__predict_true(atomic_fcmpset_int(count, &lcount,
		    lcount + 1) == 1))
			return (true);
	}
}

static __inline bool
refcount_releasen(volatile u_int *count, u_int n)
{
	u_int old;

	KASSERT(n < REFCOUNT_SATURATION_VALUE / 2,
	    ("refcount_releasen: n=%u too large", n));

	atomic_thread_fence_rel();
	old = atomic_fetchadd_int(count, -n);
	if (__predict_false(n >= REFCOUNT_COUNT(old) ||
	    REFCOUNT_SATURATED(old)))
		return (refcount_release_last(count, n, old));
	return (false);
}

static __inline bool
refcount_release(volatile u_int *count)
{

	return (refcount_releasen(count, 1));
}

static __inline void
refcount_wait(volatile u_int *count, const char *wmesg, int prio)
{

	while (*count != 0)
		refcount_sleep(count, wmesg, prio);
}

/*
 * This functions returns non-zero if the refcount was
 * incremented. Else zero is returned.
 */
static __inline __result_use_check bool
refcount_acquire_if_not_zero(volatile u_int *count)
{
	u_int old;

	old = *count;
	for (;;) {
		if (REFCOUNT_COUNT(old) == 0)
			return (false);
		if (__predict_false(REFCOUNT_SATURATED(old)))
			return (true);
		if (atomic_fcmpset_int(count, &old, old + 1))
			return (true);
	}
}

static __inline __result_use_check bool
refcount_release_if_not_last(volatile u_int *count)
{
	u_int old;

	old = *count;
	for (;;) {
		if (REFCOUNT_COUNT(old) == 1)
			return (false);
		if (__predict_false(REFCOUNT_SATURATED(old)))
			return (true);
		if (atomic_fcmpset_int(count, &old, old - 1))
			return (true);
	}
}

static __inline __result_use_check bool
refcount_release_if_gt(volatile u_int *count, u_int n)
{
	u_int old;

	KASSERT(n > 0,
	    ("refcount_release_if_gt: Use refcount_release for final ref"));
	old = *count;
	for (;;) {
		if (REFCOUNT_COUNT(old) <= n)
			return (false);
		if (__predict_false(REFCOUNT_SATURATED(old)))
			return (true);
		if (atomic_fcmpset_int(count, &old, old - 1))
			return (true);
	}
}

#endif	/* ! __SYS_REFCOUNT_H__ */
