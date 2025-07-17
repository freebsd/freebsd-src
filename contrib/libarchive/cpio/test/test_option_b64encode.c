/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_b64encode)
{
	char *p;
	size_t s;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with compress compression and uuencode. */
	assertEqualInt(0,
	    systemf("echo f | %s -o -Z --b64encode >archive.out 2>archive.err",
	    testprog));
	/* Check that the archive file has an uuencode signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "begin-base64 644", 16);
	free(p);

	/* Archive it with uuencode only. */
	assertEqualInt(0,
	    systemf("echo f | %s -o --b64encode >archive.out 2>archive.err",
	    testprog));
	/* Check that the archive file has an uuencode signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "begin-base64 644", 16);
	free(p);
}
