/*-
 * Copyright (c) 2017-2023 Steven G. Kargl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * See ../src/s_tanpi.c for implementation details.
 */

#include "fpmath.h"
#include "math.h"
#include "math_private.h"

/*
 * pi_hi contains the leading 56 bits of a 169 bit approximation for pi.
 */
static const long double
pi_hi = 3.14159265358979322702026593105983920e+00L,
pi_lo = 1.14423774522196636802434264184180742e-17L;

static inline long double
__kernel_tanpil(long double x)
{
	long double hi, lo, t;

	if (x < 0.25) {
		hi = (double)x;
		lo = x - hi;
		lo = lo * (pi_lo + pi_hi) + hi * pi_lo;
		hi *= pi_hi;
		_2sumF(hi, lo);
		t = __kernel_tanl(hi, lo, -1);
	} else if (x > 0.25) {
		x = 0.5 - x;
		hi = (double)x;
		lo = x - hi;
		lo = lo * (pi_lo + pi_hi) + hi * pi_lo;
		hi *= pi_hi;
		_2sumF(hi, lo);
		t = - __kernel_tanl(hi, lo, 1);
	} else
		t = 1;

	return (t);
}

volatile static const double vzero = 0;

long double
tanpil(long double x)
{
	long double ai, ar, ax, hi, lo, t;
	double odd;

	ax = fabsl(x);

	if (ax < 1) {
		if (ax < 0.5) {
			if (ax < 0x1p-60) {
				if (x == 0)
					return (x);
				hi = (double)x;
				hi *= 0x1p113L;
				lo = x * 0x1p113L - hi;
				t = (pi_lo + pi_hi) * lo + pi_lo * lo +
				    pi_hi * hi;
				return (t * 0x1p-113L);
			}
			t = __kernel_tanpil(ax);
		} else if (ax == 0.5)
			t = 1 / vzero;
		else
			t = -__kernel_tanpil(1 - ax);
		return (x < 0 ? -t : t);
	}

	if (ax < 0x1p112) {
		/* Split ax = ai + ar with 0 <= ar < 1. */
		FFLOORL128(ax, ai, ar);
		odd = fmodl(ai, 2.L) == 0 ? 1 : -1;
		if (ar < 0.5)
			t = ar == 0 ? copysign(0., odd) : __kernel_tanpil(ar);
		else if (ar == 0.5)
			t = odd / vzero;
		else
			t = -__kernel_tanpil(1 - ar);
		return (x < 0 ? -t : t);
	}

	/* x = +-inf or nan. */
	if (isinf(x) || isnan(x))
		return (vzero / vzero);

	/*
	 * For 0x1p112 <= |x| < 0x1p113 need to determine if x is an even
	 * or odd integer to set t = +0 or -0.
	 * For |x| >= 0x1p113, it is always an even integer, so t = 0.
	 */
	t = fmodl(ax,2.L) == 0  ? 0 : copysign(0., -1.);
	return (copysignl(t, x));
}
