/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * tar -x -P should follow existing symlinks for dirs, but not other
 * content.  Plain tar -x should remove symlinks when they're in the
 * way of a dir extraction.
 */

DEFINE_TEST(test_symlink_dir)
{
	assertUmask(0);

	assertMakeDir("source", 0755);
	assertMakeFile("source/file", 0755, "a");
	assertMakeFile("source/file2", 0755, "ab");
	assertMakeDir("source/dir", 0755);
	assertMakeDir("source/dir/d", 0755);
	assertMakeFile("source/dir/f", 0755, "abc");
	assertMakeDir("source/dir2", 0755);
	assertMakeDir("source/dir2/d2", 0755);
	assertMakeFile("source/dir2/f2", 0755, "abcd");
	assertMakeDir("source/dir3", 0755);
	assertMakeDir("source/dir3/d3", 0755);
	assertMakeFile("source/dir3/f3", 0755, "abcde");
	assertMakeDir("source/dir4", 0755);
	assertMakeFile("source/dir4/file3", 0755, "abcdef");
	assertMakeHardlink("source/dir4/file4", "source/dir4/file3");

	assertEqualInt(0,
	    systemf("%s -cf test.tar -C source dir dir2 dir3 file file2",
		testprog));

	/* Second archive with hardlinks */
	assertEqualInt(0,
	    systemf("%s -cf test2.tar -C source dir4", testprog));

	/*
	 * Extract with -x and without -P.
	 */
	assertMakeDir("dest1", 0755);
	/* "dir" is a symlink to an existing "dest1/real_dir" */
	assertMakeDir("dest1/real_dir", 0755);
	if (canSymlink()) {
		assertMakeSymlink("dest1/dir", "real_dir", 1);
		/* "dir2" is a symlink to a non-existing "real_dir2" */
		assertMakeSymlink("dest1/dir2", "real_dir2", 1);
	} else {
		skipping("Symlinks are not supported on this platform");
	}
	/* "dir3" is a symlink to an existing "non_dir3" */
	assertMakeFile("dest1/non_dir3", 0755, "abcdef");
	if (canSymlink())
		assertMakeSymlink("dest1/dir3", "non_dir3", 1);
	/* "file" is a symlink to existing "real_file" */
	assertMakeFile("dest1/real_file", 0755, "abcdefg");
	if (canSymlink()) {
		assertMakeSymlink("dest1/file", "real_file", 0);
		/* "file2" is a symlink to non-existing "real_file2" */
		assertMakeSymlink("dest1/file2", "real_file2", 0);
	}
	assertEqualInt(0, systemf("%s -xf test.tar -C dest1", testprog));

	/* dest1/dir symlink should be replaced */
	failure("symlink to dir was followed when it shouldn't be");
	assertIsDir("dest1/dir", -1);
	/* dest1/dir2 symlink should be replaced */
	failure("Broken symlink wasn't replaced with dir");
	assertIsDir("dest1/dir2", -1);
	/* dest1/dir3 symlink should be replaced */
	failure("Symlink to non-dir wasn't replaced with dir");
	assertIsDir("dest1/dir3", -1);
	/* dest1/file symlink should be replaced */
	failure("Symlink to existing file should be replaced");
	assertIsReg("dest1/file", -1);
	/* dest1/file2 symlink should be replaced */
	failure("Symlink to non-existing file should be replaced");
	assertIsReg("dest1/file2", -1);

	/*
	 * Extract with both -x and -P
	 */
	assertMakeDir("dest2", 0755);
	/* "dir" is a symlink to existing "real_dir" */
	assertMakeDir("dest2/real_dir", 0755);
	if (canSymlink())
		assertMakeSymlink("dest2/dir", "real_dir", 1);
	/* "dir2" is a symlink to a non-existing "real_dir2" */
	if (canSymlink())
		assertMakeSymlink("dest2/dir2", "real_dir2", 1);
	/* "dir3" is a symlink to an existing "non_dir3" */
	assertMakeFile("dest2/non_dir3", 0755, "abcdefgh");
	if (canSymlink())
		assertMakeSymlink("dest2/dir3", "non_dir3", 1);
	/* "file" is a symlink to existing "real_file" */
	assertMakeFile("dest2/real_file", 0755, "abcdefghi");
	if (canSymlink())
		assertMakeSymlink("dest2/file", "real_file", 0);
	/* "file2" is a symlink to non-existing "real_file2" */
	if (canSymlink())
		assertMakeSymlink("dest2/file2", "real_file2", 0);
	assertEqualInt(0, systemf("%s -xPf test.tar -C dest2", testprog));

	/* "dir4" is a symlink to existing "real_dir" */
	if (canSymlink())
		assertMakeSymlink("dest2/dir4", "real_dir", 1);
	assertEqualInt(0, systemf("%s -xPf test2.tar -C dest2", testprog));

	/* dest2/dir and dest2/dir4 symlinks should be followed */
	if (canSymlink()) {
		assertIsSymlink("dest2/dir", "real_dir", 1);
		assertIsSymlink("dest2/dir4", "real_dir", 1);
		assertIsDir("dest2/real_dir", -1);
	}

	/* Contents of 'dir' should be restored */
	assertIsDir("dest2/dir/d", -1);
	assertIsReg("dest2/dir/f", -1);
	assertFileSize("dest2/dir/f", 3);
	/* dest2/dir2 symlink should be removed */
	failure("Broken symlink wasn't replaced with dir");
	assertIsDir("dest2/dir2", -1);
	/* dest2/dir3 symlink should be removed */
	failure("Symlink to non-dir wasn't replaced with dir");
	assertIsDir("dest2/dir3", -1);
	/* dest2/file symlink should be removed;
	 * even -P shouldn't follow symlinks for files */
	failure("Symlink to existing file should be removed");
	assertIsReg("dest2/file", -1);
	/* dest2/file2 symlink should be removed */
	failure("Symlink to non-existing file should be removed");
	assertIsReg("dest2/file2", -1);

	/* dest2/dir4/file3 and dest2/dir4/file4 should be hard links */
	assertIsHardlink("dest2/dir4/file3", "dest2/dir4/file4");
}
