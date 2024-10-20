/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test x arg with single exclude path */
DEFINE_TEST(test_x_single)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s %s -x test_basic/c >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "test_basic/a");
	assertTextFileContents("contents b\n", "test_basic/b");
	assertFileNotExists("test_basic/c");
	assertTextFileContents("contents CAPS\n", "test_basic/CAPS");
}

/* Test x arg with multiple exclude paths */
DEFINE_TEST(test_x_multiple)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s %s -x test_basic/c test_basic/b >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "test_basic/a");
	assertFileNotExists("test_basic/b");
	assertFileNotExists("test_basic/c");
	assertTextFileContents("contents CAPS\n", "test_basic/CAPS");
}

/* Test x arg with multiple exclude paths and a d arg afterwards */
DEFINE_TEST(test_x_multiple_with_d)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s %s -x test_basic/c test_basic/b -d foobar >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "foobar/test_basic/a");
	assertFileNotExists("foobar/test_basic/b");
	assertFileNotExists("foobar/test_basic/c");
	assertTextFileContents("contents CAPS\n", "foobar/test_basic/CAPS");
}
