/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)DEFS.h	5.1 (Berkeley) 4/23/90
 *	from: FreeBSD: src/sys/i386/include/asm.h,v 1.7 2000/01/25
 * $FreeBSD$
 */

#ifndef _MACHINE_ASM_H_
#define	_MACHINE_ASM_H_

#include <sys/cdefs.h>

#ifdef PIC
#define	PIC_PROLOGUE(r1, r2) \
	sethi	%hi(_GLOBAL_OFFSET_TABLE_-4), r1 ; \
	rd	%pc, r2 ; \
	or	r1, %lo(_GLOBAL_OFFSET_TABLE_+4), r1 ; \
	add	r2, r1, r2
#define	SET(name, r1, r2) \
	set	name, r2 ; \
	ldx	[r1 + r2], r2
#else
#define	PIC_PROLOGUE(r1, r2)
#define	SET(name, r1, r2) \
	set	name, r2
#endif

/*
 * CNAME and HIDENAME manage the relationship between symbol names in C
 * and the equivalent assembly language names.  CNAME is given a name as
 * it would be used in a C program.  It expands to the equivalent assembly
 * language name.  HIDENAME is given an assembly-language name, and expands
 * to a possibly-modified form that will be invisible to C programs.
 */
#define CNAME(csym)		csym
#define HIDENAME(asmsym)	__CONCAT(.,asmsym)

#define	CCFSZ	192
#define	SPOFF	2047

#define _START_ENTRY \
	.text ; \
	.p2align 4

/*
 * Define a function entry point.
 *
 * The compiler produces #function for the .type pseudo-op, but the '#'
 * character has special meaning in cpp macros, so we use @function like
 * other architectures.  The assembler seems to accept both.
 * The assembler also accepts a .proc pseudo-op, which is used by the
 * peep hole optimizer, whose argument is the type code of the return
 * value.  Since this is difficult to predict and its expected that
 * assembler code is already optimized, we leave it out.
 */
#define	_ENTRY(x) \
	_START_ENTRY ; \
	.globl	CNAME(x) ; \
	.type	CNAME(x),@function ; \
CNAME(x):

#define	ENTRY(x)	_ENTRY(x)
#define	END(x)		.size x, . - x

#endif /* !_MACHINE_ASM_H_ */
