/*-
 * Copyright (c) 2012-2013 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#ifndef _MIPS_INCLUDE_CHERIASM_H_
#define	_MIPS_INCLUDE_CHERIASM_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

/*
 * 27 user-context registers -- with names where appropriate.
 */
#define	CHERI_REG_C0	$c0	/* MIPS legacy load/store capability. */
#define	CHERI_REG_C1	$c1
#define	CHERI_REG_C2	$c2
#define	CHERI_REG_C3	$c3
#define	CHERI_REG_C4	$c4
#define	CHERI_REG_C5	$c5
#define	CHERI_REG_C6	$c6
#define	CHERI_REG_C7	$c7
#define	CHERI_REG_C8	$c8
#define	CHERI_REG_C9	$c9
#define	CHERI_REG_C10	$c10
#define	CHERI_REG_C11	$c11
#define	CHERI_REG_C12	$c12
#define	CHERI_REG_C13	$c13
#define	CHERI_REG_C14	$c14
#define	CHERI_REG_C15	$c15
#define	CHERI_REG_C16	$c16
#define	CHERI_REG_C17	$c17
#define	CHERI_REG_C18	$c18
#define	CHERI_REG_C19	$c19
#define	CHERI_REG_C20	$c20
#define	CHERI_REG_C21	$c21
#define	CHERI_REG_C22	$c22
#define	CHERI_REG_C23	$c23
#define	CHERI_REG_RCC	$c24	/* Return code capability. */
#define	CHERI_REG_C25	$c25	/* Notionally reserved for exception-use. */
#define	CHERI_REG_IDC	$c26	/* Invoked data capability. */

/* 5 exception-context registers -- with names where appropriate. */
#define	CHERI_REG_KR1C	$c27	/* Kernel exception handling capability (1). */
#define	CHERI_REG_KR2C	$c28	/* Kernel exception handling capability (2). */
#define	CHERI_REG_KCC	$c29	/* Kernel code capability. */
#define	CHERI_REG_KDC	$c30	/* Kernel data capability. */
#define	CHERI_REG_EPCC	$c31	/* Exception program counter capability. */

/*
 * C-level code will manipulate capabilities using this exception-handling
 * register; label it here for consistency.  Interrupts must be disabled while
 * using the register to prevent awkward preemptions.
 */
#define	CHERI_REG_CTEMP	CHERI_REG_KR1C	/* C-level capability manipulation. */

/*
 * Where to save the user $c0 during low-level exception handling.  Possibly
 * this should be an argument to macros rather than hard-coded in the macros.
 */
#define	CHERI_REG_SEC0	CHERI_REG_KR2C	/* Saved $c0 in exception handling. */

/*
 * (Possibly) temporary ABI in which $c1 is the code argument to CCall, and
 * $c2 is the data argument.
 */
#define	CHERI_REG_CCALLCODE	$c1
#define	CHERI_REG_CCALLDATA	$c2

/*
 * Assembly code to be used in CHERI exception handling and context switching.
 *
 * When entering an exception handler from userspace, conditionally save the
 * default user data capability.  Then install the kernel's default data
 * capability.  The caller provides a temporary register to use for the
 * purposes of querying CP0 SR to determine whether the target is userspace or
 * the kernel.
 */
#define	CHERI_EXCEPTION_ENTER(reg)					\
	mfc0	reg, MIPS_COP_0_STATUS;					\
	andi	reg, reg, MIPS_SR_KSU_USER;				\
	beq	reg, $0, 64f;						\
	nop;								\
	/* Save user $c0; install kernel $c0. */			\
	cmove	CHERI_REG_SEC0, CHERI_REG_C0;				\
	cmove	CHERI_REG_C0, CHERI_REG_KDC;				\
64:

/*
 * When returning from an exception, conditionally restore the default user
 * data capability.  The caller provides a temporary register to use for the
 * purposes of querying CP0 SR to determine whether the target is userspace
 * or the kernel.
 *
 * XXXCHERI: We assume that the caller will install an appropriate PCC for a
 * return to userspace, but that in the kernel case, we need to install a
 * kernel EPCC, potentially overwriting a previously present user EPCC from
 * exception entry.  Once the kernel does multiple security domains, the
 * caller should manage EPCC in that case as well, and we can remove EPCC
 * assignment here.
 */
#define	CHERI_EXCEPTION_RETURN(reg)					\
	mfc0	reg, MIPS_COP_0_STATUS;					\
	andi	reg, reg, MIPS_SR_KSU_USER;				\
	beq	reg, $0, 65f;						\
	nop;								\
	b	66f;							\
	/* If returning to userspace, restore saved user $c0. */	\
	cmove	CHERI_REG_C0, CHERI_REG_SEC0;	/* Branch-delay. */	\
65:									\
	/* If returning to kernelspace, reinstall kernel code PCC. */	\
	cmove	CHERI_REG_EPCC, CHERI_REG_KCC;				\
66:

/*
 * Macros to save and restore CHERI capability registers registers from
 * pcb.pcb_cheriframe, individually and in quantity.  Explicitly use $kdc
 * ($30), which U_PCB_CHERIFRAME is assumed to be valid for, but that the
 * userspace $c0 has been set aside in CHERI_REG_SEC0.  This assumes previous
 * or further calls to CHERI_EXECPTION_ENTER() and CHERI_EXCEPTION_RETURN() to
 * manage $c0.
 */
#define	SZCAP	32
#define	SAVE_U_PCB_CHERIREG(creg, offs, base, treg)			\
	PTR_ADDIU	treg, base, U_PCB_CHERIFRAME;			\
	csc		creg, treg, (SZCAP * offs)(CHERI_REG_KDC)

#define	RESTORE_U_PCB_CHERIREG(creg, offs, base, treg)			\
	PTR_ADDIU	treg, base, U_PCB_CHERIFRAME;			\
	clc		creg, treg, (SZCAP * offs)(CHERI_REG_KDC)

/*
 * XXXRW: Update once the assembler supports reserved CHERI register names to
 * avoid hard-coding here.
 *
 * XXXRW: It woudld be nice to make calls to these conditional on actual CHERI
 * coprocessor use, similar to on-demand context management for other MIPS
 * coprocessors (e.g., FP).
 *
 * XXXRW: Note hard-coding of UDC here.
 */
#define	SAVE_CHERI_CONTEXT(base, treg)					\
	SAVE_U_PCB_CHERIREG(CHERI_REG_SEC0, CHERI_CR_C0_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C1, CHERI_CR_C1_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C2, CHERI_CR_C2_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C3, CHERI_CR_C3_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C4, CHERI_CR_C4_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C5, CHERI_CR_C5_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C6, CHERI_CR_C6_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C7, CHERI_CR_C7_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C8, CHERI_CR_C8_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C9, CHERI_CR_C9_OFF, base, treg);	\
	SAVE_U_PCB_CHERIREG(CHERI_REG_C10, CHERI_CR_C10_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C11, CHERI_CR_C11_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C12, CHERI_CR_C12_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C13, CHERI_CR_C13_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C14, CHERI_CR_C14_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C15, CHERI_CR_C15_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C16, CHERI_CR_C16_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C17, CHERI_CR_C17_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C18, CHERI_CR_C18_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C19, CHERI_CR_C19_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C20, CHERI_CR_C20_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C21, CHERI_CR_C21_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C22, CHERI_CR_C22_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C23, CHERI_CR_C23_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_RCC, CHERI_CR_RCC_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_C25, CHERI_CR_C25_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_IDC, CHERI_CR_IDC_OFF, base, treg); \
	SAVE_U_PCB_CHERIREG(CHERI_REG_EPCC, CHERI_CR_PCC_OFF, base, treg)

#define	RESTORE_CHERI_CONTEXT(base, treg)				\
	RESTORE_U_PCB_CHERIREG(CHERI_REG_SEC0, CHERI_CR_C0_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C1, CHERI_CR_C1_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C2, CHERI_CR_C2_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C3, CHERI_CR_C3_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C4, CHERI_CR_C4_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C5, CHERI_CR_C5_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C6, CHERI_CR_C6_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C7, CHERI_CR_C7_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C8, CHERI_CR_C8_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C9, CHERI_CR_C9_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C10, CHERI_CR_C10_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C11, CHERI_CR_C11_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C12, CHERI_CR_C12_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C13, CHERI_CR_C13_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C14, CHERI_CR_C14_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C15, CHERI_CR_C15_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C16, CHERI_CR_C16_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C17, CHERI_CR_C17_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C18, CHERI_CR_C18_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C19, CHERI_CR_C19_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C20, CHERI_CR_C20_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C21, CHERI_CR_C21_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C22, CHERI_CR_C22_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C23, CHERI_CR_C23_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_RCC, CHERI_CR_RCC_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_C25, CHERI_CR_C25_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_IDC, CHERI_CR_IDC_OFF, base, treg); \
	RESTORE_U_PCB_CHERIREG(CHERI_REG_EPCC, CHERI_CR_PCC_OFF, base, treg)

#endif /* _MIPS_INCLUDE_CHERIASM_H_ */
