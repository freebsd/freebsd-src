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

#ifdef _KERNEL

/* Global locks */
extern struct mtx	clock_lock;

/*
 * Assembly macros (for internal use only)
 *------------------------------------------------------------------------------
 */
#define	_V(x)	__STRING(x)

#if 0
/* #ifndef I386_CPU */

/*
 * For 486 and newer processors.
 */

/* Get a sleep lock, deal with recursion inline. */
#define	_getlock_sleep(mtxp, tid, type) ({				\
	int	_res = MTX_UNOWNED;					\
									\
	__asm __volatile (						\
"	" MPLOCKED ""							\
"	cmpxchgl %3,%1;"			/* Try */		\
"	jz	1f;"				/* Got it */		\
"	andl	$" _V(MTX_FLAGMASK) ",%0;"	/* turn off spec bits */ \
"	cmpl	%0,%3;"				/* already have it? */	\
"	je	2f;"				/* yes, recurse */	\
"	pushl	%4;"							\
"	pushl	%5;"							\
"	call	mtx_enter_hard;"					\
"	addl	$8,%%esp;"						\
"	jmp	1f;"							\
"2:"									\
"	" MPLOCKED ""							\
"	orl $" _V(MTX_RECURSE) ",%1;"					\
"	incl	%2;"							\
"1:"									\
"# getlock_sleep"							\
	: "+a" (_res),				/* 0 */			\
	  "+m" (mtxp->mtx_lock), 		/* 1 */			\
	  "+m" (mtxp->mtx_recurse) 		/* 2 */			\
	: "r" (tid),				/* 3 (input) */		\
	  "gi" (type),				/* 4 */			\
	  "g" (mtxp)				/* 5 */			\
	: "cc", "memory", "ecx", "edx"		/* used */ );		\
})

/* Get a spin lock, handle recursion inline (as the less common case) */
#define	_getlock_spin_block(mtxp, tid, type) ({				\
	int	_res = MTX_UNOWNED;					\
									\
	__asm __volatile (						\
"	pushfl;"							\
"	cli;"								\
"	" MPLOCKED ""							\
"	cmpxchgl %3,%1;"			/* Try */		\
"	jz	2f;"				/* got it */		\
"	pushl	%4;"							\
"	pushl	%5;"							\
"	call	mtx_enter_hard;" /* mtx_enter_hard(mtxp, type, oflags) */ \
"	addl	$12,%%esp;"						\
"	jmp	1f;"							\
"2:	popl	%2;"				/* save flags */	\
"1:"									\
"# getlock_spin_block"							\
	: "+a" (_res),				/* 0 */			\
	  "+m" (mtxp->mtx_lock),		/* 1 */			\
	  "=m" (mtxp->mtx_savecrit)		/* 2 */			\
	: "r" (tid),				/* 3 (input) */		\
	  "gi" (type),				/* 4 */			\
	  "g" (mtxp)				/* 5 */			\
	: "cc", "memory", "ecx", "edx"		/* used */ );		\
})

/*
 * Get a lock without any recursion handling. Calls the hard enter function if
 * we can't get it inline.
 */
#define	_getlock_norecurse(mtxp, tid, type) ({				\
	int	_res = MTX_UNOWNED;					\
									\
	__asm __volatile (						\
"	" MPLOCKED ""							\
"	cmpxchgl %2,%1;"			/* Try */		\
"	jz	1f;"				/* got it */		\
"	pushl	%3;"							\
"	pushl	%4;"							\
"	call	mtx_enter_hard;" /* mtx_enter_hard(mtxp, type) */	\
"	addl	$8,%%esp;"						\
"1:"									\
"# getlock_norecurse"							\
	: "+a" (_res),				/* 0 */			\
	  "+m" (mtxp->mtx_lock)			/* 1 */			\
	: "r" (tid),				/* 2 (input) */		\
	  "gi" (type),				/* 3 */			\
	  "g" (mtxp)				/* 4 */			\
	: "cc", "memory", "ecx", "edx"		/* used */ );		\
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
	: "cc", "memory", "ecx", "edx"		/* used */ );		\
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
	: "cc", "memory", "ecx", "edx"		/* used */ );		\
})

/*
 * Release a spin lock (with possible recursion).
 *
 * We use xchgl to clear lock (instead of simple store) to flush posting
 * buffers and make the change visible to other CPU's.
 */
#define	_exitlock_spin(mtxp) ({						\
	int	_res;							\
									\
	__asm __volatile (						\
"	movl	%1,%2;"							\
"	decl	%2;"							\
"	js	1f;"							\
"	movl	%2,%1;"							\
"	jmp	2f;"							\
"1:	movl	$ " _V(MTX_UNOWNED) ",%2;"				\
"	pushl	%3;"							\
"	xchgl	%2,%0;"					  		\
"	popfl;"								\
"2:"									\
"# exitlock_spin"							\
	: "+m" (mtxp->mtx_lock),	/* 0 */				\
	  "+m" (mtxp->mtx_recurse),	/* 1 */ 			\
	  "=r" (_res)			/* 2 */				\
	: "g"  (mtxp->mtx_savecrit)	/* 3 */				\
	: "cc", "memory", "ecx"		/* used */ );			\
})

#endif	/* I386_CPU */

#undef _V

#endif	/* _KERNEL */

#else	/* !LOCORE */

/*
 * Simple assembly macros to get and release mutexes.
 *
 * Note: All of these macros accept a "flags" argument and are analoguous
 *	 to the mtx_lock_flags and mtx_unlock_flags general macros. If one
 *	 desires to not pass a flag, the value 0 may be passed as second
 *	 argument.
 *
 * XXX: We only have MTX_LOCK_SPIN and MTX_UNLOCK_SPIN for now, since that's
 *	all we use right now. We should add MTX_LOCK and MTX_UNLOCK (for sleep
 *	locks) in the near future, however.
 */
#define MTX_LOCK_SPIN(lck, flags)					\
	pushl $0 ;							\
	pushl $0 ;							\
	pushl $flags ;							\
	pushl $lck ;							\
	call _mtx_lock_spin_flags ;					\
	addl $0x10, %esp ;						\

#define MTX_UNLOCK_SPIN(lck)						\
	pushl $0 ;							\
	pushl $0 ;							\
	pushl $0 ;							\
	pushl $lck ;							\
	call _mtx_unlock_spin_flags ;					\
	addl $0x10, %esp ;						\

/*
 * XXX: These two are broken right now and need to be made to work for
 * XXX: sleep locks, as the above two work for spin locks. We're not in
 * XXX: too much of a rush to do these as we do not use them right now.
 */
#define	MTX_ENTER(lck, type)						\
	pushl	$0 ;				/* dummy __LINE__ */	\
	pushl	$0 ;				/* dummy __FILE__ */	\
	pushl	$type ;							\
	pushl	$lck ;							\
	call	_mtx_lock_XXX ;						\
	addl	$16,%esp

#define	MTX_EXIT(lck, type)						\
	pushl	$0 ;				/* dummy __LINE__ */	\
	pushl	$0 ;				/* dummy __FILE__ */	\
	pushl	$type ;							\
	pushl	$lck ;							\
	call	_mtx_unlock_XXX ;					\
	addl	$16,%esp

#endif	/* !LOCORE */
#endif	/* __MACHINE_MUTEX_H */
