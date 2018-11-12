/* $FreeBSD$ */

/*-
 * Copyright (C) 2010-2016 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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
 * $FreeBSD$
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
 * Main kernel entry point.
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

	/* Unreachable */
	b	.

#include <powerpc/aim/trap_subr64.S>
