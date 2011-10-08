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

#define	TMPSTKSZ	16384		/* 16K temporary stack */

/*
 * Globals
 */
	.data
	.align	4
GLOBAL(tmpstk)
	.space	TMPSTKSZ
GLOBAL(esym)
	.llong	0			/* end of symbol table */

#define	INTRCNT_COUNT	256		/* max(HROWPIC_IRQMAX,OPENPIC_IRQMAX) */
GLOBAL(intrnames)
	.space	INTRCNT_COUNT * (MAXCOMLEN + 1) * 2
GLOBAL(sintrnames)
	.quad	INTRCNT_COUNT * (MAXCOMLEN + 1) * 2

	.align 4
GLOBAL(intrcnt)
	.space	INTRCNT_COUNT * 4 * 2
GLOBAL(sintrcnt)
	.quad	INTRCNT_COUNT * 4 * 2

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
	
	bl	OF_initial_setup
	nop

	lis	4,end@ha
	addi	4,4,end@l
	mr	5,4

	lis	3,kernbase@ha
	addi	3,3,kernbase@l

	/* Restore the argument pointer and length */
	mr	6,20
	mr	7,21

	bl	powerpc_init
	nop
	mr	%r1, %r3
	li	%r3, 0
	std	%r3, 0(%r1)
	bl	mi_startup
	nop
	b	OF_exit
	nop

/*
 * PPC64 ABI TOC base
 */

        .align  3
	.globl	tocbase
tocbase:
        .llong  .TOC.@tocbase

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
	ld	4,TD_PCB(13)	/* curthread = r13 */
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
