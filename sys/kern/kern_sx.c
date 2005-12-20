/*-
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
 */

/*
 * Shared/exclusive locks.  This implementation assures deterministic lock
 * granting behavior, so that slocks and xlocks are interleaved.
 *
 * Priority propagation will not generally raise the priority of lock holders,
 * so should not be relied upon in combination with sx locks.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/linker_set.h>
#include <sys/condvar.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>

#include <ddb/ddb.h>

#ifdef DDB
static void	db_show_sx(struct lock_object *lock);
#endif

struct lock_class lock_class_sx = {
	"sx",
	LC_SLEEPLOCK | LC_SLEEPABLE | LC_RECURSABLE | LC_UPGRADABLE,
#ifdef DDB
	db_show_sx
#endif
};

#ifndef INVARIANTS
#define	_sx_assert(sx, what, file, line)
#endif

void
sx_sysinit(void *arg)
{
	struct sx_args *sargs = arg;

	sx_init(sargs->sa_sx, sargs->sa_desc);
}

void
sx_init(struct sx *sx, const char *description)
{
	struct lock_object *lock;

	lock = &sx->sx_object;
	KASSERT((lock->lo_flags & LO_INITIALIZED) == 0,
	    ("sx lock %s %p already initialized", description, sx));
	bzero(sx, sizeof(*sx));
	lock->lo_class = &lock_class_sx;
	lock->lo_type = lock->lo_name = description;
	lock->lo_flags = LO_WITNESS | LO_RECURSABLE | LO_SLEEPABLE |
	    LO_UPGRADABLE;
	sx->sx_lock = mtx_pool_find(mtxpool_lockbuilder, sx);
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
	    0), ("%s (%s): holders or waiters\n", __func__,
	    sx->sx_object.lo_name));

	sx->sx_lock = NULL;
	cv_destroy(&sx->sx_shrd_cv);
	cv_destroy(&sx->sx_excl_cv);

	WITNESS_DESTROY(&sx->sx_object);
}

void
_sx_slock(struct sx *sx, const char *file, int line)
{

	mtx_lock(sx->sx_lock);
	KASSERT(sx->sx_xholder != curthread,
	    ("%s (%s): slock while xlock is held @ %s:%d\n", __func__,
	    sx->sx_object.lo_name, file, line));
	WITNESS_CHECKORDER(&sx->sx_object, LOP_NEWORDER, file, line);

	/*
	 * Loop in case we lose the race for lock acquisition.
	 */
	while (sx->sx_cnt < 0) {
		sx->sx_shrd_wcnt++;
		cv_wait(&sx->sx_shrd_cv, sx->sx_lock);
		sx->sx_shrd_wcnt--;
	}

	/* Acquire a shared lock. */
	sx->sx_cnt++;

	LOCK_LOG_LOCK("SLOCK", &sx->sx_object, 0, 0, file, line);
	WITNESS_LOCK(&sx->sx_object, 0, file, line);

	mtx_unlock(sx->sx_lock);
}

int
_sx_try_slock(struct sx *sx, const char *file, int line)
{

	mtx_lock(sx->sx_lock);
	if (sx->sx_cnt >= 0) {
		sx->sx_cnt++;
		LOCK_LOG_TRY("SLOCK", &sx->sx_object, 0, 1, file, line);
		WITNESS_LOCK(&sx->sx_object, LOP_TRYLOCK, file, line);
		mtx_unlock(sx->sx_lock);
		return (1);
	} else {
		LOCK_LOG_TRY("SLOCK", &sx->sx_object, 0, 0, file, line);
		mtx_unlock(sx->sx_lock);
		return (0);
	}
}

void
_sx_xlock(struct sx *sx, const char *file, int line)
{

	mtx_lock(sx->sx_lock);

	/*
	 * With sx locks, we're absolutely not permitted to recurse on
	 * xlocks, as it is fatal (deadlock). Normally, recursion is handled
	 * by WITNESS, but as it is not semantically correct to hold the
	 * xlock while in here, we consider it API abuse and put it under
	 * INVARIANTS.
	 */
	KASSERT(sx->sx_xholder != curthread,
	    ("%s (%s): xlock already held @ %s:%d", __func__,
	    sx->sx_object.lo_name, file, line));
	WITNESS_CHECKORDER(&sx->sx_object, LOP_NEWORDER | LOP_EXCLUSIVE, file,
	    line);

	/* Loop in case we lose the race for lock acquisition. */
	while (sx->sx_cnt != 0) {
		sx->sx_excl_wcnt++;
		cv_wait(&sx->sx_excl_cv, sx->sx_lock);
		sx->sx_excl_wcnt--;
	}

	MPASS(sx->sx_cnt == 0);

	/* Acquire an exclusive lock. */
	sx->sx_cnt--;
	sx->sx_xholder = curthread;

	LOCK_LOG_LOCK("XLOCK", &sx->sx_object, 0, 0, file, line);
	WITNESS_LOCK(&sx->sx_object, LOP_EXCLUSIVE, file, line);

	mtx_unlock(sx->sx_lock);
}

int
_sx_try_xlock(struct sx *sx, const char *file, int line)
{

	mtx_lock(sx->sx_lock);
	if (sx->sx_cnt == 0) {
		sx->sx_cnt--;
		sx->sx_xholder = curthread;
		LOCK_LOG_TRY("XLOCK", &sx->sx_object, 0, 1, file, line);
		WITNESS_LOCK(&sx->sx_object, LOP_EXCLUSIVE | LOP_TRYLOCK, file,
		    line);
		mtx_unlock(sx->sx_lock);
		return (1);
	} else {
		LOCK_LOG_TRY("XLOCK", &sx->sx_object, 0, 0, file, line);
		mtx_unlock(sx->sx_lock);
		return (0);
	}
}

void
_sx_sunlock(struct sx *sx, const char *file, int line)
{

	_sx_assert(sx, SX_SLOCKED, file, line);
	mtx_lock(sx->sx_lock);

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

	mtx_unlock(sx->sx_lock);
}

void
_sx_xunlock(struct sx *sx, const char *file, int line)
{

	_sx_assert(sx, SX_XLOCKED, file, line);
	mtx_lock(sx->sx_lock);
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

	mtx_unlock(sx->sx_lock);
}

int
_sx_try_upgrade(struct sx *sx, const char *file, int line)
{

	_sx_assert(sx, SX_SLOCKED, file, line);
	mtx_lock(sx->sx_lock);

	if (sx->sx_cnt == 1) {
		sx->sx_cnt = -1;
		sx->sx_xholder = curthread;

		LOCK_LOG_TRY("XUPGRADE", &sx->sx_object, 0, 1, file, line);
		WITNESS_UPGRADE(&sx->sx_object, LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);

		mtx_unlock(sx->sx_lock);
		return (1);
	} else {
		LOCK_LOG_TRY("XUPGRADE", &sx->sx_object, 0, 0, file, line);
		mtx_unlock(sx->sx_lock);
		return (0);
	}
}

void
_sx_downgrade(struct sx *sx, const char *file, int line)
{

	_sx_assert(sx, SX_XLOCKED, file, line);
	mtx_lock(sx->sx_lock);
	MPASS(sx->sx_cnt == -1);

	WITNESS_DOWNGRADE(&sx->sx_object, 0, file, line);

	sx->sx_cnt = 1;
	sx->sx_xholder = NULL;
        if (sx->sx_shrd_wcnt > 0)
                cv_broadcast(&sx->sx_shrd_cv);

	LOCK_LOG_LOCK("XDOWNGRADE", &sx->sx_object, 0, 0, file, line);

	mtx_unlock(sx->sx_lock);
}

#ifdef INVARIANT_SUPPORT
#ifndef INVARIANTS
#undef	_sx_assert
#endif

/*
 * In the non-WITNESS case, sx_assert() can only detect that at least
 * *some* thread owns an slock, but it cannot guarantee that *this*
 * thread owns an slock.
 */
void
_sx_assert(struct sx *sx, int what, const char *file, int line)
{

	if (panicstr != NULL)
		return;
	switch (what) {
	case SX_LOCKED:
	case SX_SLOCKED:
#ifdef WITNESS
		witness_assert(&sx->sx_object, what, file, line);
#else
		mtx_lock(sx->sx_lock);
		if (sx->sx_cnt <= 0 &&
		    (what == SX_SLOCKED || sx->sx_xholder != curthread))
			panic("Lock %s not %slocked @ %s:%d\n",
			    sx->sx_object.lo_name, (what == SX_SLOCKED) ?
			    "share " : "", file, line);
		mtx_unlock(sx->sx_lock);
#endif
		break;
	case SX_XLOCKED:
		mtx_lock(sx->sx_lock);
		if (sx->sx_xholder != curthread)
			panic("Lock %s not exclusively locked @ %s:%d\n",
			    sx->sx_object.lo_name, file, line);
		mtx_unlock(sx->sx_lock);
		break;
	case SX_UNLOCKED:
#ifdef WITNESS
		witness_assert(&sx->sx_object, what, file, line);
#else
		/*
		 * We are able to check only exclusive lock here,
		 * we cannot assert that *this* thread owns slock.
		 */
		mtx_lock(sx->sx_lock);
		if (sx->sx_xholder == curthread)
			panic("Lock %s exclusively locked @ %s:%d\n",
			    sx->sx_object.lo_name, file, line);
		mtx_unlock(sx->sx_lock);
#endif
		break;
	default:
		panic("Unknown sx lock assertion: %d @ %s:%d", what, file,
		    line);
	}
}
#endif	/* INVARIANT_SUPPORT */

#ifdef DDB
void
db_show_sx(struct lock_object *lock)
{
	struct thread *td;
	struct sx *sx;

	sx = (struct sx *)lock;

	db_printf(" state: ");
	if (sx->sx_cnt < 0) {
		td = sx->sx_xholder;
		db_printf("XLOCK: %p (tid %d, pid %d, \"%s\")\n", td,
		    td->td_tid, td->td_proc->p_pid, td->td_proc->p_comm);
	} else if (sx->sx_cnt > 0)
		db_printf("SLOCK: %d locks\n", sx->sx_cnt);
	else
		db_printf("UNLOCKED\n");
	db_printf(" waiters: %d shared, %d exclusive\n", sx->sx_shrd_wcnt,
	    sx->sx_excl_wcnt);
}
#endif
