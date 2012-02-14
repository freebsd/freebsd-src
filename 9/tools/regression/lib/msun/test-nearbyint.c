/*-
 * Copyright (c) 2010 David Schultz <das@FreeBSD.org>
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
 */

/*
 * Tests for nearbyint{,f,l}()
 *
 * TODO:
 * - adapt tests for rint(3)
 * - tests for harder values (more mantissa bits than float)
 * - tests in other rounding modes
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <fenv.h>
#include <math.h>
#include <stdio.h>

#define	ALL_STD_EXCEPT	(FE_DIVBYZERO | FE_INEXACT | FE_INVALID | \
			 FE_OVERFLOW | FE_UNDERFLOW)

/*
 * Compare d1 and d2 using special rules: NaN == NaN and +0 != -0.
 * Fail an assertion if they differ.
 */
static int
fpequal(long double d1, long double d2)
{

	if (d1 != d2)
		return (isnan(d1) && isnan(d2));
	return (copysignl(1.0, d1) == copysignl(1.0, d2));
}

static void testit(int testnum, float in, float out)
{

    feclearexcept(ALL_STD_EXCEPT);
    assert(fpequal(out, nearbyintf(in)));
    assert(fpequal(-out, nearbyintf(-in)));
    assert(fetestexcept(ALL_STD_EXCEPT) == 0);

    assert(fpequal(out, nearbyint(in)));
    assert(fpequal(-out, nearbyint(-in)));
    assert(fetestexcept(ALL_STD_EXCEPT) == 0);

    assert(fpequal(out, nearbyintl(in)));
    assert(fpequal(-out, nearbyintl(-in)));
    assert(fetestexcept(ALL_STD_EXCEPT) == 0);

    printf("ok %d\t\t# nearbyint(%g)\n", testnum, in);
}

static const float tests[] = {
/* input	output (expected) */
    0.0,	0.0,
    0.5,	0.0,
    M_PI,	3,
    65536.5,	65536,
    INFINITY,	INFINITY,
    NAN,	NAN,
};

int
main(int argc, char *argv[])
{
	static const int ntests = sizeof(tests) / sizeof(tests[0]) / 2;
	int i;

	printf("1..%d\n", ntests);
	for (i = 0; i < ntests; i++)
		testit(i + 1, tests[i * 2], tests[i * 2 + 1]);

	return (0);
}
