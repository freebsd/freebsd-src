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
#include <sys/queue.h>

#ifdef _KERNEL
#include <sys/ktr.h>
#include <sys/proc.h>	/* Needed for curproc. */
#include <machine/atomic.h>
#include <machine/cpufunc.h>
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
#define	MTX_DEF		0x0		/* Default (spin/sleep) */
#define MTX_SPIN	0x1		/* Spin only lock */

/* Options */
#define	MTX_RLIKELY	0x4		/* (opt) Recursion likely */
#define	MTX_NORECURSE	0x8		/* No recursion possible */
#define	MTX_NOSPIN	0x10		/* Don't spin before sleeping */
#define	MTX_NOSWITCH	0x20		/* Do not switch on release */
#define	MTX_FIRST	0x40		/* First spin lock holder */
#define MTX_TOPHALF	0x80		/* Interrupts not disabled on spin */

/* options that should be passed on to mtx_enter_hard, mtx_exit_hard */
#define	MTX_HARDOPTS	(MTX_SPIN | MTX_FIRST | MTX_TOPHALF | MTX_NOSWITCH)

/* Flags/value used in mtx_lock */
#define	MTX_RECURSE	0x01		/* (non-spin) lock held recursively */
#define	MTX_CONTESTED	0x02		/* (non-spin) lock contested */
#define	MTX_FLAGMASK	~(MTX_RECURSE | MTX_CONTESTED)
#define MTX_UNOWNED	0x8		/* Cookie for free mutex */

#endif	/* _KERNEL */

/*
 * Sleep/spin mutex
 */
struct mtx {
	volatile u_int	mtx_lock;	/* lock owner/gate/flags */
	volatile u_int	mtx_recurse;	/* number of recursive holds */
	u_int		mtx_savefl;	/* saved flags (for spin locks) */
	char		*mtx_description;
	TAILQ_HEAD(, proc) mtx_blocked;
	LIST_ENTRY(mtx)	mtx_contested;
	struct mtx	*mtx_next;	/* all locks in system */
	struct mtx	*mtx_prev;
#ifdef SMP_DEBUG
	/* If you add anything here, adjust the mtxf_t definition below */
	struct witness	*mtx_witness;
	LIST_ENTRY(mtx)	mtx_held;
	const char	*mtx_file;
	int		mtx_line;
#endif	/* SMP_DEBUG */
};

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
}	mtxf_t;

#define mp_fixme(string)

#ifdef _KERNEL
/* Misc */
#define CURTHD	((u_int)CURPROC)	/* Current thread ID */

/* Prototypes */
void	mtx_init(struct mtx *m, char *description, int flag);
void	mtx_enter_hard(struct mtx *, int type, int flags);
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
extern struct mtx	clock_lock;

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
	WITNESS_SAVE(&Giant, Giant);					\
	for (_giantcnt = 0; mtx_owned(&Giant); _giantcnt++)		\
		mtx_exit(&Giant, MTX_DEF)

#define PICKUP_GIANT()							\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_enter(&Giant, MTX_DEF);				\
	WITNESS_RESTORE(&Giant, Giant);					\
} while (0)

#define PARTIAL_PICKUP_GIANT()						\
	mtx_assert(&Giant, MA_NOTOWNED);				\
	while (_giantcnt--)						\
		mtx_enter(&Giant, MTX_DEF);				\
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
#define MPASS(ex) if (!(ex)) panic("Assertion %s failed at %s:%d",	\
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
#define	ASS_IEN		MPASS2(read_eflags() & 0x200, STR_IEN)
#define	ASS_IDIS	MPASS2((read_eflags() & 0x200) == 0, STR_IDIS)
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

#ifndef I386_CPU

/*
 * For 486 and newer processors.
 */

/* Get a sleep lock, deal with recursion inline. */
#define	_getlock_sleep(mtxp, tid, type) ({				\
	int	_res;							\
									\
	__asm __volatile (						\
"	movl	$" _V(MTX_UNOWNED) ",%%eax;"	/* Unowned cookie */	\
"	" MPLOCKED ""							\
"	cmpxchgl %3,%1;"			/* Try */		\
"	jz	1f;"				/* Got it */		\
"	andl	$" _V(MTX_FLAGMASK) ",%%eax;"	/* turn off spec bits */ \
"	cmpl	%%eax,%3;"			/* already have it? */	\
"	je	2f;"				/* yes, recurse */	\
"	pushl	%4;"							\
"	pushl	%5;"							\
"	call	mtx_enter_hard;"					\
"	addl	$8,%%esp;"						\
"	jmp	1f;"							\
"2:	lock; orl $" _V(MTX_RECURSE) ",%1;"				\
"	incl	%2;"							\
"1:"									\
"# getlock_sleep"							\
	: "=&a" (_res),				/* 0 (dummy output) */	\
	  "+m" (mtxp->mtx_lock), 		/* 1 */			\
	  "+m" (mtxp->mtx_recurse) 		/* 2 */			\
	: "r" (tid),				/* 3 (input) */		\
	  "gi" (type),				/* 4 */			\
	  "g" (mtxp)				/* 5 */			\
	: "memory", "ecx", "edx"		/* used */ );		\
})

/* Get a spin lock, handle recursion inline (as the less common case) */
#define	_getlock_spin_block(mtxp, tid, type) ({				\
	int	_res;							\
									\
	__asm __volatile (						\
"	pushfl;"							\
"	cli;"								\
"	movl	$" _V(MTX_UNOWNED) ",%%eax;"	/* Unowned cookie */	\
"	" MPLOCKED ""							\
"	cmpxchgl %3,%1;"			/* Try */		\
"	jz	2f;"				/* got it */		\
"	pushl	%4;"							\
"	pushl	%5;"							\
"	call	mtx_enter_hard;" /* mtx_enter_hard(mtxp, type, oflags) */ \
"	addl	$0xc,%%esp;"						\
"	jmp	1f;"							\
"2:	popl	%2;"				/* save flags */	\
"1:"									\
"# getlock_spin_block"							\
	: "=&a" (_res),				/* 0 (dummy output) */	\
	  "+m" (mtxp->mtx_lock),		/* 1 */			\
	  "=m" (mtxp->mtx_savefl)		/* 2 */			\
	: "r" (tid),				/* 3 (input) */		\
	  "gi" (type),				/* 4 */			\
	  "g" (mtxp)				/* 5 */			\
	: "memory", "ecx", "edx"		/* used */ );		\
})

/*
 * Get a lock without any recursion handling. Calls the hard enter function if
 * we can't get it inline.
 */
#define	_getlock_norecurse(mtxp, tid, type) ({				\
	int	_res;							\
									\
	__asm __volatile (						\
"	movl	$" _V(MTX_UNOWNED) ",%%eax;"	/* Unowned cookie */	\
"	" MPLOCKED ""							\
"	cmpxchgl %2,%1;"			/* Try */		\
"	jz	1f;"				/* got it */		\
"	pushl	%3;"							\
"	pushl	%4;"							\
"	call	mtx_enter_hard;" /* mtx_enter_hard(mtxp, type) */	\
"	addl	$8,%%esp;"						\
"1:"									\
"# getlock_norecurse"							\
	: "=&a" (_res),				/* 0 (dummy output) */	\
	  "+m" (mtxp->mtx_lock)			/* 1 */			\
	: "r" (tid),				/* 2 (input) */		\
	  "gi" (type),				/* 3 */			\
	  "g" (mtxp)				/* 4 */			\
	: "memory", "ecx", "edx"		/* used */ );		\
})

/*
 * Release a sleep lock assuming we haven't recursed on it, recursion is handled
 * in the hard function.
 */
#define	_exitlock_norecurse(mtxp, tid, type) ({				\
	int	_tid = (int)(tid);					\
									\
	__asm __volatile (						\
"	" MPLOCKED ""							\
"	cmpxchgl %4,%0;"			/* try easy rel */	\
"	jz	1f;"				/* released! */		\
"	pushl	%2;"							\
"	pushl	%3;"							\
"	call	mtx_exit_hard;"						\
"	addl	$8,%%esp;"						\
"1:"									\
"# exitlock_norecurse"							\
	: "+m" (mtxp->mtx_lock), 		/* 0 */			\
	  "+a" (_tid)				/* 1 */			\
	: "gi" (type),				/* 2 (input) */		\
	  "g" (mtxp),				/* 3 */			\
	  "r" (MTX_UNOWNED)			/* 4 */			\
	: "memory", "ecx", "edx"		/* used */ );		\
})

/*
 * Release a sleep lock when its likely we recursed (the code to
 * deal with simple recursion is inline).
 */
#define	_exitlock(mtxp, tid, type) ({					\
	int	_tid = (int)(tid);					\
									\
	__asm __volatile (						\
"	" MPLOCKED ""							\
"	cmpxchgl %5,%0;"			/* try easy rel */	\
"	jz	1f;"				/* released! */		\
"	testl	$" _V(MTX_RECURSE) ",%%eax;"	/* recursed? */		\
"	jnz	3f;"				/* handle recursion */	\
	/* Lock not recursed and contested: do the hard way */		\
"	pushl	%3;"							\
"	pushl	%4;"							\
"	call	mtx_exit_hard;"			/* mtx_exit_hard(mtxp,type) */ \
"	addl	$8,%%esp;"						\
"	jmp	1f;"							\
	/* lock recursed, lower recursion level */			\
"3:	decl	%1;"				/* one less level */	\
"	jnz	1f;"				/* still recursed, done */ \
"	lock; andl $~" _V(MTX_RECURSE) ",%0;"	/* turn off recurse flag */ \
"1:"									\
"# exitlock"								\
	: "+m" (mtxp->mtx_lock),		/* 0 */			\
	  "+m" (mtxp->mtx_recurse), 		/* 1 */ 		\
	  "+a" (_tid)				/* 2 */			\
	: "gi" (type),				/* 3 (input) */		\
	  "g" (mtxp),				/* 4 */			\
	  "r" (MTX_UNOWNED)			/* 5 */			\
	: "memory", "ecx", "edx"		/* used */ );		\
})

/*
 * Release a spin lock (with possible recursion).
 *
 * We use cmpxchgl to clear lock (instead of simple store) to flush posting
 * buffers and make the change visible to other CPU's.
 */
#define	_exitlock_spin(mtxp, inten1, inten2) ({				\
	int	_res;							\
									\
	__asm __volatile (						\
"	movl	%1,%%eax;"						\
"	decl	%%eax;"							\
"	js	1f;"							\
"	movl	%%eax,%1;"						\
"	jmp	2f;"							\
"1:	movl	%0,%%eax;"						\
"	movl	$ " _V(MTX_UNOWNED) ",%%ecx;"				\
"	" inten1 ";"							\
"	" MPLOCKED ""							\
"	cmpxchgl %%ecx,%0;"				  		\
"	" inten2 ";"							\
"2:"									\
"# exitlock_spin"							\
	: "+m" (mtxp->mtx_lock),	/* 0 */				\
	  "+m" (mtxp->mtx_recurse),	/* 1 */ 			\
	  "=&a" (_res)			/* 2 */				\
	: "g"  (mtxp->mtx_savefl)	/* 3 (used in 'inten') */	\
	: "memory", "ecx"		/* used */ );			\
})

#else	/* I386_CPU */

/*
 * For 386 processors only.
 */

/* Get a sleep lock, deal with recursion inline. */
#define	_getlock_sleep(mp, tid, type) do {				\
	if (atomic_cmpset_int(&(mp)->mtx_lock, MTX_UNOWNED, (tid)) == 0) { \
		if (((mp)->mtx_lock & MTX_FLAGMASK) != (tid))		\
			mtx_enter_hard(mp, (type) & MTX_HARDOPTS, 0);	\
		else {							\
			atomic_set_int(&(mp)->mtx_lock, MTX_RECURSE);	\
			(mp)->mtx_recurse++;				\
		}							\
	}								\
} while (0)

/* Get a spin lock, handle recursion inline (as the less common case) */
#define	_getlock_spin_block(mp, tid, type) do {				\
	u_int _mtx_fl = read_eflags();					\
	disable_intr();							\
	if (atomic_cmpset_int(&(mp)->mtx_lock, MTX_UNOWNED, (tid)) == 0) \
		mtx_enter_hard(mp, (type) & MTX_HARDOPTS, _mtx_fl);	\
	else								\
		(mp)->mtx_savefl = _mtx_fl;				\
} while (0)

/*
 * Get a lock without any recursion handling. Calls the hard enter function if
 * we can't get it inline.
 */
#define	_getlock_norecurse(mp, tid, type) do {				\
	if (atomic_cmpset_int(&(mp)->mtx_lock, MTX_UNOWNED, (tid)) == 0) \
		mtx_enter_hard((mp), (type) & MTX_HARDOPTS, 0);		\
} while (0)

/*
 * Release a sleep lock assuming we haven't recursed on it, recursion is handled
 * in the hard function.
 */
#define	_exitlock_norecurse(mp, tid, type) do {				\
	if (atomic_cmpset_int(&(mp)->mtx_lock, (tid), MTX_UNOWNED) == 0) \
		mtx_exit_hard((mp), (type) & MTX_HARDOPTS);		\
} while (0)

/*
 * Release a sleep lock when its likely we recursed (the code to
 * deal with simple recursion is inline).
 */
#define	_exitlock(mp, tid, type) do {					\
	if (atomic_cmpset_int(&(mp)->mtx_lock, (tid), MTX_UNOWNED) == 0) { \
		if ((mp)->mtx_lock & MTX_RECURSE) {			\
			if (--((mp)->mtx_recurse) == 0)			\
				atomic_clear_int(&(mp)->mtx_lock,	\
				    MTX_RECURSE);			\
		} else {						\
			mtx_exit_hard((mp), (type) & MTX_HARDOPTS);	\
		}							\
	}								\
} while (0)

/* Release a spin lock (with possible recursion). */
#define	_exitlock_spin(mp, inten1, inten2) do {				\
	if ((mp)->mtx_recurse == 0) {					\
		atomic_cmpset_int(&(mp)->mtx_lock, (mp)->mtx_lock,	\
		    MTX_UNOWNED);					\
		write_eflags((mp)->mtx_savefl);				\
	} else {							\
		(mp)->mtx_recurse--;					\
	}								\
} while (0)

#endif	/* I386_CPU */

/*
 * Externally visible mutex functions.
 *------------------------------------------------------------------------------
 */

/*
 * Return non-zero if a mutex is already owned by the current thread.
 */
#define	mtx_owned(m)    (((m)->mtx_lock & MTX_FLAGMASK) == CURTHD)

/* Common strings */
#ifdef MTX_STRS
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

	rval = atomic_cmpset_int(&mpp->mtx_lock, MTX_UNOWNED, CURTHD);
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

#define	mtx_legal2block()	(read_eflags() & 0x200)

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
			MPASS2(mpp->mtx_recurse == 0, STR_mtx_recurse);
			atomic_cmpset_int(&mpp->mtx_lock, mpp->mtx_lock,
			    MTX_UNOWNED);
			if (((type) & MTX_TOPHALF) == 0) {
				if ((type) & MTX_FIRST) {
					ASS_IDIS;
					enable_intr();
				} else
					write_eflags(mpp->mtx_savefl);
			}
		} else {
			if ((type) & MTX_TOPHALF)
				_exitlock_spin(mpp,,);
			else {
				if ((type) & MTX_FIRST) {
					ASS_IDIS;
					_exitlock_spin(mpp,, "sti");
				} else {
					_exitlock_spin(mpp,
					    "pushl %3", "popfl");
				}
			}
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
#endif	/* _KERNEL */

#else	/* !LOCORE */

/*
 * Simple assembly macros to get and release non-recursive spin locks
 */

#if defined(I386_CPU)

#define	MTX_EXIT(lck, reg)						\
	pushl	lck+MTX_SAVEFL;						\
	movl	$ MTX_UNOWNED,lck+MTX_LOCK;				\
	popf

#else	/* I386_CPU */

#define MTX_ENTER(reg, lck)						\
	pushf								\
	cli								\
9:	movl	$ MTX_UNOWNED,%eax;					\
	MPLOCKED							\
	cmpxchgl reg,lck+MTX_LOCK;					\
	jnz	9b;							\
	popl	lck+MTX_SAVEFL;

/* Must use locked bus op (cmpxchg) when setting to unowned (barrier) */
#define	MTX_EXIT(lck,reg)						\
	pushl	lck+MTX_SAVEFL;						\
	movl	lck+MTX_LOCK,%eax;					\
	movl	$ MTX_UNOWNED,reg;					\
	MPLOCKED							\
	cmpxchgl reg,lck+MTX_LOCK;					\
	popf

#define MTX_ENTER_WITH_RECURSION(reg, lck)				\
	pushf								\
	cli								\
	movl	lck+MTX_LOCK,%eax;					\
	cmpl	_curproc,%eax;						\
	jne	9f;							\
	incw	lck+MTX_RECURS;						\
	jmp	8f;							\
9:	movl	$ MTX_UNOWNED,%eax;					\
	MPLOCKED							\
	cmpxchgl reg,lck+MTX_LOCK;      				\
	jnz	9b;							\
	popl	lck+MTX_SAVEFL;						\
	jmp	10f;							\
8:	add	$4,%esp;						\
10:

#define	MTX_EXIT_WITH_RECURSION(lck,reg)				\
	movl	lck+MTX_RECURSE,%eax;					\
	decl	%eax;							\
	js	9f;							\
	movl	%eax,lck+MTX_RECURSE;					\
	jmp	8f;							\
	pushl	lck+MTX_SAVEFL;						\
9:	movl	lck+MTX_LOCK,%eax;					\
	movl	$ MTX_UNOWNED,reg;					\
	MPLOCKED							\
	cmpxchgl reg,lck+MTX_LOCK;					\
	popf								\
8:

#endif	/* I386_CPU */
#endif	/* !LOCORE */
#endif	/* __MACHINE_MUTEX_H */
