/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
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
	r = systemf("%s -zcf archive.out f 2>archive.err", testprog);
	p = slurpfile(&s, "archive.err");
	p[s] = '\0';
	if (r != 0) {
		if (!canGzip()) {
			skipping("gzip is not supported on this platform");
			goto done;
		}
		failure("-z option is broken");
		assertEqualInt(r, 0);
		goto done;
	}
	free(p);
	/* Check that the archive file has a gzip signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 4);
	assertEqualMem(p, "\x1f\x8b\x08\x00", 4);
done:
	free(p);
}
