/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Sean Purcell
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_empty_zstd)
{
	const char *reffile = "test_empty.zst";
	int f;

	extract_reference_file(reffile);
	f = systemf("%s %s >test.out 2>test.err", testprog, reffile);
	if (f == 0 || canZstd()) {
		assertEqualInt(0, f);
		assertEmptyFile("test.out");
		assertEmptyFile("test.err");
	} else {
		skipping("It seems zstd is not supported on this platform");
	}
}
