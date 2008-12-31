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
 *
 *	$NetBSD: asm.h,v 1.6.18.1 2000/07/25 08:37:14 kleink Exp $
 * $FreeBSD: src/sys/powerpc/include/asm.h,v 1.10.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_ASM_H_
#define	_MACHINE_ASM_H_

#include <sys/cdefs.h>

#ifdef PIC
#define	PIC_PROLOGUE	XXX
#define	PIC_EPILOGUE	XXX
#define	PIC_PLT(x)	x@plt
#ifdef	__STDC__
#define	PIC_GOT(x)	XXX
#else	/* not __STDC__ */
#define	PIC_GOT(x)	XXX
#endif	/* __STDC__ */
#else
#define	PIC_PROLOGUE
#define	PIC_EPILOGUE
#define	PIC_PLT(x)	x
#define PIC_GOT(x)	x
#endif

#define	CNAME(csym)		csym
#define	ASMNAME(asmsym)		asmsym
#define	HIDENAME(asmsym)	__CONCAT(.,asmsym)

#define	_GLOBAL(x) \
	.data; .align 2; .globl x; x:

#define	_ENTRY(x) \
	.text; .align 2; .globl x; .type x,@function; x:

#ifdef GPROF
# define	_PROF_PROLOGUE	mflr 0; stw 0,4(1); bl _mcount
#else
# define	_PROF_PROLOGUE
#endif

#define	ENTRY(y)	_ENTRY(CNAME(y)); _PROF_PROLOGUE
#define	ASENTRY(y)	_ENTRY(ASMNAME(y)); _PROF_PROLOGUE
#define	GLOBAL(y)	_GLOBAL(CNAME(y))

#define	ASMSTR		.asciz

#define	RCSID(x)	.text; .asciz x

#undef __FBSDID
#if !defined(lint) && !defined(STRIP_FBSDID)
#define __FBSDID(s)	.ident s
#else
#define __FBSDID(s)	/* nothing */
#endif /* not lint and not STRIP_FBSDID */

#define	WEAK_ALIAS(alias,sym)					\
	.weak alias;						\
	alias = sym

#ifdef __STDC__
#define	WARN_REFERENCES(_sym,_msg)				\
	.section .gnu.warning. ## _sym ; .ascii _msg ; .text
#else
#define	WARN_REFERENCES(_sym,_msg)				\
	.section .gnu.warning./**/_sym ; .ascii _msg ; .text
#endif /* __STDC__ */

#endif /* !_MACHINE_ASM_H_ */
