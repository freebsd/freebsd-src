/*
 * Copyright (C) 2003 Sean Chittenden <seanc@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __RANDOMIZE_FD__
#define __RANDOMIZE_FD__

#define RANDOM_TYPE_UNSET 0
#define RANDOM_TYPE_LINES 1
#define RANDOM_TYPE_WORDS 2

/* The multiple instance single integer key */
struct rand_node {
	u_char *cp;
	u_int len;
	struct rand_node *next;
};

int randomize_fd(int fd, int type, int unique, double denom);

/*
 * Generates a random number uniformly in the range [0.0, 1.0).
 */
static inline double
random_unit_float(void)
{
	static const uint64_t denom = (1ull << 53);
	static const uint64_t mask = denom - 1;

	uint64_t rand64;

	/*
	 * arc4random_buf(...) in this use generates integer outputs in [0,
	 * UINT64_MAX].
	 *
	 * The double mantissa only has 53 bits, so we uniformly mask off the
	 * high 11 bits and then floating-point divide by 2^53 to achieve a
	 * result in [0, 1).
	 *
	 * We are not allowed to emit 1.0, so denom must be one greater than
	 * the possible range of the preceeding step.
	 */
	arc4random_buf(&rand64, sizeof(rand64));
	rand64 &= mask;
	return ((double)rand64 / denom);
}

/*
 * Returns true with probability 1 / denom (a floating point number >= 1).
 * Otherwise, returns false.
 */
static inline bool
random_uniform_denom(double denom)
{
	return ((uint64_t)(denom * random_unit_float()) == 0);
}
#endif
