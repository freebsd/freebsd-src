/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_missing_file)
{
	const char * invalid_stderr[] = { "INTERNAL ERROR", NULL };
	assertMakeFile("file1", 0644, "file1");
	assertMakeFile("file2", 0644, "file2");
	assert(0 == systemf("%s -cf archive.tar file1 file2 2>stderr1", testprog));
	assertEmptyFile("stderr1");
	assert(0 != systemf("%s -cf archive.tar file1 file2 file3 2>stderr2", testprog));
	assertFileContainsNoInvalidStrings("stderr2", invalid_stderr);
	assert(0 != systemf("%s -cf archive.tar 2>stderr3", testprog));
	assertFileContainsNoInvalidStrings("stderr3", invalid_stderr);
	assert(0 != systemf("%s -cf archive.tar file3 file4 2>stderr4", testprog));
	assertFileContainsNoInvalidStrings("stderr4", invalid_stderr);
}
