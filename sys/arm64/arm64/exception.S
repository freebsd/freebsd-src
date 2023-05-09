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

#include <machine/asm.h>
#include <machine/armreg.h>
__FBSDID("$FreeBSD$");

#include "assym.inc"

	.text

/*
 * This is limited to 28 instructions as it's placed in the exception vector
 * slot that is 32 instructions long. We need one for the branch, and three
 * for the prologue.
 */
.macro	save_registers_head el
.if \el == 1
	mov	x18, sp
	stp	x0,  x1,  [sp, #(TF_X - TF_SIZE - 128)]!
.else
	stp	x0,  x1,  [sp, #(TF_X - TF_SIZE)]!
.endif
	stp	x2,  x3,  [sp, #(2  * 8)]
	stp	x4,  x5,  [sp, #(4  * 8)]
	stp	x6,  x7,  [sp, #(6  * 8)]
	stp	x8,  x9,  [sp, #(8  * 8)]
	stp	x10, x11, [sp, #(10 * 8)]
	stp	x12, x13, [sp, #(12 * 8)]
	stp	x14, x15, [sp, #(14 * 8)]
	stp	x16, x17, [sp, #(16 * 8)]
	stp	x18, x19, [sp, #(18 * 8)]
	stp	x20, x21, [sp, #(20 * 8)]
	stp	x22, x23, [sp, #(22 * 8)]
	stp	x24, x25, [sp, #(24 * 8)]
	stp	x26, x27, [sp, #(26 * 8)]
	stp	x28, x29, [sp, #(28 * 8)]
.if \el == 0
	mrs	x18, sp_el0
.endif
	mrs	x10, elr_el1
	mrs	x11, spsr_el1
	mrs	x12, esr_el1
	mrs	x13, far_el1
	stp	x18,  lr, [sp, #(TF_SP - TF_X)]!
	stp	x10, x11, [sp, #(TF_ELR)]
	stp	x12, x13, [sp, #(TF_ESR)]
	mrs	x18, tpidr_el1
.endm

.macro	save_registers el
	add	x29, sp, #(TF_SIZE)
.if \el == 0
#if defined(PERTHREAD_SSP)
	/* Load the SSP canary to sp_el0 */
	ldr	x1, [x18, #(PC_CURTHREAD)]
	add	x1, x1, #(TD_MD_CANARY)
	msr	sp_el0, x1
#endif

	/* Apply the SSBD (CVE-2018-3639) workaround if needed */
	ldr	x1, [x18, #PC_SSBD]
	cbz	x1, 1f
	mov	w0, #1
	blr	x1
1:

	ldr	x0, [x18, #PC_CURTHREAD]
	bl	ptrauth_exit_el0

	ldr	x0, [x18, #(PC_CURTHREAD)]
	bl	dbg_monitor_enter

	/* Unmask debug and SError exceptions */
	msr	daifclr, #(DAIF_D | DAIF_A)
.else
	/*
	 * Unmask debug and SError exceptions.
	 * For EL1, debug exceptions are conditionally unmasked in
	 * do_el1h_sync().
	 */
	msr	daifclr, #(DAIF_A)
.endif
.endm

.macro	restore_registers el
	/*
	 * Mask all exceptions, x18 may change in the interrupt exception
	 * handler.
	 */
	msr	daifset, #(DAIF_ALL)
.if \el == 0
	ldr	x0, [x18, #PC_CURTHREAD]
	mov	x1, sp
	bl	dbg_monitor_exit

	ldr	x0, [x18, #PC_CURTHREAD]
	bl	ptrauth_enter_el0

	/* Remove the SSBD (CVE-2018-3639) workaround if needed */
	ldr	x1, [x18, #PC_SSBD]
	cbz	x1, 1f
	mov	w0, #0
	blr	x1
1:
.endif
	ldp	x18,  lr, [sp, #(TF_SP)]
	ldp	x10, x11, [sp, #(TF_ELR)]
.if \el == 0
	msr	sp_el0, x18
.endif
	msr	spsr_el1, x11
	msr	elr_el1, x10
	ldp	x0,  x1,  [sp, #(TF_X + 0  * 8)]
	ldp	x2,  x3,  [sp, #(TF_X + 2  * 8)]
	ldp	x4,  x5,  [sp, #(TF_X + 4  * 8)]
	ldp	x6,  x7,  [sp, #(TF_X + 6  * 8)]
	ldp	x8,  x9,  [sp, #(TF_X + 8  * 8)]
	ldp	x10, x11, [sp, #(TF_X + 10 * 8)]
	ldp	x12, x13, [sp, #(TF_X + 12 * 8)]
	ldp	x14, x15, [sp, #(TF_X + 14 * 8)]
	ldp	x16, x17, [sp, #(TF_X + 16 * 8)]
.if \el == 0
	/*
	 * We only restore the callee saved registers when returning to
	 * userland as they may have been updated by a system call or signal.
	 */
	ldp	x18, x19, [sp, #(TF_X + 18 * 8)]
	ldp	x20, x21, [sp, #(TF_X + 20 * 8)]
	ldp	x22, x23, [sp, #(TF_X + 22 * 8)]
	ldp	x24, x25, [sp, #(TF_X + 24 * 8)]
	ldp	x26, x27, [sp, #(TF_X + 26 * 8)]
	ldp	x28, x29, [sp, #(TF_X + 28 * 8)]
.else
	ldr	     x29, [sp, #(TF_X + 29 * 8)]
.endif
.if \el == 0
	add	sp, sp, #(TF_SIZE)
.else
	mov	sp, x18
	mrs	x18, tpidr_el1
.endif
.endm

.macro	do_ast
	mrs	x19, daif
	/* Make sure the IRQs are enabled before calling ast() */
	bic	x19, x19, #PSR_I
1:
	/*
	 * Mask interrupts while checking the ast pending flag
	 */
	msr	daifset, #(DAIF_INTR)

	/* Read the current thread AST mask */
	ldr	x1, [x18, #PC_CURTHREAD]	/* Load curthread */
	ldr	w1, [x1, #(TD_AST)]

	/* Check if we have a non-zero AST mask */
	cbz	w1, 2f

	/* Restore interrupts */
	msr	daif, x19

	/* handle the ast */
	mov	x0, sp
	bl	_C_LABEL(ast)

	/* Re-check for new ast scheduled */
	b	1b
2:
.endm

ENTRY(handle_el1h_sync)
	save_registers 1
	ldr	x0, [x18, #PC_CURTHREAD]
	mov	x1, sp
	bl	do_el1h_sync
	restore_registers 1
	ERET
END(handle_el1h_sync)

ENTRY(handle_el1h_irq)
	save_registers 1
	mov	x0, sp
	bl	intr_irq_handler
	restore_registers 1
	ERET
END(handle_el1h_irq)

ENTRY(handle_el0_sync)
	save_registers 0
	ldr	x0, [x18, #PC_CURTHREAD]
	mov	x1, sp
	str	x1, [x0, #TD_FRAME]
	bl	do_el0_sync
	do_ast
	restore_registers 0
	ERET
END(handle_el0_sync)

ENTRY(handle_el0_irq)
	save_registers 0
	mov	x0, sp
	bl	intr_irq_handler
	do_ast
	restore_registers 0
	ERET
END(handle_el0_irq)

ENTRY(handle_serror)
	save_registers 0
	mov	x0, sp
1:	bl	do_serror
	b	1b
END(handle_serror)

ENTRY(handle_empty_exception)
	save_registers 0
	mov	x0, sp
1:	bl	unhandled_exception
	b	1b
END(handle_empty_exception)

.macro	vector	name, el
	.align 7
	save_registers_head \el
	b	handle_\name
	dsb	sy
	isb
	/* Break instruction to ensure we aren't executing code here. */
	brk	0x42
.endm

.macro	vempty el
	vector	empty_exception \el
.endm

	.align 11
	.globl exception_vectors
exception_vectors:
	vempty 1		/* Synchronous EL1t */
	vempty 1		/* IRQ EL1t */
	vempty 1		/* FIQ EL1t */
	vempty 1		/* Error EL1t */

	vector el1h_sync 1	/* Synchronous EL1h */
	vector el1h_irq 1	/* IRQ EL1h */
	vempty 1		/* FIQ EL1h */
	vector serror 1		/* Error EL1h */

	vector el0_sync 0	/* Synchronous 64-bit EL0 */
	vector el0_irq 0	/* IRQ 64-bit EL0 */
	vempty 0		/* FIQ 64-bit EL0 */
	vector serror 0		/* Error 64-bit EL0 */

	vector el0_sync 0	/* Synchronous 32-bit EL0 */
	vector el0_irq 0	/* IRQ 32-bit EL0 */
	vempty 0		/* FIQ 32-bit EL0 */
	vector serror 0		/* Error 32-bit EL0 */

