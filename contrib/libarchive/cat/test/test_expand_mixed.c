/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Mike Kazantsev
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_expand_mixed)
{
	const char *reffile1 = "test_expand.Z";
	const char *reffile2 = "test_expand.plain";

	extract_reference_file(reffile1);
	extract_reference_file(reffile2);
	assertEqualInt(0, systemf("%s %s %s >test.out 2>test.err",
	    testprog, reffile1, reffile2));

	assertTextFileContents(
	    "contents of test_expand.Z.\n"
	    "contents of test_expand.plain.\n", "test.out");
	assertEmptyFile("test.err");
}
