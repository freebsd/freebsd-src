/* $FreeBSD: src/sys/alpha/alpha/exception.s,v 1.3.2.1 2000/07/03 19:59:44 mjacob Exp $ */
/* $NetBSD: locore.s,v 1.47 1998/03/22 07:26:32 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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
 */

#include <machine/asm.h>
#include <assym.s>

/**************************************************************************/

/*
 * XentArith:
 * System arithmetic trap entry point.
 */

	PALVECT(XentArith)		/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_ARITH
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)

	jmp	zero, exception_return
	END(XentArith)

/**************************************************************************/

/*
 * XentIF:
 * System instruction fault trap entry point.
 */

	PALVECT(XentIF)			/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_IF
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)
	jmp	zero, exception_return	
	END(XentIF)

/**************************************************************************/

/*
 * XentInt:
 * System interrupt entry point.
 */

	PALVECT(XentInt)		/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	mov	sp, a3			; .loc 1 __LINE__
	CALL(interrupt)
	jmp	zero, exception_return
	END(XentInt)

/**************************************************************************/

/*
 * XentMM:
 * System memory management fault entry point.
 */

	PALVECT(XentMM)			/* setup frame, save registers */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_MM
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)

	jmp	zero, exception_return
	END(XentMM)

/**************************************************************************/

/*
 * XentSys:
 * System call entry point.
 */

	ESETUP(XentSys)			; .loc 1 __LINE__

	stq	v0,(FRAME_V0*8)(sp)		/* in case we need to restart */
	stq	s0,(FRAME_S0*8)(sp)
	stq	s1,(FRAME_S1*8)(sp)
	stq	s2,(FRAME_S2*8)(sp)
	stq	s3,(FRAME_S3*8)(sp)
	stq	s4,(FRAME_S4*8)(sp)
	stq	s5,(FRAME_S5*8)(sp)
	stq	s6,(FRAME_S6*8)(sp)
	stq	a0,(FRAME_A0*8)(sp)
	stq	a1,(FRAME_A1*8)(sp)
	stq	a2,(FRAME_A2*8)(sp)
	stq	a3,(FRAME_A3*8)(sp)
	stq	a4,(FRAME_A4*8)(sp)
	stq	a5,(FRAME_A5*8)(sp)
	stq	ra,(FRAME_RA*8)(sp)

	/* syscall number, passed in v0, is first arg, frame pointer second */
	mov	v0,a0
	mov	sp,a1			; .loc 1 __LINE__
	CALL(syscall)

	jmp	zero, exception_return
	END(XentSys)

/**************************************************************************/

/*
 * XentUna:
 * System unaligned access entry point.
 */

LEAF(XentUna, 3)				/* XXX should be NESTED */
	.set noat
	lda	sp,-(FRAME_SW_SIZE*8)(sp)
	stq	at_reg,(FRAME_AT*8)(sp)
	.set at
	stq	ra,(FRAME_RA*8)(sp)
	bsr	ra, exception_save_regs		/* jmp/CALL trashes pv/t12 */

	/* a0, a1, & a2 already set up */
	ldiq	a3, ALPHA_KENTRY_UNA
	mov	sp, a4			; .loc 1 __LINE__
	CALL(trap)

	jmp	zero, exception_return
	END(XentUna)
	
	
/**************************************************************************/

/*
 * console 'restart' routine to be placed in HWRPB.
 */
LEAF(XentRestart, 1)			/* XXX should be NESTED */
	.set noat
	lda	sp,-(FRAME_SIZE*8)(sp)
	stq	at_reg,(FRAME_AT*8)(sp)
	.set at
	stq	v0,(FRAME_V0*8)(sp)
	stq	a3,(FRAME_A3*8)(sp)
	stq	a4,(FRAME_A4*8)(sp)
	stq	a5,(FRAME_A5*8)(sp)
	stq	s0,(FRAME_S0*8)(sp)
	stq	s1,(FRAME_S1*8)(sp)
	stq	s2,(FRAME_S2*8)(sp)
	stq	s3,(FRAME_S3*8)(sp)
	stq	s4,(FRAME_S4*8)(sp)
	stq	s5,(FRAME_S5*8)(sp)
	stq	s6,(FRAME_S6*8)(sp)
	stq	t0,(FRAME_T0*8)(sp)
	stq	t1,(FRAME_T1*8)(sp)
	stq	t2,(FRAME_T2*8)(sp)
	stq	t3,(FRAME_T3*8)(sp)
	stq	t4,(FRAME_T4*8)(sp)
	stq	t5,(FRAME_T5*8)(sp)
	stq	t6,(FRAME_T6*8)(sp)
	stq	t7,(FRAME_T7*8)(sp)
	stq	t8,(FRAME_T8*8)(sp)
	stq	t9,(FRAME_T9*8)(sp)
	stq	t10,(FRAME_T10*8)(sp)
	stq	t11,(FRAME_T11*8)(sp)
	stq	t12,(FRAME_T12*8)(sp)
	stq	ra,(FRAME_RA*8)(sp)

	br	pv,LXconsole_restart1
LXconsole_restart1: LDGP(pv)

	ldq	a0,(FRAME_RA*8)(sp)		/* a0 = ra */
	ldq	a1,(FRAME_T11*8)(sp)		/* a1 = ai */
	ldq	a2,(FRAME_T12*8)(sp)		/* a2 = pv */
	CALL(console_restart)

	call_pal PAL_halt
	END(XentRestart)
