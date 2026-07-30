/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Tobias Stoeckmann
 * All rights reserved.
 */
#include "test.h"

/* Test f arg - don't create new files or overwrite newer files */
DEFINE_TEST(test_f)
{
	const char *reffile1 = "test_f_new.zip";
	const char *reffile2 = "test_f_old.zip";
	int r;

	extract_reference_file(reffile1);
	extract_reference_file(reffile2);

	/* First run with -f should not extract: test.txt does not exist */
	r = systemf("%s -f %s >test.out 2>test.err", testprog, reffile1);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertFileNotExists("test.txt");

	/* Extract archive */
	r = systemf("%s %s >test.out 2>test.err", testprog, reffile1);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertTextFileContents("world\n", "test.txt");

	/* Second run with -f should not overwrite newer file */
	r = systemf("%s -f %s >test.out 2>test.err", testprog, reffile2);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertTextFileContents("world\n", "test.txt");
}
