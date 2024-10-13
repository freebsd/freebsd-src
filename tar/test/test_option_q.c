/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_q)
{
	int r;

	/*
	 * Create an archive with several different versions of the
	 * same files.  By default, the last version will overwrite
	 * any earlier versions.  The -q/--fast-read option will
	 * stop early, so we can verify -q/--fast-read by seeing
	 * which version of each file actually ended up being
	 * extracted.  This also exercises -r mode, since that's
	 * what we use to build up the test archive.
	 */

	assertMakeFile("foo", 0644, "foo1");

	assertEqualInt(0, systemf("%s -cf archive.tar foo", testprog));

	assertMakeFile("foo", 0644, "foo2");

	assertEqualInt(0, systemf("%s -rf archive.tar foo", testprog));

	assertMakeFile("bar", 0644, "bar1");

	assertEqualInt(0, systemf("%s -rf archive.tar bar", testprog));

	assertMakeFile("foo", 0644, "foo3");

	assertEqualInt(0, systemf("%s -rf archive.tar foo", testprog));

	assertMakeFile("bar", 0644, "bar2");

	assertEqualInt(0, systemf("%s -rf archive.tar bar", testprog));

	/*
	 * Now, try extracting from the test archive with various
	 * combinations of -q.
	 */

	/* Test 1: -q foo should only extract the first foo. */
	assertMakeDir("test1", 0755);
	assertChdir("test1");
	r = systemf("%s -xf ../archive.tar -q foo >test.out 2>test.err",
	    testprog);
	failure("Fatal error trying to use -q option");
	if (!assertEqualInt(0, r))
		return;

	assertFileContents("foo1", 4, "foo");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 2: -q foo bar should extract up to the first bar. */
	assertMakeDir("test2", 0755);
	assertChdir("test2");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar -q foo bar >test.out 2>test.err", testprog));
	assertFileContents("foo2", 4, "foo");
	assertFileContents("bar1", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 3: Same as test 2, but use --fast-read spelling. */
	assertMakeDir("test3", 0755);
	assertChdir("test3");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar --fast-read foo bar >test.out 2>test.err", testprog));
	assertFileContents("foo2", 4, "foo");
	assertFileContents("bar1", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 4: Without -q, should extract everything. */
	assertMakeDir("test4", 0755);
	assertChdir("test4");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar foo bar >test.out 2>test.err", testprog));
	assertFileContents("foo3", 4, "foo");
	assertFileContents("bar2", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");
}
