/* $FreeBSD$ */
/* $NetBSD: locore.S,v 1.24 2000/05/31 05:09:17 thorpej Exp $ */

/*
 * Copyright (C) 2001 Benno Rice
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
 *
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
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

#include "opt_ddb.h"
#include "opt_ipkdb.h"
#include "assym.s"

#include <sys/syscall.h>

#include <machine/trap.h>
#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/asm.h>

/*
 * Some instructions gas doesn't understand (yet?)
 */
#define	bdneq	bdnzf 2,

/*
 * Globals
 */
	.data
GLOBAL(tmpstk)
	.space	8208
GLOBAL(esym)
	.long	0			/* end of symbol table */
GLOBAL(proc0paddr)
	.long	0			/* proc0 p_addr */
GLOBAL(PTmap)
	.long	0			/* PTmap */

GLOBAL(intrnames)
	.asciz	"irq0", "irq1", "irq2", "irq3"
	.asciz	"irq4", "irq5", "irq6", "irq7"
	.asciz	"irq8", "irq9", "irq10", "irq11"
	.asciz	"irq12", "irq13", "irq14", "irq15"
	.asciz	"irq16", "irq17", "irq18", "irq19"
	.asciz	"irq20", "irq21", "irq22", "irq23"
	.asciz	"irq24", "irq25", "irq26", "irq27"
	.asciz	"irq28", "irq29", "irq30", "irq31"
	.asciz	"irq32", "irq33", "irq34", "irq35"
	.asciz	"irq36", "irq37", "irq38", "irq39"
	.asciz	"irq40", "irq41", "irq42", "irq43"
	.asciz	"irq44", "irq45", "irq46", "irq47"
	.asciz	"irq48", "irq49", "irq50", "irq51"
	.asciz	"irq52", "irq53", "irq54", "irq55"
	.asciz	"irq56", "irq57", "irq58", "irq59"
	.asciz	"irq60", "irq61", "irq62", "irq63"
	.asciz	"clock", "softclock", "softnet", "softserial"
GLOBAL(eintrnames)
	.align	4
GLOBAL(intrcnt)
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
	.long	0,0,0,0
GLOBAL(eintrcnt)

GLOBAL(ofmsr)
	.long	0			/* msr used in Open Firmware */

GLOBAL(powersave)
	.long	0

/*
 * File-scope for locore.S
 */
idle_u:
	.long	0			/* fake uarea during idle after exit */
openfirmware_entry:
	.long	0			/* openfirmware entry point */
srsave:
	.long	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

/*
 * This symbol is here for the benefit of kvm_mkdb, and is supposed to
 * mark the start of kernel text.
 */
	.text
	.globl	kernel_text
kernel_text:

/*
 * Startup entry.  Note, this must be the first thing in the text
 * segment!
 */
	.text
	.globl	__start
__start:
#ifdef	FIRMWORKSBUGS
	mfmsr	0
	andi.	0,0,PSL_IR|PSL_DR
	beq	1f

	bl	ofwr_init
1:
#endif
	li	8,0
	li	9,0x100
	mtctr	9
1:
	dcbf	0,8
	icbi	0,8
	addi	8,8,0x20
	bdnz	1b
	sync
	isync

	mtibatu	0,0
	mtibatu	1,0
	mtibatu	2,0
	mtibatu	3,0
	mtdbatu	0,0
	mtdbatu	1,0
	mtdbatu	2,0
	mtdbatu	3,0

	li	9,0x12
	mtibatl	0,9
	mtdbatl	0,9
	li	9,0x1ffe
	mtibatu	0,9
	mtdbatu	0,9
	isync

	lis	8,openfirmware_entry@ha
	stw	5,openfirmware_entry@l(8) /* save client interface handler */
	mr	3,5

	lis	1,tmpstk@ha
	addi	1,1,tmpstk@l
	addi	1,1,8192

	mfmsr	0
	lis	9,ofmsr@ha
	stw	0,ofmsr@l(9)

	bl	OF_init

	lis	4,end@ha
	addi	4,4,end@l
	mr	5,4
	li	9,PAGE_MASK
	add	4,4,9
	andc	4,4,9
	lis	9,OF_buf@ha
	stw	4,OF_buf@l(9)
	addi	4,4,PAGE_SIZE
	lis	9,proc0paddr@ha
	stw	4,proc0paddr@l(9)
	addi	4,4,USPACE-FRAMELEN
	mr	1,4
	xor	0,0,0
	stwu	0,-16(1)

	lis	3,kernel_text@ha
	addi	3,3,kernel_text@l
#if 0
	mr	5,6
#endif

	bl	powerpc_init
	bl	mi_startup
	b	OF_exit

#if 0 /* XXX: We may switch back to this in the future. */
/*
 * OpenFirmware entry point
 */
ENTRY(openfirmware)
	mflr	0			/* save return address */
	stw	0,4(1)
	stwu	1,-16(1)		/* setup stack frame */

	mfmsr	4			/* save msr */
	stw	4,8(1)

	lis	4,openfirmware_entry@ha	/* get firmware entry point */
	lwz	4,openfirmware_entry@l(4)
	mtlr	4

	li	0,0			/* clear battable translations */
	mtdbatu	2,0
	mtdbatu	3,0
	mtibatu	2,0
	mtibatu	3,0

	lis	4,ofmsr@ha		/* Open Firmware msr */
	lwz	4,ofmsr@l(4)
	mtmsr	4
	isync

	lis	4,srsave@ha		/* save old SR */
	addi	4,4,srsave@l
	li	5,0
1:	mfsrin	0,5
	stw	0,0(4)
	addi	4,4,4
	addis	5,5,0x10000000@h
	cmpwi	5,0
	bne	1b

	lis	4,ofw_pmap@ha		/* load OFW SR */
	addi	4,4,ofw_pmap@l
	lwz	0,PM_KERNELSR(4)
	cmpwi	0,0			/* pm_sr[KERNEL_SR] == 0? */
	beq	2f			/* then skip (not initialized yet) */
	li	5,0
1:	lwz	0,0(4)
	mtsrin	0,5
	addi	4,4,4
	addis	5,5,0x10000000@h
	cmpwi	5,0
	bne	1b
2:
	blrl				/* call Open Firmware */

	mfmsr	4
	li	5,PSL_IR|PSL_DR
	andc 	4,4,5
	mtmsr	4
	isync

	lis	4,srsave@ha		/* restore saved SR */
	addi	4,4,srsave@l
	li	5,0
1:	lwz	0,0(4)
	mtsrin	0,5
	addi	4,4,4
	addis	5,5,0x10000000@h
	cmpwi	5,0
	bne	1b

	lwz	4,8(1)			/* restore msr */
	mtmsr	4
	isync

	lwz	1,0(1)			/* and return */
	lwz	0,4(1)
	mtlr	0
	blr
#endif

/*
 * Switch to/from OpenFirmware real mode stack
 *
 * Note: has to be called as the very first thing in OpenFirmware interface
 * routines.
 * E.g.:
 * int
 * OF_xxx(arg1, arg2)
 * type arg1, arg2;
 * {
 *	static struct {
 *		char *name;
 *		int nargs;
 *		int nreturns;
 *		char *method;
 *		int arg1;
 *		int arg2;
 *		int ret;
 *	} args = {
 *		"xxx",
 *		2,
 *		1,
 *	};
 *
 *	ofw_stack();
 *	args.arg1 = arg1;
 *	args.arg2 = arg2;
 *	if (openfirmware(&args) < 0)
 *		return -1;
 *	return args.ret;
 * }
 */

	.local	firmstk
	.comm	firmstk,PAGE_SIZE,8

ENTRY(ofw_stack)
	mfmsr	8			/* turn off interrupts */
	andi.	0,8,~(PSL_EE|PSL_RI)@l
	mtmsr	0
	stw	8,4(1)			/* abuse return address slot */

	lwz	5,0(1)			/* get length of stack frame */
	subf	5,1,5

	lis	7,firmstk+PAGE_SIZE-8@ha
	addi	7,7,firmstk+PAGE_SIZE-8@l
	lis	6,ofw_back@ha
	addi	6,6,ofw_back@l
	subf	4,5,7			/* make room for stack frame on
					   new stack */
	stw	6,-4(7)			/* setup return pointer */
	stwu	1,-8(7)
	
	stw	7,-8(4)

	addi	3,1,8
	addi	1,4,-8
	subi	5,5,8

	cmpw	3,4
	beqlr

	mr	0,5
	addi	5,5,-1
	cmpwi	0,0
	beqlr

1:	lwz	0,0(3)
	stw	0,0(4)
	addi	3,3,1
	addi	4,4,1
	mr	0,5
	addi	5,5,-1
	cmpwi	0,0
	bne	1b
	blr

ofw_back:
	lwz	1,0(1)			/* get callers original stack pointer */

	lwz	0,4(1)			/* get saved msr from abused slot */
	mtmsr	0
	
	lwz	1,0(1)			/* return */
	lwz	0,4(1)
	mtlr	0
	blr

/*
 * Data used during primary/secondary traps/interrupts
 */
#define	tempsave	0x2e0	/* primary save area for trap handling */
#define	disisave	0x3e0	/* primary save area for dsi/isi traps */

#define	INTSTK	(8*1024)	/* 8K interrupt stack */
	.data
	.align	4
intstk:
	.space	INTSTK		/* interrupt stack */

GLOBAL(intr_depth)
	.long	-1		/* in-use marker */

#define	SPILLSTK 1024		/* 1K spill stack */

	.comm	spillstk,SPILLSTK,8

/*
 * This code gets copied to all the trap vectors
 * (except ISI/DSI, ALI, the interrupts, and possibly the debugging 
 * traps when using IPKDB).
 */
	.text
	.globl	trapcode,trapsize
trapcode:
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
/* Test whether we already had PR set */
	mfsrr1	31
	mtcr	31
	bc	4,17,1f			/* branch if PSL_PR is clear */
	mfsprg	1,0
	lwz	1,PC_CURPCB(1)
	addi	1,1,USPACE		/* stack is top of user struct */
1:
	bla	s_trap
trapsize = .-trapcode

/*
 * For ALI: has to save DSISR and DAR
 */
	.globl	alitrap,alisize
alitrap:
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
	mfsprg	1,0
	lwz	1,PC_CURPCB(1)
	addi	1,1,USPACE		/* stack is top of user struct */
1:
	bla	s_trap
alisize = .-alitrap

/*
 * Similar to the above for DSI
 * Has to handle BAT spills
 * and standard pagetable spills
 */
	.globl	dsitrap,dsisize
dsitrap:
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
	addis	31,31,battable@ha
	lwz	30,battable@l(31)
	mtcr	30
	bc	4,30,1f			/* branch if supervisor valid is
					   false */
	/* get batl */
	lwz	31,battable+4@l(31)
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
dsisize = .-dsitrap

/*
 * Similar to the above for ISI
 */
	.globl	isitrap,isisize
isitrap:
	stmw	28,disisave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfsrr1	31			/* test kernel mode */
	mtcr	31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfsrr0	31			/* get fault address */
	rlwinm	31,31,7,25,28		/* get segment * 8 */

	/* get batu */
	addis	31,31,battable@ha
	lwz	30,battable@l(31)
	mtcr	30
	bc	4,30,1f			/* branch if supervisor valid is
					   false */
	mtibatu	3,30

	/* get batl */
	lwz	30,battable+4@l(31)
	mtibatl	3,30

	mtcr	29			/* restore CR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */
1:
	bla	s_isitrap
isisize = .-isitrap

/*
 * This one for the external interrupt handler.
 */
	.globl	extint,extsize
extint:
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
	ba	extintr
extsize = .-extint

/*
 * And this one for the decrementer interrupt handler.
 */
	.globl	decrint,decrsize
decrint:
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
decrsize = .-decrint

/*
 * Now the tlb software load for 603 processors:
 * (Code essentially from the 603e User Manual, Chapter 5, but
 * corrected a lot.)
 */
#define	DMISS	976
#define	DCMP	977
#define	HASH1	978
#define	HASH2	979
#define	IMISS	980
#define	ICMP	981
#define	RPA	982

	.globl	tlbimiss,tlbimsize
tlbimiss:
	mfspr	2,HASH1			/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,ICMP			/* get first compare value */
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
	mfspr	0,IMISS			/* get the miss address for the tlbli */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	ori	1,1,0x100		/* set the reference bit */
	mtspr	RPA,1			/* set the pte */
	srwi	1,1,8			/* get byte 7 of pte */
	tlbli	0			/* load the itlb */
	stb	1,6(2)			/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,HASH2			/* get the second pointer */
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
tlbimsize = .-tlbimiss

	.globl	tlbdlmiss,tlbdlmsize
tlbdlmiss:
	mfspr	2,HASH1			/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,DCMP			/* get first compare value */
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
	mfspr	0,DMISS			/* get the miss address for the tlbld */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	ori	1,1,0x100		/* set the reference bit */
	mtspr	RPA,1			/* set the pte */
	srwi	1,1,8			/* get byte 7 of pte */
	tlbld	0			/* load the dtlb */
	stb	1,6(2)			/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,HASH2			/* get the second pointer */
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
	mfspr	1,DMISS			/* get the miss address */
	mtdar	1			/* put in dar */
	mfmsr	0
	xoris	0,0,0x20000@h		/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_DSI
tlbdlmsize = .-tlbdlmiss

	.globl	tlbdsmiss,tlbdsmsize
tlbdsmiss:
	mfspr	2,HASH1			/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,DCMP			/* get first compare value */
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
	mfspr	0,DMISS			/* get the miss address for the tlbld */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	mtspr	RPA,1			/* set the pte */
	tlbld	0			/* load the dtlb */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,HASH2			/* get the second pointer */
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
	mfspr	3,DMISS			/* get the miss address */
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
	mfspr	1,DMISS			/* get the miss address */
	mtdar	1			/* put in dar */
	mfmsr	0
	xoris	0,0,0x20000@h		/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_DSI
tlbdsmsize = .-tlbdsmiss

#ifdef DDB
#define	ddbsave	0xde0		/* primary save area for DDB */
/*
 * In case of DDB we want a separate trap catcher for it
 */
	.local	ddbstk
	.comm	ddbstk,INTSTK,8		/* ddb stack */

	.globl	ddblow,ddbsize
ddblow:
	mtsprg	1,1			/* save SP */
	stmw	28,ddbsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	lis	1,ddbstk+INTSTK@ha	/* get new SP */
	addi	1,1,ddbstk+INTSTK@l
	bla	ddbtrap
ddbsize = .-ddblow
#endif	/* DDB */

#ifdef IPKDB
#define	ipkdbsave	0xde0		/* primary save area for IPKDB */
/*
 * In case of IPKDB we want a separate trap catcher for it
 */

	.local	ipkdbstk
	.comm	ipkdbstk,INTSTK,8		/* ipkdb stack */

	.globl	ipkdblow,ipkdbsize
ipkdblow:
	mtsprg	1,1			/* save SP */
	stmw	28,ipkdbsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	lis	1,ipkdbstk+INTSTK@ha	/* get new SP */
	addi	1,1,ipkdbstk+INTSTK@l
	bla	ipkdbtrap
ipkdbsize = .-ipkdblow
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
/*	lis	2,curpm@ha;		get real address of pmap */	\
/*	lwz	2,curpm@l(2);					*/	\
/*	lwz	3,PM_USRSR(2);					*/	\
/*	mtsr	USER_SR,3;					*/	\
/*	lwz	3,PM_KERNELSR(2);				*/	\
/*	mtsr	KERNEL_SR,3;					*/	\
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
	mfsprg	1,0
	lwz	1,PC_CURPCB(1)
	addi	1,1,USPACE		/* stack is top of user struct */

/*
 * Now the common trap catching code.
 */
s_trap:
/* First have to enable KERNEL mapping */
	lis	31,KERNEL_SEGMENT@h
	ori	31,31,KERNEL_SEGMENT@l
	mtsr	KERNEL_SR,31
	FRAME_SETUP(tempsave)
/* Now we can recover interrupts again: */
	mfmsr	7
	ori	7,7,(PSL_EE|PSL_FP|PSL_ME|PSL_RI)@l
	mtmsr	7
	isync
/* Call C trap code: */
	addi	3,1,8
	mr	30,3
	bl	trap
	mr	3,30
	bl	ast
	FRAME_LEAVE(tempsave)
	rfi

/*
 * Child comes here at the end of a fork.
 * Mostly similar to the above.
 */
	.globl	fork_trampoline
fork_trampoline:
	xor	3,3,3
#if 0 /* XXX */
	bl	lcsplx
#endif
	mtlr	31
	mr	3,30
	blrl				/* jump indirect to r31 */
	mr	3,30
	bl	ast
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
	stwu	1,-52(1)
	stw	0,48(1)			/* save non-volatile registers */
	stw	3,44(1)
	stw	4,40(1)
	stw	5,36(1)
	stw	6,32(1)
	stw	7,28(1)
	stw	8,24(1)
	stw	9,20(1)
	stw	10,16(1)
	stw	11,12(1)
	stw	12,8(1)
	mflr	30			/* save trap type */
	mfctr	31			/* & CTR */
	mfdar	3
s_pte_spill:
	bl	pte_spill		/* try a spill */
	or.	3,3,3
	mtctr	31			/* restore CTR */
	mtlr	30			/* and trap type */
	mfsprg	31,2			/* get saved XER */
	mtxer	31			/* restore XER */
	lwz	12,8(1)			/* restore non-volatile registers */
	lwz	11,12(1)
	lwz	10,16(1)
	lwz	9,20(1)
	lwz	8,24(1)
	lwz	7,28(1)
	lwz	6,32(1)
	lwz	5,36(1)
	lwz	4,40(1)
	lwz	3,44(1)
	lwz	0,48(1)
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
	stwu	1,-52(1)
	stw	0,48(1)			/* save non-volatile registers */
	stw	3,44(1)
	stw	4,40(1)
	stw	5,36(1)
	stw	6,32(1)
	stw	7,28(1)
	stw	8,24(1)
	stw	9,20(1)
	stw	10,16(1)
	stw	11,12(1)
	stw	12,8(1)
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
	stwu	1,-88(1);		/* temporarily */		\
	stw	0,84(1);						\
	mfsprg	0,1;			/* get original SP */		\
	stw	0,0(1);			/* and store it */		\
	stw	3,80(1);						\
	stw	4,76(1);						\
	stw	5,72(1);						\
	stw	6,68(1);						\
	stw	7,64(1);						\
	stw	8,60(1);						\
	stw	9,56(1);						\
	stw	10,52(1);						\
	stw	11,48(1);						\
	stw	12,44(1);						\
	stw	28,40(1);		/* saved LR */			\
	stw	29,36(1);		/* saved CR */			\
	stw	30,32(1);		/* saved XER */			\
	lmw	28,tempsave(0);		/* restore r28-r31 */		\
	mfctr	6;							\
	lis	5,intr_depth@ha;					\
	lwz	5,intr_depth@l(5);					\
	mfsrr0	4;							\
	mfsrr1	3;							\
	stw	6,28(1);						\
	stw	5,20(1);						\
	stw	4,12(1);						\
	stw	3,8(1);							\
/* interrupts are recoverable here, and enable translation */		\
	lis	3,(KERNEL_SEGMENT|SR_SUKEY|SR_PRKEY)@h;			\
	ori	3,3,(KERNEL_SEGMENT|SR_SUKEY|SR_PRKEY)@l;		\
	mtsr	KERNEL_SR,3;						\
	mfmsr	5;							\
	ori	5,5,(PSL_IR|PSL_DR|PSL_RI);				\
	mtmsr	5;							\
	isync

	.globl	extint_call
extintr:
	INTRENTER
extint_call:
	bl	extint_call		/* to be filled in later */

intr_exit:
/* Disable interrupts (should already be disabled) and MMU here: */
	mfmsr	3
	andi.	3,3,~(PSL_EE|PSL_ME|PSL_RI|PSL_DR|PSL_IR)@l
	mtmsr	3
	isync
/* restore possibly overwritten registers: */
	lwz	12,44(1)
	lwz	11,48(1)
	lwz	10,52(1)
	lwz	9,56(1)
	lwz	8,60(1)
	lwz	7,64(1)
	lwz	6,8(1)
	lwz	5,12(1)
	lwz	4,28(1)
	lwz	3,32(1)
	mtsrr1	6
	mtsrr0	5
	mtctr	4
	mtxer	3
/* Returning to user mode? */
	mtcr	6			/* saved SRR1 */
	bc	4,17,1f			/* branch if PSL_PR is false */
	mfsprg	3,0			/* get pcpu */
	lwz	3,PC_CURPCB(3)		/* get curpcb from pcpu */
	lwz	3,PCB_PMR(3)		/* get pmap real address from curpcb */
	mtsr	KERNEL_SR,3
/* Setup for entry to realtrap: */
	lwz	3,0(1)			/* get saved SP */
	mtsprg	1,3
#if 0 /* XXX */
	li	6,EXC_AST
#endif
	stmw	28,tempsave(0)		/* establish tempsave again */
	mtlr	6
	lwz	28,40(1)		/* saved LR */
	lwz	29,36(1)		/* saved CR */
	lwz	6,68(1)
	lwz	5,72(1)
	lwz	4,76(1)
	lwz	3,80(1)
	lwz	0,84(1)
	lis	30,intr_depth@ha	 /* adjust reentrancy count */
	lwz	31,intr_depth@l(30)
	addi	31,31,-1
	stw	31,intr_depth@l(30)
	b	realtrap		/* XXX:	should call ast(frame ptr) */
1:
/* Here is the normal exit of extintr: */
	lwz	5,36(1)
	lwz	6,40(1)
	mtcr	5
	mtlr	6
	lwz	6,68(1)
	lwz	5,72(1)
	lis	3,intr_depth@ha		/* adjust reentrancy count */
	lwz	4,intr_depth@l(3)
	addi	4,4,-1
	stw	4,intr_depth@l(3)
	lwz	4,76(1)
	lwz	3,80(1)
	lwz	0,84(1)
	lwz	1,0(1)
	rfi

/*
 * Decrementer interrupt second level handler
 */
decrintr:
	INTRENTER
	addi	3,1,8			/* intr frame */
	bl	decr_intr
	b	intr_exit

#ifdef DDB
/*
 * Deliberate entry to ddbtrap
 */
	.globl	ddb_trap
ddb_trap:
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

/*
 * Now the ddb trap catching code.
 */
ddbtrap:
	FRAME_SETUP(ddbsave)
/* Call C trap code: */
	addi	3,1,8
	bl	ddb_trap_glue
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
#endif /* DDB */

#ifdef IPKDB
/*
 * Deliberate entry to ipkdbtrap
 */
	.globl	ipkdb_trap
ipkdb_trap:
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
	bl	ipkdb_trap_glue
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
	.globl	ipkdbfbyte
ipkdbfbyte:
	li	9,EXC_DSI		/* establish new fault routine */
	lwz	5,0(9)
	lis	6,ipkdbfault@ha
	lwz	6,ipkdbfault@l(6)
	stw	6,0(9)
#ifdef	IPKDBUSERHACK
	lis	8,ipkdbsr@ha
	lwz	8,ipkdbsr@l(8)
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
	.globl	ipkdbsbyte
ipkdbsbyte:
	li	9,EXC_DSI		/* establish new fault routine */
	lwz	5,0(9)
	lis	6,ipkdbfault@ha
	lwz	6,ipkdbfault@l(6)
	stw	6,0(9)
#ifdef	IPKDBUSERHACK
	lis	8,ipkdbsr@ha
	lwz	8,ipkdbsr@l(8)
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
	
/*
 * int setfault()
 *
 * Similar to setjmp to setup for handling faults on accesses to user memory.
 * Any routine using this may only call bcopy, either the form below,
 * or the (currently used) C code optimized, so it doesn't use any non-volatile
 * registers.
 */
	.globl	setfault
setfault:
	mflr	0
	mfcr	12
	mfsprg	4,0
	lwz	4,PC_CURPCB(4)
	stw	3,PCB_ONFAULT(4)
	stw	0,0(3)
	stw	1,4(3)
	stw	2,8(3)
	stmw	12,12(3)
	xor	3,3,3
	blr

/*
 * Signal "trampoline" code.
 */
	.globl	sigcode
sigcode:
	b	sys_exit
esigcode:
	.data
GLOBAL(szsigcode)
	.long	esigcode-sigcode
	.text
	
