/* $FreeBSD$ */
/* $NetBSD: trap_subr.S,v 1.20 2002/04/22 23:20:08 kleink Exp $	*/

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
 */

/*
 * NOTICE: This is not a standalone file.  to use it, #include it in
 * your port's locore.S, like so:
 *
 *	#include <powerpc/powerpc/trap_subr.S>
 */

/*
 * Save/restore segment registers
 */
#define RESTORE_SRS(pmap,sr)	mtsr    0,sr; \
	lwz	sr,1*4(pmap);	mtsr	1,sr; \
	lwz	sr,2*4(pmap);	mtsr	2,sr; \
	lwz	sr,3*4(pmap);	mtsr	3,sr; \
	lwz	sr,4*4(pmap);	mtsr	4,sr; \
	lwz	sr,5*4(pmap);	mtsr	5,sr; \
	lwz	sr,6*4(pmap);	mtsr	6,sr; \
	lwz	sr,7*4(pmap);	mtsr	7,sr; \
	lwz	sr,8*4(pmap);	mtsr	8,sr; \
	lwz	sr,9*4(pmap);	mtsr	9,sr; \
	lwz	sr,10*4(pmap);	mtsr	10,sr; \
	lwz	sr,11*4(pmap);	mtsr	11,sr; \
	lwz	sr,12*4(pmap);	mtsr	12,sr; \
	lwz	sr,13*4(pmap);	mtsr	13,sr; \
	lwz	sr,14*4(pmap);	mtsr	14,sr; \
	lwz	sr,15*4(pmap);	mtsr	15,sr; isync;

/*
 * User SRs are loaded through a pointer to the current pmap.
 */
#define RESTORE_USER_SRS(pmap,sr) \
	GET_CPUINFO(pmap); \
	lwz	pmap,PC_CURPMAP(pmap); \
	lwzu	sr,PM_SR(pmap); \
	RESTORE_SRS(pmap,sr)

/*
 * Kernel SRs are loaded directly from kernel_pmap_
 */
#define RESTORE_KERN_SRS(pmap,sr) \
	lis	pmap,CNAME(kernel_pmap_store)@ha; \
	lwzu	sr,CNAME(kernel_pmap_store)+PM_SR@l(pmap); \
	RESTORE_SRS(pmap,sr)

/*
 * FRAME_SETUP assumes:
 *	SPRG1		SP (1)
 *	savearea	r28-r31,DAR,DSISR   (DAR & DSISR only for DSI traps)
 *	r28		LR
 *	r29		CR
 *	r30		scratch
 *	r31		scratch
 *	r1		kernel stack
 *	LR		trap type (from calling address, mask with 0xff00)
 *	SRR0/1		as at start of trap
 */
#define	FRAME_SETUP(savearea)						\
/* Have to enable translation to allow access of kernel stack: */	\
	GET_CPUINFO(%r31);						\
	mfsrr0	%r30;							\
	stw	%r30,(savearea+CPUSAVE_SRR0)(%r31);	/* save SRR0 */	\
	mfsrr1	%r30;							\
	stw	%r30,(savearea+CPUSAVE_SRR1)(%r31);	/* save SRR1 */	\
	mfmsr	%r30;							\
	ori	%r30,%r30,(PSL_DR|PSL_IR|PSL_RI)@l; /* relocation on */	\
	mtmsr	%r30;			/* stack can now be accessed */	\
	isync;								\
	mfsprg1	%r31;			/* get saved SP */		\
	stwu	%r31,-FRAMELEN(%r1);	/* save it in the callframe */	\
	stw	%r0, FRAME_0+8(%r1);	/* save r0 in the trapframe */	\
	stw	%r31,FRAME_1+8(%r1);	/* save SP   "      "       */	\
	stw	%r2, FRAME_2+8(%r1);	/* save r2   "      "       */	\
	stw	%r28,FRAME_LR+8(%r1);	/* save LR   "      "       */	\
	stw	%r29,FRAME_CR+8(%r1);	/* save CR   "      "       */	\
	GET_CPUINFO(%r2);						\
	lwz	%r28,(savearea+CPUSAVE_R28)(%r2); /* get saved r28 */	\
	lwz	%r29,(savearea+CPUSAVE_R29)(%r2); /* get saved r29 */	\
	lwz	%r30,(savearea+CPUSAVE_R30)(%r2); /* get saved r30 */	\
	lwz	%r31,(savearea+CPUSAVE_R31)(%r2); /* get saved r31 */	\
	stw	%r3,  FRAME_3+8(%r1);	/* save r3-r31 */		\
	stw	%r4,  FRAME_4+8(%r1);					\
	stw	%r5,  FRAME_5+8(%r1);					\
	stw	%r6,  FRAME_6+8(%r1);					\
	stw	%r7,  FRAME_7+8(%r1);					\
	stw	%r8,  FRAME_8+8(%r1);					\
	stw	%r9,  FRAME_9+8(%r1);					\
	stw	%r10, FRAME_10+8(%r1);					\
	stw	%r11, FRAME_11+8(%r1);					\
	stw	%r12, FRAME_12+8(%r1);					\
	stw	%r13, FRAME_13+8(%r1);					\
	stw	%r14, FRAME_14+8(%r1);					\
	stw	%r15, FRAME_15+8(%r1);					\
	stw	%r16, FRAME_16+8(%r1);					\
	stw	%r17, FRAME_17+8(%r1);					\
	stw	%r18, FRAME_18+8(%r1);					\
	stw	%r19, FRAME_19+8(%r1);					\
	stw	%r20, FRAME_20+8(%r1);					\
	stw	%r21, FRAME_21+8(%r1);					\
	stw	%r22, FRAME_22+8(%r1);					\
	stw	%r23, FRAME_23+8(%r1);					\
	stw	%r24, FRAME_24+8(%r1);					\
	stw	%r25, FRAME_25+8(%r1);					\
	stw	%r26, FRAME_26+8(%r1);					\
	stw	%r27, FRAME_27+8(%r1);					\
	stw	%r28, FRAME_28+8(%r1);					\
	stw	%r29, FRAME_29+8(%r1);					\
	stw	%r30, FRAME_30+8(%r1);					\
	stw	%r31, FRAME_31+8(%r1);					\
	lwz	%r28,(savearea+CPUSAVE_DAR)(%r2);  /* saved DAR */	\
	lwz	%r29,(savearea+CPUSAVE_DSISR)(%r2);/* saved DSISR */	\
	lwz	%r30,(savearea+CPUSAVE_SRR0)(%r2); /* saved SRR0 */	\
	lwz	%r31,(savearea+CPUSAVE_SRR1)(%r2); /* saved SRR1 */	\
	mfxer	%r3;							\
	mfctr	%r4;							\
	mflr	%r5;							\
	andi.	%r5,%r5,0xff00;		/* convert LR to exc # */	\
	stw	%r3, FRAME_XER+8(1);	/* save xer/ctr/exc */		\
	stw	%r4, FRAME_CTR+8(1);					\
	stw	%r5, FRAME_EXC+8(1);					\
	stw	%r28,FRAME_DAR+8(1);					\
	stw	%r29,FRAME_DSISR+8(1);	/* save dsisr/srr0/srr1 */	\
	stw	%r30,FRAME_SRR0+8(1);					\
	stw	%r31,FRAME_SRR1+8(1)

#define	FRAME_LEAVE(savearea)						\
/* Now restore regs: */							\
	lwz	%r2,FRAME_SRR0+8(%r1);					\
	lwz	%r3,FRAME_SRR1+8(%r1);					\
	lwz	%r4,FRAME_CTR+8(%r1);					\
	lwz	%r5,FRAME_XER+8(%r1);					\
	lwz	%r6,FRAME_LR+8(%r1);					\
	GET_CPUINFO(%r7);						\
	stw	%r2,(savearea+CPUSAVE_SRR0)(%r7); /* save SRR0 */	\
	stw	%r3,(savearea+CPUSAVE_SRR1)(%r7); /* save SRR1 */	\
	lwz	%r7,FRAME_CR+8(%r1);					\
	mtctr	%r4;							\
	mtxer	%r5;							\
	mtlr	%r6;							\
	mtsprg1	%r7;			/* save cr */			\
	lwz	%r31,FRAME_31+8(%r1);   /* restore r0-31 */		\
	lwz	%r30,FRAME_30+8(%r1);					\
	lwz	%r29,FRAME_29+8(%r1);					\
	lwz	%r28,FRAME_28+8(%r1);					\
	lwz	%r27,FRAME_27+8(%r1);					\
	lwz	%r26,FRAME_26+8(%r1);					\
	lwz	%r25,FRAME_25+8(%r1);					\
	lwz	%r24,FRAME_24+8(%r1);					\
	lwz	%r23,FRAME_23+8(%r1);					\
	lwz	%r22,FRAME_22+8(%r1);					\
	lwz	%r21,FRAME_21+8(%r1);					\
	lwz	%r20,FRAME_20+8(%r1);					\
	lwz	%r19,FRAME_19+8(%r1);					\
	lwz	%r18,FRAME_18+8(%r1);					\
	lwz	%r17,FRAME_17+8(%r1);					\
	lwz	%r16,FRAME_16+8(%r1);					\
	lwz	%r15,FRAME_15+8(%r1);					\
	lwz	%r14,FRAME_14+8(%r1);					\
	lwz	%r13,FRAME_13+8(%r1);					\
	lwz	%r12,FRAME_12+8(%r1);					\
	lwz	%r11,FRAME_11+8(%r1);					\
	lwz	%r10,FRAME_10+8(%r1);					\
	lwz	%r9, FRAME_9+8(%r1);					\
	lwz	%r8, FRAME_8+8(%r1);					\
	lwz	%r7, FRAME_7+8(%r1);					\
	lwz	%r6, FRAME_6+8(%r1);					\
	lwz	%r5, FRAME_5+8(%r1);					\
	lwz	%r4, FRAME_4+8(%r1);					\
	lwz	%r3, FRAME_3+8(%r1);					\
	lwz	%r2, FRAME_2+8(%r1);					\
	lwz	%r0, FRAME_0+8(%r1);					\
	lwz	%r1, FRAME_1+8(%r1);					\
/* Can't touch %r1 from here on */					\
	mtsprg2	%r2;			/* save r2 & r3 */		\
	mtsprg3	%r3;							\
/* Disable translation, machine check and recoverability: */		\
	mfmsr	%r2;							\
	andi.	%r2,%r2,~(PSL_DR|PSL_IR|PSL_EE|PSL_ME|PSL_RI)@l;	\
	mtmsr	%r2;							\
	isync;								\
/* Decide whether we return to user mode: */				\
	GET_CPUINFO(%r2);						\
	lwz	%r3,(savearea+CPUSAVE_SRR1)(%r2);			\
	mtcr	%r3;							\
	bf	17,1f;			/* branch if PSL_PR is false */	\
/* Restore user SRs */							\
	RESTORE_USER_SRS(%r2,%r3);					\
1:	mfsprg1	%r2;			/* restore cr */		\
	mtcr	%r2;							\
	GET_CPUINFO(%r2);						\
	lwz	%r3,(savearea+CPUSAVE_SRR0)(%r2); /* restore srr0 */	\
	mtsrr0	%r3;							\
	lwz	%r3,(savearea+CPUSAVE_SRR1)(%r2); /* restore srr1 */	\
	mtsrr1	%r3;							\
	mfsprg2	%r2;			/* restore r2 & r3 */		\
	mfsprg3	%r3

#ifdef KDB
/*
 * Define the kdb debugger stack
 */
	.data
GLOBAL(dbstk)
	.space INTSTK+8			/* kdb stack */
#endif

/*
 * This code gets copied to all the trap vectors
 * (except ISI/DSI, ALI, and the interrupts)
 */
	.text
	.globl	CNAME(trapcode),CNAME(trapsize)
CNAME(trapcode):
	mtsprg1	%r1			/* save SP */
	GET_CPUINFO(%r1)
	stw	%r28,(PC_TEMPSAVE+CPUSAVE_R28)(%r1)	/* free r28-r31 */
	stw	%r29,(PC_TEMPSAVE+CPUSAVE_R29)(%r1)
	stw	%r30,(PC_TEMPSAVE+CPUSAVE_R30)(%r1)
	stw	%r31,(PC_TEMPSAVE+CPUSAVE_R31)(%r1)
	mfsprg1	%r1			/* restore SP, in case of branch */
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
/* Test whether we already had PR set */
	mfsrr1	%r31
	mtcr	%r31
	bla	s_trap			/* LR & 0xff00 is exception # */
CNAME(trapsize) = .-CNAME(trapcode)

/*
 * For ALI: has to save DSISR and DAR
 */
	.globl	CNAME(alitrap),CNAME(alisize)
CNAME(alitrap):
	mtsprg1	%r1			/* save SP */
	GET_CPUINFO(%r1)
	stw	%r28,(PC_TEMPSAVE+CPUSAVE_R28)(%r1)	/* free r28-r31 */
	stw	%r29,(PC_TEMPSAVE+CPUSAVE_R29)(%r1)
	stw	%r30,(PC_TEMPSAVE+CPUSAVE_R30)(%r1)
	stw	%r31,(PC_TEMPSAVE+CPUSAVE_R31)(%r1)
	mfdar	%r30
	mfdsisr	%r31
	stw	%r30,(PC_TEMPSAVE+CPUSAVE_DAR)(%r1)
	stw	%r31,(PC_TEMPSAVE+CPUSAVE_DSISR)(%r1)
	mfsprg1	%r1			/* restore SP, in case of branch */
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
/* Test whether we already had PR set */
	mfsrr1	%r31
	mtcr	%r31
	bla	s_trap			/* LR & 0xff00 is exception # */
CNAME(alisize) = .-CNAME(alitrap)

/*
 * Similar to the above for DSI
 * Has to handle BAT spills
 * and standard pagetable spills
 */
	.globl	CNAME(dsitrap),CNAME(dsisize)
CNAME(dsitrap):
	mtsprg1	%r1			/* save SP */
	GET_CPUINFO(%r1)
	stw	%r28,(PC_DISISAVE+CPUSAVE_R28)(%r1)	/* free r28-r31 */
	stw	%r29,(PC_DISISAVE+CPUSAVE_R29)(%r1)
	stw	%r30,(PC_DISISAVE+CPUSAVE_R30)(%r1)
	stw	%r31,(PC_DISISAVE+CPUSAVE_R31)(%r1)
	mfsprg1	%r1			/* restore SP */
	mfcr	%r29			/* save CR */
	mfxer	%r30			/* save XER */
	mtsprg2	%r30			/* in SPRG2 */
	mfsrr1	%r31			/* test kernel mode */
	mtcr	%r31
	bt	17,1f			/* branch if PSL_PR is set */
	mfdar	%r31			/* get fault address */
	rlwinm	%r31,%r31,7,25,28	/* get segment * 8 */

	/* get batu */
	addis	%r31,%r31,CNAME(battable)@ha
	lwz	%r30,CNAME(battable)@l(31)
	mtcr	%r30
	bf	30,1f			/* branch if supervisor valid is
					   false */
	/* get batl */
	lwz	%r31,CNAME(battable)+4@l(31)
/* We randomly use the highest two bat registers here */
	mftb	%r28
	andi.	%r28,%r28,1
	bne	2f
	mtdbatu	2,%r30
	mtdbatl	2,%r31
	b	3f
2:
	mtdbatu	3,%r30
	mtdbatl	3,%r31
3:
	mfsprg2	%r30			/* restore XER */
	mtxer	%r30
	mtcr	%r29			/* restore CR */
	mtsprg1	%r1
	GET_CPUINFO(%r1)
	lwz	%r28,(PC_DISISAVE+CPUSAVE_R28)(%r1)	/* restore r28-r31 */
	lwz	%r29,(PC_DISISAVE+CPUSAVE_R29)(%r1)
	lwz	%r30,(PC_DISISAVE+CPUSAVE_R30)(%r1)
	lwz	%r31,(PC_DISISAVE+CPUSAVE_R31)(%r1)
	mfsprg1	%r1
	rfi				/* return to trapped code */
1:
	mflr	%r28			/* save LR (SP already saved) */
	bla	disitrap
CNAME(dsisize) = .-CNAME(dsitrap)

/*
 * Preamble code for DSI/ISI traps
 */
disitrap:
	GET_CPUINFO(%r1)
	lwz	%r30,(PC_DISISAVE+CPUSAVE_R28)(%r1)
	stw	%r30,(PC_TEMPSAVE+CPUSAVE_R28)(%r1)
	lwz	%r31,(PC_DISISAVE+CPUSAVE_R29)(%r1)
	stw	%r31,(PC_TEMPSAVE+CPUSAVE_R29)(%r1)
	lwz	%r30,(PC_DISISAVE+CPUSAVE_R30)(%r1)
	stw	%r30,(PC_TEMPSAVE+CPUSAVE_R30)(%r1)
	lwz	%r31,(PC_DISISAVE+CPUSAVE_R31)(%r1)
	stw	%r31,(PC_TEMPSAVE+CPUSAVE_R31)(%r1)
	mfdar	%r30
	mfdsisr	%r31
	stw	%r30,(PC_TEMPSAVE+CPUSAVE_DAR)(%r1)
	stw	%r31,(PC_TEMPSAVE+CPUSAVE_DSISR)(%r1)

#ifdef KDB
	/* Try and detect a kernel stack overflow */
	mfsrr1	%r31
	mtcr	%r31
	bt	17,realtrap		/* branch is user mode */
	mfsprg1	%r31			/* get old SP */
	sub.	%r30,%r31,%r30		/* SP - DAR */
	bge	1f
	neg	%r30,%r30		/* modulo value */
1:	cmplwi	%cr0,%r30,4096		/* is DAR within a page of SP? */
	bge	%cr0,realtrap		/* no, too far away. */

	/* Now convert this DSI into a DDB trap.  */
	GET_CPUINFO(%r1)
	lwz	%r30,(PC_TEMPSAVE+CPUSAVE_DAR)(%r1) /* get DAR */
	stw	%r30,(PC_DBSAVE  +CPUSAVE_DAR)(%r1) /* save DAR */
	lwz	%r30,(PC_TEMPSAVE+CPUSAVE_DSISR)(%r1) /* get DSISR */
	lwz	%r30,(PC_DBSAVE  +CPUSAVE_DSISR)(%r1) /* save DSISR */
	lwz	%r30,(PC_DISISAVE+CPUSAVE_R28)(%r1) /* get  r28 */
	stw	%r30,(PC_DBSAVE  +CPUSAVE_R28)(%r1) /* save r28 */
	lwz	%r31,(PC_DISISAVE+CPUSAVE_R29)(%r1) /* get  r29 */
	stw	%r31,(PC_DBSAVE  +CPUSAVE_R29)(%r1) /* save r29 */
	lwz	%r30,(PC_DISISAVE+CPUSAVE_R30)(%r1) /* get  r30 */
	stw	%r30,(PC_DBSAVE  +CPUSAVE_R30)(%r1) /* save r30 */
	lwz	%r31,(PC_DISISAVE+CPUSAVE_R31)(%r1) /* get  r31 */
	stw	%r31,(PC_DBSAVE  +CPUSAVE_R31)(%r1) /* save r31 */
	lis	%r1,dbstk+INTSTK@ha	/* get new SP */
	addi	%r1,%r1,dbstk+INTSTK@l
	b	dbtrap
#endif

	/* XXX need stack probe here */
realtrap:
/* Test whether we already had PR set */
	mfsrr1	%r1
	mtcr	%r1
	mfsprg1	%r1			/* restore SP (might have been
					   overwritten) */
s_trap:
	bf	17,k_trap		/* branch if PSL_PR is false */
	GET_CPUINFO(%r1)
u_trap:
	lwz	%r1,PC_CURPCB(%r1)
	RESTORE_KERN_SRS(%r30,%r31)	/* enable kernel mapping */

/*
 * Now the common trap catching code.
 */
k_trap:
	FRAME_SETUP(PC_TEMPSAVE)
/* Call C interrupt dispatcher: */
trapagain:
	addi	%r3,%r1,8
	bl	CNAME(powerpc_interrupt)
	.globl	CNAME(trapexit)		/* backtrace code sentinel */
CNAME(trapexit):

/* Disable interrupts: */
	mfmsr	%r3
	andi.	%r3,%r3,~PSL_EE@l
	mtmsr	%r3
/* Test AST pending: */
	lwz	%r5,FRAME_SRR1+8(%r1)
	mtcr	%r5
	bf	17,1f			/* branch if PSL_PR is false */

	GET_CPUINFO(%r3)		/* get per-CPU pointer */
	lwz	%r4, PC_CURTHREAD(%r3)	/* deref to get curthread */
	lwz	%r4, TD_FLAGS(%r4)	/* get thread flags value */
	lis	%r5, (TDF_ASTPENDING|TDF_NEEDRESCHED)@h
	ori	%r5,%r5, (TDF_ASTPENDING|TDF_NEEDRESCHED)@l
	and.	%r4,%r4,%r5
	beq	1f
	mfmsr	%r3			/* re-enable interrupts */
	ori	%r3,%r3,PSL_EE@l
	mtmsr	%r3
	isync
	addi	%r3,%r1,8
	bl	CNAME(ast)
	b	trapexit		/* test ast ret value ? */
1:
	FRAME_LEAVE(PC_TEMPSAVE)
	rfi

/*
 *	Temporary: vector-unavailable traps are directed to vector-assist traps
 */
	.globl	CNAME(vectrap),CNAME(vectrapsize)
CNAME(vectrap):
	ba	EXC_VECAST
CNAME(vectrapsize) = .-CNAME(vectrap)

#if defined(KDB)
/*
 * Deliberate entry to dbtrap
 */
	.globl	CNAME(ppc_db_trap)
CNAME(ppc_db_trap):
	mtsprg1	%r1
	mfmsr	%r3
	mtsrr1	%r3
	andi.	%r3,%r3,~(PSL_EE|PSL_ME)@l
	mtmsr	%r3			/* disable interrupts */
	isync
	GET_CPUINFO(%r3)
	stw	%r28,(PC_DBSAVE+CPUSAVE_R28)(%r3)
	stw	%r29,(PC_DBSAVE+CPUSAVE_R29)(%r3)
	stw	%r30,(PC_DBSAVE+CPUSAVE_R30)(%r3)
	stw	%r31,(PC_DBSAVE+CPUSAVE_R31)(%r3)
	mflr	%r28
	li	%r29,EXC_BPT
	mtlr	%r29
	mfcr	%r29
	mtsrr0	%r28

/*
 * Now the kdb trap catching code.
 */
dbtrap:
	FRAME_SETUP(PC_DBSAVE)
/* Call C trap code: */
	addi	%r3,%r1,8
	bl	CNAME(db_trap_glue)
	or.	%r3,%r3,%r3
	bne	dbleave
/* This wasn't for KDB, so switch to real trap: */
	lwz	%r3,FRAME_EXC+8(%r1)	/* save exception */
	GET_CPUINFO(%r4)
	stw	%r3,(PC_DBSAVE+CPUSAVE_R31)(%r4)
	FRAME_LEAVE(PC_DBSAVE)
	mtsprg1	%r1			/* prepare for entrance to realtrap */
	GET_CPUINFO(%r1)
	stw	%r28,(PC_TEMPSAVE+CPUSAVE_R28)(%r1)
	stw	%r29,(PC_TEMPSAVE+CPUSAVE_R29)(%r1)
	stw	%r30,(PC_TEMPSAVE+CPUSAVE_R30)(%r1)
	stw	%r31,(PC_TEMPSAVE+CPUSAVE_R31)(%r1)
	mflr	%r28
	mfcr	%r29
	lwz	%r31,(PC_DBSAVE+CPUSAVE_R31)(%r1)
	mtlr	%r31
	mfsprg1	%r1
	b	realtrap
dbleave:
	FRAME_LEAVE(PC_DBSAVE)
	rfi

/*
 * In case of KDB we want a separate trap catcher for it
 */
	.globl	CNAME(dblow),CNAME(dbsize)
CNAME(dblow):
	mtsprg1	%r1			/* save SP */
	mtsprg2	%r29			/* save r29 */
	mfcr	%r29			/* save CR in r29 */
	mfsrr1	%r1
	mtcr	%r1
	GET_CPUINFO(%r1)
	bf	17,1f			/* branch if privileged */
	stw	%r28,(PC_TEMPSAVE+CPUSAVE_R28)(%r1)	/* free r28 */
        mfsprg2	%r28				/* r29 holds cr ... */
        stw	%r28,(PC_TEMPSAVE+CPUSAVE_R29)(%r1)	/* free r29 */
        stw	%r30,(PC_TEMPSAVE+CPUSAVE_R30)(%r1)	/* free r30 */
        stw	%r31,(PC_TEMPSAVE+CPUSAVE_R31)(%r1)	/* free r31 */
        mflr	%r28					/* save LR */
        bla     u_trap
1:
	stw	%r28,(PC_DBSAVE+CPUSAVE_R28)(%r1)	/* free r28 */
        mfsprg2	%r28				/* r29 holds cr...  */
        stw	%r28,(PC_DBSAVE+CPUSAVE_R29)(%r1)	/* free r29 */
        stw	%r30,(PC_DBSAVE+CPUSAVE_R30)(%r1)	/* free r30 */
        stw	%r31,(PC_DBSAVE+CPUSAVE_R31)(%r1)	/* free r31 */
        mflr	%r28					/* save LR */
	lis	%r1,dbstk+INTSTK@ha	/* get new SP */
	addi	%r1,%r1,dbstk+INTSTK@l
	bla	dbtrap
CNAME(dbsize) = .-CNAME(dblow)
#endif /* KDB */
