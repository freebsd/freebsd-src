/*-
 * Copyright (c) 1998 Doug Rabson
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
 *
 */

/*
 * An implementation of IEEE 754 floating point arithmetic supporting
 * multiply, divide, addition, subtraction and conversion to and from
 * integer.  Probably not the fastest floating point code in the world
 * but it should be pretty accurate.
 *
 * A special thanks to John Polstra for pointing out some problems
 * with an earlier version of this code and for educating me as to the
 * correct use of sticky bits.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#ifdef TEST
#include "../include/fpu.h"
#include "ieee_float.h"
#else
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/proc.h>
#include <machine/fpu.h>
#include <alpha/alpha/ieee_float.h>
#endif

/*
 * The number of fraction bits in a T format float.
 */
#define T_FRACBITS	52

/*
 * The number of fraction bits in a S format float.
 */
#define S_FRACBITS	23

/*
 * Mask the fraction part of a float to contain only those bits which
 * should be in single precision number.
 */
#define S_FRACMASK	((1ULL << 52) - (1ULL << 29))

/*
 * The number of extra zero bits we shift into the fraction part
 * to gain accuracy.  Two guard bits and one sticky bit are required
 * to ensure accurate rounding.
 */
#define FRAC_SHIFT	3

/*
 * Values for 1.0 and 2.0 fractions (including the extra FRAC_SHIFT
 * bits).
 */
#define ONE		(1ULL << (T_FRACBITS + FRAC_SHIFT))
#define TWO		(ONE + ONE)

/*
 * The maximum and minimum values for S and T format exponents.
 */
#define T_MAXEXP	0x3ff
#define T_MINEXP	-0x3fe
#define S_MAXEXP	0x7f
#define S_MINEXP	-0x7e

/*
 * Exponent values in registers are biased by adding this value.
 */
#define BIAS_EXP	0x3ff

/*
 * Exponent value for INF and NaN.
 */
#define NAN_EXP		0x7ff

/*
 * If this bit is set in the fraction part of a NaN, then the number
 * is a quiet NaN, i.e. no traps are generated.
 */
#define QNAN_BIT	(1ULL << 51)

/*
 * Return true if the number is any kind of NaN.
 */
static __inline int
isNaN(fp_register_t f)
{
	return f.t.exponent == NAN_EXP && f.t.fraction != 0;
}

/*
 * Return true if the number is a quiet NaN.
 */
static __inline int
isQNaN(fp_register_t f)
{
	return f.t.exponent == NAN_EXP && (f.t.fraction & QNAN_BIT);
}

/*
 * Return true if the number is a signalling NaN.
 */
static __inline int
isSNaN(fp_register_t f)
{
	return isNaN(f) && !isQNaN(f);
}

/*
 * Return true if the number is +/- INF.
 */
static __inline int
isINF(fp_register_t f)
{
	return f.t.exponent == NAN_EXP && f.t.fraction == 0;
}

/*
 * Return true if the number is +/- 0.
 */
static __inline int
isZERO(fp_register_t f)
{
	return f.t.exponent == 0 && f.t.fraction == 0;
}

/*
 * Return true if the number is denormalised.
 */
static __inline int
isDENORM(fp_register_t f)
{
	return f.t.exponent == 0 && f.t.fraction != 0;
}

/*
 * Extract the exponent part of a float register.  If the exponent is
 * zero, the number may be denormalised (if the fraction is nonzero).
 * If so, return the minimum exponent for the source datatype.
 */
static __inline int
getexp(fp_register_t f, int src)
{
	int minexp[] = { S_MINEXP, 0, T_MINEXP, 0 };
	if (f.t.exponent == 0) {
		if (f.t.fraction)
			return minexp[src];
		else
			return 0;
	}
	return f.t.exponent - BIAS_EXP;
}

/*
 * Extract the fraction part of a float register, shift it up a bit
 * to give extra accuracy and add in the implicit 1 bit.  Must be
 * careful to handle denormalised numbers and zero correctly.
 */
static __inline u_int64_t
getfrac(fp_register_t f)
{
	if (f.t.exponent == 0)
		return f.t.fraction << FRAC_SHIFT;
	else
		return (f.t.fraction << FRAC_SHIFT) | ONE;
}

/*
 * Make a float (in register format) from a sign, exponent and
 * fraction, normalising and rounding as necessary.
 * Return the float and set *status if any traps are generated.
 */
static fp_register_t
makefloat(int sign, int exp, u_int64_t frac,
	  int src, int rnd,
	  u_int64_t control, u_int64_t *status)
{
	fp_register_t f;
	int minexp = 0, maxexp = 0, alpha = 0;
	u_int64_t epsilon = 0, max = 0;

 	if (frac == 0) {
		f.t.sign = sign;
		f.t.exponent = 0;
		f.t.fraction = 0;
		return f;
	}

	if (frac >= TWO) {
		/*
		 * Fraction is >= 2.0.
		 * Shift the fraction down, preserving the 'sticky'
		 * bit.
		 */
		while (frac >= TWO) {
			frac = (frac >> 1) | (frac & 1);
			exp++;
		}
	} else if (frac < ONE) {
		/*
		 * Fraction is < 1.0. Shift it up.
		 */
		while (frac < ONE) {
			frac = (frac << 1) | (frac & 1);
			exp--;
		}
	}

	switch (src) {
	case S_FORMAT:
		minexp = S_MINEXP;
		maxexp = S_MAXEXP;
		alpha = 0xc0;
		epsilon = (1ULL << (T_FRACBITS - S_FRACBITS + FRAC_SHIFT));
		max = TWO - epsilon;
		break;

	case T_FORMAT:
		minexp = T_MINEXP;
		maxexp = T_MAXEXP;
		alpha = 0x600;
		epsilon = (1ULL << FRAC_SHIFT);
		max = TWO - epsilon;
		break;
	}
		
	/*
	 * Handle underflow before rounding so that denormalised
	 * numbers are rounded correctly.
	 */
	if (exp < minexp) {
		*status |= FPCR_INE;
		if (control & IEEE_TRAP_ENABLE_UNF) {
			*status |= FPCR_UNF;
			exp += alpha;
		} else {
			/* denormalise */
			while (exp < minexp) {
				exp++;
				frac = (frac >> 1) | (frac & 1);
			}
			exp = minexp - 1;
		}
	}

	/*
	 * Round the fraction according to the rounding mode.
	 */
	if (frac & (epsilon - 1)) {
		u_int64_t fraclo, frachi;
		u_int64_t difflo, diffhi;

		fraclo = frac & max;
		frachi = fraclo + epsilon;
		switch (rnd) {
		case ROUND_CHOP:
			frac = fraclo;
			break;
		case ROUND_MINUS_INF:
			if (f.t.sign)
				frac = frachi;
			else
				frac = fraclo;
			break;
		case ROUND_NORMAL:
			difflo = frac - fraclo;
			diffhi = frachi - frac;
			if (difflo < diffhi)
				frac = fraclo;
			else if (diffhi < difflo)
				frac = frachi;
			else
				/* round to even */
				if (fraclo & epsilon)
					frac = frachi;
				else
					frac = fraclo;
			break;
		case ROUND_PLUS_INF:
			if (f.t.sign)
				frac = fraclo;
			else
				frac = frachi;
			break;
		}

		if (frac == 0)
			*status |= FPCR_UNF;

		/*
		 * Rounding up may take us to TWO if
		 * fraclo == (TWO - epsilon).  Also If fraclo has been
		 * denormalised to (ONE - epsilon) then there is a
		 * possibility that we will round to ONE exactly.
		 */
		if (frac >= TWO) {
			frac = (frac >> 1) & ~(epsilon - 1);
			exp++;
		} else if (exp == minexp - 1 && frac == ONE) {
			/* Renormalise to ONE * 2^minexp */
			exp = minexp;
		}

		*status |= FPCR_INE;
	}

	/*
	 * Check for overflow and round to the correct INF as needed.
	 */
	if (exp > maxexp) {
		*status |= FPCR_OVF | FPCR_INE;
		if (control & IEEE_TRAP_ENABLE_OVF) {
			exp -= alpha;
		} else {
			switch (rnd) {
			case ROUND_CHOP:
				exp = maxexp;
				frac = max;
				break;
			case ROUND_MINUS_INF:
				if (sign) {
					exp = maxexp + 1; /* INF */
					frac = 0;
				} else {
					exp = maxexp;
					frac = max;
				}
				break;
			case ROUND_NORMAL:
				exp = maxexp + 1; /* INF */
				frac = 0;
				break;
			case ROUND_PLUS_INF:
				if (sign) {
					exp = maxexp;
					frac = max;
				} else {
					exp = maxexp + 1; /* INF */
					frac = 0;
				}
				break;
			}
		}
	}

	f.t.sign = sign;
	if (exp > maxexp)	/* NaN, INF */
		f.t.exponent = NAN_EXP;
	else if (exp < minexp)	/* denorm, zero */
		f.t.exponent = 0;
	else
		f.t.exponent = exp + BIAS_EXP;
	f.t.fraction = (frac & ~ONE) >> FRAC_SHIFT;
	return f;
}

/*
 * Return the canonical quiet NaN in register format.
 */
static fp_register_t
makeQNaN(void)
{
	fp_register_t f;
	f.t.sign = 0;
	f.t.exponent = NAN_EXP;
	f.t.fraction = QNAN_BIT;
	return f;
}

/*
 * Return +/- INF.
 */
static fp_register_t
makeINF(int sign)
{
	fp_register_t f;
	f.t.sign = sign;
	f.t.exponent = NAN_EXP;
	f.t.fraction = 0;
	return f;
}

/*
 * Return +/- 0.
 */
static fp_register_t
makeZERO(int sign)
{
	fp_register_t f;
	f.t.sign = sign;
	f.t.exponent = 0;
	f.t.fraction = 0;
	return f;
}

fp_register_t
ieee_add(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status)
{
	int shift;
	int expa, expb, exp;
	u_int64_t fraca, fracb, frac;
	int sign, sticky;

	/* First handle NaNs */
	if (isNaN(fa) || isNaN(fb)) {
		fp_register_t result;

		/* Instructions Descriptions (I) section 4.7.10.4 */
		if (isQNaN(fb))
			result = fb;
		else if (isSNaN(fb)) {
			result = fb;
			result.t.fraction |= QNAN_BIT;
		} else if (isQNaN(fa))
			result = fa;
		else if (isSNaN(fa)) {
			result = fa;
			result.t.fraction |= QNAN_BIT;
		}
		
		/* If either operand is a signalling NaN, trap. */
		if (isSNaN(fa) || isSNaN(fb))
			*status |= FPCR_INV;

		return result;
	}

	/* Handle +/- INF */
	if (isINF(fa))
		if (isINF(fb))
			if (fa.t.sign != fb.t.sign) {
				/* If adding -INF to +INF, generate a trap. */
				*status |= FPCR_INV;
				return makeQNaN();
			} else
				return fa;
		else
			return fa;
	else if (isINF(fb))
		return fb;

	/*
	 * Unpack the registers.
	 */
	expa = getexp(fa, src);
	expb = getexp(fb, src);
	fraca = getfrac(fa);
	fracb = getfrac(fb);
	shift = expa - expb;
	if (shift < 0) {
		shift = -shift;
		exp = expb;
		sticky = (fraca & ((1ULL << shift) - 1)) != 0;
		if (shift >= 64)
			fraca = sticky;
		else
			fraca = (fraca >> shift) | sticky;
	} else if (shift > 0) {
		exp = expa;
		sticky = (fracb & ((1ULL << shift) - 1)) != 0;
		if (shift >= 64)
			fracb = sticky;
		else
			fracb = (fracb >> shift) | sticky;
	} else
		exp = expa;
	if (fa.t.sign) fraca = -fraca;
	if (fb.t.sign) fracb = -fracb;
	frac = fraca + fracb;
	if (frac >> 63) {
		sign = 1;
		frac = -frac;
	} else
		sign = 0;

	/* -0 + -0 = -0 */
	if (fa.t.exponent == 0 && fa.t.fraction == 0
	    && fb.t.exponent == 0 && fb.t.fraction == 0)
		sign = fa.t.sign && fb.t.sign;

	return makefloat(sign, exp, frac, src, rnd, control, status);
}

fp_register_t
ieee_sub(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status)
{
	fb.t.sign = !fb.t.sign;
	return ieee_add(fa, fb, src, rnd, control, status);
}

typedef struct {
	u_int64_t lo;
	u_int64_t hi;
} u_int128_t;

#define SRL128(x, b)				\
do {						\
	x.lo >>= b;				\
	x.lo |= x.hi << (64 - b);		\
	x.hi >>= b;				\
} while (0)

#define SLL128(x, b)				\
do {						\
	if (b >= 64) {				\
		x.hi = x.lo << (b - 64);	\
		x.lo = 0;			\
	} else {				\
		x.hi <<= b;			\
		x.hi |= x.lo >> (64 - b);	\
		x.lo <<= b;			\
	}					\
} while (0)

#define SUB128(a, b)				\
do {						\
	int borrow = a.lo < b.lo;		\
	a.lo = a.lo - b.lo;			\
	a.hi = a.hi - b.hi - borrow;		\
} while (0)

#define LESS128(a, b) (a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo))

fp_register_t
ieee_mul(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status)
{
	int expa, expb, exp;
	u_int64_t fraca, fracb, tmp;
	u_int128_t frac;
	int sign;

	/* First handle NaNs */
	if (isNaN(fa) || isNaN(fb)) {
		fp_register_t result;

		/* Instructions Descriptions (I) section 4.7.10.4 */
		if (isQNaN(fb))
			result = fb;
		else if (isSNaN(fb)) {
			result = fb;
			result.t.fraction |= QNAN_BIT;
		} else if (isQNaN(fa))
			result = fa;
		else if (isSNaN(fa)) {
			result = fa;
			result.t.fraction |= QNAN_BIT;
		}

		/* If either operand is a signalling NaN, trap. */
		if (isSNaN(fa) || isSNaN(fb))
			*status |= FPCR_INV;

		return result;
	}

	/* Handle INF and 0 */
	if ((isINF(fa) && isZERO(fb)) || (isZERO(fa) && isINF(fb))) {
		/* INF * 0 = NaN */
		*status |= FPCR_INV;
		return makeQNaN();
	} else
		/* If either is INF or zero, get the sign right */
		if (isINF(fa) || isINF(fb))
			return makeINF(fa.t.sign ^ fb.t.sign);
		else if (isZERO(fa) || isZERO(fb))
			return makeZERO(fa.t.sign ^ fb.t.sign);

	/*
	 * Unpack the registers.
	 */
	expa = getexp(fa, src);
	expb = getexp(fb, src);
	fraca = getfrac(fa);
	fracb = getfrac(fb);
	sign = fa.t.sign ^ fb.t.sign;

#define LO32(x)	((x) & ((1ULL << 32) - 1))
#define HI32(x)	((x) >> 32)

	/*
	 * Calculate the 128bit result of multiplying fraca and fracb.
	 */
	frac.lo = fraca * fracb;
#ifdef __alpha__
	/*
	 * The alpha has a handy instruction to find the high word.
	 */
	__asm__ __volatile__ ("umulh %1,%2,%0"
			      : "=r"(tmp)
			      : "r"(fraca), "r"(fracb));
	frac.hi = tmp;
#else
	/*
	 * Do the multiply longhand otherwise.
	 */
	frac.hi = HI32(LO32(fraca) * HI32(fracb)
		       + HI32(fraca) * LO32(fracb)
		       + HI32(LO32(fraca) * LO32(fracb)))
		+ HI32(fraca) * HI32(fracb);
#endif
	exp = expa + expb - (T_FRACBITS + FRAC_SHIFT);

	while (frac.hi > 0) {
		int sticky;
		exp++;
		sticky = frac.lo & 1;
		SRL128(frac, 1);
		frac.lo |= sticky;
	}

	return makefloat(sign, exp, frac.lo, src, rnd, control, status);
}

static u_int128_t
divide_128(u_int128_t a, u_int128_t b)
{
	u_int128_t result;
	u_int64_t bit;
	int i;

	/*
	 * Make a couple of assumptions on the numbers passed in.  The
	 * value in 'a' will have bits set in the upper 64 bits only
	 * and the number in 'b' will have zeros in the upper 64 bits.
	 * Also, 'b' will not be zero.
	 */
#ifdef TEST
	if (a.hi == 0 || b.hi != 0 || b.lo == 0)
		abort();
#endif

	/*
	 * Find out how many bits of zeros are at the beginning of the divisor.
	 */
	i = 64;
	bit = 1ULL << 63;
	while (i < 127) {
		if (b.lo & bit)
			break;
		i++;
		bit >>= 1;
	}

	/*
	 * Find out how much to shift the divisor so that its msb
	 * matches the msb of the dividend.
	 */
	bit = 1ULL << 63;
	while (i) {
		if (a.hi & bit)
			break;
		i--;
		bit >>= 1;
	}
	
	result.lo = 0;
	result.hi = 0;
	SLL128(b, i);

	/*
	 * Calculate the result in two parts to avoid keeping a 128bit
	 * value for the result bit.
	 */
	if (i >= 64) {
		bit = 1ULL << (i - 64);
		while (bit) {
			if (!LESS128(a, b)) {
				result.hi |= bit;
				SUB128(a, b);
				if (!a.lo && !a.hi)
					return result;
			}
			bit >>= 1;
			SRL128(b, 1);
		}
		i = 63;
	}
	bit = 1ULL << i;
	while (bit) {
		if (!LESS128(a, b)) {
			result.lo |= bit;
			SUB128(a, b);
			if (!a.lo && !a.hi)
				return result;
		}
		bit >>= 1;
		SRL128(b, 1);
	}

	return result;
}

fp_register_t
ieee_div(fp_register_t fa, fp_register_t fb,
	 int src, int rnd,
	 u_int64_t control, u_int64_t *status)
{
	int expa, expb, exp;
	u_int128_t fraca, fracb, frac;
	int sign;

	/* First handle NaNs, INFs and ZEROs */
	if (isNaN(fa) || isNaN(fb)) {
		fp_register_t result;

		/* Instructions Descriptions (I) section 4.7.10.4 */
		if (isQNaN(fb))
			result = fb;
		else if (isSNaN(fb)) {
			result = fb;
			result.t.fraction |= QNAN_BIT;
		} else if (isQNaN(fa))
			result = fa;
		else if (isSNaN(fa)) {
			result = fa;
			result.t.fraction |= QNAN_BIT;
		}
		
		/* If either operand is a signalling NaN, trap. */
		if (isSNaN(fa) || isSNaN(fb))
			*status |= FPCR_INV;

		return result;
	}

	/* Handle INF and 0 */
	if (isINF(fa) && isINF(fb)) {
		*status |= FPCR_INV;
		return makeQNaN();
	} else if (isZERO(fb))
		if (isZERO(fa)) {
			*status |= FPCR_INV;
			return makeQNaN();
		} else {
			*status |= FPCR_DZE;
			return makeINF(fa.t.sign ^ fb.t.sign);
		}
	else if (isZERO(fa))
		return makeZERO(fa.t.sign ^ fb.t.sign);

	/*
	 * Unpack the registers.
	 */
	expa = getexp(fa, src);
	expb = getexp(fb, src);
	fraca.hi = getfrac(fa);
	fraca.lo = 0;
	fracb.lo = getfrac(fb);
	fracb.hi = 0;
	sign = fa.t.sign ^ fb.t.sign;

	frac = divide_128(fraca, fracb);

	exp = expa - expb - (64 - T_FRACBITS - FRAC_SHIFT);
	while (frac.hi > 0) {
		int sticky;
		exp++;
		sticky = frac.lo & 1;
		SRL128(frac, 1);
		frac.lo |= sticky;
	}
	frac.lo |= 1;

	return makefloat(sign, exp, frac.lo, src, rnd, control, status);
}

#define IEEE_TRUE 0x4000000000000000ULL
#define IEEE_FALSE 0

fp_register_t
ieee_cmpun(fp_register_t fa, fp_register_t fb, u_int64_t *status)
{
	fp_register_t result;
	if (isNaN(fa) || isNaN(fb)) {
		if (isSNaN(fa) || isSNaN(fb))
			*status |= FPCR_INV;
		result.q = IEEE_TRUE;
	} else
		result.q = IEEE_FALSE;

	return result;
}

fp_register_t
ieee_cmpeq(fp_register_t fa, fp_register_t fb, u_int64_t *status)
{
	fp_register_t result;
	if (isNaN(fa) || isNaN(fb)) {
		if (isSNaN(fa) || isSNaN(fb))
			*status |= FPCR_INV;
		result.q = IEEE_FALSE;
	} else {
		if (isZERO(fa) && isZERO(fb))
			result.q = IEEE_TRUE;
		else if (fa.q == fb.q)
			result.q = IEEE_TRUE;
		else
			result.q = IEEE_FALSE;
	}

	return result;
}

fp_register_t
ieee_cmplt(fp_register_t fa, fp_register_t fb, u_int64_t *status)
{
	fp_register_t result;
	if (isNaN(fa) || isNaN(fb)) {
		if (isSNaN(fa) || isSNaN(fb))
			*status |= FPCR_INV;
		result.q = IEEE_FALSE;
	} else {
		if (isZERO(fa) && isZERO(fb))
			result.q = IEEE_FALSE;
		else if (fa.t.sign) {
			/* fa is negative */
			if (!fb.t.sign)
				/* fb is positive, return true */
				result.q = IEEE_TRUE;
			else if (fa.t.exponent > fb.t.exponent)
				/* fa has a larger exponent, return true */
				result.q = IEEE_TRUE;
			else if (fa.t.exponent == fb.t.exponent
				 && fa.t.fraction > fb.t.fraction)
				/* compare fractions */
				result.q = IEEE_TRUE;
			else
				result.q = IEEE_FALSE;
		} else {
			/* fa is positive */
			if (fb.t.sign)
				/* fb is negative, return false */
				result.q = IEEE_FALSE;
			else if (fb.t.exponent > fa.t.exponent)
				/* fb has a larger exponent, return true */
				result.q = IEEE_TRUE;
			else if (fa.t.exponent == fb.t.exponent
				 && fa.t.fraction < fb.t.fraction)
				/* compare fractions */
				result.q = IEEE_TRUE;
			else
				result.q = IEEE_FALSE;
		}
	}

	return result;
}

fp_register_t
ieee_cmple(fp_register_t fa, fp_register_t fb, u_int64_t *status)
{
	fp_register_t result;
	if (isNaN(fa) || isNaN(fb)) {
		if (isSNaN(fa) || isSNaN(fb))
			*status |= FPCR_INV;
		result.q = IEEE_FALSE;
	} else {
		if (isZERO(fa) && isZERO(fb))
			result.q = IEEE_TRUE;
		else if (fa.t.sign) {
			/* fa is negative */
			if (!fb.t.sign)
				/* fb is positive, return true */
				result.q = IEEE_TRUE;
			else if (fa.t.exponent > fb.t.exponent)
				/* fa has a larger exponent, return true */
				result.q = IEEE_TRUE;
			else if (fa.t.exponent == fb.t.exponent
				 && fa.t.fraction >= fb.t.fraction)
				/* compare fractions */
				result.q = IEEE_TRUE;
			else
				result.q = IEEE_FALSE;
		} else {
			/* fa is positive */
			if (fb.t.sign)
				/* fb is negative, return false */
				result.q = IEEE_FALSE;
			else if (fb.t.exponent > fa.t.exponent)
				/* fb has a larger exponent, return true */
				result.q = IEEE_TRUE;
			else if (fa.t.exponent == fb.t.exponent
				 && fa.t.fraction <= fb.t.fraction)
				/* compare fractions */
				result.q = IEEE_TRUE;
			else
				result.q = IEEE_FALSE;
		}
	}

	return result;
}

fp_register_t
ieee_convert_S_T(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status)
{
	/*
	 * Handle exceptional values.
	 */
	if (isNaN(f)) {
		/* Instructions Descriptions (I) section 4.7.10.1 */
		f.t.fraction |= QNAN_BIT;
		*status |= FPCR_INV;
	}
	if (isQNaN(f) || isINF(f))
		return f;

	/*
	 * If the number is a denormalised float, renormalise.
	 */
	if (isDENORM(f))
		return makefloat(f.t.sign,
				 getexp(f, S_FORMAT),
				 getfrac(f),
				 T_FORMAT, rnd, control, status);
	else
		return f;
}

fp_register_t
ieee_convert_T_S(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status)
{
	/*
	 * Handle exceptional values.
	 */
	if (isNaN(f)) {
		/* Instructions Descriptions (I) section 4.7.10.1 */
		f.t.fraction |= QNAN_BIT;
		f.t.fraction &= ~S_FRACMASK;
		*status |= FPCR_INV;
	}
	if (isQNaN(f) || isINF(f))
		return f;

	return makefloat(f.t.sign,
			 getexp(f, T_FORMAT),
			 getfrac(f),
			 S_FORMAT, rnd, control, status);
}

fp_register_t
ieee_convert_Q_S(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status)
{
	u_int64_t frac = f.q;
	int sign, exponent;

	if (frac >> 63) {
		sign = 1;
		frac = -frac;
	} else
		sign = 0;
	
	/*
	 * We shift up one bit to leave the sticky bit clear.  This is
	 * possible unless frac == (1<<63), in which case the sticky
	 * bit is already clear.
	 */
	exponent = T_FRACBITS + FRAC_SHIFT;
	if (frac < (1ULL << 63)) {
		frac <<= 1;
		exponent--;
	}

	return makefloat(sign, exponent, frac, S_FORMAT, rnd,
			 control, status);
}

fp_register_t
ieee_convert_Q_T(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status)
{
	u_int64_t frac = f.q;
	int sign, exponent;

	if (frac >> 63) {
		sign = 1;
		frac = -frac;
	} else
		sign = 0;
	
	/*
	 * We shift up one bit to leave the sticky bit clear.  This is
	 * possible unless frac == (1<<63), in which case the sticky
	 * bit is already clear.
	 */
	exponent = T_FRACBITS + FRAC_SHIFT;
	if (frac < (1ULL << 63)) {
		frac <<= 1;
		exponent--;
	}
	
	return makefloat(sign, exponent, frac, T_FORMAT, rnd,
			 control, status);
}

fp_register_t
ieee_convert_T_Q(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status)
{
	u_int64_t frac;
	int exp;

	/*
	 * Handle exceptional values.
	 */
	if (isNaN(f)) {
		/* Instructions Descriptions (I) section 4.7.10.1 */
		if (isSNaN(f))
			*status |= FPCR_INV;
		f.q = 0;
		return f;
	}
	if (isINF(f)) {
		/* Instructions Descriptions (I) section 4.7.10.1 */
		*status |= FPCR_INV;
		f.q = 0;
		return f;
	}

	exp = getexp(f, T_FORMAT) - (T_FRACBITS + FRAC_SHIFT);
	frac = getfrac(f);

	if (exp > 0) {
		if (exp > 64 || frac >= (1 << (64 - exp)))
			*status |= FPCR_IOV | FPCR_INE;
		if (exp < 64)
			frac <<= exp;
		else
			frac = 0;
	} else if (exp < 0) {
		u_int64_t mask;
		u_int64_t fraclo, frachi;
		u_int64_t diffhi, difflo;
		exp = -exp;
		if (exp > 64) {
			fraclo = 0;
			diffhi = 0;
			difflo = 0;
			if (frac) {
				frachi = 1;
				*status |= FPCR_INE;
			} else
				frachi = 0;
		} else if (exp == 64) {
			fraclo = 0;
			if (frac) {
				frachi = 1;
				difflo = frac;
				diffhi = -frac;
				*status |= FPCR_INE;
			} else {
				frachi = 0;
				difflo = 0;
				diffhi = 0;
			}
		} else {
			mask = (1 << exp) - 1;
			fraclo = frac >> exp;
			if (frac & mask) {
				frachi = fraclo + 1;
				difflo = frac - (fraclo << exp);
				diffhi = (frachi << exp) - frac;
				*status |= FPCR_INE;
			} else {
				frachi = fraclo;
				difflo = 0;
				diffhi = 0;
			}
		}
		switch (rnd) {
		case ROUND_CHOP:
			frac = fraclo;
			break;
		case ROUND_MINUS_INF:
			if (f.t.sign)
				frac = frachi;
			else
				frac = fraclo;
			break;
		case ROUND_NORMAL:
#if 0
			/*
			 * Round to nearest.
			 */
			if (difflo < diffhi)
				frac = fraclo;
			else if (diffhi > difflo)
				frac = frachi;
			else if (fraclo & 1)
				frac = frachi;
			else
				frac = fraclo;
#else
			/*
			 * Round to zero.
			 */
			frac = fraclo;
#endif
			break;
		case ROUND_PLUS_INF:
			if (f.t.sign)
				frac = fraclo;
			else
				frac = frachi;
			break;
		}
	}

	if (f.t.sign) {
		if (frac > (1ULL << 63))
			*status |= FPCR_IOV | FPCR_INE;
		frac = -frac;
	} else {
		if (frac > (1ULL << 63) - 1)
			*status |= FPCR_IOV | FPCR_INE;
	}
	
	f.q = frac;
	return f;
}

fp_register_t
ieee_convert_S_Q(fp_register_t f, int rnd,
		 u_int64_t control, u_int64_t *status)
{
	f = ieee_convert_S_T(f, rnd, control, status);
	return ieee_convert_T_Q(f, rnd, control, status);
}

#ifndef _KERNEL

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

union value {
	double d;
	fp_register_t r;
};


static double
random_double()
{
	union value a;
	int exp;
	
	a.r.t.fraction = ((long long)random() & (1ULL << 20) - 1) << 32
		| random();
	exp = random() & 0x7ff;
#if 1
	if (exp == 0)
		exp = 1;	/* no denorms */
	else if (exp == 0x7ff)
		exp = 0x7fe;	/* no NaNs and INFs */
#endif

	a.r.t.exponent = exp;
	a.r.t.sign = random() & 1;
	return a.d;
}

static float
random_float()
{
	union value a;
	int exp;
	
	a.r.t.fraction = ((long)random() & (1ULL << 23) - 1) << 29;
	exp = random() & 0xff;
#if 1
	if (exp == 0)
		exp = 1;	/* no denorms */
	else if (exp == 0xff)
		exp = 0xfe;	/* no NaNs and INFs */
#endif

	/* map exponent from S to T format */
	if (exp == 255)
		a.r.t.exponent = 0x7ff;
	else if (exp & 0x80)
		a.r.t.exponent = 0x400 + (exp & 0x7f);
	else if (exp)
		a.r.t.exponent = 0x380 + exp;
	else
		a.r.t.exponent = 0;
	a.r.t.sign = random() & 1;

	return a.d;
}

/*
 * Ignore epsilon errors
 */
int
equal_T(union value a, union value b)
{
	if (isZERO(a.r) && isZERO(b.r))
		return 1;
	if (a.r.t.sign != b.r.t.sign)
		return 0;
	if (a.r.t.exponent != b.r.t.exponent)
		return 0;

	return a.r.t.fraction == b.r.t.fraction;
}

int
equal_S(union value a, union value b)
{
	int64_t epsilon = 1ULL << 29;

	if (isZERO(a.r) && isZERO(b.r))
		return 1;
	if (a.r.t.sign != b.r.t.sign)
		return 0;
	if (a.r.t.exponent != b.r.t.exponent)
		return 0;

	return ((a.r.t.fraction & ~(epsilon-1))
		== (b.r.t.fraction & ~(epsilon-1)));
}

#define ITER 1000000

static void
test_double_add()
{
	union value a, b, c, x;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.d = random_double();
		b.d = random_double();
		status = 0;
		c.r = ieee_add(a.r, b.r, T_FORMAT, ROUND_NORMAL,
			       0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r) || isDENORM(c.r))
			continue;
		x.d = a.d + b.d;
		if (!equal_T(c, x)) {
			printf("bad double add, %g + %g = %g (should be %g)\n",
			       a.d, b.d, c.d, x.d);
			c.r = ieee_add(a.r, b.r, T_FORMAT, ROUND_NORMAL,
				       0, &status);
		}
	}
}

static void
test_single_add()
{
	union value a, b, c, x, t;
	float xf;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
#if 0
		if (i == 0) {
			a.r.q = 0xb33acf292ca49700ULL;
			b.r.q = 0xcad3191058a693aeULL;
		}
#endif
		a.d = random_float();
		b.d = random_float();
		status = 0;
		c.r = ieee_add(a.r, b.r, S_FORMAT, ROUND_NORMAL,
			       0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r) || isDENORM(c.r))
			continue;
		xf = a.d + b.d;
		x.d = xf;
		t.r = ieee_convert_S_T(c.r, ROUND_NORMAL, 0, &status);
		if (!equal_S(t, x)) {
			printf("bad single add, %g + %g = %g (should be %g)\n",
			       a.d, b.d, t.d, x.d);
			c.r = ieee_add(a.r, b.r, S_FORMAT, ROUND_NORMAL,
				       0, &status);
		}
	}
}

static void
test_double_mul()
{
	union value a, b, c, x;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.d = random_double();
		b.d = random_double();
		status = 0;
		c.r = ieee_mul(a.r, b.r, T_FORMAT, ROUND_NORMAL,
			       0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r) || isDENORM(c.r))
			continue;
		x.d = a.d * b.d;
		if (!equal_T(c, x)) {
			printf("bad double mul, %g * %g = %g (should be %g)\n",
			       a.d, b.d, c.d, x.d);
			c.r = ieee_mul(a.r, b.r, T_FORMAT, ROUND_NORMAL,
				       0, &status);
		}
	}
}

static void
test_single_mul()
{
	union value a, b, c, x, t;
	float xf;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.d = random_double();
		b.d = random_double();
		status = 0;
		c.r = ieee_mul(a.r, b.r, S_FORMAT, ROUND_NORMAL,
			       0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r) || isDENORM(c.r))
			continue;
		xf = a.d * b.d;
		x.d = xf;
		t.r = ieee_convert_S_T(c.r, ROUND_NORMAL, 0, &status);
		if (!equal_S(t, x)) {
			printf("bad single mul, %g * %g = %g (should be %g)\n",
			       a.d, b.d, t.d, x.d);
			c.r = ieee_mul(a.r, b.r, T_FORMAT, ROUND_NORMAL,
				       0, &status);
		}
	}
}

static void
test_double_div()
{
	union value a, b, c, x;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.d = random_double();
		b.d = random_double();
		status = 0;
		c.r = ieee_div(a.r, b.r, T_FORMAT, ROUND_NORMAL,
			       0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r) || isDENORM(c.r))
			continue;
		x.d = a.d / b.d;
		if (!equal_T(c, x) && !isZERO(x.r)) {
			printf("bad double div, %g / %g = %g (should be %g)\n",
			       a.d, b.d, c.d, x.d);
			c.r = ieee_div(a.r, b.r, T_FORMAT, ROUND_NORMAL,
				       0, &status);
		}
	}
}

static void
test_single_div()
{
	union value a, b, c, x, t;
	float xf;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.d = random_double();
		b.d = random_double();
		status = 0;
		c.r = ieee_div(a.r, b.r, S_FORMAT, ROUND_NORMAL,
			       0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r) || isDENORM(c.r))
			continue;
		xf = a.d / b.d;
		x.d = xf;
		t.r = ieee_convert_S_T(c.r, ROUND_NORMAL, 0, &status);
		if (!equal_S(t, x)) {
			printf("bad single div, %g / %g = %g (should be %g)\n",
			       a.d, b.d, t.d, x.d);
			c.r = ieee_mul(a.r, b.r, T_FORMAT, ROUND_NORMAL,
				       0, &status);
		}
	}
}

static void
test_convert_int_to_double()
{
	union value a, c, x;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.r.q = (u_int64_t)random() << 32
			| random();
		status = 0;
		c.r = ieee_convert_Q_T(a.r, ROUND_NORMAL, 0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r))
			continue;
		x.d = (double) a.r.q;
		if (c.d != x.d) {
			printf("bad convert double, (double)%qx = %g (should be %g)\n",
			       a.r.q, c.d, x.d);
			c.r = ieee_convert_Q_T(a.r, ROUND_NORMAL, 0, &status);
		}
	}
}

static void
test_convert_int_to_single()
{
	union value a, c, x, t;
	float xf;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.r.q = (unsigned long long)random() << 32
			| random();
		status = 0;
		c.r = ieee_convert_Q_S(a.r, ROUND_NORMAL, 0, &status);
		/* ignore NaN and INF */
		if (isNaN(c.r) || isINF(c.r))
			continue;
		xf = (float) a.r.q;
		x.d = xf;
		t.r = ieee_convert_S_T(c.r, ROUND_NORMAL, 0, &status);
		if (t.d != x.d) {
			printf("bad convert single, (double)%qx = %g (should be %g)\n",
			       a.r.q, c.d, x.d);
			c.r = ieee_convert_Q_S(a.r, ROUND_NORMAL, 0, &status);
		}
	}
}

static void
test_convert_double_to_int()
{
	union value a, c;
	u_int64_t status = 0;
	int i;

	for (i = 0; i < ITER; i++) {
		a.d = random_double();
		status = 0;
		c.r = ieee_convert_T_Q(a.r, ROUND_NORMAL, 0, &status);
		if ((int)c.r.q != (int)a.d) {
			printf("bad convert double, (int)%g = %d (should be %d)\n",
			       a.d, (int)c.r.q, (int)a.d);
			c.r = ieee_convert_T_Q(a.r, ROUND_NORMAL, 0, &status);
		}
	}
}

int
main(int argc, char* argv[])
{
	srandom(0);

	test_double_div();
	test_single_div();
	test_double_add();
	test_single_add();
	test_double_mul();
	test_single_mul();
	test_convert_int_to_double();
	test_convert_int_to_single();
#if 0
	/* x86 generates SIGFPE on overflows. */
	test_convert_double_to_int();
#endif

	return 0;
}

#endif
