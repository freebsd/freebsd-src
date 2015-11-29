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

#ifdef _CALL_ELF
.abiversion _CALL_ELF
#endif

/* Locate the per-CPU data structure */
#define GET_CPUINFO(r)  \
        mfsprg0  r
#define GET_TOCBASE(r)  \
	li	r,TRAP_TOCBASE;	/* Magic address for TOC */ \
	ld	r,0(r)

/* Glue for linker script */
.globl  kernbase
.set    kernbase, KERNBASE

/*
 * Globals
 */
	.data
	.align 3
GLOBAL(__startkernel)
	.llong	begin
GLOBAL(__endkernel)
	.llong	end

	.align	4
#define	TMPSTKSZ	16384		/* 16K temporary stack */
GLOBAL(tmpstk)
	.space	TMPSTKSZ

TOC_ENTRY(tmpstk)

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
 *
 * Calling convention:
 * r3: Flattened Device Tree pointer (or zero)
 * r4: ignored
 * r5: OF client interface pointer (or zero)
 * r6: Loader metadata pointer (or zero)
 */
	.text
ASENTRY_NOPROF(__start)
	/* Set up the TOC pointer */
	b	0f
	.align 3
0:	nop
	bl	1f
	.llong	__tocbase + 0x8000 - .
1:	mflr	%r2
	ld	%r1,0(%r2)
	add	%r2,%r1,%r2

	/* Get load offset */
	ld	%r31,-0x8000(%r2) /* First TOC entry is TOC base */
	subf    %r31,%r31,%r2	/* Subtract from real TOC base to get base */

	/* Set up the stack pointer */
	ld	%r1,TOC_REF(tmpstk)(%r2)
	addi	%r1,%r1,TMPSTKSZ-96
	add	%r1,%r1,%r31

	/* Relocate kernel */
	std	%r3,48(%r1)
	std	%r4,56(%r1)
	std	%r5,64(%r1)
	std	%r6,72(%r1)
	bl	1f
	.llong _DYNAMIC-.
1:	mflr	%r3
	ld	%r4,0(%r3)
	add	%r3,%r4,%r3
	mr	%r4,%r31
	bl	elf_reloc_self
	nop
	ld	%r3,48(%r1)
	ld	%r4,56(%r1)
	ld	%r5,64(%r1)
	ld	%r6,72(%r1)

	/* Switch to 64-bit mode */
	mfmsr	%r9
	li	%r8,1
	insrdi	%r9,%r8,1,0
	mtmsrd	%r9
	isync
	
	/* Begin CPU init */
	mr	%r4,%r2 /* Replace ignored r4 with tocbase for trap handlers */
	bl	powerpc_init
	nop

	/* Set stack pointer to new value and branch to mi_startup */
	mr	%r1, %r3
	li	%r3, 0
	std	%r3, 0(%r1)
	bl	mi_startup
	nop

	/* If this returns (it won't), go back to firmware */
	b	OF_exit
	nop

/*
 * int setfault()
 *
 * Similar to setjmp to setup for handling faults on accesses to user memory.
 * Any routine using this may only call bcopy, either the form below,
 * or the (currently used) C code optimized, so it doesn't use any non-volatile
 * registers.
 */
ASENTRY_NOPROF(setfault)
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
