/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_0)
{
	FILE *filelist;
	int r;

	assertUmask(0);

	/* Create a few files. */
	assertMakeFile("file1", 0644, "1234567890");
	assertMakeFile("file2", 0644, "1234567890");
	assertMakeFile("file3", 0644, "1234567890");
	assertMakeFile("file4", 0644, "1234567890");

	/* Create a file list of filenames with varying end-of-line. */
	filelist = fopen("filelist", "wb");
	assertEqualInt(fwrite("file1\x0a", 1, 6, filelist), 6);
	assertEqualInt(fwrite("file2\x0d", 1, 6, filelist), 6);
	assertEqualInt(fwrite("file3\x0a\x0d", 1, 7, filelist), 7);
	assertEqualInt(fwrite("file4", 1, 5, filelist), 5);
	fclose(filelist);

	/* Create a file list of null-delimited names. */
	filelist = fopen("filelistNull", "wb");
	assertEqualInt(fwrite("file1\0", 1, 6, filelist), 6);
	assertEqualInt(fwrite("file2\0", 1, 6, filelist), 6);
	assertEqualInt(fwrite("file3\0", 1, 6, filelist), 6);
	assertEqualInt(fwrite("file4", 1, 5, filelist), 5);
	fclose(filelist);

	assertUmask(022);

	/* Pack up using the file list with text line endings. */
	r = systemf("%s -o < filelist > archive 2> stderr1.txt", testprog);
	assertEqualInt(r, 0);

	/* Extract into a new dir. */
	assertMakeDir("copy", 0775);
	assertChdir("copy");
	r = systemf("%s -i < ../archive > stdout3.txt 2> stderr3.txt", testprog);
	assertEqualInt(r, 0);

	/* Verify the files. */
	assertIsReg("file1", 0644);
	assertIsReg("file2", 0644);
	assertIsReg("file3", 0644);
	assertIsReg("file4", 0644);

	assertChdir("..");

	/* Pack up using the file list with nulls. */
	r = systemf("%s -o0 < filelistNull > archiveNull 2> stderr2.txt", testprog);
	assertEqualInt(r, 0);

	/* Extract into a new dir. */
	assertMakeDir("copyNull", 0775);
	assertChdir("copyNull");
	r = systemf("%s -i < ../archiveNull > stdout4.txt 2> stderr4.txt", testprog);
	assertEqualInt(r, 0);

	/* Verify the files. */
	assertIsReg("file1", 0644);
	assertIsReg("file2", 0644);
	assertIsReg("file3", 0644);
	assertIsReg("file4", 0644);
}
