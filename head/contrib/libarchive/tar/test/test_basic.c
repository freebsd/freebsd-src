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


static void
basic_tar(const char *target, const char *pack_options,
    const char *unpack_options, const char *flist)
{
	int r;

	assertMakeDir(target, 0775);

	/* Use the tar program to create an archive. */
	r = systemf("%s cf - %s %s >%s/archive 2>%s/pack.err", testprog, pack_options, flist, target, target);
	failure("Error invoking %s cf -", testprog, pack_options);
	assertEqualInt(r, 0);

	assertChdir(target);

	/* Verify that nothing went to stderr. */
	assertEmptyFile("pack.err");

	/*
	 * Use tar to unpack the archive into another directory.
	 */
	r = systemf("%s xf archive %s >unpack.out 2>unpack.err", testprog, unpack_options);
	failure("Error invoking %s xf archive %s", testprog, unpack_options);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stderr. */
	assertEmptyFile("unpack.err");

	/*
	 * Verify unpacked files.
	 */

	/* Regular file with 2 links. */
	assertIsReg("file", -1);
	assertFileSize("file", 10);
	failure("%s", target);
	assertFileNLinks("file", 2);

	/* Another name for the same file. */
	assertIsReg("linkfile", -1);
	assertFileSize("linkfile", 10);
	assertFileNLinks("linkfile", 2);
	assertIsHardlink("file", "linkfile");

	/* Symlink */
	if (canSymlink())
		assertIsSymlink("symlink", "file");

	/* dir */
	assertIsDir("dir", 0775);
	assertChdir("..");
}

DEFINE_TEST(test_basic)
{
	FILE *f;
	const char *flist;

	assertUmask(0);

	/* File with 10 bytes content. */
	f = fopen("file", "wb");
	assert(f != NULL);
	assertEqualInt(10, fwrite("123456789", 1, 10, f));
	fclose(f);

	/* hardlink to above file. */
	assertMakeHardlink("linkfile", "file");
	assertIsHardlink("file", "linkfile");

	/* Symlink to above file. */
	if (canSymlink())
		assertMakeSymlink("symlink", "file");

	/* Directory. */
	assertMakeDir("dir", 0775);

	if (canSymlink())
		flist = "file linkfile symlink dir";
	else
		flist = "file linkfile dir";
	/* Archive/dearchive with a variety of options. */
	basic_tar("copy", "", "", flist);
	/* tar doesn't handle cpio symlinks correctly */
	/* basic_tar("copy_odc", "--format=odc", ""); */
	basic_tar("copy_ustar", "--format=ustar", "", flist);
}
