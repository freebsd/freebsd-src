/*-
 * Copyright (c) 2002 David E. O'Brien.  All rights reserved.
 * Copyright (c) 2000 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 *	$NetBSD: varargs.h,v 1.5 2000/02/27 17:50:22 tsubai Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_VARARGS_H_
#define	_MACHINE_VARARGS_H_

#if defined(__GNUC__) && (__GNUC__ == 2 && __GNUC_MINOR__ > 95 || __GNUC__ >= 3)

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

#else	/* ! __GNUC__ post GCC 2.95 */

#include <machine/stdarg.h>

#define	va_alist	__builtin_va_alist
#define	va_dcl		int __builtin_va_alist; ...

#undef va_start

#ifdef __lint__
#define	va_start(ap)	((ap) = *(va_list *)0)
#else
#define	va_start(ap)							\
	((ap).__stack = __va_stack_args,				\
	 (ap).__base = __va_reg_args,					\
	 (ap).__gpr = __va_first_gpr,					\
	 (ap).__fpr = __va_first_fpr)
#endif

#endif /* __GNUC__ post GCC 2.95 */

#endif /* _MACHINE_VARARGS_H_ */
