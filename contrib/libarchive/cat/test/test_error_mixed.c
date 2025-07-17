/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Mike Kazantsev
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_error_mixed)
{
	const char *reffile1 = "test_expand.plain";
	const char *reffile2 = "test_expand.error";
	const char *reffile3 = "test_expand.Z";

	assertFileNotExists(reffile2);
	extract_reference_file(reffile1);
	extract_reference_file(reffile3);
	assert(0 != systemf("%s %s %s %s >test.out 2>test.err",
	    testprog, reffile1, reffile2, reffile3));

	assertTextFileContents(
	    "contents of test_expand.plain.\n"
	    "contents of test_expand.Z.\n", "test.out");
	assertNonEmptyFile("test.err");
}
