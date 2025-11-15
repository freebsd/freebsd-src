/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test p arg - Print to stdout */
DEFINE_TEST(test_p)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_basic.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -p %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertTextFileContents("contents a\ncontents b\ncontents c\ncontents CAPS\n", "test.out");
	assertEmptyFile("test.err");
#else
	skipping("zlib not available");
#endif
}
