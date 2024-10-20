/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test n arg - don't overwrite existing files */
DEFINE_TEST(test_n)
{
	const char *reffile = "test_basic.zip";
	int r;

	assertMakeDir("test_basic", 0755);
	assertMakeFile("test_basic/a", 0644, "orig a\n");
	assertMakeFile("test_basic/b", 0644, "orig b\n");

	extract_reference_file(reffile);
	r = systemf("%s -n %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("orig a\n", "test_basic/a");
	assertTextFileContents("orig b\n", "test_basic/b");
	assertTextFileContents("contents c\n", "test_basic/c");
	assertTextFileContents("contents CAPS\n", "test_basic/CAPS");
}
