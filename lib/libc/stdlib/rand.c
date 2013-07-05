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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)rand.c	8.1 (Berkeley) 6/14/93";
#endif /* LIBC_SCCS and not lint */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "namespace.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <stdlib.h>
#include "un-namespace.h"

#ifdef TEST
#include <stdio.h>
#endif /* TEST */

static int
do_rand(unsigned long *ctx)
{
#ifdef  USE_WEAK_SEEDING
/*
 * Historic implementation compatibility.
 * The random sequences do not vary much with the seed,
 * even with overflowing.
 */
	return ((*ctx = *ctx * 1103515245 + 12345) % ((u_long)RAND_MAX + 1));
#else   /* !USE_WEAK_SEEDING */
/*
 * Compute x = (7^5 * x) mod (2^31 - 1)
 * without overflowing 31 bits:
 *      (2^31 - 1) = 127773 * (7^5) + 2836
 * From "Random number generators: good ones are hard to find",
 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
 * October 1988, p. 1195.
 */
	long hi, lo, x;

	/* Must be in [1, 0x7ffffffe] range at this point. */
	hi = *ctx / 127773;
	lo = *ctx % 127773;
	x = 16807 * lo - 2836 * hi;
	if (x < 0)
		x += 0x7fffffff;
	*ctx = x;
	/* Transform to [0, 0x7ffffffd] range. */
	return (x - 1);
#endif  /* !USE_WEAK_SEEDING */
}


int
rand_r(unsigned int *ctx)
{
	u_long val;
	int r;

#ifdef  USE_WEAK_SEEDING
	val = *ctx;
#else
	/* Transform to [1, 0x7ffffffe] range. */
	val = (*ctx % 0x7ffffffe) + 1;
#endif
	r = do_rand(&val);

#ifdef  USE_WEAK_SEEDING
	*ctx = (unsigned int)val;
#else
	*ctx = (unsigned int)(val - 1);
#endif
	return (r);
}


static u_long next =
#ifdef  USE_WEAK_SEEDING
    1;
#else
    2;
#endif

int
rand()
{
	return (do_rand(&next));
}

void
srand(seed)
u_int seed;
{
	next = seed;
#ifndef USE_WEAK_SEEDING
	/* Transform to [1, 0x7ffffffe] range. */
	next = (next % 0x7ffffffe) + 1;
#endif
}


/*
 * sranddev:
 *
 * Many programs choose the seed value in a totally predictable manner.
 * This often causes problems.  We seed the generator using pseudo-random
 * data from the kernel.
 */
void
sranddev()
{
	int mib[2];
	size_t len;

	len = sizeof(next);

	mib[0] = CTL_KERN;
	mib[1] = KERN_ARND;
	sysctl(mib, 2, (void *)&next, &len, NULL, 0);
#ifndef USE_WEAK_SEEDING
	/* Transform to [1, 0x7ffffffe] range. */
	next = (next % 0x7ffffffe) + 1;
#endif
}


#ifdef TEST

main()
{
    int i;
    unsigned myseed;

    printf("seeding rand with 0x19610910: \n");
    srand(0x19610910);

    printf("generating three pseudo-random numbers:\n");
    for (i = 0; i < 3; i++)
    {
	printf("next random number = %d\n", rand());
    }

    printf("generating the same sequence with rand_r:\n");
    myseed = 0x19610910;
    for (i = 0; i < 3; i++)
    {
	printf("next random number = %d\n", rand_r(&myseed));
    }

    return 0;
}

#endif /* TEST */

