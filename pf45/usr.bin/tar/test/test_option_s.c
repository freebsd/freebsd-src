/*-
 * Copyright (c) 2003-2008 Tim Kientzle
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
__FBSDID("$FreeBSD$");

static int
mkfile(const char *fn, const char *contents)
{
	int fd = open(fn, O_RDWR | O_CREAT, 0644);
	failure("Couldn't create file '%s', fd=%d, errno=%d (%s)\n",
	    fn, fd, errno, strerror(errno));
	if (!assert(fd > 0))
		return (1); /* Failure. */
	if (contents != NULL)
		assertEqualInt(strlen(contents),
		    write(fd, contents, strlen(contents)));
	assertEqualInt(0, close(fd));
	return (0); /* Success */
}

DEFINE_TEST(test_option_s)
{
	struct stat st;

	/* Create a sample file hierarchy. */
	assertEqualInt(0, mkdir("in", 0755));
	assertEqualInt(0, mkdir("in/d1", 0755));
	assertEqualInt(0, mkfile("in/d1/foo", "foo"));
	assertEqualInt(0, mkfile("in/d1/bar", "bar"));

	/* Does bsdtar support -s option ? */
	systemf("%s -cf - -s /foo/bar/ in/d1/foo > NUL 2> check.err",
	    testprog);
	assertEqualInt(0, stat("check.err", &st));
	if (st.st_size != 0) {
		skipping("bsdtar does not support -s option on this platform");
		return;
	}

	/*
	 * Test 1: Filename substitution when creating archives.
	 */
	assertEqualInt(0, mkdir("test1", 0755));
	systemf("%s -cf - -s /foo/bar/ in/d1/foo | %s -xf - -C test1",
	    testprog, testprog);
	assertFileContents("foo", 3, "test1/in/d1/bar");
	systemf("%s -cf - -s /d1/d2/ in/d1/foo | %s -xf - -C test1",
	    testprog, testprog);
	assertFileContents("foo", 3, "test1/in/d2/foo");


	/*
	 * Test 2: Basic substitution when extracting archive.
	 */
	assertEqualInt(0, mkdir("test2", 0755));
	systemf("%s -cf - in/d1/foo | %s -xf - -s /foo/bar/ -C test2",
	    testprog, testprog);
	assertFileContents("foo", 3, "test2/in/d1/bar");

	/*
	 * Test 3: Files with empty names shouldn't be archived.
	 */
	systemf("%s -cf - -s ,in/d1/foo,, in/d1/foo | %s -tvf - > in.lst",
	    testprog, testprog);
	assertEmptyFile("in.lst");

	/*
	 * Test 4: Multiple substitutions when extracting archive.
	 */
	assertEqualInt(0, mkdir("test4", 0755));
	systemf("%s -cf - in/d1/foo in/d1/bar | %s -xf - -s /foo/bar/ -s }bar}baz} -C test4",
	    testprog, testprog);
	assertFileContents("foo", 3, "test4/in/d1/bar");
	assertFileContents("bar", 3, "test4/in/d1/baz");

	/*
	 * Test 5: Name-switching substitutions when extracting archive.
	 */
	assertEqualInt(0, mkdir("test5", 0755));
	systemf("%s -cf - in/d1/foo in/d1/bar | %s -xf - -s /foo/bar/ -s }bar}foo} -C test5",
	    testprog, testprog);
	assertFileContents("foo", 3, "test5/in/d1/bar");
	assertFileContents("bar", 3, "test5/in/d1/foo");
}
