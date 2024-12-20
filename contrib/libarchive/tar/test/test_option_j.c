/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_j)
{
	char *p;
	int r;
	size_t s;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with bzip2 compression. */
	r = systemf("%s -jcf archive.out f 2>archive.err", testprog);
	p = slurpfile(&s, "archive.err");
	p[s] = '\0';
	if (r != 0) {
		if (!canBzip2()) {
			skipping("bzip2 is not supported on this platform");
			goto done;
		}
		failure("-j option is broken");
		assertEqualInt(r, 0);
		goto done;
	}
	free(p);
	assertEmptyFile("archive.err");
	/* Check that the archive file has a bzip2 signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "BZh9", 4);
done:
	free(p);
}
