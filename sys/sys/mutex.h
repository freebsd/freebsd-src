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
#include <machine/atomic.h>
#include <machine/cpufunc.h>
#include <machine/globals.h>
#endif	/* _KERNEL_ */
#endif	/* !LOCORE */

#include <machine/mutex.h>

#ifdef _KERNEL

/*
 * Mutex flags
 *
 * Types
 */
#define	MTX_DEF		0x0		/* Default (spin/sleep) */
#define MTX_SPIN	0x1		/* Spin only lock */

/* Options */
#define MTX_RECURSE	0x2		/* Recursive lock (for mtx_init) */
#define	MTX_RLIKELY	0x4		/* Recursion likely */
#define	MTX_NORECURSE	0x8		/* No recursion possible */
#define	MTX_NOSPIN	0x10		/* Don't spin before sleeping */
#define	MTX_NOSWITCH	0x20		/* Do not switch on release */
#define	MTX_FIRST	0x40		/* First spin lock holder */
#define MTX_TOPHALF	0x80		/* Interrupts not disabled on spin */
#define	MTX_QUIET	0x100		/* Don't log a mutex event */

/* options that should be passed on to mtx_enter_hard, mtx_exit_hard */
#define	MTX_HARDOPTS	(MTX_SPIN | MTX_FIRST | MTX_TOPHALF | MTX_NOSWITCH)

/* Flags/value used in mtx_lock */
#define	MTX_RECURSED	0x01		/* (non-spin) lock held recursively */
#define	MTX_CONTESTED	0x02		/* (non-spin) lock contested */
#define	MTX_FLAGMASK	~(MTX_RECURSED | MTX_CONTESTED)
#define MTX_UNOWNED	0x8		/* Cookie for free mutex */

#endif	/* _KERNEL */

#ifndef LOCORE

struct mtx_debug;

/*
 * Sleep/spin mutex
 */
struct mtx {
	volatile uintptr_t mtx_lock;	/* lock owner/gate/flags */
	volatile u_int	mtx_recurse;	/* number of recursive holds */
	u_int		mtx_saveintr;	/* saved flags (for spin locks) */
	int		mtx_flags;	/* flags passed to mtx_init() */
	union {
		struct mtx_debug *mtxu_debug;
		const char	*mtxu_description;
	}		mtx_union;
	TAILQ_HEAD(, proc) mtx_blocked;
	LIST_ENTRY(mtx)	mtx_contested;
	struct mtx	*mtx_next;	/* all locks in system */
	struct mtx	*mtx_prev;
};

#define mp_fixme(string)

#ifdef _KERNEL
/* Prototypes */
void	mtx_init(struct mtx *m, const char *description, int flag);
void	mtx_destroy(struct mtx *m);

/*
 * Wrap the following functions with cpp macros so that filenames and line
 * numbers are embedded in the code correctly.
 */
void	_mtx_enter(struct mtx *mtxp, int type, const char *file, int line);
int	_mtx_try_enter(struct mtx *mtxp, int type, const char *file, int line);
void	_mtx_exit(struct mtx *mtxp, int type, const char *file, int line);

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

#define DROP_GIANT_NOSWITCH()						\
do {									\
	int _giantcnt;							\
	WITNESS_SAVE_DECL(Giant);					\
									\
	if (mtx_owned(&Giant))						\
		WITNESS_SAVE(&Giant, Giant);				\
	for (_giantcnt = 0; mtx_owned(&Giant); _giantcnt++)		\
		mtx_exit(&Giant, MTX_DEF | MTX_NOSWITCH)

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
#define MA_RECURSED	4
#define MA_NOTRECURSED	8
void	_mtx_assert(struct mtx *m, int what, const char *file, int line);
#define	mtx_assert(m, what) _mtx_assert((m), (what), __FILE__, __LINE__)
#else	/* INVARIANTS */
#define mtx_assert(m, what)
#endif	/* INVARIANTS */

#ifdef MUTEX_DEBUG
#define MPASS(ex)							\
	if (!(ex))							\
		panic("Assertion %s failed at %s:%d", #ex, __FILE__, __LINE__)
#define MPASS2(ex, what)						\
	if (!(ex))							\
		panic("Assertion %s failed at %s:%d", what, __FILE__, __LINE__)
#define MPASS3(ex, file, line)						\
	if (!(ex))							\
		panic("Assertion %s failed at %s:%d", #ex, file, line)
#define MPASS4(ex, what, file, line)					\
	if (!(ex))							\
		panic("Assertion %s failed at %s:%d", what, file, line)
#else	/* MUTEX_DEBUG */
#define	MPASS(ex)
#define	MPASS2(ex, what)
#define	MPASS3(ex, file, line)
#define	MPASS4(ex, what, file, line)
#endif	/* MUTEX_DEBUG */

/*
 * Externally visible mutex functions.
 *------------------------------------------------------------------------------
 */

/*
 * Return non-zero if a mutex is already owned by the current thread.
 */
#define	mtx_owned(m)	(((m)->mtx_lock & MTX_FLAGMASK) == (uintptr_t)CURTHD)

/*
 * Return non-zero if a mutex has been recursively acquired.
 */ 
#define mtx_recursed(m)	((m)->mtx_recurse != 0)

/* Common strings */
#ifdef _KERN_MUTEX_C_
char	STR_mtx_enter_fmt[] = "GOT %s [%p] r=%d at %s:%d";
char	STR_mtx_exit_fmt[] = "REL %s [%p] r=%d at %s:%d";
char	STR_mtx_try_enter_fmt[] = "TRY_ENTER %s [%p] result=%d at %s:%d";
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

#ifdef	WITNESS
void	witness_save(struct mtx *, const char **, int *);
void	witness_restore(struct mtx *, const char *, int);
void	witness_enter(struct mtx *, int, const char *, int);
void	witness_try_enter(struct mtx *, int, const char *, int);
void	witness_exit(struct mtx *, int, const char *, int);
int	witness_list(struct proc *);
int	witness_sleep(int, struct mtx *, const char *, int);

#define WITNESS_ENTER(m, t, f, l) witness_enter((m), (t), (f), (l))
#define WITNESS_EXIT(m, t, f, l) witness_exit((m), (t), (f), (l))
#define	WITNESS_SLEEP(check, m) witness_sleep(check, (m), __FILE__, __LINE__)
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
