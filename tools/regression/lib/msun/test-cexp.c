/*-
 * Copyright (c) 2008-2011 David Schultz <das@FreeBSD.org>
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
 * Tests for corner cases in cexp*().
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <complex.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

#define	ALL_STD_EXCEPT	(FE_DIVBYZERO | FE_INEXACT | FE_INVALID | \
			 FE_OVERFLOW | FE_UNDERFLOW)
#define	FLT_ULP()	ldexpl(1.0, 1 - FLT_MANT_DIG)
#define	DBL_ULP()	ldexpl(1.0, 1 - DBL_MANT_DIG)
#define	LDBL_ULP()	ldexpl(1.0, 1 - LDBL_MANT_DIG)

#define	N(i)	(sizeof(i) / sizeof((i)[0]))

#pragma STDC FENV_ACCESS	ON
#pragma	STDC CX_LIMITED_RANGE	OFF

/*
 * XXX gcc implements complex multiplication incorrectly. In
 * particular, it implements it as if the CX_LIMITED_RANGE pragma
 * were ON. Consequently, we need this function to form numbers
 * such as x + INFINITY * I, since gcc evalutes INFINITY * I as
 * NaN + INFINITY * I.
 */
static inline long double complex
cpackl(long double x, long double y)
{
	long double complex z;

	__real__ z = x;
	__imag__ z = y;
	return (z);
}

/*
 * Test that a function returns the correct value and sets the
 * exception flags correctly. The exceptmask specifies which
 * exceptions we should check. We need to be lenient for several
 * reasons, but mainly because on some architectures it's impossible
 * to raise FE_OVERFLOW without raising FE_INEXACT. In some cases,
 * whether cexp() raises an invalid exception is unspecified.
 *
 * These are macros instead of functions so that assert provides more
 * meaningful error messages.
 *
 * XXX The volatile here is to avoid gcc's bogus constant folding and work
 *     around the lack of support for the FENV_ACCESS pragma.
 */
#define	test(func, z, result, exceptmask, excepts, checksign)	do {	\
	volatile long double complex _d = z;				\
	assert(feclearexcept(FE_ALL_EXCEPT) == 0);			\
	assert(cfpequal((func)(_d), (result), (checksign)));		\
	assert(((func), fetestexcept(exceptmask) == (excepts)));	\
} while (0)

/* Test within a given tolerance. */
#define	test_tol(func, z, result, tol)				do {	\
	volatile long double complex _d = z;				\
	assert(cfpequal_tol((func)(_d), (result), (tol)));		\
} while (0)

/* Test all the functions that compute cexp(x). */
#define	testall(x, result, exceptmask, excepts, checksign)	do {	\
	test(cexp, x, result, exceptmask, excepts, checksign);		\
	test(cexpf, x, result, exceptmask, excepts, checksign);		\
} while (0)

/*
 * Test all the functions that compute cexp(x), within a given tolerance.
 * The tolerance is specified in ulps.
 */
#define	testall_tol(x, result, tol)				do {	\
	test_tol(cexp, x, result, tol * DBL_ULP());			\
	test_tol(cexpf, x, result, tol * FLT_ULP());			\
} while (0)

/* Various finite non-zero numbers to test. */
static const float finites[] =
{ -42.0e20, -1.0 -1.0e-10, -0.0, 0.0, 1.0e-10, 1.0, 42.0e20 };

/*
 * Determine whether x and y are equal, with two special rules:
 *	+0.0 != -0.0
 *	 NaN == NaN
 * If checksign is 0, we compare the absolute values instead.
 */
static int
fpequal(long double x, long double y, int checksign)
{
	if (isnan(x) || isnan(y))
		return (1);
	if (checksign)
		return (x == y && !signbit(x) == !signbit(y));
	else
		return (fabsl(x) == fabsl(y));
}

static int
fpequal_tol(long double x, long double y, long double tol)
{
	fenv_t env;
	int ret;

	if (isnan(x) && isnan(y))
		return (1);
	if (!signbit(x) != !signbit(y))
		return (0);
	if (x == y)
		return (1);
	if (tol == 0)
		return (0);

	/* Hard case: need to check the tolerance. */
	feholdexcept(&env);
	/*
	 * For our purposes here, if y=0, we interpret tol as an absolute
	 * tolerance. This is to account for roundoff in the input, e.g.,
	 * cos(Pi/2) ~= 0.
	 */
	if (y == 0.0)
		ret = fabsl(x - y) <= fabsl(tol);
	else
		ret = fabsl(x - y) <= fabsl(y * tol);
	fesetenv(&env);
	return (ret);
}

static int
cfpequal(long double complex x, long double complex y, int checksign)
{
	return (fpequal(creal(x), creal(y), checksign)
		&& fpequal(cimag(x), cimag(y), checksign));
}

static int
cfpequal_tol(long double complex x, long double complex y, long double tol)
{
	return (fpequal_tol(creal(x), creal(y), tol)
		&& fpequal_tol(cimag(x), cimag(y), tol));
}


/* Tests for 0 */
void
test_zero(void)
{

	/* cexp(0) = 1, no exceptions raised */
	testall(0.0, 1.0, ALL_STD_EXCEPT, 0, 1);
	testall(-0.0, 1.0, ALL_STD_EXCEPT, 0, 1);
	testall(cpackl(0.0, -0.0), cpackl(1.0, -0.0), ALL_STD_EXCEPT, 0, 1);
	testall(cpackl(-0.0, -0.0), cpackl(1.0, -0.0), ALL_STD_EXCEPT, 0, 1);
}

/*
 * Tests for NaN.  The signs of the results are indeterminate unless the
 * imaginary part is 0.
 */
void
test_nan()
{
	int i;

	/* cexp(x + NaNi) = NaN + NaNi and optionally raises invalid */
	/* cexp(NaN + yi) = NaN + NaNi and optionally raises invalid (|y|>0) */
	for (i = 0; i < N(finites); i++) {
		testall(cpackl(finites[i], NAN), cpackl(NAN, NAN),
			ALL_STD_EXCEPT & ~FE_INVALID, 0, 0);
		if (finites[i] == 0.0)
			continue;
		/* XXX FE_INEXACT shouldn't be raised here */
		testall(cpackl(NAN, finites[i]), cpackl(NAN, NAN),
			ALL_STD_EXCEPT & ~(FE_INVALID | FE_INEXACT), 0, 0);
	}

	/* cexp(NaN +- 0i) = NaN +- 0i */
	testall(cpackl(NAN, 0.0), cpackl(NAN, 0.0), ALL_STD_EXCEPT, 0, 1);
	testall(cpackl(NAN, -0.0), cpackl(NAN, -0.0), ALL_STD_EXCEPT, 0, 1);

	/* cexp(inf + NaN i) = inf + nan i */
	testall(cpackl(INFINITY, NAN), cpackl(INFINITY, NAN),
		ALL_STD_EXCEPT, 0, 0);
	/* cexp(-inf + NaN i) = 0 */
	testall(cpackl(-INFINITY, NAN), cpackl(0.0, 0.0),
		ALL_STD_EXCEPT, 0, 0);
	/* cexp(NaN + NaN i) = NaN + NaN i */
	testall(cpackl(NAN, NAN), cpackl(NAN, NAN),
		ALL_STD_EXCEPT, 0, 0);
}

void
test_inf(void)
{
	int i;

	/* cexp(x + inf i) = NaN + NaNi and raises invalid */
	/* cexp(inf + yi) = 0 + 0yi */
	/* cexp(-inf + yi) = inf + inf yi (except y=0) */
	for (i = 0; i < N(finites); i++) {
		testall(cpackl(finites[i], INFINITY), cpackl(NAN, NAN),
			ALL_STD_EXCEPT, FE_INVALID, 1);
		/* XXX shouldn't raise an inexact exception */
		testall(cpackl(-INFINITY, finites[i]),
			cpackl(0.0, 0.0 * finites[i]),
			ALL_STD_EXCEPT & ~FE_INEXACT, 0, 1);
		if (finites[i] == 0)
			continue;
		testall(cpackl(INFINITY, finites[i]),
			cpackl(INFINITY, INFINITY * finites[i]),
			ALL_STD_EXCEPT & ~FE_INEXACT, 0, 1);
	}
	testall(cpackl(INFINITY, 0.0), cpackl(INFINITY, 0.0),
		ALL_STD_EXCEPT, 0, 1);
	testall(cpackl(INFINITY, -0.0), cpackl(INFINITY, -0.0),
		ALL_STD_EXCEPT, 0, 1);
}

void
test_reals(void)
{
	int i;

	for (i = 0; i < N(finites); i++) {
		/* XXX could check exceptions more meticulously */
		test(cexp, cpackl(finites[i], 0.0),
		     cpackl(exp(finites[i]), 0.0),
		     FE_INVALID | FE_DIVBYZERO, 0, 1);
		test(cexp, cpackl(finites[i], -0.0),
		     cpackl(exp(finites[i]), -0.0),
		     FE_INVALID | FE_DIVBYZERO, 0, 1);
		test(cexpf, cpackl(finites[i], 0.0),
		     cpackl(expf(finites[i]), 0.0),
		     FE_INVALID | FE_DIVBYZERO, 0, 1);
		test(cexpf, cpackl(finites[i], -0.0),
		     cpackl(expf(finites[i]), -0.0),
		     FE_INVALID | FE_DIVBYZERO, 0, 1);
	}
}

void
test_imaginaries(void)
{
	int i;

	for (i = 0; i < N(finites); i++) {
		test(cexp, cpackl(0.0, finites[i]),
		     cpackl(cos(finites[i]), sin(finites[i])),
		     ALL_STD_EXCEPT & ~FE_INEXACT, 0, 1);
		test(cexp, cpackl(-0.0, finites[i]),
		     cpackl(cos(finites[i]), sin(finites[i])),
		     ALL_STD_EXCEPT & ~FE_INEXACT, 0, 1);
		test(cexpf, cpackl(0.0, finites[i]),
		     cpackl(cosf(finites[i]), sinf(finites[i])),
		     ALL_STD_EXCEPT & ~FE_INEXACT, 0, 1);
		test(cexpf, cpackl(-0.0, finites[i]),
		     cpackl(cosf(finites[i]), sinf(finites[i])),
		     ALL_STD_EXCEPT & ~FE_INEXACT, 0, 1);
	}
}

void
test_small(void)
{
	static const double tests[] = {
	     /* csqrt(a + bI) = x + yI */
	     /* a	b	x			y */
		 1.0,	M_PI_4,	M_SQRT2 * 0.5 * M_E,	M_SQRT2 * 0.5 * M_E,
		-1.0,	M_PI_4,	M_SQRT2 * 0.5 / M_E,	M_SQRT2 * 0.5 / M_E,
		 2.0,	M_PI_2,	0.0,			M_E * M_E,
		 M_LN2,	M_PI,	-2.0,			0.0,
	};
	double a, b;
	double x, y;
	int i;

	for (i = 0; i < N(tests); i += 4) {
		a = tests[i];
		b = tests[i + 1];
		x = tests[i + 2];
		y = tests[i + 3];
		test_tol(cexp, cpackl(a, b), cpackl(x, y), 3 * DBL_ULP());

		/* float doesn't have enough precision to pass these tests */
		if (x == 0 || y == 0)
			continue;
		test_tol(cexpf, cpackl(a, b), cpackl(x, y), 1 * FLT_ULP());
        }
}

/* Test inputs with a real part r that would overflow exp(r). */
void
test_large(void)
{

	test_tol(cexp, cpackl(709.79, 0x1p-1074),
		 cpackl(INFINITY, 8.94674309915433533273e-16), DBL_ULP());
	test_tol(cexp, cpackl(1000, 0x1p-1074),
		 cpackl(INFINITY, 9.73344457300016401328e+110), DBL_ULP());
	test_tol(cexp, cpackl(1400, 0x1p-1074),
		 cpackl(INFINITY, 5.08228858149196559681e+284), DBL_ULP());
	test_tol(cexp, cpackl(900, 0x1.23456789abcdep-1020),
		 cpackl(INFINITY, 7.42156649354218408074e+83), DBL_ULP());
	test_tol(cexp, cpackl(1300, 0x1.23456789abcdep-1020),
		 cpackl(INFINITY, 3.87514844965996756704e+257), DBL_ULP());

	test_tol(cexpf, cpackl(88.73, 0x1p-149),
		 cpackl(INFINITY, 4.80265603e-07), 2 * FLT_ULP());
	test_tol(cexpf, cpackl(90, 0x1p-149),
		 cpackl(INFINITY, 1.7101492622e-06f), 2 * FLT_ULP());
	test_tol(cexpf, cpackl(192, 0x1p-149),
		 cpackl(INFINITY, 3.396809344e+38f), 2 * FLT_ULP());
	test_tol(cexpf, cpackl(120, 0x1.234568p-120),
		 cpackl(INFINITY, 1.1163382522e+16f), 2 * FLT_ULP());
	test_tol(cexpf, cpackl(170, 0x1.234568p-120),
		 cpackl(INFINITY, 5.7878851079e+37f), 2 * FLT_ULP());
}

int
main(int argc, char *argv[])
{

	printf("1..7\n");

	test_zero();
	printf("ok 1 - cexp zero\n");

	test_nan();
	printf("ok 2 - cexp nan\n");

	test_inf();
	printf("ok 3 - cexp inf\n");

	test_reals();
	printf("ok 4 - cexp reals\n");

	test_imaginaries();
	printf("ok 5 - cexp imaginaries\n");

	test_small();
	printf("ok 6 - cexp small\n");

	test_large();
	printf("ok 7 - cexp large\n");

	return (0);
}
