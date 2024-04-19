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

#include <machine/asm.h>

.global variant_pcs_test_ret

/*
 * void variant_pcs_helper(void *buffer);
 *
 * This follows the aapcs64 convention so needs to save and restore
 * the callee-saved registers. It puts a known value in each register
 * other than x0 and x30 as x0 contains the buffer used to store the
 * register data, and x30 is the link register so will be trashed by
 * the branch and link to variant_pcs_test.
 */
ENTRY(variant_pcs_helper)
	sub	sp, sp, #(14 * 8)
	stp	x29, x30, [sp, #(12 * 8)]
	stp	x27, x28, [sp, #(10 * 8)]
	stp	x25, x26, [sp, #( 8 * 8)]
	stp	x23, x24, [sp, #( 6 * 8)]
	stp	x22, x23, [sp, #( 4 * 8)]
	stp	x21, x22, [sp, #( 2 * 8)]
	stp	x19, x20, [sp, #( 0 * 8)]
	add	x29, sp, #(12 * 8)
	mov	x1,  #1
	mov	x2,  #2
	mov	x3,  #3
	mov	x4,  #4
	mov	x5,  #5
	mov	x6,  #6
	mov	x7,  #7
	mov	x8,  #8
	mov	x9,  #9
	mov	x10, #10
	mov	x11, #11
	mov	x12, #12
	mov	x13, #13
	mov	x14, #14
	mov	x15, #15
	mov	x16, #16
	mov	x17, #17
	mov	x18, #18
	mov	x19, #19
	mov	x20, #20
	mov	x21, #21
	mov	x22, #22
	mov	x23, #23
	mov	x24, #24
	mov	x25, #25
	mov	x26, #26
	mov	x27, #27
	mov	x28, #28
	mov	x29, #29
	bl	variant_pcs_test
variant_pcs_test_ret:
	ldp	x19, x20, [sp, #( 0 * 8)]
	ldp	x21, x22, [sp, #( 2 * 8)]
	ldp	x22, x23, [sp, #( 4 * 8)]
	ldp	x23, x24, [sp, #( 6 * 8)]
	ldp	x25, x26, [sp, #( 8 * 8)]
	ldp	x27, x28, [sp, #(10 * 8)]
	ldp	x29, x30, [sp, #(12 * 8)]
	add	sp, sp, #(14 * 8)
	ret
END(variant_pcs_helper)
