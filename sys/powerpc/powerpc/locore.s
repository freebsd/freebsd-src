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
#include <machine/sr.h>
#include <machine/spr.h>
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
GLOBAL(decrnest)
	.long	0

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

#define	INTSTK		8192		/* 8K interrupt stack */
#define	SPILLSTK	4096		/* 4K spill stack */

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
	lwz	4,PC_CURTHREAD(4)
	lwz	4,TD_PCB(4)
	stw	3,PCB_ONFAULT(4)
	stw	0,0(3)
	stw	1,4(3)
	stw	2,8(3)
	stmw	12,12(3)
	xor	3,3,3
	blr

#include <powerpc/powerpc/trap_subr.S>
