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

#ifndef _MACHINE_MUTEX_H_
#define _MACHINE_MUTEX_H_

#ifndef LOCORE

#include <sys/ktr.h>
#include <sys/queue.h>
#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/globaldata.h>
#include <machine/globals.h>

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
#define	MTX_DEF		0x1		/* Default (spin/sleep) */
#define MTX_SPIN	0x2		/* Spin only lock */

/* Options */
#define	MTX_RLIKELY	0x4		/* (opt) Recursion likely */
#define	MTX_NORECURSE	0x8		/* No recursion possible */
#define	MTX_NOSPIN	0x10		/* Don't spin before sleeping */
#define	MTX_NOSWITCH	0x20		/* Do not switch on release */
#define	MTX_FIRST	0x40		/* First spin lock holder */
#define MTX_TOPHALF	0x80		/* Interrupts not disabled on spin */

/* options that should be passed on to mtx_enter_hard, mtx_exit_hard */
#define	MTX_HARDOPTS	(MTX_DEF | MTX_SPIN | MTX_FIRST | MTX_TOPHALF | MTX_NOSWITCH)

/* Flags/value used in mtx_lock */
#define	MTX_RECURSE	0x01		/* (non-spin) lock held recursively */
#define	MTX_CONTESTED	0x02		/* (non-spin) lock contested */
#define	MTX_FLAGMASK	~(MTX_RECURSE | MTX_CONTESTED)
#define MTX_UNOWNED	0x8		/* Cookie for free mutex */

struct proc;	/* XXX */

/*
 * Sleep/spin mutex
 */
struct mtx {
	volatile u_int64_t mtx_lock;	/* lock owner/gate/flags */
	volatile u_int32_t mtx_recurse;	/* number of recursive holds */
	u_int32_t	mtx_saveipl;	/* saved ipl (for spin locks) */
	char		*mtx_description;
	TAILQ_HEAD(, proc) mtx_blocked;
	LIST_ENTRY(mtx) mtx_contested;
	struct mtx	*mtx_next;	/* all locks in system */
	struct mtx	*mtx_prev;
#ifdef SMP_DEBUG
	/* If you add anything here, adjust the mtxf_t definition below */
	struct witness	*mtx_witness;
	LIST_ENTRY(mtx)	mtx_held;
	const char	*mtx_file;
	int		 mtx_line;
#endif /* SMP_DEBUG */
};

typedef struct mtx mtx_t;

/*
 * Filler for structs which need to remain the same size
 * whether or not SMP_DEBUG is turned on.
 */
typedef struct mtxf {
#ifdef SMP_DEBUG
	char	mtxf_data[0];
#else
	char	mtxf_data[4*sizeof(void *) + sizeof(int)];
#endif
} mtxf_t;

#define mp_fixme(string)

#ifdef _KERNEL
/* Misc */
#define CURTHD	((u_int64_t)CURPROC)	/* Current thread ID */

/* Prototypes */
void	mtx_init(mtx_t *m, char *description, int flag);
void	mtx_enter_hard(mtx_t *, int type, int ipl);
void	mtx_exit_hard(mtx_t *, int type);
void	mtx_destroy(mtx_t *m);

/*
 * Wrap the following functions with cpp macros so that filenames and line
 * numbers are embedded in the code correctly.
 */
#if (defined(KLD_MODULE) || defined(_KERN_MUTEX_C_))
void	_mtx_enter(mtx_t *mtxp, int type, const char *file, int line);
int	_mtx_try_enter(mtx_t *mtxp, int type, const char *file, int line);
void	_mtx_exit(mtx_t *mtxp, int type, const char *file, int line);
#endif

#define	mtx_enter(mtxp, type)						\
	_mtx_enter((mtxp), (type), __FILE__, __LINE__)

#define	mtx_try_enter(mtxp, type)					\
	_mtx_try_enter((mtxp), (type), __FILE__, __LINE__)

#define	mtx_exit(mtxp, type)						\
	_mtx_exit((mtxp), (type), __FILE__, __LINE__)

/* Global locks */
extern mtx_t sched_lock;
extern mtx_t Giant;

/*
 * Used to replace return with an exit Giant and return.
 */

#define EGAR(a)				\
do {					\
	mtx_exit(&Giant, MTX_DEF);	\
	return (a);			\
} while (0)

#define VEGAR				\
do {					\
	mtx_exit(&Giant, MTX_DEF);	\
	return;				\
} while (0)

#define DROP_GIANT()						\
do {								\
	int _giantcnt;						\
	WITNESS_SAVE_DECL(Giant);				\
								\
	WITNESS_SAVE(&Giant, Giant);				\
	for (_giantcnt = 0; mtx_owned(&Giant); _giantcnt++)	\
		mtx_exit(&Giant, MTX_DEF)

#define PICKUP_GIANT()						\
	mtx_assert(&Giant, MA_NOTOWNED);			\
	while (_giantcnt--)					\
		mtx_enter(&Giant, MTX_DEF);			\
	WITNESS_RESTORE(&Giant, Giant);				\
} while (0)

#define PARTIAL_PICKUP_GIANT()					\
	mtx_assert(&Giant, MA_NOTOWNED);			\
	while (_giantcnt--)					\
		mtx_enter(&Giant, MTX_DEF);			\
	WITNESS_RESTORE(&Giant, Giant)


/*
 * Debugging
 */
#ifndef SMP_DEBUG
#define mtx_assert(m, what)
#else	/* SMP_DEBUG */

#define MA_OWNED	1
#define MA_NOTOWNED	2
#define mtx_assert(m, what) {						\
	switch ((what)) {						\
	case MA_OWNED:							\
		ASS(mtx_owned((m)));					\
		break;							\
	case MA_NOTOWNED:						\
		ASS(!mtx_owned((m)));					\
		break;							\
	default:							\
		panic("unknown mtx_assert at %s:%d", __FILE__, __LINE__); \
	}								\
}

#ifdef INVARIANTS
#define	ASS(ex) MPASS(ex)
#define MPASS(ex) if (!(ex)) panic("Assertion %s failed at %s:%d", \
                #ex, __FILE__, __LINE__)
#define MPASS2(ex, what) if (!(ex)) panic("Assertion %s failed at %s:%d", \
                what, __FILE__, __LINE__)

#ifdef MTX_STRS
char STR_IEN[] = "fl & 0x200";
char STR_IDIS[] = "!(fl & 0x200)";
#else	/* MTX_STRS */
extern char STR_IEN[];
extern char STR_IDIS[];
#endif	/* MTX_STRS */
#define	ASS_IEN		MPASS2((alpha_pal_rdps() & ALPHA_PSL_IPL_MASK)	\
			       == ALPHA_PSL_IPL_HIGH, STR_IEN)
#define	ASS_IDIS	MPASS2((alpha_pal_rdps() & ALPHA_PSL_IPL_MASK)	\
			       != ALPHA_PSL_IPL_HIGH, STR_IDIS)
#endif	/* INVARIANTS */

#endif	/* SMP_DEBUG */

#if !defined(SMP_DEBUG) || !defined(INVARIANTS)
#define ASS(ex)
#define	MPASS(ex)
#define	MPASS2(ex, where)
#define	ASS_IEN
#define ASS_IDIS
#endif	/* !defined(SMP_DEBUG) || !defined(INVARIANTS) */

#ifdef	WITNESS
#ifndef	SMP_DEBUG
#error	WITNESS requires SMP_DEBUG
#endif	/* SMP_DEBUG */
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

void	witness_init(mtx_t *, int flag);
void	witness_destroy(mtx_t *);
void	witness_enter(mtx_t *, int, const char *, int);
void	witness_try_enter(mtx_t *, int, const char *, int);
void	witness_exit(mtx_t *, int, const char *, int);
void	witness_display(void(*)(const char *fmt, ...));
void	witness_list(struct proc *);
int	witness_sleep(int, mtx_t *, const char *, int);
void	witness_save(mtx_t *, const char **, int *);
void	witness_restore(mtx_t *, const char *, int);
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
 *--------------------------------------------------------------------------
 */

/*
 * Get a sleep lock, deal with recursion inline
 */

#define	_V(x)	__STRING(x)

#define	_getlock_sleep(mp, tid, type) do {				     \
	if (atomic_cmpset_64(&(mp)->mtx_lock, MTX_UNOWNED, (tid)) == 0) {    \
		if (((mp)->mtx_lock & MTX_FLAGMASK) != (tid))		     \
			mtx_enter_hard(mp, (type) & MTX_HARDOPTS, 0);	     \
		else {							     \
			if (((mp)->mtx_lock & MTX_RECURSE) == 0)	     \
				atomic_set_64(&(mp)->mtx_lock, MTX_RECURSE); \
			(mp)->mtx_recurse++;				     \
		}							     \
	} else {							     \
		alpha_mb();						     \
	}								     \
} while (0)

/*
 * Get a spin lock, handle recusion inline (as the less common case)
 */

#define	_getlock_spin_block(mp, tid, type) do {				\
	u_int _ipl = alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);		\
	if (atomic_cmpset_64(&(mp)->mtx_lock, MTX_UNOWNED, (tid)) == 0) \
		mtx_enter_hard(mp, (type) & MTX_HARDOPTS, _ipl);	\
	else {								\
		alpha_mb();						\
		(mp)->mtx_saveipl = _ipl;				\
	}								\
} while (0)

/*
 * Get a lock without any recursion handling. Calls the hard enter
 * function if we can't get it inline.
 */

#define	_getlock_norecurse(mp, tid, type) do {				\
	if (atomic_cmpset_64(&(mp)->mtx_lock, MTX_UNOWNED, (tid)) == 0)	\
		mtx_enter_hard((mp), (type) & MTX_HARDOPTS, 0);		\
	else								\
		alpha_mb();						\
} while (0)

/*
 * Release a sleep lock assuming we haven't recursed on it, recursion is
 * handled in the hard function.
 */

#define	_exitlock_norecurse(mp, tid, type) do {				\
	alpha_mb();							\
	if (atomic_cmpset_64(&(mp)->mtx_lock, (tid), MTX_UNOWNED) == 0)	\
		mtx_exit_hard((mp), (type) & MTX_HARDOPTS);		\
} while (0)

/*
 * Release a sleep lock when its likely we recursed (the code to
 * deal with simple recursion is inline).
 */

#define	_exitlock(mp, tid, type) do {					\
	alpha_mb();							\
	if (atomic_cmpset_64(&(mp)->mtx_lock, (tid), MTX_UNOWNED) == 0) {\
		if (((mp)->mtx_lock & MTX_RECURSE) &&			\
		    (--(mp)->mtx_recurse == 0))				\
			atomic_clear_64(&(mp)->mtx_lock, MTX_RECURSE);	\
		else							\
			mtx_exit_hard((mp), (type) & MTX_HARDOPTS);	\
	}								\
} while (0)

/*
 * Release a spin lock (with possible recursion)
 */

#define	_exitlock_spin(mp) do {						\
	int _ipl = (mp)->mtx_saveipl;					\
	alpha_mb();							\
	if ((mp)->mtx_recurse == 0 || (--(mp)->mtx_recurse) == 0)	\
		atomic_cmpset_64(&(mp)->mtx_lock, (mp)->mtx_lock,	\
		    MTX_UNOWNED);					\
	alpha_pal_swpipl(_ipl);						\
} while (0)

/*
 * Externally visible mutex functions
 *------------------------------------------------------------------------
 */

/*
 * Return non-zero if a mutex is already owned by the current thread
 */
#define	mtx_owned(m)    (((m)->mtx_lock & MTX_FLAGMASK) == CURTHD)

/* Common strings */
#ifdef MTX_STRS
char	STR_mtx_enter_fmt[] = "GOT %s [%p] at %s:%d r=%d";
char	STR_mtx_bad_type[] = "((type) & (MTX_NORECURSE | MTX_NOSWITCH)) == 0";
char	STR_mtx_exit_fmt[] = "REL %s [%p] at %s:%d r=%d";
char	STR_mtx_owned[] = "mtx_owned(mpp)";
char	STR_mtx_recurse[] = "mpp->mtx_recurse == 0";
char	STR_mtx_try_enter_fmt[] = "TRY_ENTER %s [%p] at %s:%d result=%d";
#else	/* MTX_STRS */
extern	char STR_mtx_enter_fmt[];
extern	char STR_mtx_bad_type[];
extern	char STR_mtx_exit_fmt[];
extern	char STR_mtx_owned[];
extern	char STR_mtx_recurse[];
extern	char STR_mtx_try_enter_fmt[];
#endif	/* MTX_STRS */

#ifndef KLD_MODULE
/*
 * Get lock 'm', the macro handles the easy (and most common cases) and
 * leaves the slow stuff to the mtx_enter_hard() function.
 *
 * Note: since type is usually a constant much of this code is optimized out
 */
_MTX_INLINE void
_mtx_enter(mtx_t *mtxp, int type, const char *file, int line)
{
	mtx_t	*mpp = mtxp;

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
			 * Check for recursion, if we already have this lock we
			 * just bump the recursion count.
			 */
			if (mpp->mtx_lock == CURTHD) {
				mpp->mtx_recurse++;
				goto done;
			}
		}

		if (((type) & MTX_TOPHALF) == 0) {
			/*
			 * If an interrupt thread uses this we must block
			 * interrupts here.
			 */
			_getlock_spin_block(mpp, CURTHD, (type) & MTX_HARDOPTS);
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
 * Attempt to get MTX_DEF lock, return non-zero if lock acquired
 *
 * XXX DOES NOT HANDLE RECURSION
 */
_MTX_INLINE int
_mtx_try_enter(mtx_t *mtxp, int type, const char *file, int line)
{
	mtx_t	*const mpp = mtxp;
	int	rval;

	rval = atomic_cmpset_64(&mpp->mtx_lock, MTX_UNOWNED, CURTHD);
#ifdef SMP_DEBUG
	if (rval && mpp->mtx_witness != NULL) {
		ASS(mpp->mtx_recurse == 0);
		witness_try_enter(mpp, type, file, line);
	}
#endif
	CTR5(KTR_LOCK, STR_mtx_try_enter_fmt,
	    mpp->mtx_description, mpp, file, line, rval);

	return rval;
}

/*
 * Release lock m
 */
_MTX_INLINE void
_mtx_exit(mtx_t *mtxp, int type, const char *file, int line)
{
	mtx_t	*const mpp = mtxp;

	MPASS2(mtx_owned(mpp), STR_mtx_owned);
	WITNESS_EXIT(mpp, type, file, line);
	CTR5(KTR_LOCK, STR_mtx_exit_fmt,
	    mpp->mtx_description, mpp, file, line,
	    mpp->mtx_recurse);
	if ((type) & MTX_SPIN) {
		if ((type) & MTX_NORECURSE) {
			MPASS2(mpp->mtx_recurse == 0, STR_mtx_recurse);
			atomic_cmpset_64(&mpp->mtx_lock, mpp->mtx_lock,
			    MTX_UNOWNED);
			if (((type) & MTX_TOPHALF) == 0)
				alpha_pal_swpipl(mpp->mtx_saveipl);
		} else
			if ((type) & MTX_TOPHALF) {
				_exitlock_norecurse(mpp, CURTHD,
				    (type) & MTX_HARDOPTS);
			} else
				_exitlock_spin(mpp);
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
#endif	/* _KERNEL */

#else	/* !LOCORE */

/*
 * Simple assembly macros to get and release non-recursive spin locks
 */
#define MTX_ENTER(lck)				\
	ldiq	a0, ALPHA_PSL_IPL_HIGH;		\
	call_pal PAL_OSF1_swpipl;		\
1:	ldq_l	a0, lck+MTX_LOCK;		\
	cmpeq	a0, MTX_UNOWNED, a1;		\
	beq	a1, 1b;				\
	ldq	a0, PC_CURPROC(globalp);	\
	stq_c	a0, lck+MTX_LOCK;		\
	beq	a0, 1b;				\
	mb;					\
	stl	v0, lck+MTX_SAVEIPL

#define MTX_EXIT(lck)				\
	mb;					\
	ldiq	a0, MTX_UNOWNED;		\
	stq	a0, lck+MTX_LOCK;		\
	ldl	a0, lck+MTX_SAVEIPL;		\
	call_pal PAL_OSF1_swpipl

#endif	/* !LOCORE */

#endif	/* __MACHINE_MUTEX_H */
