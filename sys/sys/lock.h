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

/*
 * Lock classes.  Each lock has a class which describes characteristics
 * common to all types of locks of a given class.
 *
 * Spin locks in general must always protect against preemption, as it is
 * an error to perform any type of context switch while holding a spin lock.
 * Also, for an individual lock to be recursable, its class must allow
 * recursion and the lock itself must explicitly allow recursion.
 */

struct	lock_class {
	const	char *lc_name;
	u_int	lc_flags;
};

#define	LC_SLEEPLOCK	0x00000001	/* Sleep lock. */
#define	LC_SPINLOCK	0x00000002	/* Spin lock. */
#define	LC_SLEEPABLE	0x00000004	/* Sleeping allowed with this lock. */
#define	LC_RECURSABLE	0x00000008	/* Locks of this type may recurse. */

struct	witness;

struct	lock_object {
	struct	lock_class *lo_class;
	const	char *lo_name;
	const	char *lo_file;		/* File and line of last acquire. */
	int	lo_line;
	u_int	lo_flags;
	STAILQ_ENTRY(lock_object) lo_list; /* List of all locks in system. */
	struct	witness *lo_witness;
};

#define	LO_CLASSFLAGS	0x0000ffff	/* Class specific flags. */
#define	LO_INITIALIZED	0x00010000	/* Lock has been initialized. */
#define	LO_WITNESS	0x00020000	/* Should witness monitor this lock. */
#define	LO_QUIET	0x00040000	/* Don't log locking operations. */
#define	LO_RECURSABLE	0x00080000	/* Lock may recurse. */
#define	LO_SLEEPABLE	0x00100000	/* Lock may be held while sleeping. */
#define	LO_LOCKED	0x01000000	/* Someone holds this lock. */
#define	LO_RECURSED	0x02000000	/* Someone has recursed on this lock. */

/*
 * Option flags passed to lock operations that witness also needs to know
 * about or that are generic across all locks.
 */
#define	LOP_NOSWITCH	0x00000001	/* Lock doesn't switch on release. */
#define	LOP_QUIET	0x00000002	/* Don't log locking operations. */
#define	LOP_TRYLOCK	0x00000004	/* Don't check lock order. */

#ifdef _KERNEL
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
#define	LOCK_NCHILDREN	6

struct	lock_list_entry {
	struct	lock_list_entry *ll_next;
	struct	lock_object *ll_children[LOCK_NCHILDREN];
	u_int	ll_count;
};

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
		CTR3(KTR_LOCK, __func__ ": %p (%s) %s",	(lo),		\
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
void	witness_unlock(struct lock_object *, int, const char *, int);
void	witness_save(struct lock_object *, const char **, int *);
void	witness_restore(struct lock_object *, const char *, int);
int	witness_list(struct proc *);
int	witness_sleep(int, struct lock_object *, const char *, int);

#ifdef	WITNESS
#define	WITNESS_INIT(lock)						\
	witness_init((lock))

#define WITNESS_DESTROY(lock)						\
	witness_destroy(lock)

#define	WITNESS_LOCK(lock, flags, file, line)				\
	witness_lock((lock), (flags), (file), (line))

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
#define WITNESS_DESTROY(lock)	(lock)->lo_flags &= ~LO_INITIALIZED
#define	WITNESS_LOCK(lock, flags, file, line)
#define	WITNESS_UNLOCK(lock, flags, file, line)
#define	WITNESS_SLEEP(check, lock)
#define	WITNESS_SAVE_DECL(n)
#define	WITNESS_SAVE(lock, n)
#define	WITNESS_RESTORE(lock, n)
#endif	/* WITNESS */

#endif	/* _KERNEL */
#endif	/* _SYS_LOCK_H_ */
