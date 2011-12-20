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
 * Also see test_option_q for additional validation of -r support.
 */
DEFINE_TEST(test_option_r)
{
	char buff[15];
	char *p0, *p1;
	size_t s;
	FILE *f;
	int r;

	/* Create a file */
	f = fopen("f1", "w");
	if (!assert(f != NULL))
		return;
	assertEqualInt(3, fwrite("abc", 1, 3, f));
	fclose(f);

	/* Archive that one file. */
	r = systemf("%s cf archive.tar --format=ustar f1 >step1.out 2>step1.err", testprog);
	failure("Error invoking %s cf archive.tar f1", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("step1.out");
	assertEmptyFile("step1.err");


	/* Do some basic validation of the constructed archive. */
	p0 = slurpfile(&s, "archive.tar");
	if (!assert(p0 != NULL))
		return;
	if (!assert(s >= 2048)) {
		free(p0);
		return;
	}
	assertEqualMem(p0 + 0, "f1", 3);
	assertEqualMem(p0 + 512, "abc", 3);
	assertEqualMem(p0 + 1024, "\0\0\0\0\0\0\0\0", 8);
	assertEqualMem(p0 + 1536, "\0\0\0\0\0\0\0\0", 8);

	/* Edit that file */
	f = fopen("f1", "w");
	if (!assert(f != NULL))
		return;
	assertEqualInt(3, fwrite("123", 1, 3, f));
	fclose(f);

	/* Update the archive. */
	r = systemf("%s rf archive.tar --format=ustar f1 >step2.out 2>step2.err", testprog);
	failure("Error invoking %s rf archive.tar f1", testprog);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stdout or stderr. */
	assertEmptyFile("step2.out");
	assertEmptyFile("step2.err");

	/* Do some basic validation of the constructed archive. */
	p1 = slurpfile(&s, "archive.tar");
	if (!assert(p1 != NULL)) {
		free(p0);
		return;
	}
	assert(s >= 3072);
	/* Verify first entry is unchanged. */
	assertEqualMem(p0, p1, 1024);
	/* Verify that second entry is correct. */
	assertEqualMem(p1 + 1024, "f1", 3);
	assertEqualMem(p1 + 1536, "123", 3);
	/* Verify end-of-archive marker. */
	assertEqualMem(p1 + 2048, "\0\0\0\0\0\0\0\0", 8);
	assertEqualMem(p1 + 2560, "\0\0\0\0\0\0\0\0", 8);
	free(p0);
	free(p1);

	/* Unpack both items */
	assertMakeDir("step3", 0775);
	assertChdir("step3");
	r = systemf("%s xf ../archive.tar", testprog);
	failure("Error invoking %s xf archive.tar", testprog);
	assertEqualInt(r, 0);

	/* Verify that the second one overwrote the first. */
	f = fopen("f1", "r");
	if (assert(f != NULL)) {
		assertEqualInt(3, fread(buff, 1, 3, f));
		assertEqualMem(buff, "123", 3);
		fclose(f);
	}
}
