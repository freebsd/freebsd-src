/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_k)
{
	/*
	 * Create an archive with a couple of different versions of the
	 * same file.
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
	 * combinations of -k
	 */

	/* Test 1: No option */
	assertMakeDir("test1", 0755);
	assertChdir("test1");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar >test.out 2>test.err", testprog));
	assertFileContents("foo3", 4, "foo");
	assertFileContents("bar2", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 2: With -k, we should just get the first versions. */
	assertMakeDir("test2", 0755);
	assertChdir("test2");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar -k >test.out 2>test.err", testprog));
	assertFileContents("foo1", 4, "foo");
	assertFileContents("bar1", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 3: Without -k, existing files should get overwritten */
	assertMakeDir("test3", 0755);
	assertChdir("test3");
	assertMakeFile("bar", 0644, "bar0");
	assertMakeFile("foo", 0644, "foo0");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar >test.out 2>test.err", testprog));
	assertFileContents("foo3", 4, "foo");
	assertFileContents("bar2", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");

	/* Test 4: With -k, existing files should not get overwritten */
	assertMakeDir("test4", 0755);
	assertChdir("test4");
	assertMakeFile("bar", 0644, "bar0");
	assertMakeFile("foo", 0644, "foo0");
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar -k >test.out 2>test.err", testprog));
	assertFileContents("foo0", 4, "foo");
	assertFileContents("bar0", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertChdir("..");
}
