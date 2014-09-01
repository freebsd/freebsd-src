/*-
 * Copyright (c) 2014 Andrew Turner
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
 */

#include "assym.s"

#include <machine/asm.h>

__FBSDID("$FreeBSD$");

.Lcurpcpu:
	.quad	_C_LABEL(__pcpu)
	.quad	PCPU_SIZE

/*
 * void cpu_throw(struct thread *old, struct thread *new)
 */
ENTRY(cpu_throw)
	adr	x0, .Lcpu_throw_panic_str
	bl	_C_LABEL(panic)
	ret
.Lcpu_throw_panic_str:
	.asciz "cpu_throw\0\0"
END(cpu_throw)

/*
 * void cpu_switch(struct thread *old, struct thread *new, struct mtx *mtx)
 *
 * x0 = old
 * x1 = new
 * x2 = mtx
 * x3 to x7, x16 and x17 are caller saved
 */
ENTRY(cpu_switch)
#ifdef SMP
#error cpu_switch needs to be ported to support SMP
#endif

	/* Load the PCPU area */
	/* TODO: Adjust for the core we are on */
	ldr	x3, .Lcurpcpu

	/* Store the new curthread */
	str	x1, [x3, #PC_CURTHREAD]
	msr	tpidr_el1, x1
	/* And the new pcb */
	ldr	x4, [x1, #TD_PCB]
	str	x4, [x3, #PC_CURPCB]

	/*
	 * Save the old context.
	 */
	ldr	x4, [x0, #TD_PCB]

	/* Store the callee-saved registers */
	stp	x8, x9, [x4, #PCB_REGS + 8 * 8]
	stp	x10, x11, [x4, #PCB_REGS + 10 * 8]
	stp	x12, x13, [x4, #PCB_REGS + 12 * 8]
	stp	x14, x15, [x4, #PCB_REGS + 14 * 8]
	stp	x16, x17, [x4, #PCB_REGS + 16 * 8]
	stp	x18, x19, [x4, #PCB_REGS + 18 * 8]
	stp	x20, x21, [x4, #PCB_REGS + 20 * 8]
	stp	x22, x23, [x4, #PCB_REGS + 22 * 8]
	stp	x24, x25, [x4, #PCB_REGS + 24 * 8]
	stp	x26, x27, [x4, #PCB_REGS + 26 * 8]
	stp	x28, x29, [x4, #PCB_REGS + 28 * 8]
	str	x30, [x4, #PCB_REGS + 30 * 8]
	/* And the old stack pointer */
	mov	x5, sp
	mrs	x6, tpidr_el0
	stp	x5, x6, [x4, #PCB_SP]

	/*
	 * Restore the saved context.
	 */
	ldr	x4, [x1, #TD_PCB]

	/*
	 * TODO: We may need to flush the cache here if switching
	 * to a user process.
	 */

	/* Switch to the new pmap */
	ldr	x5, [x4, #PCB_L1ADDR]
	msr	ttbr0_el1, x5
	isb

	/* Invalidate the TLB */
	tlbi	vmalle1is

	/* Release the old thread */
	str	x2, [x0, #TD_LOCK]
#if defined(SCHED_ULE) && defined(SMP)
#error We may need to wait for the lock here
#endif

	/* Restore the registers */
	ldp	x5, x6, [x4, #PCB_SP]
	mov	sp, x5
	msr	tpidr_el0, x6
	ldp	x8, x9, [x4, #PCB_REGS + 8 * 8]
	ldp	x10, x11, [x4, #PCB_REGS + 10 * 8]
	ldp	x12, x13, [x4, #PCB_REGS + 12 * 8]
	ldp	x14, x15, [x4, #PCB_REGS + 14 * 8]
	ldp	x16, x17, [x4, #PCB_REGS + 16 * 8]
	ldp	x18, x19, [x4, #PCB_REGS + 18 * 8]
	ldp	x20, x21, [x4, #PCB_REGS + 20 * 8]
	ldp	x22, x23, [x4, #PCB_REGS + 22 * 8]
	ldp	x24, x25, [x4, #PCB_REGS + 24 * 8]
	ldp	x26, x27, [x4, #PCB_REGS + 26 * 8]
	ldp	x28, x29, [x4, #PCB_REGS + 28 * 8]
	ldr	x30, [x4, #PCB_REGS + 30 * 8]

	str	xzr, [x4, #PCB_REGS + 18 * 8]
	ret
.Lcpu_switch_panic_str:
	.asciz "cpu_switch: %p\0"
END(cpu_switch)

ENTRY(fork_trampoline)
	mov	x0, x8
	mov	x1, x9
	mov	x2, sp
	bl	_C_LABEL(fork_exit)

	/* Back up the stack pointer */
	mov	x0, sp

	/* Restore sp and lr */
	ldp	x2, x3, [x0]
	msr	sp_el0, x2
	mov	lr, x3

	/* Restore elr and spsr */
	ldp	x2, x3, [x0, #16]
	msr	elr_el1, x2
	msr	spsr_el1, x3

	/* Restore the registers other than x0 and x1 */
	ldp	x2, x3, [x0, #TF_X + 2 * 8]
	ldp	x4, x5, [x0, #TF_X + 4 * 8]
	ldp	x6, x7, [x0, #TF_X + 6 * 8]
	ldp	x8, x9, [x0, #TF_X + 8 * 8]
	ldp	x10, x11, [x0, #TF_X + 10 * 8]
	ldp	x12, x13, [x0, #TF_X + 12 * 8]
	ldp	x14, x15, [x0, #TF_X + 14 * 8]
	ldp	x16, x17, [x0, #TF_X + 16 * 8]
	ldp	x18, x19, [x0, #TF_X + 18 * 8]
	ldp	x20, x21, [x0, #TF_X + 20 * 8]
	ldp	x22, x23, [x0, #TF_X + 22 * 8]
	ldp	x24, x25, [x0, #TF_X + 24 * 8]
	ldp	x26, x27, [x0, #TF_X + 26 * 8]
	ldp	x28, x29, [x0, #TF_X + 28 * 8]
	/* Skip x30 as it was restored above as lr */

	/* Finally x0 and x1 */
	ldp	x0, x1, [x0, #TF_X + 0 * 8]

	eret
	
END(fork_trampoline)

ENTRY(savectx)
	adr	x0, .Lsavectx_panic_str
	bl	panic
	ret
.Lsavectx_panic_str:
	.asciz "savectx"
END(savectx)

