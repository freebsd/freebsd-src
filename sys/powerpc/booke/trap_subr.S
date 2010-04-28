/*-
 * Copyright (C) 2006-2009 Semihalf, Rafal Jaworowski <raj@semihalf.com>
 * Copyright (C) 2006 Semihalf, Marian Balakowicz <m8@semihalf.com>
 * Copyright (C) 2006 Juniper Networks, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: $NetBSD: trap_subr.S,v 1.20 2002/04/22 23:20:08 kleink Exp $
 */

/*
 * NOTICE: This is not a standalone file.  to use it, #include it in
 * your port's locore.S, like so:
 *
 *	#include <powerpc/booke/trap_subr.S>
 */

/*
 * SPRG usage notes
 *
 * SPRG0 - pcpu pointer
 * SPRG1 - all interrupts except TLB miss, critical, machine check
 * SPRG2 - critical
 * SPRG3 - machine check
 * SPRG4-6 - scratch
 *
 */

/* Get the per-CPU data structure */
#define GET_CPUINFO(r) mfsprg0 r

#define RES_GRANULE	32
#define RES_LOCK	0	/* offset to the 'lock' word */
#define RES_RECURSE	4	/* offset to the 'recurse' word */

/*
 * Standard interrupt prolog
 *
 * sprg_sp - SPRG{1-3} reg used to temporarily store the SP
 * savearea - temp save area (pc_{tempsave, disisave, critsave, mchksave})
 * isrr0-1 - save restore registers with CPU state at interrupt time (may be
 *           SRR0-1, CSRR0-1, MCSRR0-1
 *
 * 1. saves in the given savearea:
 *   - R30-31
 *   - DEAR, ESR
 *   - xSRR0-1
 *
 * 2. saves CR -> R30
 *
 * 3. switches to kstack if needed
 *
 * 4. notes:
 *   - R31 can be used as scratch register until a new frame is layed on
 *     the stack with FRAME_SETUP
 *
 *   - potential TLB miss: NO. Saveareas are always acessible via TLB1 
 *     permanent entries, and within this prolog we do not dereference any
 *     locations potentially not in the TLB
 */
#define STANDARD_PROLOG(sprg_sp, savearea, isrr0, isrr1)		\
	mtspr	sprg_sp, %r1;		/* Save SP */			\
	GET_CPUINFO(%r1);		/* Per-cpu structure */		\
	stw	%r30, (savearea+CPUSAVE_R30)(%r1);			\
	stw	%r31, (savearea+CPUSAVE_R31)(%r1); 			\
	mfdear	%r30;		 					\
	mfesr	%r31;							\
	stw	%r30, (savearea+CPUSAVE_BOOKE_DEAR)(%r1); 		\
	stw	%r31, (savearea+CPUSAVE_BOOKE_ESR)(%r1); 		\
	mfspr	%r30, isrr0;						\
	mfspr	%r31, isrr1;	 	/* MSR at interrupt time */	\
	stw	%r30, (savearea+CPUSAVE_SRR0)(%r1);			\
	stw	%r31, (savearea+CPUSAVE_SRR1)(%r1);			\
	isync;			 					\
	mfspr	%r1, sprg_sp;	 	/* Restore SP */		\
	mfcr	%r30;		 	/* Save CR */			\
	/* switch to per-thread kstack if intr taken in user mode */	\
	mtcr	%r31;			/* MSR at interrupt time  */	\
	bf	17, 1f;							\
	GET_CPUINFO(%r1);		/* Per-cpu structure */		\
	lwz	%r1, PC_CURPCB(%r1); 	/* Per-thread kernel stack */	\
1:

#define	STANDARD_CRIT_PROLOG(sprg_sp, savearea, isrr0, isrr1)		\
	mtspr	sprg_sp, %r1;		/* Save SP */			\
	GET_CPUINFO(%r1);		/* Per-cpu structure */		\
	stw	%r30, (savearea+CPUSAVE_R30)(%r1);			\
	stw	%r31, (savearea+CPUSAVE_R31)(%r1);			\
	mfdear	%r30;							\
	mfesr	%r31;							\
	stw	%r30, (savearea+CPUSAVE_BOOKE_DEAR)(%r1);		\
	stw	%r31, (savearea+CPUSAVE_BOOKE_ESR)(%r1);		\
	mfspr	%r30, isrr0;						\
	mfspr	%r31, isrr1;		/* MSR at interrupt time */	\
	stw	%r30, (savearea+CPUSAVE_SRR0)(%r1);			\
	stw	%r31, (savearea+CPUSAVE_SRR1)(%r1);			\
	mfspr	%r30, SPR_SRR0;						\
	mfspr	%r31, SPR_SRR1;		/* MSR at interrupt time */	\
	stw	%r30, (savearea+CPUSAVE_SRR0+8)(%r1);			\
	stw	%r31, (savearea+CPUSAVE_SRR1+8)(%r1);			\
	isync;								\
	mfspr	%r1, sprg_sp;		/* Restore SP */		\
	mfcr	%r30;			/* Save CR */			\
	/* switch to per-thread kstack if intr taken in user mode */	\
	mtcr	%r31;			/* MSR at interrupt time  */	\
	bf	17, 1f;							\
	GET_CPUINFO(%r1);		/* Per-cpu structure */		\
	lwz	%r1, PC_CURPCB(%r1);	/* Per-thread kernel stack */	\
1:

/*
 * FRAME_SETUP assumes:
 *	SPRG{1-3}	SP at the time interrupt occured
 *	savearea	r30-r31, DEAR, ESR, xSRR0-1
 *	r30		CR
 *	r31		scratch
 *	r1		kernel stack
 *
 * sprg_sp - SPRG reg containing SP at the time interrupt occured
 * savearea - temp save
 * exc - exception number (EXC_xxx)
 *
 * 1. sets a new frame
 * 2. saves in the frame:
 *   - R0, R1 (SP at the time of interrupt), R2, LR, CR
 *   - R3-31 (R30-31 first restored from savearea)
 *   - XER, CTR, DEAR, ESR (from savearea), xSRR0-1
 *
 * Notes:
 * - potential TLB miss: YES, since we make dereferences to kstack, which
 *   can happen not covered (we can have up to two DTLB misses if fortunate
 *   enough i.e. when kstack crosses page boundary and both pages are
 *   untranslated)
 */
#define	FRAME_SETUP(sprg_sp, savearea, exc)				\
	mfspr	%r31, sprg_sp;		/* get saved SP */		\
	/* establish a new stack frame and put everything on it */	\
	stwu	%r31, -FRAMELEN(%r1);					\
	stw	%r0, FRAME_0+8(%r1);	/* save r0 in the trapframe */	\
	stw	%r31, FRAME_1+8(%r1);	/* save SP   "      "       */	\
	stw	%r2, FRAME_2+8(%r1);	/* save r2   "      "       */	\
	mflr	%r31;		 					\
	stw	%r31, FRAME_LR+8(%r1);	/* save LR   "      "       */	\
	stw	%r30, FRAME_CR+8(%r1);	/* save CR   "      "       */	\
	GET_CPUINFO(%r2);						\
	lwz	%r30, (savearea+CPUSAVE_R30)(%r2); /* get saved r30 */	\
	lwz	%r31, (savearea+CPUSAVE_R31)(%r2); /* get saved r31 */	\
	/* save R3-31 */						\
	stmw	%r3,  FRAME_3+8(%r1) ;					\
	/* save DEAR, ESR */						\
	lwz	%r28, (savearea+CPUSAVE_BOOKE_DEAR)(%r2);		\
	lwz	%r29, (savearea+CPUSAVE_BOOKE_ESR)(%r2);		\
	stw	%r28, FRAME_BOOKE_DEAR+8(%r1);				\
	stw	%r29, FRAME_BOOKE_ESR+8(%r1);				\
	/* save XER, CTR, exc number */					\
	mfxer	%r3;							\
	mfctr	%r4;							\
	stw	%r3, FRAME_XER+8(%r1);					\
	stw	%r4, FRAME_CTR+8(%r1);					\
	li	%r5, exc;						\
	stw	%r5, FRAME_EXC+8(%r1);					\
	/* save DBCR0 */						\
	mfspr	%r3, SPR_DBCR0;						\
	stw	%r3, FRAME_BOOKE_DBCR0+8(%r1);				\
	/* save xSSR0-1 */						\
	lwz	%r30, (savearea+CPUSAVE_SRR0)(%r2);			\
	lwz	%r31, (savearea+CPUSAVE_SRR1)(%r2);			\
	stw	%r30, FRAME_SRR0+8(%r1);				\
	stw	%r31, FRAME_SRR1+8(%r1)

/*
 *
 * isrr0-1 - save restore registers to restore CPU state to (may be
 *           SRR0-1, CSRR0-1, MCSRR0-1
 *
 * Notes:
 *  - potential TLB miss: YES. The deref'd kstack may be not covered
 */
#define	FRAME_LEAVE(isrr0, isrr1)					\
	/* restore CTR, XER, LR, CR */					\
	lwz	%r4, FRAME_CTR+8(%r1);					\
	lwz	%r5, FRAME_XER+8(%r1);					\
	lwz	%r6, FRAME_LR+8(%r1);					\
	lwz	%r7, FRAME_CR+8(%r1);					\
	mtctr	%r4;							\
	mtxer	%r5;							\
	mtlr	%r6;							\
	mtcr	%r7;							\
	/* restore DBCR0 */						\
	lwz	%r4, FRAME_BOOKE_DBCR0+8(%r1);				\
	mtspr	SPR_DBCR0, %r4;						\
	/* restore xSRR0-1 */						\
	lwz	%r30, FRAME_SRR0+8(%r1);				\
	lwz	%r31, FRAME_SRR1+8(%r1);				\
	mtspr	isrr0, %r30;						\
	mtspr	isrr1, %r31;						\
	/* restore R2-31, SP */						\
	lmw	%r2, FRAME_2+8(%r1) ;					\
	lwz	%r0, FRAME_0+8(%r1);					\
	lwz	%r1, FRAME_1+8(%r1);					\
	isync

/*
 * TLB miss prolog
 *
 * saves LR, CR, SRR0-1, R20-31 in the TLBSAVE area
 *
 * Notes:
 *  - potential TLB miss: NO. It is crucial that we do not generate a TLB
 *    miss within the TLB prolog itself!
 *  - TLBSAVE is always translated
 */
#define TLB_PROLOG							\
	mtsprg4	%r1;			/* Save SP */			\
	mtsprg5 %r28;							\
	mtsprg6 %r29;							\
	/* calculate TLB nesting level and TLBSAVE instance address */	\
	GET_CPUINFO(%r1);	 	/* Per-cpu structure */		\
	lwz	%r28, PC_BOOKE_TLB_LEVEL(%r1);				\
	rlwinm	%r29, %r28, 6, 23, 25;	/* 4 x TLBSAVE_LEN */		\
	addi	%r28, %r28, 1;						\
	stw	%r28, PC_BOOKE_TLB_LEVEL(%r1);				\
	addi	%r29, %r29, PC_BOOKE_TLBSAVE@l; 			\
	add	%r1, %r1, %r29;		/* current TLBSAVE ptr */	\
									\
	/* save R20-31 */						\
	mfsprg5 %r28;		 					\
	mfsprg6 %r29;							\
	stmw	%r20, (TLBSAVE_BOOKE_R20)(%r1);				\
	/* save LR, CR */						\
	mflr	%r30;		 					\
	mfcr	%r31;							\
	stw	%r30, (TLBSAVE_BOOKE_LR)(%r1);				\
	stw	%r31, (TLBSAVE_BOOKE_CR)(%r1);				\
	/* save SRR0-1 */						\
	mfsrr0	%r30;		/* execution addr at interrupt time */	\
	mfsrr1	%r31;		/* MSR at interrupt time*/		\
	stw	%r30, (TLBSAVE_BOOKE_SRR0)(%r1);	/* save SRR0 */	\
	stw	%r31, (TLBSAVE_BOOKE_SRR1)(%r1);	/* save SRR1 */	\
	isync;								\
	mfsprg4	%r1

/*
 * restores LR, CR, SRR0-1, R20-31 from the TLBSAVE area
 *
 * same notes as for the TLB_PROLOG
 */
#define TLB_RESTORE							\
	mtsprg4	%r1;			/* Save SP */			\
	GET_CPUINFO(%r1);	 	/* Per-cpu structure */		\
	/* calculate TLB nesting level and TLBSAVE instance addr */	\
	lwz	%r28, PC_BOOKE_TLB_LEVEL(%r1);				\
	subi	%r28, %r28, 1;						\
	stw	%r28, PC_BOOKE_TLB_LEVEL(%r1);				\
	rlwinm	%r29, %r28, 6, 23, 25; /* 4 x TLBSAVE_LEN */		\
	addi	%r29, %r29, PC_BOOKE_TLBSAVE@l;				\
	add	%r1, %r1, %r29;						\
									\
	/* restore LR, CR */						\
	lwz	%r30, (TLBSAVE_BOOKE_LR)(%r1);				\
	lwz	%r31, (TLBSAVE_BOOKE_CR)(%r1);				\
	mtlr	%r30;							\
	mtcr	%r31;							\
	/* restore SRR0-1 */						\
	lwz	%r30, (TLBSAVE_BOOKE_SRR0)(%r1);			\
	lwz	%r31, (TLBSAVE_BOOKE_SRR1)(%r1);			\
	mtsrr0	%r30;							\
	mtsrr1	%r31;							\
	/* restore R20-31 */						\
	lmw	%r20, (TLBSAVE_BOOKE_R20)(%r1);				\
	mfsprg4	%r1

#ifdef SMP
#define TLB_LOCK							\
	GET_CPUINFO(%r20);						\
	lwz	%r21, PC_CURTHREAD(%r20);				\
	lwz	%r22, PC_BOOKE_TLB_LOCK(%r20);				\
									\
1:	lwarx	%r23, 0, %r22;						\
	cmpwi	%r23, MTX_UNOWNED;					\
	beq	2f;							\
									\
	/* check if this is recursion */				\
	cmplw	cr0, %r21, %r23;					\
	bne-	1b;							\
									\
2:	/* try to acquire lock */					\
	stwcx.	%r21, 0, %r22;						\
	bne-	1b;							\
									\
	/* got it, update recursion counter */				\
	lwz	%r21, RES_RECURSE(%r22);				\
	addi	%r21, %r21, 1;						\
	stw	%r21, RES_RECURSE(%r22);				\
	isync;								\
	msync

#define TLB_UNLOCK							\
	GET_CPUINFO(%r20);						\
	lwz	%r21, PC_CURTHREAD(%r20);				\
	lwz	%r22, PC_BOOKE_TLB_LOCK(%r20);				\
									\
	/* update recursion counter */					\
	lwz	%r23, RES_RECURSE(%r22);				\
	subi	%r23, %r23, 1;						\
	stw	%r23, RES_RECURSE(%r22);				\
									\
	cmpwi	%r23, 0;						\
	bne	1f;							\
	isync;								\
	msync;								\
									\
	/* release the lock */						\
	li	%r23, MTX_UNOWNED;					\
	stw	%r23, 0(%r22);						\
1:	isync;								\
	msync
#else
#define TLB_LOCK
#define TLB_UNLOCK
#endif	/* SMP */

#define INTERRUPT(label)						\
	.globl	label;							\
	.align	5;							\
	CNAME(label):

/*
 * Interrupt handling routines in BookE can be flexibly placed and do not have
 * to live in pre-defined vectors location. Note they need to be TLB-mapped at
 * all times in order to be able to handle exceptions. We thus arrange for
 * them to be part of kernel text which is always TLB-accessible.
 *
 * The interrupt handling routines have to be 16 bytes aligned: we align them
 * to 32 bytes (cache line length) which supposedly performs better.
 *
 */
	.text
	.globl CNAME(interrupt_vector_base)
	.align 5
interrupt_vector_base:

/*****************************************************************************
 * Critical input interrupt
 ****************************************************************************/
INTERRUPT(int_critical_input)
	STANDARD_PROLOG(SPR_SPRG2, PC_BOOKE_CRITSAVE, SPR_CSRR0, SPR_CSRR1)
	FRAME_SETUP(SPR_SPRG2, PC_BOOKE_CRITSAVE, EXC_CRIT)
	addi	%r3, %r1, 8
	bl	CNAME(powerpc_crit_interrupt)
	FRAME_LEAVE(SPR_CSRR0, SPR_CSRR1)
	rfci


/*****************************************************************************
 * Machine check interrupt
 ****************************************************************************/
INTERRUPT(int_machine_check)
	STANDARD_PROLOG(SPR_SPRG3, PC_BOOKE_MCHKSAVE, SPR_MCSRR0, SPR_MCSRR1)
	FRAME_SETUP(SPR_SPRG3, PC_BOOKE_MCHKSAVE, EXC_MCHK)
	addi	%r3, %r1, 8
	bl	CNAME(powerpc_mchk_interrupt)
	FRAME_LEAVE(SPR_MCSRR0, SPR_MCSRR1)
	rfmci


/*****************************************************************************
 * Data storage interrupt
 ****************************************************************************/
INTERRUPT(int_data_storage)
	STANDARD_PROLOG(SPR_SPRG1, PC_DISISAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_DISISAVE, EXC_DSI)
	b	trap_common


/*****************************************************************************
 * Instruction storage interrupt
 ****************************************************************************/
INTERRUPT(int_instr_storage)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_ISI)
	b	trap_common


/*****************************************************************************
 * External input interrupt
 ****************************************************************************/
INTERRUPT(int_external_input)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_EXI)
	addi	%r3, %r1, 8
	bl	CNAME(powerpc_extr_interrupt)
	b	trapexit


INTERRUPT(int_alignment)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_ALI)
	b	trap_common


INTERRUPT(int_program)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_PGM)
	b	trap_common


/*****************************************************************************
 * System call
 ****************************************************************************/
INTERRUPT(int_syscall)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_SC)
	b	trap_common


/*****************************************************************************
 * Decrementer interrupt
 ****************************************************************************/
INTERRUPT(int_decrementer)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_DECR)
	addi	%r3, %r1, 8
	bl	CNAME(powerpc_decr_interrupt)
	b	trapexit


/*****************************************************************************
 * Fixed interval timer
 ****************************************************************************/
INTERRUPT(int_fixed_interval_timer)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_FIT)
	b	trap_common


/*****************************************************************************
 * Watchdog interrupt
 ****************************************************************************/
INTERRUPT(int_watchdog)
	STANDARD_PROLOG(SPR_SPRG1, PC_TEMPSAVE, SPR_SRR0, SPR_SRR1)
	FRAME_SETUP(SPR_SPRG1, PC_TEMPSAVE, EXC_WDOG)
	b	trap_common


/*****************************************************************************
 * Data TLB miss interrupt
 *
 * There can be nested TLB misses - while handling a TLB miss we reference
 * data structures that may be not covered by translations. We support up to
 * TLB_NESTED_MAX-1 nested misses.
 * 
 * Registers use:
 *	r31 - dear
 *	r30 - unused
 *	r29 - saved mas0
 *	r28 - saved mas1
 *	r27 - saved mas2
 *	r26 - pmap address
 *	r25 - pte address
 *
 *	r20:r23 - scratch registers
 ****************************************************************************/
INTERRUPT(int_data_tlb_error)
	TLB_PROLOG
	TLB_LOCK

	mfdear	%r31

	/*
	 * Save MAS0-MAS2 registers. There might be another tlb miss during
	 * pte lookup overwriting current contents (which was hw filled).
	 */
	mfspr	%r29, SPR_MAS0
	mfspr	%r28, SPR_MAS1
	mfspr	%r27, SPR_MAS2

	/* Check faulting address. */
	lis	%r21, VM_MAXUSER_ADDRESS@h
	ori	%r21, %r21, VM_MAXUSER_ADDRESS@l
	cmplw	cr0, %r31, %r21
	blt	search_user_pmap
	
	/* If it's kernel address, allow only supervisor mode misses. */
	mfsrr1	%r21
	mtcr	%r21
	bt	17, search_failed	/* check MSR[PR] */

search_kernel_pmap:
	/* Load r26 with kernel_pmap address */
	lis	%r26, kernel_pmap_store@h
	ori	%r26, %r26, kernel_pmap_store@l

	/* Force kernel tid, set TID to 0 in MAS1. */
	li	%r21, 0
	rlwimi	%r28, %r21, 0, 8, 15	/* clear TID bits */

tlb_miss_handle:
	/* This may result in nested tlb miss. */
	bl	pte_lookup		/* returns PTE address in R25 */

	cmpwi	%r25, 0			/* pte found? */
	beq	search_failed

	/* Finish up, write TLB entry. */
	bl	tlb_fill_entry

tlb_miss_return:
	TLB_UNLOCK
	TLB_RESTORE
	rfi

search_user_pmap:
	/* Load r26 with current user space process pmap */
	GET_CPUINFO(%r26)
	lwz	%r26, PC_CURPMAP(%r26)

	b	tlb_miss_handle

search_failed:
	/*
	 * Whenever we don't find a TLB mapping in PT, set a TLB0 entry with
	 * the faulting virtual address anyway, but put a fake RPN and no
	 * access rights. This should cause a following {D,I}SI exception.
	 */
	lis	%r23, 0xffff0000@h	/* revoke all permissions */

	/* Load MAS registers. */
	mtspr	SPR_MAS0, %r29
	isync
	mtspr	SPR_MAS1, %r28
	isync
	mtspr	SPR_MAS2, %r27
	isync
	mtspr	SPR_MAS3, %r23
	isync

	tlbwe
	msync
	isync
	b	tlb_miss_return

/*****************************************************************************
 *
 * Return pte address that corresponds to given pmap/va.  If there is no valid
 * entry return 0.
 *
 * input: r26 - pmap
 * input: r31 - dear
 * output: r25 - pte address
 *
 * scratch regs used: r21
 *
 ****************************************************************************/
pte_lookup:
	cmpwi	%r26, 0
	beq	1f			/* fail quickly if pmap is invalid */

	srwi	%r21, %r31, PDIR_SHIFT		/* pdir offset */
	slwi	%r21, %r21, PDIR_ENTRY_SHIFT	/* multiply by pdir entry size */

	addi	%r25, %r26, PM_PDIR	/* pmap pm_dir[] address */
	add	%r25, %r25, %r21	/* offset within pm_pdir[] table */
	/*
	 * Get ptbl address, i.e. pmap->pm_pdir[pdir_idx]
	 * This load may cause a Data TLB miss for non-kernel pmap!
	 */
	lwz	%r25, 0(%r25)
	cmpwi	%r25, 0
	beq	2f

	lis	%r21, PTBL_MASK@h
	ori	%r21, %r21, PTBL_MASK@l
	and	%r21, %r21, %r31

	/* ptbl offset, multiply by ptbl entry size */
	srwi	%r21, %r21, (PTBL_SHIFT - PTBL_ENTRY_SHIFT)

	add	%r25, %r25, %r21		/* address of pte entry */
	/*
	 * Get pte->flags
	 * This load may cause a Data TLB miss for non-kernel pmap!
	 */
	lwz	%r21, PTE_FLAGS(%r25)
	andis.	%r21, %r21, PTE_VALID@h
	bne	2f
1:
	li	%r25, 0
2:
	blr

/*****************************************************************************
 *
 * Load MAS1-MAS3 registers with data, write TLB entry
 *
 * input:
 * r29 - mas0
 * r28 - mas1
 * r27 - mas2
 * r25 - pte
 *
 * output: none
 *
 * scratch regs: r21-r23
 *
 ****************************************************************************/
tlb_fill_entry:
	/*
	 * Update PTE flags: we have to do it atomically, as pmap_protect()
	 * running on other CPUs could attempt to update the flags at the same
	 * time.
	 */
	li	%r23, PTE_FLAGS
1:
	lwarx	%r21, %r23, %r25		/* get pte->flags */
	oris	%r21, %r21, PTE_REFERENCED@h	/* set referenced bit */

	andi.	%r22, %r21, (PTE_UW | PTE_UW)@l	/* check if writable */
	beq	2f
	oris	%r21, %r21, PTE_MODIFIED@h	/* set modified bit */
2:
	stwcx.	%r21, %r23, %r25		/* write it back */
	bne-	1b

	/* Update MAS2. */
	rlwimi	%r27, %r21, 0, 27, 30		/* insert WIMG bits from pte */

	/* Setup MAS3 value in r23. */
	lwz	%r23, PTE_RPN(%r25)		/* get pte->rpn */

	rlwimi	%r23, %r21, 24, 26, 31		/* insert protection bits from pte */

	/* Load MAS registers. */
	mtspr	SPR_MAS0, %r29
	isync
	mtspr	SPR_MAS1, %r28
	isync
	mtspr	SPR_MAS2, %r27
	isync
	mtspr	SPR_MAS3, %r23
	isync

	tlbwe
	isync
	msync
	blr

/*****************************************************************************
 * Instruction TLB miss interrupt
 *
 * Same notes as for the Data TLB miss
 ****************************************************************************/
INTERRUPT(int_inst_tlb_error)
	TLB_PROLOG
	TLB_LOCK

	mfsrr0	%r31			/* faulting address */

	/*
	 * Save MAS0-MAS2 registers. There might be another tlb miss during pte
	 * lookup overwriting current contents (which was hw filled).
	 */
	mfspr	%r29, SPR_MAS0
	mfspr	%r28, SPR_MAS1
	mfspr	%r27, SPR_MAS2

	mfsrr1	%r21
	mtcr	%r21

	/* check MSR[PR] */
	bt	17, search_user_pmap
	b	search_kernel_pmap


	.globl	interrupt_vector_top
interrupt_vector_top:

/*****************************************************************************
 * Debug interrupt
 ****************************************************************************/
INTERRUPT(int_debug)
	STANDARD_CRIT_PROLOG(SPR_SPRG2, PC_BOOKE_CRITSAVE, SPR_CSRR0, SPR_CSRR1)
	FRAME_SETUP(SPR_SPRG2, PC_BOOKE_CRITSAVE, EXC_DEBUG)
	lwz	%r3, (PC_BOOKE_CRITSAVE+CPUSAVE_SRR0)(%r2);
	lis	%r4, interrupt_vector_base@ha
	addi	%r4, %r4, interrupt_vector_base@l
	cmplw	cr0, %r3, %r4
	blt	1f
	lis	%r4, interrupt_vector_top@ha
	addi	%r4, %r4, interrupt_vector_top@l
	cmplw	cr0, %r3, %r4
	bge	1f
	/* Disable single-stepping for the interrupt handlers. */
	lwz	%r3, FRAME_SRR1+8(%r1);
	rlwinm	%r3, %r3, 0, 23, 21
	stw	%r3, FRAME_SRR1+8(%r1);
	/* Restore srr0 and srr1 as they could have been clobbered. */
	lwz	%r3, (PC_BOOKE_CRITSAVE+CPUSAVE_SRR0+8)(%r2);
	mtspr	SPR_SRR0, %r3
	lwz	%r4, (PC_BOOKE_CRITSAVE+CPUSAVE_SRR1+8)(%r2);
	mtspr	SPR_SRR1, %r4
	b	9f
1:
	addi	%r3, %r1, 8
	bl	CNAME(trap)
	/*
	 * Handle ASTs, needed for proper support of single-stepping.
	 * We actually need to return to the process with an rfi.
	 */
	b	trapexit
9:
	FRAME_LEAVE(SPR_CSRR0, SPR_CSRR1)
	rfci


/*****************************************************************************
 * Common trap code
 ****************************************************************************/
trap_common:
	/* Call C trap dispatcher */
	addi	%r3, %r1, 8
	bl	CNAME(trap)

	.globl	CNAME(trapexit)		/* exported for db_backtrace use */
CNAME(trapexit):
	/* disable interrupts */
	wrteei	0

	/* Test AST pending - makes sense for user process only */
	lwz	%r5, FRAME_SRR1+8(%r1)
	mtcr	%r5
	bf	17, 1f

	GET_CPUINFO(%r3)
	lwz	%r4, PC_CURTHREAD(%r3)
	lwz	%r4, TD_FLAGS(%r4)
	lis	%r5, (TDF_ASTPENDING | TDF_NEEDRESCHED)@h
	ori	%r5, %r5, (TDF_ASTPENDING | TDF_NEEDRESCHED)@l
	and.	%r4, %r4, %r5
	beq	1f

	/* re-enable interrupts before calling ast() */
	wrteei	1

	addi	%r3, %r1, 8
	bl	CNAME(ast)
	.globl	CNAME(asttrapexit)	/* db_backtrace code sentinel #2 */
CNAME(asttrapexit):
	b	trapexit		/* test ast ret value ? */
1:
	FRAME_LEAVE(SPR_SRR0, SPR_SRR1)
	rfi


#if defined(KDB)
/*
 * Deliberate entry to dbtrap
 */
	.globl	CNAME(breakpoint)
CNAME(breakpoint):
	mtsprg1	%r1
	mfmsr	%r3
	mtsrr1	%r3
	andi.	%r3, %r3, ~(PSL_EE | PSL_ME)@l
	mtmsr	%r3			/* disable interrupts */
	isync
	GET_CPUINFO(%r3)
	stw	%r30, (PC_DBSAVE+CPUSAVE_R30)(%r3)
	stw	%r31, (PC_DBSAVE+CPUSAVE_R31)(%r3)

	mflr	%r31
	mtsrr0	%r31

	mfdear	%r30
	mfesr	%r31
	stw	%r30, (PC_DBSAVE+CPUSAVE_BOOKE_DEAR)(%r3)
	stw	%r31, (PC_DBSAVE+CPUSAVE_BOOKE_ESR)(%r3)

	mfsrr0	%r30
	mfsrr1	%r31
	stw	%r30, (PC_DBSAVE+CPUSAVE_SRR0)(%r3)
	stw	%r31, (PC_DBSAVE+CPUSAVE_SRR1)(%r3)
	isync

	mfcr	%r30

/*
 * Now the kdb trap catching code.
 */
dbtrap:
	FRAME_SETUP(SPR_SPRG1, PC_DBSAVE, EXC_DEBUG)
/* Call C trap code: */
	addi	%r3, %r1, 8
	bl	CNAME(db_trap_glue)
	or.	%r3, %r3, %r3
	bne	dbleave
/* This wasn't for KDB, so switch to real trap: */
	b	trap_common

dbleave:
	FRAME_LEAVE(SPR_SRR0, SPR_SRR1)
	rfi
#endif /* KDB */

#ifdef SMP
ENTRY(tlb_lock)
	GET_CPUINFO(%r5)
	lwz	%r5, PC_CURTHREAD(%r5)
1:	lwarx	%r4, 0, %r3
	cmpwi	%r4, MTX_UNOWNED
	bne	1b
	stwcx.	%r5, 0, %r3
	bne-	1b
	isync
	msync
	blr

ENTRY(tlb_unlock)
	isync
	msync
	li	%r4, MTX_UNOWNED
	stw	%r4, 0(%r3)
	isync
	msync
	blr
/*
 * TLB miss spin locks. For each CPU we have a reservation granule (32 bytes);
 * only a single word from this granule will actually be used as a spin lock
 * for mutual exclusion between TLB miss handler and pmap layer that
 * manipulates page table contents.
 */
	.data
	.align	5
GLOBAL(tlb0_miss_locks)
	.space	RES_GRANULE * MAXCPU
#endif
