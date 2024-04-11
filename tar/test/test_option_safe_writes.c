/*-
 * Copyright (c) 2020 Martin Matuska
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
