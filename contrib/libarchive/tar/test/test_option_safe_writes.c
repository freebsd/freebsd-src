/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Martin Matuska
 * All rights reserved.
 */
#include "test.h"

DEFINE_TEST(test_option_safe_writes)
{
	/* Create files */
	assertMakeDir("in", 0755);
	assertEqualInt(0, chdir("in"));
	assertMakeFile("f", 0644, "a");
	assertMakeFile("fh", 0644, "b");
	assertMakeFile("d", 0644, "c");
	assertMakeFile("fs", 0644, "d");
	assertMakeFile("ds", 0644, "e");
	assertEqualInt(0, chdir(".."));

	/* Tar files up */
	assertEqualInt(0,
	    systemf("%s -c -C in -f t.tar f fh d fs ds "
	    ">pack.out 2>pack.err", testprog));

        /* Verify that nothing went to stdout or stderr. */
        assertEmptyFile("pack.err");
        assertEmptyFile("pack.out");

	/* Create various objects */
	assertMakeDir("out", 0755);
	assertEqualInt(0, chdir("out"));
	assertMakeFile("f", 0644, "a");
	assertMakeHardlink("fh", "f");
	assertMakeDir("d", 0755);
	if (canSymlink()) {
		assertMakeSymlink("fs", "f", 0);
		assertMakeSymlink("ds", "d", 1);
	}
	assertEqualInt(0, chdir(".."));

	/* Extract created archive with safe writes */
	assertEqualInt(0,
	    systemf("%s -x -C out --safe-writes -f t.tar "
	    ">unpack.out 2>unpack.err", testprog));

        /* Verify that nothing went to stdout or stderr. */
        assertEmptyFile("unpack.err");
        assertEmptyFile("unpack.out");

	/* Verify that files were overwritten properly */
	assertEqualInt(0, chdir("out"));
	assertTextFileContents("a","f");
	assertTextFileContents("b","fh");
	assertTextFileContents("c","d");
	assertTextFileContents("d","fs");
	assertTextFileContents("e","ds");
}
