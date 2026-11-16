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

	/* The passphrase must also be applied to archives read via @archive. */
	failure("--passphrase option is broken with @archive");
	assertEqualInt(0, systemf(
	    "%s --passphrase pass1 -cf append.tar @%s "
	    ">append-create.out 2>append-create.err",
	    testprog, reffile));
	assertEmptyFile("append-create.out");
	assertEmptyFile("append-create.err");

	assertMakeDir("append", 0755);
	assertEqualInt(0, systemf(
	    "%s -xf append.tar -C append "
	    ">append-extract.out 2>append-extract.err",
	    testprog));
	assertTextFileContents("contents of file1.\n", "append/file1");
	assertTextFileContents("contents of file2.\n", "append/file2");
	assertEmptyFile("append-extract.out");
	assertEmptyFile("append-extract.err");
}
