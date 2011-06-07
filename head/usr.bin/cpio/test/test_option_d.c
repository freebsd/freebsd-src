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


DEFINE_TEST(test_option_d)
{
	struct stat st;
	int r, fd;

	/*
	 * Create a file in a directory.
	 */
	assertEqualInt(0, mkdir("dir", 0755));
	fd = open("dir/file", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	close(fd);

	/* Create an archive. */
	r = systemf("echo dir/file | %s -o > archive.cpio 2>archive.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "archive.err");
	assertEqualInt(0, stat("archive.cpio", &st));
	assertEqualInt(512, st.st_size);

	/* Dearchive without -d, this should fail. */
	assertEqualInt(0, mkdir("without-d", 0755));
	assertEqualInt(0, chdir("without-d"));
	r = systemf("%s -i < ../archive.cpio >out 2>err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("out");
	/* And the file should not be restored. */
	assert(0 != stat("dir/file", &st));

	/* Dearchive with -d, this should succeed. */
	assertEqualInt(0, chdir(".."));
	assertEqualInt(0, mkdir("with-d", 0755));
	assertEqualInt(0, chdir("with-d"));
	r = systemf("%s -id < ../archive.cpio >out 2>err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("out");
	assertTextFileContents("1 block\n", "err");
	/* And the file should be restored. */
	assertEqualInt(0, stat("dir/file", &st));
}
