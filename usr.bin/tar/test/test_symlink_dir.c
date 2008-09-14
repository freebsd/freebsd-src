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
 * tar -x -P should follow existing symlinks for dirs, but not other
 * content.  Plain tar -x should remove symlinks when they're in the
 * way of a dir extraction.
 */

static int
mkfile(const char *name, int mode, const char *contents, ssize_t size)
{
	int fd = open(name, O_CREAT | O_WRONLY, mode);
	if (fd < 0)
		return (-1);
	if (size != write(fd, contents, size)) {
		close(fd);
		return (-1);
	}
	close(fd);
	return (0);
}

DEFINE_TEST(test_symlink_dir)
{
	struct stat st, st2;
	int oldumask;

	oldumask = umask(0);

	assertEqualInt(0, mkdir("source", 0755));
	assertEqualInt(0, mkfile("source/file", 0755, "a", 1));
	assertEqualInt(0, mkfile("source/file2", 0755, "ab", 2));
	assertEqualInt(0, mkdir("source/dir", 0755));
	assertEqualInt(0, mkdir("source/dir/d", 0755));
	assertEqualInt(0, mkfile("source/dir/f", 0755, "abc", 3));
	assertEqualInt(0, mkdir("source/dir2", 0755));
	assertEqualInt(0, mkdir("source/dir2/d2", 0755));
	assertEqualInt(0, mkfile("source/dir2/f2", 0755, "abcd", 4));
	assertEqualInt(0, mkdir("source/dir3", 0755));
	assertEqualInt(0, mkdir("source/dir3/d3", 0755));
	assertEqualInt(0, mkfile("source/dir3/f3", 0755, "abcde", 5));

	assertEqualInt(0,
	    systemf("%s -cf test.tar -C source dir dir2 dir3 file file2",
		testprog));

	/*
	 * Extract with -x and without -P.
	 */
	assertEqualInt(0, mkdir("dest1", 0755));
	/* "dir" is a symlink to an existing "real_dir" */
	assertEqualInt(0, mkdir("dest1/real_dir", 0755));
	assertEqualInt(0, symlink("real_dir", "dest1/dir"));
	/* "dir2" is a symlink to a non-existing "real_dir2" */
	assertEqualInt(0, symlink("real_dir2", "dest1/dir2"));
	/* "dir3" is a symlink to an existing "non_dir3" */
	assertEqualInt(0, mkfile("dest1/non_dir3", 0755, "abcdef", 6));
	assertEqualInt(0, symlink("non_dir3", "dest1/dir3"));
	/* "file" is a symlink to existing "real_file" */
	assertEqualInt(0, mkfile("dest1/real_file", 0755, "abcdefg", 7));
	assertEqualInt(0, symlink("real_file", "dest1/file"));
	/* "file2" is a symlink to non-existing "real_file2" */
	assertEqualInt(0, symlink("real_file2", "dest1/file2"));

	assertEqualInt(0, systemf("%s -xf test.tar -C dest1", testprog));

	/* dest1/dir symlink should be removed */
	assertEqualInt(0, lstat("dest1/dir", &st));
	failure("symlink to dir was followed when it shouldn't be");
	assert(S_ISDIR(st.st_mode));
	/* dest1/dir2 symlink should be removed */
	assertEqualInt(0, lstat("dest1/dir2", &st));
	failure("Broken symlink wasn't replaced with dir");
	assert(S_ISDIR(st.st_mode));
	/* dest1/dir3 symlink should be removed */
	assertEqualInt(0, lstat("dest1/dir3", &st));
	failure("Symlink to non-dir wasn't replaced with dir");
	assert(S_ISDIR(st.st_mode));
	/* dest1/file symlink should be removed */
	assertEqualInt(0, lstat("dest1/file", &st));
	failure("Symlink to existing file should be removed");
	assert(S_ISREG(st.st_mode));
	/* dest1/file2 symlink should be removed */
	assertEqualInt(0, lstat("dest1/file2", &st));
	failure("Symlink to non-existing file should be removed");
	assert(S_ISREG(st.st_mode));

	/*
	 * Extract with both -x and -P
	 */
	assertEqualInt(0, mkdir("dest2", 0755));
	/* "dir" is a symlink to existing "real_dir" */
	assertEqualInt(0, mkdir("dest2/real_dir", 0755));
	assertEqualInt(0, symlink("real_dir", "dest2/dir"));
	/* "dir2" is a symlink to a non-existing "real_dir2" */
	assertEqualInt(0, symlink("real_dir2", "dest2/dir2"));
	/* "dir3" is a symlink to an existing "non_dir3" */
	assertEqualInt(0, mkfile("dest2/non_dir3", 0755, "abcdefgh", 8));
	assertEqualInt(0, symlink("non_dir3", "dest2/dir3"));
	/* "file" is a symlink to existing "real_file" */
	assertEqualInt(0, mkfile("dest2/real_file", 0755, "abcdefghi", 9));
	assertEqualInt(0, symlink("real_file", "dest2/file"));
	/* "file2" is a symlink to non-existing "real_file2" */
	assertEqualInt(0, symlink("real_file2", "dest2/file2"));

	assertEqualInt(0, systemf("%s -xPf test.tar -C dest2", testprog));

	/* dest2/dir symlink should be followed */
	assertEqualInt(0, lstat("dest2/dir", &st));
	failure("tar -xP removed symlink instead of following it");
	if (assert(S_ISLNK(st.st_mode))) {
		/* Only verify what the symlink points to if it
		 * really is a symlink. */
		failure("The symlink should point to a directory");
		assertEqualInt(0, stat("dest2/dir", &st));
		assert(S_ISDIR(st.st_mode));
		failure("The pre-existing directory should still be there");
		assertEqualInt(0, lstat("dest2/real_dir", &st2));
		assert(S_ISDIR(st2.st_mode));
		assertEqualInt(st.st_dev, st2.st_dev);
		failure("symlink should still point to the existing directory");
		assertEqualInt(st.st_ino, st2.st_ino);
	}
	/* Contents of 'dir' should be restored */
	assertEqualInt(0, lstat("dest2/dir/d", &st));
	assert(S_ISDIR(st.st_mode));
	assertEqualInt(0, lstat("dest2/dir/f", &st));
	assert(S_ISREG(st.st_mode));
	assertEqualInt(3, st.st_size);
	/* dest2/dir2 symlink should be removed */
	assertEqualInt(0, lstat("dest2/dir2", &st));
	failure("Broken symlink wasn't replaced with dir");
	assert(S_ISDIR(st.st_mode));
	/* dest2/dir3 symlink should be removed */
	assertEqualInt(0, lstat("dest2/dir3", &st));
	failure("Symlink to non-dir wasn't replaced with dir");
	assert(S_ISDIR(st.st_mode));
	/* dest2/file symlink should be removed;
	 * even -P shouldn't follow symlinks for files */
	assertEqualInt(0, lstat("dest2/file", &st));
	failure("Symlink to existing file should be removed");
	assert(S_ISREG(st.st_mode));
	/* dest2/file2 symlink should be removed */
	assertEqualInt(0, lstat("dest2/file2", &st));
	failure("Symlink to non-existing file should be removed");
	assert(S_ISREG(st.st_mode));
}
