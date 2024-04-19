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

/*
 * Mark variant_pcs_test as a variant pcs function so we can check the
 * saved register values later. The buffer to store the registers is
 * passed in x0.
 */
.variant_pcs variant_pcs_test
ENTRY(variant_pcs_test)
	stp	x0,  x1,  [x0, #(0  * 8)]
	stp	x2,  x3,  [x0, #(2  * 8)]
	stp	x4,  x5,  [x0, #(4  * 8)]
	stp	x6,  x7,  [x0, #(6  * 8)]
	stp	x8,  x9,  [x0, #(8  * 8)]
	stp	x10, x11, [x0, #(10 * 8)]
	stp	x12, x13, [x0, #(12 * 8)]
	stp	x14, x15, [x0, #(14 * 8)]
	stp	x16, x17, [x0, #(16 * 8)]
	stp	x18, x19, [x0, #(18 * 8)]
	stp	x20, x21, [x0, #(20 * 8)]
	stp	x22, x23, [x0, #(22 * 8)]
	stp	x24, x25, [x0, #(24 * 8)]
	stp	x26, x27, [x0, #(26 * 8)]
	stp	x28, x29, [x0, #(28 * 8)]
	str	x30,      [x0, #(30 * 8)]
	ret
END(variant_pcs_test)
