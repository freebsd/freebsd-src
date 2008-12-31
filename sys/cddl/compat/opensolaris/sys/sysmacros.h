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
 *
 * $FreeBSD: src/sys/cddl/compat/opensolaris/sys/sysmacros.h,v 1.2.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _OPENSOLARIS_SYS_SYSMACROS_H_
#define	_OPENSOLARIS_SYS_SYSMACROS_H_

#include <sys/param.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Macro for checking power of 2 address alignment.
 */
#define	IS_P2ALIGNED(v, a) ((((uintptr_t)(v)) & ((uintptr_t)(a) - 1)) == 0)

/*
 * Macro to determine if value is a power of 2
 */
#define	ISP2(x)		(((x) & ((x) - 1)) == 0)

/*
 * Macros for various sorts of alignment and rounding when the alignment
 * is known to be a power of 2.
 */
#define	P2ALIGN(x, align)		((x) & -(align))
#define	P2PHASE(x, align)		((x) & ((align) - 1))
#define	P2NPHASE(x, align)		(-(x) & ((align) - 1))
#define	P2ROUNDUP(x, align)		(-(-(x) & -(align)))
#define	P2END(x, align)			(-(~(x) & -(align)))
#define	P2PHASEUP(x, align, phase)	((phase) - (((phase) - (x)) & -(align)))
#define	P2CROSS(x, y, align)		(((x) ^ (y)) > (align) - 1)
/*
 * Determine whether two numbers have the same high-order bit.
 */
#define	P2SAMEHIGHBIT(x, y)		(((x) ^ (y)) < ((x) & (y)))

/*
 * Typed version of the P2* macros.  These macros should be used to ensure
 * that the result is correctly calculated based on the data type of (x),
 * which is passed in as the last argument, regardless of the data
 * type of the alignment.  For example, if (x) is of type uint64_t,
 * and we want to round it up to a page boundary using "PAGESIZE" as
 * the alignment, we can do either
 *	P2ROUNDUP(x, (uint64_t)PAGESIZE)
 * or
 *	P2ROUNDUP_TYPED(x, PAGESIZE, uint64_t)
 */
#define	P2ALIGN_TYPED(x, align, type)	\
	((type)(x) & -(type)(align))
#define	P2PHASE_TYPED(x, align, type)	\
	((type)(x) & ((type)(align) - 1))
#define	P2NPHASE_TYPED(x, align, type)	\
	(-(type)(x) & ((type)(align) - 1))
#define	P2ROUNDUP_TYPED(x, align, type)	\
	(-(-(type)(x) & -(type)(align)))
#define	P2END_TYPED(x, align, type)	\
	(-(~(type)(x) & -(type)(align)))
#define	P2PHASEUP_TYPED(x, align, phase, type)	\
	((type)(phase) - (((type)(phase) - (type)(x)) & -(type)(align)))
#define	P2CROSS_TYPED(x, y, align, type)	\
	(((type)(x) ^ (type)(y)) > (type)(align) - 1)
#define	P2SAMEHIGHBIT_TYPED(x, y, type) \
	(((type)(x) ^ (type)(y)) < ((type)(x) & (type)(y)))

#ifdef _KERNEL
#define	memmove(dst, src, size)	bcopy((src), (dst), (size))
#endif

/*
 * Find highest one bit set.
 *      Returns bit number + 1 of highest bit that is set, otherwise returns 0.
 * High order bit is 31 (or 63 in _LP64 kernel).
 */
static __inline int
highbit(ulong_t i)
{
	register int h = 1;

	if (i == 0)
		return (0);
#ifdef _LP64
	if (i & 0xffffffff00000000ul) {
		h += 32; i >>= 32;
	}
#endif
	if (i & 0xffff0000) {
		h += 16; i >>= 16;
	}
	if (i & 0xff00) {
		h += 8; i >>= 8;
	}
	if (i & 0xf0) {
		h += 4; i >>= 4;
	}
	if (i & 0xc) {
		h += 2; i >>= 2;
	}
	if (i & 0x2) {
		h += 1;
	}
	return (h);
}

#ifdef	__cplusplus
}
#endif

#endif	/* _OPENSOLARIS_SYS_SYSMACROS_H_ */
