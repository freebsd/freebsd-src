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
 * Tests for csin[h](), ccos[h](), and ctan[h]().
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
#define	OPT_INVALID	(ALL_STD_EXCEPT & ~FE_INVALID)
#define	OPT_INEXACT	(ALL_STD_EXCEPT & ~FE_INEXACT)
#define	FLT_ULP()	ldexpl(1.0, 1 - FLT_MANT_DIG)
#define	DBL_ULP()	ldexpl(1.0, 1 - DBL_MANT_DIG)
#define	LDBL_ULP()	ldexpl(1.0, 1 - LDBL_MANT_DIG)

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

/* Flags that determine whether to check the signs of the result. */
#define	CS_REAL	1
#define	CS_IMAG	2
#define	CS_BOTH	(CS_REAL | CS_IMAG)

#ifdef	DEBUG
#define	debug(...)	printf(__VA_ARGS__)
#else
#define	debug(...)	(void)0
#endif

/*
 * Test that a function returns the correct value and sets the
 * exception flags correctly. The exceptmask specifies which
 * exceptions we should check. We need to be lenient for several
 * reasons, but mainly because on some architectures it's impossible
 * to raise FE_OVERFLOW without raising FE_INEXACT.
 *
 * These are macros instead of functions so that assert provides more
 * meaningful error messages.
 *
 * XXX The volatile here is to avoid gcc's bogus constant folding and work
 *     around the lack of support for the FENV_ACCESS pragma.
 */
#define	test_p(func, z, result, exceptmask, excepts, checksign)	do {	\
	volatile long double complex _d = z;				\
	debug("  testing %s(%Lg + %Lg I) == %Lg + %Lg I\n", #func,	\
	    creall(_d), cimagl(_d), creall(result), cimagl(result));	\
	assert(feclearexcept(FE_ALL_EXCEPT) == 0);			\
	assert(cfpequal((func)(_d), (result), (checksign)));		\
	assert(((func), fetestexcept(exceptmask) == (excepts)));	\
} while (0)

/*
 * Test within a given tolerance.  The tolerance indicates relative error
 * in ulps.  If result is 0, however, it measures absolute error in units
 * of <format>_EPSILON.
 */
#define	test_p_tol(func, z, result, tol)			do {	\
	volatile long double complex _d = z;				\
	debug("  testing %s(%Lg + %Lg I) ~= %Lg + %Lg I\n", #func,	\
	    creall(_d), cimagl(_d), creall(result), cimagl(result));	\
	assert(cfpequal_tol((func)(_d), (result), (tol)));		\
} while (0)

/* These wrappers apply the identities f(conj(z)) = conj(f(z)). */
#define	test(func, z, result, exceptmask, excepts, checksign)	do {	\
	test_p(func, z, result, exceptmask, excepts, checksign);	\
	test_p(func, conjl(z), conjl(result), exceptmask, excepts, checksign); \
} while (0)
#define	test_tol(func, z, result, tol)				do {	\
	test_p_tol(func, z, result, tol);				\
	test_p_tol(func, conjl(z), conjl(result), tol);			\
} while (0)

/* Test the given function in all precisions. */
#define	testall(func, x, result, exceptmask, excepts, checksign) do {	\
	test(func, x, result, exceptmask, excepts, checksign);		\
	test(func##f, x, result, exceptmask, excepts, checksign);	\
} while (0)
#define	testall_odd(func, x, result, exceptmask, excepts, checksign) do { \
	testall(func, x, result, exceptmask, excepts, checksign);	\
	testall(func, -x, -result, exceptmask, excepts, checksign);	\
} while (0)
#define	testall_even(func, x, result, exceptmask, excepts, checksign) do { \
	testall(func, x, result, exceptmask, excepts, checksign);	\
	testall(func, -x, result, exceptmask, excepts, checksign);	\
} while (0)

/*
 * Test the given function in all precisions, within a given tolerance.
 * The tolerance is specified in ulps.
 */
#define	testall_tol(func, x, result, tol)	       		   do { \
	test_tol(func, x, result, tol * DBL_ULP());			\
	test_tol(func##f, x, result, tol * FLT_ULP());			\
} while (0)
#define	testall_odd_tol(func, x, result, tol)	       		   do { \
	test_tol(func, x, result, tol * DBL_ULP());			\
	test_tol(func, -x, -result, tol * DBL_ULP());			\
} while (0)
#define	testall_even_tol(func, x, result, tol)	       		   do { \
	test_tol(func, x, result, tol * DBL_ULP());			\
	test_tol(func, -x, result, tol * DBL_ULP());			\
} while (0)

/*
 * Determine whether x and y are equal, with two special rules:
 *	+0.0 != -0.0
 *	 NaN == NaN
 * If checksign is 0, we compare the absolute values instead.
 */
static int
fpequal(long double x, long double y, int checksign)
{
	if (isnan(x) && isnan(y))
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
	if (!signbit(x) != !signbit(y) && tol == 0)
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
	return (fpequal(creal(x), creal(y), checksign & CS_REAL)
		&& fpequal(cimag(x), cimag(y), checksign & CS_IMAG));
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
	long double complex zero = cpackl(0.0, 0.0);

	/* csinh(0) = ctanh(0) = 0; ccosh(0) = 1 (no exceptions raised) */
	testall_odd(csinh, zero, zero, ALL_STD_EXCEPT, 0, CS_BOTH);
	testall_odd(csin, zero, zero, ALL_STD_EXCEPT, 0, CS_BOTH);
	testall_even(ccosh, zero, 1.0, ALL_STD_EXCEPT, 0, CS_BOTH);
	testall_even(ccos, zero, cpackl(1.0, -0.0), ALL_STD_EXCEPT, 0, CS_BOTH);
	testall_odd(ctanh, zero, zero, ALL_STD_EXCEPT, 0, CS_BOTH);
	testall_odd(ctan, zero, zero, ALL_STD_EXCEPT, 0, CS_BOTH);
}

/*
 * Tests for NaN inputs.
 */
void
test_nan()
{
	long double complex nan_nan = cpackl(NAN, NAN);
	long double complex z;

	/*
	 * IN		CSINH		CCOSH		CTANH
	 * NaN,NaN	NaN,NaN		NaN,NaN		NaN,NaN
	 * finite,NaN	NaN,NaN [inval]	NaN,NaN [inval]	NaN,NaN [inval]
	 * NaN,finite	NaN,NaN [inval]	NaN,NaN [inval]	NaN,NaN [inval]
	 * NaN,Inf	NaN,NaN [inval]	NaN,NaN	[inval]	NaN,NaN [inval]
	 * Inf,NaN	+-Inf,NaN	Inf,NaN		1,+-0
	 * 0,NaN	+-0,NaN		NaN,+-0		NaN,NaN	[inval]
	 * NaN,0	NaN,0		NaN,+-0		NaN,0
	 */
	z = nan_nan;
	testall_odd(csinh, z, nan_nan, ALL_STD_EXCEPT, 0, 0);
	testall_even(ccosh, z, nan_nan, ALL_STD_EXCEPT, 0, 0);
	testall_odd(ctanh, z, nan_nan, ALL_STD_EXCEPT, 0, 0);
	testall_odd(csin, z, nan_nan, ALL_STD_EXCEPT, 0, 0);
	testall_even(ccos, z, nan_nan, ALL_STD_EXCEPT, 0, 0);
	testall_odd(ctan, z, nan_nan, ALL_STD_EXCEPT, 0, 0);

	z = cpackl(42, NAN);
	testall_odd(csinh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_even(ccosh, z, nan_nan, OPT_INVALID, 0, 0);
	/* XXX We allow a spurious inexact exception here. */
	testall_odd(ctanh, z, nan_nan, OPT_INVALID & ~FE_INEXACT, 0, 0);
	testall_odd(csin, z, nan_nan, OPT_INVALID, 0, 0);
	testall_even(ccos, z, nan_nan, OPT_INVALID, 0, 0);
	testall_odd(ctan, z, nan_nan, OPT_INVALID, 0, 0);

	z = cpackl(NAN, 42);
	testall_odd(csinh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_even(ccosh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_odd(ctanh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_odd(csin, z, nan_nan, OPT_INVALID, 0, 0);
	testall_even(ccos, z, nan_nan, OPT_INVALID, 0, 0);
	/* XXX We allow a spurious inexact exception here. */
	testall_odd(ctan, z, nan_nan, OPT_INVALID & ~FE_INEXACT, 0, 0);

	z = cpackl(NAN, INFINITY);
	testall_odd(csinh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_even(ccosh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_odd(ctanh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_odd(csin, z, cpackl(NAN, INFINITY), ALL_STD_EXCEPT, 0, 0);
	testall_even(ccos, z, cpackl(INFINITY, NAN), ALL_STD_EXCEPT, 0,
	    CS_IMAG);
	testall_odd(ctan, z, cpackl(0, 1), ALL_STD_EXCEPT, 0, CS_IMAG);

	z = cpackl(INFINITY, NAN);
	testall_odd(csinh, z, cpackl(INFINITY, NAN), ALL_STD_EXCEPT, 0, 0);
	testall_even(ccosh, z, cpackl(INFINITY, NAN), ALL_STD_EXCEPT, 0,
		     CS_REAL);
	testall_odd(ctanh, z, cpackl(1, 0), ALL_STD_EXCEPT, 0, CS_REAL);
	testall_odd(csin, z, nan_nan, OPT_INVALID, 0, 0);
	testall_even(ccos, z, nan_nan, OPT_INVALID, 0, 0);
	testall_odd(ctan, z, nan_nan, OPT_INVALID, 0, 0);

	z = cpackl(0, NAN);
	testall_odd(csinh, z, cpackl(0, NAN), ALL_STD_EXCEPT, 0, 0);
	testall_even(ccosh, z, cpackl(NAN, 0), ALL_STD_EXCEPT, 0, 0);
	testall_odd(ctanh, z, nan_nan, OPT_INVALID, 0, 0);
	testall_odd(csin, z, cpackl(0, NAN), ALL_STD_EXCEPT, 0, CS_REAL);
	testall_even(ccos, z, cpackl(NAN, 0), ALL_STD_EXCEPT, 0, 0);
	testall_odd(ctan, z, cpackl(0, NAN), ALL_STD_EXCEPT, 0, CS_REAL);

	z = cpackl(NAN, 0);
	testall_odd(csinh, z, cpackl(NAN, 0), ALL_STD_EXCEPT, 0, CS_IMAG);
	testall_even(ccosh, z, cpackl(NAN, 0), ALL_STD_EXCEPT, 0, 0);
	testall_odd(ctanh, z, cpackl(NAN, 0), ALL_STD_EXCEPT, 0, CS_IMAG);
	testall_odd(csin, z, cpackl(NAN, 0), ALL_STD_EXCEPT, 0, 0);
	testall_even(ccos, z, cpackl(NAN, 0), ALL_STD_EXCEPT, 0, 0);
	testall_odd(ctan, z, nan_nan, OPT_INVALID, 0, 0);
}

void
test_inf(void)
{
	static const long double finites[] = {
	    0, M_PI / 4, 3 * M_PI / 4, 5 * M_PI / 4,
	};
	long double complex z, c, s;
	int i;

	/*
	 * IN		CSINH		CCOSH		CTANH
	 * Inf,Inf	+-Inf,NaN inval	+-Inf,NaN inval	1,+-0
	 * Inf,finite	Inf cis(finite)	Inf cis(finite)	1,0 sin(2 finite)
	 * 0,Inf	+-0,NaN	inval	NaN,+-0 inval	NaN,NaN	inval
	 * finite,Inf	NaN,NaN inval	NaN,NaN inval	NaN,NaN inval
	 */
	z = cpackl(INFINITY, INFINITY);
	testall_odd(csinh, z, cpackl(INFINITY, NAN),
		    ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_even(ccosh, z, cpackl(INFINITY, NAN),
		     ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_odd(ctanh, z, cpackl(1, 0), ALL_STD_EXCEPT, 0, CS_REAL);
	testall_odd(csin, z, cpackl(NAN, INFINITY),
		    ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_even(ccos, z, cpackl(INFINITY, NAN),
		     ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_odd(ctan, z, cpackl(0, 1), ALL_STD_EXCEPT, 0, CS_REAL);

	/* XXX We allow spurious inexact exceptions here (hard to avoid). */
	for (i = 0; i < sizeof(finites) / sizeof(finites[0]); i++) {
		z = cpackl(INFINITY, finites[i]);
		c = INFINITY * cosl(finites[i]);
		s = finites[i] == 0 ? finites[i] : INFINITY * sinl(finites[i]);
		testall_odd(csinh, z, cpackl(c, s), OPT_INEXACT, 0, CS_BOTH);
		testall_even(ccosh, z, cpackl(c, s), OPT_INEXACT, 0, CS_BOTH);
		testall_odd(ctanh, z, cpackl(1, 0 * sin(finites[i] * 2)),
			    OPT_INEXACT, 0, CS_BOTH);
		z = cpackl(finites[i], INFINITY);
		testall_odd(csin, z, cpackl(s, c), OPT_INEXACT, 0, CS_BOTH);
		testall_even(ccos, z, cpackl(c, -s), OPT_INEXACT, 0, CS_BOTH);
		testall_odd(ctan, z, cpackl(0 * sin(finites[i] * 2), 1),
			    OPT_INEXACT, 0, CS_BOTH);
	}

	z = cpackl(0, INFINITY);
	testall_odd(csinh, z, cpackl(0, NAN), ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_even(ccosh, z, cpackl(NAN, 0), ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_odd(ctanh, z, cpackl(NAN, NAN), ALL_STD_EXCEPT, FE_INVALID, 0);
	z = cpackl(INFINITY, 0);
	testall_odd(csin, z, cpackl(NAN, 0), ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_even(ccos, z, cpackl(NAN, 0), ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_odd(ctan, z, cpackl(NAN, NAN), ALL_STD_EXCEPT, FE_INVALID, 0);

	z = cpackl(42, INFINITY);
	testall_odd(csinh, z, cpackl(NAN, NAN), ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_even(ccosh, z, cpackl(NAN, NAN), ALL_STD_EXCEPT, FE_INVALID, 0);
	/* XXX We allow a spurious inexact exception here. */
	testall_odd(ctanh, z, cpackl(NAN, NAN), OPT_INEXACT, FE_INVALID, 0);
	z = cpackl(INFINITY, 42);
	testall_odd(csin, z, cpackl(NAN, NAN), ALL_STD_EXCEPT, FE_INVALID, 0);
	testall_even(ccos, z, cpackl(NAN, NAN), ALL_STD_EXCEPT, FE_INVALID, 0);
	/* XXX We allow a spurious inexact exception here. */
	testall_odd(ctan, z, cpackl(NAN, NAN), OPT_INEXACT, FE_INVALID, 0);
}

/* Tests along the real and imaginary axes. */
void
test_axes(void)
{
	static const long double nums[] = {
	    M_PI / 4, M_PI / 2, 3 * M_PI / 4,
	    5 * M_PI / 4, 3 * M_PI / 2, 7 * M_PI / 4,
	};
	long double complex z;
	int i;

	for (i = 0; i < sizeof(nums) / sizeof(nums[0]); i++) {
		/* Real axis */
		z = cpackl(nums[i], 0.0);
		testall_odd_tol(csinh, z, cpackl(sinh(nums[i]), 0), 0);
		testall_even_tol(ccosh, z, cpackl(cosh(nums[i]), 0), 0);
		testall_odd_tol(ctanh, z, cpackl(tanh(nums[i]), 0), 1);
		testall_odd_tol(csin, z, cpackl(sin(nums[i]),
					    copysign(0, cos(nums[i]))), 0);
		testall_even_tol(ccos, z, cpackl(cos(nums[i]),
		    -copysign(0, sin(nums[i]))), 0);
		testall_odd_tol(ctan, z, cpackl(tan(nums[i]), 0), 1);

		/* Imaginary axis */
		z = cpackl(0.0, nums[i]);
		testall_odd_tol(csinh, z, cpackl(copysign(0, cos(nums[i])),
						 sin(nums[i])), 0);
		testall_even_tol(ccosh, z, cpackl(cos(nums[i]),
		    copysign(0, sin(nums[i]))), 0);
		testall_odd_tol(ctanh, z, cpackl(0, tan(nums[i])), 1);
		testall_odd_tol(csin, z, cpackl(0, sinh(nums[i])), 0);
		testall_even_tol(ccos, z, cpackl(cosh(nums[i]), -0.0), 0);
		testall_odd_tol(ctan, z, cpackl(0, tanh(nums[i])), 1);
	}
}

void
test_small(void)
{
	/*
	 * z =  0.5 + i Pi/4
	 *     sinh(z) = (sinh(0.5) + i cosh(0.5)) * sqrt(2)/2
	 *     cosh(z) = (cosh(0.5) + i sinh(0.5)) * sqrt(2)/2
	 *     tanh(z) = (2cosh(0.5)sinh(0.5) + i) / (2 cosh(0.5)**2 - 1)
	 * z = -0.5 + i Pi/2
	 *     sinh(z) = cosh(0.5)
	 *     cosh(z) = -i sinh(0.5)
	 *     tanh(z) = -coth(0.5)
	 * z =  1.0 + i 3Pi/4
	 *     sinh(z) = (-sinh(1) + i cosh(1)) * sqrt(2)/2
	 *     cosh(z) = (-cosh(1) + i sinh(1)) * sqrt(2)/2
	 *     tanh(z) = (2cosh(1)sinh(1) - i) / (2cosh(1)**2 - 1)
	 */
	static const struct {
		long double a, b;
		long double sinh_a, sinh_b;
		long double cosh_a, cosh_b;
		long double tanh_a, tanh_b;
	} tests[] = {
		{  0.5L,
		   0.78539816339744830961566084581987572L,
		   0.36847002415910435172083660522240710L,
		   0.79735196663945774996093142586179334L,
		   0.79735196663945774996093142586179334L,
		   0.36847002415910435172083660522240710L,
		   0.76159415595576488811945828260479359L,
		   0.64805427366388539957497735322615032L },
		{ -0.5L,
		   1.57079632679489661923132169163975144L,
		   0.0L,
		   1.12762596520638078522622516140267201L,
		   0.0L,
		  -0.52109530549374736162242562641149156L,
		  -2.16395341373865284877000401021802312L,
		   0.0L },
		{  1.0L,
		   2.35619449019234492884698253745962716L,
		  -0.83099273328405698212637979852748608L,
		   1.09112278079550143030545602018565236L,
		  -1.09112278079550143030545602018565236L,
		   0.83099273328405698212637979852748609L,
		   0.96402758007581688394641372410092315L,
		  -0.26580222883407969212086273981988897L }
	};
	long double complex z;
	int i;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
		z = cpackl(tests[i].a, tests[i].b);
		testall_odd_tol(csinh, z,
		    cpackl(tests[i].sinh_a, tests[i].sinh_b), 1.1);
		testall_even_tol(ccosh, z,
		    cpackl(tests[i].cosh_a, tests[i].cosh_b), 1.1);
		testall_odd_tol(ctanh, z,
		    cpackl(tests[i].tanh_a, tests[i].tanh_b), 1.1);
        }
}

/* Test inputs that might cause overflow in a sloppy implementation. */
void
test_large(void)
{
	long double complex z;

	/* tanh() uses a threshold around x=22, so check both sides. */
	z = cpackl(21, 0.78539816339744830961566084581987572L);
	testall_odd_tol(ctanh, z,
	    cpackl(1.0, 1.14990445285871196133287617611468468e-18L), 1);
	z++;
	testall_odd_tol(ctanh, z,
	    cpackl(1.0, 1.55622644822675930314266334585597964e-19L), 1);

	z = cpackl(355, 0.78539816339744830961566084581987572L);
	testall_odd_tol(ctanh, z,
	    cpackl(1.0, 8.95257245135025991216632140458264468e-309L), 1);
	z = cpackl(30, 0x1p1023L);
	testall_odd_tol(ctanh, z,
	    cpackl(1.0, -1.62994325413993477997492170229268382e-26L), 1);
	z = cpackl(1, 0x1p1023L);
	testall_odd_tol(ctanh, z,
	    cpackl(0.878606311888306869546254022621986509L,
		   -0.225462792499754505792678258169527424L), 1);

	z = cpackl(710.6, 0.78539816339744830961566084581987572L);
	testall_odd_tol(csinh, z,
	    cpackl(1.43917579766621073533185387499658944e308L,
		   1.43917579766621073533185387499658944e308L), 1);
	testall_even_tol(ccosh, z,
	    cpackl(1.43917579766621073533185387499658944e308L,
		   1.43917579766621073533185387499658944e308L), 1);

	z = cpackl(1500, 0.78539816339744830961566084581987572L);
	testall_odd(csinh, z, cpackl(INFINITY, INFINITY), OPT_INEXACT,
	    FE_OVERFLOW, CS_BOTH);
	testall_even(ccosh, z, cpackl(INFINITY, INFINITY), OPT_INEXACT,
	    FE_OVERFLOW, CS_BOTH);
}

int
main(int argc, char *argv[])
{

	printf("1..6\n");

	test_zero();
	printf("ok 1 - ctrig zero\n");

	test_nan();
	printf("ok 2 - ctrig nan\n");

	test_inf();
	printf("ok 3 - ctrig inf\n");

	test_axes();
	printf("ok 4 - ctrig axes\n");

	test_small();
	printf("ok 5 - ctrig small\n");

	test_large();
	printf("ok 6 - ctrig large\n");

	return (0);
}
