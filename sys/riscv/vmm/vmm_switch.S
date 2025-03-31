/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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

#include <machine/asm.h>
#include "assym.inc"

	.text

/*
 * a0 == hypctx *
 */
ENTRY(vmm_switch)
	sd	ra, (HYP_H_RA)(a0)
	sd	sp, (HYP_H_SP)(a0)
	sd	tp, (HYP_H_TP)(a0)
	sd	gp, (HYP_H_GP)(a0)
	sd	s0, (HYP_H_S + 0 * 8)(a0)
	sd	s1, (HYP_H_S + 1 * 8)(a0)
	sd	s2, (HYP_H_S + 2 * 8)(a0)
	sd	s3, (HYP_H_S + 3 * 8)(a0)
	sd	s4, (HYP_H_S + 4 * 8)(a0)
	sd	s5, (HYP_H_S + 5 * 8)(a0)
	sd	s6, (HYP_H_S + 6 * 8)(a0)
	sd	s7, (HYP_H_S + 7 * 8)(a0)
	sd	s8, (HYP_H_S + 8 * 8)(a0)
	sd	s9, (HYP_H_S + 9 * 8)(a0)
	sd	s10, (HYP_H_S + 10 * 8)(a0)
	sd	s11, (HYP_H_S + 11 * 8)(a0)

	sd	a1, (HYP_H_A + 1 * 8)(a0)
	sd	a2, (HYP_H_A + 2 * 8)(a0)
	sd	a3, (HYP_H_A + 3 * 8)(a0)
	sd	a4, (HYP_H_A + 4 * 8)(a0)
	sd	a5, (HYP_H_A + 5 * 8)(a0)
	sd	a6, (HYP_H_A + 6 * 8)(a0)
	sd	a7, (HYP_H_A + 7 * 8)(a0)

	ld	t0, (HYP_G_SSTATUS)(a0)
	ld	t1, (HYP_G_HSTATUS)(a0)
	ld	t2, (HYP_G_SCOUNTEREN)(a0)
	la	t4, .Lswitch_return
	ld	t5, (HYP_G_SEPC)(a0)

	csrrw	t0, sstatus, t0
	csrrw	t1, hstatus, t1
	csrrw	t2, scounteren, t2
	csrrw	t3, sscratch, a0
	csrrw	t4, stvec, t4
	csrw	sepc, t5

	sd	t0, (HYP_H_SSTATUS)(a0)
	sd	t1, (HYP_H_HSTATUS)(a0)
	sd	t2, (HYP_H_SCOUNTEREN)(a0)
	sd	t3, (HYP_H_SSCRATCH)(a0)
	sd	t4, (HYP_H_STVEC)(a0)

	ld	ra, (HYP_G_RA)(a0)
	ld	sp, (HYP_G_SP)(a0)
	ld	gp, (HYP_G_GP)(a0)
	ld	tp, (HYP_G_TP)(a0)
	ld	t0, (HYP_G_T + 0 * 8)(a0)
	ld	t1, (HYP_G_T + 1 * 8)(a0)
	ld	t2, (HYP_G_T + 2 * 8)(a0)
	ld	t3, (HYP_G_T + 3 * 8)(a0)
	ld	t4, (HYP_G_T + 4 * 8)(a0)
	ld	t5, (HYP_G_T + 5 * 8)(a0)
	ld	t6, (HYP_G_T + 6 * 8)(a0)
	ld	s0, (HYP_G_S + 0 * 8)(a0)
	ld	s1, (HYP_G_S + 1 * 8)(a0)
	ld	s2, (HYP_G_S + 2 * 8)(a0)
	ld	s3, (HYP_G_S + 3 * 8)(a0)
	ld	s4, (HYP_G_S + 4 * 8)(a0)
	ld	s5, (HYP_G_S + 5 * 8)(a0)
	ld	s6, (HYP_G_S + 6 * 8)(a0)
	ld	s7, (HYP_G_S + 7 * 8)(a0)
	ld	s8, (HYP_G_S + 8 * 8)(a0)
	ld	s9, (HYP_G_S + 9 * 8)(a0)
	ld	s10, (HYP_G_S + 10 * 8)(a0)
	ld	s11, (HYP_G_S + 11 * 8)(a0)
	/* skip a0 for now. */
	ld	a1, (HYP_G_A + 1 * 8)(a0)
	ld	a2, (HYP_G_A + 2 * 8)(a0)
	ld	a3, (HYP_G_A + 3 * 8)(a0)
	ld	a4, (HYP_G_A + 4 * 8)(a0)
	ld	a5, (HYP_G_A + 5 * 8)(a0)
	ld	a6, (HYP_G_A + 6 * 8)(a0)
	ld	a7, (HYP_G_A + 7 * 8)(a0)
	/* now load a0. */
	ld	a0, (HYP_G_A + 0 * 8)(a0)

	sret

	.align 2
.Lswitch_return:

	csrrw	a0, sscratch, a0
	sd	ra, (HYP_G_RA)(a0)
	sd	sp, (HYP_G_SP)(a0)
	sd	gp, (HYP_G_GP)(a0)
	sd	tp, (HYP_G_TP)(a0)
	sd	t0, (HYP_G_T + 0 * 8)(a0)
	sd	t1, (HYP_G_T + 1 * 8)(a0)
	sd	t2, (HYP_G_T + 2 * 8)(a0)
	sd	t3, (HYP_G_T + 3 * 8)(a0)
	sd	t4, (HYP_G_T + 4 * 8)(a0)
	sd	t5, (HYP_G_T + 5 * 8)(a0)
	sd	t6, (HYP_G_T + 6 * 8)(a0)
	sd	s0, (HYP_G_S + 0 * 8)(a0)
	sd	s1, (HYP_G_S + 1 * 8)(a0)
	sd	s2, (HYP_G_S + 2 * 8)(a0)
	sd	s3, (HYP_G_S + 3 * 8)(a0)
	sd	s4, (HYP_G_S + 4 * 8)(a0)
	sd	s5, (HYP_G_S + 5 * 8)(a0)
	sd	s6, (HYP_G_S + 6 * 8)(a0)
	sd	s7, (HYP_G_S + 7 * 8)(a0)
	sd	s8, (HYP_G_S + 8 * 8)(a0)
	sd	s9, (HYP_G_S + 9 * 8)(a0)
	sd	s10, (HYP_G_S + 10 * 8)(a0)
	sd	s11, (HYP_G_S + 11 * 8)(a0)
	/* skip a0 */
	sd	a1, (HYP_G_A + 1 * 8)(a0)
	sd	a2, (HYP_G_A + 2 * 8)(a0)
	sd	a3, (HYP_G_A + 3 * 8)(a0)
	sd	a4, (HYP_G_A + 4 * 8)(a0)
	sd	a5, (HYP_G_A + 5 * 8)(a0)
	sd	a6, (HYP_G_A + 6 * 8)(a0)
	sd	a7, (HYP_G_A + 7 * 8)(a0)

	ld	t1, (HYP_H_STVEC)(a0)
	ld	t2, (HYP_H_SSCRATCH)(a0)
	ld	t3, (HYP_H_SCOUNTEREN)(a0)
	ld	t4, (HYP_H_HSTATUS)(a0)
	ld	t5, (HYP_H_SSTATUS)(a0)

	csrr	t0, sepc
	csrw	stvec, t1
	csrrw	t2, sscratch, t2
	csrrw	t3, scounteren, t3
	csrrw	t4, hstatus, t4
	csrrw	t5, sstatus, t5

	sd	t0, (HYP_G_SEPC)(a0)
	sd	t2, (HYP_G_A + 0 * 8)(a0)
	sd	t3, (HYP_G_SCOUNTEREN)(a0)
	sd	t4, (HYP_G_HSTATUS)(a0)
	sd	t5, (HYP_G_SSTATUS)(a0)

	ld	ra, (HYP_H_RA)(a0)
	ld	sp, (HYP_H_SP)(a0)
	ld	tp, (HYP_H_TP)(a0)
	ld	gp, (HYP_H_GP)(a0)
	ld	s0, (HYP_H_S + 0 * 8)(a0)
	ld	s1, (HYP_H_S + 1 * 8)(a0)
	ld	s2, (HYP_H_S + 2 * 8)(a0)
	ld	s3, (HYP_H_S + 3 * 8)(a0)
	ld	s4, (HYP_H_S + 4 * 8)(a0)
	ld	s5, (HYP_H_S + 5 * 8)(a0)
	ld	s6, (HYP_H_S + 6 * 8)(a0)
	ld	s7, (HYP_H_S + 7 * 8)(a0)
	ld	s8, (HYP_H_S + 8 * 8)(a0)
	ld	s9, (HYP_H_S + 9 * 8)(a0)
	ld	s10, (HYP_H_S + 10 * 8)(a0)
	ld	s11, (HYP_H_S + 11 * 8)(a0)

	ld	a1, (HYP_H_A + 1 * 8)(a0)
	ld	a2, (HYP_H_A + 2 * 8)(a0)
	ld	a3, (HYP_H_A + 3 * 8)(a0)
	ld	a4, (HYP_H_A + 4 * 8)(a0)
	ld	a5, (HYP_H_A + 5 * 8)(a0)
	ld	a6, (HYP_H_A + 6 * 8)(a0)
	ld	a7, (HYP_H_A + 7 * 8)(a0)

	ret

END(vmm_switch)

ENTRY(vmm_unpriv_trap)
	csrr	a1, sepc
	sd	a1, HYP_TRAP_SEPC(a0)
	addi	a1, a1, 4	/* Next instruction after hlvx.hu */
	csrw	sepc, a1
	csrr	a1, scause
	sd	a1, HYP_TRAP_SCAUSE(a0)
	csrr	a1, stval
	sd	a1, HYP_TRAP_STVAL(a0)
	csrr	a1, htval
	sd	a1, HYP_TRAP_HTVAL(a0)
	csrr	a1, htinst
	sd	a1, HYP_TRAP_HTINST(a0)
	sret
END(vmm_unpriv_trap)
