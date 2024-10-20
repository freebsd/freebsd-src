/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_missing_file)
{
	int r;

	assertMakeFile("file1", 0644, "file1");
	assertMakeFile("file2", 0644, "file2");

	assertMakeFile("filelist1", 0644, "file1\nfile2\n");
	r = systemf("%s -o <filelist1 >stdout1 2>stderr1", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "stderr1");

	assertMakeFile("filelist2", 0644, "file1\nfile2\nfile3\n");
	r = systemf("%s -o <filelist2 >stdout2 2>stderr2", testprog);
	assert(r != 0);

	assertMakeFile("filelist3", 0644, "");
	r = systemf("%s -o <filelist3 >stdout3 2>stderr3", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "stderr3");

	assertMakeFile("filelist4", 0644, "file3\n");
	r = systemf("%s -o <filelist4 >stdout4 2>stderr4", testprog);
	assert(r != 0);
}
