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

ENTRY(cpu_throw)
	save	%sp, -CCFSZ, %sp
	call	choosethread
	 ldx	[PCPU(CURTHREAD)], %l0
	flushw
	b,a	%xcc, .Lsw1
	 nop
END(cpu_throw)

ENTRY(cpu_switch)
	/*
	 * Choose a new thread.  If its the same as the current one, do
	 * nothing.
	 */
	save	%sp, -CCFSZ, %sp
	call	choosethread
	 ldx	[PCPU(CURTHREAD)], %l0
	cmp	%l0, %o0
	be,a,pn	%xcc, 4f
	 nop
	ldx	[%l0 + TD_PCB], %l1

	/*
	 * If the current thread was using floating point, save its context.
	 */
	ldx	[%l0 + TD_FRAME], %l2
	ldub	[%l2 + TF_FPRS], %l3
	andcc	%l3, FPRS_FEF, %g0
	bz,a,pt	%xcc, 1f
	 nop
	wr	%g0, FPRS_FEF, %fprs
	wr	%g0, ASI_BLK_S, %asi
	stda	%f0, [%l1 + PCB_FPSTATE + FP_FB0] %asi
	stda	%f16, [%l1 + PCB_FPSTATE + FP_FB1] %asi
	stda	%f32, [%l1 + PCB_FPSTATE + FP_FB2] %asi
	stda	%f48, [%l1 + PCB_FPSTATE + FP_FB3] %asi
	membar	#Sync
	wr	%g0, 0, %fprs
	andn	%l3, FPRS_FEF, %l3
	stb	%l3, [%l2 + TF_FPRS]

	/*
	 * Flush the windows out to the stack and save the current frame
	 * pointer and program counter.
	 */
1:	flushw
	wrpr	%g0, 0, %cleanwin
	stx	%fp, [%l1 + PCB_FP]
	stx	%i7, [%l1 + PCB_PC]

	/*
	 * Load the new thread's frame pointer and program counter, and set
	 * the current thread and pcb.
	 */
.Lsw1:
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: td=%d (%s) pc=%#lx fp=%#lx"
	    , %l3, %l4, %l5, 7, 8, 9)
	ldx	[%o0 + TD_PROC], %l4
	lduw	[%l4 + P_PID], %l5
	stx	%l5, [%l3 + KTR_PARM1]
	add	%l4, P_COMM, %l5
	stx	%l5, [%l3 + KTR_PARM2]
	ldx	[%o0 + TD_PCB], %l4
	ldx	[%l4 + PCB_PC], %l5
	stx	%l5, [%l3 + KTR_PARM3]
	ldx	[%l4 + PCB_FP], %l5
	stx	%l5, [%l3 + KTR_PARM4]
9:
#endif
	ldx	[%o0 + TD_PCB], %o1
	ldx	[%o1 + PCB_FP], %fp
	ldx	[%o1 + PCB_PC], %i7
	sub	%fp, CCFSZ, %sp
	stx	%o0, [PCPU(CURTHREAD)]
	stx	%o1, [PCPU(CURPCB)]

	wrpr	%g0, PSTATE_NORMAL, %pstate
	mov	%o1, PCB_REG
	wrpr	%g0, PSTATE_ALT, %pstate
	mov	%o1, PCB_REG
	wrpr	%g0, PSTATE_KERNEL, %pstate

	/*
	 * Point to the vmspaces of the new and old processes.
	 */
2:	ldx	[%l0 + TD_PROC], %l2
	ldx	[%o0 + TD_PROC], %o2
	ldx	[%l2 + P_VMSPACE], %l2
	ldx	[%o2 + P_VMSPACE], %o2

	/*
	 * If they're the same we are done.
	 */
	cmp	%l2, %o2
	be,a,pn %xcc, 4f
	 nop

	/*
	 * If the old process has nucleus context we can skip demapping the
	 * tsb.
	 */
	lduw	[PCPU(CPUID)], %l3
	sllx	%l3, INT_SHIFT, %l3
	add	%l2, VM_PMAP + PM_CONTEXT, %l4
	lduw	[%l3 + %l4], %l5
	brz,a,pn %l5, 3f
	 nop

	/*
	 * Demap the old process's tsb.
	 */
	ldx	[%l2 + VM_PMAP + PM_TSB], %l3
	or	%l3, TLB_DEMAP_NUCLEUS | TLB_DEMAP_PAGE, %l3
	stxa	%g0, [%l3] ASI_DMMU_DEMAP
	membar	#Sync

	/*
	 * Mark the pmap no longer active on this cpu.
	 */
	lduw	[%l2 + VM_PMAP + PM_ACTIVE], %l3
	mov	1, %l4
	lduw	[PCPU(CPUID)], %l5
	sllx	%l4, %l5, %l4
	andn	%l3, %l4, %l3
	stw	%l3, [%l2 + VM_PMAP + PM_ACTIVE]

	/*
	 * If the new process has nucleus context we are done.
	 */
3:	lduw	[PCPU(CPUID)], %o3
	sllx	%o3, INT_SHIFT, %o3
	add	%o2, VM_PMAP + PM_CONTEXT, %o4
	lduw	[%o3 + %o4], %o5
	brz,a,pn %o5, 4f
	 nop

	/*
	 * Install the new primary context.
	 */
	mov	AA_DMMU_PCXR, %o4
	stxa	%o5, [%o4] ASI_DMMU
	flush	%o0

	/*
	 * Mark the pmap as active on this cpu.
	 */
	lduw	[%o2 + VM_PMAP + PM_ACTIVE], %o3
	mov	1, %o4
	lduw	[PCPU(CPUID)], %o5
	sllx	%o4, %o5, %o4
	or	%o3, %o4, %o3
	stw	%o3, [%o2 + VM_PMAP + PM_ACTIVE]

	/*
	 * Switch to mmu globals and install the preloaded tsb pointer.
	 */
	ldx	[%o2 + VM_PMAP + PM_TSB], %o3
	wrpr	%g0, PSTATE_MMU, %pstate
	mov	%o3, TSB_REG
	wrpr	%g0, PSTATE_KERNEL, %pstate

	/*
	 * Done.  Return and load the new process's window from the stack.
	 */
4:	ret
	 restore
END(cpu_switch)

ENTRY(savectx)
	save	%sp, -CCFSZ, %sp
	flushw
	call	savefpctx
	 mov	%i0, %o0
	stx	%fp, [%i0 + PCB_FP]
	stx	%i7, [%i0 + PCB_PC]
	ret
	 restore %g0, 0, %o0
END(savectx)

ENTRY(savefpctx)
	wr	%g0, FPRS_FEF, %fprs
	wr	%g0, ASI_BLK_S, %asi
	stda	%f0, [%o0 + PCB_FPSTATE + FP_FB0] %asi
	stda	%f16, [%o0 + PCB_FPSTATE + FP_FB1] %asi
	stda	%f32, [%o0 + PCB_FPSTATE + FP_FB2] %asi
	stda	%f48, [%o0 + PCB_FPSTATE + FP_FB3] %asi
	membar	#Sync
	retl
	 wr	%g0, 0, %fprs
END(savefpctx)

ENTRY(restorefpctx)
	wr	%g0, FPRS_FEF, %fprs
	wr	%g0, ASI_BLK_S, %asi
	ldda	[%o0 + PCB_FPSTATE + FP_FB0] %asi, %f0
	ldda	[%o0 + PCB_FPSTATE + FP_FB1] %asi, %f16
	ldda	[%o0 + PCB_FPSTATE + FP_FB2] %asi, %f32
	ldda	[%o0 + PCB_FPSTATE + FP_FB3] %asi, %f48
	membar	#Sync
	retl
	 wr	%g0, 0, %fprs
END(restorefpctx)
