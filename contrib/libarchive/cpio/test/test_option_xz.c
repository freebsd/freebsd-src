/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_xz)
{
	char *p;
	int r;
	size_t s;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Archive it with xz compression. */
	r = systemf("echo f | %s -o --xz >archive.out 2>archive.err",
	    testprog);
	p = slurpfile(&s, "archive.err");
	p[s] = '\0';
	if (r != 0) {
		if (strstr(p, "compression not available") != NULL) {
			skipping("This version of bsdcpio was compiled "
			    "without xz support");
			free(p);
			return;
		}
		free(p);
		failure("--xz option is broken");
		assertEqualInt(r, 0);
		return;
	}
	free(p);
	/* Check that the archive file has an xz signature. */
	p = slurpfile(&s, "archive.out");
	assert(s > 2);
	assertEqualMem(p, "\xFD\x37\x7A\x58\x5A\x00", 6);
	free(p);
}
