/*-
 * Copyright (c) 2010-2011
 * 	Swinburne University of Technology, Melbourne, Australia.
 * All rights reserved.
 *
 * This software was developed at the Centre for Advanced Internet
 * Architectures, Swinburne University of Technology, by Sebastian Zander, made
 * possible in part by a gift from The Cisco University Research Program Fund, a
 * corporate advised fund of Silicon Valley Community Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef _NETINET_IPFW_DIFFUSE_COMMON_H_
#define _NETINET_IPFW_DIFFUSE_COMMON_H_

/* MSVC does not support designated initializers so we need this ugly macro. */
#ifdef _WIN32
#define	_FI(fld)
#else
#define	_FI(fld) fld
#endif

/* Feature or classifier instance data. */
struct di_cdata {
	void	*conf;	/* Instance configuration ptr. */
};

/* Flow data. */
struct di_fdata {
	void	*data;	/* Work data ptr. */
	int32_t	*stats;	/* Stats ptr. */
};

/*
 * Fast fixed point division with rounding for dividing by a number of 2.
 * a is the divident and b is the power of the divisor.
 */
static inline uint64_t
fixp_div(uint64_t a, int b)
{
	uint64_t q, r;

	if (b <= 0)
		return (a);

	q = a >> b;
	r = a & (b - 1);
	if ((r << 1) >= ((uint64_t)1 << b))
		return (q + 1);
	else
		return (q);
}

static inline uint32_t
fixp_sqrt(uint64_t x)
{
	uint64_t rem_hi, rem_lo, test_div;
	uint32_t root;
	int count;

	rem_hi = 0;
	rem_lo = x;
	root = 0;
	count = 31;

	do {
		rem_hi = (rem_hi << 2) | (rem_lo >> 62);
		rem_lo <<= 2; /* Get 2 bits of arg. */
		root <<= 1; /* Get ready for the next bit in the root. */
		test_div = (root << 1) + 1; /* Test radical. */
		if (rem_hi >= test_div) {
			rem_hi -= test_div;
			root++;
		}
	} while (count-- != 0);

	return (root);
}

/* Similar to timevalsub, but ensures the timeval returned will be >= 0. */
static inline struct timeval
tv_sub0(struct timeval *num, struct timeval *sub)
{
	struct timeval rv;

	rv.tv_sec = num->tv_sec - sub->tv_sec;
	rv.tv_usec = num->tv_usec - sub->tv_usec;

	if (rv.tv_usec < 0) {
		rv.tv_usec += 1000000;
		rv.tv_sec--;
	}
	if (rv.tv_sec < 0) {
		rv.tv_sec = 0;
		rv.tv_usec = 0;
	}

	return (rv);
}

#endif /* _NETINET_IPFW_DIFFUSE_COMMON_H_ */
