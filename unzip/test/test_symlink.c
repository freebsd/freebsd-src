/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Tobias Stoeckmann
 * All rights reserved.
 */
#include "test.h"

/* Test symlinks */
DEFINE_TEST(test_symlink)
{
	const char *reffile1 = "test_symlink_1.zip";
	const char *reffile2 = "test_symlink_2.zip";
	int r;

	if (!canSymlink()) {
		skipping("System cannot create symlinks.");
		return;
	}

	assertUmask(0);
	extract_reference_file(reffile1);
	extract_reference_file(reffile2);

	/* Extract first archive */
	r = systemf("%s %s >test.out 2>test.err", testprog, reffile1);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertNonEmptyFile("test.err");
	assertIsDir("dir", 0755);
	assertIsSymlink("dlink", "dir", 1);
	assertIsReg("file.txt", 0644);
	assertIsSymlink("flink", "file.txt", 0);
	assertFileNotExists("link.insecure");

	/* Create dangling symlinks by deleting targets */
	rmdir("dir");
	unlink("file.txt");
	assertFileNotExists("dir");
	assertFileNotExists("file.txt");
	assertIsSymlink("dlink", "dir", 1);
	assertIsSymlink("flink", "file.txt", 0);

	/* Extract second archive, overwriting dangling symlinks */
	r = systemf("%s -o %s >test.out 2>test.err", testprog, reffile2);
	assertEqualInt(0, r);
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertFileNotExists("dir");
	assertIsDir("dlink", 0755);
	assertTextFileContents("hello world\n", "dlink/file.txt");
	assertIsReg("flink", 0644);
}
