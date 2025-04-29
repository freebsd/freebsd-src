/*-
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/elf_common.h>
#include <sys/errno.h>

#include <machine/asm.h>

#include "assym.inc"

/*
 * int efi_rt_arch_call(struct efirt_callinfo *);
 */
ENTRY(efi_rt_arch_call)
	sub	sp, sp, #(14 * 8)
	stp	x19, x20, [sp, #(2  * 8)]
	stp	x21, x22, [sp, #(4  * 8)]
	stp	x23, x24, [sp, #(6  * 8)]
	stp	x25, x26, [sp, #(8  * 8)]
	stp	x27, x28, [sp, #(10 * 8)]
	stp	x29, x30, [sp, #(12 * 8)]
	add	x29, sp, #(12 * 8)

	/* Save the stack pointer so we can find it later */
	ldr	x23, [x18, #PC_CURTHREAD]
	mov	x24, sp
	str	x24, [x23, #TD_MD_EFIRT_TMP]

	mov	x22, x0

	/* Load the function to branch to */
	ldr	x9, [x22, #(EC_FPTR)]

	/* Load the arguments */
	ldr	x4, [x22, #(EC_ARG1 + (4 * 8))]
	ldr	x3, [x22, #(EC_ARG1 + (3 * 8))]
	ldr	x2, [x22, #(EC_ARG1 + (2 * 8))]
	ldr	x1, [x22, #(EC_ARG1 + (1 * 8))]
	ldr	x0, [x22, #(EC_ARG1 + (0 * 8))]

	/* Set the fault handler */
	adr	x10, efi_rt_fault
	SET_FAULT_HANDLER(x10, x11)

	blr	x9

	/* Clear the fault handler */
	SET_FAULT_HANDLER(xzr, x11)

	/* Store the result */
	str	x0, [x22, #(EC_EFI_STATUS)]
	mov	x0, #0

.Lefi_rt_arch_call_exit:
	ldp	x19, x20, [sp, #(2  * 8)]
	ldp	x21, x22, [sp, #(4  * 8)]
	ldp	x23, x24, [sp, #(6  * 8)]
	ldp	x25, x26, [sp, #(8  * 8)]
	ldp	x27, x28, [sp, #(10 * 8)]
	ldp	x29, x30, [sp, #(12 * 8)]
	add	sp, sp, #(14 * 8)

	ret
END(efi_rt_arch_call)

LENTRY(efi_rt_fault)
	/* Clear pcb_onfault */
	SET_FAULT_HANDLER(xzr, x11)
	/* Load curthread */
	ldr	x1, [x18, #PC_CURTHREAD]
	/* Restore the stack pointer */
	ldr	x2, [x1, #TD_MD_EFIRT_TMP]
	mov	sp, x2
	/* Normal exit returning an error */
	ldr	x0, =EFAULT
	b	.Lefi_rt_arch_call_exit
LEND(efi_rt_fault)

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
