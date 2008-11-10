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

static int
touch(const char *fn)
{
	int fd = open(fn, O_RDWR | O_CREAT | 0644);
	failure("Couldn't create file '%s', fd=%d, errno=%d (%s)\n",
	    fn, fd, errno, strerror(errno));
	if (!assert(fd > 0))
		return (0); /* Failure. */
	close(fd);
	return (1); /* Success */
}

DEFINE_TEST(test_strip_components)
{
	struct stat st;

	assertEqualInt(0, mkdir("d0", 0755));
	assertEqualInt(0, chdir("d0"));
	assertEqualInt(0, mkdir("d1", 0755));
	assertEqualInt(0, mkdir("d1/d2", 0755));
	assertEqualInt(0, mkdir("d1/d2/d3", 0755));
	assertEqualInt(1, touch("d1/d2/f1"));
	assertEqualInt(0, link("d1/d2/f1", "l1"));
	assertEqualInt(0, link("d1/d2/f1", "d1/l2"));
	assertEqualInt(0, symlink("d1/d2/f1", "s1"));
	assertEqualInt(0, symlink("d2/f1", "d1/s2"));
	assertEqualInt(0, chdir(".."));

	assertEqualInt(0, systemf("%s -cf test.tar d0", testprog));

	assertEqualInt(0, mkdir("target", 0755));
	assertEqualInt(0, systemf("%s -x -C target --strip-components 2 "
	    "-f test.tar", testprog));

	failure("d0/ is too short and should not get restored");
	assertEqualInt(-1, lstat("target/d0", &st));
	failure("d0/d1/ is too short and should not get restored");
	assertEqualInt(-1, lstat("target/d1", &st));
	failure("d0/l1 is too short and should not get restored");
	assertEqualInt(-1, lstat("target/l1", &st));
	failure("d0/d1/l2 is a hardlink to file whose name was too short");
	assertEqualInt(-1, lstat("target/l2", &st));
	assertEqualInt(0, lstat("target/s2", &st));
	failure("d0/d1/s2 is a symlink to something that won't be extracted");
	assertEqualInt(-1, stat("target/s2", &st));
	failure("d0/d1/d2 should be extracted");
	assertEqualInt(0, lstat("target/d2", &st));
	failure("d0/d1/d2/f1 is a hardlink to file whose name was too short");
	assertEqualInt(-1, lstat("target/d2/f1", &st));
}
