/* 
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
 * $Id: kern_lock.c,v 1.13 1997/10/28 15:58:19 bde Exp $
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/systm.h>

/*
 * Locking primitives implementation.
 * Locks provide shared/exclusive sychronization.
 */

#ifdef SIMPLELOCK_DEBUG
#define COUNT(p, x) if (p) (p)->p_locks += (x)
#else
#define COUNT(p, x)
#endif

#define LOCK_WAIT_TIME 100
#define LOCK_SAMPLE_WAIT 7

#if defined(DIAGNOSTIC)
#define LOCK_INLINE
#else
#define LOCK_INLINE inline
#endif

#define LK_ALL (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE | \
	LK_SHARE_NONZERO | LK_WAIT_NONZERO)

static int acquire(struct lock *lkp, int extflags, int wanted);
static int apause(struct lock *lkp, int flags);
static int acquiredrain(struct lock *lkp, int extflags) ;

static LOCK_INLINE void
sharelock(struct lock *lkp, int incr) {
	lkp->lk_flags |= LK_SHARE_NONZERO;
	lkp->lk_sharecount += incr;
}

static LOCK_INLINE void
shareunlock(struct lock *lkp, int decr) {
#if defined(DIAGNOSTIC)
	if (lkp->lk_sharecount < decr)
#if defined(DDB)
		Debugger("shareunlock: count < decr");
#else
		panic("shareunlock: count < decr");
#endif
#endif

	lkp->lk_sharecount -= decr;
	if (lkp->lk_sharecount == 0)
		lkp->lk_flags &= ~LK_SHARE_NONZERO;
}

/*
 * This is the waitloop optimization, and note for this to work
 * simple_lock and simple_unlock should be subroutines to avoid
 * optimization troubles.
 */
static int
apause(struct lock *lkp, int flags) {
	int lock_wait;
	lock_wait = LOCK_WAIT_TIME;
	for (; lock_wait > 0; lock_wait--) {
		int i;
		if ((lkp->lk_flags & flags) == 0)
			return 0;
		simple_unlock(&lkp->lk_interlock);
		for (i = LOCK_SAMPLE_WAIT; i > 0; i--) {
			if ((lkp->lk_flags & flags) == 0) {
				simple_lock(&lkp->lk_interlock);
				if ((lkp->lk_flags & flags) == 0)
					return 0;
				break;
			}
		}
	}
	return 1;
}

static int
acquire(struct lock *lkp, int extflags, int wanted) {
	int error;

	if ((extflags & LK_NOWAIT) && (lkp->lk_flags & wanted)) {
		return EBUSY;
	}

	if (((lkp->lk_flags | extflags) & LK_NOPAUSE) == 0) {
		error = apause(lkp, wanted);
		if (error == 0)
			return 0;
	}

	while ((lkp->lk_flags & wanted) != 0) {
		lkp->lk_flags |= LK_WAIT_NONZERO;
		lkp->lk_waitcount++;
		simple_unlock(&lkp->lk_interlock);
		error = tsleep(lkp, lkp->lk_prio, lkp->lk_wmesg, lkp->lk_timo);
		simple_lock(&lkp->lk_interlock);
		lkp->lk_waitcount--;
		if (lkp->lk_waitcount == 0)
			lkp->lk_flags &= ~LK_WAIT_NONZERO;
		if (error)
			return error;
		if (extflags & LK_SLEEPFAIL) {
			return ENOLCK;
		}
	}
	return 0;
}

/*
 * Set, change, or release a lock.
 *
 * Shared requests increment the shared count. Exclusive requests set the
 * LK_WANT_EXCL flag (preventing further shared locks), and wait for already
 * accepted shared locks and shared-to-exclusive upgrades to go away.
 */
int
lockmgr(lkp, flags, interlkp, p)
	struct lock *lkp;
	u_int flags;
	struct simplelock *interlkp;
	struct proc *p;
{
	int error;
	pid_t pid;
	int extflags;

	error = 0;
	if (p == NULL)
		pid = LK_KERNPROC;
	else
		pid = p->p_pid;

	simple_lock(&lkp->lk_interlock);
	if (flags & LK_INTERLOCK)
		simple_unlock(interlkp);

	extflags = (flags | lkp->lk_flags) & LK_EXTFLG_MASK;

	switch (flags & LK_TYPE_MASK) {

	case LK_SHARED:
		if (lkp->lk_lockholder != pid) {
			error = acquire(lkp, extflags,
				LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE);
			if (error)
				break;
			sharelock(lkp, 1);
			COUNT(p, 1);
			break;
		}
		/*
		 * We hold an exclusive lock, so downgrade it to shared.
		 * An alternative would be to fail with EDEADLK.
		 */
		sharelock(lkp, 1);
		COUNT(p, 1);
		/* fall into downgrade */

	case LK_DOWNGRADE:
		if (lkp->lk_lockholder != pid || lkp->lk_exclusivecount == 0)
			panic("lockmgr: not holding exclusive lock");
		sharelock(lkp, lkp->lk_exclusivecount);
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
			shareunlock(lkp, 1);
			COUNT(p, -1);
			error = EBUSY;
			break;
		}
		/* fall into normal upgrade */

	case LK_UPGRADE:
		/*
		 * Upgrade a shared lock to an exclusive one. If another
		 * shared lock has already requested an upgrade to an
		 * exclusive lock, our shared lock is released and an
		 * exclusive lock is requested (which will be granted
		 * after the upgrade). If we return an error, the file
		 * will always be unlocked.
		 */
		if ((lkp->lk_lockholder == pid) || (lkp->lk_sharecount <= 0))
			panic("lockmgr: upgrade exclusive lock");
		shareunlock(lkp, 1);
		COUNT(p, -1);
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
			error = acquire(lkp, extflags , LK_SHARE_NONZERO);
			lkp->lk_flags &= ~LK_WANT_UPGRADE;
			if (error)
				break;
			lkp->lk_flags |= LK_HAVE_EXCL;
			lkp->lk_lockholder = pid;
			if (lkp->lk_exclusivecount != 0)
				panic("lockmgr: non-zero exclusive count");
			lkp->lk_exclusivecount = 1;
			COUNT(p, 1);
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
		/* fall into exclusive request */

	case LK_EXCLUSIVE:
		if (lkp->lk_lockholder == pid && pid != LK_KERNPROC) {
			/*
			 *	Recursive lock.
			 */
			if ((extflags & LK_CANRECURSE) == 0)
				panic("lockmgr: locking against myself");
			lkp->lk_exclusivecount++;
			COUNT(p, 1);
			break;
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
		error = acquire(lkp, extflags, (LK_HAVE_EXCL | LK_WANT_EXCL));
		if (error)
			break;
		lkp->lk_flags |= LK_WANT_EXCL;
		/*
		 * Wait for shared locks and upgrades to finish.
		 */
		error = acquire(lkp, extflags, LK_WANT_UPGRADE | LK_SHARE_NONZERO);
		lkp->lk_flags &= ~LK_WANT_EXCL;
		if (error)
			break;
		lkp->lk_flags |= LK_HAVE_EXCL;
		lkp->lk_lockholder = pid;
		if (lkp->lk_exclusivecount != 0)
			panic("lockmgr: non-zero exclusive count");
		lkp->lk_exclusivecount = 1;
		COUNT(p, 1);
		break;

	case LK_RELEASE:
		if (lkp->lk_exclusivecount != 0) {
			if (pid != lkp->lk_lockholder)
				panic("lockmgr: pid %d, not %s %d unlocking",
				    pid, "exclusive lock holder",
				    lkp->lk_lockholder);
			lkp->lk_exclusivecount--;
			COUNT(p, -1);
			if (lkp->lk_exclusivecount == 0) {
				lkp->lk_flags &= ~LK_HAVE_EXCL;
				lkp->lk_lockholder = LK_NOPROC;
			}
		} else if (lkp->lk_flags & LK_SHARE_NONZERO) {
			shareunlock(lkp, 1);
			COUNT(p, -1);
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
		if (lkp->lk_lockholder == pid)
			panic("lockmgr: draining against myself");

		error = acquiredrain(lkp, extflags);
		if (error)
			break;
		lkp->lk_flags |= LK_DRAINING | LK_HAVE_EXCL;
		lkp->lk_lockholder = pid;
		lkp->lk_exclusivecount = 1;
		COUNT(p, 1);
		break;

	default:
		simple_unlock(&lkp->lk_interlock);
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
	simple_unlock(&lkp->lk_interlock);
	return (error);
}

static int
acquiredrain(struct lock *lkp, int extflags) {
	int error;

	if ((extflags & LK_NOWAIT) && (lkp->lk_flags & LK_ALL)) {
		return EBUSY;
	}

	error = apause(lkp, LK_ALL);
	if (error == 0)
		return 0;

	while (lkp->lk_flags & LK_ALL) {
		lkp->lk_flags |= LK_WAITDRAIN;
		simple_unlock(&lkp->lk_interlock);
		error = tsleep(&lkp->lk_flags, lkp->lk_prio,
			lkp->lk_wmesg, lkp->lk_timo);
		simple_lock(&lkp->lk_interlock);
		if (error)
			return error;
		if (extflags & LK_SLEEPFAIL) {
			return ENOLCK;
		}
	}
	return 0;
}

/*
 * Initialize a lock; required before use.
 */
void
lockinit(lkp, prio, wmesg, timo, flags)
	struct lock *lkp;
	int prio;
	char *wmesg;
	int timo;
	int flags;
{

	simple_lock_init(&lkp->lk_interlock);
	lkp->lk_flags = (flags & LK_EXTFLG_MASK);
	lkp->lk_sharecount = 0;
	lkp->lk_waitcount = 0;
	lkp->lk_exclusivecount = 0;
	lkp->lk_prio = prio;
	lkp->lk_wmesg = wmesg;
	lkp->lk_timo = timo;
	lkp->lk_lockholder = LK_NOPROC;
}

/*
 * Determine the status of a lock.
 */
int
lockstatus(lkp)
	struct lock *lkp;
{
	int lock_type = 0;

	simple_lock(&lkp->lk_interlock);
	if (lkp->lk_exclusivecount != 0)
		lock_type = LK_EXCLUSIVE;
	else if (lkp->lk_sharecount != 0)
		lock_type = LK_SHARED;
	simple_unlock(&lkp->lk_interlock);
	return (lock_type);
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
		printf(" lock type %s: EXCL (count %d) by pid %d",
		    lkp->lk_wmesg, lkp->lk_exclusivecount, lkp->lk_lockholder);
	if (lkp->lk_waitcount > 0)
		printf(" with %d pending", lkp->lk_waitcount);
}

#if defined(SIMPLELOCK_DEBUG) && NCPUS == 1
#include <sys/kernel.h>
#include <sys/sysctl.h>

static int lockpausetime = 0;
SYSCTL_INT(_debug, OID_AUTO, lockpausetime, CTLFLAG_RW, &lockpausetime, 0, "");

int simplelockrecurse;

/*
 * Simple lock functions so that the debugger can see from whence
 * they are being called.
 */
void
simple_lock_init(alp)
	struct simplelock *alp;
{

	alp->lock_data = 0;
}

void
_simple_lock(alp, id, l)
	struct simplelock *alp;
	const char *id;
	int l;
{

	if (simplelockrecurse)
		return;
	if (alp->lock_data == 1) {
		if (lockpausetime == -1)
			panic("%s:%d: simple_lock: lock held", id, l);
		printf("%s:%d: simple_lock: lock held\n", id, l);
		if (lockpausetime == 1) {
			Debugger("simple_lock");
			/*BACKTRACE(curproc); */
		} else if (lockpausetime > 1) {
			printf("%s:%d: simple_lock: lock held...", id, l);
			tsleep(&lockpausetime, PCATCH | PPAUSE, "slock",
			    lockpausetime * hz);
			printf(" continuing\n");
		}
	}
	alp->lock_data = 1;
	if (curproc)
		curproc->p_simple_locks++;
}

int
_simple_lock_try(alp, id, l)
	struct simplelock *alp;
	const char *id;
	int l;
{

	if (alp->lock_data)
		return (0);
	if (simplelockrecurse)
		return (1);
	alp->lock_data = 1;
	if (curproc)
		curproc->p_simple_locks++;
	return (1);
}

void
_simple_unlock(alp, id, l)
	struct simplelock *alp;
	const char *id;
	int l;
{

	if (simplelockrecurse)
		return;
	if (alp->lock_data == 0) {
		if (lockpausetime == -1)
			panic("%s:%d: simple_unlock: lock not held", id, l);
		printf("%s:%d: simple_unlock: lock not held\n", id, l);
		if (lockpausetime == 1) {
			Debugger("simple_unlock");
			/* BACKTRACE(curproc); */
		} else if (lockpausetime > 1) {
			printf("%s:%d: simple_unlock: lock not held...", id, l);
			tsleep(&lockpausetime, PCATCH | PPAUSE, "sunlock",
			    lockpausetime * hz);
			printf(" continuing\n");
		}
	}
	alp->lock_data = 0;
	if (curproc)
		curproc->p_simple_locks--;
}
#endif /* SIMPLELOCK_DEBUG && NCPUS == 1 */
