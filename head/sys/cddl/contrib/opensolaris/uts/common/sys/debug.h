/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/


#ifndef _SYS_DEBUG_H
#define	_SYS_DEBUG_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * ASSERT(ex) causes a panic or debugger entry if expression ex is not
 * true.  ASSERT() is included only for debugging, and is a no-op in
 * production kernels.  VERIFY(ex), on the other hand, behaves like
 * ASSERT and is evaluated on both debug and non-debug kernels.
 */

#if defined(__STDC__)
extern int assfail(const char *, const char *, int);
#define	VERIFY(EX) ((void)((EX) || assfail(#EX, __FILE__, __LINE__)))
#ifdef DEBUG
#define	ASSERT(EX) VERIFY(EX)
#else
#define	ASSERT(x)  ((void)0)
#endif
#else	/* defined(__STDC__) */
extern int assfail();
#define	VERIFY(EX) ((void)((EX) || assfail("EX", __FILE__, __LINE__)))
#ifdef DEBUG
#define	ASSERT(EX) VERIFY(EX)
#else
#define	ASSERT(x)  ((void)0)
#endif
#endif	/* defined(__STDC__) */

/*
 * Assertion variants sensitive to the compilation data model
 */
#if defined(_LP64)
#define	ASSERT64(x)	ASSERT(x)
#define	ASSERT32(x)
#else
#define	ASSERT64(x)
#define	ASSERT32(x)	ASSERT(x)
#endif

/*
 * ASSERT3() behaves like ASSERT() except that it is an explicit conditional,
 * and prints out the values of the left and right hand expressions as part of
 * the panic message to ease debugging.  The three variants imply the type
 * of their arguments.  ASSERT3S() is for signed data types, ASSERT3U() is
 * for unsigned, and ASSERT3P() is for pointers.  The VERIFY3*() macros
 * have the same relationship as above.
 */
extern void assfail3(const char *, uintmax_t, const char *, uintmax_t,
    const char *, int);
#define	VERIFY3_IMPL(LEFT, OP, RIGHT, TYPE) do { \
	const TYPE __left = (TYPE)(LEFT); \
	const TYPE __right = (TYPE)(RIGHT); \
	if (!(__left OP __right)) \
		assfail3(#LEFT " " #OP " " #RIGHT, \
			(uintmax_t)__left, #OP, (uintmax_t)__right, \
			__FILE__, __LINE__); \
_NOTE(CONSTCOND) } while (0)

#define	VERIFY3S(x, y, z)	VERIFY3_IMPL(x, y, z, int64_t)
#define	VERIFY3U(x, y, z)	VERIFY3_IMPL(x, y, z, uint64_t)
#define	VERIFY3P(x, y, z)	VERIFY3_IMPL(x, y, z, uintptr_t)
#ifdef DEBUG
#define	ASSERT3S(x, y, z)	VERIFY3S(x, y, z)
#define	ASSERT3U(x, y, z)	VERIFY3U(x, y, z)
#define	ASSERT3P(x, y, z)	VERIFY3P(x, y, z)
#else
#define	ASSERT3S(x, y, z)	((void)0)
#define	ASSERT3U(x, y, z)	((void)0)
#define	ASSERT3P(x, y, z)	((void)0)
#endif

#ifdef	_KERNEL

extern void abort_sequence_enter(char *);
extern void debug_enter(char *);

#endif	/* _KERNEL */

#if defined(DEBUG) && !defined(__sun)
/* CSTYLED */
#define	STATIC
#else
/* CSTYLED */
#define	STATIC static
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUG_H */
