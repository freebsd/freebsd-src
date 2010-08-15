/* $FreeBSD$ */
/* $NetBSD: locore.S,v 1.24 2000/05/31 05:09:17 thorpej Exp $ */

/*-
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

#include "assym.s"

#include <sys/syscall.h>

#include <machine/trap.h>
#include <machine/param.h>
#include <machine/spr.h>
#include <machine/asm.h>

/* Locate the per-CPU data structure */
#define GET_CPUINFO(r)  \
        mfsprg0  r

/*
 * Compiled KERNBASE location and the kernel load address
 */
        .globl  kernbase
        .set    kernbase, KERNBASE

#define	TMPSTKSZ	8192		/* 8K temporary stack */
#define	OFWSTKSZ	4096		/* 4K Open Firmware stack */

/*
 * Globals
 */
	.data
	.align	4
GLOBAL(tmpstk)
	.space	TMPSTKSZ
GLOBAL(ofwstk)
	.space	OFWSTKSZ
GLOBAL(esym)
	.llong	0			/* end of symbol table */

GLOBAL(ofmsr)
	.llong	0, 0, 0, 0, 0		/* msr/sprg0-3 used in Open Firmware */

#define	INTRCNT_COUNT	256		/* max(HROWPIC_IRQMAX,OPENPIC_IRQMAX) */
GLOBAL(intrnames)
	.space	INTRCNT_COUNT * (MAXCOMLEN + 1) * 2
GLOBAL(eintrnames)
	.align 4
GLOBAL(intrcnt)
	.space	INTRCNT_COUNT * 4 * 2
GLOBAL(eintrcnt)

/*
 * File-scope for locore.S
 */
idle_u:
	.llong	0			/* fake uarea during idle after exit */
openfirmware_entry:
	.llong	0			/* Open Firmware entry point */
srsave:
	.llong	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0

	.text
	.globl	btext
btext:

/*
 * This symbol is here for the benefit of kvm_mkdb, and is supposed to
 * mark the start of kernel text.
 */
	.globl	kernel_text
kernel_text:

/*
 * Startup entry.  Note, this must be the first thing in the text
 * segment!
 */
	.text
ASENTRY(__start)
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

	/* Save the argument pointer and length */
	mr	20,6
	mr	21,7

	lis	8,openfirmware_entry@ha
	std	5,openfirmware_entry@l(8) /* save client interface handler */

	/* Set up the stack pointer */
	lis	1,(tmpstk+TMPSTKSZ-48)@ha
	addi	1,1,(tmpstk+TMPSTKSZ-48)@l

	/* Set up the TOC pointer */
	lis	2,tocbase@ha
	ld	2,tocbase@l(2)

	mfmsr	0
	lis	9,ofmsr@ha
	stdu	0,ofmsr@l(9)

	mfsprg0	0			/* save SPRG0-3 */
	std	0,8(9)			/* ofmsr[1] = sprg0 */
	mfsprg1 0
	std	0,16(9)			/* ofmsr[2] = sprg1 */
	mfsprg2 0
	std	0,24(9)			/* ofmsr[3] = sprg2 */
	mfsprg3 0
	std	0,32(9)			/* ofmsr[4] = sprg3 */

	/* Switch to 64-bit mode */
	mfmsr	9
	li	8,1
	insrdi	9,8,1,0
	mtmsrd	9
	
	bl	.OF_initial_setup
	nop

	lis	4,end@ha
	addi	4,4,end@l
	mr	5,4

	lis	3,kernbase@ha
	addi	3,3,kernbase@l

	/* Restore the argument pointer and length */
	mr	6,20
	mr	7,21

	bl	.powerpc_init
	nop
	mr	%r1, %r3
	li	%r3, 0
	std	%r3, 0(%r1)
	bl	.mi_startup
	nop
	b	.OF_exit
	nop

/*
 * PPC64 ABI TOC base
 */

        .align  3
	.globl	tocbase
tocbase:
        .llong  .TOC.@tocbase

/*
 * Open Firmware Real-mode Entry Point. This is a huge pain.
 */

ASENTRY(ofw_32bit_mode_entry)
	mflr	%r0
	std	%r0,16(%r1)
	stdu	%r1,-208(%r1)

	/*
	 * We need to save the following, because OF's register save/
	 * restore code assumes that the contents of registers are
	 * at most 32 bits wide: lr, cr, r2, r13-r31, the old MSR. These
	 * get placed in that order in the stack.
	 */

	mfcr	%r4
	std	%r4,48(%r1)
	std	%r13,56(%r1)
	std	%r14,64(%r1)
	std	%r15,72(%r1)
	std	%r16,80(%r1)
	std	%r17,88(%r1)
	std	%r18,96(%r1)
	std	%r19,104(%r1)
	std	%r20,112(%r1)
	std	%r21,120(%r1)
	std	%r22,128(%r1)
	std	%r23,136(%r1)
	std	%r24,144(%r1)
	std	%r25,152(%r1)
	std	%r26,160(%r1)
	std	%r27,168(%r1)
	std	%r28,176(%r1)
	std	%r29,184(%r1)
	std	%r30,192(%r1)
	std	%r31,200(%r1)

	/* Record the old MSR */
	mfmsr	%r6

	/* read client interface handler */
	lis	%r4,openfirmware_entry@ha
	ld	%r4,openfirmware_entry@l(%r4)

	/*
	 * Set the MSR to the OF value. This has the side effect of disabling
	 * exceptions, which is important for the next few steps.
	 */

	lis	%r5,ofmsr@ha
	ld	%r5,ofmsr@l(%r5)
	mtmsrd	%r5
	isync

	/*
	 * Set up OF stack. This needs to be accessible in real mode and
	 * use the 32-bit ABI stack frame format. The pointer to the current
	 * kernel stack is placed at the very top of the stack along with
	 * the old MSR so we can get them back later.
	 */
	mr	%r5,%r1
	lis	%r1,(ofwstk+OFWSTKSZ-32)@ha
	addi	%r1,%r1,(ofwstk+OFWSTKSZ-32)@l
	std	%r5,8(%r1)	/* Save real stack pointer */
	std	%r2,16(%r1)	/* Save old TOC */
	std	%r6,24(%r1)	/* Save old MSR */
	li	%r5,0
	stw	%r5,4(%r1)
	stw	%r5,0(%r1)

	/* Finally, branch to OF */
	mtctr	%r4
	bctrl

	/* Reload stack pointer and MSR from the OFW stack */
	ld	%r6,24(%r1)
	ld	%r2,16(%r1)
	ld	%r1,8(%r1)

	/* Now set the real MSR */
	mtmsrd	%r6
	isync

	/* Sign-extend the return value from OF */
	extsw	%r3,%r3

	/* Restore all the non-volatile registers */
	ld	%r5,48(%r1)
	mtcr	%r5
	ld	%r13,56(%r1)
	ld	%r14,64(%r1)
	ld	%r15,72(%r1)
	ld	%r16,80(%r1)
	ld	%r17,88(%r1)
	ld	%r18,96(%r1)
	ld	%r19,104(%r1)
	ld	%r20,112(%r1)
	ld	%r21,120(%r1)
	ld	%r22,128(%r1)
	ld	%r23,136(%r1)
	ld	%r24,144(%r1)
	ld	%r25,152(%r1)
	ld	%r26,160(%r1)
	ld	%r27,168(%r1)
	ld	%r28,176(%r1)
	ld	%r29,184(%r1)
	ld	%r30,192(%r1)
	ld	%r31,200(%r1)

	/* Restore the stack and link register */
	ld	%r1,0(%r1)
	ld	%r0,16(%r1)
	mtlr 	%r0
	blr

/*
 * int setfault()
 *
 * Similar to setjmp to setup for handling faults on accesses to user memory.
 * Any routine using this may only call bcopy, either the form below,
 * or the (currently used) C code optimized, so it doesn't use any non-volatile
 * registers.
 */
ASENTRY(setfault)
	mflr	0
	mfcr	12
	mfsprg	4,0
	ld	4,PC_CURTHREAD(4)
	ld	4,TD_PCB(4)
	std	3,PCB_ONFAULT(4)
	std	0,0(3)
	std	1,8(3)
	std	2,16(3)

	std	%r12,24(%r3)	/* Save the non-volatile GP regs. */
	std	%r13,24+1*8(%r3)	
	std	%r14,24+2*8(%r3)	
	std	%r15,24+3*8(%r3)	
	std	%r16,24+4*8(%r3)	
	std	%r17,24+5*8(%r3)	
	std	%r18,24+6*8(%r3)	
	std	%r19,24+7*8(%r3)	
	std	%r20,24+8*8(%r3)	
	std	%r21,24+9*8(%r3)	
	std	%r22,24+10*8(%r3)	
	std	%r23,24+11*8(%r3)	
	std	%r24,24+12*8(%r3)	
	std	%r25,24+13*8(%r3)	
	std	%r26,24+14*8(%r3)	
	std	%r27,24+15*8(%r3)	
	std	%r28,24+16*8(%r3)
	std	%r29,24+17*8(%r3)	
	std	%r30,24+18*8(%r3)	
	std	%r31,24+19*8(%r3)	

	xor	3,3,3
	blr

#include <powerpc/aim/trap_subr64.S>
