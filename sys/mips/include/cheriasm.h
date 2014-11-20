/*-
 * Copyright (c) 2012-2014 Robert N. M. Watson
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
 * The kernel maintains a CHERI micro-ABI preserving two registers, $c11 and
 * $c12, for use by kernel threads.  Label them here for consistency.
 */
#define	CHERI_REG_CTEMP0	CHERI_REG_C11	/* C capability manipulation. */
#define	CHERI_REG_CTEMP1	CHERI_REG_C12	/* C capability manipulation. */

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
	/*								\
	 * XXXRW: If requested PC has been adjusted by stack, similarly	\
	 * adjust $epcc.offset, which will overwrite an earlier $epc	\
	 * assignment.							\
	 */								\
	MFC0	reg, MIPS_COP_0_EXC_PC;					\
	csetoffset	CHERI_REG_EPCC, CHERI_REG_EPCC, reg;		\
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
#define	SAVE_U_PCB_CHERIFRAME_CREG(creg, offs, base, treg)		\
	PTR_ADDIU	treg, base, U_PCB_CHERIFRAME;			\
	csc		creg, treg, (SZCAP * offs)(CHERI_REG_KDC)

#define	RESTORE_U_PCB_CHERIFRAME_CREG(creg, offs, base, treg)		\
	PTR_ADDIU	treg, base, U_PCB_CHERIFRAME;			\
	clc		creg, treg, (SZCAP * offs)(CHERI_REG_KDC)

/*
 * Macro to save the capability-cause register; we will never restore it as
 * part of a context switch.
 *
 * XXXRW: Or should we?
 *
 * The immediate field in csd is only 8 bits (signed), wheres the immediate
 * field in [d]addiu is 16 bits (unsigned), so we do all of the offset
 * calculation in the daddiu.
 */
#define	SAVE_U_PCB_CHERIFRAME_CAPCAUSE(cause, base, treg)		\
	PTR_ADDIU	treg, base, (SZCAP * CHERIFRAME_OFF_CAPCAUSE) + \
			U_PCB_CHERIFRAME;				\
	csd		cause, treg, 0\
			    (CHERI_REG_KDC);				\

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
#define	SAVE_U_PCB_CHERIFRAME(cause, base, treg)			\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_SEC0, CHERIFRAME_OFF_C0, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C1, CHERIFRAME_OFF_C1, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C2, CHERIFRAME_OFF_C2, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C3, CHERIFRAME_OFF_C3, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C4, CHERIFRAME_OFF_C4, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C5, CHERIFRAME_OFF_C5, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C6, CHERIFRAME_OFF_C6, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C7, CHERIFRAME_OFF_C7, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C8, CHERIFRAME_OFF_C8, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C9, CHERIFRAME_OFF_C9, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C10, CHERIFRAME_OFF_C10, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C11, CHERIFRAME_OFF_C11, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C12, CHERIFRAME_OFF_C12, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C13, CHERIFRAME_OFF_C13, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C14, CHERIFRAME_OFF_C14, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C15, CHERIFRAME_OFF_C15, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C16, CHERIFRAME_OFF_C16, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C17, CHERIFRAME_OFF_C17, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C18, CHERIFRAME_OFF_C18, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C19, CHERIFRAME_OFF_C19, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C20, CHERIFRAME_OFF_C20, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C21, CHERIFRAME_OFF_C21, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C22, CHERIFRAME_OFF_C22, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C23, CHERIFRAME_OFF_C23, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_RCC, CHERIFRAME_OFF_RCC, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C25, CHERIFRAME_OFF_C25, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_IDC, CHERIFRAME_OFF_IDC, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CREG(CHERI_REG_EPCC, CHERIFRAME_OFF_PCC, base, treg);\
	SAVE_U_PCB_CHERIFRAME_CAPCAUSE(cause, base, treg)

#define	RESTORE_U_PCB_CHERIFRAME(base, treg)				\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_SEC0, CHERIFRAME_OFF_C0, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C1, CHERIFRAME_OFF_C1, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C2, CHERIFRAME_OFF_C2, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C3, CHERIFRAME_OFF_C3, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C4, CHERIFRAME_OFF_C4, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C5, CHERIFRAME_OFF_C5, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C6, CHERIFRAME_OFF_C6, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C7, CHERIFRAME_OFF_C7, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C8, CHERIFRAME_OFF_C8, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C9, CHERIFRAME_OFF_C9, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C10, CHERIFRAME_OFF_C10, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C11, CHERIFRAME_OFF_C11, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C12, CHERIFRAME_OFF_C12, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C13, CHERIFRAME_OFF_C13, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C14, CHERIFRAME_OFF_C14, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C15, CHERIFRAME_OFF_C15, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C16, CHERIFRAME_OFF_C16, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C17, CHERIFRAME_OFF_C17, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C18, CHERIFRAME_OFF_C18, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C19, CHERIFRAME_OFF_C19, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C20, CHERIFRAME_OFF_C20, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C21, CHERIFRAME_OFF_C21, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C22, CHERIFRAME_OFF_C22, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C23, CHERIFRAME_OFF_C23, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_RCC, CHERIFRAME_OFF_RCC, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_C25, CHERIFRAME_OFF_C25, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_IDC, CHERIFRAME_OFF_IDC, base, treg);\
	RESTORE_U_PCB_CHERIFRAME_CREG(CHERI_REG_EPCC, CHERIFRAME_OFF_PCC, base, treg)

/*
 * Macros saving capability state to, and restoring it from, voluntary kernel
 * context-switch storage in pcb.pcb_cherikframe.
 */
#define	SAVE_U_PCB_CHERIKFRAME_CREG(creg, offs, base, treg)		\
	PTR_ADDIU	treg, base, U_PCB_CHERIKFRAME;			\
	csc		creg, treg, (SZCAP * offs)(CHERI_REG_KDC)

#define	RESTORE_U_PCB_CHERIKFRAME_CREG(creg, offs, base, treg)		\
	PTR_ADDIU	treg, base, U_PCB_CHERIKFRAME;			\
	clc		creg, treg, (SZCAP * offs)(CHERI_REG_KDC)

/*
 * Macros saving a full voluntary kernel CHERI register frame.
 */
#define	SAVE_U_PCB_CHERIKFRAME(base, treg)				\
	SAVE_U_PCB_CHERIKFRAME_CREG(CHERI_REG_C11, CHERIKFRAME_OFF_C11,	\
	    base, treg);						\
	SAVE_U_PCB_CHERIKFRAME_CREG(CHERI_REG_C12, CHERIKFRAME_OFF_C12,	\
	    base, treg);

#define	RESTORE_U_PCB_CHERIKFRAME(base, treg)				\
	RESTORE_U_PCB_CHERIKFRAME_CREG(CHERI_REG_C11,			\
	    CHERIKFRAME_OFF_C11, base, treg);				\
	RESTORE_U_PCB_CHERIKFRAME_CREG(CHERI_REG_C12,			\
	    CHERIKFRAME_OFF_C12, base, treg);

#endif /* _MIPS_INCLUDE_CHERIASM_H_ */
