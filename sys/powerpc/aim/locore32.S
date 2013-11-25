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
#include "opt_platform.h"

/* Locate the per-CPU data structure */
#define GET_CPUINFO(r)  \
        mfsprg0  r

/*
 * Compiled KERNBASE location and the kernel load address
 */
        .globl  kernbase
        .set    kernbase, KERNBASE

#define	TMPSTKSZ	8192		/* 8K temporary stack */

/*
 * Globals
 */
	.data
	.align	4
GLOBAL(tmpstk)
	.space	TMPSTKSZ
GLOBAL(esym)
	.long	0			/* end of symbol table */

#define	INTRCNT_COUNT	256		/* max(HROWPIC_IRQMAX,OPENPIC_IRQMAX) */
GLOBAL(intrnames)
	.space	INTRCNT_COUNT * (MAXCOMLEN + 1) * 2
GLOBAL(sintrnames)
	.long	INTRCNT_COUNT * (MAXCOMLEN + 1) * 2

	.align 4
GLOBAL(intrcnt)
	.space	INTRCNT_COUNT * 4 * 2
GLOBAL(sintrcnt)
	.long	INTRCNT_COUNT * 4 * 2

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
	.globl	__start
__start:
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
#ifdef WII
#include <powerpc/wii/locore.S>
#endif
	/* Zero bss, in case we were started by something unhelpful */
	li	0,0
	lis	8,_edata@ha
	addi	8,8,_edata@l
	lis	9,_end@ha
	addi	9,9,_end@l
2:	stw     0,0(8)
	addi	8,8,4
	cmplw	8,9
	blt	2b
	
	/* Save the argument pointer and length */
	mr	20,6
	mr	21,7

	lis	8,openfirmware_entry@ha
	stw	5,openfirmware_entry@l(8) /* save client interface handler */

	lis	1,(tmpstk+TMPSTKSZ-16)@ha
	addi	1,1,(tmpstk+TMPSTKSZ-16)@l

	mfmsr	0
	lis	9,ofmsr@ha
	stwu	0,ofmsr@l(9)

	mfsprg0	0			/* save SPRG0-3 */
	stw	0,4(9)			/* ofmsr[1] = sprg0 */
	mfsprg1 0
	stw	0,8(9)			/* ofmsr[2] = sprg1 */
	mfsprg2 0
	stw	0,12(9)			/* ofmsr[3] = sprg2 */
	mfsprg3 0
	stw	0,16(9)			/* ofmsr[4] = sprg3 */
	
	bl	OF_initial_setup

	lis	3,kernel_text@ha
	addi	3,3,kernel_text@l

	lis	4,end@ha
	addi	4,4,end@l
	add	4,4,3
	mr	5,4

	/* Restore the argument pointer and length */
	mr	6,20
	mr	7,21

	bl	powerpc_init
	mr	%r1, %r3
	li	%r3, 0
	stw	%r3, 0(%r1)
	bl	mi_startup
	b	OF_exit

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
	lwz	4,TD_PCB(2)	/* curthread = r2 */
	stw	3,PCB_ONFAULT(4)
	stw	0,0(3)
	stw	1,4(3)
	stw	2,8(3)
	stmw	12,12(3)
	xor	3,3,3
	blr

#include <powerpc/aim/trap_subr32.S>
