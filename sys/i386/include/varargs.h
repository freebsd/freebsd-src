/*-
 * Copyright (c) 2002 David E. O'Brien.  All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 *	@(#)varargs.h	8.2 (Berkeley) 3/22/94
 * $FreeBSD: src/sys/i386/include/varargs.h,v 1.14.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_VARARGS_H_
#define	_MACHINE_VARARGS_H_

#ifndef _SYS_CDEFS_H_
#error this file needs sys/cdefs.h as a prerequisite
#endif

#ifdef __GNUCLIKE_BUILTIN_VARARGS

#include <sys/_types.h>

#ifndef _VA_LIST_DECLARED
#define	_VA_LIST_DECLARED
typedef	__va_list	va_list;
#endif

typedef int __builtin_va_alist_t __attribute__((__mode__(__word__)));

#define	va_alist		__builtin_va_alist
#define	va_dcl			__builtin_va_alist_t __builtin_va_alist; ...
#define	va_start(ap)		__builtin_varargs_start(ap)
#define	va_arg(ap, type)	__builtin_va_arg((ap), type)
#define	va_end(ap)		__builtin_va_end(ap)

#else	/* !__GNUCLIKE_BUILTIN_VARARGS */

typedef char *va_list;

#define	__va_size(type) \
	(((sizeof(type) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

#if defined(__GNUCLIKE_BUILTIN_VAALIST)
#define	va_alist	__builtin_va_alist
#endif
#ifdef __CC_SUPPORTS_VARADIC_XXX
#define	va_dcl	int va_alist; ...
#else
#define	va_dcl	int va_alist; 
#endif

#define	va_start(ap) \
	((ap) = (va_list)&va_alist)

#define	va_arg(ap, type) \
	(*(type *)((ap) += __va_size(type), (ap) - __va_size(type)))

#define	va_end(ap)

#endif /* __GNUCLIKE_BUILTIN_VARARGS */

#endif /* !_MACHINE_VARARGS_H_ */
