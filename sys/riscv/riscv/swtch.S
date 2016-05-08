/*-
 * Copyright (c) 2015-2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
 */

#include "assym.s"
#include "opt_sched.h"

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/riscvreg.h>
#include <machine/pte.h>

__FBSDID("$FreeBSD$");

/*
 * void cpu_throw(struct thread *old, struct thread *new)
 */
ENTRY(cpu_throw)
	/* Store the new curthread */
	sd	a1, PC_CURTHREAD(gp)
	/* And the new pcb */
	ld	x13, TD_PCB(a1)
	sd	x13, PC_CURPCB(gp)

	sfence.vm

	/* Switch to the new pmap */
	ld	t0, PCB_L1ADDR(x13)
	csrw	sptbr, t0

	/* TODO: Invalidate the TLB */

	sfence.vm

	/* Load registers */
	ld	ra, (PCB_RA)(x13)
	ld	sp, (PCB_SP)(x13)

	/* s[0-11] */
	ld	s0, (PCB_S + 0 * 8)(x13)
	ld	s1, (PCB_S + 1 * 8)(x13)
	ld	s2, (PCB_S + 2 * 8)(x13)
	ld	s3, (PCB_S + 3 * 8)(x13)
	ld	s4, (PCB_S + 4 * 8)(x13)
	ld	s5, (PCB_S + 5 * 8)(x13)
	ld	s6, (PCB_S + 6 * 8)(x13)
	ld	s7, (PCB_S + 7 * 8)(x13)
	ld	s8, (PCB_S + 8 * 8)(x13)
	ld	s9, (PCB_S + 9 * 8)(x13)
	ld	s10, (PCB_S + 10 * 8)(x13)
	ld	s11, (PCB_S + 11 * 8)(x13)
	ret

.Lcpu_throw_panic_str:
	.asciz "cpu_throw: %p\0"
END(cpu_throw)

/*
 * void cpu_switch(struct thread *old, struct thread *new, struct mtx *mtx)
 *
 * a0 = old
 * a1 = new
 * a2 = mtx
 * x3 to x7, x16 and x17 are caller saved
 */
ENTRY(cpu_switch)
	/* Store the new curthread */
	sd	a1, PC_CURTHREAD(gp)
	/* And the new pcb */
	ld	x13, TD_PCB(a1)
	sd	x13, PC_CURPCB(gp)

	/* Save the old context. */
	ld	x13, TD_PCB(a0)

	/* Store ra, sp and the callee-saved registers */
	sd	ra, (PCB_RA)(x13)
	sd	sp, (PCB_SP)(x13)

	/* s[0-11] */
	sd	s0, (PCB_S + 0 * 8)(x13)
	sd	s1, (PCB_S + 1 * 8)(x13)
	sd	s2, (PCB_S + 2 * 8)(x13)
	sd	s3, (PCB_S + 3 * 8)(x13)
	sd	s4, (PCB_S + 4 * 8)(x13)
	sd	s5, (PCB_S + 5 * 8)(x13)
	sd	s6, (PCB_S + 6 * 8)(x13)
	sd	s7, (PCB_S + 7 * 8)(x13)
	sd	s8, (PCB_S + 8 * 8)(x13)
	sd	s9, (PCB_S + 9 * 8)(x13)
	sd	s10, (PCB_S + 10 * 8)(x13)
	sd	s11, (PCB_S + 11 * 8)(x13)

	/*
	 * Restore the saved context.
	 */
	ld	x13, TD_PCB(a1)

	/*
	 * TODO: We may need to flush the cache here if switching
	 * to a user process.
	 */

	sfence.vm

	/* Switch to the new pmap */
	ld	t0, PCB_L1ADDR(x13)
	csrw	sptbr, t0

	/* TODO: Invalidate the TLB */

	sfence.vm

	/* Release the old thread */
	sd	a2, TD_LOCK(a0)
#if defined(SCHED_ULE) && defined(SMP)
	/* Spin if TD_LOCK points to a blocked_lock */
	la	a2, _C_LABEL(blocked_lock)
1:
	ld	t0, TD_LOCK(a1)
	beq	t0, a2, 1b
#endif

	/* Restore the registers */
	ld	ra, (PCB_RA)(x13)
	ld	sp, (PCB_SP)(x13)

	/* s[0-11] */
	ld	s0, (PCB_S + 0 * 8)(x13)
	ld	s1, (PCB_S + 1 * 8)(x13)
	ld	s2, (PCB_S + 2 * 8)(x13)
	ld	s3, (PCB_S + 3 * 8)(x13)
	ld	s4, (PCB_S + 4 * 8)(x13)
	ld	s5, (PCB_S + 5 * 8)(x13)
	ld	s6, (PCB_S + 6 * 8)(x13)
	ld	s7, (PCB_S + 7 * 8)(x13)
	ld	s8, (PCB_S + 8 * 8)(x13)
	ld	s9, (PCB_S + 9 * 8)(x13)
	ld	s10, (PCB_S + 10 * 8)(x13)
	ld	s11, (PCB_S + 11 * 8)(x13)
	ret
.Lcpu_switch_panic_str:
	.asciz "cpu_switch: %p\0"
END(cpu_switch)

/*
 * fork_exit(void (*callout)(void *, struct trapframe *), void *arg,
 *  struct trapframe *frame)
 */

ENTRY(fork_trampoline)
	mv	a0, s0
	mv	a1, s1
	mv	a2, sp
	call	_C_LABEL(fork_exit)

	/* Restore sstatus */
	ld	t0, (TF_SSTATUS)(sp)
	/* Ensure interrupts disabled */
	li	t1, ~SSTATUS_IE
	and	t0, t0, t1
	csrw	sstatus, t0

	/* Restore exception program counter */
	ld	t0, (TF_SEPC)(sp)
	csrw	sepc, t0

	/* Restore the registers */
	ld	t0, (TF_T + 0 * 8)(sp)
	ld	t1, (TF_T + 1 * 8)(sp)
	ld	t2, (TF_T + 2 * 8)(sp)
	ld	t3, (TF_T + 3 * 8)(sp)
	ld	t4, (TF_T + 4 * 8)(sp)
	ld	t5, (TF_T + 5 * 8)(sp)
	ld	t6, (TF_T + 6 * 8)(sp)

	ld	s0, (TF_S + 0 * 8)(sp)
	ld	s1, (TF_S + 1 * 8)(sp)
	ld	s2, (TF_S + 2 * 8)(sp)
	ld	s3, (TF_S + 3 * 8)(sp)
	ld	s4, (TF_S + 4 * 8)(sp)
	ld	s5, (TF_S + 5 * 8)(sp)
	ld	s6, (TF_S + 6 * 8)(sp)
	ld	s7, (TF_S + 7 * 8)(sp)
	ld	s8, (TF_S + 8 * 8)(sp)
	ld	s9, (TF_S + 9 * 8)(sp)
	ld	s10, (TF_S + 10 * 8)(sp)
	ld	s11, (TF_S + 11 * 8)(sp)

	ld	a0, (TF_A + 0 * 8)(sp)
	ld	a1, (TF_A + 1 * 8)(sp)
	ld	a2, (TF_A + 2 * 8)(sp)
	ld	a3, (TF_A + 3 * 8)(sp)
	ld	a4, (TF_A + 4 * 8)(sp)
	ld	a5, (TF_A + 5 * 8)(sp)
	ld	a6, (TF_A + 6 * 8)(sp)
	ld	a7, (TF_A + 7 * 8)(sp)

	/* Load user ra and sp */
	ld	tp, (TF_TP)(sp)
	ld	ra, (TF_RA)(sp)

	/*
	 * Store our pcpup on stack, we will load it back
	 * on kernel mode trap.
	 */
	sd	gp, (TF_SIZE)(sp)
	ld	gp, (TF_GP)(sp)

	/* Save kernel stack so we can use it doing a user trap */
	addi	sp, sp, TF_SIZE
	csrw	sscratch, sp

	/* Load user stack */
	ld	sp, (TF_SP - TF_SIZE)(sp)

	eret
END(fork_trampoline)

ENTRY(savectx)
	/* Store ra, sp and the callee-saved registers */
	sd	ra, (PCB_RA)(a0)
	sd	sp, (PCB_SP)(a0)

	/* s[0-11] */
	sd	s0, (PCB_S + 0 * 8)(a0)
	sd	s1, (PCB_S + 1 * 8)(a0)
	sd	s2, (PCB_S + 2 * 8)(a0)
	sd	s3, (PCB_S + 3 * 8)(a0)
	sd	s4, (PCB_S + 4 * 8)(a0)
	sd	s5, (PCB_S + 5 * 8)(a0)
	sd	s6, (PCB_S + 6 * 8)(a0)
	sd	s7, (PCB_S + 7 * 8)(a0)
	sd	s8, (PCB_S + 8 * 8)(a0)
	sd	s9, (PCB_S + 9 * 8)(a0)
	sd	s10, (PCB_S + 10 * 8)(a0)
	sd	s11, (PCB_S + 11 * 8)(a0)

	/* Store the VFP registers */
#ifdef VFP
	/* TODO */
#endif
	ret
END(savectx)
