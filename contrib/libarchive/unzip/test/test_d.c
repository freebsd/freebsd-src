/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test d arg - extract to target dir - before zipfile argument */
DEFINE_TEST(test_d_before_zipfile)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -d foobar %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "foobar/test_basic/a");
	assertTextFileContents("contents b\n", "foobar/test_basic/b");
	assertTextFileContents("contents c\n", "foobar/test_basic/c");
	assertTextFileContents("contents CAPS\n", "foobar/test_basic/CAPS");
}

/* Test d arg - extract to target dir - after zipfile argument */
DEFINE_TEST(test_d_after_zipfile)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s %s -d foobar >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "foobar/test_basic/a");
	assertTextFileContents("contents b\n", "foobar/test_basic/b");
	assertTextFileContents("contents c\n", "foobar/test_basic/c");
	assertTextFileContents("contents CAPS\n", "foobar/test_basic/CAPS");
}
