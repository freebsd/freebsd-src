/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2012 SRI International
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#ifndef _LIBNETBSD_SYS_CDEFS_H_
#define _LIBNETBSD_SYS_CDEFS_H_

#include_next <sys/cdefs.h>

#ifndef __dead
#ifdef __dead2
#define __dead __dead2
#else
#define __dead
#endif
#endif /* !__dead */

/*
 * The __CONCAT macro is used to concatenate parts of symbol names, e.g.
 * with "#define OLD(foo) __CONCAT(old,foo)", OLD(foo) produces oldfoo.
 * The __CONCAT macro is a bit tricky -- make sure you don't put spaces
 * in between its arguments.  __CONCAT can also concatenate double-quoted
 * strings produced by the __STRING macro, but this only works with ANSI C.
 */

#define	___STRING(x)	__STRING(x)
#define	___CONCAT(x,y)	__CONCAT(x,y)

/*
 * Compile Time Assertion.
 */
#ifdef __COUNTER__
#define	__CTASSERT(x)		__CTASSERT0(x, __ctassert, __COUNTER__)
#else
#define	__CTASSERT(x)		__CTASSERT99(x, __INCLUDE_LEVEL__, __LINE__)
#define	__CTASSERT99(x, a, b)	__CTASSERT0(x, __CONCAT(__ctassert,a), \
					       __CONCAT(_,b))
#endif
#define	__CTASSERT0(x, y, z)	__CTASSERT1(x, y, z)
#define	__CTASSERT1(x, y, z)	\
	struct y ## z ## _struct { \
		unsigned int y ## z : /*CONSTCOND*/(x) ? 1 : -1; \
	}

/*
 * The following macro is used to remove const cast-away warnings
 * from gcc -Wcast-qual; it should be used with caution because it
 * can hide valid errors; in particular most valid uses are in
 * situations where the API requires it, not to cast away string
 * constants. We don't use *intptr_t on purpose here and we are
 * explicit about unsigned long so that we don't have additional
 * dependencies.
 */
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))

/*
 * Return the number of elements in a statically-allocated array,
 * __x.
 */
#define	__arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))

/* __BIT(n): nth bit, where __BIT(0) == 0x1. */
#define	__BIT(__n)	\
    (((uintmax_t)(__n) >= NBBY * sizeof(uintmax_t)) ? 0 : \
    ((uintmax_t)1 << (uintmax_t)((__n) & (NBBY * sizeof(uintmax_t) - 1))))

/* __BITS(m, n): bits m through n, m < n. */
#define	__BITS(__m, __n)	\
	((__BIT(MAX((__m), (__n)) + 1) - 1) ^ (__BIT(MIN((__m), (__n))) - 1))

/*
 * To be used when an empty body is required like:
 *
 * #ifdef DEBUG
 * # define dprintf(a) printf(a)
 * #else
 * # define dprintf(a) __nothing
 * #endif
 *
 * We use ((void)0) instead of do {} while (0) so that it
 * works on , expressions.
 */
#define __nothing	(/*LINTED*/(void)0)

#define __negative_p(x) (!((x) > 0) && ((x) != 0))

#define __type_min_s(t) ((t)((1ULL << (sizeof(t) * __CHAR_BIT__ - 1))))
#define __type_max_s(t) ((t)~((1ULL << (sizeof(t) * __CHAR_BIT__ - 1))))
#define __type_min_u(t) ((t)0ULL)
#define __type_max_u(t) ((t)~0ULL)
#define __type_is_signed(t) (/*LINTED*/__type_min_s(t) + (t)1 < (t)1)
#define __type_min(t) (__type_is_signed(t) ? __type_min_s(t) : __type_min_u(t))
#define __type_max(t) (__type_is_signed(t) ? __type_max_s(t) : __type_max_u(t))


#define __type_fit_u(t, a)						      \
	(/*LINTED*/!__negative_p(a) &&					      \
	    ((__UINTMAX_TYPE__)((a) + __zeroull()) <=			      \
		(__UINTMAX_TYPE__)__type_max_u(t)))

#define __type_fit_s(t, a)						      \
	(/*LINTED*/__negative_p(a)					      \
	    ? ((__INTMAX_TYPE__)((a) + __zeroll()) >=			      \
		(__INTMAX_TYPE__)__type_min_s(t))			      \
	    : ((__INTMAX_TYPE__)((a) + __zeroll()) >= (__INTMAX_TYPE__)0 &&   \
		((__INTMAX_TYPE__)((a) + __zeroll()) <=			      \
		    (__INTMAX_TYPE__)__type_max_s(t))))

/*
 * return true if value 'a' fits in type 't'
 */
#define __type_fit(t, a) (__type_is_signed(t) ? \
    __type_fit_s(t, a) : __type_fit_u(t, a))

#endif /* _LIBNETBSD_SYS_CDEFS_H_ */
