/*	$NetBSD: cdefs.h,v 1.18 1997/06/18 19:09:50 christos Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Berkeley Software Design, Inc.
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
 *	@(#)cdefs.h	8.7 (Berkeley) 1/21/94
 */

#ifndef	_SYS_CDEFS_H_

#if defined(NEED_HOST_CDEFS_H)
/*
 * make sure we don't come past here again.
 */
#undef NEED_HOST_CDEFS_H
/*
 * Some systems - notably linux, have sys/cdefs.h
 * which is not really compatible with our's.
 */
#ifdef __GNUC__
# include_next <sys/cdefs.h>
#else
/*
 * It sucks that we have to hard code a path like this.
 * But systems that have a sys/cdefs.h that don't use gcc
 * should be few.
 */
# include "/usr/include/sys/cdefs.h"
#endif
/*
 * We are about to [re]define these
 */
#undef __P
#undef _SYS_CDEFS_H_
#endif

#define	_SYS_CDEFS_H_

#ifdef NetBSD
#include <machine/cdefs.h>
#endif

#if defined(__cplusplus)
#  ifndef __BEGIN_DECLS
#  define __BEGIN_DECLS	extern "C" {
#  endif
#  ifndef __END_DECLS
#  define __END_DECLS	};
#  endif
#else
#  ifndef __BEGIN_DECLS
#  define __BEGIN_DECLS
#  endif
#  ifndef __END_DECLS
#  define __END_DECLS
#  endif
#endif

/*
 * The __CONCAT macro is used to concatenate parts of symbol names, e.g.
 * with "#define OLD(foo) __CONCAT(old,foo)", OLD(foo) produces oldfoo.
 * The __CONCAT macro is a bit tricky -- make sure you don't put spaces
 * in between its arguments.  __CONCAT can also concatenate double-quoted
 * strings produced by the __STRING macro, but this only works with ANSI C.
 */
#if defined(__STDC__) || defined(__cplusplus)
#define	__P(protos)	protos		/* full-blown ANSI C */
#ifndef __CONCAT
#define	__CONCAT(x,y)	x ## y
#endif
#define	__STRING(x)	#x

#define	__const		const		/* define reserved names to standard */
#define	__signed	signed
#define	__volatile	volatile
#if defined(__cplusplus)
#define	__inline	inline		/* convert to C++ keyword */
#else
#ifndef __GNUC__
#define	__inline			/* delete GCC keyword */
#endif /* !__GNUC__ */
#endif /* !__cplusplus */

#else	/* !(__STDC__ || __cplusplus) */
#define	__P(protos)	()		/* traditional C preprocessor */
#define	__CONCAT(x,y)	x/**/y
#define	__STRING(x)	"x"

#ifndef __GNUC__
#define	__const				/* delete pseudo-ANSI C keywords */
#define	__inline
#define	__signed
#define	__volatile
#endif	/* !__GNUC__ */

/*
 * In non-ANSI C environments, new programs will want ANSI-only C keywords
 * deleted from the program and old programs will want them left alone.
 * Programs using the ANSI C keywords const, inline etc. as normal
 * identifiers should define -DNO_ANSI_KEYWORDS.
 */
#ifndef	NO_ANSI_KEYWORDS
#define	const		__const		/* convert ANSI C keywords */
#define	inline		__inline
#define	signed		__signed
#define	volatile	__volatile
#endif /* !NO_ANSI_KEYWORDS */
#endif	/* !(__STDC__ || __cplusplus) */

/*
 * GCC1 and some versions of GCC2 declare dead (non-returning) and
 * pure (no side effects) functions using "volatile" and "const";
 * unfortunately, these then cause warnings under "-ansi -pedantic".
 * GCC2 uses a new, peculiar __attribute__((attrs)) style.  All of
 * these work for GNU C++ (modulo a slight glitch in the C++ grammar
 * in the distribution version of 2.5.5).
 */
#if !defined(__GNUC__) || __GNUC__ < 2 || \
	(__GNUC__ == 2 && __GNUC_MINOR__ < 5)
#define	__attribute__(x)	/* delete __attribute__ if non-gcc or gcc1 */
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define	__dead		__volatile
#define	__pure		__const
#endif
#endif

#ifdef sun386
# define __attribute__(x)
#endif

#ifdef __KPRINTF_ATTRIBUTE__
#define __kprintf_attribute__(a) __attribute__(a)
#else
#define __kprintf_attribute__(a)
#endif

/* Delete pseudo-keywords wherever they are not available or needed. */
#ifndef __dead
#define	__dead
#define	__pure
#endif

#define __IDSTRING(name,string) \
	static const char name[] __attribute__((__unused__)) = string

#ifndef __RCSID
#define __RCSID(s) __IDSTRING(rcsid,s)
#endif

#ifndef __COPYRIGHT
#define __COPYRIGHT(s) __IDSTRING(copyright,s)
#endif

#endif /* !_SYS_CDEFS_H_ */
