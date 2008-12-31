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
 *
 *	$NetBSD: stdarg.h,v 1.5 2000/02/27 17:50:21 tsubai Exp $
 * $FreeBSD: src/sys/powerpc/include/stdarg.h,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _MACHINE_STDARG_H_
#define	_MACHINE_STDARG_H_

#include <sys/cdefs.h>
#include <sys/_types.h>

#ifndef _VA_LIST_DECLARED
#define	_VA_LIST_DECLARED
typedef	__va_list	va_list;
#endif

#if defined(__GNUCLIKE_BUILTIN_STDARG)

#define	va_start(ap, last) \
	__builtin_va_start((ap), (last))

#define	va_arg(ap, type) \
	__builtin_va_arg((ap), type)

#if __ISO_C_VISIBLE >= 1999
#define	va_copy(dest, src) \
	__builtin_va_copy((dest), (src))

#define	__va_copy(dest, src) \
	va_copy((dest), (src))
#endif

#define	va_end(ap) \
	__builtin_va_end(ap)

#else	/* !__GNUCLIKE_BUILTIN_STDARG */

#ifdef __lint__

#define	va_start(ap, last)	((ap) = *(va_list *)0)
#define	va_arg(ap, type)	(*(type *)(void *)&(ap))

#else

#if defined(__GNUC_VA_LIST_COMPATIBILITY)
#define	va_start(ap, last)						\
	(__builtin_next_arg(last),					\
	 __builtin_memcpy((void *)&(ap), __builtin_saveregs (),		\
	 sizeof(__gnuc_va_list)))
#else
#define	va_start(ap, last)						\
	(__builtin_next_arg(last),					\
	 (ap).__stack = __va_stack_args,				\
	 (ap).__base = __va_reg_args,					\
	 (ap).__gpr = __va_first_gpr,					\
	 (ap).__fpr = __va_first_fpr)
#endif

#define	__va_first_gpr	(__builtin_args_info(0))
#define	__va_first_fpr	(__builtin_args_info(1) - 32 - 1)
#define	__va_stack_args							\
	((char *)__builtin_saveregs() +					\
	 (__va_first_gpr >= 8 ? __va_first_gpr - 8 : 0) * sizeof(int))
#define	__va_reg_args							\
	((char *)__builtin_frame_address(0) + __builtin_args_info(4))

#define	__INTEGER_TYPE_CLASS	1
#define	__REAL_TYPE_CLASS	8
#define	__RECORD_TYPE_CLASS	12

#define	__va_longlong(type)						\
	(__builtin_classify_type(*(type *)0) == __INTEGER_TYPE_CLASS &&	\
	 sizeof(type) == 8)

#define	__va_double(type)						\
	(__builtin_classify_type(*(type *)0) == __REAL_TYPE_CLASS)

#define	__va_struct(type)						\
	(__builtin_classify_type(*(type *)0) >= __RECORD_TYPE_CLASS)

#define	__va_size(type)							\
	((sizeof(type) + sizeof(int) - 1) / sizeof(int) * sizeof(int))

#define	__va_savedgpr(ap, type)						\
	((ap).__base + (ap).__gpr * sizeof(int) - sizeof(type))

#define	__va_savedfpr(ap, type)						\
	((ap).__base + 8 * sizeof(int) + (ap).__fpr * sizeof(double) -	\
	 sizeof(type))

#define	__va_stack(ap, type)						\
	((ap).__stack += __va_size(type) +				\
			(__va_longlong(type) ? (int)(ap).__stack & 4 : 0), \
	 (ap).__stack - sizeof(type))

#define	__va_gpr(ap, type)						\
	((ap).__gpr += __va_size(type) / sizeof(int) +			\
		      (__va_longlong(type) ? (ap).__gpr & 1 : 0),	\
	 (ap).__gpr <= 8 ? __va_savedgpr(ap, type) : __va_stack(ap, type))

#define	__va_fpr(ap, type)						\
	((ap).__fpr++,							\
	 (ap).__fpr <= 8 ? __va_savedfpr(ap, type) : __va_stack(ap, type))

#define	va_arg(ap, type)						\
	(*(type *)(__va_struct(type) ? (*(void **)__va_gpr(ap, void *)) : \
		   __va_double(type) ? __va_fpr(ap, type) :		\
		   __va_gpr(ap, type)))

#endif /* __lint__ */

#define	va_end(ap)	

#if __ISO_C_VISIBLE >= 1999
#if !defined(_ANSI_SOURCE) &&						\
    (!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) ||		\
     defined(_ISOC99_SOURCE) || (__STDC_VERSION__ - 0) >= 199901L)
#define	va_copy(dest, src)						\
	((dest) = (src))
#endif
#endif

#endif /* __GNUCLIKE_BUILTIN_STDARG */

#endif /* _MACHINE_STDARG_H_ */
