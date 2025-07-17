/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_y)
{
	char *p;
	int r;
	size_t s;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with bzip2 compression. */
	r = systemf("echo f | %s -oy >archive.out 2>archive.err",
	    testprog);
	p = slurpfile(&s, "archive.err");
	free(p);
	if (r != 0) {
		if (!canBzip2()) {
			skipping("bzip2 is not supported on this platform");
			return;
		}
		failure("-y option is broken");
		assertEqualInt(r, 0);
		return;
	}
	assertTextFileContents("1 block\n", "archive.err");
	/* Check that the archive file has a bzip2 signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "BZh9", 4);
	free(p);
}
