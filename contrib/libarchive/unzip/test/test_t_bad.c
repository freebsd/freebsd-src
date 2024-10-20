/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test t arg - Test zip contents, but fail! */
DEFINE_TEST(test_t_bad)
{
	const char *reffile = "test_t_bad.zip";
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -t %s >test.out 2>test.err", testprog, reffile);
	assert(r != 0);
	assertNonEmptyFile("test.out");
	assertNonEmptyFile("test.err");
}
