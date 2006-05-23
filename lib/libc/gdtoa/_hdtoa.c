/*-
 * Copyright (c) 2004 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gdtoa/_hdtoa.c,v 1.2 2004/01/21 04:51:50 grehan Exp $");

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include "fpmath.h"
#include "gdtoaimp.h"

/* Strings values used by dtoa() */
#define	INFSTR	"Infinity"
#define	NANSTR	"NaN"

#define	DBL_BIAS	(DBL_MAX_EXP - 1)
#define	LDBL_BIAS	(LDBL_MAX_EXP - 1)

#ifdef	LDBL_IMPLICIT_NBIT
#define	LDBL_NBIT_ADJ	0
#else
#define	LDBL_NBIT_ADJ	1
#endif

/*
 * Efficiently compute the log2 of an integer.  Uses a combination of
 * arcane tricks found in fortune and arcane tricks not (yet) in
 * fortune.  This routine behaves similarly to fls(9).
 */
static int
log2_32(uint32_t n)
{

        n |= (n >> 1);
        n |= (n >> 2);
        n |= (n >> 4);
        n |= (n >> 8);
        n |= (n >> 16);

	n = (n & 0x55555555) + ((n & 0xaaaaaaaa) >> 1);
	n = (n & 0x33333333) + ((n & 0xcccccccc) >> 2);
	n = (n & 0x0f0f0f0f) + ((n & 0xf0f0f0f0) >> 4);
	n = (n & 0x00ff00ff) + ((n & 0xff00ff00) >> 8);
	n = (n & 0x0000ffff) + ((n & 0xffff0000) >> 16);
	return (n - 1);
}

#if (LDBL_MANH_SIZE > 32 || LDBL_MANL_SIZE > 32)

static int
log2_64(uint64_t n)
{

	if (n >> 32 != 0)
		return (log2_32((uint32_t)(n >> 32)) + 32);
	else
		return (log2_32((uint32_t)n));
}

#endif	/* (LDBL_MANH_SIZE > 32 || LDBL_MANL_SIZE > 32) */

/*
 * Round up the given digit string.  If the digit string is fff...f,
 * this procedure sets it to 100...0 and returns 1 to indicate that
 * the exponent needs to be bumped.  Otherwise, 0 is returned.
 */
static int
roundup(char *s0, int ndigits)
{
	char *s;

	for (s = s0 + ndigits - 1; *s == 0xf; s--) {
		if (s == s0) {
			*s = 1;
			return (1);
		}
		++*s;
	}
	++*s;
	return (0);
}

/*
 * Round the given digit string to ndigits digits according to the
 * current rounding mode.  Note that this could produce a string whose
 * value is not representable in the corresponding floating-point
 * type.  The exponent pointed to by decpt is adjusted if necessary.
 */
static void
dorounding(char *s0, int ndigits, int sign, int *decpt)
{
	int adjust = 0;	/* do we need to adjust the exponent? */

	switch (FLT_ROUNDS) {
	case 0:		/* toward zero */
	default:	/* implementation-defined */
		break;
	case 1:		/* to nearest, halfway rounds to even */
		if ((s0[ndigits] > 8) ||
		    (s0[ndigits] == 8 && s0[ndigits - 1] & 1))
			adjust = roundup(s0, ndigits);
		break;
	case 2:		/* toward +inf */
		if (sign == 0)
			adjust = roundup(s0, ndigits);
		break;
	case 3:		/* toward -inf */
		if (sign != 0)
			adjust = roundup(s0, ndigits);
		break;
	}

	if (adjust)
		*decpt += 4;
}

/*
 * This procedure converts a double-precision number in IEEE format
 * into a string of hexadecimal digits and an exponent of 2.  Its
 * behavior is bug-for-bug compatible with dtoa() in mode 2, with the
 * following exceptions:
 *
 * - An ndigits < 0 causes it to use as many digits as necessary to
 *   represent the number exactly.
 * - The additional xdigs argument should point to either the string
 *   "0123456789ABCDEF" or the string "0123456789abcdef", depending on
 *   which case is desired.
 * - This routine does not repeat dtoa's mistake of setting decpt
 *   to 9999 in the case of an infinity or NaN.  INT_MAX is used
 *   for this purpose instead.
 *
 * Note that the C99 standard does not specify what the leading digit
 * should be for non-zero numbers.  For instance, 0x1.3p3 is the same
 * as 0x2.6p2 is the same as 0x4.cp3.  This implementation chooses the
 * first digit so that subsequent digits are aligned on nibble
 * boundaries (before rounding).
 *
 * Inputs:	d, xdigs, ndigits
 * Outputs:	decpt, sign, rve
 */
char *
__hdtoa(double d, const char *xdigs, int ndigits, int *decpt, int *sign,
    char **rve)
{
	union IEEEd2bits u;
	char *s, *s0;
	int bufsize;
	int impnbit;	/* implicit normalization bit */
	int pos;
	int shift;	/* for subnormals, # of shifts required to normalize */
	int sigfigs;	/* number of significant hex figures in result */

	u.d = d;
	*sign = u.bits.sign;

	switch (fpclassify(d)) {
	case FP_NORMAL:
		sigfigs = (DBL_MANT_DIG + 3) / 4;
		impnbit = 1 << ((DBL_MANT_DIG - 1) % 4);
		*decpt = u.bits.exp - DBL_BIAS + 1 -
		    ((DBL_MANT_DIG - 1) % 4);
		break;
	case FP_ZERO:
		*decpt = 1;
		return (nrv_alloc("0", rve, 1));
	case FP_SUBNORMAL:
		/*
		 * The position of the highest-order bit tells us by
		 * how much to adjust the exponent (decpt).  The
		 * adjustment is raised to the next nibble boundary
		 * since we will later choose the leftmost hexadecimal
		 * digit so that all subsequent digits align on nibble
		 * boundaries.
		 */
		if (u.bits.manh != 0) {
			pos = log2_32(u.bits.manh);
			shift = DBL_MANH_SIZE - pos;
		} else {
			pos = log2_32(u.bits.manl);
			shift = DBL_MANH_SIZE + DBL_MANL_SIZE - pos;
		}
		sigfigs = (3 + DBL_MANT_DIG - shift) / 4;
		impnbit = 0;
		*decpt = DBL_MIN_EXP - ((shift + 3) & ~(4 - 1));
		break;
	case FP_INFINITE:
		*decpt = INT_MAX;
		return (nrv_alloc(INFSTR, rve, sizeof(INFSTR) - 1));
	case FP_NAN:
		*decpt = INT_MAX;
		return (nrv_alloc(NANSTR, rve, sizeof(NANSTR) - 1));
	default:
		abort();
	}

	/* FP_NORMAL or FP_SUBNORMAL */

	if (ndigits == 0)		/* dtoa() compatibility */
		ndigits = 1;

	/*
	 * For simplicity, we generate all the digits even if the
	 * caller has requested fewer.
	 */
	bufsize = (sigfigs > ndigits) ? sigfigs : ndigits;
	s0 = rv_alloc(bufsize);

	/*
	 * We work from right to left, first adding any requested zero
	 * padding, then the least significant portion of the
	 * mantissa, followed by the most significant.  The buffer is
	 * filled with the byte values 0x0 through 0xf, which are
	 * converted to xdigs[0x0] through xdigs[0xf] after the
	 * rounding phase.
	 */
	for (s = s0 + bufsize - 1; s > s0 + sigfigs - 1; s--)
		*s = 0;
	for (; s > s0 + sigfigs - (DBL_MANL_SIZE / 4) - 1 && s > s0; s--) {
		*s = u.bits.manl & 0xf;
		u.bits.manl >>= 4;
	}
	for (; s > s0; s--) {
		*s = u.bits.manh & 0xf;
		u.bits.manh >>= 4;
	}

	/*
	 * At this point, we have snarfed all the bits in the
	 * mantissa, with the possible exception of the highest-order
	 * (partial) nibble, which is dealt with by the next
	 * statement.  That nibble is usually in manh, but it could be
	 * in manl instead for small subnormals.  We also tack on the
	 * implicit normalization bit if appropriate.
	 */
	*s = u.bits.manh | u.bits.manl | impnbit;

	/* If ndigits < 0, we are expected to auto-size the precision. */
	if (ndigits < 0) {
		for (ndigits = sigfigs; s0[ndigits - 1] == 0; ndigits--)
			;
	}

	if (sigfigs > ndigits && s0[ndigits] != 0)
		dorounding(s0, ndigits, u.bits.sign, decpt);

	s = s0 + ndigits;
	if (rve != NULL)
		*rve = s;
	*s-- = '\0';
	for (; s >= s0; s--)
		*s = xdigs[(unsigned int)*s];

	return (s0);
}

#if (LDBL_MANT_DIG > DBL_MANT_DIG)

/*
 * This is the long double version of __hdtoa().
 *
 * On architectures that have an explicit integer bit, unnormals and
 * pseudo-denormals cause problems in the conversion routine, so they
 * are ``fixed'' by effectively toggling the integer bit.  Although
 * this is not correct behavior, the hardware will not produce these
 * formats externally.
 */
char *
__hldtoa(long double e, const char *xdigs, int ndigits, int *decpt, int *sign,
    char **rve)
{
	union IEEEl2bits u;
	char *s, *s0;
	int bufsize;
	int impnbit;	/* implicit normalization bit */
	int pos;
	int shift;	/* for subnormals, # of shifts required to normalize */
	int sigfigs;	/* number of significant hex figures in result */

	u.e = e;
	*sign = u.bits.sign;

	switch (fpclassify(e)) {
	case FP_NORMAL:
		sigfigs = (LDBL_MANT_DIG + 3) / 4;
		impnbit = 1 << ((LDBL_MANT_DIG - 1) % 4);
		*decpt = u.bits.exp - LDBL_BIAS + 1 -
		    ((LDBL_MANT_DIG - 1) % 4);
		break;
	case FP_ZERO:
		*decpt = 1;
		return (nrv_alloc("0", rve, 1));
	case FP_SUBNORMAL:
		/*
		 * The position of the highest-order bit tells us by
		 * how much to adjust the exponent (decpt).  The
		 * adjustment is raised to the next nibble boundary
		 * since we will later choose the leftmost hexadecimal
		 * digit so that all subsequent digits align on nibble
		 * boundaries.
		 */
#ifdef	LDBL_IMPLICIT_NBIT
		/* Don't trust the normalization bit to be off. */
		u.bits.manh &= ~(~0ULL << (LDBL_MANH_SIZE - 1));
#endif
		if (u.bits.manh != 0) {
#if LDBL_MANH_SIZE > 32
			pos = log2_64(u.bits.manh);
#else
			pos = log2_32(u.bits.manh);
#endif
			shift = LDBL_MANH_SIZE - LDBL_NBIT_ADJ - pos;
		} else {
#if LDBL_MANL_SIZE > 32
			pos = log2_64(u.bits.manl);
#else
			pos = log2_32(u.bits.manl);
#endif
			shift = LDBL_MANH_SIZE + LDBL_MANL_SIZE -
			    LDBL_NBIT_ADJ - pos;
		}
		sigfigs = (3 + LDBL_MANT_DIG - LDBL_NBIT_ADJ - shift) / 4;
		*decpt = LDBL_MIN_EXP + LDBL_NBIT_ADJ -
		    ((shift + 3) & ~(4 - 1));
		impnbit = 0;
		break;
	case FP_INFINITE:
		*decpt = INT_MAX;
		return (nrv_alloc(INFSTR, rve, sizeof(INFSTR) - 1));
	case FP_NAN:
		*decpt = INT_MAX;
		return (nrv_alloc(NANSTR, rve, sizeof(NANSTR) - 1));
	default:
		abort();
	}

	/* FP_NORMAL or FP_SUBNORMAL */

	if (ndigits == 0)		/* dtoa() compatibility */
		ndigits = 1;

	/*
	 * For simplicity, we generate all the digits even if the
	 * caller has requested fewer.
	 */
	bufsize = (sigfigs > ndigits) ? sigfigs : ndigits;
	s0 = rv_alloc(bufsize);

	/*
	 * We work from right to left, first adding any requested zero
	 * padding, then the least significant portion of the
	 * mantissa, followed by the most significant.  The buffer is
	 * filled with the byte values 0x0 through 0xf, which are
	 * converted to xdigs[0x0] through xdigs[0xf] after the
	 * rounding phase.
	 */
	for (s = s0 + bufsize - 1; s > s0 + sigfigs - 1; s--)
		*s = 0;
	for (; s > s0 + sigfigs - (LDBL_MANL_SIZE / 4) - 1 && s > s0; s--) {
		*s = u.bits.manl & 0xf;
		u.bits.manl >>= 4;
	}
	for (; s > s0; s--) {
		*s = u.bits.manh & 0xf;
		u.bits.manh >>= 4;
	}

	/*
	 * At this point, we have snarfed all the bits in the
	 * mantissa, with the possible exception of the highest-order
	 * (partial) nibble, which is dealt with by the next
	 * statement.  That nibble is usually in manh, but it could be
	 * in manl instead for small subnormals.  We also tack on the
	 * implicit normalization bit if appropriate.
	 */
	*s = u.bits.manh | u.bits.manl | impnbit;

	/* If ndigits < 0, we are expected to auto-size the precision. */
	if (ndigits < 0) {
		for (ndigits = sigfigs; s0[ndigits - 1] == 0; ndigits--)
			;
	}

	if (sigfigs > ndigits && s0[ndigits] != 0)
		dorounding(s0, ndigits, u.bits.sign, decpt);

	s = s0 + ndigits;
	if (rve != NULL)
		*rve = s;
	*s-- = '\0';
	for (; s >= s0; s--)
		*s = xdigs[(unsigned int)*s];

	return (s0);
}

#else	/* (LDBL_MANT_DIG == DBL_MANT_DIG) */

char *
__hldtoa(long double e, const char *xdigs, int ndigits, int *decpt, int *sign,
    char **rve)
{

	return (__hdtoa((double)e, xdigs, ndigits, decpt, sign, rve));
}

#endif	/* (LDBL_MANT_DIG == DBL_MANT_DIG) */
