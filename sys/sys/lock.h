/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: mutex.h,v 2.7.2.35 2000/04/27 03:10:26 cp Exp $
 * $FreeBSD$
 */

#ifndef _SYS_LOCK_H_
#define _SYS_LOCK_H_

/*
 * XXX - compatability until lockmgr() goes away or all the #includes are
 * updated.
 */
#include <sys/lockmgr.h>

#include <sys/queue.h>
#include <sys/_lock.h>

/*
 * Lock classes.  Each lock has a class which describes characteristics
 * common to all types of locks of a given class.
 *
 * Spin locks in general must always protect against preemption, as it is
 * an error to perform any type of context switch while holding a spin lock.
 * Also, for an individual lock to be recursable, its class must allow
 * recursion and the lock itself must explicitly allow recursion.
 */

struct lock_class {
	const	char *lc_name;
	u_int	lc_flags;
};

#define	LC_SLEEPLOCK	0x00000001	/* Sleep lock. */
#define	LC_SPINLOCK	0x00000002	/* Spin lock. */
#define	LC_SLEEPABLE	0x00000004	/* Sleeping allowed with this lock. */
#define	LC_RECURSABLE	0x00000008	/* Locks of this type may recurse. */
#define	LC_UPGRADABLE	0x00000010	/* Upgrades and downgrades permitted. */

#define	LO_CLASSFLAGS	0x0000ffff	/* Class specific flags. */
#define	LO_INITIALIZED	0x00010000	/* Lock has been initialized. */
#define	LO_WITNESS	0x00020000	/* Should witness monitor this lock. */
#define	LO_QUIET	0x00040000	/* Don't log locking operations. */
#define	LO_RECURSABLE	0x00080000	/* Lock may recurse. */
#define	LO_SLEEPABLE	0x00100000	/* Lock may be held while sleeping. */
#define	LO_UPGRADABLE	0x00200000	/* Lock may be upgraded/downgraded. */

#define	LI_RECURSEMASK	0x0000ffff	/* Recursion depth of lock instance. */
#define	LI_SLEPT	0x00010000	/* Lock instance has been slept with. */
#define	LI_EXCLUSIVE	0x00020000	/* Exclusive lock instance. */

/*
 * Option flags passed to lock operations that witness also needs to know
 * about or that are generic across all locks.
 */
#define	LOP_QUIET	0x00000002	/* Don't log locking operations. */
#define	LOP_TRYLOCK	0x00000004	/* Don't check lock order. */
#define	LOP_EXCLUSIVE	0x00000008	/* Exclusive lock. */

/* Flags passed to witness_assert. */
#define	LA_UNLOCKED	0x00000000	/* Lock is unlocked. */
#define	LA_LOCKED	0x00000001	/* Lock is at least share locked. */
#define	LA_SLOCKED	0x00000002	/* Lock is exactly share locked. */
#define	LA_XLOCKED	0x00000004	/* Lock is exclusively locked. */
#define	LA_RECURSED	0x00000008	/* Lock is recursed. */
#define	LA_NOTRECURSED	0x00000010	/* Lock is not recursed. */

#ifdef _KERNEL
/*
 * Lock instances.  A lock instance is the data associated with a lock while
 * it is held by witness.  For example, a lock instance will hold the
 * recursion count of a lock.  Lock instances are held in lists.  Spin locks
 * are held in a per-cpu list while sleep locks are held in per-process list.
 */
struct lock_instance {
	struct	lock_object *li_lock;
	const	char *li_file;		/* File and line of last acquire. */
	int	li_line;
	u_int	li_flags;		/* Recursion count and LI_* flags. */
};

/*
 * A simple list type used to build the list of locks held by a process
 * or CPU.  We can't simply embed the list in struct lock_object since a
 * lock may be held by more than one process if it is a shared lock.  Locks
 * are added to the head of the list, so we fill up each list entry from
 * "the back" logically.  To ease some of the arithmetic, we actually fill
 * in each list entry the normal way (childer[0] then children[1], etc.) but
 * when we traverse the list we read children[count-1] as the first entry
 * down to children[0] as the final entry.
 */
#define	LOCK_NCHILDREN	3

struct lock_list_entry {
	struct	lock_list_entry *ll_next;
	struct	lock_instance ll_children[LOCK_NCHILDREN];
	u_int	ll_count;
};

/*
 * If any of WITNESS, INVARIANTS, or KTR_LOCK KTR tracing has been enabled,
 * then turn on LOCK_DEBUG.  When this option is on, extra debugging
 * facilities such as tracking the file and line number of lock operations
 * are enabled.  Also, mutex locking operations are not inlined to avoid
 * bloat from all the extra debugging code.  We also have to turn on all the
 * calling conventions for this debugging code in modules so that modules can
 * work with both debug and non-debug kernels.
 */
#if defined(KLD_MODULE) || defined(WITNESS) || defined(INVARIANTS) || defined(INVARIANT_SUPPORT) || defined(KTR)
#define	LOCK_DEBUG	1
#else
#define	LOCK_DEBUG	0
#endif

/*
 * In the LOCK_DEBUG case, use the filename and line numbers for debugging
 * operations.  Otherwise, use default values to avoid the unneeded bloat.
 */
#if LOCK_DEBUG > 0
#define	LOCK_FILE	__FILE__
#define	LOCK_LINE	__LINE__
#else
#define	LOCK_FILE	NULL
#define	LOCK_LINE	0
#endif

/*
 * Macros for KTR_LOCK tracing.
 *
 * opname  - name of this operation (LOCK/UNLOCK/SLOCK, etc.)
 * lo      - struct lock_object * for this lock
 * flags   - flags passed to the lock operation
 * recurse - this locks recursion level (or 0 if class is not recursable)
 * result  - result of a try lock operation
 * file    - file name
 * line    - line number
 */
#define	LOCK_LOG_TEST(lo, flags)					\
	(((flags) & LOP_QUIET) == 0 && ((lo)->lo_flags & LO_QUIET) == 0)

#define	LOCK_LOG_LOCK(opname, lo, flags, recurse, file, line) do {	\
	if (LOCK_LOG_TEST((lo), (flags)))				\
		CTR5(KTR_LOCK, opname " (%s) %s r = %d at %s:%d",	\
		    (lo)->lo_class->lc_name, (lo)->lo_name,		\
		    (u_int)(recurse), (file), (line));			\
} while (0)

#define	LOCK_LOG_TRY(opname, lo, flags, result, file, line) do {	\
	if (LOCK_LOG_TEST((lo), (flags)))				\
		CTR5(KTR_LOCK, "TRY_" opname " (%s) %s result=%d at %s:%d",\
		    (lo)->lo_class->lc_name, (lo)->lo_name,		\
		    (u_int)(result), (file), (line));			\
} while (0)

#define	LOCK_LOG_INIT(lo, flags) do {					\
	if (LOCK_LOG_TEST((lo), (flags)))				\
		CTR4(KTR_LOCK, "%s: %p (%s) %s", __func__, (lo),	\
 		    (lo)->lo_class->lc_name, (lo)->lo_name);		\
} while (0)

#define	LOCK_LOG_DESTROY(lo, flags)	LOCK_LOG_INIT(lo, flags)

/*
 * Helpful macros for quickly coming up with assertions with informative
 * panic messages.
 */
#define MPASS(ex)		MPASS4(ex, #ex, __FILE__, __LINE__)
#define MPASS2(ex, what)	MPASS4(ex, what, __FILE__, __LINE__)
#define MPASS3(ex, file, line)	MPASS4(ex, #ex, file, line)
#define MPASS4(ex, what, file, line)					\
	KASSERT((ex), ("Assertion %s failed at %s:%d", what, file, line))

extern struct lock_class lock_class_mtx_sleep;
extern struct lock_class lock_class_mtx_spin;
extern struct lock_class lock_class_sx;

void	witness_init(struct lock_object *);
void	witness_destroy(struct lock_object *);
void	witness_lock(struct lock_object *, int, const char *, int);
void	witness_upgrade(struct lock_object *, int, const char *, int);
void	witness_downgrade(struct lock_object *, int, const char *, int);
void	witness_unlock(struct lock_object *, int, const char *, int);
void	witness_save(struct lock_object *, const char **, int *);
void	witness_restore(struct lock_object *, const char *, int);
int	witness_list_locks(struct lock_list_entry **);
int	witness_list(struct thread *);
int	witness_sleep(int, struct lock_object *, const char *, int);
void	witness_assert(struct lock_object *, int, const char *, int);

#ifdef	WITNESS
#define	WITNESS_INIT(lock)						\
	witness_init((lock))

#define WITNESS_DESTROY(lock)						\
	witness_destroy(lock)

#define	WITNESS_LOCK(lock, flags, file, line)				\
	witness_lock((lock), (flags), (file), (line))

#define	WITNESS_UPGRADE(lock, flags, file, line)			\
	witness_upgrade((lock), (flags), (file), (line))

#define	WITNESS_DOWNGRADE(lock, flags, file, line)			\
	witness_downgrade((lock), (flags), (file), (line))

#define	WITNESS_UNLOCK(lock, flags, file, line)				\
	witness_unlock((lock), (flags), (file), (line))

#define	WITNESS_SLEEP(check, lock) 					\
	witness_sleep((check), (lock), __FILE__, __LINE__)

#define	WITNESS_SAVE_DECL(n)						\
	const char * __CONCAT(n, __wf);					\
	int __CONCAT(n, __wl)

#define	WITNESS_SAVE(lock, n) 						\
	witness_save((lock), &__CONCAT(n, __wf), &__CONCAT(n, __wl))

#define	WITNESS_RESTORE(lock, n) 					\
	witness_restore((lock), __CONCAT(n, __wf), __CONCAT(n, __wl))

#else	/* WITNESS */
#define	WITNESS_INIT(lock)	(lock)->lo_flags |= LO_INITIALIZED
#define	WITNESS_DESTROY(lock)	(lock)->lo_flags &= ~LO_INITIALIZED
#define	WITNESS_LOCK(lock, flags, file, line)
#define	WITNESS_UPGRADE(lock, flags, file, line)
#define	WITNESS_DOWNGRADE(lock, flags, file, line)
#define	WITNESS_UNLOCK(lock, flags, file, line)
#define	WITNESS_SLEEP(check, lock)
#define	WITNESS_SAVE_DECL(n)
#define	WITNESS_SAVE(lock, n)
#define	WITNESS_RESTORE(lock, n)
#endif	/* WITNESS */

#endif	/* _KERNEL */
#endif	/* _SYS_LOCK_H_ */
