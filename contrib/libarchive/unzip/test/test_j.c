/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test j arg - don't make directories */
DEFINE_TEST(test_j)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -j %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "a");
	assertTextFileContents("contents b\n", "b");
	assertTextFileContents("contents c\n", "c");
	assertTextFileContents("contents CAPS\n", "CAPS");
#else
	skipping("zlib not available");
#endif
}
