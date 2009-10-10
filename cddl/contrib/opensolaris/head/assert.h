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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_ASSERT_H
#define	_ASSERT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"	/* SVr4.0 1.6.1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__STDC__)
#if __STDC_VERSION__ - 0 >= 199901L
extern void __assert(const char *, const char *, int);
#else
extern void __assert(const char *, const char *, int);
#endif /* __STDC_VERSION__ - 0 >= 199901L */
#else
extern void _assert();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _ASSERT_H */

/*
 * Note that the ANSI C Standard requires all headers to be idempotent except
 * <assert.h> which is explicitly required not to be idempotent (section 4.1.2).
 * Therefore, it is by intent that the header guards (#ifndef _ASSERT_H) do
 * not span this entire file.
 */

#undef	assert

#ifdef	NDEBUG

#define	assert(EX) ((void)0)

#else

#if defined(__STDC__)
#if __STDC_VERSION__ - 0 >= 199901L
#define	assert(EX) (void)((EX) || (__assert(#EX, __FILE__, __LINE__), 0))
#else
#define	assert(EX) (void)((EX) || (__assert(#EX, __FILE__, __LINE__), 0))
#endif /* __STDC_VERSION__ - 0 >= 199901L */
#else
#define	assert(EX) (void)((EX) || (_assert("EX", __FILE__, __LINE__), 0))
#endif	/* __STDC__ */

#endif	/* NDEBUG */
