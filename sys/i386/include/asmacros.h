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
 *	$Id: asmacros.h,v 1.9.2.4 1997/12/08 01:08:02 steve Exp $
 */

#ifndef _MACHINE_ASMACROS_H_
#define _MACHINE_ASMACROS_H_

#ifdef KERNEL
#include <sys/cdefs.h>

/* XXX too much duplication in various asm*.h's. */

#define ALIGN_DATA	.align	2	/* 4 byte alignment, zero filled */
#ifdef GPROF
#define ALIGN_TEXT	.align	4,0x90	/* 16-byte alignment, nop filled */
#else
#define ALIGN_TEXT	.align	2,0x90	/* 4-byte alignment, nop filled */
#endif
#define SUPERALIGN_TEXT	.align	4,0x90	/* 16-byte alignment, nop filled */

#define GEN_ENTRY(name)		ALIGN_TEXT; .globl __CONCAT(_,name); \
				__CONCAT(_,name):
#define NON_GPROF_ENTRY(name)	GEN_ENTRY(name)
#define NON_GPROF_RET		.byte 0xc3	/* opcode for `ret' */

#ifdef GPROF
/*
 * __mcount is like mcount except that doesn't require its caller to set
 * up a frame pointer.  It must be called before pushing anything onto the
 * stack.  gcc should eventually generate code to call __mcount in most
 * cases.  This would make -pg in combination with -fomit-frame-pointer
 * useful.  gcc has a configuration variable PROFILE_BEFORE_PROLOGUE to
 * allow profiling before setting up the frame pointer, but this is
 * inadequate for good handling of special cases, e.g., -fpic works best
 * with profiling after the prologue.
 *
 * mexitcount is a new function to support non-statistical profiling if an
 * accurate clock is available.  For C sources, calls to it are generated
 * by the FreeBSD extension `-mprofiler-epilogue' to gcc.  It is best to
 * call mexitcount at the end of a function like the MEXITCOUNT macro does,
 * but gcc currently generates calls to it at the start of the epilogue to
 * avoid problems with -fpic.
 *
 * mcount and __mexitcount may clobber the call-used registers and %ef.
 * mexitcount may clobber %ecx and %ef.
 *
 * Cross-jumping makes non-statistical profiling timing more complicated.
 * It is handled in many cases by calling mexitcount before jumping.  It is
 * handled for conditional jumps using CROSSJUMP() and CROSSJUMP_LABEL().
 * It is handled for some fault-handling jumps by not sharing the exit
 * routine.
 *
 * ALTENTRY() must be before a corresponding ENTRY() so that it can jump to
 * the main entry point.  Note that alt entries are counted twice.  They
 * have to be counted as ordinary entries for gprof to get the call times
 * right for the ordinary entries.
 *
 * High local labels are used in macros to avoid clashes with local labels
 * in functions.
 *
 * Ordinary `ret' is used instead of a macro `RET' because there are a lot
 * of `ret's.  0xc3 is the opcode for `ret' (`#define ret ... ret' can't
 * be used because this file is sometimes preprocessed in traditional mode).
 * `ret' clobbers eflags but this doesn't matter.
 */
#define ALTENTRY(name)		GEN_ENTRY(name) ; MCOUNT ; MEXITCOUNT ; jmp 9f
#define	CROSSJUMP(jtrue, label, jfalse) \
	jfalse 8f; MEXITCOUNT; jmp __CONCAT(to,label); 8:
#define CROSSJUMPTARGET(label) \
	ALIGN_TEXT; __CONCAT(to,label): ; MCOUNT; jmp label
#define ENTRY(name)		GEN_ENTRY(name) ; 9: ; MCOUNT
#define FAKE_MCOUNT(caller)	pushl caller ; call __mcount ; popl %ecx
#define MCOUNT			call __mcount
#define MCOUNT_LABEL(name)	GEN_ENTRY(name) ; nop ; ALIGN_TEXT
#define MEXITCOUNT		call mexitcount
#define ret			MEXITCOUNT ; NON_GPROF_RET

#else /* !GPROF */
/*
 * ALTENTRY() has to align because it is before a corresponding ENTRY().
 * ENTRY() has to align to because there may be no ALTENTRY() before it.
 * If there is a previous ALTENTRY() then the alignment code for ENTRY()
 * is empty.
 */
#define ALTENTRY(name)		GEN_ENTRY(name)
#define	CROSSJUMP(jtrue, label, jfalse)	jtrue label
#define	CROSSJUMPTARGET(label)
#define ENTRY(name)		GEN_ENTRY(name)
#define FAKE_MCOUNT(caller)
#define MCOUNT
#define MCOUNT_LABEL(name)
#define MEXITCOUNT
#endif /* GPROF */

#else /* !KERNEL */

#include "/usr/src/lib/libc/i386/DEFS.h"	/* XXX blech */

/*
 * In the !KERNEL case, this header is only (ab)used in lib/msun/i387.
 * Use it to generate code to select between the generic math functions 
 * and the i387 ones.
 */
#undef ENTRY
#define	ANAME(x)	CNAME(__CONCAT(__i387_,x))
#define	ASELNAME(x)	CNAME(__CONCAT(__arch_select_,x))
#define	AVECNAME(x)	CNAME(__CONCAT(__arch_,x))
#define	GNAME(x)	CNAME(__CONCAT(__generic_,x))

/* Don't bother profiling this. */
#ifdef PIC
#define	ARCH_DISPATCH(x) \
			_START_ENTRY; \
			.globl CNAME(x); .type CNAME(x),@function; CNAME(x): ; \
			PIC_PROLOGUE; \
			movl PIC_GOT(AVECNAME(x)),%eax; \
			PIC_EPILOGUE; \
			jmpl *(%eax)

#define	ARCH_SELECT(x)	_START_ENTRY; \
			.type ASELNAME(x),@function; \
			ASELNAME(x): \
			PIC_PROLOGUE; \
			call PIC_PLT(CNAME(__get_hw_float)); \
			testl %eax,%eax; \
			movl PIC_GOT(ANAME(x)),%eax; \
			jne 8f; \
			movl PIC_GOT(GNAME(x)),%eax; \
			8: \
			movl PIC_GOT(AVECNAME(x)),%edx; \
			movl %eax,(%edx); \
			PIC_EPILOGUE; \
			jmpl *%eax
#else /* !PIC */
#define	ARCH_DISPATCH(x) \
			_START_ENTRY; \
			.globl CNAME(x); .type CNAME(x),@function; CNAME(x): ; \
			jmpl *AVECNAME(x)

#define	ARCH_SELECT(x)	_START_ENTRY; \
			.type ASELNAME(x),@function; \
			ASELNAME(x): \
			call CNAME(__get_hw_float); \
			testl %eax,%eax; \
			movl $ANAME(x),%eax; \
			jne 8f; \
			movl $GNAME(x),%eax; \
			8: \
			movl %eax,AVECNAME(x); \
			jmpl *%eax
#endif /* PIC */

#define	ARCH_VECTOR(x)	.data; .align 2; \
			.globl AVECNAME(x); \
			.type AVECNAME(x),@object; \
			.size AVECNAME(x),4; \
			AVECNAME(x): .long ASELNAME(x)

#ifdef PROF

#define	ALTENTRY(x)	ENTRY(x); jmp 9f
#define	ENTRY(x)	ARCH_VECTOR(x); ARCH_SELECT(x); ARCH_DISPATCH(x); \
			_START_ENTRY; \
			.globl ANAME(x); .type ANAME(x),@function; ANAME(x):; \
			call HIDENAME(mcount); 9:

#else /* !PROF */

#define	ALTENTRY(x)	ENTRY(x)
#define	ENTRY(x)	ARCH_VECTOR(x); ARCH_SELECT(x); ARCH_DISPATCH(x); \
			_START_ENTRY; \
			.globl ANAME(x); .type ANAME(x),@function; ANAME(x):

#endif /* PROF */

#ifndef RCSID
#define RCSID(a)
#endif

#endif /* KERNEL */

#endif /* !_MACHINE_ASMACROS_H_ */
