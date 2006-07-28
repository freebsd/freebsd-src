/*-
 * Copyright (c) 2006, Simon L. Nielsen <simon@FreeBSD.org>
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
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <mp.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>

MINT *c0, *c1, *c2, *c3, *c5, *c6, *c8, *c10, *c14, *c15, *c25, \
    *c42,*c43, *c44, *c45, *t0, *t1;
static int tnr = 0;

static void
testmcmp(const MINT *mp1, const MINT *mp2, const char *tname)
{

	if (mcmp(mp1, mp2) == 0)
		printf("ok %d - %s\n", ++tnr, tname);
	else
		printf("not ok - %d %s\n", ++tnr, tname);
}

static int
testsimpel(void)
{
	const char str42[] = "2a";
	MINT *t2;
	char *s;

	madd(c42, c1, t0);
	testmcmp(c43, t0, "madd0");
	madd(t0, c1, t0);
	testmcmp(c44, t0, "madd1");
	msub(t0, c1, t0);
	testmcmp(c43, t0, "msub0");
	msub(t0, c1, t0);
	testmcmp(c42, t0, "msub1");
	move(c42, t0);
	testmcmp(c42, t0, "move0");

	t2 = xtom(str42);
	testmcmp(c42, t2, "xtom");
	s = mtox(t2);
	if (strcmp(str42, s) == 0)
		printf("ok %d - %s\n", ++tnr, "mtox0");
	else
		printf("not ok %d - %s\n", ++tnr, "mtox0");
	mfree(t2);
}

static int
testgcd(void)
{

	gcd(c10, c15, t0);
	testmcmp(t0, c5, "gcd0");
}

static int
testmsqrt(void)
{

	msqrt(c25, t0, t1);
	testmcmp(t0, c5, "msqrt0");
	testmcmp(t1, c0, "msqrt1");
	msqrt(c42, t0, t1);
	testmcmp(t0, c6, "msqrt2");
	testmcmp(t1, c6, "msqrt3");
}

static int
testdiv(void)
{
	short ro;
	MINT *t2;

	mdiv(c42, c5, t0, t1);
	testmcmp(t0, c8, "mdiv0");
	testmcmp(t1, c2, "mdiv1");

	mdiv(c10, c8, t0, t1);
	testmcmp(t0, c1, "mdiv2");
	testmcmp(t1, c2, "mdiv3");

	sdiv(c42, 5, t0, &ro);
	testmcmp(t0, c8, "sdiv0");
	t2 = itom(ro); // Simpler to use common testmcmp()
	testmcmp(t2, c2, "sdiv1");
	mfree(t2);

	sdiv(c10, 8, t0, &ro);
	testmcmp(t0, c1, "sdiv2");
	t2 = itom(ro); // Simpler to use common testmcmp()
	testmcmp(t2, c2, "sdiv3");
	mfree(t2);
}

static int
testmult(void)
{

	mult(c5, c2, t0);
	testmcmp(t0, c10, "mmult0");
	mult(c3, c14, t0);
	testmcmp(t0, c42, "mmult1");
}

static int
testpow(void)
{

	pow(c2, c3, c10, t0);
	testmcmp(t0, c8, "pow0");
	pow(c2, c3, c3, t0);
	testmcmp(t0, c2, "pow1");
	rpow(c2, 3, t0);
	testmcmp(t0, c8, "rpow0");
}

/*
 * This program performs some very basic tests of libmp(3).  It is by
 * no means expected to perform a complete test of the library for
 * correctness, but is meant to test the API to make sure libmp (or
 * libcrypto) updates don't totally break the library.
 */
int
main(int argc, char *argv[])
{

	printf("1..25\n");

	/*
	 * Init "constants" variables - done in this somewhat
	 * cumbersome way to in theory be able to check for memory
	 * leaks.
	 */
	c0 = itom(0);
	c1 = itom(1);
	c2 = itom(2);
	c3 = itom(3);
	c5 = itom(5);
	c6 = itom(6);
	c8 = itom(8);
	c10 = itom(10);
	c14 = itom(14);
	c15 = itom(15);
	c25 = itom(25);
	c42 = itom(42);
	c43 = itom(43);
	c44 = itom(44);
	c45 = itom(45);

	// Init temp variables
	t0 = itom(0);
	t1 = itom(0);

	// Run tests
	testsimpel();
	testgcd();
	testdiv();
	testmult();
	testpow();
	testmsqrt();

	// Cleanup
	mfree(c0);
	mfree(c1);
	mfree(c2);
	mfree(c3);
	mfree(c5);
	mfree(c6);
	mfree(c8);
	mfree(c10);
	mfree(c14);
	mfree(c15);
	mfree(c25);
	mfree(c42);
	mfree(c43);
	mfree(c44);
	mfree(c45);
	mfree(t0);
	mfree(t1);

	return (EX_OK);
}
