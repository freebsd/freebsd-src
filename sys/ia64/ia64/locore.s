/*-
 * Copyright (c) 1998 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
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

#include <sys/cdefs.h>
#include <machine/asm.h>
#include <machine/fpu.h>
#include <sys/syscall.h>
#include <assym.s>

#ifndef EVCNT_COUNTERS
#define _LOCORE
#include <machine/intrcnt.h>
#endif

	.text

/*
 * Not really a leaf but we can't return.
 */
ENTRY(locorestart, 1)

	movl	r8=ia64_vector_table	// set up IVT early
	movl	r9=ia64_vhpt+(1<<8)+(15<<2)+1 // and VHPT
	;;
	mov	cr.iva=r8
	mov	cr.pta=r9
	;;
	srlz.i
	;;
	srlz.d
	;; 
	movl	gp=__gp			// find kernel globals
	;;
	br.call.sptk.many rp=ia64_init

	/*
	 * switch to proc0 and then initialise the rest of the kernel.
	 */
	alloc	r16=ar.pfs,0,0,1,0
	;; 
	movl	out0=proc0
	;;
	add	out0=P_ADDR,out0
	;;
	ld8	out0=[out0]
	;; 
	add	r16=U_PCB_B0,out0	// return to mi_startup
	movl	r17=mi_startup
	;;
	st8	[r16]=r17
	;; 
	br.call.sptk.many rp=restorectx

	/* NOTREACHED */	
	
	END(locorestart)

	
/**************************************************************************/

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 *
 * On entry, registers look like:
 *
 *      r14	signal number
 *      r15	pointer to siginfo_t
 *	r16	pointer to signal context frame (scp)
 *      r17	address of handler function descriptor
 *	r18	address of new backing store (if any)
 *      sp+16	pointer to sigframe
 */

ENTRY(sigcode,0)
	ld8	r8=[r17],8		// function address
	;;
	ld8	gp=[r17]		// function's gp value
	mov	b6=r8			// transfer to a branch register
	cover
	;;
	alloc	r5=ar.pfs,0,0,3,0	// register frame for call
	;;
	mov	out0=r14		// signal number
	add	r8=UC_MCONTEXT_MC_AR_BSP,r16 // address or mc_ar_bsp
	mov	r9=ar.bsp		// save ar.bsp
	;;
	st8	[r8]=r9
	cmp.eq	p1,p0=r0,r18		// check for new bs
(p1)	br.cond.sptk.few 1f		// branch if not switching
	flushrs				// flush out to old bs
	mov	ar.rsc=0		// switch off RSE
	add	r8=UC_MCONTEXT_MC_AR_RNAT,r16 // address of mc_ar_rnat
	;;
	mov	r9=ar.rnat		// value of ar.rnat after flush
	mov	ar.bspstore=r18		// point at new bs
	;;
	st8	[r8]=r9			// remember ar.rnat
	mov	ar.rsc=15		// XXX bogus value - check
	invala
	;; 
1:	mov	out1=r15		// siginfo
	mov	out2=r16		// ucontext
	mov	r4=r17			// save ucontext pointer from call
	br.call.sptk.few rp=b6		// call the signal handler
(p1)	br.cond.sptk.few 2f		// note: p1 is preserved
	flushrs
	mov	ar.rsc=0
	add	r8=UC_MCONTEXT_MC_AR_RNAT,r4 // address of mc_ar_rnat
	;;
	ld8	r9=[r8]
	;; 
	add	r8=UC_MCONTEXT_MC_AR_BSP,r4 // address of mc_ar_bsp
	;;
	ld8	r10=[r8]
	;;
	mov	ar.bspstore=r10
	;;
	mov	ar.rnat=r9
	mov	ar.rsc=15
2:	
	CALLSYS_NOERROR(sigreturn)	// and call sigreturn() with it.
	mov	out0=ret0		// if that failed, get error code
	CALLSYS_NOERROR(exit)		// and call exit() with it.
XENTRY(esigcode)
	END(sigcode)

	.data
	EXPORT(szsigcode)
	.quad	esigcode-sigcode
	.text
	
/**************************************************************************/

/*
 * savefpstate: Save a process's floating point state.
 *
 * Arguments:
 *	a0	'struct fpstate *' to save into
 */

ENTRY(savefpstate, 1)
#if 0
	LDGP(pv)
	/* save all of the FP registers */
	lda	t1, FPREG_FPR_REGS(a0)	/* get address of FP reg. save area */
	stt	$f0,   (0 * 8)(t1)	/* save first register, using hw name */
	stt	$f1,   (1 * 8)(t1)	/* etc. */
	stt	$f2,   (2 * 8)(t1)
	stt	$f3,   (3 * 8)(t1)
	stt	$f4,   (4 * 8)(t1)
	stt	$f5,   (5 * 8)(t1)
	stt	$f6,   (6 * 8)(t1)
	stt	$f7,   (7 * 8)(t1)
	stt	$f8,   (8 * 8)(t1)
	stt	$f9,   (9 * 8)(t1)
	stt	$f10, (10 * 8)(t1)
	stt	$f11, (11 * 8)(t1)
	stt	$f12, (12 * 8)(t1)
	stt	$f13, (13 * 8)(t1)
	stt	$f14, (14 * 8)(t1)
	stt	$f15, (15 * 8)(t1)
	stt	$f16, (16 * 8)(t1)
	stt	$f17, (17 * 8)(t1)
	stt	$f18, (18 * 8)(t1)
	stt	$f19, (19 * 8)(t1)
	stt	$f20, (20 * 8)(t1)
	stt	$f21, (21 * 8)(t1)
	stt	$f22, (22 * 8)(t1)
	stt	$f23, (23 * 8)(t1)
	stt	$f24, (24 * 8)(t1)
	stt	$f25, (25 * 8)(t1)
	stt	$f26, (26 * 8)(t1)
	stt	$f27, (27 * 8)(t1)
	stt	$f28, (28 * 8)(t1)
	stt	$f29, (29 * 8)(t1)
	stt	$f30, (30 * 8)(t1)

	/*
	 * Then save the FPCR; note that the necessary 'trapb's are taken
	 * care of on kernel entry and exit.
	 */
	mf_fpcr	ft0
	stt	ft0, FPREG_FPR_CR(a0)	/* store to FPCR save area */

	RET
#endif
	END(savefpstate)

/**************************************************************************/

/*
 * restorefpstate: Restore a process's floating point state.
 *
 * Arguments:
 *	a0	'struct fpstate *' to restore from
 */

ENTRY(restorefpstate, 1)
#if 0
	LDGP(pv)
	/*
	 * Restore the FPCR; note that the necessary 'trapb's are taken care of
	 * on kernel entry and exit.
	 */
	ldt	ft0, FPREG_FPR_CR(a0)	/* load from FPCR save area */
	mt_fpcr	ft0

	/* Restore all of the FP registers. */
	lda	t1, FPREG_FPR_REGS(a0)	/* get address of FP reg. save area */
	ldt	$f0,   (0 * 8)(t1)	/* restore first reg., using hw name */
	ldt	$f1,   (1 * 8)(t1)	/* etc. */
	ldt	$f2,   (2 * 8)(t1)
	ldt	$f3,   (3 * 8)(t1)
	ldt	$f4,   (4 * 8)(t1)
	ldt	$f5,   (5 * 8)(t1)
	ldt	$f6,   (6 * 8)(t1)
	ldt	$f7,   (7 * 8)(t1)
	ldt	$f8,   (8 * 8)(t1)
	ldt	$f9,   (9 * 8)(t1)
	ldt	$f10, (10 * 8)(t1)
	ldt	$f11, (11 * 8)(t1)
	ldt	$f12, (12 * 8)(t1)
	ldt	$f13, (13 * 8)(t1)
	ldt	$f14, (14 * 8)(t1)
	ldt	$f15, (15 * 8)(t1)
	ldt	$f16, (16 * 8)(t1)
	ldt	$f17, (17 * 8)(t1)
	ldt	$f18, (18 * 8)(t1)
	ldt	$f19, (19 * 8)(t1)
	ldt	$f20, (20 * 8)(t1)
	ldt	$f21, (21 * 8)(t1)
	ldt	$f22, (22 * 8)(t1)
	ldt	$f23, (23 * 8)(t1)
	ldt	$f24, (24 * 8)(t1)
	ldt	$f25, (25 * 8)(t1)
	ldt	$f26, (26 * 8)(t1)
	ldt	$f27, (27 * 8)(t1)
	ldt	$f28, (28 * 8)(t1)
	ldt	$f29, (29 * 8)(t1)
	ldt	$f30, (30 * 8)(t1)

	RET
#endif
	END(restorefpstate)

/*
 * When starting init, call this to configure the process for user
 * mode.  This will be inherited by other processes.
 */
	ENTRY_NOPROFILE(prepare_usermode, 0)
	END(prepare_usermode)

	.data
	EXPORT(proc0paddr)
	.quad	0
	
	.text
	
/* XXX: make systat/vmstat happy */
	.data
EXPORT(intrnames)
	.asciz	"clock"
intr_n = 0
.rept INTRCNT_COUNT
	.ascii "intr "
	.byte intr_n / 10 + '0, intr_n % 10 + '0
	.asciz "     "		/* space for platform-specific rewrite */
	intr_n = intr_n + 1
.endr
EXPORT(eintrnames)
	.align 8
EXPORT(intrcnt)
	.fill INTRCNT_COUNT + 1, 8, 0
EXPORT(eintrcnt)
	.text
