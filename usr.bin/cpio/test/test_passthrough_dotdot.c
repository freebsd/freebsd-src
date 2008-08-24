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
 * Verify that "cpio -p .." works.
 */

DEFINE_TEST(test_passthrough_dotdot)
{
	struct stat st;
	int fd, r;
	int filelist;
	int oldumask;

	oldumask = umask(0);

	/*
	 * Create an assortment of files on disk.
	 */
	filelist = open("filelist", O_CREAT | O_WRONLY, 0644);

	/* Directory. */
	assertEqualInt(0, mkdir("dir", 0755));
	assertEqualInt(0, chdir("dir"));

	write(filelist, ".\n", 2);

	/* File with 10 bytes content. */
	fd = open("file", O_CREAT | O_WRONLY, 0642);
	assert(fd >= 0);
	assertEqualInt(10, write(fd, "123456789", 10));
	close(fd);
	write(filelist, "file\n", 5);

	/* All done. */
	close(filelist);


	/*
	 * Use cpio passthrough mode to copy files to another directory.
	 */
	r = systemf("%s -pdvm .. <../filelist >../stdout 2>../stderr",
	    testprog);
	failure("Error invoking %s -pd ..", testprog);
	assertEqualInt(r, 0);

	assertEqualInt(0, chdir(".."));

	/* Verify stderr and stdout. */
	assertFileContents("../file\n", 8, "stderr");
	assertEmptyFile("stdout");

	/* Regular file. */
	r = lstat("file", &st);
	failure("Failed to stat file, errno=%d", errno);
	assertEqualInt(r, 0);
	if (r == 0) {
		assert(S_ISREG(st.st_mode));
		assertEqualInt(0642, st.st_mode & 0777);
		assertEqualInt(10, st.st_size);
		assertEqualInt(1, st.st_nlink);
	}

	umask(oldumask);
}
