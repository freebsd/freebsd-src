/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Posix rand_r function added May 1999 by Wes Peters <wes@softweyr.com>.
 */

#include "namespace.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>
#include "un-namespace.h"

#include "libc_private.h"
#include "random.h"

/*
 * Implement rand(3), the standard C PRNG API, using the non-standard but
 * higher quality random(3) implementation and the same size 128-byte state
 * LFSR as the random(3) default.
 *
 * It turns out there are portable applications that want a PRNG but are too
 * lazy to use better-but-nonstandard interfaces like random(3), when
 * available, and too lazy to import higher-quality and faster PRNGs into their
 * codebase (such as any of SFC, JSF, 128-bit LCGs, PCG, or Splitmix64).
 *
 * Since we're stuck with rand(3) due to the C standard, we can at least have
 * it produce a relatively good PRNG sequence using our existing random(3)
 * LFSR.  The random(3) design is not particularly fast nor compact, but it has
 * the advantage of being the one already in the tree.
 */
static struct __random_state *rand3_state;
static pthread_once_t rand3_state_once = PTHREAD_ONCE_INIT;

static void
initialize_rand3(void)
{
	int error;

	rand3_state = allocatestate(TYPE_3);
	error = initstate_r(rand3_state, 1, rand3_state->rst_randtbl, BREAK_3);
	assert(error == 0);
}

int
rand(void)
{
	_once(&rand3_state_once, initialize_rand3);
	return ((int)random_r(rand3_state));
}

void
srand(unsigned seed)
{
	_once(&rand3_state_once, initialize_rand3);
	srandom_r(rand3_state, seed);
}

/*
 * FreeBSD 12 and prior compatibility implementation of rand(3).
 */
static int
do_rand(unsigned long *ctx)
{
/*
 * Compute x = (7^5 * x) mod (2^31 - 1)
 * without overflowing 31 bits:
 *      (2^31 - 1) = 127773 * (7^5) + 2836
 * From "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 */
	long hi, lo, x;

	/* Transform to [1, 0x7ffffffe] range. */
	x = (*ctx % 0x7ffffffe) + 1;
	hi = x / 127773;
	lo = x % 127773;
	x = 16807 * lo - 2836 * hi;
	if (x < 0)
		x += 0x7fffffff;
	/* Transform to [0, 0x7ffffffd] range. */
	x--;
	*ctx = x;
	return (x);
}

/*
 * Can't fix this garbage; too little state.
 */
int
rand_r(unsigned *ctx)
{
	u_long val;
	int r;

	val = *ctx;
	r = do_rand(&val);
	*ctx = (unsigned)val;
	return (r);
}

static u_long next = 1;

int __rand_fbsd12(void);
int
__rand_fbsd12(void)
{
	return (do_rand(&next));
}
__sym_compat(rand, __rand_fbsd12, FBSD_1.0);

void __srand_fbsd12(unsigned seed);
void
__srand_fbsd12(unsigned seed)
{
	next = seed;
}
__sym_compat(srand, __srand_fbsd12, FBSD_1.0);

void __sranddev_fbsd12(void);
void
__sranddev_fbsd12(void)
{
	static bool warned = false;

	if (!warned) {
		syslog(LOG_DEBUG, "Deprecated function sranddev() called");
		warned = true;
	}
}
__sym_compat(sranddev, __sranddev_fbsd12, FBSD_1.0);
