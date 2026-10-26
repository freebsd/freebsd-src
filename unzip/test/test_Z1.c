/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test -Z1 arg - List filenames */
DEFINE_TEST(test_Z1)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -Z1 %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertTextFileContents("test_basic/\ntest_basic/a\ntest_basic/b\ntest_basic/c\ntest_basic/CAPS\n", "test.out");
	assertEmptyFile("test.err");
}

/* Test -Z -1 arg - List filenames */
DEFINE_TEST(test_Z_1)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -Z -1 test_basic >test.out 2>test.err", testprog);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertTextFileContents("test_basic/\ntest_basic/a\ntest_basic/b\ntest_basic/c\ntest_basic/CAPS\n", "test.out");
	assertEmptyFile("test.err");
}
