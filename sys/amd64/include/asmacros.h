/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 *	$Id: asmacros.h,v 1.4 1994/08/19 11:20:11 jkh Exp $
 */

#ifndef _MACHINE_ASMACROS_H_
#define _MACHINE_ASMACROS_H_

#ifdef KERNEL

#define ALIGN_DATA	.align	2	/* 4 byte alignment, zero filled */
#define ALIGN_TEXT	.align	2,0x90	/* 4-byte alignment, nop filled */
#define SUPERALIGN_TEXT	.align	4,0x90	/* 16-byte alignment (better for 486), nop filled */

#define GEN_ENTRY(name)		ALIGN_TEXT;	.globl name; name:
#define NON_GPROF_ENTRY(name)	GEN_ENTRY(_/**/name)

/* These three are place holders for future changes to the profiling code */
#define MCOUNT_LABEL(name)
#define MEXITCOUNT
#define FAKE_MCOUNT(caller)

#ifdef GPROF
/*
 * ALTENTRY() must be before a corresponding ENTRY() so that it can jump
 * over the mcounting.
 */
#define ALTENTRY(name)	GEN_ENTRY(_/**/name); MCOUNT; jmp 2f
#define ENTRY(name)	GEN_ENTRY(_/**/name); MCOUNT; 2:
/*
 * The call to mcount supports the usual (bad) conventions.  We allocate
 * some data and pass a pointer to it although the FreeBSD doesn't use
 * the data.  We set up a frame before calling mcount because that is
 * the standard convention although it makes work for both mcount and
 * callers.
 */
#define MCOUNT		.data; ALIGN_DATA; 1:; .long 0; .text; \
			pushl %ebp; movl %esp,%ebp; \
			movl $1b,%eax; call mcount; popl %ebp
#else
/*
 * ALTENTRY() has to align because it is before a corresponding ENTRY().
 * ENTRY() has to align to because there may be no ALTENTRY() before it.
 * If there is a previous ALTENTRY() then the alignment code is empty.
 */
#define ALTENTRY(name)	GEN_ENTRY(_/**/name)
#define ENTRY(name)	GEN_ENTRY(_/**/name)
#define MCOUNT

#endif

#ifdef DUMMY_NOPS			/* this will break some older machines */
#define FASTER_NOP
#define NOP
#else
#define FASTER_NOP	pushl %eax ; inb $0x84,%al ; popl %eax
#define NOP		pushl %eax ; inb $0x84,%al ; inb $0x84,%al ; popl %eax
#endif

#else /* !KERNEL */

#include "/usr/src/lib/libc/i386/DEFS.h"	/* XXX blech */

#ifndef RCSID
#define RCSID(a)
#endif

#endif /* KERNEL */

#endif /* !_MACHINE_ASMACROS_H_ */
