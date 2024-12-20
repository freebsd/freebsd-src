/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_lzop)
{
	char *p;
	int r;
	size_t s;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with lzop compression. */
	r = systemf("%s -cf - --lzop f >archive.out 2>archive.err", testprog);
	p = slurpfile(&s, "archive.err");
	p[s] = '\0';
	if (r != 0) {
		if (!canLzop()) {
			skipping("lzop is not supported on this platform");
			goto done;
		}
		failure("--lzop option is broken");
		assertEqualInt(r, 0);
		goto done;
	}
	free(p);
	/* Check that the archive file has an lzma signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "\x89\x4c\x5a\x4f\x00\x0d\x0a\x1a\x0a", 9);
done:
	free(p);
}
