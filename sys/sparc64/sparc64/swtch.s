/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#include <machine/asmacros.h>
#include <machine/asi.h>
#include <machine/ktr.h>
#include <machine/tstate.h>

#include "assym.s"

/*
 * Save and restore FPU state. This is done at switch time.
 * We could use FPRS_DL and FPRS_DU; however, it is accessible to non-privileged
 * software, so it is avoided for compatabilities sake.
 * savefp clobbers %fprs.
 */
	.macro	savefp	state, tmp, fprs
	or	\fprs, FPRS_FEF, \tmp
	wr	\tmp, 0, %fprs
	stx	%fsr, [\state + FP_FSR]
	rd	%asi, \tmp
	wr	%g0, ASI_BLK_S, %asi
	stda	%f0, [\state + FP_FB0] %asi
	stda	%f16, [\state + FP_FB1] %asi
	stda	%f32, [\state + FP_FB2] %asi
	stda	%f48, [\state + FP_FB3] %asi
	wr	\tmp, 0, %asi
	membar	#Sync
	.endm
	
	.macro	restrfp	state, tmp
	rd	%fprs, \tmp
	or	\tmp, FPRS_FEF, \tmp
	wr	\tmp, 0, %fprs
	rd	%asi, \tmp
	wr	%g0, ASI_BLK_S, %asi
	ldda	[\state + FP_FB0] %asi, %f0
	ldda	[\state + FP_FB1] %asi, %f16
	ldda	[\state + FP_FB2] %asi, %f32
	ldda	[\state + FP_FB3] %asi, %f48
	wr	\tmp, 0, %asi
	membar	#Sync
	ldx	[\state + FP_FSR], %fsr
	.endm

ENTRY(cpu_throw)
	save	%sp, -CCFSZ, %sp
	call	choosethread
	 ldx	[PCPU(CURTHREAD)], %l0
	flushw
	b,a	.Lsw1
END(cpu_throw)

ENTRY(cpu_switch)
	/*
	 * Choose a new process.  If its the same as the current one, do
	 * nothing.
	 */
	save	%sp, -CCFSZ, %sp
	call	choosethread
	 ldx	[PCPU(CURTHREAD)], %l0
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: from=%p (%s) to=%p (%s)"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%l0, [%g1 + KTR_PARM1]
	ldx	[%l0 + TD_PROC], %g2
	add	%g2, P_COMM, %g2
	stx	%g2, [%g1 + KTR_PARM2]
	stx	%o0, [%g1 + KTR_PARM3]
	ldx	[%o0 + TD_PROC], %g2
	add	%g2, P_COMM, %g2
	stx	%g2, [%g1 + KTR_PARM4]
9:
#endif
	cmp	%l0, %o0
	be,pn	%xcc, 3f
	 EMPTY

	/*
	 * Always save %fprs and %y. Both are not used within the kernel
	 * and are therefore not saved in the trap frame.
	 * If the process was using floating point, save its context.
	 */
	 ldx	[%l0 + TD_FRAME], %l1
	ldx	[%l0 + TD_PCB], %l2
	rd	%y, %l3
	stx	%l3, [%l2 + PCB_Y]
	rd	%fprs, %l3
	stx	%l3, [%l2 + PCB_FPSTATE + FP_FPRS]
	ldx	[%l1 + TF_TSTATE], %l1
	andcc	%l1, TSTATE_PEF, %l1
	be,pt	%xcc, 1f
	 nop
	savefp	%l2 + PCB_FPSTATE, %l4, %l3

	/*
	 * Flush the windows out to the stack and save the current frame
	 * pointer and program counter.
	 */
1:	flushw
	wrpr	%g0, 0, %cleanwin
	rdpr	%cwp, %l3
	stx	%l3, [%l2 + PCB_CWP]
	stx	%fp, [%l2 + PCB_FP]
	stx	%i7, [%l2 + PCB_PC]

	/*
	 * Load the new process's frame pointer and program counter, and set
	 * the current process and pcb.
	 */
.Lsw1:	ldx	[%o0 + TD_PCB], %o1
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: to=%p pc=%#lx fp=%#lx sp=%#lx cwp=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
	ldx	[%o1 + PCB_PC], %g2
	stx	%g2, [%g1 + KTR_PARM2]
	ldx	[%o1 + PCB_FP], %g2
	stx	%g2, [%g1 + KTR_PARM3]
	sub	%g2, CCFSZ, %g2
	stx	%g2, [%g1 + KTR_PARM4]
	ldx	[%o1 + PCB_CWP], %g2
	stx	%g2, [%g1 + KTR_PARM5]
9:
#endif
#if 1
	mov	%o0, %g4
	mov	%l0, %g5
	ldx	[%o1 + PCB_CWP], %o2
	wrpr	%o2, %cwp
	mov	%g4, %o0
	mov	%g5, %l0
#endif
	ldx	[%o0 + TD_PCB], %o1
	ldx	[%o1 + PCB_FP], %fp
	ldx	[%o1 + PCB_PC], %i7
	sub	%fp, CCFSZ, %sp
	stx	%o0, [PCPU(CURTHREAD)]
	stx	%o1, [PCPU(CURPCB)]

	/*
	 * Point to the new process's vmspace and load its vm context number.
	 * If its nucleus context we are done.
	 */
	ldx	[%o0 + TD_PROC], %o2
	ldx	[%o2 + P_VMSPACE], %o2
	lduw	[%o2 + VM_PMAP + PM_CONTEXT], %o3
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: to=%p vm=%p context=%#x"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%o0, [%g1 + KTR_PARM1]
	stx	%o2, [%g1 + KTR_PARM2]
	stx	%o3, [%g1 + KTR_PARM3]
9:
#endif
	brz,pn	%o3, 3f
	 EMPTY

	/*
	 * If the new process was using floating point, restore its context.
	 * Always restore %fprs and %y.
	 */
	 ldx	[%o0 + TD_FRAME], %o4
	ldx	[%o4 + TF_TSTATE], %o4
	andcc	%o4, TSTATE_PEF, %o4
	be,pt	%xcc, 2f
	 nop
	restrfp	%o1 + PCB_FPSTATE, %o4

2:	ldx	[%o1 + PCB_FPSTATE + FP_FPRS], %o4
	wr	%o4, 0, %fprs
	ldx	[%o1 + PCB_Y], %o4
	wr	%o4, 0, %y

	/*
	 * Point to the current process's vmspace and load the hardware
	 * context number.  If its the same as the new process, we are
	 * done.
	 */
	ldx	[%l0 + TD_PROC], %l1
	ldx	[%l1 + P_VMSPACE], %l1
	lduw	[%l1 + VM_PMAP + PM_CONTEXT], %l3
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: from=%p vm=%p context=%#x"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%l0, [%g1 + KTR_PARM1]
	stx	%l1, [%g1 + KTR_PARM2]
	stx	%l3, [%g1 + KTR_PARM3]
9:
#endif
	cmp	%l3, %o3
	be,pn	%xcc, 3f
	 EMPTY

	/*
	 * Install the new primary context.
	 */
	mov	AA_DMMU_PCXR, %o1
	stxa	%o3, [%o1] ASI_DMMU
	flush	%o0

	/*
	 * Map the primary user tsb.
	 */
	setx	TSB_USER_MIN_ADDRESS, %o1, %o0
	mov	AA_DMMU_TAR, %o1
	stxa	%o0, [%o1] ASI_DMMU
	mov	TLB_DAR_TSB_USER_PRIMARY, %o1
	ldx	[%o2 + VM_PMAP + PM_STTE + TTE_DATA], %o3
	stxa	%o3, [%o1] ASI_DTLB_DATA_ACCESS_REG
	membar	#Sync

	/*
	 * If the primary tsb page hasn't been initialized, initialize it
	 * and update the bit in the tte.
	 */
	andcc	%o3, TD_INIT, %g0
	bnz	%xcc, 3f
	 or	%o3, TD_INIT, %o3
	stx	%o3, [%o2 + VM_PMAP + PM_STTE + TTE_DATA]
	call	tsb_page_init
	 clr	%o1

	/*
	 * Done.  Return and load the new process's window from the stack.
	 */
3:
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: return td=%p (%s)"
	    , %g1, %g2, %g3, 7, 8, 9)
	ldx	[PCPU(CURTHREAD)], %g2
	stx	%g2, [%g1 + KTR_PARM1]
	ldx	[%g2 + TD_PROC], %g2
	add	%g2, P_COMM, %g3
	stx	%g3, [%g1 + KTR_PARM2]
9:
#endif
	ret
	 restore
END(cpu_switch)

ENTRY(savectx)
	save	%sp, -CCFSZ, %sp
	flushw
	rd	%y, %l0
	stx	%l0, [%i0 + PCB_Y]
	rd	%fprs, %l0
	stx	%l0, [%i0 + PCB_FPSTATE + FP_FPRS]
	ldx	[PCPU(CURTHREAD)], %l0
	ldx	[%l0 + TD_FRAME], %l0
	ldx	[%l0 + TF_TSTATE], %l0
	andcc	%l0, TSTATE_PEF, %l0
	be,pt	%xcc, 1f
	 stx	%fp, [%i0 + PCB_FP]
	add	%i0, PCB_FPSTATE, %o0
	call	savefpctx
1:	 stx	%i7, [%i0 + PCB_PC]
	ret
	 restore %g0, 0, %o0
END(savectx)

/* Note: this does not save %fprs. */
ENTRY(savefpctx)
	rd	%fprs, %o2
	savefp	%o0, %o1, %o2
	retl
	 wr	%o2, 0, %fprs
END(savefpctx)

ENTRY(restorefpctx)
	restrfp	%o0, %o1
	ldx	[%o0 + FP_FPRS], %o1
	retl
	 wr	%o1, 0, %fprs
END(restorefpctx)
