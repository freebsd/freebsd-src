/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include <machine/asm.h>

.macro arm_smccc_1_0	insn
ENTRY(arm_smccc_\insn)
	\insn	#0
	ldr	x4, [sp]
	cbz	x4, 1f
	stp	x0, x1, [x4, #16 * 0]
	stp	x2, x3, [x4, #16 * 1]
1:	ret
END(arm_smccc_\insn)
.endm

/*
 * int arm_smccc_*(register_t, register_t, register_t, register_t,
 *     register_t, register_t, register_t, register_t,
 *     struct arm_smccc_res *res)
 */
arm_smccc_1_0	hvc
arm_smccc_1_0	smc

.macro arm_smccc_1_2	insn
ENTRY(arm_smccc_1_2_\insn)
	stp	x1, x19, [sp, #-16]!
	mov	x19, x0
	ldp	x0, x1, [x19, #16 * 0]
	ldp	x2, x3, [x19, #16 * 1]
	ldp	x4, x5, [x19, #16 * 2]
	ldp	x6, x7, [x19, #16 * 3]
	ldp	x8, x9, [x19, #16 * 4]
	ldp	x10, x11, [x19, #16 * 5]
	ldp	x12, x13, [x19, #16 * 6]
	ldp	x14, x15, [x19, #16 * 7]
	ldp	x16, x17, [x19, #16 * 8]
	\insn	#0
	ldr	x19, [sp]
	cbz	x19, 1f
	stp	x0, x1, [x19, #16 * 0]
	stp	x2, x3, [x19, #16 * 1]
	stp	x4, x5, [x19, #16 * 2]
	stp	x6, x7, [x19, #16 * 3]
	stp	x8, x9, [x19, #16 * 4]
	stp	x10, x11, [x19, #16 * 5]
	stp	x12, x13, [x19, #16 * 6]
	stp	x14, x15, [x19, #16 * 7]
	stp	x16, x17, [x19, #16 * 8]
1:	ldp	xzr, x19, [sp], #16
	ret
END(arm_smccc_1_2_\insn)
.endm
/* int arm_smccc_1_2_*(const struct arm_smccc_1_2_regs *args,
 *     struct arm_smccc_1_2_regs *res)
 */
arm_smccc_1_2	hvc
arm_smccc_1_2	smc

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
