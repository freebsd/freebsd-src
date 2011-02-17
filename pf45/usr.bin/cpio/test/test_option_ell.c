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

/*
 * This is called "test_option_ell" instead of "test_option_l" to
 * avoid any conflicts with "test_option_L" on case-insensitive
 * filesystems.
 */

DEFINE_TEST(test_option_ell)
{
	struct stat st, st2;
	int fd;
	int r;

	/* Create a file. */
	fd = open("f", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(1, write(fd, "a", 1));
	close(fd);

	/* Stat it. */
	assertEqualInt(0, stat("f", &st));

	/* Copy the file to the "copy" dir. */
	r = systemf("echo f | %s -pd copy >copy.out 2>copy.err",
	    testprog);
	assertEqualInt(r, 0);

	/* Check that the copy is a true copy and not a link. */
	assertEqualInt(0, stat("copy/f", &st2));
	assert(st2.st_ino != st.st_ino);

	/* Copy the file to the "link" dir with the -l option. */
	r = systemf("echo f | %s -pld link >link.out 2>link.err",
	    testprog);
	assertEqualInt(r, 0);

	/* Check that this is a link and not a copy. */
	assertEqualInt(0, stat("link/f", &st2));
	assert(st2.st_ino == st.st_ino);
}
