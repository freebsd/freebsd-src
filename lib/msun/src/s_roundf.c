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

#include "math.h"
#include "math_private.h"

float
roundf(float x)
{
	uint32_t hx, mask, round_bit;
	int biased_e, shift;

	GET_FLOAT_WORD(hx, x);
	biased_e = (int)((hx >> 23) & 0xff);

	/* Hot path: 1 <= |x| < 2**23. */
	if (biased_e > 126 && biased_e < 150) {
		shift = 150 - biased_e;
		round_bit = 1u << (shift - 1);
		mask = ~((1u << shift) - 1);
		hx = (hx + round_bit) & mask;
		SET_FLOAT_WORD(x, hx);
		return (x);
	}

	/* |x| < 1: round to signed 0 or signed 1. */
	if (biased_e <= 126) {
		hx = (hx & 0x80000000u) | (biased_e == 126 ? 0x3f800000u : 0u);
		SET_FLOAT_WORD(x, hx);
		return (x);
	}

	/* |x| >= 2**23 is already integral.  Propagate NaNs. */
	return (biased_e == 255 ? x + x : x);
}
