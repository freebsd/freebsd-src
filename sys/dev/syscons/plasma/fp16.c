/*-
 * Copyright (c) 2015 Dag-Erling Smørgrav
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
 * $FreeBSD$
 */

#ifdef _KERNEL
#include <sys/libkern.h>
#else
#include <stdio.h>
#include <strings.h>
#endif

#include "fp16.h"

/*
 * Compute the quare root of x, using Newton's method with 2^(log2(x)/2)
 * as the initial estimate.
 */
fp16_t
fp16_sqrt(fp16_t x)
{
	fp16_t y, delta;
	signed int log2x;

	/* special case */
	if (x == 0)
		return (0);

	/* shift toward 0 by half the logarithm */
	log2x = flsl(x) - 1;
	if (log2x >= 16) {
		y = x >> (log2x - 16) / 2;
	} else {
#if 0
		y = x << (16 - log2x) / 2;
#else
		/* XXX for now, return 0 for anything < 1 */
		return (0);
#endif
	}
	while (y > 0) {
		/* delta = y^2 / 2y */
		delta = fp16_div(fp16_sub(fp16_mul(y, y), x), y * 2);
		if (delta == 0)
			break;
		y = fp16_sub(y, delta);
	}
	return (y);
}

static fp16_t fp16_cos_table[256] = {
	65536,	65534,	65531,	65524,	65516,	65505,	65491,	65475,
	65457,	65436,	65412,	65386,	65358,	65327,	65294,	65258,
	65220,	65179,	65136,	65091,	65043,	64992,	64939,	64884,
	64826,	64766,	64703,	64638,	64571,	64501,	64428,	64353,
	64276,	64197,	64115,	64030,	63943,	63854,	63762,	63668,
	63571,	63473,	63371,	63268,	63162,	63053,	62942,	62829,
	62714,	62596,	62475,	62353,	62228,	62100,	61971,	61839,
	61705,	61568,	61429,	61288,	61144,	60998,	60850,	60700,
	60547,	60392,	60235,	60075,	59913,	59749,	59583,	59414,
	59243,	59070,	58895,	58718,	58538,	58356,	58172,	57986,
	57797,	57606,	57414,	57219,	57022,	56822,	56621,	56417,
	56212,	56004,	55794,	55582,	55368,	55152,	54933,	54713,
	54491,	54266,	54040,	53811,	53581,	53348,	53114,	52877,
	52639,	52398,	52155,	51911,	51665,	51416,	51166,	50914,
	50660,	50403,	50146,	49886,	49624,	49360,	49095,	48828,
	48558,	48288,	48015,	47740,	47464,	47186,	46906,	46624,
	46340,	46055,	45768,	45480,	45189,	44897,	44603,	44308,
	44011,	43712,	43412,	43110,	42806,	42501,	42194,	41885,
	41575,	41263,	40950,	40636,	40319,	40002,	39682,	39362,
	39039,	38716,	38390,	38064,	37736,	37406,	37075,	36743,
	36409,	36074,	35738,	35400,	35061,	34721,	34379,	34036,
	33692,	33346,	32999,	32651,	32302,	31952,	31600,	31247,
	30893,	30538,	30181,	29824,	29465,	29105,	28745,	28383,
	28020,	27656,	27291,	26925,	26557,	26189,	25820,	25450,
	25079,	24707,	24334,	23960,	23586,	23210,	22833,	22456,
	22078,	21699,	21319,	20938,	20557,	20175,	19792,	19408,
	19024,	18638,	18253,	17866,	17479,	17091,	16702,	16313,
	15923,	15533,	15142,	14751,	14359,	13966,	13573,	13179,
	12785,	12390,	11995,	11600,	11204,	10807,	10410,	10013,
	 9616,	 9218,	 8819,	 8421,	 8022,	 7623,	 7223,	 6823,
	 6423,	 6023,	 5622,	 5222,	 4821,	 4420,	 4018,	 3617,
	 3215,	 2814,	 2412,	 2010,	 1608,	 1206,	  804,	  402,
};

/*
 * Compute the cosine of theta.
 */
fp16_t
fp16_cos(fp16_t theta)
{
	unsigned int i;

	i = 1024 * (theta % FP16_2PI) / FP16_2PI;
	switch (i / 256) {
	case 0:
		return (fp16_cos_table[i % 256]);
	case 1:
		return (-fp16_cos_table[255 - i % 256]);
	case 2:
		return (-fp16_cos_table[i % 256]);
	case 3:
		return (fp16_cos_table[255 - i % 256]);
	default:
		/* inconceivable! */
		return (0);
	}
}
