/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Mike Kazantsev
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_expand_Z)
{
	const char *reffile = "test_expand.Z";

	extract_reference_file(reffile);
	assertEqualInt(0, systemf("%s %s >test.out 2>test.err", testprog, reffile));

	assertTextFileContents("contents of test_expand.Z.\n", "test.out");
	assertEmptyFile("test.err");
}
