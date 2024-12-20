/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2017 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * Test that --version option works and generates reasonable output.
 */

DEFINE_TEST(test_version)
{
	assertVersion(testprog, "bsdunzip");
}
