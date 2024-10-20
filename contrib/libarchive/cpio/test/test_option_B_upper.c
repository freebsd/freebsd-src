/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_B_upper)
{
	struct stat st;
	int r;

	/*
	 * Create a file on disk.
	 */
	assertMakeFile("file", 0644, NULL);

	/* Create an archive without -B; this should be 512 bytes. */
	r = systemf("echo file | %s -o > small.cpio 2>small.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "small.err");
	assertEqualInt(0, stat("small.cpio", &st));
	assertEqualInt(512, st.st_size);

	/* Create an archive with -B; this should be 5120 bytes. */
	r = systemf("echo file | %s -oB > large.cpio 2>large.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "large.err");
	assertEqualInt(0, stat("large.cpio", &st));
	assertEqualInt(5120, st.st_size);
}
