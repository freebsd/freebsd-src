/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2015 Andrew Turner
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

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

.arch_extension sec	/* For smc */
.arch_extension virt	/* For hvc */

/*
 * int arm_smccc_hvc(register_t, register_t, register_t, register_t,
 *     register_t, register_t, register_t, register_t,
 *     struct arm_smccc_res *res)
 */
ENTRY(arm_smccc_hvc)
	mov	r12, sp
	push	{r4-r7}
	ldm	r12, {r4-r7}
	hvc	#0
	pop	{r4-r7}
	ldr	r12, [sp, #(4 * 4)]
	cmp     r12, #0
	beq	1f
	stm	r12, {r0-r3}
1:	bx	lr
END(arm_smccc_hvc)

/*
 * int arm_smccc_smc(register_t, register_t, register_t, register_t,
 *     register_t, register_t, register_t, register_t,
 *     struct arm_smccc_res *res)
 */
ENTRY(arm_smccc_smc)
	mov	r12, sp
	push	{r4-r7}
	ldm	r12, {r4-r7}
	smc	#0
	pop	{r4-r7}
	ldr	r12, [sp, #(4 * 4)]
	cmp     r12, #0
	beq	1f
	stm	r12, {r0-r3}
1:	bx	lr
END(arm_smccc_smc)
