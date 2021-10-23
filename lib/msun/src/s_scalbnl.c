/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 *
 * SPDX-License-Identifier: MIT
 *
 * Please see https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
 * for all contributors to musl.
 */
#include <math.h>
#include <float.h>
#include "math_private.h"
#include "fpmath.h"
/*
 * scalbnl (long double x, int n)
 * scalbnl(x,n) returns x* 2**n  computed by  exponent
 * manipulation rather than by actually performing an
 * exponentiation or a multiplication.
 */
#if (LDBL_MANT_DIG == 64 || LDBL_MANT_DIG == 113) && LDBL_MAX_EXP == 16384
long double scalbnl(long double x, int n)
{
	union IEEEl2bits u;

	if (n > 16383) {
		x *= 0x1p16383L;
		n -= 16383;
		if (n > 16383) {
			x *= 0x1p16383L;
			n -= 16383;
			if (n > 16383)
				n = 16383;
		}
	} else if (n < -16382) {
		x *= 0x1p-16382L * 0x1p113L;
		n += 16382 - 113;
		if (n < -16382) {
			x *= 0x1p-16382L * 0x1p113L;
			n += 16382 - 113;
			if (n < -16382)
				n = -16382;
		}
	}
	u.e = 1.0;
	u.xbits.expsign = 0x3fff + n;
	return x * u.e;
}
__strong_reference(scalbnl, ldexpl);
#endif

