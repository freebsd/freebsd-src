/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test q arg - Quiet */
DEFINE_TEST(test_q)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -q %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents a\n", "test_basic/a");
	assertTextFileContents("contents b\n", "test_basic/b");
	assertTextFileContents("contents c\n", "test_basic/c");
	assertTextFileContents("contents CAPS\n", "test_basic/CAPS");
#else
	skipping("zlib not available");
#endif
}
