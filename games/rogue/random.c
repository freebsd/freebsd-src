/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Timothy C. Stoehr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)random.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif /* not lint */

#include "rogue.h"

#if 0
/*
 * random.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

static long rntb[32] = {
	         3, 0x9a319039, 0x32d9c024, 0x9b663182, 0x5da1f342,
	0xde3b81e0, 0xdf0a6fb5, 0xf103bc02, 0x48f340fb, 0x7449e56b,
	0xbeb1dbb0, 0xab5c5918, 0x946554fd, 0x8c2e680f, 0xeb3d799f,
	0xb11ee0b7, 0x2d436b86, 0xda672e2a, 0x1588ca88, 0xe369735d,
	0x904f35f7, 0xd7158fd6, 0x6fa6f051, 0x616e6b96, 0xac94efdc,
	0x36413f93, 0xc622c298, 0xf5a42ab8, 0x8a88d77b, 0xf5ad9d0e,
	0x8999220b, 0x27fb47b9
};

static long *fptr = &rntb[4];
static long *rptr = &rntb[1];
static long *state = &rntb[1];
static int rand_type = 3;
static int rand_deg = 31;
static int rand_sep = 3;
static long *end_ptr = &rntb[32];

srrandom(x)
int x;
{
	int i;

	state[0] = (long) x;
	if (rand_type != 0) {
		for (i = 1; i < rand_deg; i++) {
			state[i] = 1103515245 * state[i - 1] + 12345;
		}
		fptr = &state[rand_sep];
		rptr = &state[0];
		for (i = 0; i < 10 * rand_deg; i++) {
			(void) rrandom();
		}
	}
}

long
rrandom()
{
	long i;

	if (rand_type == 0) {
		i = state[0] = (state[0]*1103515245 + 12345) & 0x7fffffff;
	} else {
		*fptr += *rptr;
		i = (*fptr >> 1) & 0x7fffffff;
		if (++fptr >= end_ptr) {
			fptr = state;
			++rptr;
		} else {
			if (++rptr >= end_ptr) {
				rptr = state;
			}
		}
	}
	return(i);
}
#endif

get_rand(x, y)
int x, y;
{
	int r, t;
	long lr;

	if (x > y) {
		t = y;
		y = x;
		x = t;
	}
	lr = rrandom();
	lr &= (long) 0x00003fff;
	r = (int) lr;
	r = (r % ((y - x) + 1)) + x;
	return(r);
}

rand_percent(percentage)
int percentage;
{
	return(get_rand(1, 100) <= percentage);
}

coin_toss()
{

	return(((rrandom() & 01) ? 1 : 0));
}
