/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef _MIPS_INCLUDE_EXCEPTIONASM_H_
#define _MIPS_INCLUDE_EXCEPTIONASM_H_

#if defined(CPU_CNMIPS)
#define CLEAR_STATUS \
	mfc0    a0, MIPS_COP_0_STATUS   ;\
	li      a2, (MIPS_SR_KX | MIPS_SR_SX | MIPS_SR_UX) ; \
	or      a0, a0, a2	        ; \
	li      a2, ~(MIPS_SR_INT_IE | MIPS_SR_EXL | MIPS_SR_KSU_USER)   ; \
	and     a0, a0, a2              ; \
        mtc0    a0, MIPS_COP_0_STATUS   ; \
	ITLBNOPFIX
#elif defined(CPU_RMI) || defined(CPU_NLM)
#define CLEAR_STATUS \
	mfc0    a0, MIPS_COP_0_STATUS   ;\
	li      a2, (MIPS_SR_KX | MIPS_SR_UX | MIPS_SR_COP_2_BIT) ; \
	or      a0, a0, a2	        ; \
	li      a2, ~(MIPS_SR_INT_IE | MIPS_SR_EXL | MIPS_SR_KSU_USER)   ; \
	and     a0, a0, a2              ; \
        mtc0    a0, MIPS_COP_0_STATUS   ; \
	ITLBNOPFIX
#else
#define CLEAR_STATUS \
	mfc0    a0, MIPS_COP_0_STATUS   ;\
	li      a2, ~(MIPS_SR_INT_IE | MIPS_SR_EXL | MIPS_SR_KSU_USER)   ; \
	and     a0, a0, a2              ; \
	mtc0	a0, MIPS_COP_0_STATUS   ; \
	ITLBNOPFIX
#endif

/*
 * Save all of the registers except for the kernel temporaries in u.u_pcb.
 */
#define	SAVE_REGS_TO_PCB(pcb)			\
	SAVE_U_PCB_REG(AT, AST, pcb)		; \
	.set	at				; \
	SAVE_U_PCB_REG(v0, V0, pcb)		; \
	SAVE_U_PCB_REG(v1, V1, pcb)		; \
	SAVE_U_PCB_REG(a0, A0, pcb)		; \
	mflo	v0				; \
	SAVE_U_PCB_REG(a1, A1, pcb)		; \
	SAVE_U_PCB_REG(a2, A2, pcb)		; \
	SAVE_U_PCB_REG(a3, A3, pcb)		; \
	SAVE_U_PCB_REG(t0, T0, pcb)		; \
	mfhi	v1				; \
	SAVE_U_PCB_REG(t1, T1, pcb)		; \
	SAVE_U_PCB_REG(t2, T2, pcb)		; \
	SAVE_U_PCB_REG(t3, T3, pcb)		; \
	SAVE_U_PCB_REG(ta0, TA0, pcb)		; \
	mfc0	a0, MIPS_COP_0_STATUS		; \
	SAVE_U_PCB_REG(ta1, TA1, pcb)		; \
	SAVE_U_PCB_REG(ta2, TA2, pcb)		; \
	SAVE_U_PCB_REG(ta3, TA3, pcb)		; \
	SAVE_U_PCB_REG(s0, S0, pcb)		; \
	mfc0	a1, MIPS_COP_0_CAUSE		; \
	SAVE_U_PCB_REG(s1, S1, pcb)		; \
	SAVE_U_PCB_REG(s2, S2, pcb)		; \
	SAVE_U_PCB_REG(s3, S3, pcb)		; \
	SAVE_U_PCB_REG(s4, S4, pcb)		; \
	MFC0	a2, MIPS_COP_0_BAD_VADDR	; \
	SAVE_U_PCB_REG(s5, S5, pcb)		; \
	SAVE_U_PCB_REG(s6, S6, pcb)		; \
	SAVE_U_PCB_REG(s7, S7, pcb)		; \
	SAVE_U_PCB_REG(t8, T8, pcb)		; \
	MFC0	a3, MIPS_COP_0_EXC_PC		; \
	SAVE_U_PCB_REG(t9, T9, pcb)		; \
	SAVE_U_PCB_REG(gp, GP, pcb)		; \
	SAVE_U_PCB_REG(sp, SP, pcb)		; \
	SAVE_U_PCB_REG(s8, S8, pcb)		; \
	PTR_SUBU	sp, pcb, CALLFRAME_SIZ	; \
	SAVE_U_PCB_REG(ra, RA, pcb)		; \
	SAVE_U_PCB_REG(v0, MULLO, pcb)		; \
	SAVE_U_PCB_REG(v1, MULHI, pcb)		; \
	SAVE_U_PCB_REG(a0, SR, pcb)		; \
	SAVE_U_PCB_REG(a1, CAUSE, pcb)		; \
	SAVE_U_PCB_REG(a2, BADVADDR, pcb)	; \
	SAVE_U_PCB_REG(a3, PC, pcb)

#define	RESTORE_REGS_FROM_PCB(pcb)		\
	RESTORE_U_PCB_REG(t0, MULLO, pcb)	; \
	RESTORE_U_PCB_REG(t1, MULHI, pcb)	; \
	mtlo	t0				; \
	mthi	t1				; \
	RESTORE_U_PCB_REG(a0, PC, pcb)		; \
	RESTORE_U_PCB_REG(v0, V0, pcb)		; \
        MTC0	a0, MIPS_COP_0_EXC_PC		; \
	RESTORE_U_PCB_REG(v1, V1, pcb)		; \
	RESTORE_U_PCB_REG(a0, A0, pcb)		; \
	RESTORE_U_PCB_REG(a1, A1, pcb)		; \
	RESTORE_U_PCB_REG(a2, A2, pcb)		; \
	RESTORE_U_PCB_REG(a3, A3, pcb)		; \
	RESTORE_U_PCB_REG(t0, T0, pcb)		; \
	RESTORE_U_PCB_REG(t1, T1, pcb)		; \
	RESTORE_U_PCB_REG(t2, T2, pcb)		; \
	RESTORE_U_PCB_REG(t3, T3, pcb)		; \
	RESTORE_U_PCB_REG(ta0, TA0, pcb)		; \
	RESTORE_U_PCB_REG(ta1, TA1, pcb)		; \
	RESTORE_U_PCB_REG(ta2, TA2, pcb)		; \
	RESTORE_U_PCB_REG(ta3, TA3, pcb)		; \
	RESTORE_U_PCB_REG(s0, S0, pcb)		; \
	RESTORE_U_PCB_REG(s1, S1, pcb)		; \
	RESTORE_U_PCB_REG(s2, S2, pcb)		; \
	RESTORE_U_PCB_REG(s3, S3, pcb)		; \
	RESTORE_U_PCB_REG(s4, S4, pcb)		; \
	RESTORE_U_PCB_REG(s5, S5, pcb)		; \
	RESTORE_U_PCB_REG(s6, S6, pcb)		; \
	RESTORE_U_PCB_REG(s7, S7, pcb)		; \
	RESTORE_U_PCB_REG(t8, T8, pcb)		; \
	RESTORE_U_PCB_REG(t9, T9, pcb)		; \
	RESTORE_U_PCB_REG(gp, GP, pcb)		; \
	RESTORE_U_PCB_REG(sp, SP, pcb)		; \
	RESTORE_U_PCB_REG(k0, SR, pcb)		; \
	RESTORE_U_PCB_REG(s8, S8, pcb)		; \
	RESTORE_U_PCB_REG(ra, RA, pcb)		; \
	.set noat				; \
	RESTORE_U_PCB_REG(AT, AST, pcb)

#endif /* _MIPS_INCLUDE_EXCEPTIONASM_H_ */
