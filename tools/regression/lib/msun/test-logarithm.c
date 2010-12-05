/*-
 * Copyright (c) 2008-2010 David Schultz <das@FreeBSD.org>
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
 * Tests for corner cases in log*().
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#ifdef __i386__
#include <ieeefp.h>
#endif

#define	ALL_STD_EXCEPT	(FE_DIVBYZERO | FE_INEXACT | FE_INVALID | \
			 FE_OVERFLOW | FE_UNDERFLOW)

#pragma STDC FENV_ACCESS ON

/*
 * Test that a function returns the correct value and sets the
 * exception flags correctly. The exceptmask specifies which
 * exceptions we should check. We need to be lenient for several
 * reasoons, but mainly because on some architectures it's impossible
 * to raise FE_OVERFLOW without raising FE_INEXACT.
 *
 * These are macros instead of functions so that assert provides more
 * meaningful error messages.
 *
 * XXX The volatile here is to avoid gcc's bogus constant folding and work
 *     around the lack of support for the FENV_ACCESS pragma.
 */
#define	test(func, x, result, exceptmask, excepts)	do {		\
	volatile long double _d = x;					\
	assert(feclearexcept(FE_ALL_EXCEPT) == 0);			\
	assert(fpequal((func)(_d), (result)));				 \
	assert(((func), fetestexcept(exceptmask) == (excepts)));	\
} while (0)

/* Test all the functions that compute log(x). */
#define	testall0(x, result, exceptmask, excepts)	do {		\
	test(log, x, result, exceptmask, excepts);			\
	test(logf, x, result, exceptmask, excepts);			\
	test(log2, x, result, exceptmask, excepts);			\
	test(log2f, x, result, exceptmask, excepts);			\
	test(log10, x, result, exceptmask, excepts);			\
	test(log10f, x, result, exceptmask, excepts);			\
} while (0)

/* Test all the functions that compute log(1+x). */
#define	testall1(x, result, exceptmask, excepts)	do {		\
	test(log1p, x, result, exceptmask, excepts);			\
	test(log1pf, x, result, exceptmask, excepts);			\
} while (0)

/*
 * Determine whether x and y are equal, with two special rules:
 *	+0.0 != -0.0
 *	 NaN == NaN
 */
int
fpequal(long double x, long double y)
{
	return ((x == y && signbit(x) == signbit(y)) || isnan(x) && isnan(y));
}

void
run_generic_tests(void)
{

	/* exp(1) == 0, no exceptions raised */
	testall0(1.0, 0.0, ALL_STD_EXCEPT, 0);
	testall1(0.0, 0.0, ALL_STD_EXCEPT, 0);
	testall1(-0.0, -0.0, ALL_STD_EXCEPT, 0);

	/* log(NaN) == NaN, no exceptions raised */
	testall0(NAN, NAN, ALL_STD_EXCEPT, 0);
	testall1(NAN, NAN, ALL_STD_EXCEPT, 0);

	/* log(Inf) == Inf, no exceptions raised */
	testall0(INFINITY, INFINITY, ALL_STD_EXCEPT, 0);
	testall1(INFINITY, INFINITY, ALL_STD_EXCEPT, 0);

	/* log(x) == NaN for x < 0, invalid exception raised */
	testall0(-INFINITY, NAN, ALL_STD_EXCEPT, FE_INVALID);
	testall1(-INFINITY, NAN, ALL_STD_EXCEPT, FE_INVALID);
	testall0(-1.0, NAN, ALL_STD_EXCEPT, FE_INVALID);
	testall1(-1.5, NAN, ALL_STD_EXCEPT, FE_INVALID);

	/* log(0) == -Inf, divide-by-zero exception */
	testall0(0.0, -INFINITY, ALL_STD_EXCEPT & ~FE_INEXACT, FE_DIVBYZERO);
	testall0(-0.0, -INFINITY, ALL_STD_EXCEPT & ~FE_INEXACT, FE_DIVBYZERO);
	testall1(-1.0, -INFINITY, ALL_STD_EXCEPT & ~FE_INEXACT, FE_DIVBYZERO);
}

void
run_log2_tests(void)
{
	int i;

	/*
	 * We should insist that log2() return exactly the correct
	 * result and not raise an inexact exception for powers of 2.
	 */
	feclearexcept(FE_ALL_EXCEPT);
	for (i = FLT_MIN_EXP - FLT_MANT_DIG; i < FLT_MAX_EXP; i++) {
		assert(log2f(ldexpf(1.0, i)) == i);
		assert(fetestexcept(ALL_STD_EXCEPT) == 0);
	}
	for (i = DBL_MIN_EXP - DBL_MANT_DIG; i < DBL_MAX_EXP; i++) {
		assert(log2(ldexp(1.0, i)) == i);
		assert(fetestexcept(ALL_STD_EXCEPT) == 0);
	}
}

int
main(int argc, char *argv[])
{

	printf("1..2\n");

	run_generic_tests();
	printf("ok 1 - logarithm\n");

	run_log2_tests();
	printf("ok 2 - logarithm\n");

	return (0);
}
