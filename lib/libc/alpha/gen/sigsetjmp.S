/*	$NetBSD: sigsetjmp.S,v 1.2 1996/10/17 03:08:07 cgd Exp $	*/

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

#include <machine/asm.h>

/*
 * C library -- sigsetjmp, siglongjmp
 *
 *	siglongjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	sigsetjmp(a, mask)
 * by restoring registers from the stack.
 * If `mask' is non-zero, the previous signal
 * state will be restored.
 */

	.set	noreorder

LEAF(sigsetjmp, 2)
	LDGP(pv)
	stq	a1, (81 * 8)(a0)		/* save the mask */
	bne	a1, Lsavesig			/* if !zero, save signals */
	jmp	zero, _setjmp				/* else don't. */
Lsavesig:
	jmp	zero, setjmp
END(sigsetjmp)

#ifdef _THREAD_SAFE
LEAF(__siglongjmp, 2)
#else
XLEAF(__siglongjmp, 2)
LEAF(siglongjmp, 2)
#endif
	LDGP(pv)
	ldq	t0, (81 * 8)(a0)		/* get the mask */
	bne	t0, Lrestoresig			/* if !zero, restore signals */
	jmp	zero, _longjmp
Lrestoresig:
	jmp	zero, longjmp
#ifdef _THREAD_SAFE
END(__siglongjmp)
#else
END(siglongjmp)
#endif