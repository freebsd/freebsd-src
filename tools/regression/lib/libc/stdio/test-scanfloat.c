/*-
 * Copyright (C) 2003, 2005 David Schultz <das@FreeBSD.org>
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
 * Test for scanf() floating point formats.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define	eq(type, a, b)	_eq(type##_EPSILON, (a), (b))
static int _eq(long double epsilon, long double a, long double b);

int
main(int argc, char *argv[])
{
	char buf[128];
	long double ld = 0.0;
	double d = 0.0;
	float f = 0.0;

	printf("1..3\n");

	buf[0] = '\0';
	assert(setlocale(LC_NUMERIC, ""));

	/*
	 * Various tests for normalized numbers
	 */
	sscanf("3.141592", "%e", &f);
	assert(eq(FLT, f, 3.141592));

	sscanf("3.141592653589793", "%lf", &d);
	assert(eq(DBL, d, 3.141592653589793));

	sscanf("1.234568e+06", "%E", &f);
	assert(eq(FLT, f, 1.234568e+06));

	sscanf("-1.234568e6", "%lF", &d);
	assert(eq(DBL, d, -1.234568e6));

	sscanf("+1.234568e-52", "%LG", &ld);
	assert(eq(LDBL, ld, 1.234568e-52L));

	sscanf("0.1", "%la", &d);
	assert(eq(DBL, d, 0.1));

	sscanf("00.2", "%lA", &d);
	assert(eq(DBL, d, 0.2));

	sscanf("123456", "%5le%s", &d, buf);
	assert(eq(DBL, d, 12345.));
	assert(strcmp(buf, "6") == 0);

	sscanf("1.0Q", "%*5le%s", buf);
	assert(strcmp(buf, "Q") == 0);

	sscanf("-1.23e", "%e%s", &f, buf);
	assert(eq(FLT, f, -1.23));
	assert(strcmp(buf, "e") == 0);

	sscanf("1.25e+", "%le%s", &d, buf);
	assert(eq(DBL, d, 1.25));
	assert(strcmp(buf, "e+") == 0);

	sscanf("1.23E4E5", "%le%s", &d, buf);
	assert(eq(DBL, d, 1.23e4));
	assert(strcmp(buf, "E5") == 0);

	sscanf("12e6", "%le", &d);
	assert(eq(DBL, d, 12e6));

	sscanf("1.a", "%le%s", &d, buf);
	assert(eq(DBL, d, 1.0));
	assert(strcmp(buf, "a") == 0);

	sscanf(".0p4", "%le%s", &d, buf);
	assert(eq(DBL, d, 0.0));
	assert(strcmp(buf, "p4") == 0);

	d = 0.25;
	assert(sscanf(".", "%le", &d) == 0);
	assert(d == 0.25);

	sscanf("0x08", "%le", &d);
	assert(d == 0x8p0);

	sscanf("0x90a.bcdefP+09a", "%le%s", &d, buf);
	assert(d == 0x90a.bcdefp+09);
	assert(strcmp(buf, "a") == 0);

#if (LDBL_MANT_DIG > DBL_MANT_DIG) && !defined(__i386__)
	sscanf("3.14159265358979323846", "%Lg", &ld);
	assert(eq(LDBL, ld, 3.14159265358979323846L));

	sscanf("  0X.0123456789abcdefffp-3g", "%Le%s", &ld, buf);
	assert(ld == 0x0.0123456789abcdefffp-3L);
	assert(strcmp(buf, "g") == 0);
#endif

	sscanf("0xg", "%le%s", &d, buf);
	assert(d == 0.0);
	assert(strcmp(buf, "xg") == 0);

	assert(setlocale(LC_NUMERIC, "ru_RU.ISO8859-5")); /* decimalpoint==, */

	sscanf("1.23", "%le%s", &d, buf);
	assert(d == 1.0);
	assert(strcmp(buf, ".23") == 0);

	sscanf("1,23", "%le", &d);
	assert(d == 1.23);

	assert(setlocale(LC_NUMERIC, ""));

	printf("ok 1 - scanfloat\n");

	/*
	 * Infinity and NaN tests
	 */
	sscanf("-Inf", "%le", &d);
	assert(d < 0.0 && isinf(d));

	sscanf("iNfInItY and beyond", "%le%s", &d, buf);
	assert(d > 0.0 && isinf(d));
	assert(strcmp(buf, " and beyond"));

	sscanf("NaN", "%le", &d);
	assert(isnan(d));

	sscanf("NAN(123Y", "%le%s", &d, buf);
	assert(isnan(d));
	assert(strcmp(buf, "(123Y") == 0);

	sscanf("nan(f00f)plugh", "%le%s", &d, buf);
	assert(isnan(d));
	assert(strcmp(buf, "plugh") == 0);

	sscanf("-nan", "%le", &d);
	assert(isnan(d));

	/* Only quiet NaNs should be returned. */
	sscanf("NaN", "%e", &f);
	sscanf("nan", "%le", &d);
	sscanf("nan", "%Le", &ld);
	feclearexcept(FE_ALL_EXCEPT);
	assert(f != f);
	assert(d != d);
	assert(ld != ld);
	assert(fetestexcept(FE_INVALID) == 0);
	sscanf("nan(1234)", "%e", &f);
	sscanf("nan(1234)", "%le", &d);
	sscanf("nan(1234)", "%Le", &ld);
	feclearexcept(FE_ALL_EXCEPT);
	assert(f != f);
	assert(d != d);
	assert(ld != ld);
#if 0
	/*
	 * POSIX says we should only generate quiet NaNs, but the gdtoa
	 * author convincingly argues that if you ask for a NaN format
	 * based on some implementation-defined string, you should get
	 * what you asked for, even if it's a signaling NaN.
	 */
	assert(fetestexcept(FE_INVALID) == 0);
#endif

	printf("ok 2 - scanfloat\n");

	/*
	 * Rounding tests
	 */

	fesetround(FE_DOWNWARD);

	sscanf("1.999999999999999999999999999999999", "%le", &d);
	assert(d < 2.0);
	sscanf("0x1.ffffffffffffffp0", "%le", &d);
	assert(d < 2.0);
	sscanf("1.999999999999999999999999999999999", "%Le", &ld);
	assert(ld < 2.0);

	sscanf("1.0571892669084007", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc59p0);
	sscanf("-1.0571892669084007", "%le", &d);
	assert(d == -0x1.0ea3f4af0dc5ap0);
	sscanf("1.0571892669084010", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc5ap0);

	sscanf("0x1.23p-5000", "%le", &d);
	assert(d == 0.0);

	sscanf("0x1.2345678p-1050", "%le", &d);
	assert(d == 0x1.234567p-1050);

	fesetround(FE_UPWARD);

	sscanf("1.0571892669084007", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc5ap0);
	sscanf("-1.0571892669084007", "%le", &d);
	assert(d == -0x1.0ea3f4af0dc59p0);
	sscanf("1.0571892669084010", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc5bp0);

	sscanf("0x1.23p-5000", "%le", &d);
	assert(d == 0x1p-1074);

	sscanf("0x1.2345678p-1050", "%le", &d);
	assert(d == 0x1.234568p-1050);

	fesetround(FE_TOWARDZERO);

	sscanf("1.0571892669084007", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc59p0);
	sscanf("-1.0571892669084007", "%le", &d);
	assert(d == -0x1.0ea3f4af0dc59p0);
	sscanf("1.0571892669084010", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc5ap0);

	sscanf("0x1.23p-5000", "%le", &d);
	assert(d == 0.0);

	sscanf("0x1.2345678p-1050", "%le", &d);
	assert(d == 0x1.234567p-1050);

	fesetround(FE_TONEAREST);

	/* 1.0571892669084007 is slightly closer to 0x1.0ea3f4af0dc59p0 */
	sscanf("1.0571892669084007", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc59p0);
	sscanf("-1.0571892669084007", "%le", &d);
	assert(d == -0x1.0ea3f4af0dc59p0);
	sscanf("1.0571892669084010", "%le", &d);
	assert(d == 0x1.0ea3f4af0dc5bp0);

	/* strtod() should round small numbers to 0. */
	sscanf("0x1.23p-5000", "%le", &d);
	assert(d == 0.0);

	/* Extra digits in a denormal shouldn't break anything. */
	sscanf("0x1.2345678p-1050", "%le", &d);
	assert(d == 0x1.234568p-1050);

	printf("ok 3 - scanfloat\n");

	return (0);
}

static int
_eq(long double epsilon, long double a, long double b)
{
	long double delta;

	delta = a - b;
	if (delta < 0)		/* XXX no fabsl() */
		delta = -delta;
	return (delta <= epsilon);
}
