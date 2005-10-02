/*-
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (C) 1997
 *	John S. Dyson.  All rights reserved.
 *
 * This code contains ideas from software contributed to Berkeley by
 * Avadis Tevanian, Jr., Michael Wayne Young, and the Mach Operating
 * System project at Carnegie-Mellon University.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_lock.c	8.18 (Berkeley) 5/21/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/systm.h>
#ifdef DEBUG_LOCKS
#include <sys/stack.h>
#include <sys/sysctl.h>
#endif
#include <sys/kdb.h>

/*
 * Locking primitives implementation.
 * Locks provide shared/exclusive sychronization.
 */

#define	COUNT(td, x)	if ((td)) (td)->td_locks += (x)
#define LK_ALL (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE | \
	LK_SHARE_NONZERO | LK_WAIT_NONZERO)

static int acquire(struct lock **lkpp, int extflags, int wanted);
static int acquiredrain(struct lock *lkp, int extflags) ;

static __inline void
sharelock(struct thread *td, struct lock *lkp, int incr) {
	lkp->lk_flags |= LK_SHARE_NONZERO;
	lkp->lk_sharecount += incr;
	COUNT(td, incr);
}

static __inline void
shareunlock(struct thread *td, struct lock *lkp, int decr) {

	KASSERT(lkp->lk_sharecount >= decr, ("shareunlock: count < decr"));

	COUNT(td, -decr);
	if (lkp->lk_sharecount == decr) {
		lkp->lk_flags &= ~LK_SHARE_NONZERO;
		if (lkp->lk_flags & (LK_WANT_UPGRADE | LK_WANT_EXCL)) {
			wakeup(lkp);
		}
		lkp->lk_sharecount = 0;
	} else {
		lkp->lk_sharecount -= decr;
	}
}

static int
acquire(struct lock **lkpp, int extflags, int wanted)
{
	struct lock *lkp = *lkpp;
	int error;
	CTR3(KTR_LOCK,
	    "acquire(): lkp == %p, extflags == 0x%x, wanted == 0x%x",
	    lkp, extflags, wanted);

	if ((extflags & LK_NOWAIT) && (lkp->lk_flags & wanted))
		return EBUSY;
	error = 0;
	while ((lkp->lk_flags & wanted) != 0) {
		CTR2(KTR_LOCK,
		    "acquire(): lkp == %p, lk_flags == 0x%x sleeping",
		    lkp, lkp->lk_flags);
		lkp->lk_flags |= LK_WAIT_NONZERO;
		lkp->lk_waitcount++;
		error = msleep(lkp, lkp->lk_interlock, lkp->lk_prio,
		    lkp->lk_wmesg, 
		    ((extflags & LK_TIMELOCK) ? lkp->lk_timo : 0));
		lkp->lk_waitcount--;
		if (lkp->lk_waitcount == 0)
			lkp->lk_flags &= ~LK_WAIT_NONZERO;
		if (error)
			break;
		if (extflags & LK_SLEEPFAIL) {
			error = ENOLCK;
			break;
		}
		if (lkp->lk_newlock != NULL) {
			mtx_lock(lkp->lk_newlock->lk_interlock);
			mtx_unlock(lkp->lk_interlock);
			if (lkp->lk_waitcount == 0)
				wakeup((void *)(&lkp->lk_newlock));
			*lkpp = lkp = lkp->lk_newlock;
		}
	}
	mtx_assert(lkp->lk_interlock, MA_OWNED);
	return (error);
}

/*
 * Set, change, or release a lock.
 *
 * Shared requests increment the shared count. Exclusive requests set the
 * LK_WANT_EXCL flag (preventing further shared locks), and wait for already
 * accepted shared locks and shared-to-exclusive upgrades to go away.
 */
int
lockmgr(lkp, flags, interlkp, td)
	struct lock *lkp;
	u_int flags;
	struct mtx *interlkp;
	struct thread *td;
{
	int error;
	struct thread *thr;
	int extflags, lockflags;

	error = 0;
	if (td == NULL)
		thr = LK_KERNPROC;
	else
		thr = td;

	if ((flags & LK_INTERNAL) == 0)
		mtx_lock(lkp->lk_interlock);
	CTR6(KTR_LOCK,
	    "lockmgr(): lkp == %p (lk_wmesg == \"%s\"), owner == %p, exclusivecount == %d, flags == 0x%x, "
	    "td == %p", lkp, lkp->lk_wmesg, lkp->lk_lockholder,
	    lkp->lk_exclusivecount, flags, td);
#ifdef DEBUG_LOCKS
	{
		struct stack stack; /* XXX */
		stack_save(&stack);
		CTRSTACK(KTR_LOCK, &stack, 0, 1);
	}
#endif

	if (flags & LK_INTERLOCK) {
		mtx_assert(interlkp, MA_OWNED | MA_NOTRECURSED);
		mtx_unlock(interlkp);
	}

	if ((flags & (LK_NOWAIT|LK_RELEASE)) == 0)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK,
		    &lkp->lk_interlock->mtx_object,
		    "Acquiring lockmgr lock \"%s\"", lkp->lk_wmesg);

	if (panicstr != NULL) {
		mtx_unlock(lkp->lk_interlock);
		return (0);
	}
	if ((lkp->lk_flags & LK_NOSHARE) &&
	    (flags & LK_TYPE_MASK) == LK_SHARED) {
		flags &= ~LK_TYPE_MASK;
		flags |= LK_EXCLUSIVE;
	}
	extflags = (flags | lkp->lk_flags) & LK_EXTFLG_MASK;

	switch (flags & LK_TYPE_MASK) {

	case LK_SHARED:
		/*
		 * If we are not the exclusive lock holder, we have to block
		 * while there is an exclusive lock holder or while an
		 * exclusive lock request or upgrade request is in progress.
		 *
		 * However, if TDP_DEADLKTREAT is set, we override exclusive
		 * lock requests or upgrade requests ( but not the exclusive
		 * lock itself ).
		 */
		if (lkp->lk_lockholder != thr) {
			lockflags = LK_HAVE_EXCL;
			if (td != NULL && !(td->td_pflags & TDP_DEADLKTREAT))
				lockflags |= LK_WANT_EXCL | LK_WANT_UPGRADE;
			error = acquire(&lkp, extflags, lockflags);
			if (error)
				break;
			sharelock(td, lkp, 1);
#if defined(DEBUG_LOCKS)
			stack_save(&lkp->lk_stack);
#endif
			break;
		}
		/*
		 * We hold an exclusive lock, so downgrade it to shared.
		 * An alternative would be to fail with EDEADLK.
		 */
		sharelock(td, lkp, 1);
		/* FALLTHROUGH downgrade */

	case LK_DOWNGRADE:
		KASSERT(lkp->lk_lockholder == thr && lkp->lk_exclusivecount != 0,
			("lockmgr: not holding exclusive lock "
			"(owner thread (%p) != thread (%p), exlcnt (%d) != 0",
			lkp->lk_lockholder, thr, lkp->lk_exclusivecount));
		sharelock(td, lkp, lkp->lk_exclusivecount);
		COUNT(td, -lkp->lk_exclusivecount);
		lkp->lk_exclusivecount = 0;
		lkp->lk_flags &= ~LK_HAVE_EXCL;
		lkp->lk_lockholder = LK_NOPROC;
		if (lkp->lk_waitcount)
			wakeup((void *)lkp);
		break;

	case LK_EXCLUPGRADE:
		/*
		 * If another process is ahead of us to get an upgrade,
		 * then we want to fail rather than have an intervening
		 * exclusive access.
		 */
		if (lkp->lk_flags & LK_WANT_UPGRADE) {
			shareunlock(td, lkp, 1);
			error = EBUSY;
			break;
		}
		/* FALLTHROUGH normal upgrade */

	case LK_UPGRADE:
		/*
		 * Upgrade a shared lock to an exclusive one. If another
		 * shared lock has already requested an upgrade to an
		 * exclusive lock, our shared lock is released and an
		 * exclusive lock is requested (which will be granted
		 * after the upgrade). If we return an error, the file
		 * will always be unlocked.
		 */
		if (lkp->lk_lockholder == thr)
			panic("lockmgr: upgrade exclusive lock");
		if (lkp->lk_sharecount <= 0)
			panic("lockmgr: upgrade without shared");
		shareunlock(td, lkp, 1);
		/*
		 * If we are just polling, check to see if we will block.
		 */
		if ((extflags & LK_NOWAIT) &&
		    ((lkp->lk_flags & LK_WANT_UPGRADE) ||
		     lkp->lk_sharecount > 1)) {
			error = EBUSY;
			break;
		}
		if ((lkp->lk_flags & LK_WANT_UPGRADE) == 0) {
			/*
			 * We are first shared lock to request an upgrade, so
			 * request upgrade and wait for the shared count to
			 * drop to zero, then take exclusive lock.
			 */
			lkp->lk_flags |= LK_WANT_UPGRADE;
			error = acquire(&lkp, extflags, LK_SHARE_NONZERO);
			lkp->lk_flags &= ~LK_WANT_UPGRADE;

			if (error) {
			         if ((lkp->lk_flags & ( LK_WANT_EXCL | LK_WAIT_NONZERO)) == (LK_WANT_EXCL | LK_WAIT_NONZERO))
			                   wakeup((void *)lkp);
			         break;
			}
			if (lkp->lk_exclusivecount != 0)
				panic("lockmgr: non-zero exclusive count");
			lkp->lk_flags |= LK_HAVE_EXCL;
			lkp->lk_lockholder = thr;
			lkp->lk_exclusivecount = 1;
			COUNT(td, 1);
#if defined(DEBUG_LOCKS)
			stack_save(&lkp->lk_stack);
#endif
			break;
		}
		/*
		 * Someone else has requested upgrade. Release our shared
		 * lock, awaken upgrade requestor if we are the last shared
		 * lock, then request an exclusive lock.
		 */
		if ( (lkp->lk_flags & (LK_SHARE_NONZERO|LK_WAIT_NONZERO)) ==
			LK_WAIT_NONZERO)
			wakeup((void *)lkp);
		/* FALLTHROUGH exclusive request */

	case LK_EXCLUSIVE:
		if (lkp->lk_lockholder == thr && thr != LK_KERNPROC) {
			/*
			 *	Recursive lock.
			 */
			if ((extflags & (LK_NOWAIT | LK_CANRECURSE)) == 0)
				panic("lockmgr: locking against myself");
			if ((extflags & LK_CANRECURSE) != 0) {
				lkp->lk_exclusivecount++;
				COUNT(td, 1);
				break;
			}
		}
		/*
		 * If we are just polling, check to see if we will sleep.
		 */
		if ((extflags & LK_NOWAIT) &&
		    (lkp->lk_flags & (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE | LK_SHARE_NONZERO))) {
			error = EBUSY;
			break;
		}
		/*
		 * Try to acquire the want_exclusive flag.
		 */
		error = acquire(&lkp, extflags, (LK_HAVE_EXCL | LK_WANT_EXCL));
		if (error)
			break;
		lkp->lk_flags |= LK_WANT_EXCL;
		/*
		 * Wait for shared locks and upgrades to finish.
		 */
		error = acquire(&lkp, extflags, LK_HAVE_EXCL | LK_WANT_UPGRADE | LK_SHARE_NONZERO);
		lkp->lk_flags &= ~LK_WANT_EXCL;
		if (error) {
			if (lkp->lk_flags & LK_WAIT_NONZERO)		
			         wakeup((void *)lkp);
			break;
		}	
		lkp->lk_flags |= LK_HAVE_EXCL;
		lkp->lk_lockholder = thr;
		if (lkp->lk_exclusivecount != 0)
			panic("lockmgr: non-zero exclusive count");
		lkp->lk_exclusivecount = 1;
		COUNT(td, 1);
#if defined(DEBUG_LOCKS)
		stack_save(&lkp->lk_stack);
#endif
		break;

	case LK_RELEASE:
		if (lkp->lk_exclusivecount != 0) {
			if (lkp->lk_lockholder != thr &&
			    lkp->lk_lockholder != LK_KERNPROC) {
				panic("lockmgr: thread %p, not %s %p unlocking",
				    thr, "exclusive lock holder",
				    lkp->lk_lockholder);
			}
			if (lkp->lk_lockholder != LK_KERNPROC)
				COUNT(td, -1);
			if (lkp->lk_exclusivecount == 1) {
				lkp->lk_flags &= ~LK_HAVE_EXCL;
				lkp->lk_lockholder = LK_NOPROC;
				lkp->lk_exclusivecount = 0;
			} else {
				lkp->lk_exclusivecount--;
			}
		} else if (lkp->lk_flags & LK_SHARE_NONZERO)
			shareunlock(td, lkp, 1);
		else  {
			printf("lockmgr: thread %p unlocking unheld lock\n",
			    thr);
			kdb_backtrace();
		}

		if (lkp->lk_flags & LK_WAIT_NONZERO)
			wakeup((void *)lkp);
		break;

	case LK_DRAIN:
		/*
		 * Check that we do not already hold the lock, as it can 
		 * never drain if we do. Unfortunately, we have no way to
		 * check for holding a shared lock, but at least we can
		 * check for an exclusive one.
		 */
		if (lkp->lk_lockholder == thr)
			panic("lockmgr: draining against myself");

		error = acquiredrain(lkp, extflags);
		if (error)
			break;
		lkp->lk_flags |= LK_DRAINING | LK_HAVE_EXCL;
		lkp->lk_lockholder = thr;
		lkp->lk_exclusivecount = 1;
		COUNT(td, 1);
#if defined(DEBUG_LOCKS)
		stack_save(&lkp->lk_stack);
#endif
		break;

	default:
		mtx_unlock(lkp->lk_interlock);
		panic("lockmgr: unknown locktype request %d",
		    flags & LK_TYPE_MASK);
		/* NOTREACHED */
	}
	if ((lkp->lk_flags & LK_WAITDRAIN) &&
	    (lkp->lk_flags & (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE |
		LK_SHARE_NONZERO | LK_WAIT_NONZERO)) == 0) {
		lkp->lk_flags &= ~LK_WAITDRAIN;
		wakeup((void *)&lkp->lk_flags);
	}
	mtx_unlock(lkp->lk_interlock);
	return (error);
}

static int
acquiredrain(struct lock *lkp, int extflags) {
	int error;

	if ((extflags & LK_NOWAIT) && (lkp->lk_flags & LK_ALL)) {
		return EBUSY;
	}
	while (lkp->lk_flags & LK_ALL) {
		lkp->lk_flags |= LK_WAITDRAIN;
		error = msleep(&lkp->lk_flags, lkp->lk_interlock, lkp->lk_prio,
			lkp->lk_wmesg, 
			((extflags & LK_TIMELOCK) ? lkp->lk_timo : 0));
		if (error)
			return error;
		if (extflags & LK_SLEEPFAIL) {
			return ENOLCK;
		}
	}
	return 0;
}

/*
 * Transfer any waiting processes from one lock to another.
 */
void
transferlockers(from, to)
	struct lock *from;
	struct lock *to;
{

	KASSERT(from != to, ("lock transfer to self"));
	KASSERT((from->lk_flags&LK_WAITDRAIN) == 0, ("transfer draining lock"));

	mtx_lock(from->lk_interlock);
	if (from->lk_waitcount == 0) {
		mtx_unlock(from->lk_interlock);
		return;
	}
	from->lk_newlock = to;
	wakeup((void *)from);
	msleep(&from->lk_newlock, from->lk_interlock, from->lk_prio,
	    "lkxfer", 0);
	from->lk_newlock = NULL;
	from->lk_flags &= ~(LK_WANT_EXCL | LK_WANT_UPGRADE);
	KASSERT(from->lk_waitcount == 0, ("active lock"));
	mtx_unlock(from->lk_interlock);
}


/*
 * Initialize a lock; required before use.
 */
void
lockinit(lkp, prio, wmesg, timo, flags)
	struct lock *lkp;
	int prio;
	const char *wmesg;
	int timo;
	int flags;
{
	CTR5(KTR_LOCK, "lockinit(): lkp == %p, prio == %d, wmesg == \"%s\", "
	    "timo == %d, flags = 0x%x\n", lkp, prio, wmesg, timo, flags);

	lkp->lk_interlock = mtx_pool_alloc(mtxpool_lockbuilder);
	lkp->lk_flags = (flags & LK_EXTFLG_MASK);
	lkp->lk_sharecount = 0;
	lkp->lk_waitcount = 0;
	lkp->lk_exclusivecount = 0;
	lkp->lk_prio = prio;
	lkp->lk_wmesg = wmesg;
	lkp->lk_timo = timo;
	lkp->lk_lockholder = LK_NOPROC;
	lkp->lk_newlock = NULL;
#ifdef DEBUG_LOCKS
	stack_zero(&lkp->lk_stack);
#endif
}

/*
 * Destroy a lock.
 */
void
lockdestroy(lkp)
	struct lock *lkp;
{
	CTR2(KTR_LOCK, "lockdestroy(): lkp == %p (lk_wmesg == \"%s\")",
	    lkp, lkp->lk_wmesg);
}

/*
 * Determine the status of a lock.
 */
int
lockstatus(lkp, td)
	struct lock *lkp;
	struct thread *td;
{
	int lock_type = 0;
	int interlocked;

	if (!kdb_active) {
		interlocked = 1;
		mtx_lock(lkp->lk_interlock);
	} else
		interlocked = 0;
	if (lkp->lk_exclusivecount != 0) {
		if (td == NULL || lkp->lk_lockholder == td)
			lock_type = LK_EXCLUSIVE;
		else
			lock_type = LK_EXCLOTHER;
	} else if (lkp->lk_sharecount != 0)
		lock_type = LK_SHARED;
	if (interlocked)
		mtx_unlock(lkp->lk_interlock);
	return (lock_type);
}

/*
 * Determine the number of holders of a lock.
 */
int
lockcount(lkp)
	struct lock *lkp;
{
	int count;

	mtx_lock(lkp->lk_interlock);
	count = lkp->lk_exclusivecount + lkp->lk_sharecount;
	mtx_unlock(lkp->lk_interlock);
	return (count);
}

/*
 * Print out information about state of a lock. Used by VOP_PRINT
 * routines to display status about contained locks.
 */
void
lockmgr_printinfo(lkp)
	struct lock *lkp;
{

	if (lkp->lk_sharecount)
		printf(" lock type %s: SHARED (count %d)", lkp->lk_wmesg,
		    lkp->lk_sharecount);
	else if (lkp->lk_flags & LK_HAVE_EXCL)
		printf(" lock type %s: EXCL (count %d) by thread %p (pid %d)",
		    lkp->lk_wmesg, lkp->lk_exclusivecount,
		    lkp->lk_lockholder, lkp->lk_lockholder->td_proc->p_pid);
	if (lkp->lk_waitcount > 0)
		printf(" with %d pending", lkp->lk_waitcount);
#ifdef DEBUG_LOCKS
	stack_print(&lkp->lk_stack);
#endif
}
