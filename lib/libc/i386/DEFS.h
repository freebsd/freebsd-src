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
 *
 *	$Id: DEFS.h,v 1.2 1994/08/05 01:17:56 wollman Exp $
 */

#include <sys/cdefs.h>

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
#define _START_ENTRY	.align 2,0x90;
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
