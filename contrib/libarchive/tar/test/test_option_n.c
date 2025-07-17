/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

DEFINE_TEST(test_option_n)
{
	int status;

	assertMakeDir("d1", 0755);
	assertMakeFile("d1/file1", 0644, "d1/file1");

	/* Test 1: -c without -n */
	assertMakeDir("test1", 0755);
	assertChdir("test1");
	assertEqualInt(0,
	    systemf("%s -cf archive.tar -C .. d1 >c.out 2>c.err", testprog));
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertEqualInt(0,
	    systemf("%s -xf archive.tar >x.out 2>x.err", testprog));
	assertEmptyFile("x.out");
	assertEmptyFile("x.err");
	assertFileContents("d1/file1", 8, "d1/file1");
	assertChdir("..");

	/* Test 2: -c with -n */
	assertMakeDir("test2", 0755);
	assertChdir("test2");
	assertEqualInt(0,
	    systemf("%s -cnf archive.tar -C .. d1 >c.out 2>c.err", testprog));
	assertEmptyFile("c.out");
	assertEmptyFile("c.err");
	assertEqualInt(0,
	    systemf("%s -xf archive.tar >x.out 2>x.err", testprog));
	assertEmptyFile("x.out");
	assertEmptyFile("x.err");
	assertIsDir("d1", umasked(0755));
	assertFileNotExists("d1/file1");
	assertChdir("..");

	/*
	 * Create a test archive with the following content:
	 * d1/
	 * d1/file1
	 * d1/file2
	 * file3
	 * d2/file4
	 *
	 * Extracting uses the same code as listing and thus does not
	 * get tested separately. This also covers the
	 * archive_match_set_inclusion_recursion()
	 * API.
	 */
	assertMakeFile("d1/file2", 0644, "d1/file2");
	assertMakeFile("file3", 0644, "file3");
	assertMakeDir("d2", 0755);
	assertMakeFile("d2/file4", 0644, "d2/file4");
	assertEqualInt(0,
	    systemf("%s -cnf partial-archive.tar d1 d1/file1 d1/file2 file3 "
	    "d2/file4 >c.out 2>c.err", testprog));

	/* Test 3: -t without other options */
	assertEqualInt(0,
	    systemf("%s -tf partial-archive.tar >test3.out 2>test3.err",
	    testprog));
	assertEmptyFile("test3.err");
	assertTextFileContents("d1/\n"
			      "d1/file1\n"
			      "d1/file2\n"
			      "file3\n"
			      "d2/file4\n",
			      "test3.out");

	/* Test 4: -t without -n and some entries selected */
	assertEqualInt(0,
	    systemf("%s -tf partial-archive.tar d1 file3 d2/file4 "
	    ">test4.out 2>test4.err", testprog));
	assertEmptyFile("test4.err");
	assertTextFileContents("d1/\n"
			      "d1/file1\n"
			      "d1/file2\n"
			      "file3\n"
			      "d2/file4\n",
			      "test4.out");

	/* Test 5: -t with -n and some entries selected */
	assertEqualInt(0,
	    systemf("%s -tnf partial-archive.tar d1 file3 d2/file4 "
	    ">test5.out 2>test5.err", testprog));
	assertEmptyFile("test5.err");
	assertTextFileContents("d1/\n"
			      "file3\n"
			      "d2/file4\n",
			      "test5.out");

	/* Test 6: -t without -n and non-existent directory selected */
	assertEqualInt(0,
	    systemf("%s -tf partial-archive.tar d2 >test6.out 2>test6.err",
	    testprog));
	assertEmptyFile("test6.err");
	assertTextFileContents("d2/file4\n",
			      "test6.out");

	/* Test 7: -t with -n and non-existent directory selected */
	status = systemf("%s -tnf partial-archive.tar d2 "
	">test7.out 2>test7.err", testprog);
	assert(status);
	assert(status != -1);
#if !defined(_WIN32) || defined(__CYGWIN__)
	assert(WIFEXITED(status));
	assertEqualInt(1, WEXITSTATUS(status));
#endif
	assertNonEmptyFile("test7.err");
	assertEmptyFile("test7.out");
}
