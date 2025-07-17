/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test double dash arg - swallow "--" and use next argument as file name  */
DEFINE_TEST(test_doubledash)
{
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -- %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "test_basic/a");
	assertTextFileContents("contents b\n", "test_basic/b");
	assertTextFileContents("contents c\n", "test_basic/c");
	assertTextFileContents("contents CAPS\n", "test_basic/CAPS");
}
