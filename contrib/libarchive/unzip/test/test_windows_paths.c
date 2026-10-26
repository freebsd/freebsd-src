/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Dwij Mehta
 * All rights reserved.
 */
#include "test.h"

/* Verify that native Windows path syntax cannot escape the destination. */
DEFINE_TEST(test_windows_paths)
{
#if defined(_WIN32) && !defined(__CYGWIN__)
	const char *reffile = "test_windows_paths.zip";
	const char *expected_warnings[] = {
		"unzip: skipping insecure entry 'safe/..\\..\\outside.txt'",
		"unzip: skipping insecure entry 'C:/outside.txt'",
		"unzip: skipping insecure entry '\\rooted/rooted.txt'",
		"unzip: skipping insecure symlink 'links/escape' to "
		    "'..\\..\\outside-link.txt'",
		NULL
	};
	int r;

	extract_reference_file(reffile);
	r = systemf("%s -d sandbox %s >test.out 2>test.err", testprog,
	    reffile);
	assertEqualInt(0, r);

	assertFileNotExists("outside.txt");
	assertFileNotExists("sandbox/C:/outside.txt");
	assertFileNotExists("sandbox/\\rooted/rooted.txt");
	assertFileNotExists("sandbox/links/escape");
	assertFileContainsLinesAnyOrder("test.err", expected_warnings);
	assertTextFileContents("ok\n", "sandbox/safe.txt");
#else
	skipping("Native Windows path semantics required");
#endif
}
