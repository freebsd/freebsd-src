/*	$NetBSD: setjmp.S,v 1.3 1997/12/05 02:06:27 thorpej Exp $	*/
/* $FreeBSD$ */

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
 */

#define     LOCORE
#include    <machine/asm.h>
#include    <machine/setjmp.h>

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

ENTRY(setjmp, 1)
	alloc	loc0=ar.pfs,1,2,3,0
	mov	loc1=rp
	;; 
	mov	out0=1				// how = SIG_BLOCK
	mov	out1=0				// set = NULL
	add	out2=J_SIG0,in0			// oset = &jb[J_SIG0]
	br.call.sptk.few rp=__sys_sigprocmask
	;;
	mov	rp=loc1
	mov	r14=loc0
	;;
	alloc	r15=ar.pfs,1,0,0,0		// drop register frame
	;;
	mov	ar.pfs=r14			// restore ar.pfs
	br.sptk.many _setjmp			// finish saving state
END(setjmp)

	WEAK_ALIAS(longjmp,__longjmp)
ENTRY(__longjmp, 2)
	alloc	loc0=ar.pfs,2,2,3,0
	mov	loc1=rp
	;;
	mov	out0=3				// how = SIG_SETMASK
	add	out1=J_SIG0,in0			// set = &jb[J_SIG0]
	mov	out2=0				// oset = NULL
	br.call.sptk.few rp=__sys_sigprocmask
	;;
	mov	rp=loc1
	mov	r14=loc0
	;;
	alloc	r15=ar.pfs,2,0,0,0		// drop register frame
	;;
	mov	ar.pfs=r14			// restore ar.pfs
	br.sptk.many _longjmp			// finish restoring state
END(__longjmp)
