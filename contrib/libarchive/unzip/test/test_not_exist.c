/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Adrian Vovk
 * All rights reserved.
 */
#include "test.h"

/* Test non existent file */
DEFINE_TEST(test_not_exist)
{
	int r;
	r = systemf("%s nonexist.zip >test.out 2>test.err", testprog);
	assert(r != 0);
	assertEmptyFile("test.out");
	assertNonEmptyFile("test.err");
}
