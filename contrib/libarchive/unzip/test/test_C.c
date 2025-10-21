/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test C arg - match case-insensitive */
DEFINE_TEST(test_C)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -C %s test_basic/caps >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("contents CAPS\n", "test_basic/CAPS");
#else
	skipping("zlib not available");
#endif
}
