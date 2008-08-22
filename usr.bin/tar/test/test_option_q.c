/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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

DEFINE_TEST(test_option_q)
{
	int fd;

	/*
	 * Create an archive with several different versions of the
	 * same files.  By default, the last version will overwrite
	 * any earlier versions.  The -q/--fast-read option will
	 * stop early, so we can verify -q/--fast-read by seeing
	 * which version of each file actually ended up being
	 * extracted.  This also exercises -r mode, since that's
	 * what we use to build up the test archive.
	 */

	fd = open("foo", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(4, write(fd, "foo1", 4));
	close(fd);

	assertEqualInt(0, systemf("%s -cf archive.tar foo", testprog));

	fd = open("foo", O_TRUNC | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(4, write(fd, "foo2", 4));
	close(fd);

	assertEqualInt(0, systemf("%s -rf archive.tar foo", testprog));

	fd = open("bar", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(4, write(fd, "bar1", 4));
	close(fd);

	assertEqualInt(0, systemf("%s -rf archive.tar bar", testprog));

	fd = open("foo", O_TRUNC | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(4, write(fd, "foo3", 4));
	close(fd);

	assertEqualInt(0, systemf("%s -rf archive.tar foo", testprog));

	fd = open("bar", O_TRUNC | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(4, write(fd, "bar2", 4));
	close(fd);

	assertEqualInt(0, systemf("%s -rf archive.tar bar", testprog));

	/*
	 * Now, try extracting from the test archive with various
	 * combinations of -q.
	 */

	/* Test 1: -q foo should only extract the first foo. */
	assertEqualInt(0, mkdir("test1", 0755));
	assertEqualInt(0, chdir("test1"));
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar -q foo >test.out 2>test.err",
		testprog));
	assertFileContents("foo1", 4, "foo");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertEqualInt(0, chdir(".."));

	/* Test 2: -q foo bar should extract up to the first bar. */
	assertEqualInt(0, mkdir("test2", 0755));
	assertEqualInt(0, chdir("test2"));
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar -q foo bar >test.out 2>test.err", testprog));
	assertFileContents("foo2", 4, "foo");
	assertFileContents("bar1", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertEqualInt(0, chdir(".."));

	/* Test 3: Same as test 2, but use --fast-read spelling. */
	assertEqualInt(0, mkdir("test3", 0755));
	assertEqualInt(0, chdir("test3"));
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar --fast-read foo bar >test.out 2>test.err", testprog));
	assertFileContents("foo2", 4, "foo");
	assertFileContents("bar1", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertEqualInt(0, chdir(".."));

	/* Test 4: Without -q, should extract everything. */
	assertEqualInt(0, mkdir("test4", 0755));
	assertEqualInt(0, chdir("test4"));
	assertEqualInt(0,
	    systemf("%s -xf ../archive.tar foo bar >test.out 2>test.err", testprog));
	assertFileContents("foo3", 4, "foo");
	assertFileContents("bar2", 4, "bar");
	assertEmptyFile("test.out");
	assertEmptyFile("test.err");
	assertEqualInt(0, chdir(".."));
}
