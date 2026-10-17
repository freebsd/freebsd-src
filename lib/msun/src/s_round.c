/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003, Steven G. Kargl
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

#include <float.h>
#include <stdint.h>

#include "math.h"
#include "math_private.h"

static const double huge = 1.0e300;

double
round(double x)
{
	uint64_t ix, mask, round_bit;
	int biased_e, shift;

	EXTRACT_WORD64(ix, x);
	biased_e = (int)((ix >> 52) & 0x7ff);

	/* Hot path: 1 <= |x| < 2**52. */
	if (biased_e > 1022 && biased_e < 1075) {
		shift = 1075 - biased_e;
		round_bit = 1ULL << (shift - 1);
		mask = ~((1ULL << shift) - 1);
		if ((ix & ~mask) == 0)
			return (x);
		if (huge + x > 0.0) {
			ix = (ix + round_bit) & mask;
			INSERT_WORD64(x, ix);
		}
		return (x);
	}

	/* |x| < 1: round to signed 0 or signed 1. */
	if (biased_e <= 1022) {
		if ((ix & 0x7fffffffffffffffULL) == 0)
			return (x);
		if (huge + x > 0.0) {
			ix = (ix & 0x8000000000000000ULL) |
			    (biased_e == 1022 ? 0x3ff0000000000000ULL : 0);
			INSERT_WORD64(x, ix);
		}
		return (x);
	}

	/* |x| >= 2**52 is already integral.  Propagate NaNs. */
	return (biased_e == 2047 ? x + x : x);
}

double
roundeven(double x)
{
	uint64_t ix, mask, round_bit;
	int biased_e, shift;

	EXTRACT_WORD64(ix, x);
	biased_e = (int)((ix >> 52) & 0x7ff);

	if (biased_e > 1022 && biased_e < 1075) {
		shift = 1075 - biased_e;
		round_bit = 1ULL << (shift - 1);
		mask = ~((1ULL << shift) - 1);
		if ((ix & ~mask) == 0)
			return (x);
		ix = (ix + round_bit - 1 + ((ix >> shift) & 1)) & mask;
		INSERT_WORD64(x, ix);
		return (x);
	}

	if (biased_e <= 1022) {
		if ((ix & 0x7fffffffffffffffULL) == 0)
			return (x);
		ix = (ix & 0x8000000000000000ULL) |
		    (biased_e == 1022 &&
		    (ix & 0xfffffffffffffULL) != 0 ?
		    0x3ff0000000000000ULL : 0);
		INSERT_WORD64(x, ix);
		return (x);
	}

	return (biased_e == 2047 ? x + x : x);
}

#if (LDBL_MANT_DIG == 53)
__weak_reference(round, roundl);
__weak_reference(roundeven, roundevenl);
#endif
