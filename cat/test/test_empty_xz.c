/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Sebastian Freundt
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_empty_xz)
{
	const char *reffile = "test_empty.xz";
	int f;

	extract_reference_file(reffile);
	f = systemf("%s %s >test.out 2>test.err", testprog, reffile);
	if (f == 0 || canXz()) {
		assertEqualInt(0, f);
		assertEmptyFile("test.out");
		assertEmptyFile("test.err");
	} else {
		skipping("It seems xz is not supported on this platform");
	}
}
