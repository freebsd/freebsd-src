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
#include <sys/_lock.h>
#include <sys/_mutex.h>

#ifdef _KERNEL
#include <sys/pcpu.h>
#include <machine/atomic.h>
#include <machine/cpufunc.h>
#endif	/* _KERNEL_ */
#endif	/* !LOCORE */

#include <machine/mutex.h>

#ifdef _KERNEL

/*
 * Mutex types and options passed to mtx_init().  MTX_QUIET can also be
 * passed in.
 */
#define	MTX_DEF		0x00000000	/* DEFAULT (sleep) lock */ 
#define MTX_SPIN	0x00000001	/* Spin lock (disables interrupts) */
#define MTX_RECURSE	0x00000004	/* Option: lock allowed to recurse */
#define	MTX_NOWITNESS	0x00000008	/* Don't do any witness checking. */
#define	MTX_SLEEPABLE	0x00000010	/* We can sleep with this lock. */
#define	MTX_DUPOK	0x00000020	/* Don't log a duplicate acquire */

/*
 * Option flags passed to certain lock/unlock routines, through the use
 * of corresponding mtx_{lock,unlock}_flags() interface macros.
 */
#define	MTX_QUIET	LOP_QUIET	/* Don't log a mutex event */

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

/*
 * XXX: Friendly reminder to fix things in MP code that is presently being
 * XXX: worked on.
 */
#define mp_fixme(string)

#ifdef _KERNEL

/*
 * Prototypes
 *
 * NOTE: Functions prepended with `_' (underscore) are exported to other parts
 *	 of the kernel via macros, thus allowing us to use the cpp LOCK_FILE
 *	 and LOCK_LINE. These functions should not be called directly by any
 *	 code using the API. Their macros cover their functionality.
 *
 * [See below for descriptions]
 *
 */
void	mtx_init(struct mtx *m, const char *description, int opts);
void	mtx_destroy(struct mtx *m);
void	mtx_sysinit(void *arg);
void	mutex_init(void);
void	_mtx_lock_sleep(struct mtx *m, int opts, const char *file, int line);
void	_mtx_unlock_sleep(struct mtx *m, int opts, const char *file, int line);
void	_mtx_lock_spin(struct mtx *m, int opts, const char *file, int line);
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
int	mtx_lock_giant(int sysctlvar);
void	mtx_unlock_giant(int s);

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
	critical_enter();						\
	if (!_obtain_lock((mp), (tid))) {				\
		if ((mp)->mtx_lock == (uintptr_t)(tid))			\
			(mp)->mtx_recurse++;				\
		else							\
			_mtx_lock_spin((mp), (opts), (file), (line));	\
	}								\
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
 *
 * Since we always perform a critical_enter() when attempting to acquire a
 * spin lock, we need to always perform a matching critical_exit() when
 * releasing a spin lock.  This includes the recursion cases.
 */
#ifndef _rel_spin_lock
#define _rel_spin_lock(mp) do {						\
	if (mtx_recursed((mp)))						\
		(mp)->mtx_recurse--;					\
	else								\
		_release_lock_quick((mp));				\
	critical_exit();					\
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
 *     to the appropriate lock manipulation routines.
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
 * mtx_initialized(m) returns non-zero if the lock `m' has been initialized.
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

struct mtx *mtx_pool_find(void *ptr);
struct mtx *mtx_pool_alloc(void);
void mtx_pool_lock(void *ptr);
void mtx_pool_unlock(void *ptr);

extern int mtx_pool_valid;

#ifndef LOCK_DEBUG
#error LOCK_DEBUG not defined, include <sys/lock.h> before <sys/mutex.h>
#endif
#if LOCK_DEBUG > 0
#define	mtx_lock_flags(m, opts)						\
	_mtx_lock_flags((m), (opts), LOCK_FILE, LOCK_LINE)
#define	mtx_unlock_flags(m, opts)					\
	_mtx_unlock_flags((m), (opts), LOCK_FILE, LOCK_LINE)
#define	mtx_lock_spin_flags(m, opts)					\
	_mtx_lock_spin_flags((m), (opts), LOCK_FILE, LOCK_LINE)
#define	mtx_unlock_spin_flags(m, opts)					\
	_mtx_unlock_spin_flags((m), (opts), LOCK_FILE, LOCK_LINE)
#else
#define	mtx_lock_flags(m, opts)						\
	_get_sleep_lock((m), curthread, (opts), LOCK_FILE, LOCK_LINE)
#define	mtx_unlock_flags(m, opts)					\
	_rel_sleep_lock((m), curthread, (opts), LOCK_FILE, LOCK_LINE)
#define	mtx_lock_spin_flags(m, opts)					\
	_get_spin_lock((m), curthread, (opts), LOCK_FILE, LOCK_LINE)
#define	mtx_unlock_spin_flags(m, opts)					\
	_rel_spin_lock((m))
#endif

#define mtx_trylock_flags(m, opts)					\
	_mtx_trylock((m), (opts), LOCK_FILE, LOCK_LINE)

#define	mtx_initialized(m)	((m)->mtx_object.lo_flags & LO_INITIALIZED)

#define mtx_owned(m)	(((m)->mtx_lock & MTX_FLAGMASK) == (uintptr_t)curthread)

#define mtx_recursed(m)	((m)->mtx_recurse != 0)

#define mtx_name(m)	((m)->mtx_object.lo_name)

/*
 * Global locks.
 */
extern struct mtx sched_lock;
extern struct mtx Giant;

/*
 * Giant lock sysctl variables used by other modules
 */
extern int kern_giant_proc;
extern int kern_giant_file;
extern int kern_giant_ucred;

/*
 * Giant lock manipulation and clean exit macros.
 * Used to replace return with an exit Giant and return.
 *
 * Note that DROP_GIANT*() needs to be paired with PICKUP_GIANT() 
 */
#define DROP_GIANT()							\
do {									\
	int _giantcnt;							\
	WITNESS_SAVE_DECL(Giant);					\
									\
	if (mtx_owned(&Giant))						\
		WITNESS_SAVE(&Giant.mtx_object, Giant);			\
	for (_giantcnt = 0; mtx_owned(&Giant); _giantcnt++)		\
		mtx_unlock(&Giant)

#define PICKUP_GIANT()							\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_lock(&Giant);					\
	if (mtx_owned(&Giant))						\
		WITNESS_RESTORE(&Giant.mtx_object, Giant);		\
} while (0)

#define PARTIAL_PICKUP_GIANT()						\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_lock(&Giant);					\
	if (mtx_owned(&Giant))						\
		WITNESS_RESTORE(&Giant.mtx_object, Giant)

#define	UGAR(rval) do {							\
	int _val = (rval);						\
	mtx_unlock(&Giant);						\
	return (_val);							\
} while (0)

struct mtx_args {
	struct mtx	*ma_mtx;
	const char 	*ma_desc;
	int		 ma_opts;
};

#define	MTX_SYSINIT(name, mtx, desc, opts)				\
	static struct mtx_args name##_args = {				\
		mtx,							\
		desc,							\
		opts							\
	};								\
	SYSINIT(name##_mtx_sysinit, SI_SUB_LOCK, SI_ORDER_MIDDLE,	\
	    mtx_sysinit, &name##_args)

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

#define GIANT_REQUIRED	mtx_assert(&Giant, MA_OWNED)

#else	/* INVARIANTS */
#define mtx_assert(m, what)
#define GIANT_REQUIRED
#endif	/* INVARIANTS */

#endif	/* _KERNEL */
#endif	/* !LOCORE */
#endif	/* _SYS_MUTEX_H_ */
