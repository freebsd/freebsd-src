/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_U_upper)
{
	int r;

	assertMakeFile("file1", 0644, "file1");
	assertMakeDir("d1", 0755);
	assertMakeFile("d1/file1", 0644, "d1/file1");
	assertEqualInt(0, systemf("%s -cf archive.tar file1 d1/file1", testprog));

	/*
	 * bsdtar's man page used to claim that -x without -U would
	 * not break hard links.  This was and is nonsense.  The first
	 * two tests here simply verify that existing hard links get
	 * broken regardless.
	 */

	/* Test 1: -x without -U */
	assertMakeDir("test1", 0755);
	assertChdir("test1");
	assertMakeFile("file1", 0644, "file1new");
	assertMakeHardlink("file2", "file1");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar >test.out 2>test.err", testprog));
	assertFileContents("file1", 5, "file1");
	assertFileContents("file1new", 8, "file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");


	/* Test 2: -x with -U */
	assertMakeDir("test2", 0755);
	assertChdir("test2");
	assertMakeFile("file1", 0644, "file1new");
	assertMakeHardlink("file2", "file1");
	assertEqualInt(0,
	    systemf("%s -xUf ../archive.tar >test.out 2>test.err", testprog));
	assertFileContents("file1", 5, "file1");
	assertFileContents("file1new", 8, "file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/*
	 * -U does make a difference in how bsdtar handles unwanted symlinks,
	 * though.  It interacts with -P.
	 */
	if (!canSymlink())
		return;

	/* Test 3: Intermediate dir symlink causes error by default */
	assertMakeDir("test3", 0755);
	assertChdir("test3");
	assertMakeDir("realDir", 0755);
	assertMakeSymlink("d1", "realDir", 1);
	r = systemf("%s -xf ../archive.tar d1/file1 >test.out 2>test.err", testprog);
	assert(r != 0);
	assertIsSymlink("d1", "realDir", 1);
	assertFileNotExists("d1/file1");
	assertEmptyFile("test.out");
	assertNonEmptyFile("test.err");
	assertChdir("..");

	/* Test 4: Intermediate dir symlink gets removed with -U */
	assertMakeDir("test4", 0755);
	assertChdir("test4");
	assertMakeDir("realDir", 0755);
	assertMakeSymlink("d1", "realDir", 1);
	assertEqualInt(0,
	    systemf("%s -xUf ../archive.tar >test.out 2>test.err", testprog));
	assertIsDir("d1", -1);
	assertFileContents("d1/file1", 8, "d1/file1");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 5: Intermediate dir symlink is followed with -P */
	assertMakeDir("test5", 0755);
	assertChdir("test5");
	assertMakeDir("realDir", 0755);
	assertMakeSymlink("d1", "realDir", 1);
	assertEqualInt(0,
	    systemf("%s -xPf ../archive.tar d1/file1 >test.out 2>test.err", testprog));
	assertIsSymlink("d1", "realDir", 1);
	assertFileContents("d1/file1", 8, "d1/file1");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 6: Intermediate dir symlink is followed with -PU */
	assertMakeDir("test6", 0755);
	assertChdir("test6");
	assertMakeDir("realDir", 0755);
	assertMakeSymlink("d1", "realDir", 1);
	assertEqualInt(0,
	    systemf("%s -xPUf ../archive.tar d1/file1 >test.out 2>test.err", testprog));
	assertIsSymlink("d1", "realDir", 1);
	assertFileContents("d1/file1", 8, "d1/file1");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 7: Final file symlink replaced by default */
	assertMakeDir("test7", 0755);
	assertChdir("test7");
	assertMakeDir("d1", 0755);
	assertMakeFile("d1/realfile1", 0644, "realfile1");
	assertMakeSymlink("d1/file1", "d1/realfile1", 0);
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar d1/file1 >test.out 2>test.err", testprog));
	assertIsReg("d1/file1", umasked(0644));
	assertFileContents("d1/file1", 8, "d1/file1");
	assertFileContents("realfile1", 9, "d1/realfile1");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 8: Final file symlink replaced with -PU */
	assertMakeDir("test8", 0755);
	assertChdir("test8");
	assertMakeDir("d1", 0755);
	assertMakeFile("d1/realfile1", 0644, "realfile1");
	assertMakeSymlink("d1/file1", "d1/realfile1", 0);
	assertEqualInt(0,
	    systemf("%s -xPUf ../archive.tar d1/file1 >test.out 2>test.err", testprog));
	assertIsReg("d1/file1", umasked(0644));
	assertFileContents("d1/file1", 8, "d1/file1");
	assertFileContents("realfile1", 9, "d1/realfile1");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");
}
