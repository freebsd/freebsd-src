/*
 * Copyright (c) 1988, 1993
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
 * $FreeBSD$
 */

#ifndef	_SYS_LIMITS_H_
#define	_SYS_LIMITS_H_

#include <machine/_limits.h>

#define	CHAR_BIT	__CHAR_BIT	/* number of bits in a char */

/*
 * According to ANSI (section 2.2.4.2), the values below must be usable by
 * #if preprocessing directives.  Additionally, the expression must have the
 * same type as would an expression that is an object of the corresponding
 * type converted according to the integral promotions.  The subtraction for
 * INT_MIN, etc., is so the value is not unsigned; e.g., 0x80000000 is an
 * unsigned int for 32-bit two's complement ANSI compilers (section 3.1.3.2).
 * These numbers are for the default configuration of gcc.  They work for
 * some other compilers as well, but this should not be depended on.
 */
#define	SCHAR_MAX	__SCHAR_MAX	/* max value for a signed char */
#define	SCHAR_MIN	__SCHAR_MIN	/* min value for a signed char */

#define	UCHAR_MAX	__UCHAR_MAX	/* max value for an unsigned char */
#define	CHAR_MAX	__CHAR_MAX	/* max value for a char */
#define	CHAR_MIN	__CHAR_MIN	/* min value for a char */

#define	USHRT_MAX	__USHRT_MAX	/* max value for an unsigned short */
#define	SHRT_MAX	__SHRT_MAX	/* max value for a short */
#define	SHRT_MIN	__SHRT_MIN	/* min value for a short */

#define	UINT_MAX	__UNIT_MAX	/* max value for an unsigned int */
#define	INT_MAX		__INT_MAX	/* max value for an int */
#define	INT_MIN		__INT_MIN	/* min value for an int */

#define	ULONG_MAX	__ULONG_MAX	/* max for an unsigned long */
#define	LONG_MAX	__LONG_MAX	/* max for a long */
#define	LONG_MIN	__LONG_MIN	/* min for a long */

/* Long longs and longs are the same size on the alpha. */
					/* max for an unsigned long long */
#define	ULLONG_MAX	__ULLONG_MAX
#define	LLONG_MAX	__LLONG_MAX	/* max for a long long */
#define	LLONG_MIN	__LLONG_MIN	/* min for a long long */

#if !defined(_ANSI_SOURCE)
#define	SSIZE_MAX	__SSIZE_MAX	/* max value for a ssize_t */

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#define	SIZE_T_MAX	__SIZE_T_MAX	/* max value for a size_t */

#define	OFF_MAX		__OFF_MAX	/* max value for a off_t */
#define	OFF_MIN		__OFF_MIN	/* min value for a off_t */

/* Quads and longs are the same on the alpha.  Ensure they stay in sync. */
#define	UQUAD_MAX	(__UQUAD_MAX)	/* max value for a uquad_t */
#define	QUAD_MAX	(__QUAD_MAX)	/* max value for a quad_t */
#define	QUAD_MIN	(__QUAD_MIN)	/* min value for a quad_t */
#endif /* !_POSIX_SOURCE && !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE */

#if (!defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)) || defined(_XOPEN_SOURCE)
#define	LONG_BIT	__LONG_BIT
#define	WORD_BIT	__WORD_BIT

#define	DBL_DIG		__DBL_DIG
#define	DBL_MAX		__DBL_MAX
#define	DBL_MIN		__DBL_MIN

#define	FLT_DIG		__FLT_DIG
#define	FLT_MAX		__FLT_MAX
#define	FLT_MIN		__FLT_MIN
#endif

#endif /* !_SYS_LIMITS_H_ */
