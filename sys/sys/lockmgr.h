/*-
 * Copyright (c) 1995
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)lock.h	8.12 (Berkeley) 5/19/95
 * $FreeBSD$
 */

#ifndef	_SYS_LOCKMGR_H_
#define	_SYS_LOCKMGR_H_

#ifdef	DEBUG_LOCKS
#include <sys/stack.h> /* XXX */
#endif
#include <sys/queue.h>
#include <sys/_lock.h>

struct	mtx;

/*
 * The general lock structure.  Provides for multiple shared locks,
 * upgrading from shared to exclusive, and sleeping until the lock
 * can be gained.
 */
struct lock {
	struct lock_object lk_object;   /* common lock properties */
	struct	mtx *lk_interlock;	/* lock on remaining fields */
	u_int	lk_flags;		/* see below */
	int	lk_sharecount;		/* # of accepted shared locks */
	int	lk_waitcount;		/* # of processes sleeping for lock */
	short	lk_exclusivecount;	/* # of recursive exclusive locks */
	short	lk_prio;		/* priority at which to sleep */
	int	lk_timo;		/* maximum sleep time (for tsleep) */
	struct thread *lk_lockholder;	/* thread of exclusive lock holder */
	struct	lock *lk_newlock;	/* lock taking over this lock */

#ifdef	DEBUG_LOCKS
	struct stack lk_stack;
#endif
};

#define lk_wmesg lk_object.lo_name

#ifdef _KERNEL

/*
 * Lock request types:
 *   LK_SHARED - get one of many possible shared locks. If a process
 *	holding an exclusive lock requests a shared lock, the exclusive
 *	lock(s) will be downgraded to shared locks.
 *   LK_EXCLUSIVE - stop further shared locks, when they are cleared,
 *	grant a pending upgrade if it exists, then grant an exclusive
 *	lock. Only one exclusive lock may exist at a time, except that
 *	a process holding an exclusive lock may get additional exclusive
 *	locks if it explicitly sets the LK_CANRECURSE flag in the lock
 *	request, or if the LK_CANRECUSE flag was set when the lock was
 *	initialized.
 *   LK_UPGRADE - the process must hold a shared lock that it wants to
 *	have upgraded to an exclusive lock. Other processes may get
 *	exclusive access to the resource between the time that the upgrade
 *	is requested and the time that it is granted.
 *   LK_DOWNGRADE - the process must hold an exclusive lock that it wants
 *	to have downgraded to a shared lock. If the process holds multiple
 *	(recursive) exclusive locks, they will all be downgraded to shared
 *	locks.
 *   LK_RELEASE - release one instance of a lock.
 *   LK_DRAIN - wait for all activity on the lock to end, then mark it
 *	decommissioned. This feature is used before freeing a lock that
 *	is part of a piece of memory that is about to be freed.
 *   LK_EXCLOTHER - return for lockstatus().  Used when another process
 *	holds the lock exclusively.
 *
 * These are flags that are passed to the lockmgr routine.
 */
#define	LK_TYPE_MASK	0x0000000f	/* type of lock sought */
#define	LK_SHARED	0x00000001	/* shared lock */
#define	LK_EXCLUSIVE	0x00000002	/* exclusive lock */
#define	LK_UPGRADE	0x00000003	/* shared-to-exclusive upgrade */
#define	LK_DOWNGRADE	0x00000005	/* exclusive-to-shared downgrade */
#define	LK_RELEASE	0x00000006	/* release any type of lock */
#define	LK_DRAIN	0x00000007	/* wait for all lock activity to end */
#define	LK_EXCLOTHER	0x00000008	/* other process holds lock */
/*
 * External lock flags.
 *
 * These may be set in lock_init to set their mode permanently,
 * or passed in as arguments to the lock manager.
 */
#define	LK_EXTFLG_MASK	0x0000fff0	/* mask of external flags */
#define	LK_NOWAIT	0x00000010	/* do not sleep to await lock */
#define	LK_SLEEPFAIL	0x00000020	/* sleep, then return failure */
#define	LK_CANRECURSE	0x00000040	/* allow recursive exclusive lock */
#define	LK_NOSHARE	0x00000080	/* Only allow exclusive locks */
#define	LK_TIMELOCK	0x00000100	/* use lk_timo, else no timeout */
#define	LK_NOWITNESS	0x00000200	/* disable WITNESS */
#define	LK_NODUP	0x00000400	/* enable duplication logging */
#define	LK_NOPROFILE	0x00000800	/* disable lock profiling */
#define	LK_QUIET	0x00001000	/* disable lock operations tracking */
#define	LK_FUNC_MASK	(LK_NODUP | LK_NOPROFILE | LK_NOWITNESS | LK_QUIET)
/*
 * Nonpersistent external flags.
 */
#define	LK_RETRY	0x00010000 /* vn_lock: retry until locked */
#define	LK_INTERLOCK	0x00020000 /*
				    * unlock passed mutex after getting
				    * lk_interlock
				    */

/*
 * Default values for lockmgr_args().
 */
#define	LK_WMESG_DEFAULT	(NULL)
#define	LK_PRIO_DEFAULT		(-1)
#define	LK_TIMO_DEFAULT		(0)

/*
 * Internal lock flags.
 *
 * These flags are used internally to the lock manager.
 */
#define	LK_WANT_UPGRADE	0x00100000	/* waiting for share-to-excl upgrade */
#define	LK_WANT_EXCL	0x00200000	/* exclusive lock sought */
#define	LK_HAVE_EXCL	0x00400000	/* exclusive lock obtained */
#define	LK_WAITDRAIN	0x00800000	/* process waiting for lock to drain */
#define	LK_DRAINING	0x01000000	/* lock is being drained */
#define	LK_DESTROYED	0x02000000	/* lock is destroyed */
/*
 * Internal state flags corresponding to lk_sharecount, and lk_waitcount
 */
#define	LK_SHARE_NONZERO 0x10000000
#define	LK_WAIT_NONZERO  0x20000000

#ifndef LOCK_FILE
#error "LOCK_FILE not defined, include <sys/lock.h> before <sys/lockmgr.h>"
#endif

/*
 * Assertion flags.
 */
#if defined(INVARIANTS) || defined(INVARIANT_SUPPORT)
#define	KA_BASE		(LA_MASKASSERT + 1)
#define	KA_LOCKED	LA_LOCKED
#define	KA_SLOCKED	LA_SLOCKED
#define	KA_XLOCKED	LA_XLOCKED
#define	KA_UNLOCKED	LA_UNLOCKED
#define	KA_RECURSED	LA_RECURSED
#define	KA_NOTRECURSED	LA_NOTRECURSED
#define	KA_HELD		(KA_BASE << 0x00)
#define	KA_UNHELD	(KA_BASE << 0x01)
#endif

/*
 * Lock return status.
 *
 * Successfully obtained locks return 0. Locks will always succeed
 * unless one of the following is true:
 *	LK_FORCEUPGRADE is requested and some other process has already
 *	    requested a lock upgrade (returns EBUSY).
 *	LK_WAIT is set and a sleep would be required (returns EBUSY).
 *	LK_SLEEPFAIL is set and a sleep was done (returns ENOLCK).
 *	PCATCH is set in lock priority and a signal arrives (returns
 *	    either EINTR or ERESTART if system calls is to be restarted).
 *	Non-null lock timeout and timeout expires (returns EWOULDBLOCK).
 * A failed lock attempt always returns a non-zero error value. No lock
 * is held after an error return (in particular, a failed LK_UPGRADE
 * or LK_FORCEUPGRADE will have released its shared access lock).
 */

/*
 * Indicator that no process holds exclusive lock
 */
#define LK_KERNPROC ((struct thread *)-2)
#define LK_NOPROC ((struct thread *) -1)

struct thread;

void	lockinit(struct lock *, int prio, const char *wmesg,
			int timo, int flags);
void	lockdestroy(struct lock *);

int	_lockmgr_args(struct lock *, u_int flags, struct mtx *,
	    const char *wmesg, int prio, int timo, char *file, int line);
#if defined(INVARIANTS) || defined(INVARIANT_SUPPORT)
void	_lockmgr_assert(struct lock *, int what, const char *, int);
#endif
void	_lockmgr_disown(struct lock *, const char *, int);
void	lockmgr_printinfo(struct lock *);
int	lockstatus(struct lock *);

#define lockmgr(lock, flags, mtx)					\
	_lockmgr_args((lock), (flags), (mtx), LK_WMESG_DEFAULT,		\
	    LK_PRIO_DEFAULT, LK_TIMO_DEFAULT, LOCK_FILE, LOCK_LINE)
#define	lockmgr_disown(lock)						\
	_lockmgr_disown((lock), LOCK_FILE, LOCK_LINE)
#define	lockmgr_args(lock, flags, mtx, wmesg, prio, timo)		\
	_lockmgr_args((lock), (flags), (mtx), (wmesg), (prio), (timo),	\
	    LOCK_FILE, LOCK_LINE)
#define	lockmgr_recursed(lkp)						\
	((lkp)->lk_exclusivecount > 1)
#define	lockmgr_waiters(lkp)						\
	((lkp)->lk_waitcount != 0)
#ifdef INVARIANTS
#define	lockmgr_assert(lkp, what)					\
	_lockmgr_assert((lkp), (what), LOCK_FILE, LOCK_LINE)
#else
#define	lockmgr_assert(lkp, what)
#endif
#ifdef DDB
int	lockmgr_chain(struct thread *td, struct thread **ownerp);
#endif

#endif /* _KERNEL */

#endif /* !_SYS_LOCKMGR_H_ */
