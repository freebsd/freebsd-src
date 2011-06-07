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


DEFINE_TEST(test_option_m)
{
	struct stat st;
	int r;
	time_t now;

	/*
	 * The reference archive has one file with an mtime in 1970, 1
	 * second after the start of the epoch.
	 */

	/* Restored without -m, the result should have a current mtime. */
	assertEqualInt(0, mkdir("without-m", 0755));
	assertEqualInt(0, chdir("without-m"));
	extract_reference_file("test_option_m.cpio");
	r = systemf("%s -i < test_option_m.cpio >out 2>err", testprog);
	now = time(NULL);
	assertEqualInt(r, 0);
	assertEmptyFile("out");
	assertTextFileContents("1 block\n", "err");
	assertEqualInt(0, stat("file", &st));
	/* Should have been created within the last few seconds. */
	assert(st.st_mtime <= now);
	assert(st.st_mtime > now - 5);

	/* With -m, it should have an mtime in 1970. */
	assertEqualInt(0, chdir(".."));
	assertEqualInt(0, mkdir("with-m", 0755));
	assertEqualInt(0, chdir("with-m"));
	extract_reference_file("test_option_m.cpio");
	r = systemf("%s -im < test_option_m.cpio >out 2>err", testprog);
	now = time(NULL);
	assertEqualInt(r, 0);
	assertEmptyFile("out");
	assertTextFileContents("1 block\n", "err");
	assertEqualInt(0, stat("file", &st));
	/*
	 * mtime in reference archive is '1' == 1 second after
	 * midnight Jan 1, 1970 UTC.
	 */
	assertEqualInt(1, st.st_mtime);
}
