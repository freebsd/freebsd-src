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
#include <sys/proc.h>	/* Needed for curproc. */
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/globals.h>
#endif	/* _KERNEL_ */
#endif	/* !LOCORE */

#include <machine/mutex.h>

#ifndef LOCORE
#ifdef _KERNEL

/*
 * If kern_mutex.c is being built, compile non-inlined versions of various
 * functions so that kernel modules can use them.
 */
#ifndef _KERN_MUTEX_C_
#define _MTX_INLINE	static __inline
#else
#define _MTX_INLINE
#endif

/*
 * Mutex flags
 *
 * Types
 */
#define	MTX_DEF		0x0		/* Default (spin/sleep) */
#define MTX_SPIN	0x1		/* Spin only lock */

/* Options */
#define	MTX_RLIKELY	0x4		/* (opt) Recursion likely */
#define	MTX_NORECURSE	0x8		/* No recursion possible */
#define	MTX_NOSPIN	0x10		/* Don't spin before sleeping */
#define	MTX_NOSWITCH	0x20		/* Do not switch on release */
#define	MTX_FIRST	0x40		/* First spin lock holder */
#define MTX_TOPHALF	0x80		/* Interrupts not disabled on spin */
#define MTX_COLD	0x100		/* Mutex init'd before malloc works */

/* options that should be passed on to mtx_enter_hard, mtx_exit_hard */
#define	MTX_HARDOPTS	(MTX_SPIN | MTX_FIRST | MTX_TOPHALF | MTX_NOSWITCH)

/* Flags/value used in mtx_lock */
#define	MTX_RECURSE	0x01		/* (non-spin) lock held recursively */
#define	MTX_CONTESTED	0x02		/* (non-spin) lock contested */
#define	MTX_FLAGMASK	~(MTX_RECURSE | MTX_CONTESTED)
#define MTX_UNOWNED	0x8		/* Cookie for free mutex */

#endif	/* _KERNEL */

#ifdef MUTEX_DEBUG
struct mtx_debug {
	/* If you add anything here, adjust the mtxf_t definition below */
	struct witness	*mtxd_witness;
	LIST_ENTRY(mtx)	mtxd_held;
	const char	*mtxd_file;
	int		mtxd_line;
	const char	*mtxd_description;
};

#define mtx_description	mtx_debug->mtxd_description
#define mtx_held	mtx_debug->mtxd_held
#define	mtx_line	mtx_debug->mtxd_line
#define	mtx_file	mtx_debug->mtxd_file
#define	mtx_witness	mtx_debug->mtxd_witness
#endif

/*
 * Sleep/spin mutex
 */
struct mtx {
	volatile uintptr_t mtx_lock;	/* lock owner/gate/flags */
	volatile u_int	mtx_recurse;	/* number of recursive holds */
	u_int		mtx_saveintr;	/* saved flags (for spin locks) */
#ifdef MUTEX_DEBUG
	struct mtx_debug *mtx_debug;
#else
	const char	*mtx_description;
#endif
	TAILQ_HEAD(, proc) mtx_blocked;
	LIST_ENTRY(mtx)	mtx_contested;
	struct mtx	*mtx_next;	/* all locks in system */
	struct mtx	*mtx_prev;
};

#ifdef	MUTEX_DEBUG
#define	MUTEX_DECLARE(modifiers, name)					\
	static struct mtx_debug __mtx_debug_##name;			\
	modifiers struct mtx name = { 0, 0, 0, &__mtx_debug_##name }
#else
#define MUTEX_DECLARE(modifiers, name)	modifiers struct mtx name
#endif

#define mp_fixme(string)

#ifdef _KERNEL
/* Misc */
#define CURTHD	CURPROC	/* Current thread ID */

/* Prototypes */
void	mtx_init(struct mtx *m, const char *description, int flag);
void	mtx_enter_hard(struct mtx *, int type, int saveintr);
void	mtx_exit_hard(struct mtx *, int type);
void	mtx_destroy(struct mtx *m);

/*
 * Wrap the following functions with cpp macros so that filenames and line
 * numbers are embedded in the code correctly.
 */
#if (defined(KLD_MODULE) || defined(_KERN_MUTEX_C_))
void	_mtx_enter(struct mtx *mtxp, int type, const char *file, int line);
int	_mtx_try_enter(struct mtx *mtxp, int type, const char *file, int line);
void	_mtx_exit(struct mtx *mtxp, int type, const char *file, int line);
#endif

#define	mtx_enter(mtxp, type)						\
	_mtx_enter((mtxp), (type), __FILE__, __LINE__)

#define	mtx_try_enter(mtxp, type)					\
	_mtx_try_enter((mtxp), (type), __FILE__, __LINE__)

#define	mtx_exit(mtxp, type)						\
	_mtx_exit((mtxp), (type), __FILE__, __LINE__)

/* Global locks */
extern struct mtx	sched_lock;
extern struct mtx	Giant;

/*
 * Used to replace return with an exit Giant and return.
 */

#define EGAR(a)								\
do {									\
	mtx_exit(&Giant, MTX_DEF);					\
	return (a);							\
} while (0)

#define VEGAR								\
do {									\
	mtx_exit(&Giant, MTX_DEF);					\
	return;								\
} while (0)

#define DROP_GIANT()							\
do {									\
	int _giantcnt;							\
	WITNESS_SAVE_DECL(Giant);					\
									\
	if (mtx_owned(&Giant))						\
		WITNESS_SAVE(&Giant, Giant);				\
	for (_giantcnt = 0; mtx_owned(&Giant); _giantcnt++)		\
		mtx_exit(&Giant, MTX_DEF)

#define PICKUP_GIANT()							\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_enter(&Giant, MTX_DEF);				\
	if (mtx_owned(&Giant))						\
		WITNESS_RESTORE(&Giant, Giant);				\
} while (0)

#define PARTIAL_PICKUP_GIANT()						\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_enter(&Giant, MTX_DEF);				\
	if (mtx_owned(&Giant))						\
		WITNESS_RESTORE(&Giant, Giant)


/*
 * Debugging
 */
#ifdef INVARIANTS
#define MA_OWNED	1
#define MA_NOTOWNED	2
#define mtx_assert(m, what) {						\
	switch ((what)) {						\
	case MA_OWNED:							\
		if (!mtx_owned((m)))					\
			panic("mutex %s not owned at %s:%d",		\
			    (m)->mtx_description, __FILE__, __LINE__);	\
		break;							\
	case MA_NOTOWNED:						\
		if (mtx_owned((m)))					\
			panic("mutex %s owned at %s:%d",		\
			    (m)->mtx_description, __FILE__, __LINE__);	\
		break;							\
	default:							\
		panic("unknown mtx_assert at %s:%d", __FILE__, __LINE__); \
	}								\
}
#else	/* INVARIANTS */
#define mtx_assert(m, what)
#endif	/* INVARIANTS */

#ifdef MUTEX_DEBUG
#define MPASS(ex) if (!(ex)) panic("Assertion %s failed at %s:%d",	\
                #ex, __FILE__, __LINE__)
#define MPASS2(ex, what) if (!(ex)) panic("Assertion %s failed at %s:%d", \
                what, __FILE__, __LINE__)

#else	/* MUTEX_DEBUG */
#define	MPASS(ex)
#define	MPASS2(ex, where)
#endif	/* MUTEX_DEBUG */

#ifdef	WITNESS
#ifndef	MUTEX_DEBUG
#error	WITNESS requires MUTEX_DEBUG
#endif	/* MUTEX_DEBUG */
#define WITNESS_ENTER(m, t, f, l)					\
	if ((m)->mtx_witness != NULL)					\
		witness_enter((m), (t), (f), (l))
#define WITNESS_EXIT(m, t, f, l)					\
	if ((m)->mtx_witness != NULL)					\
		witness_exit((m), (t), (f), (l))

#define	WITNESS_SLEEP(check, m) witness_sleep(check, (m), __FILE__, __LINE__)
#define	WITNESS_SAVE_DECL(n)						\
	const char * __CONCAT(n, __wf);					\
	int __CONCAT(n, __wl)

#define	WITNESS_SAVE(m, n) 						\
do {									\
	if ((m)->mtx_witness != NULL) 					\
	    witness_save(m, &__CONCAT(n, __wf), &__CONCAT(n, __wl));	\
} while (0)

#define	WITNESS_RESTORE(m, n) 						\
do {									\
	if ((m)->mtx_witness != NULL)					\
	    witness_restore(m, __CONCAT(n, __wf), __CONCAT(n, __wl));	\
} while (0)

void	witness_init(struct mtx *, int flag);
void	witness_destroy(struct mtx *);
void	witness_enter(struct mtx *, int, const char *, int);
void	witness_try_enter(struct mtx *, int, const char *, int);
void	witness_exit(struct mtx *, int, const char *, int);
void	witness_display(void(*)(const char *fmt, ...));
void	witness_list(struct proc *);
int	witness_sleep(int, struct mtx *, const char *, int);
void	witness_save(struct mtx *, const char **, int *);
void	witness_restore(struct mtx *, const char *, int);
#else	/* WITNESS */
#define WITNESS_ENTER(m, t, f, l)
#define WITNESS_EXIT(m, t, f, l)
#define	WITNESS_SLEEP(check, m)
#define	WITNESS_SAVE_DECL(n)
#define	WITNESS_SAVE(m, n)
#define	WITNESS_RESTORE(m, n)

/*
 * flag++ is slezoid way of shutting up unused parameter warning
 * in mtx_init()
 */
#define witness_init(m, flag) flag++
#define witness_destroy(m)
#define witness_enter(m, t, f, l)
#define witness_try_enter(m, t, f, l)
#define witness_exit(m, t, f, l)
#endif	/* WITNESS */

/*
 * Assembly macros (for internal use only)
 *------------------------------------------------------------------------------
 */

#define	_V(x)	__STRING(x)

/*
 * Default, unoptimized mutex micro-operations
 */

#ifndef _obtain_lock
/* Actually obtain mtx_lock */
#define _obtain_lock(mp, tid)						\
	atomic_cmpset_acq_ptr(&(mp)->mtx_lock, (void *)MTX_UNOWNED, (tid))
#endif

#ifndef _release_lock
/* Actually release mtx_lock */
#define _release_lock(mp, tid)		       				\
	atomic_cmpset_rel_ptr(&(mp)->mtx_lock, (tid), (void *)MTX_UNOWNED)
#endif

#ifndef _release_lock_quick
/* Actually release mtx_lock quickly assuming that we own it */
#define	_release_lock_quick(mp) 					\
	atomic_store_rel_ptr(&(mp)->mtx_lock, (void *)MTX_UNOWNED)
#endif

#ifndef _getlock_sleep
/* Get a sleep lock, deal with recursion inline. */
#define	_getlock_sleep(mp, tid, type) do {				\
	if (!_obtain_lock(mp, tid)) {					\
		if (((mp)->mtx_lock & MTX_FLAGMASK) != ((uintptr_t)(tid)))\
			mtx_enter_hard(mp, (type) & MTX_HARDOPTS, 0);	\
		else {							\
			atomic_set_ptr(&(mp)->mtx_lock, MTX_RECURSE);	\
			(mp)->mtx_recurse++;				\
		}							\
	}								\
} while (0)
#endif

#ifndef _getlock_spin_block
/* Get a spin lock, handle recursion inline (as the less common case) */
#define	_getlock_spin_block(mp, tid, type) do {				\
	u_int _mtx_intr = save_intr();					\
	disable_intr();							\
	if (!_obtain_lock(mp, tid))					\
		mtx_enter_hard(mp, (type) & MTX_HARDOPTS, _mtx_intr);	\
	else								\
		(mp)->mtx_saveintr = _mtx_intr;				\
} while (0)
#endif

#ifndef _getlock_norecurse
/*
 * Get a lock without any recursion handling. Calls the hard enter function if
 * we can't get it inline.
 */
#define	_getlock_norecurse(mp, tid, type) do {				\
	if (!_obtain_lock(mp, tid))					\
		mtx_enter_hard((mp), (type) & MTX_HARDOPTS, 0);		\
} while (0)
#endif

#ifndef _exitlock_norecurse
/*
 * Release a sleep lock assuming we haven't recursed on it, recursion is handled
 * in the hard function.
 */
#define	_exitlock_norecurse(mp, tid, type) do {				\
	if (!_release_lock(mp, tid))					\
		mtx_exit_hard((mp), (type) & MTX_HARDOPTS);		\
} while (0)
#endif

#ifndef _exitlock
/*
 * Release a sleep lock when its likely we recursed (the code to
 * deal with simple recursion is inline).
 */
#define	_exitlock(mp, tid, type) do {					\
	if (!_release_lock(mp, tid)) {					\
		if ((mp)->mtx_lock & MTX_RECURSE) {			\
			if (--((mp)->mtx_recurse) == 0)			\
				atomic_clear_ptr(&(mp)->mtx_lock,	\
				    MTX_RECURSE);			\
		} else {						\
			mtx_exit_hard((mp), (type) & MTX_HARDOPTS);	\
		}							\
	}								\
} while (0)
#endif

#ifndef _exitlock_spin
/* Release a spin lock (with possible recursion). */
#define	_exitlock_spin(mp) do {						\
	if ((mp)->mtx_recurse == 0) {					\
		int _mtx_intr = (mp)->mtx_saveintr;			\
									\
		_release_lock_quick(mp);				\
		restore_intr(_mtx_intr);				\
	} else {							\
		(mp)->mtx_recurse--;					\
	}								\
} while (0)
#endif

/*
 * Externally visible mutex functions.
 *------------------------------------------------------------------------------
 */

/*
 * Return non-zero if a mutex is already owned by the current thread.
 */
#define	mtx_owned(m)    (((m)->mtx_lock & MTX_FLAGMASK) == (uintptr_t)CURTHD)

/* Common strings */
#ifdef _KERN_MUTEX_C_
#ifdef KTR_EXTEND

/*
 * KTR_EXTEND saves file name and line for all entries, so we don't need them
 * here.  Theoretically we should also change the entries which refer to them
 * (from CTR5 to CTR3), but since they're just passed to snprintf as the last
 * parameters, it doesn't do any harm to leave them.
 */
char	STR_mtx_enter_fmt[] = "GOT %s [%x] r=%d";
char	STR_mtx_exit_fmt[] = "REL %s [%x] r=%d";
char	STR_mtx_try_enter_fmt[] = "TRY_ENTER %s [%x] result=%d";
#else
char	STR_mtx_enter_fmt[] = "GOT %s [%x] at %s:%d r=%d";
char	STR_mtx_exit_fmt[] = "REL %s [%x] at %s:%d r=%d";
char	STR_mtx_try_enter_fmt[] = "TRY_ENTER %s [%x] at %s:%d result=%d";
#endif
char	STR_mtx_bad_type[] = "((type) & (MTX_NORECURSE | MTX_NOSWITCH)) == 0";
char	STR_mtx_owned[] = "mtx_owned(mpp)";
char	STR_mtx_recurse[] = "mpp->mtx_recurse == 0";
#else	/* _KERN_MUTEX_C_ */
extern	char STR_mtx_enter_fmt[];
extern	char STR_mtx_bad_type[];
extern	char STR_mtx_exit_fmt[];
extern	char STR_mtx_owned[];
extern	char STR_mtx_recurse[];
extern	char STR_mtx_try_enter_fmt[];
#endif	/* _KERN_MUTEX_C_ */

#ifndef KLD_MODULE
/*
 * Get lock 'm', the macro handles the easy (and most common cases) and leaves
 * the slow stuff to the mtx_enter_hard() function.
 *
 * Note: since type is usually a constant much of this code is optimized out.
 */
_MTX_INLINE void
_mtx_enter(struct mtx *mtxp, int type, const char *file, int line)
{
	struct mtx	*mpp = mtxp;

	/* bits only valid on mtx_exit() */
	MPASS2(((type) & (MTX_NORECURSE | MTX_NOSWITCH)) == 0,
	    STR_mtx_bad_type);

	if ((type) & MTX_SPIN) {
		/*
		 * Easy cases of spin locks:
		 *
		 * 1) We already own the lock and will simply recurse on it (if
		 *    RLIKELY)
		 *
		 * 2) The lock is free, we just get it
		 */
		if ((type) & MTX_RLIKELY) {
			/*
			 * Check for recursion, if we already have this
			 * lock we just bump the recursion count.
			 */
			if (mpp->mtx_lock == (uintptr_t)CURTHD) {
				mpp->mtx_recurse++;
				goto done;
			}
		}

		if (((type) & MTX_TOPHALF) == 0) {
			/*
			 * If an interrupt thread uses this we must block
			 * interrupts here.
			 */
			if ((type) & MTX_FIRST) {
				ASS_IEN;
				disable_intr();
				_getlock_norecurse(mpp, CURTHD,
				    (type) & MTX_HARDOPTS);
			} else {
				_getlock_spin_block(mpp, CURTHD,
				    (type) & MTX_HARDOPTS);
			}
		} else
			_getlock_norecurse(mpp, CURTHD, (type) & MTX_HARDOPTS);
	} else {
		/* Sleep locks */
		if ((type) & MTX_RLIKELY)
			_getlock_sleep(mpp, CURTHD, (type) & MTX_HARDOPTS);
		else
			_getlock_norecurse(mpp, CURTHD, (type) & MTX_HARDOPTS);
	}
	done:
	WITNESS_ENTER(mpp, type, file, line);
	CTR5(KTR_LOCK, STR_mtx_enter_fmt,
	    mpp->mtx_description, mpp, file, line,
	    mpp->mtx_recurse);
}

/*
 * Attempt to get MTX_DEF lock, return non-zero if lock acquired.
 *
 * XXX DOES NOT HANDLE RECURSION
 */
_MTX_INLINE int
_mtx_try_enter(struct mtx *mtxp, int type, const char *file, int line)
{
	struct mtx	*const mpp = mtxp;
	int	rval;

	rval = _obtain_lock(mpp, CURTHD);
#ifdef MUTEX_DEBUG
	if (rval && mpp->mtx_witness != NULL) {
		MPASS(mpp->mtx_recurse == 0);
		witness_try_enter(mpp, type, file, line);
	}
#endif
	CTR5(KTR_LOCK, STR_mtx_try_enter_fmt,
	    mpp->mtx_description, mpp, file, line, rval);

	return rval;
}

/*
 * Release lock m.
 */
_MTX_INLINE void
_mtx_exit(struct mtx *mtxp, int type, const char *file, int line)
{
	struct mtx	*const mpp = mtxp;

	MPASS2(mtx_owned(mpp), STR_mtx_owned);
	WITNESS_EXIT(mpp, type, file, line);
	CTR5(KTR_LOCK, STR_mtx_exit_fmt,
	    mpp->mtx_description, mpp, file, line,
	    mpp->mtx_recurse);
	if ((type) & MTX_SPIN) {
		if ((type) & MTX_NORECURSE) {
			int mtx_intr = mpp->mtx_saveintr;

			MPASS2(mpp->mtx_recurse == 0, STR_mtx_recurse);
			_release_lock_quick(mpp);
			if (((type) & MTX_TOPHALF) == 0) {
				if ((type) & MTX_FIRST) {
					ASS_IDIS;
					enable_intr();
				} else
					restore_intr(mtx_intr);
			}
		} else {
			if (((type & MTX_TOPHALF) == 0) &&
			    (type & MTX_FIRST)) {
				ASS_IDIS;
				ASS_SIEN(mpp);
			}
			_exitlock_spin(mpp);
		}
	} else {
		/* Handle sleep locks */
		if ((type) & MTX_RLIKELY)
			_exitlock(mpp, CURTHD, (type) & MTX_HARDOPTS);
		else {
			_exitlock_norecurse(mpp, CURTHD,
			    (type) & MTX_HARDOPTS);
		}
	}
}

#endif	/* KLD_MODULE */

/* Avoid namespace pollution */
#ifndef _KERN_MUTEX_C_
#undef	_obtain_lock
#undef	_release_lock
#undef	_release_lock_quick
#undef	_getlock_sleep
#undef	_getlock_spin_block
#undef	_getlock_norecurse
#undef	_exitlock_norecurse
#undef	_exitlock
#undef	_exitlock_spin
#endif	/* !_KERN_MUTEX_C_ */

#endif	/* _KERNEL */
#endif	/* !LOCORE */
#endif	/* _SYS_MUTEX_H_ */
