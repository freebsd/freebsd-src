/*	$NetBSD: setjmp.S,v 1.3 1997/12/05 02:06:27 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 *
 * $FreeBSD$
 */

#include "SYS.h"

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * and the previous signal state.
 */

	.set	noreorder

LEAF(setjmp, 1)
	LDGP(pv)
	stq	ra, (2 * 8)(a0)			/* sc_pc = return address */
	stq	s0, (( 9 + 4) * 8)(a0)		/* saved bits of sc_regs */
	stq	s1, ((10 + 4) * 8)(a0)
	stq	s2, ((11 + 4) * 8)(a0)
	stq	s3, ((12 + 4) * 8)(a0)
	stq	s4, ((13 + 4) * 8)(a0)
	stq	s5, ((14 + 4) * 8)(a0)
	stq	s6, ((15 + 4) * 8)(a0)
	stq	ra, ((26 + 4) * 8)(a0)
	stq	sp, ((30 + 4) * 8)(a0)

	/*
	 * get signal information
	 */
	mov	a0, s0				/* squirrel away ptr to sc */

	/* see what's blocked */
	lda	a2, (71 * 8)(a0)		/* oset: sc_reserved */
	mov	zero, a1			/* set:	 NULL */
	addq	a1, 1, a0			/* how:	 SIG_BLOCK */
	PCALL(sigprocmask)			/* see what's blocked */

	lda	sp, -24(sp)			/* sizeof struct sigaltstack */
	mov	zero, a0
	mov	sp, a1
	PCALL(sigaltstack)
	ldl	t0, 16(sp)			/* offset of ss_flags */
	lda	sp, 24(sp)			/* sizeof struct sigaltstack */
	ldq	ra, ((26 + 4) * 8)(s0)		/* restore return address */
	blt	v0, botch			/* check for error */
	and	t0, 0x1, t0			/* get SA_ONSTACK flag */
	stq	t0, (0 * 8)(s0)			/* and save it in sc_onstack */
	/*
	 * Restore old s0 and a0, and continue saving registers
	 */
	mov	s0, a0
	ldq	s0, (( 9 + 4) * 8)(a0)

	ldiq	t0, 0xacedbade			/* sigcontext magic number */
	stq	t0, ((31 + 4) * 8)(a0)		/* magic in sc_regs[31] */
	/* Too bad we can't check if we actually used FP */
	ldiq	t0, 1
	stq	t0, (36 * 8)(a0)		/* say we've used FP.  */
	stt	fs0, ((2 + 37) * 8)(a0)		/* saved bits of sc_fpregs */
	stt	fs1, ((3 + 37) * 8)(a0)
	stt	fs2, ((4 + 37) * 8)(a0)
	stt	fs3, ((5 + 37) * 8)(a0)
	stt	fs4, ((6 + 37) * 8)(a0)
	stt	fs5, ((7 + 37) * 8)(a0)
	stt	fs6, ((8 + 37) * 8)(a0)
	stt	fs7, ((9 + 37) * 8)(a0)
	mf_fpcr	ft0				/* get FP control reg */
	stt	ft0, (69 * 8)(a0)		/* and store it in sc_fpcr */
	stq	zero, (70 * 8)(a0)		/* FP software control XXX */
	stq	zero, (71 * 8)(a0)		/* sc_reserved[0] */
	stq	zero, (72 * 8)(a0)		/* sc_reserved[1] */
	stq	zero, (73 * 8)(a0)		/* sc_xxx[0] */
	stq	zero, (74 * 8)(a0)		/* sc_xxx[1] */
	stq	zero, (75 * 8)(a0)		/* sc_xxx[2] */
	stq	zero, (76 * 8)(a0)		/* sc_xxx[3] */
	stq	zero, (77 * 8)(a0)		/* sc_xxx[4] */
	stq	zero, (78 * 8)(a0)		/* sc_xxx[5] */
	stq	zero, (79 * 8)(a0)		/* sc_xxx[6] */
	stq	zero, (80 * 8)(a0)		/* sc_xxx[7] */

	mov	zero, v0			/* return zero */
	RET
END(setjmp)

#ifdef _THREAD_SAFE
LEAF(__longjmp, 2)
#else
XLEAF(__longjmp, 2)
LEAF(longjmp, 2)
#endif
	LDGP(pv)
	stq	a1, (( 0 + 4) * 8)(a0)		/* save return value */
	CALL(sigreturn)				/* use sigreturn to return */

botch:
	CALL(longjmperror)
	CALL(abort)
	RET					/* "can't" get here... */
#ifdef _THREAD_SAFE
END(__longjmp)
#else
END(longjmp)
#endif
