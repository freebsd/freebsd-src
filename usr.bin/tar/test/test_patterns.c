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
	const char *reffile2_out = "test_patterns_2.tgz.out";

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
	extract_reference_file(reffile2_out);

	r = systemf("%s tf %s /tmp/foo/bar > tar2a.out 2> tar2a.err",
	    testprog, reffile2);
	assertEqualInt(r, 0);
	assertEqualFile("tar2a.out", reffile2_out);
	assertEmptyFile("tar2a.err");

	/*
	 *
	 */
}
