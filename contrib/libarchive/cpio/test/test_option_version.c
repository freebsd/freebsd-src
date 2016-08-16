/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "test.h"
__FBSDID("$FreeBSD$");

/*
 * Test that --version option works and generates reasonable output.
 */

static void
verify(const char *p, size_t s)
{
	const char *q = p;

	/* Version message should start with name of program, then space. */
	failure("version message too short:", p);
	if (!assert(s > 6))
		return;
	failure("Version message should begin with 'bsdcpio': %s", p);
	if (!assertEqualMem(q, "bsdcpio ", 8))
		/* If we're not testing bsdcpio, don't keep going. */
		return;
	q += 8; s -= 8;
	/* Version number is a series of digits and periods. */
	while (s > 0 && (*q == '.' || (*q >= '0' && *q <= '9'))) {
		++q;
		--s;
	}
	/* Version number terminated by space. */
	failure("Version: %s", p);
	assert(s > 1);
	/* Skip a single trailing a,b,c, or d. */
	if (*q == 'a' || *q == 'b' || *q == 'c' || *q == 'd')
		++q;
	failure("Version: %s", p);
	assert(*q == ' ');
	++q; --s;
	/* Separator. */
	failure("Version: %s", p);
	assertEqualMem(q, "- ", 2);
	q += 2; s -= 2;
	/* libarchive name and version number */
	assert(s > 11);
	failure("Version: %s", p);
	assertEqualMem(q, "libarchive ", 11);
	q += 11; s -= 11;
	/* Version number is a series of digits and periods. */
	while (s > 0 && (*q == '.' || (*q >= '0' && *q <= '9'))) {
		++q;
		--s;
	}
	/* Skip a single trailing a,b,c, or d. */
	if (*q == 'a' || *q == 'b' || *q == 'c' || *q == 'd')
		++q;
	/* Skip arbitrary third-party version numbers. */
	while (s > 0 && (*q == ' ' || *q == '-' || *q == '/' || *q == '.' || isalnum(*q))) {
		++q;
		--s;
	}
	/* All terminated by end-of-line: \r, \r\n, or \n */
	assert(s >= 1);
	failure("Version: %s", p);
	if (*q == '\x0d') {
		if (q[1] != '\0')
			assertEqualMem(q, "\x0d\x0a", 2);
	} else
		assertEqualMem(q, "\x0a", 1);
}


DEFINE_TEST(test_option_version)
{
	int r;
	char *p;
	size_t s;

	r = systemf("%s --version >version.stdout 2>version.stderr", testprog);
	if (r != 0)
		r = systemf("%s -W version >version.stdout 2>version.stderr",
		    testprog);
	failure("Unable to run either %s --version or %s -W version",
	    testprog, testprog);
	if (!assert(r == 0))
		return;

	/* --version should generate nothing to stderr. */
	assertEmptyFile("version.stderr");
	/* Verify format of version message. */
	p = slurpfile(&s, "version.stdout");
	verify(p, s);
	free(p);
}
