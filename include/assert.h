/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
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

#include <sys/cdefs.h>

/*
 * Unlike other ANSI header files, <assert.h> may usefully be included
 * multiple times, with and without NDEBUG defined.
 */

#undef assert
#undef _assert
#undef __assert_unreachable

#ifdef NDEBUG
#define	assert(e)	((void)0)
#define	_assert(e)	((void)0)
#if __BSD_VISIBLE
#define	__assert_unreachable()	__unreachable()
#endif	/* __BSD_VISIBLE */
#else
#ifdef __cplusplus
#if __cplusplus < 202002L
/*
 * C++ modes prior to C++20 cannot simultaneously satisfy all three
 * desirable properties of the sanitiser:
 *
 *   Approach                       No double-eval  Lambda support  Arity check
 *   -----------------------------  --------------  --------------  -----------
 *   sizeof(cast(expression))       yes             no              yes
 *   static_cast<bool>(expression)  no              yes             no
 *   (void)bool(expression)         no              yes             no
 *
 *   NOTE: C++20 introduced lambdas in unevaluated contexts; see P0315R4.
 *
 * Since no approach satisfies all three below C++20, the least harmful
 * choice is to forgo the check entirely rather than silently break one
 * of the remaining guarantees.
 *
 */
#define	__assert_sanitize(...)	((void)0)
#else
#define	__assert_sanitize(...)	(void)sizeof(((bool(*)(bool))0)(__VA_ARGS__))
#endif /* __cplusplus < 202002L */
#else
#define	__assert_sanitize(...)	(void)sizeof(((_Bool(*)(_Bool))0)(__VA_ARGS__))
#endif /* __cplusplus */
#define	assert(...)	(__assert_sanitize(__VA_ARGS__),       \
			    (__VA_ARGS__) ? (void)0 :          \
			    __assert(__func__, __FILE__,       \
			    __LINE__, #__VA_ARGS__))
#define	_assert(...)	assert(__VA_ARGS__)
#if __BSD_VISIBLE
#define	__assert_unreachable()	assert(0 && "unreachable segment reached")
#endif	/* __BSD_VISIBLE */
#endif /* NDEBUG */

#ifndef __STDC_VERSION_ASSERT_H__
#define	__STDC_VERSION_ASSERT_H__ 202311L

/*
 * Static assertions.  In principle we could define static_assert for
 * C++ older than C++11, but this breaks if _Static_assert is
 * implemented as a macro.
 *
 * C++ template parameters may contain commas, even if not enclosed in
 * parentheses, causing the _Static_assert macro to be invoked with more
 * than two parameters.
 *
 * C23 defines static_assert and its obsolescent alternative spelling,
 * _Static_assert, as keywords.
 */
#if __ISO_C_VISIBLE >= 2011 && !defined(__cplusplus) && \
    __STDC_VERSION__ < 202311L
#define	static_assert	_Static_assert
#endif

__BEGIN_DECLS
void __assert(const char *, const char *, int, const char *) __dead2;
__END_DECLS

#endif /* !__STDC_VERSION_ASSERT_H__ */
