/* $FreeBSD$ */
/* From: NetBSD: limits.h,v 1.3 1997/04/06 08:47:31 cgd Exp */

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
 *	@(#)limits.h	8.3 (Berkeley) 1/4/94
 */

#define	CHAR_BIT	8		/* number of bits in a char */
#define	MB_LEN_MAX	6		/* Allow 31 bit UTF2 */

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
#define	SCHAR_MAX	0x7f		/* max value for a signed char */
#define	SCHAR_MIN	(-0x7f-1)	/* min value for a signed char */

#define	UCHAR_MAX	0xffU		/* max value for an unsigned char */
#define	CHAR_MAX	0x7f		/* max value for a char */
#define	CHAR_MIN	(-0x7f-1)	/* min value for a char */

#define	USHRT_MAX	0xffffU		/* max value for an unsigned short */
#define	SHRT_MAX	0x7fff		/* max value for a short */
#define	SHRT_MIN	(-0x7fff-1)	/* min value for a short */

#define	UINT_MAX	0xffffffffU	/* max value for an unsigned int */
#define	INT_MAX		0x7fffffff	/* max value for an int */
#define	INT_MIN		(-0x7fffffff-1)	/* min value for an int */

#define	ULONG_MAX	0xffffffffffffffffUL	/* max for an unsigned long */
#define	LONG_MAX	0x7fffffffffffffffL	/* max for a long */
#define	LONG_MIN	(-0x7fffffffffffffffL-1) /* min for a long */

/* Long longs and longs are the same size on the IA-64. */
					/* max for an unsigned long long */
#define	ULLONG_MAX	0xffffffffffffffffULL
#define	LLONG_MAX	0x7fffffffffffffffLL	/* max for a long long */
#define	LLONG_MIN	(-0x7fffffffffffffffLL-1) /* min for a long long */

#if !defined(_ANSI_SOURCE)
#define	SSIZE_MAX	LONG_MAX	/* max value for a ssize_t */

#if !defined(_POSIX_SOURCE) && !defined(_XOPEN_SOURCE)
#define	SIZE_T_MAX	ULONG_MAX	/* max value for a size_t */

#define	OFF_MAX		LONG_MAX	/* max value for a off_t */
#define	OFF_MIN		LONG_MIN	/* min value for a off_t */

/* Quads and longs are the same.  Ensure they stay in sync. */
#define	UQUAD_MAX	(ULONG_MAX)	/* max value for a uquad_t */
#define	QUAD_MAX	(LONG_MAX)	/* max value for a quad_t */
#define	QUAD_MIN	(LONG_MIN)	/* min value for a quad_t */

#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)
/*
 * ISO/IEC 9899:1999
 * 7.18.2.1 Limits of exact-width integer types
 */
/* Minimum values of exact-width signed integer types. */
#define	INT8_MIN	(-0x7f-1)
#define	INT16_MIN	(-0x7fff-1)
#define	INT32_MIN	(-0x7fffffff-1)
#define	INT64_MIN	(-0x7fffffffffffffffL-1)

/* Maximum values of exact-width signed integer types. */
#define	INT8_MAX	0x7f
#define	INT16_MAX	0x7fff
#define	INT32_MAX	0x7fffffff
#define	INT64_MAX	0x7fffffffffffffffL

/* Maximum values of exact-width unsigned integer types. */
#define	UINT8_MAX	0xff
#define	UINT16_MAX	0xffff
#define	UINT32_MAX	0xffffffffU
#define	UINT64_MAX	0xffffffffffffffffUL

/*
 * ISO/IEC 9899:1999
 * 7.18.2.2  Limits of minimum-width integer types
 */
/* Minimum values of minimum-width signed integer types. */
#define	INT_LEAST8_MIN	SCHAR_MIN
#define	INT_LEAST16_MIN	SHRT_MIN
#define	INT_LEAST32_MIN	INT_MIN
#define	INT_LEAST64_MIN	LONG_MIN

/* Maximum values of minimum-width signed integer types. */
#define	INT_LEAST8_MAX	SCHAR_MAX
#define	INT_LEAST16_MAX	SHRT_MAX
#define	INT_LEAST32_MAX	INT_MAX
#define	INT_LEAST64_MAX	LONG_MAX

/* Maximum values of minimum-width unsigned integer types. */
#define	UINT_LEAST8_MAX	 UCHAR_MAX
#define	UINT_LEAST16_MAX USHRT_MAX
#define	UINT_LEAST32_MAX UINT_MAX
#define	UINT_LEAST64_MAX ULONG_MAX

/*
 * ISO/IEC 9899:1999
 * 7.18.2.3  Limits of fastest minimum-width integer types
 */
/* Minimum values of fastest minimum-width signed integer types. */
#define	INT_FAST8_MIN	INT_MIN
#define	INT_FAST16_MIN	INT_MIN
#define	INT_FAST32_MIN	INT_MIN
#define	INT_FAST64_MIN	LONG_MIN

/* Maximum values of fastest minimum-width signed integer types. */
#define	INT_FAST8_MAX	INT_MAX
#define	INT_FAST16_MAX	INT_MAX
#define	INT_FAST32_MAX	INT_MAX
#define	INT_FAST64_MAX	LONG_MAX

/* Maximum values of fastest minimum-width unsigned integer types. */
#define	UINT_FAST8_MAX	UINT_MAX
#define	UINT_FAST16_MAX	UINT_MAX
#define	UINT_FAST32_MAX	UINT_MAX
#define	UINT_FAST64_MAX	ULONG_MAX

/*
 * ISO/IEC 9899:1999
 * 7.18.2.4  Limits of integer types capable of holding object pointers
 */
#define	INTPTR_MIN	LONG_MIN
#define	INTPTR_MAX	LONG_MAX
#define	UINTPTR_MAX	ULONG_MAX

/*
 * ISO/IEC 9899:1999
 * 7.18.2.5  Limits of greatest-width integer types
 */
#define	INTMAX_MIN	LONG_MIN
#define	INTMAX_MAX	LONG_MAX
#define	UINTMAX_MAX	ULONG_MAX

/*
 * ISO/IEC 9899:1999
 * 7.18.3  Limits of other integer types
 */
/* Limits of ptrdiff_t. */
#define	PTRDIFF_MIN	LONG_MIN	
#define	PTRDIFF_MAX	LONG_MAX

/* Limits of sig_atomic_t. */
#define	SIG_ATOMIC_MIN	INT_MIN
#define	SIG_ATOMIC_MAX	INT_MAX

/* Limit of size_t. */
#define	SIZE_MAX	ULONG_MAX

#ifndef WCHAR_MIN /* Also possibly defined in <wchar.h> */
/* Limits of wchar_t. */
#define	WCHAR_MIN	INT_MIN
#define	WCHAR_MAX	INT_MAX

/* Limits of wint_t. */
#define	WINT_MIN	INT_MIN
#define	WINT_MAX	INT_MAX
#endif
#endif /* !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS) */
#endif /* !_POSIX_SOURCE && !_XOPEN_SOURCE */
#endif /* !_ANSI_SOURCE */

#if (!defined(_ANSI_SOURCE)&&!defined(_POSIX_SOURCE)) || defined(_XOPEN_SOURCE)
#define	LONG_BIT	64
#define	WORD_BIT	32

#define	DBL_DIG		15
#define	DBL_MAX		1.7976931348623157E+308
#define	DBL_MIN		2.2250738585072014E-308

#define	FLT_DIG		6
#define	FLT_MAX		3.40282347E+38F
#define	FLT_MIN		1.17549435E-38F
#endif
