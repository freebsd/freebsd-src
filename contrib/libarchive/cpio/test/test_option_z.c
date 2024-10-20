/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_z)
{
	char *p;
	int r;
	size_t s;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with gzip compression. */
	r = systemf("echo f | %s -oz >archive.out 2>archive.err",
	    testprog);
	p = slurpfile(&s, "archive.err");
	free(p);
	if (r != 0) {
		if (!canGzip()) {
			skipping("gzip is not supported on this platform");
			return;
		}
		failure("-z option is broken");
		assertEqualInt(r, 0);
		return;
	}
	/* Check that the archive file has a gzip signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 4);
	assertEqualMem(p, "\x1f\x8b\x08\x00", 4);
	free(p);
}
