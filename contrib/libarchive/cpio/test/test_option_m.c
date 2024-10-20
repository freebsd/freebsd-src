/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_m)
{
	int r;

	/*
	 * The reference archive has one file with an mtime in 1970, 1
	 * second after the start of the epoch.
	 */

	/* Restored without -m, the result should have a current mtime. */
	assertMakeDir("without-m", 0755);
	assertChdir("without-m");
	extract_reference_file("test_option_m.cpio");
	r = systemf("%s --no-preserve-owner -i < test_option_m.cpio >out 2>err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("out");
	assertTextFileContents("1 block\n", "err");
	/* Should have been created within the last few seconds. */
	assertFileMtimeRecent("file");

	/* With -m, it should have an mtime in 1970. */
	assertChdir("..");
	assertMakeDir("with-m", 0755);
	assertChdir("with-m");
	extract_reference_file("test_option_m.cpio");
	r = systemf("%s --no-preserve-owner -im < test_option_m.cpio >out 2>err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("out");
	assertTextFileContents("1 block\n", "err");
	/*
	 * mtime in reference archive is '1' == 1 second after
	 * midnight Jan 1, 1970 UTC.
	 */
	assertFileMtime("file", 1, 0);
}
