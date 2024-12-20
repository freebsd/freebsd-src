/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_d)
{
	int r;

	/*
	 * Create a file in a directory.
	 */
	assertMakeDir("dir", 0755);
	assertMakeFile("dir/file", 0644, NULL);

	/* Create an archive. */
	r = systemf("echo dir/file | %s -o > archive.cpio 2>archive.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "archive.err");
	assertFileSize("archive.cpio", 512);

	/* Dearchive without -d, this should fail. */
	assertMakeDir("without-d", 0755);
	assertChdir("without-d");
	r = systemf("%s -i < ../archive.cpio >out 2>err", testprog);
	assert(r != 0);
	assertEmptyFile("out");
	/* And the file should not be restored. */
	assertFileNotExists("dir/file");

	/* Dearchive with -d, this should succeed. */
	assertChdir("..");
	assertMakeDir("with-d", 0755);
	assertChdir("with-d");
	r = systemf("%s -id < ../archive.cpio >out 2>err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("out");
	assertTextFileContents("1 block\n", "err");
	/* And the file should be restored. */
	assertFileExists("dir/file");
}
