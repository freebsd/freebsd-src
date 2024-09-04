/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2017 Alexandru Elisei <alexandru.elisei@gmail.com>
 * Copyright (c) 2021 Andrew Turner
 *
 * This software was developed by Alexandru Elisei under sponsorship
 * from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sys/elf_common.h>
#include <machine/asm.h>
#include <machine/hypervisor.h>

#include "assym.inc"
#include "hyp.h"

.macro	save_host_registers
	/* TODO: Only store callee saved registers */
	sub	sp, sp, #(32 * 8)
	str	x30,      [sp, #(30 * 8)]
	stp	x28, x29, [sp, #(28 * 8)]
	stp	x26, x27, [sp, #(26 * 8)]
	stp	x24, x25, [sp, #(24 * 8)]
	stp	x22, x23, [sp, #(22 * 8)]
	stp	x20, x21, [sp, #(20 * 8)]
	stp	x18, x19, [sp, #(18 * 8)]
	stp	x16, x17, [sp, #(16 * 8)]
	stp	x14, x15, [sp, #(14 * 8)]
	stp	x12, x13, [sp, #(12 * 8)]
	stp	x10, x11, [sp, #(10 * 8)]
	stp	x8,  x9,  [sp, #(8  * 8)]
	stp	x6,  x7,  [sp, #(6  * 8)]
	stp	x4,  x5,  [sp, #(4  * 8)]
	stp	x2,  x3,  [sp, #(2  * 8)]
	stp	x0,  x1,  [sp, #(0  * 8)]
.endm

.macro	restore_host_registers
	/* TODO: Only restore callee saved registers */
	ldp	x0,  x1,  [sp, #(0  * 8)]
	ldp	x2,  x3,  [sp, #(2  * 8)]
	ldp	x4,  x5,  [sp, #(4  * 8)]
	ldp	x6,  x7,  [sp, #(6  * 8)]
	ldp	x8,  x9,  [sp, #(8  * 8)]
	ldp	x10, x11, [sp, #(10 * 8)]
	ldp	x12, x13, [sp, #(12 * 8)]
	ldp	x14, x15, [sp, #(14 * 8)]
	ldp	x16, x17, [sp, #(16 * 8)]
	ldp	x18, x19, [sp, #(18 * 8)]
	ldp	x20, x21, [sp, #(20 * 8)]
	ldp	x22, x23, [sp, #(22 * 8)]
	ldp	x24, x25, [sp, #(24 * 8)]
	ldp	x26, x27, [sp, #(26 * 8)]
	ldp	x28, x29, [sp, #(28 * 8)]
	ldr	x30,      [sp, #(30 * 8)]
	add	sp, sp, #(32 * 8)
.endm

.macro	save_guest_registers
	/* Back up x0 so we can use it as a temporary register */
	stp	x0,  x1,  [sp, #-(2 * 8)]!

	/* Restore the hypctx pointer */
	mrs	x0, tpidr_el2

	stp	x2,  x3,  [x0, #(TF_X + 2  * 8)]
	stp	x4,  x5,  [x0, #(TF_X + 4  * 8)]
	stp	x6,  x7,  [x0, #(TF_X + 6  * 8)]
	stp	x8,  x9,  [x0, #(TF_X + 8  * 8)]
	stp	x10, x11, [x0, #(TF_X + 10 * 8)]
	stp	x12, x13, [x0, #(TF_X + 12 * 8)]
	stp	x14, x15, [x0, #(TF_X + 14 * 8)]
	stp	x16, x17, [x0, #(TF_X + 16 * 8)]
	stp	x18, x19, [x0, #(TF_X + 18 * 8)]
	stp	x20, x21, [x0, #(TF_X + 20 * 8)]
	stp	x22, x23, [x0, #(TF_X + 22 * 8)]
	stp	x24, x25, [x0, #(TF_X + 24 * 8)]
	stp	x26, x27, [x0, #(TF_X + 26 * 8)]
	stp	x28, x29, [x0, #(TF_X + 28 * 8)]

	str	lr, [x0, #(TF_LR)]

	/* Restore the saved x0 & x1 and save them */
	ldp	x2,  x3,  [sp], #(2 * 8)
	stp	x2,  x3,  [x0, #(TF_X + 0  * 8)]
.endm

.macro	restore_guest_registers
	/*
	 * Copy the guest x0 and x1 to the stack so we can restore them
	 * after loading the other registers.
	 */
	ldp	x2,  x3,  [x0, #(TF_X + 0  * 8)]
	stp	x2,  x3,  [sp, #-(2 * 8)]!

	ldr	lr, [x0, #(TF_LR)]

	ldp	x28, x29, [x0, #(TF_X + 28 * 8)]
	ldp	x26, x27, [x0, #(TF_X + 26 * 8)]
	ldp	x24, x25, [x0, #(TF_X + 24 * 8)]
	ldp	x22, x23, [x0, #(TF_X + 22 * 8)]
	ldp	x20, x21, [x0, #(TF_X + 20 * 8)]
	ldp	x18, x19, [x0, #(TF_X + 18 * 8)]
	ldp	x16, x17, [x0, #(TF_X + 16 * 8)]
	ldp	x14, x15, [x0, #(TF_X + 14 * 8)]
	ldp	x12, x13, [x0, #(TF_X + 12 * 8)]
	ldp	x10, x11, [x0, #(TF_X + 10 * 8)]
	ldp	x8,  x9,  [x0, #(TF_X + 8  * 8)]
	ldp	x6,  x7,  [x0, #(TF_X + 6  * 8)]
	ldp	x4,  x5,  [x0, #(TF_X + 4  * 8)]
	ldp	x2,  x3,  [x0, #(TF_X + 2  * 8)]

	ldp	x0,  x1,  [sp], #(2 * 8)
.endm

.macro vempty
	.align 7
	1: b	1b
.endm

.macro vector name
	.align 7
	b	handle_\name
.endm

	.text
	.align 11
hyp_vectors:
	vempty			/* Synchronous EL2t */
	vempty			/* IRQ EL2t */
	vempty			/* FIQ EL2t */
	vempty			/* Error EL2t */

	vector el2_el2h_sync	/* Synchronous EL2h */
	vector el2_el2h_irq	/* IRQ EL2h */
	vector el2_el2h_fiq	/* FIQ EL2h */
	vector el2_el2h_error	/* Error EL2h */

	vector el2_el1_sync64	/* Synchronous 64-bit EL1 */
	vector el2_el1_irq64	/* IRQ 64-bit EL1 */
	vector el2_el1_fiq64	/* FIQ 64-bit EL1 */
	vector el2_el1_error64	/* Error 64-bit EL1 */

	vempty			/* Synchronous 32-bit EL1 */
	vempty			/* IRQ 32-bit EL1 */
	vempty			/* FIQ 32-bit EL1 */
	vempty			/* Error 32-bit EL1 */

.macro do_world_switch_to_host
	save_guest_registers
	restore_host_registers

	/* Restore host VTTBR */
	mov	x9, #VTTBR_HOST
	msr	vttbr_el2, x9

#ifdef VMM_VHE
	msr	vbar_el1, x1
#endif
.endm


.macro handle_el2_excp type
#ifndef VMM_VHE
	/* Save registers before modifying so we can restore them */
	str	x9, [sp, #-16]!

	/* Test if the exception happened when the host was running */
	mrs	x9, vttbr_el2
	cmp	x9, #VTTBR_HOST
	beq	1f

	/* We got the exception while the guest was running */
	ldr	x9, [sp], #16
#endif /* !VMM_VHE */
	do_world_switch_to_host
	mov	x0, \type
	ret

#ifndef VMM_VHE
1:
	/* We got the exception while the host was running */
	ldr	x9, [sp], #16
	mov	x0, \type
	ERET
#endif /* !VMM_VHE */
.endm


LENTRY(handle_el2_el2h_sync)
	handle_el2_excp #EXCP_TYPE_EL2_SYNC
LEND(handle_el2_el2h_sync)

LENTRY(handle_el2_el2h_irq)
	handle_el2_excp #EXCP_TYPE_EL2_IRQ
LEND(handle_el2_el2h_irq)

LENTRY(handle_el2_el2h_fiq)
	handle_el2_excp #EXCP_TYPE_EL2_FIQ
LEND(handle_el2_el2h_fiq)

LENTRY(handle_el2_el2h_error)
	handle_el2_excp #EXCP_TYPE_EL2_ERROR
LEND(handle_el2_el2h_error)


LENTRY(handle_el2_el1_sync64)
#ifndef VMM_VHE
	/* Save registers before modifying so we can restore them */
	str	x9, [sp, #-16]!

	/* Check for host hypervisor call */
	mrs	x9, vttbr_el2
	cmp	x9, #VTTBR_HOST
	ldr	x9, [sp], #16 /* Restore the temp register */
	bne	1f

	/*
	 * Called from the host
	 */

	/* Check if this is a cleanup call and handle in a controlled state */
	cmp	x0, #(HYP_CLEANUP)
	b.eq	vmm_cleanup

	str	lr, [sp, #-16]!
	bl	vmm_hyp_enter
	ldr	lr, [sp], #16
	ERET

1:
#endif
	/* Guest exception taken to EL2 */
	do_world_switch_to_host
	mov	x0, #EXCP_TYPE_EL1_SYNC
	ret
LEND(handle_el2_el1_sync64)

/*
 * We only trap IRQ, FIQ and SError exceptions when a guest is running. Do a
 * world switch to host to handle these exceptions.
 */

LENTRY(handle_el2_el1_irq64)
	do_world_switch_to_host
	str	x9, [sp, #-16]!
	mrs	x9, ich_misr_el2
	cmp	x9, xzr
	beq	1f
	mov	x0, #EXCP_TYPE_MAINT_IRQ
	b	2f
1:
	mov	x0, #EXCP_TYPE_EL1_IRQ
2:
	ldr	x9, [sp], #16
	ret
LEND(handle_el2_el1_irq64)

LENTRY(handle_el2_el1_fiq64)
	do_world_switch_to_host
	mov	x0, #EXCP_TYPE_EL1_FIQ
	ret
LEND(handle_el2_el1_fiq64)

LENTRY(handle_el2_el1_error64)
	do_world_switch_to_host
	mov	x0, #EXCP_TYPE_EL1_ERROR
	ret
LEND(handle_el2_el1_error64)


/*
 * Usage:
 * uint64_t vmm_do_call_guest(struct hypctx *hypctx)
 *
 * Expecting:
 * x0 - hypctx address
 */
ENTRY(VMM_HYP_FUNC(do_call_guest))
#ifdef VMM_VHE
	mrs	x1, vbar_el1
	adrp	x2, hyp_vectors
	add	x2, x2, :lo12:hyp_vectors
	msr	vbar_el1, x2
	isb
#endif

	/* Save hypctx address */
	msr	tpidr_el2, x0

	save_host_registers
	restore_guest_registers

	/* Enter guest */
	ERET
END(VMM_HYP_FUNC(do_call_guest))

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
