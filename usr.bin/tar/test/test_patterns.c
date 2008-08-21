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

DEFINE_TEST(test_patterns)
{
	int fd, r;
	const char *reffile2 = "test_patterns_2.tgz";
	const char *reffile3 = "test_patterns_3.tgz";
	const char *p;

	/*
	 * Test basic command-line pattern handling.
	 */

	/*
	 * Test 1: Files on the command line that don't get matched
	 * didn't produce an error.
	 *
	 * John Baldwin reported this problem in PR bin/121598
	 */
	fd = open("foo", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	close(fd);
	r = systemf("%s zcfv tar1.tgz foo > tar1a.out 2> tar1a.err", testprog);
	assertEqualInt(r, 0);
	r = systemf("%s zxfv tar1.tgz foo bar > tar1b.out 2> tar1b.err", testprog);
	failure("tar should return non-zero because a file was given on the command line that's not in the archive");
	assert(r != 0);

	/*
	 * Test 2: Check basic matching of full paths that start with /
	 */
	extract_reference_file(reffile2);

	r = systemf("%s tf %s /tmp/foo/bar > tar2a.out 2> tar2a.err",
	    testprog, reffile2);
	assertEqualInt(r, 0);
	p = "/tmp/foo/bar/\n/tmp/foo/bar/baz\n";
	assertFileContents(p, strlen(p), "tar2a.out");
	assertEmptyFile("tar2a.err");

	/*
	 * Test 3 archive has some entries starting with '/' and some not.
	 */
	extract_reference_file(reffile3);

	/* Test 3a:  Pattern tmp/foo/bar should not match /tmp/foo/bar */
	r = systemf("%s xf %s tmp/foo/bar > tar3a.out 2> tar3a.err",
	    testprog, reffile3);
	assert(r != 0);
	assertEmptyFile("tar3a.out");

	/* Test 3b:  Pattern /tmp/foo/baz should not match tmp/foo/baz */
	assertNonEmptyFile("tar3a.err");
	/* Again, with the '/' */
	r = systemf("%s xf %s /tmp/foo/baz > tar3b.out 2> tar3b.err",
	    testprog, reffile3);
	assert(r != 0);
	assertEmptyFile("tar3b.out");
	assertNonEmptyFile("tar3b.err");

	/* Test 3c: ./tmp/foo/bar should not match /tmp/foo/bar */
	r = systemf("%s xf %s ./tmp/foo/bar > tar3c.out 2> tar3c.err",
	    testprog, reffile3);
	assert(r != 0);
	assertEmptyFile("tar3c.out");
	assertNonEmptyFile("tar3c.err");

	/* Test 3d: ./tmp/foo/baz should match tmp/foo/baz */
	r = systemf("%s xf %s ./tmp/foo/baz > tar3d.out 2> tar3d.err",
	    testprog, reffile3);
	assertEqualInt(r, 0);
	assertEmptyFile("tar3d.out");
	assertEmptyFile("tar3d.err");
	assertEqualInt(0, access("tmp/foo/baz/bar", F_OK));
}
