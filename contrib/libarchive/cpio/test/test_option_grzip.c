/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_grzip)
{
	char *p;
	size_t s;

	if (!canGrzip()) {
		skipping("grzip is not supported on this platform");
		return;
	}

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with grzip compression. */
	assertEqualInt(0,
	    systemf("echo f | %s -o --grzip >archive.out 2>archive.err",
	    testprog));
	p = slurpfile(&s, "archive.err");
	free(p);
	/* Check that the archive file has an grzip signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "GRZipII\x00\x02\x04:)", 12);
	free(p);
}
