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

	.register	%g2, #ignore
	.register	%g3, #ignore

#include "assym.s"

/*
 * void cpu_throw(struct thread *old, struct thread *new)
 */
ENTRY(cpu_throw)
	save	%sp, -CCFSZ, %sp
	flushw
	ba	%xcc, .Lsw1
	 mov	%i1, %i0
END(cpu_throw)

/*
 * void cpu_switch(struct thread *old, struct thread *new)
 */
ENTRY(cpu_switch)
	save	%sp, -CCFSZ, %sp
	mov	%i1, %i0

	/*
	 * If the current thread was using floating point in the kernel, save
	 * its context.  The userland floating point context has already been
	 * saved.
	 */
	rd	%fprs, %l2
	andcc	%l2, FPRS_FEF, %g0
	bz,a,pt %xcc, 1f
	 nop
	call	savefpctx
	 add	PCB_REG, PCB_KFP, %o0
	ba,a	%xcc, 2f
	 nop

	/*
	 * If the current thread was using floating point in userland, save
	 * its context.
	 */
1:	sub	PCB_REG, TF_SIZEOF, %l2
	ldx	[%l2 + TF_FPRS], %l3
	andcc	%l3, FPRS_FEF, %g0
	bz,a,pt	%xcc, 2f
	 nop
	call	savefpctx
	 add	PCB_REG, PCB_UFP, %o0
	andn	%l3, FPRS_FEF, %l3
	stx	%l3, [%l2 + TF_FPRS]

	ldx	[PCB_REG + PCB_FLAGS], %l3
	or	%l3, PCB_FEF, %l3
	stx	%l3, [PCB_REG + PCB_FLAGS]

	/*
	 * Flush the windows out to the stack and save the current frame
	 * pointer and program counter.
	 */
2:	flushw
	wrpr	%g0, 0, %cleanwin
	stx	%fp, [PCB_REG + PCB_SP]
	stx	%i7, [PCB_REG + PCB_PC]

	/*
	 * Load the new thread's frame pointer and program counter, and set
	 * the current thread and pcb.
	 */
.Lsw1:
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: new td=%p pc=%#lx fp=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%i0, [%g1 + KTR_PARM1]
	ldx	[%i0 + TD_PCB], %g2
	ldx	[%g2 + PCB_PC], %g3
	stx	%g3, [%g1 + KTR_PARM2]
	ldx	[%g2 + PCB_SP], %g3
	stx	%g3, [%g1 + KTR_PARM3]
9:
#endif
	ldx	[%i0 + TD_PCB], %i1

	stx	%i0, [PCPU(CURTHREAD)]
	stx	%i1, [PCPU(CURPCB)]

	wrpr	%g0, PSTATE_NORMAL, %pstate
	mov	%i1, PCB_REG
	wrpr	%g0, PSTATE_ALT, %pstate
	mov	%i1, PCB_REG
	wrpr	%g0, PSTATE_KERNEL, %pstate

	ldx	[PCB_REG + PCB_SP], %fp
	ldx	[PCB_REG + PCB_PC], %i7
	sub	%fp, CCFSZ, %sp

	SET(sched_lock, %i3, %i2)
	stx	%i0, [%i2 + MTX_LOCK]

	/*
	 * Point to the vmspaces of the new process, and of the last non-kernel
	 * process to run.
	 */
	ldx	[%i0 + TD_PROC], %i2
	ldx	[PCPU(VMSPACE)], %l2
	ldx	[%i2 + P_VMSPACE], %i2

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: new vm=%p old vm=%p"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%i2, [%g1 + KTR_PARM1]
	stx	%l2, [%g1 + KTR_PARM2]
9:
#endif

	/*
	 * If they are the same we are done.
	 */
	cmp	%l2, %i2
	be,a,pn %xcc, 5f
	 nop

	/*
	 * If the new process has nucleus context we are done.
	 */
	lduw	[PCPU(CPUID)], %i3
	sllx	%i3, INT_SHIFT, %i3
	add	%i2, VM_PMAP + PM_CONTEXT, %i4
	lduw	[%i3 + %i4], %i5

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: ctx=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%i5, [%g1 + KTR_PARM1]
9:
#endif

	brz,a,pn %i5, 5f
	 nop

	/*
	 * If there was no non-kernel vmspace, don't try to deactivate it.
	 */
	brz,a,pn %l2, 3f
	 nop

	/*
	 * Mark the pmap of the last non-kernel vmspace to run as no longer
	 * active on this cpu.
	 */
	lduw	[%l2 + VM_PMAP + PM_ACTIVE], %l3
	lduw	[PCPU(CPUMASK)], %l4
	andn	%l3, %l4, %l3
	stw	%l3, [%l2 + VM_PMAP + PM_ACTIVE]

	/*
	 * Take away its context.
	 */
	lduw	[PCPU(CPUID)], %l3
	sllx	%l3, INT_SHIFT, %l3
	add	%l2, VM_PMAP + PM_CONTEXT, %l4
	mov	-1, %l5
	stw	%l5, [%l3 + %l4]

	/*
	 * Find the current free tlb context for this cpu and install it as
	 * the new primary context.
	 */
3:	lduw	[PCPU(TLB_CTX)], %i5
	stw	%i5, [%i3 + %i4]
	mov	AA_DMMU_PCXR, %i4
	stxa	%i5, [%i4] ASI_DMMU
	membar	#Sync

	/*
	 * See if we have run out of free contexts.
	 */
	lduw	[PCPU(TLB_CTX_MAX)], %i3

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: ctx=%#lx next=%#lx max=%#lx"
	    , %g1, %g2, %g3, 7, 8, 9)
	stx	%i5, [%g1 + KTR_PARM1]
	add	%i5, 1, %g2
	stx	%g2, [%g1 + KTR_PARM2]
	stx	%i3, [%g1 + KTR_PARM3]
9:
#endif

	add	%i5, 1, %i5
	cmp	%i3, %i5
	bne,a,pt %xcc, 4f
	 stw	%i5, [PCPU(TLB_CTX)]

#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: context rollover"
	    , %g1, %g2, %g3, 7, 8, 9)
9:
#endif

	/*
	 * We will start re-using contexts on the next switch.  Flush all
	 * non-nucleus mappings from the tlb, and reset the next free context.
	 */
	call	pmap_context_rollover
	 nop

	/*
	 * Mark the pmap as active on this cpu.
	 */
4:	lduw	[%i2 + VM_PMAP + PM_ACTIVE], %i3
	lduw	[PCPU(CPUMASK)], %i4
	or	%i3, %i4, %i3
	stw	%i3, [%i2 + VM_PMAP + PM_ACTIVE]

	/*
	 * Make note of the change in vmspace.
	 */
	stx	%i2, [PCPU(VMSPACE)]

	/*
	 * Load the tsb registers.
	 */
	ldx	[%i2 + VM_PMAP + PM_TSB], %i3
	mov	AA_DMMU_TSB, %i4
	stxa	%i3, [%i4] ASI_DMMU
	mov	AA_IMMU_TSB, %i4
	stxa	%i3, [%i4] ASI_IMMU
	membar	#Sync

5:
#if KTR_COMPILE & KTR_PROC
	CATR(KTR_PROC, "cpu_switch: return"
	    , %g1, %g2, %g3, 7, 8, 9)
9:
#endif

	/*
	 * Done.  Return and load the new process's window from the stack.
	 */
	ret
	 restore
END(cpu_switch)

ENTRY(savectx)
	save	%sp, -CCFSZ, %sp
	flushw
	call	savefpctx
	 add	%i0, PCB_UFP, %o0
	stx	%fp, [%i0 + PCB_SP]
	stx	%i7, [%i0 + PCB_PC]
	ret
	 restore %g0, 0, %o0
END(savectx)

/*
 * void savefpctx(uint32_t *);
 */
ENTRY(savefpctx)
	wr	%g0, FPRS_FEF, %fprs
	wr	%g0, ASI_BLK_S, %asi
	stda	%f0, [%o0 + (0 * 64)] %asi
	stda	%f16, [%o0 + (1 * 64)] %asi
	stda	%f32, [%o0 + (2 * 64)] %asi
	stda	%f48, [%o0 + (3 * 64)] %asi
	membar	#Sync
	retl
	 wr	%g0, 0, %fprs
END(savefpctx)
