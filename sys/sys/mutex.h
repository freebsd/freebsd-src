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

#ifndef _SYS_MUTEX_H_
#define _SYS_MUTEX_H_

#ifndef LOCORE
#include <sys/queue.h>

#ifdef _KERNEL
#include <sys/ktr.h>
#include <sys/systm.h>
#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/globals.h>
#endif	/* _KERNEL_ */
#endif	/* !LOCORE */

#include <machine/mutex.h>

#ifdef _KERNEL

/*
 * Mutex types and options stored in mutex->mtx_flags 
 */
#define	MTX_DEF		0x00000000	/* DEFAULT (sleep) lock */ 
#define MTX_SPIN	0x00000001	/* Spin lock (disables interrupts) */
#define MTX_RECURSE	0x00000002	/* Option: lock allowed to recurse */

/*
 * Option flags passed to certain lock/unlock routines, through the use
 * of corresponding mtx_{lock,unlock}_flags() interface macros.
 *
 * XXX: The only reason we make these bits not interfere with the above "types
 *	and options" bits is because we have to pass both to the witness
 *	routines right now; if/when we clean up the witness interface to
 *	not check for mutex type from the passed in flag, but rather from
 *	the mutex lock's mtx_flags field, then we can change these values to
 *	0x1, 0x2, ... 
 */
#define	MTX_NOSWITCH	0x00000004	/* Do not switch on release */
#define	MTX_QUIET	0x00000008	/* Don't log a mutex event */

/*
 * State bits kept in mutex->mtx_lock, for the DEFAULT lock type. None of this,
 * with the exception of MTX_UNOWNED, applies to spin locks.
 */
#define	MTX_RECURSED	0x00000001	/* lock recursed (for MTX_DEF only) */
#define	MTX_CONTESTED	0x00000002	/* lock contested (for MTX_DEF only) */
#define MTX_UNOWNED	0x00000004	/* Cookie for free mutex */
#define	MTX_FLAGMASK	~(MTX_RECURSED | MTX_CONTESTED)

#endif	/* _KERNEL */

#ifndef LOCORE

struct mtx_debug;

/*
 * Sleep/spin mutex
 */
struct mtx {
	volatile uintptr_t mtx_lock;	/* owner (and state for sleep locks) */
	volatile u_int	mtx_recurse;	/* number of recursive holds */
	u_int		mtx_savecrit;	/* saved flags (for spin locks) */
	int		mtx_flags;	/* flags passed to mtx_init() */
	const char	*mtx_description;
	TAILQ_HEAD(, proc) mtx_blocked;	/* threads blocked on this lock */
	LIST_ENTRY(mtx)	mtx_contested;	/* list of all contested locks */
	struct mtx	*mtx_next;	/* all existing locks 	*/
	struct mtx	*mtx_prev;	/*  in system...	*/
	struct mtx_debug *mtx_debug;	/* debugging information... */
};

/*
 * XXX: Friendly reminder to fix things in MP code that is presently being
 * XXX: worked on.
 */
#define mp_fixme(string)

#ifdef _KERNEL

/*
 * Strings for KTR_LOCK tracing.
 */
extern char	STR_mtx_lock_slp[];
extern char	STR_mtx_lock_spn[];
extern char	STR_mtx_unlock_slp[];
extern char	STR_mtx_unlock_spn[]; 

/*
 * Prototypes
 *
 * NOTE: Functions prepended with `_' (underscore) are exported to other parts
 *	 of the kernel via macros, thus allowing us to use the cpp __FILE__
 *	 and __LINE__. These functions should not be called directly by any
 *	 code using the IPI. Their macros cover their functionality.
 *
 * [See below for descriptions]
 *
 */
void	mtx_init(struct mtx *m, const char *description, int opts);
void	mtx_destroy(struct mtx *m);
void	_mtx_lock_sleep(struct mtx *m, int opts, const char *file, int line);
void	_mtx_unlock_sleep(struct mtx *m, int opts, const char *file, int line);
void	_mtx_lock_spin(struct mtx *m, int opts, critical_t mtx_crit,
		       const char *file, int line);
void	_mtx_unlock_spin(struct mtx *m, int opts, const char *file, int line);
int	_mtx_trylock(struct mtx *m, int opts, const char *file, int line);
void	_mtx_lock_flags(struct mtx *m, int opts, const char *file, int line);
void	_mtx_unlock_flags(struct mtx *m, int opts, const char *file, int line);
void	_mtx_lock_spin_flags(struct mtx *m, int opts, const char *file,
			     int line);
void	_mtx_unlock_spin_flags(struct mtx *m, int opts, const char *file,
			     int line);
#ifdef INVARIANT_SUPPORT
void	_mtx_assert(struct mtx *m, int what, const char *file, int line);
#endif

/*
 * We define our machine-independent (unoptimized) mutex micro-operations
 * here, if they are not already defined in the machine-dependent mutex.h 
 */

/* Actually obtain mtx_lock */
#ifndef _obtain_lock
#define _obtain_lock(mp, tid)						\
	atomic_cmpset_acq_ptr(&(mp)->mtx_lock, (void *)MTX_UNOWNED, (tid))
#endif

/* Actually release mtx_lock */
#ifndef _release_lock
#define _release_lock(mp, tid)						\
	atomic_cmpset_rel_ptr(&(mp)->mtx_lock, (tid), (void *)MTX_UNOWNED)
#endif

/* Actually release mtx_lock quickly, assuming we own it. */
#ifndef _release_lock_quick
#define _release_lock_quick(mp)						\
	atomic_store_rel_ptr(&(mp)->mtx_lock, (void *)MTX_UNOWNED)
#endif

/*
 * Obtain a sleep lock inline, or call the "hard" function if we can't get it
 * easy.
 */
#ifndef _get_sleep_lock
#define _get_sleep_lock(mp, tid, opts, file, line) do {			\
	if (!_obtain_lock((mp), (tid)))					\
		_mtx_lock_sleep((mp), (opts), (file), (line));		\
} while (0)
#endif

/*
 * Obtain a spin lock inline, or call the "hard" function if we can't get it
 * easy. For spinlocks, we handle recursion inline (it turns out that function
 * calls can be significantly expensive on some architectures).
 * Since spin locks are not _too_ common, inlining this code is not too big 
 * a deal.
 */
#ifndef _get_spin_lock
#define _get_spin_lock(mp, tid, opts, file, line) do {			\
	critical_t _mtx_crit;						\
	_mtx_crit = critical_enter();					\
	if (!_obtain_lock((mp), (tid))) {				\
		if ((mp)->mtx_lock == (uintptr_t)(tid))			\
			(mp)->mtx_recurse++;				\
		else							\
			_mtx_lock_spin((mp), (opts), _mtx_crit, (file),	\
			    (line));					\
	} else								\
		(mp)->mtx_savecrit = _mtx_crit;				\
} while (0)
#endif

/*
 * Release a sleep lock inline, or call the "hard" function if we can't do it
 * easy.
 */
#ifndef _rel_sleep_lock
#define _rel_sleep_lock(mp, tid, opts, file, line) do {			\
	if (!_release_lock((mp), (tid)))				\
		_mtx_unlock_sleep((mp), (opts), (file), (line));	\
} while (0)
#endif

/*
 * For spinlocks, we can handle everything inline, as it's pretty simple and
 * a function call would be too expensive (at least on some architectures).
 * Since spin locks are not _too_ common, inlining this code is not too big 
 * a deal.
 */
#ifndef _rel_spin_lock
#define _rel_spin_lock(mp) do {						\
	critical_t _mtx_crit = (mp)->mtx_savecrit;			\
	if (mtx_recursed((mp)))						\
		(mp)->mtx_recurse--;					\
	else {								\
		_release_lock_quick((mp));				\
		critical_exit(_mtx_crit);				\
	}								\
} while (0)
#endif

/*
 * Exported lock manipulation interface.
 *
 * mtx_lock(m) locks MTX_DEF mutex `m'
 *
 * mtx_lock_spin(m) locks MTX_SPIN mutex `m'
 *
 * mtx_unlock(m) unlocks MTX_DEF mutex `m'
 *
 * mtx_unlock_spin(m) unlocks MTX_SPIN mutex `m'
 *
 * mtx_lock_spin_flags(m, opts) and mtx_lock_flags(m, opts) locks mutex `m'
 *     and passes option flags `opts' to the "hard" function, if required.
 *     With these routines, it is possible to pass flags such as MTX_QUIET
 *     and/or MTX_NOSWITCH to the appropriate lock manipulation routines.
 *
 * mtx_trylock(m) attempts to acquire MTX_DEF mutex `m' but doesn't sleep if
 *     it cannot. Rather, it returns 0 on failure and non-zero on success.
 *     It does NOT handle recursion as we assume that if a caller is properly
 *     using this part of the interface, he will know that the lock in question
 *     is _not_ recursed.
 *
 * mtx_trylock_flags(m, opts) is used the same way as mtx_trylock() but accepts
 *     relevant option flags `opts.'
 *
 * mtx_owned(m) returns non-zero if the current thread owns the lock `m'
 *
 * mtx_recursed(m) returns non-zero if the lock `m' is presently recursed.
 */ 
#define mtx_lock(m)		mtx_lock_flags((m), 0)
#define mtx_lock_spin(m)	mtx_lock_spin_flags((m), 0)
#define mtx_trylock(m)		mtx_trylock_flags((m), 0)
#define mtx_unlock(m)		mtx_unlock_flags((m), 0)
#define mtx_unlock_spin(m)	mtx_unlock_spin_flags((m), 0)

#ifdef KLD_MODULE
#define	mtx_lock_flags(m, opts)						\
	_mtx_lock_flags((m), (opts), __FILE__, __LINE__)
#define	mtx_unlock_flags(m, opts)					\
	_mtx_unlock_flags((m), (opts), __FILE__, __LINE__)
#define	mtx_lock_spin_flags(m, opts)					\
	_mtx_lock_spin_flags((m), (opts), __FILE__, __LINE__)
#define	mtx_unlock_spin_flags(m, opts)					\
	_mtx_unlock_spin_flags((m), (opts), __FILE__, __LINE__)
#else
#define	mtx_lock_flags(m, opts)						\
	__mtx_lock_flags((m), (opts), __FILE__, __LINE__)
#define	mtx_unlock_flags(m, opts)					\
	__mtx_unlock_flags((m), (opts), __FILE__, __LINE__)
#define	mtx_lock_spin_flags(m, opts)					\
	__mtx_lock_spin_flags((m), (opts), __FILE__, __LINE__)
#define	mtx_unlock_spin_flags(m, opts)					\
	__mtx_unlock_spin_flags((m), (opts), __FILE__, __LINE__)
#endif

#define __mtx_lock_flags(m, opts, file, line) do {			\
	MPASS(curproc != NULL);						\
	KASSERT(((opts) & MTX_NOSWITCH) == 0,				\
	    ("MTX_NOSWITCH used at %s:%d", (file), (line)));		\
	_get_sleep_lock((m), curproc, (opts), (file), (line));		\
	if (((opts) & MTX_QUIET) == 0)					\
		CTR5(KTR_LOCK, STR_mtx_lock_slp,			\
		    (m)->mtx_description, (m), (m)->mtx_recurse, 	\
		    (file), (line));					\
	WITNESS_ENTER((m), ((m)->mtx_flags | (opts)), (file), (line));	\
} while (0)

#define __mtx_lock_spin_flags(m, opts, file, line) do {			\
	MPASS(curproc != NULL);						\
	_get_spin_lock((m), curproc, (opts), (file), (line));		\
	if (((opts) & MTX_QUIET) == 0)					\
		CTR5(KTR_LOCK, STR_mtx_lock_spn,			\
		    (m)->mtx_description, (m), (m)->mtx_recurse, 	\
		    (file), (line));					\
	WITNESS_ENTER((m), ((m)->mtx_flags | (opts)), (file), (line));	\
} while (0)

#define __mtx_unlock_flags(m, opts, file, line) do {			\
	MPASS(curproc != NULL);						\
	mtx_assert((m), MA_OWNED);					\
	WITNESS_EXIT((m), ((m)->mtx_flags | (opts)), (file), (line));	\
	_rel_sleep_lock((m), curproc, (opts), (file), (line));		\
	if (((opts) & MTX_QUIET) == 0)					\
		CTR5(KTR_LOCK, STR_mtx_unlock_slp,			\
		    (m)->mtx_description, (m), (m)->mtx_recurse, 	\
		    (file), (line));					\
} while (0)

#define __mtx_unlock_spin_flags(m, opts, file, line) do {		\
	MPASS(curproc != NULL);						\
	mtx_assert((m), MA_OWNED);					\
	WITNESS_EXIT((m), ((m)->mtx_flags | (opts)), (file), (line));	\
	_rel_spin_lock((m));						\
	if (((opts) & MTX_QUIET) == 0)					\
		CTR5(KTR_LOCK, STR_mtx_unlock_spn,			\
		    (m)->mtx_description, (m), (m)->mtx_recurse, 	\
		    (file), (line));					\
} while (0)

#define mtx_trylock_flags(m, opts)					\
	_mtx_trylock((m), (opts), __FILE__, __LINE__)

#define mtx_owned(m)	(((m)->mtx_lock & MTX_FLAGMASK) == (uintptr_t)curproc)

#define mtx_recursed(m)	((m)->mtx_recurse != 0)

/*
 * Global locks.
 */
extern struct mtx	sched_lock;
extern struct mtx	Giant;

/*
 * Giant lock manipulation and clean exit macros.
 * Used to replace return with an exit Giant and return.
 *
 * Note that DROP_GIANT*() needs to be paired with PICKUP_GIANT() 
 */
#define DROP_GIANT_NOSWITCH()						\
do {									\
	int _giantcnt;							\
	WITNESS_SAVE_DECL(Giant);					\
									\
	if (mtx_owned(&Giant))						\
		WITNESS_SAVE(&Giant, Giant);				\
	for (_giantcnt = 0; mtx_owned(&Giant); _giantcnt++)		\
		mtx_unlock_flags(&Giant, MTX_NOSWITCH)

#define DROP_GIANT()							\
do {									\
	int _giantcnt;							\
	WITNESS_SAVE_DECL(Giant);					\
									\
	if (mtx_owned(&Giant))						\
		WITNESS_SAVE(&Giant, Giant);				\
	for (_giantcnt = 0; mtx_owned(&Giant); _giantcnt++)		\
		mtx_unlock(&Giant)

#define PICKUP_GIANT()							\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_lock(&Giant);					\
	if (mtx_owned(&Giant))						\
		WITNESS_RESTORE(&Giant, Giant);				\
} while (0)

#define PARTIAL_PICKUP_GIANT()						\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_lock(&Giant);					\
	if (mtx_owned(&Giant))						\
		WITNESS_RESTORE(&Giant, Giant)

/*
 * The INVARIANTS-enabled mtx_assert() functionality.
 *
 * The constants need to be defined for INVARIANT_SUPPORT infrastructure
 * support as _mtx_assert() itself uses them and the latter implies that
 * _mtx_assert() must build.
 */
#ifdef INVARIANT_SUPPORT
#define MA_OWNED	0x01
#define MA_NOTOWNED	0x02
#define MA_RECURSED	0x04
#define MA_NOTRECURSED	0x08
#endif /* INVARIANT_SUPPORT */

#ifdef INVARIANTS
#define	mtx_assert(m, what)						\
	_mtx_assert((m), (what), __FILE__, __LINE__)

#else	/* INVARIANTS */
#define mtx_assert(m, what)
#endif	/* INVARIANTS */

#define MPASS(ex)		MPASS4(ex, #ex, __FILE__, __LINE__)
#define MPASS2(ex, what)	MPASS4(ex, what, __FILE__, __LINE__)
#define MPASS3(ex, file, line)	MPASS4(ex, #ex, file, line)
#define MPASS4(ex, what, file, line)					\
	KASSERT((ex), ("Assertion %s failed at %s:%d", what, file, line))

/*
 * Exported WITNESS-enabled functions and corresponding wrapper macros.
 */
#ifdef	WITNESS
void	witness_save(struct mtx *, const char **, int *);
void	witness_restore(struct mtx *, const char *, int);
void	witness_enter(struct mtx *, int, const char *, int);
void	witness_try_enter(struct mtx *, int, const char *, int);
void	witness_exit(struct mtx *, int, const char *, int);
int	witness_list(struct proc *);
int	witness_sleep(int, struct mtx *, const char *, int);

#define WITNESS_ENTER(m, t, f, l)					\
	witness_enter((m), (t), (f), (l))

#define WITNESS_EXIT(m, t, f, l)					\
	witness_exit((m), (t), (f), (l))

#define	WITNESS_SLEEP(check, m) 					\
	witness_sleep(check, (m), __FILE__, __LINE__)

#define	WITNESS_SAVE_DECL(n)						\
	const char * __CONCAT(n, __wf);					\
	int __CONCAT(n, __wl)

#define	WITNESS_SAVE(m, n) 						\
	witness_save(m, &__CONCAT(n, __wf), &__CONCAT(n, __wl))

#define	WITNESS_RESTORE(m, n) 						\
	witness_restore(m, __CONCAT(n, __wf), __CONCAT(n, __wl))

#else	/* WITNESS */
#define witness_enter(m, t, f, l)
#define witness_tryenter(m, t, f, l)
#define witness_exit(m, t, f, l)
#define	witness_list(p)
#define	witness_sleep(c, m, f, l)

#define WITNESS_ENTER(m, t, f, l)
#define WITNESS_EXIT(m, t, f, l)
#define	WITNESS_SLEEP(check, m)
#define	WITNESS_SAVE_DECL(n)
#define	WITNESS_SAVE(m, n)
#define	WITNESS_RESTORE(m, n)
#endif	/* WITNESS */

#endif	/* _KERNEL */
#endif	/* !LOCORE */
#endif	/* _SYS_MUTEX_H_ */
