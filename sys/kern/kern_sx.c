/*
 * Copyright (C) 2001 Jason Evans <jasone@freebsd.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible 
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Shared/exclusive locks.  This implementation assures deterministic lock
 * granting behavior, so that slocks and xlocks are interleaved.
 *
 * Priority propagation will not generally raise the priority of lock holders,
 * so should not be relied upon in combination with sx locks.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>

struct lock_class lock_class_sx = {
	"sx",
	LC_SLEEPLOCK | LC_SLEEPABLE | LC_RECURSABLE
};

void
sx_init(struct sx *sx, const char *description)
{
	struct lock_object *lock;

	bzero(sx, sizeof(*sx));
	lock = &sx->sx_object;
	lock->lo_class = &lock_class_sx;
	lock->lo_name = description;
	lock->lo_flags = LO_WITNESS | LO_RECURSABLE | LO_SLEEPABLE;
	mtx_init(&sx->sx_lock, "sx backing lock",
	    MTX_DEF | MTX_NOWITNESS | MTX_QUIET);
	sx->sx_cnt = 0;
	cv_init(&sx->sx_shrd_cv, description);
	sx->sx_shrd_wcnt = 0;
	cv_init(&sx->sx_excl_cv, description);
	sx->sx_excl_wcnt = 0;
	sx->sx_xholder = NULL;

	LOCK_LOG_INIT(lock, 0);

	WITNESS_INIT(lock);
}

void
sx_destroy(struct sx *sx)
{

	LOCK_LOG_DESTROY(&sx->sx_object, 0);

	KASSERT((sx->sx_cnt == 0 && sx->sx_shrd_wcnt == 0 && sx->sx_excl_wcnt ==
	    0), ("%s (%s): holders or waiters\n", __FUNCTION__,
	    sx->sx_object.lo_name));

	mtx_destroy(&sx->sx_lock);
	cv_destroy(&sx->sx_shrd_cv);
	cv_destroy(&sx->sx_excl_cv);

	WITNESS_DESTROY(&sx->sx_object);
}

void
_sx_slock(struct sx *sx, const char *file, int line)
{

	mtx_lock(&sx->sx_lock);
	KASSERT(sx->sx_xholder != curproc,
	    ("%s (%s): trying to get slock while xlock is held\n", __FUNCTION__,
	    sx->sx_object.lo_name));

	/*
	 * Loop in case we lose the race for lock acquisition.
	 */
	while (sx->sx_cnt < 0) {
		sx->sx_shrd_wcnt++;
		cv_wait(&sx->sx_shrd_cv, &sx->sx_lock);
		sx->sx_shrd_wcnt--;
	}

	/* Acquire a shared lock. */
	sx->sx_cnt++;

	LOCK_LOG_LOCK("SLOCK", &sx->sx_object, 0, 0, file, line);
	WITNESS_LOCK(&sx->sx_object, 0, file, line);

	mtx_unlock(&sx->sx_lock);
}

void
_sx_xlock(struct sx *sx, const char *file, int line)
{

	mtx_lock(&sx->sx_lock);

	/*
	 * With sx locks, we're absolutely not permitted to recurse on
	 * xlocks, as it is fatal (deadlock). Normally, recursion is handled
	 * by WITNESS, but as it is not semantically correct to hold the
	 * xlock while in here, we consider it API abuse and put it under
	 * INVARIANTS.
	 */
	KASSERT(sx->sx_xholder != curproc,
	    ("%s (%s): xlock already held @ %s:%d", __FUNCTION__,
	    sx->sx_object.lo_name, file, line));

	/* Loop in case we lose the race for lock acquisition. */
	while (sx->sx_cnt != 0) {
		sx->sx_excl_wcnt++;
		cv_wait(&sx->sx_excl_cv, &sx->sx_lock);
		sx->sx_excl_wcnt--;
	}

	MPASS(sx->sx_cnt == 0);

	/* Acquire an exclusive lock. */
	sx->sx_cnt--;
	sx->sx_xholder = curproc;

	LOCK_LOG_LOCK("XLOCK", &sx->sx_object, 0, 0, file, line);
	WITNESS_LOCK(&sx->sx_object, LOP_EXCLUSIVE, file, line);

	mtx_unlock(&sx->sx_lock);
}

void
_sx_sunlock(struct sx *sx, const char *file, int line)
{

	mtx_lock(&sx->sx_lock);
	_SX_ASSERT_SLOCKED(sx);

	WITNESS_UNLOCK(&sx->sx_object, 0, file, line);

	/* Release. */
	sx->sx_cnt--;

	/*
	 * If we just released the last shared lock, wake any waiters up, giving
	 * exclusive lockers precedence.  In order to make sure that exclusive
	 * lockers won't be blocked forever, don't wake shared lock waiters if
	 * there are exclusive lock waiters.
	 */
	if (sx->sx_excl_wcnt > 0) {
		if (sx->sx_cnt == 0)
			cv_signal(&sx->sx_excl_cv);
	} else if (sx->sx_shrd_wcnt > 0)
		cv_broadcast(&sx->sx_shrd_cv);

	LOCK_LOG_LOCK("SUNLOCK", &sx->sx_object, 0, 0, file, line);

	mtx_unlock(&sx->sx_lock);
}

void
_sx_xunlock(struct sx *sx, const char *file, int line)
{

	mtx_lock(&sx->sx_lock);
	_SX_ASSERT_XLOCKED(sx);
	MPASS(sx->sx_cnt == -1);

	WITNESS_UNLOCK(&sx->sx_object, LOP_EXCLUSIVE, file, line);

	/* Release. */
	sx->sx_cnt++;
	sx->sx_xholder = NULL;

	/*
	 * Wake up waiters if there are any.  Give precedence to slock waiters.
	 */
	if (sx->sx_shrd_wcnt > 0)
		cv_broadcast(&sx->sx_shrd_cv);
	else if (sx->sx_excl_wcnt > 0)
		cv_signal(&sx->sx_excl_cv);

	LOCK_LOG_LOCK("XUNLOCK", &sx->sx_object, 0, 0, file, line);

	mtx_unlock(&sx->sx_lock);
}
