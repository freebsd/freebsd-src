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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
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
#define LOCK_INLINE __inline
#endif

#define LK_ALL (LK_HAVE_EXCL | LK_WANT_EXCL | LK_WANT_UPGRADE | \
	LK_SHARE_NONZERO | LK_WAIT_NONZERO)

/*
 * Mutex array variables.  Rather than each lockmgr lock having its own mutex,
 * share a fixed (at boot time) number of mutexes across all lockmgr locks in
 * order to keep sizeof(struct lock) down.
 */
extern int lock_nmtx;
int lock_mtx_selector;
struct mtx *lock_mtx_array;
MUTEX_DECLARE(static, lock_mtx);

static int acquire(struct lock *lkp, int extflags, int wanted);
static int apause(struct lock *lkp, int flags);
static int acquiredrain(struct lock *lkp, int extflags) ;

static void
lockmgr_init(void *dummy __unused)
{
	int	i;

	/*
	 * Initialize the lockmgr protection mutex if it hasn't already been
	 * done.  Unless something changes about kernel startup order, VM
	 * initialization will always cause this mutex to already be
	 * initialized in a call to lockinit().
	 */
	if (lock_mtx_selector == 0)
		mtx_init(&lock_mtx, "lockmgr", MTX_DEF | MTX_COLD);
	else {
		/*
		 * This is necessary if (lock_nmtx == 1) and doesn't hurt
		 * otherwise.
		 */
		lock_mtx_selector = 0;
	}

	lock_mtx_array = (struct mtx *)malloc(sizeof(struct mtx) * lock_nmtx,
	    M_CACHE, M_WAITOK);
	for (i = 0; i < lock_nmtx; i++)
		mtx_init(&lock_mtx_array[i], "lockmgr interlock", MTX_DEF);
}
SYSINIT(lmgrinit, SI_SUB_LOCK, SI_ORDER_FIRST, lockmgr_init, NULL)

static LOCK_INLINE void
sharelock(struct lock *lkp, int incr) {
	lkp->lk_flags |= LK_SHARE_NONZERO;
	lkp->lk_sharecount += incr;
}

static LOCK_INLINE void
shareunlock(struct lock *lkp, int decr) {

	KASSERT(lkp->lk_sharecount >= decr, ("shareunlock: count < decr"));

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

/*
 * This is the waitloop optimization, and note for this to work
 * simple_lock and simple_unlock should be subroutines to avoid
 * optimization troubles.
 */
static int
apause(struct lock *lkp, int flags)
{
#ifdef SMP
	int i, lock_wait;
#endif

	if ((lkp->lk_flags & flags) == 0)
		return 0;
#ifdef SMP
	for (lock_wait = LOCK_WAIT_TIME; lock_wait > 0; lock_wait--) {
		mtx_exit(lkp->lk_interlock, MTX_DEF);
		for (i = LOCK_SAMPLE_WAIT; i > 0; i--)
			if ((lkp->lk_flags & flags) == 0)
				break;
		mtx_enter(lkp->lk_interlock, MTX_DEF);
		if ((lkp->lk_flags & flags) == 0)
			return 0;
	}
#endif
	return 1;
}

static int
acquire(struct lock *lkp, int extflags, int wanted) {
	int s, error;

	CTR3(KTR_LOCKMGR,
	    "acquire(): lkp == %p, extflags == 0x%x, wanted == 0x%x\n",
	    lkp, extflags, wanted);

	if ((extflags & LK_NOWAIT) && (lkp->lk_flags & wanted)) {
		return EBUSY;
	}

	if (((lkp->lk_flags | extflags) & LK_NOPAUSE) == 0) {
		error = apause(lkp, wanted);
		if (error == 0)
			return 0;
	}

	s = splhigh();
	while ((lkp->lk_flags & wanted) != 0) {
		lkp->lk_flags |= LK_WAIT_NONZERO;
		lkp->lk_waitcount++;
		error = msleep(lkp, lkp->lk_interlock, lkp->lk_prio,
		    lkp->lk_wmesg, lkp->lk_timo);
		if (lkp->lk_waitcount == 1) {
			lkp->lk_flags &= ~LK_WAIT_NONZERO;
			lkp->lk_waitcount = 0;
		} else {
			lkp->lk_waitcount--;
		}
		if (error) {
			splx(s);
			return error;
		}
		if (extflags & LK_SLEEPFAIL) {
			splx(s);
			return ENOLCK;
		}
	}
	splx(s);
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
#ifndef	DEBUG_LOCKS
lockmgr(lkp, flags, interlkp, p)
#else
debuglockmgr(lkp, flags, interlkp, p, name, file, line)
#endif
	struct lock *lkp;
	u_int flags;
	struct mtx *interlkp;
	struct proc *p;
#ifdef	DEBUG_LOCKS
	const char *name;	/* Name of lock function */
	const char *file;	/* Name of file call is from */
	int line;		/* Line number in file */
#endif
{
	int error;
	pid_t pid;
	int extflags;

	CTR5(KTR_LOCKMGR,
	    "lockmgr(): lkp == %p (lk_wmesg == \"%s\"), flags == 0x%x, "
	    "interlkp == %p, p == %p", lkp, lkp->lk_wmesg, flags, interlkp, p);

	error = 0;
	if (p == NULL)
		pid = LK_KERNPROC;
	else
		pid = p->p_pid;

	mtx_enter(lkp->lk_interlock, MTX_DEF);
	if (flags & LK_INTERLOCK)
		mtx_exit(interlkp, MTX_DEF);

	extflags = (flags | lkp->lk_flags) & LK_EXTFLG_MASK;

	switch (flags & LK_TYPE_MASK) {

	case LK_SHARED:
		/*
		 * If we are not the exclusive lock holder, we have to block
		 * while there is an exclusive lock holder or while an
		 * exclusive lock request or upgrade request is in progress.
		 *
		 * However, if P_DEADLKTREAT is set, we override exclusive
		 * lock requests or upgrade requests ( but not the exclusive
		 * lock itself ).
		 */
		if (lkp->lk_lockholder != pid) {
			if (p && (p->p_flag & P_DEADLKTREAT)) {
				error = acquire(
					    lkp,
					    extflags,
					    LK_HAVE_EXCL
					);
			} else {
				error = acquire(
					    lkp, 
					    extflags,
					    LK_HAVE_EXCL | LK_WANT_EXCL | 
					     LK_WANT_UPGRADE
					);
			}
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
			error = acquire(lkp, extflags, LK_SHARE_NONZERO);
			lkp->lk_flags &= ~LK_WANT_UPGRADE;

			if (error)
				break;
			lkp->lk_flags |= LK_HAVE_EXCL;
			lkp->lk_lockholder = pid;
			if (lkp->lk_exclusivecount != 0)
				panic("lockmgr: non-zero exclusive count");
			lkp->lk_exclusivecount = 1;
#if defined(DEBUG_LOCKS)
			lkp->lk_filename = file;
			lkp->lk_lineno = line;
			lkp->lk_lockername = name;
#endif
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
			if ((extflags & (LK_NOWAIT | LK_CANRECURSE)) == 0)
				panic("lockmgr: locking against myself");
			if ((extflags & LK_CANRECURSE) != 0) {
				lkp->lk_exclusivecount++;
				COUNT(p, 1);
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
#if defined(DEBUG_LOCKS)
			lkp->lk_filename = file;
			lkp->lk_lineno = line;
			lkp->lk_lockername = name;
#endif
		COUNT(p, 1);
		break;

	case LK_RELEASE:
		if (lkp->lk_exclusivecount != 0) {
			if (lkp->lk_lockholder != pid &&
			    lkp->lk_lockholder != LK_KERNPROC) {
				panic("lockmgr: pid %d, not %s %d unlocking",
				    pid, "exclusive lock holder",
				    lkp->lk_lockholder);
			}
			if (lkp->lk_lockholder != LK_KERNPROC) {
				COUNT(p, -1);
			}
			if (lkp->lk_exclusivecount == 1) {
				lkp->lk_flags &= ~LK_HAVE_EXCL;
				lkp->lk_lockholder = LK_NOPROC;
				lkp->lk_exclusivecount = 0;
			} else {
				lkp->lk_exclusivecount--;
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
#if defined(DEBUG_LOCKS)
			lkp->lk_filename = file;
			lkp->lk_lineno = line;
			lkp->lk_lockername = name;
#endif
		COUNT(p, 1);
		break;

	default:
		mtx_exit(lkp->lk_interlock, MTX_DEF);
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
	mtx_exit(lkp->lk_interlock, MTX_DEF);
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
		error = msleep(&lkp->lk_flags, lkp->lk_interlock, lkp->lk_prio,
			lkp->lk_wmesg, lkp->lk_timo);
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
	CTR5(KTR_LOCKMGR, "lockinit(): lkp == %p, prio == %d, wmesg == \"%s\", "
	    "timo == %d, flags = 0x%x\n", lkp, prio, wmesg, timo, flags);

	if (lock_mtx_array != NULL) {
		mtx_enter(&lock_mtx, MTX_DEF);
		lkp->lk_interlock = &lock_mtx_array[lock_mtx_selector];
		lock_mtx_selector++;
		if (lock_mtx_selector == lock_nmtx)
			lock_mtx_selector = 0;
		mtx_exit(&lock_mtx, MTX_DEF);
	} else {
		/*
		 * Giving lockmgr locks that are initialized during boot a
		 * pointer to the internal lockmgr mutex is safe, since the
		 * lockmgr code itself doesn't call lockinit() (which could
		 * cause mutex recursion).
		 */
		if (lock_mtx_selector == 0) {
			/*
			 * This  case only happens during kernel bootstrapping,
			 * so there's no reason to protect modification of
			 * lock_mtx_selector or lock_mtx.
			 */
			mtx_init(&lock_mtx, "lockmgr", MTX_DEF | MTX_COLD);
			lock_mtx_selector = 1;
		}
		lkp->lk_interlock = &lock_mtx;
	}
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
 * Destroy a lock.
 */
void
lockdestroy(lkp)
	struct lock *lkp;
{
	CTR2(KTR_LOCKMGR, "lockdestroy(): lkp == %p (lk_wmesg == \"%s\")",
	    lkp, lkp->lk_wmesg);
}

/*
 * Determine the status of a lock.
 */
int
lockstatus(lkp, p)
	struct lock *lkp;
	struct proc *p;
{
	int lock_type = 0;

	mtx_enter(lkp->lk_interlock, MTX_DEF);
	if (lkp->lk_exclusivecount != 0) {
		if (p == NULL || lkp->lk_lockholder == p->p_pid)
			lock_type = LK_EXCLUSIVE;
		else
			lock_type = LK_EXCLOTHER;
	} else if (lkp->lk_sharecount != 0)
		lock_type = LK_SHARED;
	mtx_exit(lkp->lk_interlock, MTX_DEF);
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

	mtx_enter(lkp->lk_interlock, MTX_DEF);
	count = lkp->lk_exclusivecount + lkp->lk_sharecount;
	mtx_exit(lkp->lk_interlock, MTX_DEF);
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
		printf(" lock type %s: EXCL (count %d) by pid %d",
		    lkp->lk_wmesg, lkp->lk_exclusivecount, lkp->lk_lockholder);
	if (lkp->lk_waitcount > 0)
		printf(" with %d pending", lkp->lk_waitcount);
}

#if defined(SIMPLELOCK_DEBUG) && (MAXCPU == 1 || defined(COMPILING_LINT))
#include <sys/kernel.h>
#include <sys/sysctl.h>

static int lockpausetime = 0;
SYSCTL_INT(_debug, OID_AUTO, lockpausetime, CTLFLAG_RW, &lockpausetime, 0, "");

static int simplelockrecurse;

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
#elif defined(SIMPLELOCK_DEBUG)
#error "SIMPLELOCK_DEBUG is not compatible with SMP!"
#endif /* SIMPLELOCK_DEBUG && MAXCPU == 1 */
