/*	$OpenBSD: locore.S,v 1.18 1998/09/15 10:58:53 pefo Exp $	*/
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corporation and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/loMem.s,
 *	v 1.1 89/07/11 17:55:04 nelson Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsm.s,
 *	v 9.2 90/01/29 18:00:39 shirriff Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/vmPmaxAsm.s,
 *	v 1.1 89/07/10 14:27:41 nelson Exp  SPRITE (DECWRL)
 *	from: @(#)locore.s	8.5 (Berkeley) 1/4/94
 *	JNPR: exception.S,v 1.5 2007/01/08 04:58:37 katta
 * $FreeBSD$
 */

#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/exceptionasm.h>
#include <machine/regnum.h>
#include <machine/cpuregs.h>
#include <machine/pte.h>
#include <machine/pcb.h>

#ifdef CPU_CHERI
#include <machine/cheriasm.h>
#include <machine/cherireg.h>
#endif

#include "assym.s"

/*
 * The CHERICCallKtrace and CHERICReturnKtrace functions in this file
 * are near copies of MipsUserGenException.  The differences are:
 *  - They call different C functions.
 *  - They return to their calling context rather than to userland.
 */

	.set	noreorder		# Noreorder is default style!

NESTED_NOPROFILE(CHERICCallKtrace, CALLFRAME_SIZ, ra)
	.set	push
	.set	noat
	.mask	0x80000000, (CALLFRAME_RA - CALLFRAME_SIZ)
/*
 * Save all of the registers except for the kernel temporaries in u.u_pcb.
 */
	GET_CPU_PCPU(k1)
	PTR_L	k1, PC_CURPCB(k1)
	SAVE_REGS_TO_PCB(k1)
#ifdef CPU_CHERI
	/*
	 * Note: This saves EPCC, matching the explicit EPC save above.
	 */
	CGetCause	k0
	SAVE_U_PCB_CHERIFRAME(k0, k1, t0)
#endif
	REG_S	a3, CALLFRAME_RA(sp)	# for debugging
	PTR_LA	gp, _C_LABEL(_gp)	# switch to kernel GP
# Turn off fpu and enter kernel mode
	and	t0, a0, ~(MIPS_SR_COP_1_BIT | MIPS_SR_EXL | MIPS_SR_KSU_MASK | MIPS_SR_INT_IE)
#if defined(CPU_CNMIPS)
	and	t0, t0, ~(MIPS_SR_COP_2_BIT)
	or      t0, t0, (MIPS_SR_KX | MIPS_SR_SX | MIPS_SR_UX | MIPS_SR_PX)
#elif defined(CPU_RMI)	|| defined(CPU_NLM)
	or      t0, t0, (MIPS_SR_KX | MIPS_SR_UX | MIPS_SR_COP_2_BIT)
#endif
	mtc0	t0, MIPS_COP_0_STATUS
	PTR_ADDU a0, k1, U_PCB_REGS
	ITLBNOPFIX

/*
 * Call the exception handler.
 */
	PTR_LA	k0, _C_LABEL(ktrccall)
	jalr	k0
	nop

/*
 * Restore user registers and return.
 * First disable interrupts and set exeption level.
 */
	DO_AST

	CLEAR_STATUS

/*
 * The use of k1 for storing the PCB pointer must be done only
 * after interrupts are disabled.  Otherwise it will get overwritten
 * by the interrupt code.
 */
	GET_CPU_PCPU(k1)
	PTR_L	k1, PC_CURPCB(k1)

	/*
	 * Update interrupt mask in saved status register
	 * Some of interrupts could be enabled by ithread
	 * scheduled by ast()
	 */
	mfc0	a0, MIPS_COP_0_STATUS
	and	a0, a0, MIPS_SR_INT_MASK
	RESTORE_U_PCB_REG(a1, SR, k1)
	and	a1, a1, ~MIPS_SR_INT_MASK
	or	a1, a1, a0
	SAVE_U_PCB_REG(a1, SR, k1)

#ifdef CPU_CHERI
	/*
	 * Note: This restores EPCC, matching the explicit EPC restore below.
	 */
	RESTORE_U_PCB_CHERIFRAME(k1, t0)
#endif
	RESTORE_REGS_FROM_PCB(k1)

	mtc0	k0, MIPS_COP_0_STATUS	# still exception level
	ITLBNOPFIX
	sync

	j	ra
	nop

	.set	pop
END(CHERICCallKtrace)

NESTED_NOPROFILE(CHERICReturnKtrace, CALLFRAME_SIZ, ra)
	.set	push
	.set	noat
	.mask	0x80000000, (CALLFRAME_RA - CALLFRAME_SIZ)
/*
 * Save all of the registers except for the kernel temporaries in u.u_pcb.
 */
	GET_CPU_PCPU(k1)
	PTR_L	k1, PC_CURPCB(k1)
	SAVE_REGS_TO_PCB(k1)
#ifdef CPU_CHERI
	/*
	 * Note: This saves EPCC, matching the explicit EPC save above.
	 */
	CGetCause	k0
	SAVE_U_PCB_CHERIFRAME(k0, k1, t0)
#endif
	REG_S	a3, CALLFRAME_RA(sp)	# for debugging
	PTR_LA	gp, _C_LABEL(_gp)	# switch to kernel GP
# Turn off fpu and enter kernel mode
	and	t0, a0, ~(MIPS_SR_COP_1_BIT | MIPS_SR_EXL | MIPS_SR_KSU_MASK | MIPS_SR_INT_IE)
#if defined(CPU_CNMIPS)
	and	t0, t0, ~(MIPS_SR_COP_2_BIT)
	or      t0, t0, (MIPS_SR_KX | MIPS_SR_SX | MIPS_SR_UX | MIPS_SR_PX)
#elif defined(CPU_RMI)	|| defined(CPU_NLM)
	or      t0, t0, (MIPS_SR_KX | MIPS_SR_UX | MIPS_SR_COP_2_BIT)
#endif
	mtc0	t0, MIPS_COP_0_STATUS
	PTR_ADDU a0, k1, U_PCB_REGS
	ITLBNOPFIX

/*
 * Call the exception handler.
 */
	PTR_LA	k0, _C_LABEL(ktrcreturn)
	jalr	k0
	nop

/*
 * Restore user registers and return.
 * First disable interrupts and set exeption level.
 */
	DO_AST

	CLEAR_STATUS

/*
 * The use of k1 for storing the PCB pointer must be done only
 * after interrupts are disabled.  Otherwise it will get overwritten
 * by the interrupt code.
 */
	GET_CPU_PCPU(k1)
	PTR_L	k1, PC_CURPCB(k1)

	/*
	 * Update interrupt mask in saved status register
	 * Some of interrupts could be enabled by ithread
	 * scheduled by ast()
	 */
	mfc0	a0, MIPS_COP_0_STATUS
	and	a0, a0, MIPS_SR_INT_MASK
	RESTORE_U_PCB_REG(a1, SR, k1)
	and	a1, a1, ~MIPS_SR_INT_MASK
	or	a1, a1, a0
	SAVE_U_PCB_REG(a1, SR, k1)

#ifdef CPU_CHERI
	/*
	 * Note: This restores EPCC, matching the explicit EPC restore below.
	 */
	RESTORE_U_PCB_CHERIFRAME(k1, t0)
#endif
	RESTORE_REGS_FROM_PCB(k1)

	mtc0	k0, MIPS_COP_0_STATUS	# still exception level
	ITLBNOPFIX
	sync

	j	ra
	nop

	.set	pop
END(CHERICReturnKtrace)
