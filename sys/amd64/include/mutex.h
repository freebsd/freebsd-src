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
#include <machine/psl.h>

/* Global locks */
extern struct mtx	clock_lock;

/*
 * Debugging
 */
#ifdef MUTEX_DEBUG

#ifdef _KERN_MUTEX_C_
char STR_IEN[] = "fl & PSL_I";
char STR_IDIS[] = "!(fl & PSL_I)";
char STR_SIEN[] = "mpp->mtx_saveintr & PSL_I";
#else	/* _KERN_MUTEX_C_ */
extern char STR_IEN[];
extern char STR_IDIS[];
extern char STR_SIEN[];
#endif	/* _KERN_MUTEX_C_ */
#endif	/* MUTEX_DEBUG */

#define	ASS_IEN		MPASS2(read_eflags() & PSL_I, STR_IEN)
#define	ASS_IDIS	MPASS2((read_eflags() & PSL_I) == 0, STR_IDIS)
#define ASS_SIEN(mpp)	MPASS2((mpp)->mtx_saveintr & PSL_I, STR_SIEN)

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
	  "=m" (mtxp->mtx_saveintr)		/* 2 */			\
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
#define	_exitlock_spin(mtxp) ({						\
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
"	pushl	%3;"							\
"	" MPLOCKED ""							\
"	cmpxchgl %%ecx,%0;"				  		\
"	popfl;"								\
"2:"									\
"# exitlock_spin"							\
	: "+m" (mtxp->mtx_lock),	/* 0 */				\
	  "+m" (mtxp->mtx_recurse),	/* 1 */ 			\
	  "=&a" (_res)			/* 2 */				\
	: "g"  (mtxp->mtx_saveintr)	/* 3 */				\
	: "memory", "ecx"		/* used */ );			\
})

#endif	/* I386_CPU */

#endif	/* _KERNEL */

#else	/* !LOCORE */

/*
 * Simple assembly macros to get and release non-recursive spin locks
 */

#if defined(I386_CPU)

#define	MTX_EXIT(lck, reg)						\
	pushl	lck+MTX_SAVEINTR;					\
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
	popl	lck+MTX_SAVEINTR;

/* Must use locked bus op (cmpxchg) when setting to unowned (barrier) */
#define	MTX_EXIT(lck,reg)						\
	pushl	lck+MTX_SAVEINTR;					\
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
	popl	lck+MTX_SAVEINTR;					\
	jmp	10f;							\
8:	add	$4,%esp;						\
10:

#define	MTX_EXIT_WITH_RECURSION(lck,reg)				\
	movl	lck+MTX_RECURSE,%eax;					\
	decl	%eax;							\
	js	9f;							\
	movl	%eax,lck+MTX_RECURSE;					\
	jmp	8f;							\
	pushl	lck+MTX_SAVEINTR;					\
9:	movl	lck+MTX_LOCK,%eax;					\
	movl	$ MTX_UNOWNED,reg;					\
	MPLOCKED							\
	cmpxchgl reg,lck+MTX_LOCK;					\
	popf								\
8:

#endif	/* I386_CPU */
#endif	/* !LOCORE */
#endif	/* __MACHINE_MUTEX_H */
