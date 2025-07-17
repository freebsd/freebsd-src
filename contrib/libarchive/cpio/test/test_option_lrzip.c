/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_lrzip)
{
	char *p;
	size_t s;

	if (!canLrzip()) {
		skipping("lrzip is not supported on this platform");
		return;
	}

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with lrzip compression. */
	assertEqualInt(0,
	    systemf("echo f | %s -o --lrzip >archive.out 2>archive.err",
	    testprog));
	p = slurpfile(&s, "archive.err");
	free(p);
	/* Check that the archive file has an lzma signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "LRZI\x00", 5);
	free(p);
}
