/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * Verify that "cpio -p .." works.
 */

DEFINE_TEST(test_passthrough_dotdot)
{
	int r;
	FILE *filelist;

	assertUmask(0);

	/*
	 * Create an assortment of files on disk.
	 */
	filelist = fopen("filelist", "w");

	/* Directory. */
	assertMakeDir("dir", 0755);
	assertChdir("dir");

	fprintf(filelist, ".\n");

	/* File with 10 bytes content. */
	assertMakeFile("file", 0642, "1234567890");
	fprintf(filelist, "file\n");

	/* All done. */
	fclose(filelist);


	/*
	 * Use cpio passthrough mode to copy files to another directory.
	 */
	r = systemf("%s -pdvm .. <../filelist >../stdout 2>../stderr",
	    testprog);
	failure("Error invoking %s -pd ..", testprog);
	assertEqualInt(r, 0);

	assertChdir("..");

	/* Verify stderr and stdout. */
	assertTextFileContents("../.\n../file\n1 block\n", "stderr");
	assertEmptyFile("stdout");

	/* Regular file. */
	assertIsReg("file", 0642);
	assertFileSize("file", 10);
	assertFileNLinks("file", 1);
}
