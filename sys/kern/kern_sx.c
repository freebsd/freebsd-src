/*-
 * Copyright (c) 2007 Attilio Rao <attilio@freebsd.org>
 * Copyright (c) 2001 Jason Evans <jasone@freebsd.org>
 * All rights reserved.
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
 * Shared/exclusive locks.  This implementation attempts to ensure
 * deterministic lock granting behavior, so that slocks and xlocks are
 * interleaved.
 *
 * Priority propagation will not generally raise the priority of lock holders,
 * so should not be relied upon in combination with sx locks.
 */

#include "opt_adaptive_sx.h"
#include "opt_ddb.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sleepqueue.h>
#include <sys/sx.h>
#include <sys/systm.h>

#ifdef ADAPTIVE_SX
#include <machine/cpu.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

#if !defined(SMP) && defined(ADAPTIVE_SX)
#error "You must have SMP to enable the ADAPTIVE_SX option"
#endif

CTASSERT(((SX_ADAPTIVESPIN | SX_RECURSE) & LO_CLASSFLAGS) ==
    (SX_ADAPTIVESPIN | SX_RECURSE));

/* Handy macros for sleep queues. */
#define	SQ_EXCLUSIVE_QUEUE	0
#define	SQ_SHARED_QUEUE		1

/*
 * Variations on DROP_GIANT()/PICKUP_GIANT() for use in this file.  We
 * drop Giant anytime we have to sleep or if we adaptively spin.
 */
#define	GIANT_DECLARE							\
	int _giantcnt = 0;						\
	WITNESS_SAVE_DECL(Giant)					\

#define	GIANT_SAVE() do {						\
	if (mtx_owned(&Giant)) {					\
		WITNESS_SAVE(&Giant.mtx_object, Giant);		\
		while (mtx_owned(&Giant)) {				\
			_giantcnt++;					\
			mtx_unlock(&Giant);				\
		}							\
	}								\
} while (0)

#define GIANT_RESTORE() do {						\
	if (_giantcnt > 0) {						\
		mtx_assert(&Giant, MA_NOTOWNED);			\
		while (_giantcnt--)					\
			mtx_lock(&Giant);				\
		WITNESS_RESTORE(&Giant.mtx_object, Giant);		\
	}								\
} while (0)

/*
 * Returns true if an exclusive lock is recursed.  It assumes
 * curthread currently has an exclusive lock.
 */
#define	sx_recursed(sx)		((sx)->sx_recurse != 0)

#ifdef DDB
static void	db_show_sx(struct lock_object *lock);
#endif

struct lock_class lock_class_sx = {
	.lc_name = "sx",
	.lc_flags = LC_SLEEPLOCK | LC_SLEEPABLE | LC_RECURSABLE | LC_UPGRADABLE,
#ifdef DDB
	.lc_ddb_show = db_show_sx,
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

	sx_init_flags(sx, description, 0);
}

void
sx_init_flags(struct sx *sx, const char *description, int opts)
{
	int flags;

	MPASS((opts & ~(SX_QUIET | SX_RECURSE | SX_NOWITNESS | SX_DUPOK |
	    SX_NOPROFILE | SX_ADAPTIVESPIN)) == 0);

	flags = LO_RECURSABLE | LO_SLEEPABLE | LO_UPGRADABLE;
	if (opts & SX_DUPOK)
		flags |= LO_DUPOK;
	if (!(opts & SX_NOWITNESS))
		flags |= LO_WITNESS;
	if (opts & SX_QUIET)
		flags |= LO_QUIET;

	flags |= opts & (SX_ADAPTIVESPIN | SX_RECURSE);
	sx->sx_lock = SX_LOCK_UNLOCKED;
	sx->sx_recurse = 0;
	lock_init(&sx->lock_object, &lock_class_sx, description, NULL, flags);
}

void
sx_destroy(struct sx *sx)
{

	KASSERT(sx->sx_lock == SX_LOCK_UNLOCKED, ("sx lock still held"));
	KASSERT(sx->sx_recurse == 0, ("sx lock still recursed"));
	sx->sx_lock = SX_LOCK_DESTROYED;
	lock_destroy(&sx->lock_object);
}

int
_sx_slock(struct sx *sx, int opts, const char *file, int line)
{
	int error = 0;

	MPASS(curthread != NULL);
	KASSERT(sx->sx_lock != SX_LOCK_DESTROYED,
	    ("sx_slock() of destroyed sx @ %s:%d", file, line));
	WITNESS_CHECKORDER(&sx->lock_object, LOP_NEWORDER, file, line);
	error = __sx_slock(sx, opts, file, line);
	if (!error) {
		LOCK_LOG_LOCK("SLOCK", &sx->lock_object, 0, 0, file, line);
		WITNESS_LOCK(&sx->lock_object, 0, file, line);
		curthread->td_locks++;
	}

	return (error);
}

int
_sx_try_slock(struct sx *sx, const char *file, int line)
{
	uintptr_t x;

	x = sx->sx_lock;
	KASSERT(x != SX_LOCK_DESTROYED,
	    ("sx_try_slock() of destroyed sx @ %s:%d", file, line));
	if ((x & SX_LOCK_SHARED) && atomic_cmpset_acq_ptr(&sx->sx_lock, x,
	    x + SX_ONE_SHARER)) {
		LOCK_LOG_TRY("SLOCK", &sx->lock_object, 0, 1, file, line);
		WITNESS_LOCK(&sx->lock_object, LOP_TRYLOCK, file, line);
		curthread->td_locks++;
		return (1);
	}

	LOCK_LOG_TRY("SLOCK", &sx->lock_object, 0, 0, file, line);
	return (0);
}

int
_sx_xlock(struct sx *sx, int opts, const char *file, int line)
{
	int error = 0;

	MPASS(curthread != NULL);
	KASSERT(sx->sx_lock != SX_LOCK_DESTROYED,
	    ("sx_xlock() of destroyed sx @ %s:%d", file, line));
	WITNESS_CHECKORDER(&sx->lock_object, LOP_NEWORDER | LOP_EXCLUSIVE, file,
	    line);
	error = __sx_xlock(sx, curthread, opts, file, line);
	if (!error) {
		LOCK_LOG_LOCK("XLOCK", &sx->lock_object, 0, sx->sx_recurse,
		    file, line);
		WITNESS_LOCK(&sx->lock_object, LOP_EXCLUSIVE, file, line);
		curthread->td_locks++;
	}

	return (error);
}

int
_sx_try_xlock(struct sx *sx, const char *file, int line)
{
	int rval;

	MPASS(curthread != NULL);
	KASSERT(sx->sx_lock != SX_LOCK_DESTROYED,
	    ("sx_try_xlock() of destroyed sx @ %s:%d", file, line));

	if (sx_xlocked(sx) && (sx->lock_object.lo_flags & SX_RECURSE) != 0) {
		sx->sx_recurse++;
		atomic_set_ptr(&sx->sx_lock, SX_LOCK_RECURSED);
		rval = 1;
	} else
		rval = atomic_cmpset_acq_ptr(&sx->sx_lock, SX_LOCK_UNLOCKED,
		    (uintptr_t)curthread);
	LOCK_LOG_TRY("XLOCK", &sx->lock_object, 0, rval, file, line);
	if (rval) {
		WITNESS_LOCK(&sx->lock_object, LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
		curthread->td_locks++;
	}

	return (rval);
}

void
_sx_sunlock(struct sx *sx, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(sx->sx_lock != SX_LOCK_DESTROYED,
	    ("sx_sunlock() of destroyed sx @ %s:%d", file, line));
	_sx_assert(sx, SA_SLOCKED, file, line);
	curthread->td_locks--;
	WITNESS_UNLOCK(&sx->lock_object, 0, file, line);
	LOCK_LOG_LOCK("SUNLOCK", &sx->lock_object, 0, 0, file, line);
#ifdef LOCK_PROFILING_SHARED
	if (SX_SHARERS(sx->sx_lock) == 1)
		lock_profile_release_lock(&sx->lock_object);
#endif
	__sx_sunlock(sx, file, line);
}

void
_sx_xunlock(struct sx *sx, const char *file, int line)
{

	MPASS(curthread != NULL);
	KASSERT(sx->sx_lock != SX_LOCK_DESTROYED,
	    ("sx_xunlock() of destroyed sx @ %s:%d", file, line));
	_sx_assert(sx, SA_XLOCKED, file, line);
	curthread->td_locks--;
	WITNESS_UNLOCK(&sx->lock_object, LOP_EXCLUSIVE, file, line);
	LOCK_LOG_LOCK("XUNLOCK", &sx->lock_object, 0, sx->sx_recurse, file,
	    line);
	if (!sx_recursed(sx))
		lock_profile_release_lock(&sx->lock_object);
	__sx_xunlock(sx, curthread, file, line);
}

/*
 * Try to do a non-blocking upgrade from a shared lock to an exclusive lock.
 * This will only succeed if this thread holds a single shared lock.
 * Return 1 if if the upgrade succeed, 0 otherwise.
 */
int
_sx_try_upgrade(struct sx *sx, const char *file, int line)
{
	uintptr_t x;
	int success;

	KASSERT(sx->sx_lock != SX_LOCK_DESTROYED,
	    ("sx_try_upgrade() of destroyed sx @ %s:%d", file, line));
	_sx_assert(sx, SA_SLOCKED, file, line);

	/*
	 * Try to switch from one shared lock to an exclusive lock.  We need
	 * to maintain the SX_LOCK_EXCLUSIVE_WAITERS flag if set so that
	 * we will wake up the exclusive waiters when we drop the lock.
	 */
	x = sx->sx_lock & SX_LOCK_EXCLUSIVE_WAITERS;
	success = atomic_cmpset_ptr(&sx->sx_lock, SX_SHARERS_LOCK(1) | x,
	    (uintptr_t)curthread | x);
	LOCK_LOG_TRY("XUPGRADE", &sx->lock_object, 0, success, file, line);
	if (success)
		WITNESS_UPGRADE(&sx->lock_object, LOP_EXCLUSIVE | LOP_TRYLOCK,
		    file, line);
	return (success);
}

/*
 * Downgrade an unrecursed exclusive lock into a single shared lock.
 */
void
_sx_downgrade(struct sx *sx, const char *file, int line)
{
	uintptr_t x;

	KASSERT(sx->sx_lock != SX_LOCK_DESTROYED,
	    ("sx_downgrade() of destroyed sx @ %s:%d", file, line));
	_sx_assert(sx, SA_XLOCKED | SA_NOTRECURSED, file, line);
#ifndef INVARIANTS
	if (sx_recursed(sx))
		panic("downgrade of a recursed lock");
#endif

	WITNESS_DOWNGRADE(&sx->lock_object, 0, file, line);

	/*
	 * Try to switch from an exclusive lock with no shared waiters
	 * to one sharer with no shared waiters.  If there are
	 * exclusive waiters, we don't need to lock the sleep queue so
	 * long as we preserve the flag.  We do one quick try and if
	 * that fails we grab the sleepq lock to keep the flags from
	 * changing and do it the slow way.
	 *
	 * We have to lock the sleep queue if there are shared waiters
	 * so we can wake them up.
	 */
	x = sx->sx_lock;
	if (!(x & SX_LOCK_SHARED_WAITERS) &&
	    atomic_cmpset_rel_ptr(&sx->sx_lock, x, SX_SHARERS_LOCK(1) |
	    (x & SX_LOCK_EXCLUSIVE_WAITERS))) {
		LOCK_LOG_LOCK("XDOWNGRADE", &sx->lock_object, 0, 0, file, line);
		return;
	}

	/*
	 * Lock the sleep queue so we can read the waiters bits
	 * without any races and wakeup any shared waiters.
	 */
	sleepq_lock(&sx->lock_object);

	/*
	 * Preserve SX_LOCK_EXCLUSIVE_WAITERS while downgraded to a single
	 * shared lock.  If there are any shared waiters, wake them up.
	 */
	x = sx->sx_lock;
	atomic_store_rel_ptr(&sx->sx_lock, SX_SHARERS_LOCK(1) |
	    (x & SX_LOCK_EXCLUSIVE_WAITERS));
	if (x & SX_LOCK_SHARED_WAITERS)
		sleepq_broadcast(&sx->lock_object, SLEEPQ_SX, -1,
		    SQ_SHARED_QUEUE);
	else
		sleepq_release(&sx->lock_object);

	LOCK_LOG_LOCK("XDOWNGRADE", &sx->lock_object, 0, 0, file, line);
}

/*
 * This function represents the so-called 'hard case' for sx_xlock
 * operation.  All 'easy case' failures are redirected to this.  Note
 * that ideally this would be a static function, but it needs to be
 * accessible from at least sx.h.
 */
int
_sx_xlock_hard(struct sx *sx, uintptr_t tid, int opts, const char *file,
    int line)
{
	GIANT_DECLARE;
#ifdef ADAPTIVE_SX
	volatile struct thread *owner;
#endif
	/* uint64_t waittime = 0; */
	uintptr_t x;
	int /* contested = 0, */error = 0;

	/* If we already hold an exclusive lock, then recurse. */
	if (sx_xlocked(sx)) {
		KASSERT((sx->lock_object.lo_flags & SX_RECURSE) != 0,
	    ("_sx_xlock_hard: recursed on non-recursive sx %s @ %s:%d\n",
		    sx->lock_object.lo_name, file, line));
		sx->sx_recurse++;
		atomic_set_ptr(&sx->sx_lock, SX_LOCK_RECURSED);
		if (LOCK_LOG_TEST(&sx->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p recursing", __func__, sx);
		return (0);
	}

	if (LOCK_LOG_TEST(&sx->lock_object, 0))
		CTR5(KTR_LOCK, "%s: %s contested (lock=%p) at %s:%d", __func__,
		    sx->lock_object.lo_name, (void *)sx->sx_lock, file, line);

	while (!atomic_cmpset_acq_ptr(&sx->sx_lock, SX_LOCK_UNLOCKED, tid)) {
#ifdef ADAPTIVE_SX
		/*
		 * If the lock is write locked and the owner is
		 * running on another CPU, spin until the owner stops
		 * running or the state of the lock changes.
		 */
		x = sx->sx_lock;
		if (!(x & SX_LOCK_SHARED) &&
		    (sx->lock_object.lo_flags & SX_ADAPTIVESPIN)) {
			x = SX_OWNER(x);
			owner = (struct thread *)x;
			if (TD_IS_RUNNING(owner)) {
				if (LOCK_LOG_TEST(&sx->lock_object, 0))
					CTR3(KTR_LOCK,
					    "%s: spinning on %p held by %p",
					    __func__, sx, owner);
				GIANT_SAVE();
				lock_profile_obtain_lock_failed(
				    &sx->lock_object, &contested, &waittime);
				while (SX_OWNER(sx->sx_lock) == x &&
				    TD_IS_RUNNING(owner))
					cpu_spinwait();
				continue;
			}
		}
#endif

		sleepq_lock(&sx->lock_object);
		x = sx->sx_lock;

		/*
		 * If the lock was released while spinning on the
		 * sleep queue chain lock, try again.
		 */
		if (x == SX_LOCK_UNLOCKED) {
			sleepq_release(&sx->lock_object);
			continue;
		}

#ifdef ADAPTIVE_SX
		/*
		 * The current lock owner might have started executing
		 * on another CPU (or the lock could have changed
		 * owners) while we were waiting on the sleep queue
		 * chain lock.  If so, drop the sleep queue lock and try
		 * again.
		 */
		if (!(x & SX_LOCK_SHARED) &&
		    (sx->lock_object.lo_flags & SX_ADAPTIVESPIN)) {
			owner = (struct thread *)SX_OWNER(x);
			if (TD_IS_RUNNING(owner)) {
				sleepq_release(&sx->lock_object);
				continue;
			}
		}
#endif

		/*
		 * If an exclusive lock was released with both shared
		 * and exclusive waiters and a shared waiter hasn't
		 * woken up and acquired the lock yet, sx_lock will be
		 * set to SX_LOCK_UNLOCKED | SX_LOCK_EXCLUSIVE_WAITERS.
		 * If we see that value, try to acquire it once.  Note
		 * that we have to preserve SX_LOCK_EXCLUSIVE_WAITERS
		 * as there are other exclusive waiters still.  If we
		 * fail, restart the loop.
		 */
		if (x == (SX_LOCK_UNLOCKED | SX_LOCK_EXCLUSIVE_WAITERS)) {
			if (atomic_cmpset_acq_ptr(&sx->sx_lock,
			    SX_LOCK_UNLOCKED | SX_LOCK_EXCLUSIVE_WAITERS,
			    tid | SX_LOCK_EXCLUSIVE_WAITERS)) {
				sleepq_release(&sx->lock_object);
				CTR2(KTR_LOCK, "%s: %p claimed by new writer",
				    __func__, sx);
				break;
			}
			sleepq_release(&sx->lock_object);
			continue;
		}

		/*
		 * Try to set the SX_LOCK_EXCLUSIVE_WAITERS.  If we fail,
		 * than loop back and retry.
		 */
		if (!(x & SX_LOCK_EXCLUSIVE_WAITERS)) {
			if (!atomic_cmpset_ptr(&sx->sx_lock, x,
			    x | SX_LOCK_EXCLUSIVE_WAITERS)) {
				sleepq_release(&sx->lock_object);
				continue;
			}
			if (LOCK_LOG_TEST(&sx->lock_object, 0))
				CTR2(KTR_LOCK, "%s: %p set excl waiters flag",
				    __func__, sx);
		}

		/*
		 * Since we have been unable to acquire the exclusive
		 * lock and the exclusive waiters flag is set, we have
		 * to sleep.
		 */
		if (LOCK_LOG_TEST(&sx->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p blocking on sleep queue",
			    __func__, sx);

		GIANT_SAVE();
		lock_profile_obtain_lock_failed(&sx->lock_object, &contested,
		    &waittime);
		sleepq_add(&sx->lock_object, NULL, sx->lock_object.lo_name,
		    SLEEPQ_SX | ((opts & SX_INTERRUPTIBLE) ?
		    SLEEPQ_INTERRUPTIBLE : 0), SQ_EXCLUSIVE_QUEUE);
		if (!(opts & SX_INTERRUPTIBLE))
			sleepq_wait(&sx->lock_object);
		else
			error = sleepq_wait_sig(&sx->lock_object);

		if (error) {
			if (LOCK_LOG_TEST(&sx->lock_object, 0))
				CTR2(KTR_LOCK,
			"%s: interruptible sleep by %p suspended by signal",
				    __func__, sx);
			break;
		}
		if (LOCK_LOG_TEST(&sx->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p resuming from sleep queue",
			    __func__, sx);
	}

	GIANT_RESTORE();
	if (!error)
		lock_profile_obtain_lock_success(&sx->lock_object, contested,
		    waittime, file, line);
	return (error);
}

/*
 * This function represents the so-called 'hard case' for sx_xunlock
 * operation.  All 'easy case' failures are redirected to this.  Note
 * that ideally this would be a static function, but it needs to be
 * accessible from at least sx.h.
 */
void
_sx_xunlock_hard(struct sx *sx, uintptr_t tid, const char *file, int line)
{
	uintptr_t x;
	int queue;

	MPASS(!(sx->sx_lock & SX_LOCK_SHARED));

	/* If the lock is recursed, then unrecurse one level. */
	if (sx_xlocked(sx) && sx_recursed(sx)) {
		if ((--sx->sx_recurse) == 0)
			atomic_clear_ptr(&sx->sx_lock, SX_LOCK_RECURSED);
		if (LOCK_LOG_TEST(&sx->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p unrecursing", __func__, sx);
		return;
	}
	MPASS(sx->sx_lock & (SX_LOCK_SHARED_WAITERS |
	    SX_LOCK_EXCLUSIVE_WAITERS));
	if (LOCK_LOG_TEST(&sx->lock_object, 0))
		CTR2(KTR_LOCK, "%s: %p contested", __func__, sx);

	sleepq_lock(&sx->lock_object);
	x = SX_LOCK_UNLOCKED;

	/*
	 * The wake up algorithm here is quite simple and probably not
	 * ideal.  It gives precedence to shared waiters if they are
	 * present.  For this condition, we have to preserve the
	 * state of the exclusive waiters flag.
	 */
	if (sx->sx_lock & SX_LOCK_SHARED_WAITERS) {
		queue = SQ_SHARED_QUEUE;
		x |= (sx->sx_lock & SX_LOCK_EXCLUSIVE_WAITERS);
	} else
		queue = SQ_EXCLUSIVE_QUEUE;

	/* Wake up all the waiters for the specific queue. */
	if (LOCK_LOG_TEST(&sx->lock_object, 0))
		CTR3(KTR_LOCK, "%s: %p waking up all threads on %s queue",
		    __func__, sx, queue == SQ_SHARED_QUEUE ? "shared" :
		    "exclusive");
	atomic_store_rel_ptr(&sx->sx_lock, x);
	sleepq_broadcast(&sx->lock_object, SLEEPQ_SX, -1, queue);
}

/*
 * This function represents the so-called 'hard case' for sx_slock
 * operation.  All 'easy case' failures are redirected to this.  Note
 * that ideally this would be a static function, but it needs to be
 * accessible from at least sx.h.
 */
int
_sx_slock_hard(struct sx *sx, int opts, const char *file, int line)
{
	GIANT_DECLARE;
#ifdef ADAPTIVE_SX
	volatile struct thread *owner;
#endif
#ifdef LOCK_PROFILING_SHARED
	uint64_t waittime = 0;
	int contested = 0;
#endif
	uintptr_t x;
	int error = 0;

	/*
	 * As with rwlocks, we don't make any attempt to try to block
	 * shared locks once there is an exclusive waiter.
	 */
	for (;;) {
		x = sx->sx_lock;

		/*
		 * If no other thread has an exclusive lock then try to bump up
		 * the count of sharers.  Since we have to preserve the state
		 * of SX_LOCK_EXCLUSIVE_WAITERS, if we fail to acquire the
		 * shared lock loop back and retry.
		 */
		if (x & SX_LOCK_SHARED) {
			MPASS(!(x & SX_LOCK_SHARED_WAITERS));
			if (atomic_cmpset_acq_ptr(&sx->sx_lock, x,
			    x + SX_ONE_SHARER)) {
#ifdef LOCK_PROFILING_SHARED
				if (SX_SHARERS(x) == 0)
					lock_profile_obtain_lock_success(
					    &sx->lock_object, contested,
					    waittime, file, line);
#endif
				if (LOCK_LOG_TEST(&sx->lock_object, 0))
					CTR4(KTR_LOCK,
					    "%s: %p succeed %p -> %p", __func__,
					    sx, (void *)x,
					    (void *)(x + SX_ONE_SHARER));
				break;
			}
			continue;
		}

#ifdef ADAPTIVE_SX
		/*
		 * If the owner is running on another CPU, spin until
		 * the owner stops running or the state of the lock
		 * changes.
		 */
		else if (sx->lock_object.lo_flags & SX_ADAPTIVESPIN) {
			x = SX_OWNER(x);
			owner = (struct thread *)x;
			if (TD_IS_RUNNING(owner)) {
				if (LOCK_LOG_TEST(&sx->lock_object, 0))
					CTR3(KTR_LOCK,
					    "%s: spinning on %p held by %p",
					    __func__, sx, owner);
				GIANT_SAVE();
#ifdef LOCK_PROFILING_SHARED
				lock_profile_obtain_lock_failed(
				    &sx->lock_object, &contested, &waittime);
#endif
				while (SX_OWNER(sx->sx_lock) == x &&
				    TD_IS_RUNNING(owner))
					cpu_spinwait();
				continue;
			}
		}
#endif

		/*
		 * Some other thread already has an exclusive lock, so
		 * start the process of blocking.
		 */
		sleepq_lock(&sx->lock_object);
		x = sx->sx_lock;

		/*
		 * The lock could have been released while we spun.
		 * In this case loop back and retry.
		 */
		if (x & SX_LOCK_SHARED) {
			sleepq_release(&sx->lock_object);
			continue;
		}

#ifdef ADAPTIVE_SX
		/*
		 * If the owner is running on another CPU, spin until
		 * the owner stops running or the state of the lock
		 * changes.
		 */
		if (!(x & SX_LOCK_SHARED) &&
		    (sx->lock_object.lo_flags & SX_ADAPTIVESPIN)) {
			owner = (struct thread *)SX_OWNER(x);
			if (TD_IS_RUNNING(owner)) {
				sleepq_release(&sx->lock_object);
				continue;
			}
		}
#endif

		/*
		 * Try to set the SX_LOCK_SHARED_WAITERS flag.  If we
		 * fail to set it drop the sleep queue lock and loop
		 * back.
		 */
		if (!(x & SX_LOCK_SHARED_WAITERS)) {
			if (!atomic_cmpset_ptr(&sx->sx_lock, x,
			    x | SX_LOCK_SHARED_WAITERS)) {
				sleepq_release(&sx->lock_object);
				continue;
			}
			if (LOCK_LOG_TEST(&sx->lock_object, 0))
				CTR2(KTR_LOCK, "%s: %p set shared waiters flag",
				    __func__, sx);
		}

		/*
		 * Since we have been unable to acquire the shared lock,
		 * we have to sleep.
		 */
		if (LOCK_LOG_TEST(&sx->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p blocking on sleep queue",
			    __func__, sx);

		GIANT_SAVE();
#ifdef LOCK_PROFILING_SHARED
		lock_profile_obtain_lock_failed(&sx->lock_object, &contested,
		    &waittime);
#endif
		sleepq_add(&sx->lock_object, NULL, sx->lock_object.lo_name,
		    SLEEPQ_SX | ((opts & SX_INTERRUPTIBLE) ?
		    SLEEPQ_INTERRUPTIBLE : 0), SQ_SHARED_QUEUE);
		if (!(opts & SX_INTERRUPTIBLE))
			sleepq_wait(&sx->lock_object);
		else
			error = sleepq_wait_sig(&sx->lock_object);

		if (error) {
			if (LOCK_LOG_TEST(&sx->lock_object, 0))
				CTR2(KTR_LOCK,
			"%s: interruptible sleep by %p suspended by signal",
				    __func__, sx);
			break;
		}
		if (LOCK_LOG_TEST(&sx->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p resuming from sleep queue",
			    __func__, sx);
	}

	GIANT_RESTORE();
	return (error);
}

/*
 * This function represents the so-called 'hard case' for sx_sunlock
 * operation.  All 'easy case' failures are redirected to this.  Note
 * that ideally this would be a static function, but it needs to be
 * accessible from at least sx.h.
 */
void
_sx_sunlock_hard(struct sx *sx, const char *file, int line)
{
	uintptr_t x;

	for (;;) {
		x = sx->sx_lock;

		/*
		 * We should never have sharers while at least one thread
		 * holds a shared lock.
		 */
		KASSERT(!(x & SX_LOCK_SHARED_WAITERS),
		    ("%s: waiting sharers", __func__));

		/*
		 * See if there is more than one shared lock held.  If
		 * so, just drop one and return.
		 */
		if (SX_SHARERS(x) > 1) {
			if (atomic_cmpset_ptr(&sx->sx_lock, x,
			    x - SX_ONE_SHARER)) {
				if (LOCK_LOG_TEST(&sx->lock_object, 0))
					CTR4(KTR_LOCK,
					    "%s: %p succeeded %p -> %p",
					    __func__, sx, (void *)x,
					    (void *)(x - SX_ONE_SHARER));
				break;
			}
			continue;
		}

		/*
		 * If there aren't any waiters for an exclusive lock,
		 * then try to drop it quickly.
		 */
		if (!(x & SX_LOCK_EXCLUSIVE_WAITERS)) {
			MPASS(x == SX_SHARERS_LOCK(1));
			if (atomic_cmpset_ptr(&sx->sx_lock, SX_SHARERS_LOCK(1),
			    SX_LOCK_UNLOCKED)) {
				if (LOCK_LOG_TEST(&sx->lock_object, 0))
					CTR2(KTR_LOCK, "%s: %p last succeeded",
					    __func__, sx);
				break;
			}
			continue;
		}

		/*
		 * At this point, there should just be one sharer with
		 * exclusive waiters.
		 */
		MPASS(x == (SX_SHARERS_LOCK(1) | SX_LOCK_EXCLUSIVE_WAITERS));

		sleepq_lock(&sx->lock_object);

		/*
		 * Wake up semantic here is quite simple:
		 * Just wake up all the exclusive waiters.
		 * Note that the state of the lock could have changed,
		 * so if it fails loop back and retry.
		 */
		if (!atomic_cmpset_ptr(&sx->sx_lock,
		    SX_SHARERS_LOCK(1) | SX_LOCK_EXCLUSIVE_WAITERS,
		    SX_LOCK_UNLOCKED)) {
			sleepq_release(&sx->lock_object);
			continue;
		}
		if (LOCK_LOG_TEST(&sx->lock_object, 0))
			CTR2(KTR_LOCK, "%s: %p waking up all thread on"
			    "exclusive queue", __func__, sx);
		sleepq_broadcast(&sx->lock_object, SLEEPQ_SX, -1,
		    SQ_EXCLUSIVE_QUEUE);
		break;
	}
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
#ifndef WITNESS
	int slocked = 0;
#endif

	if (panicstr != NULL)
		return;
	switch (what) {
	case SA_SLOCKED:
	case SA_SLOCKED | SA_NOTRECURSED:
	case SA_SLOCKED | SA_RECURSED:
#ifndef WITNESS
		slocked = 1;
		/* FALLTHROUGH */
#endif
	case SA_LOCKED:
	case SA_LOCKED | SA_NOTRECURSED:
	case SA_LOCKED | SA_RECURSED:
#ifdef WITNESS
		witness_assert(&sx->lock_object, what, file, line);
#else
		/*
		 * If some other thread has an exclusive lock or we
		 * have one and are asserting a shared lock, fail.
		 * Also, if no one has a lock at all, fail.
		 */
		if (sx->sx_lock == SX_LOCK_UNLOCKED ||
		    (!(sx->sx_lock & SX_LOCK_SHARED) && (slocked ||
		    sx_xholder(sx) != curthread)))
			panic("Lock %s not %slocked @ %s:%d\n",
			    sx->lock_object.lo_name, slocked ? "share " : "",
			    file, line);

		if (!(sx->sx_lock & SX_LOCK_SHARED)) {
			if (sx_recursed(sx)) {
				if (what & SA_NOTRECURSED)
					panic("Lock %s recursed @ %s:%d\n",
					    sx->lock_object.lo_name, file,
					    line);
			} else if (what & SA_RECURSED)
				panic("Lock %s not recursed @ %s:%d\n",
				    sx->lock_object.lo_name, file, line);
		}
#endif
		break;
	case SA_XLOCKED:
	case SA_XLOCKED | SA_NOTRECURSED:
	case SA_XLOCKED | SA_RECURSED:
		if (sx_xholder(sx) != curthread)
			panic("Lock %s not exclusively locked @ %s:%d\n",
			    sx->lock_object.lo_name, file, line);
		if (sx_recursed(sx)) {
			if (what & SA_NOTRECURSED)
				panic("Lock %s recursed @ %s:%d\n",
				    sx->lock_object.lo_name, file, line);
		} else if (what & SA_RECURSED)
			panic("Lock %s not recursed @ %s:%d\n",
			    sx->lock_object.lo_name, file, line);
		break;
	case SA_UNLOCKED:
#ifdef WITNESS
		witness_assert(&sx->lock_object, what, file, line);
#else
		/*
		 * If we hold an exclusve lock fail.  We can't
		 * reliably check to see if we hold a shared lock or
		 * not.
		 */
		if (sx_xholder(sx) == curthread)
			panic("Lock %s exclusively locked @ %s:%d\n",
			    sx->lock_object.lo_name, file, line);
#endif
		break;
	default:
		panic("Unknown sx lock assertion: %d @ %s:%d", what, file,
		    line);
	}
}
#endif	/* INVARIANT_SUPPORT */

#ifdef DDB
static void
db_show_sx(struct lock_object *lock)
{
	struct thread *td;
	struct sx *sx;

	sx = (struct sx *)lock;

	db_printf(" state: ");
	if (sx->sx_lock == SX_LOCK_UNLOCKED)
		db_printf("UNLOCKED\n");
	else if (sx->sx_lock == SX_LOCK_DESTROYED) {
		db_printf("DESTROYED\n");
		return;
	} else if (sx->sx_lock & SX_LOCK_SHARED)
		db_printf("SLOCK: %ju\n", (uintmax_t)SX_SHARERS(sx->sx_lock));
	else {
		td = sx_xholder(sx);
		db_printf("XLOCK: %p (tid %d, pid %d, \"%s\")\n", td,
		    td->td_tid, td->td_proc->p_pid, td->td_proc->p_comm);
		if (sx_recursed(sx))
			db_printf(" recursed: %d\n", sx->sx_recurse);
	}

	db_printf(" waiters: ");
	switch(sx->sx_lock &
	    (SX_LOCK_SHARED_WAITERS | SX_LOCK_EXCLUSIVE_WAITERS)) {
	case SX_LOCK_SHARED_WAITERS:
		db_printf("shared\n");
		break;
	case SX_LOCK_EXCLUSIVE_WAITERS:
		db_printf("exclusive\n");
		break;
	case SX_LOCK_SHARED_WAITERS | SX_LOCK_EXCLUSIVE_WAITERS:
		db_printf("exclusive and shared\n");
		break;
	default:
		db_printf("none\n");
	}
}

/*
 * Check to see if a thread that is blocked on a sleep queue is actually
 * blocked on an sx lock.  If so, output some details and return true.
 * If the lock has an exclusive owner, return that in *ownerp.
 */
int
sx_chain(struct thread *td, struct thread **ownerp)
{
	struct sx *sx;

	/*
	 * Check to see if this thread is blocked on an sx lock.
	 * First, we check the lock class.  If that is ok, then we
	 * compare the lock name against the wait message.
	 */
	sx = td->td_wchan;
	if (LOCK_CLASS(&sx->lock_object) != &lock_class_sx ||
	    sx->lock_object.lo_name != td->td_wmesg)
		return (0);

	/* We think we have an sx lock, so output some details. */
	db_printf("blocked on sx \"%s\" ", td->td_wmesg);
	*ownerp = sx_xholder(sx);
	if (sx->sx_lock & SX_LOCK_SHARED)
		db_printf("SLOCK (count %ju)\n",
		    (uintmax_t)SX_SHARERS(sx->sx_lock));
	else
		db_printf("XLOCK\n");
	return (1);
}
#endif
