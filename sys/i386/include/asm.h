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
 *	$Id: asm.h,v 1.2 1997/03/09 13:57:32 bde Exp $
 */

#include <sys/cdefs.h>

#ifdef PIC
#define	PIC_PROLOGUE	\
	pushl	%ebx;	\
	call	1f;	\
1:			\
	popl	%ebx;	\
	addl	$_GLOBAL_OFFSET_TABLE_+[.-1b],%ebx
#define	PIC_EPILOGUE	\
	popl	%ebx
#define	PIC_PLT(x)	x@PLT
#define	PIC_GOT(x)	x@GOT(%ebx)
#define	PIC_GOTOFF(x)	x@GOTOFF(%ebx)
#else
#define	PIC_PROLOGUE
#define	PIC_EPILOGUE
#define	PIC_PLT(x)	x
#define	PIC_GOT(x)	x
#define	PIC_GOTOFF(x)	x
#endif

/*
 * CNAME and HIDENAME manage the relationship between symbol names in C
 * and the equivalent assembly language names.  CNAME is given a name as
 * it would be used in a C program.  It expands to the equivalent assembly
 * language name.  HIDENAME is given an assembly-language name, and expands
 * to a possibly-modified form that will be invisible to C programs.
 */
#if defined(__ELF__) /* { */
#define CNAME(csym)		csym
#define HIDENAME(asmsym)	__CONCAT(.,asmsym)
#else /* } { */
#define CNAME(csym)		__CONCAT(_,csym)
#define HIDENAME(asmsym)	asmsym
#endif /* } */


/* XXX should use align 4,0x90 for -m486. */
#define _START_ENTRY	.text; .align 2,0x90;
#if 0
/* Data is not used, except perhaps by non-g prof, which we don't support. */
#define _MID_ENTRY	.data; .align 2; 8:; .long 0;		\
			.text; lea 8b,%eax;
#else
#define _MID_ENTRY
#endif

#ifdef PROF

#define ALTENTRY(x)	_START_ENTRY	\
			.globl CNAME(x); .type CNAME(x),@function; CNAME(x):; \
			_MID_ENTRY	\
			call HIDENAME(mcount); jmp 9f

#define ENTRY(x)	_START_ENTRY	\
			.globl CNAME(x); .type CNAME(x),@function; CNAME(x):; \
			_MID_ENTRY	\
			call HIDENAME(mcount); 9:


#define	ALTASENTRY(x)	_START_ENTRY	\
			.globl x; .type x,@function; x:;	\
			_MID_ENTRY	\
			call HIDENAME(mcount); jmp 9f

#define	ASENTRY(x)	_START_ENTRY	\
			.globl x; .type x,@function; x:;	\
			_MID_ENTRY	\
			call HIDENAME(mcount); 9:

#else	/* !PROF */

#define	ENTRY(x)	_START_ENTRY .globl CNAME(x); .type CNAME(x),@function; \
			CNAME(x):
#define	ALTENTRY(x)	ENTRY(x)

#define	ASENTRY(x)	_START_ENTRY .globl x; .type x,@function; x:
#define	ALTASENTRY(x)	ASENTRY(x)

#endif

#ifdef _ARCH_INDIRECT
/*
 * Generate code to select between the generic functions and _ARCH_INDIRECT
 * specific ones.
 * XXX nested __CONCATs don't work with non-ANSI cpp's.
 */
#undef ENTRY
#define	ANAME(x)	CNAME(__CONCAT(__CONCAT(__,_ARCH_INDIRECT),x))
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

#endif /* _ARCH_INDIRECT */

#ifndef RCSID
#define RCSID(a)
#endif
