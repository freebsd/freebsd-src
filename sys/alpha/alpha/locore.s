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

#include <machine/asm.h>
#include <sys/syscall.h>
#include <assym.s>

#ifndef EVCNT_COUNTERS
#define _LOCORE
#include <machine/intrcnt.h>
#endif

/*
 * PTmap is recursive pagemap at top of virtual address space.
 * Within PTmap, the lev1 and lev0 page tables can be found.
 */
	.globl	PTmap,PTlev2,PTlev1,PTlev1pte
	.equ	PTmap,VPTBASE
	.equ	PTlev2,PTmap + (PTLEV1I << ALPHA_L2SHIFT)
	.equ	PTlev1,PTlev2 + (PTLEV1I << ALPHA_L3SHIFT)
	.equ	PTlev1pte,PTlev1 + (PTLEV1I * PTESIZE)
	
/*
 * Perform actions necessary to switch to a new context.  The
 * hwpcb should be in a0.
 */
#define	SWITCH_CONTEXT							\
	/* Make a note of the context we're running on. */		\
	stq	a0, GD_CURPCB(globalp);					\
									\
	/* Swap in the new context. */					\
	call_pal PAL_OSF1_swpctx
	
	.text

	NESTED(locorestart, 1, 0, ra, 0, 0)

	br	pv,1f
1:	LDGP(pv)
	
	/* Load KGP with current GP. */
	or	a0,zero,s0		/* save pfn */
	or	gp,zero,a0
	call_pal PAL_OSF1_wrkgp		/* clobbers a0, t0, t8-t11 */
	or	s0,zero,a0		/* restore pfn */

	/*
	 * Call alpha_init() to do pre-main initialization.
	 * alpha_init() gets the arguments we were called with,
	 * which are already in a0, a1, a2, a3, and a4.
	 */
	CALL(alpha_init)

	/* Set up the virtual page table pointer. */
	ldiq	a0, VPTBASE
	call_pal PAL_OSF1_wrvptptr	/* clobbers a0, t0, t8-t11 */

	/*
	 * Initialise globalp.
	 */
	call_pal PAL_OSF1_rdval		/* clobbers t0, t8-t11 */
	mov	v0, globalp

	/*
	 * Switch to proc0's PCB, which is at U_PCB off of proc0paddr.
	 */
	lda	t0,proc0			/* get phys addr of pcb */
	ldq	a0,P_MD_PCBPADDR(t0)
	SWITCH_CONTEXT

	/*
	 * We've switched to a new page table base, so invalidate the TLB
	 * and I-stream.  This happens automatically everywhere but here.
	 */
	ldiq	a0, -2				/* TBIA */
	call_pal PAL_OSF1_tbi
	call_pal PAL_imb

	/*
	 * Construct a fake trap frame, so execve() can work normally.
	 * Note that setregs() is responsible for setting its contents
	 * to 'reasonable' values.
	 */
	lda	sp,-288(sp)	/* space for struct trapframe */
	mov	sp, a0				/* arg is frame ptr */
	CALL(mi_startup)			/* go to mi_startup()! */

	/* NOTREACHED */

	END(locorestart)

#ifdef SMP
	/*
	 * Secondary processors start executing here. They will have their
	 * unique value set to point at the per-cpu structure and will 
	 * be executing on their private idle stack.
	 */
	NESTED(smp_init_secondary_glue, 1, 0, ra, 0, 0)

	br	pv, 1f
1:	LDGP(pv)

	call_pal PAL_rdunique			/* initialise globalp */	
	mov	v0, globalp

	ldq	a0, GD_IDLEPCBPHYS(globalp)	/* switch to idle ctx */
	call_pal PAL_OSF1_swpctx

	/* Load KGP with current GP. */
	or	gp,zero,a0
	call_pal PAL_OSF1_wrkgp		/* clobbers a0, t0, t8-t11 */

	CALL(smp_init_secondary)		/* never returns */

	/* NOTREACHED */

	call_pal PAL_halt

	END(smp_init_secondary_glue)
#endif
		
/**************************************************************************/

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 *
 * On entry, stack & registers look like:
 *
 *      a0	signal number
 *      a1	pointer to siginfo_t
 *      a2	pointer to signal context frame (scp)
 *      a3	address of handler
 *      sp+0	saved hardware state
 *                      .
 *                      .
 *      scp+0	beginning of signal context frame
 */

NESTED(sigcode,0,0,ra,0,0)
	lda	sp, -16(sp)		/* save the sigcontext pointer */
	stq	a2, 0(sp)
	jsr	ra, (t12)		/* call the signal handler (t12==pv) */
	ldq	a0, 0(sp)		/* get the sigcontext pointer */
	lda	sp, 16(sp)
	CALLSYS_NOERROR(sigreturn)	/* and call sigreturn() with it. */
	mov	v0, a0			/* if that failed, get error code */
	CALLSYS_NOERROR(exit)		/* and call exit() with it. */
XNESTED(esigcode,0)
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

LEAF(savefpstate, 1)
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
	END(savefpstate)

/**************************************************************************/

/*
 * restorefpstate: Restore a process's floating point state.
 *
 * Arguments:
 *	a0	'struct fpstate *' to restore from
 */

LEAF(restorefpstate, 1)
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
	END(restorefpstate)

/*
 * When starting init, call this to configure the process for user
 * mode.  This will be inherited by other processes.
 */
	LEAF_NOPROFILE(prepare_usermode, 0)
	RET
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
	.asciz "     "		# space for platform-specific rewrite
	intr_n = intr_n + 1
.endr
EXPORT(eintrnames)
	.align 3
EXPORT(intrcnt)
	.fill INTRCNT_COUNT + 1, 8, 0
EXPORT(eintrcnt)
	.text
