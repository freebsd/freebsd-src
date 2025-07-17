/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_C_upper)
{
	int r;

	/*
	 * Create a file on disk.
	 */
	assertMakeFile("file", 0644, NULL);

	/* Create an archive without -C; this should be 512 bytes. */
	r = systemf("echo file | %s -o > small.cpio 2>small.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "small.err");
	assertFileSize("small.cpio", 512);

	/* Create an archive with -C 513; this should be 513 bytes. */
	r = systemf("echo file | %s -o -C 513 > 513.cpio 2>513.err",
		    testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "513.err");
	assertFileSize("513.cpio", 513);

	/* Create an archive with -C 12345; this should be 12345 bytes. */
	r = systemf("echo file | %s -o -C12345 > 12345.cpio 2>12345.err",
		    testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "12345.err");
	assertFileSize("12345.cpio", 12345);

	/* Create an archive with invalid -C request */
	assert(0 != systemf("echo file | %s -o -C > bad.cpio 2>bad.err",
			    testprog));
	assertEmptyFile("bad.cpio");
}
