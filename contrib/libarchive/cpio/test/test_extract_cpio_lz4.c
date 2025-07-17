/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012,2014 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_extract_cpio_lz4)
{
	const char *reffile = "test_extract.cpio.lz4";
	int f;

	extract_reference_file(reffile);
	f = systemf("%s -it < %s >test.out 2>test.err", testprog, reffile);
	if (f == 0 || canLz4()) {
		assertEqualInt(0, systemf("%s -i < %s >test.out 2>test.err",
		    testprog, reffile));

		assertFileExists("file1");
		assertTextFileContents("contents of file1.\n", "file1");
		assertFileExists("file2");
		assertTextFileContents("contents of file2.\n", "file2");
		assertEmptyFile("test.out");
		assertTextFileContents("1 block\n", "test.err");
	} else {
		skipping("It seems lz4 is not supported on this platform");
	}
}
