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
 * See ../src/s_sinpi.c for implementation details.
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

#include "k_cospil.h"
#include "k_sinpil.h"

volatile static const double vzero = 0;

long double
sinpil(long double x)
{
	long double ai, ar, ax, hi, lo, s, xhi, xlo;

	ax = fabsl(x);

	if (ax < 1) {
		if (ax < 0.25) {
			if (ax < 0x1p-60) {
				if (x == 0)
					return (x);
				hi = (double)x;
				hi *= 0x1p113L;
				lo = x * 0x1p113L - hi;
				s = (pi_lo + pi_hi) * lo + pi_lo * lo +
				    pi_hi * hi;
				return (s * 0x1p-113L);
			}

			s = __kernel_sinpil(ax);
			return (x < 0 ? -s : s);
		}

		if (ax < 0.5)
			s = __kernel_cospil(0.5 - ax);
		else if (ax < 0.75)
			s = __kernel_cospil(ax - 0.5);
		else
			s = __kernel_sinpil(1 - ax);
		return (x < 0 ? -s : s);
	}

	if (ax < 0x1p112) {
		/* Split ax = ai + ar with 0 <= ar < 1. */
		FFLOORL128(ax, ai, ar);

		if (ar == 0) {
			s = 0;
		} else {
			if (ar < 0.5) {
				if (ar <= 0.25)
					s = __kernel_sinpil(ar);
				else
					s = __kernel_cospil(0.5 - ar);
			} else {
				if (ar < 0.75)
					s = __kernel_cospil(ar - 0.5);
				else
					s = __kernel_sinpil(1 - ar);
			}

			s = fmodl(ai, 2.L) == 0 ? s : -s;
		}
		return (x < 0 ? -s : s);
	}

	if (isinf(x) || isnan(x))
		return (vzero / vzero);

	/*
	 * |x| >= 0x1p112 is always an integer, so return +-0.
	 */
	return (copysignl(0, x));
}
