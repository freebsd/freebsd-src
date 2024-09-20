/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Arm Ltd
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define	VMM_HYP_FUNC(func)	vmm_nvhe_ ## func

#include "vmm_hyp_exception.S"

	.section ".vmm_vectors","ax"
	.align 11
hyp_init_vectors:
	vempty		/* Synchronous EL2t */
	vempty		/* IRQ EL2t */
	vempty		/* FIQ EL2t */
	vempty		/* Error EL2t */

	vempty		/* Synchronous EL2h */
	vempty		/* IRQ EL2h */
	vempty		/* FIQ EL2h */
	vempty		/* Error EL2h */

	vector hyp_init	/* Synchronous 64-bit EL1 */
	vempty		/* IRQ 64-bit EL1 */
	vempty		/* FIQ 64-bit EL1 */
	vempty		/* Error 64-bit EL1 */

	vempty		/* Synchronous 32-bit EL1 */
	vempty		/* IRQ 32-bit EL1 */
	vempty		/* FIQ 32-bit EL1 */
	vempty		/* Error 32-bit EL1 */

	.text

/*
 * Initialize the hypervisor mode with a new exception vector table, translation
 * table and stack.
 *
 * Expecting:
 * x0 - translation tables physical address
 * x1 - stack top virtual address
 * x2 - TCR_EL2 value
 * x3 - SCTLR_EL2 value
 * x4 - VTCR_EL2 value
 */
LENTRY(handle_hyp_init)
	/* Install the new exception vectors */
	adrp	x6, hyp_vectors
	add	x6, x6, :lo12:hyp_vectors
	msr	vbar_el2, x6
	/* Set the stack top address */
	mov	sp, x1
	/* Use the host VTTBR_EL2 to tell the host and the guests apart */
	mov	x9, #VTTBR_HOST
	msr	vttbr_el2, x9
	/* Load the base address for the translation tables */
	msr	ttbr0_el2, x0
	/* Invalidate the TLB */
	dsb	ish
	tlbi	alle2
	dsb	ishst
	isb
	/* Use the same memory attributes as EL1 */
	mrs	x9, mair_el1
	msr	mair_el2, x9
	/* Configure address translation */
	msr	tcr_el2, x2
	isb
	/* Set the system control register for EL2 */
	msr	sctlr_el2, x3
	/* Set the Stage 2 translation control register */
	msr	vtcr_el2, x4
	/* Return success */
	mov	x0, #0
	/* MMU is up and running */
	ERET
LEND(handle_hyp_init)

/*
 * Usage:
 * void vmm_cleanup(uint64_t handle, void *hyp_stub_vectors)
 *
 * Expecting:
 * x1 - physical address of hyp_stub_vectors
 */
LENTRY(vmm_cleanup)
	/* Restore the stub vectors */
	msr	vbar_el2, x1

	/* Disable the MMU */
	dsb	sy
	mrs	x2, sctlr_el2
	bic	x2, x2, #SCTLR_EL2_M
	msr	sctlr_el2, x2
	isb

	ERET
LEND(vmm_cleanup)
