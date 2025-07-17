/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_extract_tar_Z)
{
	const char *reffile = "test_extract.tar.Z";

	extract_reference_file(reffile);
	assertEqualInt(0, systemf("%s -xf %s >test.out 2>test.err",
	    testprog, reffile));

	assertFileExists("file1");
	assertTextFileContents("contents of file1.\n", "file1");
	assertFileExists("file2");
	assertTextFileContents("contents of file2.\n", "file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
}
