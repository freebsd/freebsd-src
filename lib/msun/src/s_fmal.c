/*-
 * Copyright (c) 2005-2011 David Schultz <das@FreeBSD.ORG>
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
__FBSDID("$FreeBSD$");

#include <fenv.h>
#include <float.h>
#include <math.h>

/*
 * A struct dd represents a floating-point number with twice the precision
 * of a long double.  We maintain the invariant that "hi" stores the high-order
 * bits of the result.
 */
struct dd {
	long double hi;
	long double lo;
};

/*
 * Compute a+b exactly, returning the exact result in a struct dd.  We assume
 * that both a and b are finite, but make no assumptions about their relative
 * magnitudes.
 */
static inline struct dd
dd_add(long double a, long double b)
{
	struct dd ret;
	long double s;

	ret.hi = a + b;
	s = ret.hi - a;
	ret.lo = (a - (ret.hi - s)) + (b - s);
	return (ret);
}

/*
 * Compute a*b exactly, returning the exact result in a struct dd.  We assume
 * that both a and b are normalized, so no underflow or overflow will occur.
 * The current rounding mode must be round-to-nearest.
 */
static inline struct dd
dd_mul(long double a, long double b)
{
#if LDBL_MANT_DIG == 64
	static const long double split = 0x1p32L + 1.0;
#elif LDBL_MANT_DIG == 113
	static const long double split = 0x1p57L + 1.0;
#endif
	struct dd ret;
	long double ha, hb, la, lb, p, q;

	p = a * split;
	ha = a - p;
	ha += p;
	la = a - ha;

	p = b * split;
	hb = b - p;
	hb += p;
	lb = b - hb;

	p = ha * hb;
	q = ha * lb + la * hb;

	ret.hi = p + q;
	ret.lo = p - ret.hi + q + la * lb;
	return (ret);
}

/*
 * Fused multiply-add: Compute x * y + z with a single rounding error.
 *
 * We use scaling to avoid overflow/underflow, along with the
 * canonical precision-doubling technique adapted from:
 *
 *	Dekker, T.  A Floating-Point Technique for Extending the
 *	Available Precision.  Numer. Math. 18, 224-242 (1971).
 */
long double
fmal(long double x, long double y, long double z)
{
	long double xs, ys, zs;
	struct dd xy, r, r2;
	long double p;
	long double s;
	int oround;
	int ex, ey, ez;
	int spread;

	/*
	 * Handle special cases. The order of operations and the particular
	 * return values here are crucial in handling special cases involving
	 * infinities, NaNs, overflows, and signed zeroes correctly.
	 */
	if (x == 0.0 || y == 0.0)
		return (x * y + z);
	if (z == 0.0)
		return (x * y);
	if (!isfinite(x) || !isfinite(y))
		return (x * y + z);
	if (!isfinite(z))
		return (z);

	xs = frexpl(x, &ex);
	ys = frexpl(y, &ey);
	zs = frexpl(z, &ez);
	oround = fegetround();
	spread = ex + ey - ez;

	/*
	 * If x * y and z are many orders of magnitude apart, the scaling
	 * will overflow, so we handle these cases specially.  Rounding
	 * modes other than FE_TONEAREST are painful.
	 */
	if (spread > LDBL_MANT_DIG * 2) {
		fenv_t env;
		feraiseexcept(FE_INEXACT);
		switch(oround) {
		case FE_TONEAREST:
			return (x * y);
		case FE_TOWARDZERO:
			if (x > 0.0 ^ y < 0.0 ^ z < 0.0)
				return (x * y);
			feholdexcept(&env);
			s = x * y;
			if (!fetestexcept(FE_INEXACT))
				s = nextafterl(s, 0);
			feupdateenv(&env);
			return (s);
		case FE_DOWNWARD:
			if (z > 0.0)
				return (x * y);
			feholdexcept(&env);
			s = x * y;
			if (!fetestexcept(FE_INEXACT))
				s = nextafterl(s, -INFINITY);
			feupdateenv(&env);
			return (s);
		default:	/* FE_UPWARD */
			if (z < 0.0)
				return (x * y);
			feholdexcept(&env);
			s = x * y;
			if (!fetestexcept(FE_INEXACT))
				s = nextafterl(s, INFINITY);
			feupdateenv(&env);
			return (s);
		}
	}
	if (spread < -LDBL_MANT_DIG) {
		feraiseexcept(FE_INEXACT);
		if (!isnormal(z))
			feraiseexcept(FE_UNDERFLOW);
		switch (oround) {
		case FE_TONEAREST:
			return (z);
		case FE_TOWARDZERO:
			if (x > 0.0 ^ y < 0.0 ^ z < 0.0)
				return (z);
			else
				return (nextafterl(z, 0));
		case FE_DOWNWARD:
			if (x > 0.0 ^ y < 0.0)
				return (z);
			else
				return (nextafterl(z, -INFINITY));
		default:	/* FE_UPWARD */
			if (x > 0.0 ^ y < 0.0)
				return (nextafterl(z, INFINITY));
			else
				return (z);
		}
	}

	fesetround(FE_TONEAREST);

	xy = dd_mul(xs, ys);
	zs = ldexpl(zs, -spread);
	r = dd_add(xy.hi, zs);
	r.lo += xy.lo;

	spread = ex + ey;
	if (spread + ilogbl(r.hi) > -16383) {
		fesetround(oround);
		r.hi = r.hi + r.lo;
	} else {
		/*
		 * The result is subnormal, so we round before scaling to
		 * avoid double rounding.
		 */
		p = ldexpl(copysignl(0x1p-16382L, r.hi), -spread);
		r2 = dd_add(r.hi, p);
		r2.lo += r.lo;
		fesetround(oround);
		r.hi = (r2.hi + r2.lo) - p;
	}
	return (ldexpl(r.hi, spread));
}
