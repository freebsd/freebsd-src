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

#include "assym.s"

/*
 * Save and restore FPU state. This is done at switch time.
 * We could use FPRS_DL and FPRS_DU; however, it is accessible to non-privileged
 * software, so it is avoided for compatabilities sake.
 * savefp clobbers %fprs.
 */
	.macro	savefp	state, tmp
	rd	%fprs, \tmp
	stx	\tmp, [\state + FP_FPRS]
	or	\tmp, FPRS_FEF, \tmp
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
	ldx	[\state + FP_FPRS], \tmp
	wr	\tmp, 0, %fprs
	.endm

ENTRY(cpu_switch)
	/*
	 * Choose a new process.  If its the same as the current one, do
	 * nothing.
	 */
	save	%sp, -CCFSZ, %sp
	call	chooseproc
	 ldx	[PCPU(CURPROC)], %l0
	cmp	%l0, %o0
	be,pn	%xcc, 3f
	 EMPTY

	/*
	 * If the process was using floating point, save its context.
	 */
	 ldx	[%l0 + P_FRAME], %l1
	ldx	[%l1 + TF_TSTATE], %l1
	andcc	%l1, TSTATE_PEF, %l1
	be,pt	%xcc, 1f
	 ldx	[PCPU(CURPCB)], %l2
	savefp	%l2 + PCB_FPSTATE, %l3

	/*
	 * Flush the windows out to the stack and save the current frame
	 * pointer and program counter.
	 */
1:	flushw
	wrpr	%g0, 0, %cleanwin
	stx	%fp, [%l2 + PCB_FP]
	stx	%i7, [%l2 + PCB_PC]

	/*
	 * Load the new process's frame pointer and program counter, and set
	 * the current process and pcb.
	 */
	ldx	[%o0 + P_ADDR], %o1
	ldx	[%o1 + U_PCB + PCB_FP], %fp
	ldx	[%o1 + U_PCB + PCB_PC], %i7
	sub	%fp, CCFSZ, %sp
	stx	%o0, [PCPU(CURPROC)]
	stx	%o1, [PCPU(CURPCB)]

	/*
	 * Point to the new process's vmspace and load its vm context number.
	 * If its nucleus context we are done.
	 */
	ldx	[%o0 + P_VMSPACE], %o2
	lduw	[%o2 + VM_PMAP + PM_CONTEXT], %o3
	brz,pn	%o3, 3f
	 EMPTY

	/*
	 * If the new process was using floating point, restore its context.
	 */
	 ldx	[%o0 + P_FRAME], %o4
	ldx	[%o4 + TF_TSTATE], %o4
	andcc	%o4, TSTATE_PEF, %o4
	be,pt	%xcc, 2f
	 nop
	restrfp	%o1 + U_PCB + PCB_FPSTATE, %o4

	/*
	 * Point to the current process's vmspace and load the hardware
	 * context number.  If its the same as the new process, we are
	 * done.
	 */
	ldx	[%l0 + P_VMSPACE], %l1
	lduw	[%l1 + VM_PMAP + PM_CONTEXT], %l3
	cmp	%l3, %o3
	be,pn	%xcc, 3f
	 EMPTY

	/*
	 * Install the new primary context.
	 */
2:	mov	AA_DMMU_PCXR, %o1
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
3:	ret
	 restore
END(cpu_switch)

ENTRY(savectx)
	save	%sp, -CCFSZ, %sp
	flushw
	ldx	[PCPU(CURPROC)], %l0
	ldx	[%l0 + P_FRAME], %l0
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

ENTRY(savefpctx)
	rd	%fprs, %o2
	savefp	%o0, %o1
	retl
	 wr	%o2, 0, %fprs
END(savefpctx)

ENTRY(restorefpctx)
	restrfp	%o0, %o1
	retl
	 nop
END(restorefpctx)
