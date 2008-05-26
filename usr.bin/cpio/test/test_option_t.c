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


DEFINE_TEST(test_option_t)
{
	int r;

	/* List reference archive, make sure the TOC is correct. */
	extract_reference_file("test_option_t.cpio");
	r = systemf("%s -it < test_option_t.cpio >t.out 2>t.err", testprog);
	assertEqualInt(r, 0);
	assertFileContents("1 block\n", 8, "t.err");
	extract_reference_file("test_option_t.stdout");
	assertEqualFile("t.out", "test_option_t.stdout");

	/* List reference archive verbosely, make sure the TOC is correct. */
	r = systemf("%s -itv < test_option_t.cpio >tv.out 2>tv.err", testprog);
	assertEqualInt(r, 0);
	assertFileContents("1 block\n", 8, "tv.err");
	extract_reference_file("test_option_tv.stdout");
	assertEqualFile("tv.out", "test_option_tv.stdout");
}
