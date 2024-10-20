/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Sean Purcell
 * All rights reserved.
 */
#include "test.h"

#if !defined(_WIN32) || defined(__CYGWIN__)
#define DEV_NULL "/dev/null"
#else
#define DEV_NULL "NUL"
#endif

DEFINE_TEST(test_stdin)
{
	int f;

	f = systemf("%s <%s >test.out 2>test.err", testprog, DEV_NULL);
	assertEqualInt(0, f);
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
}

