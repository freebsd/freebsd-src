/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_passphrase)
{
	const char *reffile = "test_option_passphrase.zip";

	extract_reference_file(reffile);
	failure("--passphrase option is broken");
	assertEqualInt(0, systemf(
	    "%s --passphrase pass1 -xf %s >test.out 2>test.err",
	    testprog, reffile));
	assertFileExists("file1");
	assertTextFileContents("contents of file1.\n", "file1");
	assertFileExists("file2");
	assertTextFileContents("contents of file2.\n", "file2");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
}
