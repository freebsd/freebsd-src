/* $FreeBSD$ */
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

	.text
/*
 * Kernel setjmp and longjmp.  Rather minimalist.
 *
 *	longjmp(label_t *a)
 * will generate a "return (1)" from the last call to
 *	setjmp(label_t *a)
 * by restoring registers from the stack,
 */

	.set	noreorder

LEAF(setjmp, 1)
	LDGP(pv)

	stq	ra, (0 * 8)(a0)			/* return address */
	stq	s0, (1 * 8)(a0)			/* callee-saved registers */
	stq	s1, (2 * 8)(a0)
	stq	s2, (3 * 8)(a0)
	stq	s3, (4 * 8)(a0)
	stq	s4, (5 * 8)(a0)
	stq	s5, (6 * 8)(a0)
	stq	s6, (7 * 8)(a0)
	stq	sp, (8 * 8)(a0)

	ldiq	t0, 0xbeeffedadeadbabe		/* set magic number */
	stq	t0, (9 * 8)(a0)

	mov	zero, v0			/* return zero */
	RET
END(setjmp)

LEAF(longjmp, 1)
	LDGP(pv)

	ldiq	t0, 0xbeeffedadeadbabe		/* check magic number */
	ldq	t1, (9 * 8)(a0)
	cmpeq	t0, t1, t0
	beq	t0, longjmp_botch		/* if bad, punt */

	ldq	ra, (0 * 8)(a0)			/* return address */
	ldq	s0, (1 * 8)(a0)			/* callee-saved registers */
	ldq	s1, (2 * 8)(a0)
	ldq	s2, (3 * 8)(a0)
	ldq	s3, (4 * 8)(a0)
	ldq	s4, (5 * 8)(a0)
	ldq	s5, (6 * 8)(a0)
	ldq	s6, (7 * 8)(a0)
	ldq	sp, (8 * 8)(a0)

	ldiq	v0, 1
	RET

longjmp_botch:
	lda	a0, longjmp_botchmsg
	mov	ra, a1
	CALL(panic)
	call_pal PAL_bugchk

	.data
longjmp_botchmsg:
	.asciz	"longjmp botch from %p"
	.text

END(longjmp)
