/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Ensure single-file zips work */
DEFINE_TEST(test_singlefile)
{
#ifdef HAVE_LIBZ
	const char *reffile = "test_singlefile.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s %s >test.out 2>test.err", testprog, reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("hello\n", "file.txt");
#else
	skipping("zlib not available");
#endif
}
