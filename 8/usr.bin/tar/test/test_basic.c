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
	struct stat st, st2;
#if !defined(_WIN32) || defined(__CYGWIN__)
	char buff[128];
#endif
	int r;

	assertEqualInt(0, mkdir(target, 0775));

	/* Use the tar program to create an archive. */
#if !defined(_WIN32) || defined(__CYGWIN__)
	r = systemf("%s cf - %s `cat %s` >%s/archive 2>%s/pack.err", testprog, pack_options, flist, target, target);
#else
	r = systemf("%s cf - %s %s >%s/archive 2>%s/pack.err", testprog, pack_options, flist, target, target);
#endif
	failure("Error invoking %s cf -", testprog, pack_options);
	assertEqualInt(r, 0);

	chdir(target);

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
	r = lstat("file", &st);
	failure("Failed to stat file %s/file, errno=%d", target, errno);
	assertEqualInt(r, 0);
	if (r == 0) {
		assert(S_ISREG(st.st_mode));
#if !defined(_WIN32) || defined(__CYGWIN__)
		assertEqualInt(0644, st.st_mode & 0777);
#else
		assertEqualInt(0600, st.st_mode & 0700);
#endif
		assertEqualInt(10, st.st_size);
		failure("file %s/file", target);
		assertEqualInt(2, st.st_nlink);
	}

	/* Another name for the same file. */
	r = lstat("linkfile", &st2);
	failure("Failed to stat file %s/linkfile, errno=%d", target, errno);
	assertEqualInt(r, 0);
	if (r == 0) {
		assert(S_ISREG(st2.st_mode));
#if !defined(_WIN32) || defined(__CYGWIN__)
		assertEqualInt(0644, st2.st_mode & 0777);
#else
		assertEqualInt(0600, st2.st_mode & 0700);
#endif
		assertEqualInt(10, st2.st_size);
		failure("file %s/linkfile", target);
		assertEqualInt(2, st2.st_nlink);
		/* Verify that the two are really hardlinked. */
		assertEqualInt(st.st_dev, st2.st_dev);
		failure("%s/linkfile and %s/file aren't really hardlinks", target, target);
		assertEqualInt(st.st_ino, st2.st_ino);
	}

#if !defined(_WIN32) || defined(__CYGWIN__)
	/* Symlink */
	r = lstat("symlink", &st);
	failure("Failed to stat file %s/symlink, errno=%d", target, errno);
	assertEqualInt(r, 0);
	if (r == 0) {
		failure("symlink should be a symlink; actual mode is %o",
		    st.st_mode);
		assert(S_ISLNK(st.st_mode));
		if (S_ISLNK(st.st_mode)) {
			r = readlink("symlink", buff, sizeof(buff));
			assertEqualInt(r, 4);
			buff[r] = '\0';
			assertEqualString(buff, "file");
		}
	}
#endif

	/* dir */
	r = lstat("dir", &st);
	if (r == 0) {
		assertEqualInt(r, 0);
		assert(S_ISDIR(st.st_mode));
#if !defined(_WIN32) || defined(__CYGWIN__)
		assertEqualInt(0775, st.st_mode & 0777);
#else
		assertEqualInt(0700, st.st_mode & 0700);
#endif
	}

	chdir("..");
}

DEFINE_TEST(test_basic)
{
	int fd;
	int filelist;
	int oldumask;
	const char *flist;

	oldumask = umask(0);

	/*
	 * Create an assortment of files on disk.
	 */
	filelist = open("filelist", O_CREAT | O_WRONLY, 0644);

	/* File with 10 bytes content. */
	fd = open("file", O_CREAT | O_WRONLY, 0644);
	assert(fd >= 0);
	assertEqualInt(10, write(fd, "123456789", 10));
	close(fd);
	write(filelist, "file\n", 5);

	/* hardlink to above file. */
	assertEqualInt(0, link("file", "linkfile"));
	write(filelist, "linkfile\n", 9);

	/* Symlink to above file. */
	assertEqualInt(0, symlink("file", "symlink"));
	write(filelist, "symlink\n", 8);

	/* Directory. */
	assertEqualInt(0, mkdir("dir", 0775));
	write(filelist, "dir\n", 4);
	/* All done. */
	close(filelist);

#if !defined(_WIN32) || defined(__CYGWIN__)
	flist = "filelist";
#else
	flist = "file linkfile symlink dir";
#endif
	/* Archive/dearchive with a variety of options. */
	basic_tar("copy", "", "", flist);
	/* tar doesn't handle cpio symlinks correctly */
	/* basic_tar("copy_odc", "--format=odc", ""); */
	basic_tar("copy_ustar", "--format=ustar", "", flist);

	umask(oldumask);
}
