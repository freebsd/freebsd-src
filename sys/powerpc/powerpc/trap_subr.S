/* $FreeBSD$ */
/* $NetBSD: trap_subr.S,v 1.20 2002/04/22 23:20:08 kleink Exp $	*/

/*
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
 * XXX Fake AST trap vector.  This is here until we work out how to safely
 * remove the AST code.
 */
#define	EXC_AST	0x3000
	.data
	.align	4
astpending:
	.long	0
cpassert:
	.asciz	"attempting to return from kernel with no current pmap"

/*
 * Data used during primary/secondary traps/interrupts
 */
#define	tempsave	EXC_MCHK+0xe0 /* primary save area for trap handling */
#define	disisave	EXC_DSI+0xe0  /* primary save area for dsi/isi traps */

/*
 * XXX Interrupt and spill stacks need to be per-CPU.
 */
	.data
	.align	4
intstk:
	.space	INTSTK		/* interrupt stack */

GLOBAL(intr_depth)
	.long	-1		/* in-use marker */

	.comm	spillstk,SPILLSTK,8

/*
 * This code gets copied to all the trap vectors
 * (except ISI/DSI, ALI, the interrupts, and possibly the debugging 
 * traps when using IPKDB).
 */
	.text
	.globl	_C_LABEL(trapcode),_C_LABEL(trapsize)
_C_LABEL(trapcode):
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
/* Test whether we already had PR set */
	mfsrr1	31
	mtcr	31
	bc	4,17,1f			/* branch if PSL_PR is clear */
	mfsprg	31,0
	lwz	1,PC_CURPCB(31)
	addi	1,1,USPACE		/* stack is top of user struct */
1:
	bla	s_trap
_C_LABEL(trapsize) = .-_C_LABEL(trapcode)

/*
 * For ALI: has to save DSISR and DAR
 */
	.globl	_C_LABEL(alitrap),_C_LABEL(alisize)
_C_LABEL(alitrap):
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mfdar	30
	mfdsisr	31
	stmw	30,tempsave+16(0)
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
/* Test whether we already had PR set */
	mfsrr1	31
	mtcr	31
	bc	4,17,1f			/* branch if PSL_PR is clear */
	mfsprg	31,0
	lwz	1,PC_CURPCB(31)
	addi	1,1,USPACE		/* stack is top of user struct */
1:
	bla	s_trap
_C_LABEL(alisize) = .-_C_LABEL(alitrap)

/*
 * Similar to the above for DSI
 * Has to handle BAT spills
 * and standard pagetable spills
 */
	.globl	_C_LABEL(dsitrap),_C_LABEL(dsisize)
_C_LABEL(dsitrap):
	stmw	28,disisave(0)		/* free r28-r31 */
	mfcr	29			/* save CR */
	mfxer	30			/* save XER */
	mtsprg	2,30			/* in SPRG2 */
	mfsrr1	31			/* test kernel mode */
	mtcr	31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfdar	31			/* get fault address */
	rlwinm	31,31,7,25,28		/* get segment * 8 */

	/* get batu */
	addis	31,31,_C_LABEL(battable)@ha
	lwz	30,_C_LABEL(battable)@l(31)
	mtcr	30
	bc	4,30,1f			/* branch if supervisor valid is
					   false */
	/* get batl */
	lwz	31,_C_LABEL(battable)+4@l(31)
/* We randomly use the highest two bat registers here */
	mftb	28
	andi.	28,28,1
	bne	2f
	mtdbatu	2,30
	mtdbatl	2,31
	b	3f
2:
	mtdbatu	3,30
	mtdbatl	3,31
3:
	mfsprg	30,2			/* restore XER */
	mtxer	30
	mtcr	29			/* restore CR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */
1:
	mflr	28			/* save LR */
	bla	s_dsitrap
_C_LABEL(dsisize) = .-_C_LABEL(dsitrap)

/*
 * Dedicated MPC601 version of the above.
 * Considers different BAT format and combined implementation
 * (being addressed as I-BAT).
 */
	.globl	_C_LABEL(dsitrap601),_C_LABEL(dsi601size)
_C_LABEL(dsitrap601):
	stmw	28,disisave(0)		/* free r28-r31 */
	mfcr	29			/* save CR */
	mfxer	30			/* save XER */
	mtsprg	2,30			/* in SPRG2 */
	mfsrr1	31			/* test kernel mode */
	mtcr	31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfdar	31			/* get fault address */
	rlwinm	31,31,12,20,28		/* get "segment" battable offset */

	/* get batl */
	addis	31,31,_C_LABEL(battable)@ha
	lwz	30,_C_LABEL(battable)+4@l(31)
	mtcr	30
	bc	4,25,1f			/* branch if Valid is is false,
					   presently assumes supervisor only */

	/* get batu */
	lwz	31,_C_LABEL(battable)@l(31)
/* We randomly use the highest two bat registers here */
	mfspr	28,SPR_RTCL_R
	andi.	28,28,128
	bne	2f
	mtibatu	2,31
	mtibatl	2,30
	b	3f
2:
	mtibatu	3,31
	mtibatl	3,30
3:
	mfsprg	30,2			/* restore XER */
	mtxer	30
	mtcr	29			/* restore CR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */
1:
	mflr	28			/* save LR */
	bla	s_dsitrap
_C_LABEL(dsi601size) = .-_C_LABEL(dsitrap601)

/*
 * Similar to the above for ISI
 */
	.globl	_C_LABEL(isitrap),_C_LABEL(isisize)
_C_LABEL(isitrap):
	stmw	28,disisave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfsrr1	31			/* test kernel mode */
	mtcr	31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfsrr0	31			/* get fault address */
	rlwinm	31,31,7,25,28		/* get segment * 8 */

	/* get batu */
	addis	31,31,_C_LABEL(battable)@ha
	lwz	30,_C_LABEL(battable)@l(31)
	mtcr	30
	bc	4,30,1f			/* branch if supervisor valid is
					   false */
	mtibatu	3,30

	/* get batl */
	lwz	30,_C_LABEL(battable)+4@l(31)
	mtibatl	3,30

	mtcr	29			/* restore CR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */
1:
	bla	s_isitrap
_C_LABEL(isisize)= .-_C_LABEL(isitrap)

/*
 * Dedicated MPC601 version of the above.
 * Considers different BAT format.
 */
	.globl	_C_LABEL(isitrap601),_C_LABEL(isi601size)
_C_LABEL(isitrap601):
	stmw	28,disisave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfsrr1	31			/* test kernel mode */
	mtcr	31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfsrr0	31			/* get fault address */
	rlwinm	31,31,12,20,28		/* get "segment" battable offset */

	/* get batl */
	addis	31,31,_C_LABEL(battable)@ha
	lwz	30,_C_LABEL(battable)+4@l(31)
	mtcr	30
	bc	4,25,1f			/* branch if Valid is is false,
					   presently assumes supervisor only */
	/* get batu */
	lwz	31,_C_LABEL(battable)@l(31)

	mtibatu	3,31
	mtibatl	3,30

	mtcr	29			/* restore CR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */
1:
	bla	s_isitrap
_C_LABEL(isi601size)= .-_C_LABEL(isitrap601)

/*
 * This one for the external interrupt handler.
 */
	.globl	_C_LABEL(extint),_C_LABEL(extsize)
_C_LABEL(extint):
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfxer	30			/* save XER */
	lis	1,intstk+INTSTK@ha	/* get interrupt stack */
	addi	1,1,intstk+INTSTK@l	/* this is really intr_depth! */
	lwz	31,0(1)			/* were we already running on intstk? */
	addic.	31,31,1
	stw	31,0(1)
	beq	1f
	mfsprg	1,1			/* yes, get old SP */
1:
	ba	extintr
_C_LABEL(extsize) = .-_C_LABEL(extint)

/*
 * And this one for the decrementer interrupt handler.
 */
	.globl	_C_LABEL(decrint),_C_LABEL(decrsize)
_C_LABEL(decrint):
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfxer	30			/* save XER */
	lis	1,intstk+INTSTK@ha	/* get interrupt stack */
	addi	1,1,intstk+INTSTK@l
	lwz	31,0(1)			/* were we already running on intstk? */
	addic.	31,31,1
	stw	31,0(1)
	beq	1f
	mfsprg	1,1			/* yes, get old SP */
1:
	ba	decrintr
_C_LABEL(decrsize) = .-_C_LABEL(decrint)

/*
 * Now the tlb software load for 603 processors:
 * (Code essentially from the 603e User Manual, Chapter 5, but
 * corrected a lot.)
 */

	.globl	_C_LABEL(tlbimiss),_C_LABEL(tlbimsize)
_C_LABEL(tlbimiss):
#ifdef PMAPDEBUG
	mfspr	2,SPR_IMISS		/* exception address */
	li	1,24			/* get rid of the lower */
	srw	2,2,1			/*   24 bits */
	li	1,1			/* Load 1 */
	cmpl	2,1,1			/* is it > 16MB */
	blt	99f			/* nope, skip saving these SPRs */
	li	1,0xc0			/* arbitrary */
	mfspr	2,SPR_HASH1
	stw	2,0(1)
	mfspr	2,SPR_HASH2
	stw	2,4(1)
	mfspr	2,SPR_IMISS
	stw	2,8(1)
	mfspr	2,SPR_ICMP
	stw	2,12(1)
99:
#endif /* PMAPDEBUG */
	mfspr	2,SPR_HASH1		/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,SPR_ICMP		/* get first compare value */
	addi	2,2,-8			/* predec pointer */
1:
	mtctr	1			/* load counter */
2:
	lwzu	1,8(2)			/* get next pte */
	cmpl	0,1,3			/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	1,4(2)			/* load tlb entry lower word */
	andi.	3,1,8			/* check G-bit */
	bne	4f			/* if guarded, take ISI */
	mtctr	0			/* restore counter */
	mfspr	0,SPR_IMISS		/* get the miss address for the tlbli */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	ori	1,1,0x100		/* set the reference bit */
	mtspr	SPR_RPA,1		/* set the pte */
	srwi	1,1,8			/* get byte 7 of pte */
	tlbli	0			/* load the itlb */
	stb	1,6(2)			/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,SPR_HASH2		/* get the second pointer */
	ori	3,3,0x40		/* change the compare value */
	li	1,8
	addi	2,2,-8			/* predec pointer */
	b	1b
4:	/* guarded */
	mfsrr1	3
	andi.	2,3,0xffff		/* clean upper srr1 */
	oris	2,2,0x8000000@h		/* set srr<4> to flag prot violation */
	b	6f
5:	/* not found anywhere */
	mfsrr1	3
	andi.	2,3,0xffff		/* clean upper srr1 */
	oris	2,2,0x40000000@h	/* set srr1<1> to flag pte not found */
6:
	mtctr	0			/* restore counter */
	mtsrr1	2
	mfmsr	0
	xoris	0,0,0x20000@h		/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_ISI
_C_LABEL(tlbimsize) = .-_C_LABEL(tlbimiss)

	.globl	_C_LABEL(tlbdlmiss),_C_LABEL(tlbdlmsize)
_C_LABEL(tlbdlmiss):
	mfspr	2,SPR_HASH1		/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,SPR_DCMP		/* get first compare value */
	addi	2,2,-8			/* predec pointer */
1:
	mtctr	1			/* load counter */
2:
	lwzu	1,8(2)			/* get next pte */
	cmpl	0,1,3			/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	1,4(2)			/* load tlb entry lower word */
	mtctr	0			/* restore counter */
	mfspr	0,SPR_DMISS		/* get the miss address for the tlbld */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	ori	1,1,0x100		/* set the reference bit */
	mtspr	SPR_RPA,1			/* set the pte */
	srwi	1,1,8			/* get byte 7 of pte */
	tlbld	0			/* load the dtlb */
	stb	1,6(2)			/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,SPR_HASH2		/* get the second pointer */
	ori	3,3,0x40		/* change the compare value */
	li	1,8
	addi	2,2,-8			/* predec pointer */
	b	1b
5:	/* not found anywhere */
	mfsrr1	3
	lis	1,0x40000000@h		/* set dsisr<1> to flag pte not found */
	mtctr	0			/* restore counter */
	andi.	2,3,0xffff		/* clean upper srr1 */
	mtsrr1	2
	mtdsisr	1			/* load the dsisr */
	mfspr	1,SPR_DMISS		/* get the miss address */
	mtdar	1			/* put in dar */
	mfmsr	0
	xoris	0,0,0x20000@h		/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_DSI
_C_LABEL(tlbdlmsize) = .-_C_LABEL(tlbdlmiss)

	.globl	_C_LABEL(tlbdsmiss),_C_LABEL(tlbdsmsize)
_C_LABEL(tlbdsmiss):
	mfspr	2,SPR_HASH1		/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,SPR_DCMP		/* get first compare value */
	addi	2,2,-8			/* predec pointer */
1:
	mtctr	1			/* load counter */
2:
	lwzu	1,8(2)			/* get next pte */
	cmpl	0,1,3			/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	1,4(2)			/* load tlb entry lower word */
	andi.	3,1,0x80		/* check the C-bit */
	beq	4f
5:
	mtctr	0			/* restore counter */
	mfspr	0,SPR_DMISS		/* get the miss address for the tlbld */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	mtspr	SPR_RPA,1		/* set the pte */
	tlbld	0			/* load the dtlb */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,SPR_HASH2		/* get the second pointer */
	ori	3,3,0x40		/* change the compare value */
	li	1,8
	addi	2,2,-8			/* predec pointer */
	b	1b
4:	/* found, but C-bit = 0 */
	rlwinm.	3,1,30,0,1		/* test PP */
	bge-	7f
	andi.	3,1,1
	beq+	8f
9:	/* found, but protection violation (PP==00)*/
	mfsrr1	3
	lis	1,0xa000000@h		/* indicate protection violation
					   on store */
	b	1f
7:	/* found, PP=1x */
	mfspr	3,SPR_DMISS		/* get the miss address */
	mfsrin	1,3			/* get the segment register */
	mfsrr1	3
	rlwinm	3,3,18,31,31		/* get PR-bit */
	rlwnm.	2,2,3,1,1		/* get the key */
	bne-	9b			/* protection violation */
8:	/* found, set reference/change bits */
	lwz	1,4(2)			/* reload tlb entry */
	ori	1,1,0x180
	sth	1,6(2)
	b	5b
5:	/* not found anywhere */
	mfsrr1	3
	lis	1,0x42000000@h		/* set dsisr<1> to flag pte not found */
					/* dsisr<6> to flag store */
1:
	mtctr	0			/* restore counter */
	andi.	2,3,0xffff		/* clean upper srr1 */
	mtsrr1	2
	mtdsisr	1			/* load the dsisr */
	mfspr	1,SPR_DMISS		/* get the miss address */
	mtdar	1			/* put in dar */
	mfmsr	0
	xoris	0,0,0x20000@h		/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_DSI
_C_LABEL(tlbdsmsize) = .-_C_LABEL(tlbdsmiss)

#if defined(DDB) || defined(KGDB)
#define	ddbsave	0xde0		/* primary save area for DDB */
/*
 * In case of DDB we want a separate trap catcher for it
 */
	.local	ddbstk
	.comm	ddbstk,INTSTK,8		/* ddb stack */

	.globl	_C_LABEL(ddblow),_C_LABEL(ddbsize)
_C_LABEL(ddblow):
	mtsprg	1,1			/* save SP */
	stmw	28,ddbsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	lis	1,ddbstk+INTSTK@ha	/* get new SP */
	addi	1,1,ddbstk+INTSTK@l
	bla	ddbtrap
_C_LABEL(ddbsize) = .-_C_LABEL(ddblow)
#endif	/* DDB | KGDB */

#ifdef IPKDB
#define	ipkdbsave	0xde0		/* primary save area for IPKDB */
/*
 * In case of IPKDB we want a separate trap catcher for it
 */

	.local	ipkdbstk
	.comm	ipkdbstk,INTSTK,8		/* ipkdb stack */

	.globl	_C_LABEL(ipkdblow),_C_LABEL(ipkdbsize)
_C_LABEL(ipkdblow):
	mtsprg	1,1			/* save SP */
	stmw	28,ipkdbsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	lis	1,ipkdbstk+INTSTK@ha	/* get new SP */
	addi	1,1,ipkdbstk+INTSTK@l
	bla	ipkdbtrap
_C_LABEL(ipkdbsize) = .-_C_LABEL(ipkdblow)
#endif	/* IPKDB */

/*
 * FRAME_SETUP assumes:
 *	SPRG1		SP (1)
 *	savearea	r28-r31,DAR,DSISR	(DAR & DSISR only for DSI traps)
 *	28		LR
 *	29		CR
 *	1		kernel stack
 *	LR		trap type
 *	SRR0/1		as at start of trap
 */
#define	FRAME_SETUP(savearea)						\
/* Have to enable translation to allow access of kernel stack: */	\
	mfsrr0	30;							\
	mfsrr1	31;							\
	stmw	30,savearea+24(0);					\
	mfmsr	30;							\
	ori	30,30,(PSL_DR|PSL_IR);					\
	mtmsr	30;							\
	isync;								\
	mfsprg	31,1;							\
	stwu	31,-FRAMELEN(1);					\
	stw	0,FRAME_0+8(1);						\
	stw	31,FRAME_1+8(1);					\
	stw	28,FRAME_LR+8(1);					\
	stw	29,FRAME_CR+8(1);					\
	lmw	28,savearea(0);						\
	stmw	2,FRAME_2+8(1);						\
	lmw	28,savearea+16(0);					\
	mfxer	3;							\
	mfctr	4;							\
	mflr	5;							\
	andi.	5,5,0xff00;						\
	stw	3,FRAME_XER+8(1);					\
	stw	4,FRAME_CTR+8(1);					\
	stw	5,FRAME_EXC+8(1);					\
	stw	28,FRAME_DAR+8(1);					\
	stw	29,FRAME_DSISR+8(1);					\
	stw	30,FRAME_SRR0+8(1);					\
	stw	31,FRAME_SRR1+8(1)

#define	FRAME_LEAVE(savearea)						\
/* Now restore regs: */							\
	lwz	2,FRAME_SRR0+8(1);					\
	lwz	3,FRAME_SRR1+8(1);					\
	lwz	4,FRAME_CTR+8(1);					\
	lwz	5,FRAME_XER+8(1);					\
	lwz	6,FRAME_LR+8(1);					\
	lwz	7,FRAME_CR+8(1);					\
	stw	2,savearea(0);						\
	stw	3,savearea+4(0);					\
	mtctr	4;							\
	mtxer	5;							\
	mtlr	6;							\
	mtsprg	1,7;			/* save cr */			\
	lmw	2,FRAME_2+8(1);						\
	lwz	0,FRAME_0+8(1);						\
	lwz	1,FRAME_1+8(1);						\
	mtsprg	2,2;			/* save r2 & r3 */		\
	mtsprg	3,3;							\
/* Disable translation, machine check and recoverability: */		\
	mfmsr	2;							\
	andi.	2,2,~(PSL_DR|PSL_IR|PSL_ME|PSL_RI)@l;			\
	mtmsr	2;							\
	isync;								\
/* Decide whether we return to user mode: */				\
	lwz	3,savearea+4(0);					\
	mtcr	3;							\
	bc	4,17,1f;		/* branch if PSL_PR is false */	\
/* Restore user & kernel access SR: */					\
	mfsprg	2,0;							\
	lwz	2,PC_CURPMAP(2);					\
	cmpwi	cr4,2,0;		/* is curpmap NULL? */		\
	bne	cr4,2f;							\
	lis	3,cpassert@ha;		/* if it is, panic */		\
	addi	3,3,cpassert@l;						\
	b	panic;							\
2:	lwz	3,PM_SR+0(2);						\
	mtsr	0,3;			/* restore SR0 */		\
	lwz	3,PM_SR+4(2);						\
	mtsr	1,3;			/* restore SR1 */		\
	lwz	3,PM_SR+8(2);						\
	mtsr	2,3;			/* restore SR2 */		\
	lwz	3,PM_SR+12(2);						\
	mtsr	3,3;			/* restore SR3 */		\
	lwz	3,PM_SR+16(2);						\
	mtsr	4,3;			/* restore SR4 */		\
	lwz	3,PM_SR+20(2);						\
	mtsr	5,3;			/* restore SR5 */		\
	lwz	3,PM_SR+24(2);						\
	mtsr	6,3;			/* restore SR6 */		\
	lwz	3,PM_SR+28(2);						\
	mtsr	7,3;			/* restore SR7 */		\
	lwz	3,PM_USRSR(2);						\
	mtsr	USER_SR,3;						\
	lwz	3,PM_KERNELSR(2);					\
	mtsr	KERNEL_SR,3;						\
1:	mfsprg	2,1;			/* restore cr */		\
	mtcr	2;							\
	lwz	2,savearea(0);						\
	lwz	3,savearea+4(0);					\
	mtsrr0	2;							\
	mtsrr1	3;							\
	mfsprg	2,2;			/* restore r2 & r3 */		\
	mfsprg	3,3

/*
 * Preamble code for DSI/ISI traps
 */
disitrap:
	lmw	30,disisave(0)
	stmw	30,tempsave(0)
	lmw	30,disisave+8(0)
	stmw	30,tempsave+8(0)
	mfdar	30
	mfdsisr	31
	stmw	30,tempsave+16(0)
realtrap:
/* Test whether we already had PR set */
	mfsrr1	1
	mtcr	1
	mfsprg	1,1			/* restore SP (might have been
					   overwritten) */
	bc	4,17,s_trap		/* branch if PSL_PR is false */
	mfsprg	31,0
	lwz	1,PC_CURPCB(31)
	addi	1,1,USPACE		/* stack is top of user struct */

/*
 * Now the common trap catching code.
 */
s_trap:
/* First have to enable KERNEL mapping */
	lis	31,KERNEL_SEGMENT@h
	ori	31,31,KERNEL_SEGMENT@l
	mtsr	KERNEL_SR,31
/* Obliterate SRs so BAT spills work correctly */
	lis	31,EMPTY_SEGMENT@h
	ori	31,31,EMPTY_SEGMENT@l
	mtsr	0,31
	mtsr	1,31
	mtsr	2,31
	mtsr	3,31
	mtsr	4,31
	mtsr	5,31
	mtsr	6,31
	mtsr	7,31
	FRAME_SETUP(tempsave)
/* Now we can recover interrupts again: */
	mfmsr	7
	ori	7,7,(PSL_EE|PSL_ME|PSL_RI)@l
	mtmsr	7
	isync
/* Call C trap code: */
trapagain:
	addi	3,1,8
	bl	_C_LABEL(trap)
	.globl	_C_LABEL(trapexit)
_C_LABEL(trapexit):
/* Disable interrupts: */
	mfmsr	3
	andi.	3,3,~PSL_EE@l
	mtmsr	3
/* Test AST pending: */
	lwz	5,FRAME_SRR1+8(1)
	mtcr	5
	bc	4,17,1f			/* branch if PSL_PR is false */
	lis	3,_C_LABEL(astpending)@ha
	lwz	4,_C_LABEL(astpending)@l(3)
	andi.	4,4,1
	beq	1f
	li	6,EXC_AST
	stw	6,FRAME_EXC+8(1)
	b	trapagain
1:
	FRAME_LEAVE(tempsave)
	rfi

/*
 * DSI second stage fault handler
 */
s_dsitrap:
	mfdsisr	31			/* test whether this may be a
					   spill fault */
	mtcr	31
	mtsprg	1,1			/* save SP */
	bc	4,1,disitrap		/* branch if table miss is false */
	lis	1,spillstk+SPILLSTK@ha
	addi	1,1,spillstk+SPILLSTK@l	/* get spill stack */
	stwu	1,-SPFRAMELEN(1)
	stw	0,SPFRAME_R0(1)		/* save non-volatile registers */
	stw	3,SPFRAME_R3(1)
	stw	4,SPFRAME_R4(1)
	stw	5,SPFRAME_R5(1)
	stw	6,SPFRAME_R6(1)
	stw	7,SPFRAME_R7(1)
	stw	8,SPFRAME_R8(1)
	stw	9,SPFRAME_R9(1)
	stw	10,SPFRAME_R10(1)
	stw	11,SPFRAME_R11(1)
	stw	12,SPFRAME_R12(1)
	mflr	30			/* save trap type */
	mfctr	31			/* & CTR */
	mfdar	3
s_pte_spill:
	bl	_C_LABEL(pmap_pte_spill)	/* try a spill */
	or.	3,3,3
	mtctr	31			/* restore CTR */
	mtlr	30			/* and trap type */
	mfsprg	31,2			/* get saved XER */
	mtxer	31			/* restore XER */
	lwz	12,SPFRAME_R12(1)	/* restore non-volatile registers */
	lwz	11,SPFRAME_R11(1)
	lwz	10,SPFRAME_R10(1)
	lwz	9,SPFRAME_R9(1)
	lwz	8,SPFRAME_R8(1)
	lwz	7,SPFRAME_R7(1)
	lwz	6,SPFRAME_R6(1)
	lwz	5,SPFRAME_R5(1)
	lwz	4,SPFRAME_R4(1)
	lwz	3,SPFRAME_R3(1)
	lwz	0,SPFRAME_R0(1)
	beq	disitrap
	mfsprg	1,1			/* restore SP */
	mtcr	29			/* restore CR */
	mtlr	28			/* restore LR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */

/*
 * ISI second stage fault handler
 */
s_isitrap:
	mfsrr1	31			/* test whether this may be a
					   spill fault */
	mtcr	31
	mtsprg	1,1			/* save SP */
	bc	4,1,disitrap		/* branch if table miss is false */
	lis	1,spillstk+SPILLSTK@ha
	addi	1,1,spillstk+SPILLSTK@l	/* get spill stack */
	stwu	1,-SPFRAMELEN(1)
	stw	0,SPFRAME_R0(1)		/* save non-volatile registers */
	stw	3,SPFRAME_R3(1)
	stw	4,SPFRAME_R4(1)
	stw	5,SPFRAME_R5(1)
	stw	6,SPFRAME_R6(1)
	stw	7,SPFRAME_R7(1)
	stw	8,SPFRAME_R8(1)
	stw	9,SPFRAME_R9(1)
	stw	10,SPFRAME_R10(1)
	stw	11,SPFRAME_R11(1)
	stw	12,SPFRAME_R12(1)
	mfxer	30			/* save XER */
	mtsprg	2,30
	mflr	30			/* save trap type */
	mfctr	31			/* & ctr */
	mfsrr0	3
	b	s_pte_spill		/* above */

/*
 * External interrupt second level handler
 */
#define	INTRENTER							\
/* Save non-volatile registers: */					\
	stwu	1,-IFRAMELEN(1);	/* temporarily */		\
	stw	0,IFRAME_R0(1);						\
	mfsprg	0,1;			/* get original SP */		\
	stw	0,IFRAME_R1(1);		/* and store it */		\
	stw	3,IFRAME_R3(1);						\
	stw	4,IFRAME_R4(1);						\
	stw	5,IFRAME_R5(1);						\
	stw	6,IFRAME_R6(1);						\
	stw	7,IFRAME_R7(1);						\
	stw	8,IFRAME_R8(1);						\
	stw	9,IFRAME_R9(1);						\
	stw	10,IFRAME_R10(1);					\
	stw	11,IFRAME_R11(1);					\
	stw	12,IFRAME_R12(1);					\
	stw	28,IFRAME_LR(1);	/* saved LR */			\
	stw	29,IFRAME_CR(1);	/* saved CR */			\
	stw	30,IFRAME_XER(1);	/* saved XER */			\
	lmw	28,tempsave(0);		/* restore r28-r31 */		\
	mfctr	6;							\
	lis	5,_C_LABEL(intr_depth)@ha;				\
	lwz	5,_C_LABEL(intr_depth)@l(5);				\
	mfsrr0	4;							\
	mfsrr1	3;							\
	stw	6,IFRAME_CTR(1);					\
	stw	5,IFRAME_INTR_DEPTH(1);					\
	stw	4,IFRAME_SRR0(1);					\
	stw	3,IFRAME_SRR1(1);					\
	mtcr	3;							\
	bc	4,17,99f;	/* branch if PSL_PR is false */		\
	lis	3,EMPTY_SEGMENT@h;					\
	ori	3,3,EMPTY_SEGMENT@l;					\
	mtsr	0,3;		/* reset SRs so BAT spills work */	\
	mtsr	1,3;							\
	mtsr	2,3;							\
	mtsr	3,3;							\
	mtsr	4,3;							\
	mtsr	5,3;							\
	mtsr	6,3;							\
	mtsr	7,3;							\
/* interrupts are recoverable here, and enable translation */		\
	lis	3,(KERNEL_SEGMENT|SR_KS|SR_KP)@h;			\
	ori	3,3,(KERNEL_SEGMENT|SR_KS|SR_KP)@l;			\
	mtsr	KERNEL_SR,3;						\
99:	mfmsr	5;							\
	ori	5,5,(PSL_IR|PSL_DR|PSL_RI);				\
	mtmsr	5;							\
	isync

	.globl	_C_LABEL(extint_call)
extintr:
	INTRENTER
_C_LABEL(extint_call):
	bl	_C_LABEL(extint_call)	/* to be filled in later */

intr_exit:
/* Disable interrupts (should already be disabled) and MMU here: */
	mfmsr	3
	andi.	3,3,~(PSL_EE|PSL_ME|PSL_RI|PSL_DR|PSL_IR)@l
	mtmsr	3
	isync
/* restore possibly overwritten registers: */
	lwz	12,IFRAME_R12(1)
	lwz	11,IFRAME_R11(1)
	lwz	10,IFRAME_R10(1)
	lwz	9,IFRAME_R9(1)
	lwz	8,IFRAME_R8(1)
	lwz	7,IFRAME_R7(1)
	lwz	6,IFRAME_SRR1(1)
	lwz	5,IFRAME_SRR0(1)
	lwz	4,IFRAME_CTR(1)
	lwz	3,IFRAME_XER(1)
	mtsrr1	6
	mtsrr0	5
	mtctr	4
	mtxer	3
/* Returning to user mode? */
	mtcr	6			/* saved SRR1 */
	bc	4,17,1f			/* branch if PSL_PR is false */
	mfsprg	31,0
	lwz	3,PC_CURPMAP(31)
	lwz	4,PM_SR+0(3)
	mtsr	0,4			/* Restore SR0 */
	lwz	4,PM_SR+4(3)
	mtsr	1,4			/* Restore SR1 */
	lwz	4,PM_SR+8(3)
	mtsr	2,4			/* Restore SR2 */
	lwz	4,PM_SR+12(3)
	mtsr	3,4			/* Restore SR3 */
	lwz	4,PM_SR+16(3)
	mtsr	4,4			/* Restore SR4 */
	lwz	4,PM_SR+20(3)
	mtsr	5,4			/* Restore SR5 */
	lwz	4,PM_SR+24(3)
	mtsr	6,4			/* Restore SR6 */
	lwz	4,PM_SR+28(3)
	mtsr	7,4			/* Restore SR7 */
	lwz	3,PM_KERNELSR(3)
	mtsr	KERNEL_SR,3		/* Restore kernel SR */
	lis	3,_C_LABEL(astpending)@ha /* Test AST pending */
	lwz	4,_C_LABEL(astpending)@l(3)
	andi.	4,4,1
	beq	1f
/* Setup for entry to realtrap: */
	lwz	3,IFRAME_R1(1)		/* get saved SP */
	mtsprg	1,3
	li	6,EXC_AST
	stmw	28,tempsave(0)		/* establish tempsave again */
	mtlr	6
	lwz	28,IFRAME_LR(1)		/* saved LR */
	lwz	29,IFRAME_CR(1)		/* saved CR */
	lwz	6,IFRAME_R6(1)
	lwz	5,IFRAME_R5(1)
	lwz	4,IFRAME_R4(1)
	lwz	3,IFRAME_R3(1)
	lwz	0,IFRAME_R0(1)
	lis	30,_C_LABEL(intr_depth)@ha /* adjust reentrancy count */
	lwz	31,_C_LABEL(intr_depth)@l(30)
	addi	31,31,-1
	stw	31,_C_LABEL(intr_depth)@l(30)
	b	realtrap
1:
/* Here is the normal exit of extintr: */
	lwz	5,IFRAME_CR(1)
	lwz	6,IFRAME_LR(1)
	mtcr	5
	mtlr	6
	lwz	6,IFRAME_R6(1)
	lwz	5,IFRAME_R5(1)
	lis	3,_C_LABEL(intr_depth)@ha /* adjust reentrancy count */
	lwz	4,_C_LABEL(intr_depth)@l(3)
	addi	4,4,-1
	stw	4,_C_LABEL(intr_depth)@l(3)
	lwz	4,IFRAME_R4(1)
	lwz	3,IFRAME_R3(1)
	lwz	0,IFRAME_R0(1)
	lwz	1,IFRAME_R1(1)
	rfi

/*
 * Decrementer interrupt second level handler
 */
decrintr:
	INTRENTER
	addi	3,1,8			/* intr frame -> clock frame */
	bl	_C_LABEL(decr_intr)
	b	intr_exit

#if defined(DDB)
/*
 * Deliberate entry to ddbtrap
 */
	.globl	_C_LABEL(ddb_trap)
_C_LABEL(ddb_trap):
	mtsprg	1,1
	mfmsr	3
	mtsrr1	3
	andi.	3,3,~(PSL_EE|PSL_ME)@l
	mtmsr	3			/* disable interrupts */
	isync
	stmw	28,ddbsave(0)
	mflr	28
	li	29,EXC_BPT
	mtlr	29
	mfcr	29
	mtsrr0	28
#endif /* DDB */

#if defined(DDB) || defined(KGDB)
/*
 * Now the ddb trap catching code.
 */
ddbtrap:
	FRAME_SETUP(ddbsave)
/* Call C trap code: */
	addi	3,1,8
	bl	_C_LABEL(ddb_trap_glue)
	or.	3,3,3
	bne	ddbleave
/* This wasn't for DDB, so switch to real trap: */
	lwz	3,FRAME_EXC+8(1)	/* save exception */
	stw	3,ddbsave+8(0)
	FRAME_LEAVE(ddbsave)
	mtsprg	1,1			/* prepare for entrance to realtrap */
	stmw	28,tempsave(0)
	mflr	28
	mfcr	29
	lwz	31,ddbsave+8(0)
	mtlr	31
	b	realtrap
ddbleave:
	FRAME_LEAVE(ddbsave)
	rfi
#endif /* DDB || KGDB */

#ifdef IPKDB
/*
 * Deliberate entry to ipkdbtrap
 */
	.globl	_C_LABEL(ipkdb_trap)
_C_LABEL(ipkdb_trap):
	mtsprg	1,1
	mfmsr	3
	mtsrr1	3
	andi.	3,3,~(PSL_EE|PSL_ME)@l
	mtmsr	3			/* disable interrupts */
	isync
	stmw	28,ipkdbsave(0)
	mflr	28
	li	29,EXC_BPT
	mtlr	29
	mfcr	29
	mtsrr0	28

/*
 * Now the ipkdb trap catching code.
 */
ipkdbtrap:
	FRAME_SETUP(ipkdbsave)
/* Call C trap code: */
	addi	3,1,8
	bl	_C_LABEL(ipkdb_trap_glue)
	or.	3,3,3
	bne	ipkdbleave
/* This wasn't for IPKDB, so switch to real trap: */
	lwz	3,FRAME_EXC+8(1)	/* save exception */
	stw	3,ipkdbsave+8(0)
	FRAME_LEAVE(ipkdbsave)
	mtsprg	1,1			/* prepare for entrance to realtrap */
	stmw	28,tempsave(0)
	mflr	28
	mfcr	29
	lwz	31,ipkdbsave+8(0)
	mtlr	31
	b	realtrap
ipkdbleave:
	FRAME_LEAVE(ipkdbsave)
	rfi

ipkdbfault:
	ba	_ipkdbfault
_ipkdbfault:
	mfsrr0	3
	addi	3,3,4
	mtsrr0	3
	li	3,-1
	rfi

/*
 * int ipkdbfbyte(unsigned char *p)
 */
	.globl	_C_LABEL(ipkdbfbyte)
_C_LABEL(ipkdbfbyte):
	li	9,EXC_DSI		/* establish new fault routine */
	lwz	5,0(9)
	lis	6,ipkdbfault@ha
	lwz	6,ipkdbfault@l(6)
	stw	6,0(9)
#ifdef	IPKDBUSERHACK
	lis	8,_C_LABEL(ipkdbsr)@ha
	lwz	8,_C_LABEL(ipkdbsr)@l(8)
	mtsr	USER_SR,8
	isync
#endif
	dcbst	0,9			/* flush data... */
	sync
	icbi	0,9			/* and instruction caches */
	lbz	3,0(3)			/* fetch data */
	stw	5,0(9)			/* restore previous fault handler */
	dcbst	0,9			/* and flush data... */
	sync
	icbi	0,9			/* and instruction caches */
	blr

/*
 * int ipkdbsbyte(unsigned char *p, int c)
 */
	.globl	_C_LABEL(ipkdbsbyte)
_C_LABEL(ipkdbsbyte):
	li	9,EXC_DSI		/* establish new fault routine */
	lwz	5,0(9)
	lis	6,ipkdbfault@ha
	lwz	6,ipkdbfault@l(6)
	stw	6,0(9)
#ifdef	IPKDBUSERHACK
	lis	8,_C_LABEL(ipkdbsr)@ha
	lwz	8,_C_LABEL(ipkdbsr)@l(8)
	mtsr	USER_SR,8
	isync
#endif
	dcbst	0,9			/* flush data... */
	sync
	icbi	0,9			/* and instruction caches */
	mr	6,3
	xor	3,3,3
	stb	4,0(6)
	dcbst	0,6			/* Now do appropriate flushes
					   to data... */
	sync
	icbi	0,6			/* and instruction caches */
	stw	5,0(9)			/* restore previous fault handler */
	dcbst	0,9			/* and flush data... */
	sync
	icbi	0,9			/* and instruction caches */	
	blr
#endif	/* IPKDB */
