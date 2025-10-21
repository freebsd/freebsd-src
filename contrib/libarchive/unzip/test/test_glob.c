/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test that the glob works */
DEFINE_TEST(test_glob)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s %s test_*/[ab] >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "test_basic/a");
	assertTextFileContents("contents b\n", "test_basic/b");
	assertFileNotExists("test_basic/c");
	assertFileNotExists("test_basic/CAPS");
#else
	skipping("zlib not available");
#endif
}
