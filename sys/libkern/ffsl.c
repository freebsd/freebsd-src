/*-
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

#include <sys/cdefs.h>
#include <sys/types.h>

#ifdef UNIT_TEST
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#else
__FBSDID("$FreeBSD$");
#include <sys/libkern.h>
#endif


/*
 * Find First Set bit starting at least significant and 
 * numbered starting from one. Returns zero if no bits are set.
 *
 * Portable implementation with no assumption about sizeof(x). We use
 * bisect for an implementation which should be efficient on most
 * architectures with approx. constant runtime proprotional to
 * log(sizeof(x)).
 */
int
ffsl(long x)
{
	unsigned long mask  = (unsigned long) -1;
	unsigned int  shift = sizeof(x) * 8;
	unsigned int  r     = 1;
	if (x == 0)
		return (0);
	/* 
	 * Invariants: 
	 * 0) At least one bit set in x.
	 * 1) first set bit is in bottom 'shift' bits of x,
	 *    with corresponding mask in 'mask'. 
	 * 2) Answer is r + first set bit in x (when numbered from zero).
	 * Stopping condition: only one bit left.
	 */
	while(shift > 1)
	{
		/* Set mask to half remaining bits */
		shift  /= 2;
		mask  >>= shift;
		//printf("x=%016lx mask=%016lx shift=%u r=%u\n", x, mask, shift, r);
		if ((x & mask) == 0)
		{
			/* 
			 * First set bit must be in top half of
			 * remaining bits, so increment return value
			 * and shift down to maintain invariants 
			 */
			x >>= shift;
			r +=  shift;
		}
	}
	return (r);
}

#ifdef UNIT_TEST

static int
old_ffsl(long mask)
{
  int bit;
  if (mask == 0)
    return (0);
  for (bit = 1; !(mask & 1); bit++)
    mask = (unsigned long) mask >> 1;
  return (bit);
}

#if 1
int main(void)
{
  for(long x = 0; x < 0xffffffff; x++)
    {
      assert(ffsl(x)  == old_ffsl(x));
      if ((x & 0xfffffff) == 0)
	printf(".\n");
    }
  printf("SUCCESS!\n");
}
#else
int main(void)
{
  printf("%d %d\n", 0, ffsl(0));
  assert(ffsl(0) == 0);
  for(int x = 0; x < 8*(sizeof(long)); x++)
    {
      int r = ffsl((random() << x) | (1l << x));
      printf("%d %d\n", x + 1, r);
      assert(r == x + 1);
    }
  printf("SUCCESS!\n");
}
#endif
#endif
