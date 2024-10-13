/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

static void
verify_files(const char *msg)
{
	/*
	 * Verify unpacked files.
	 */

	/* Regular file with 2 links. */
	failure("%s", msg);
	assertIsReg("file", 0644);
	failure("%s", msg);
	assertFileSize("file", 10);
	failure("%s", msg);
	assertFileNLinks("file", 2);

	/* Another name for the same file. */
	failure("%s", msg);
	assertIsHardlink("linkfile", "file");

	/* Symlink */
	if (canSymlink())
		assertIsSymlink("symlink", "file", 0);

	/* Another file with 1 link and different permissions. */
	failure("%s", msg);
	assertIsReg("file2", 0777);
	failure("%s", msg);
	assertFileSize("file2", 10);
	failure("%s", msg);
	assertFileNLinks("file2", 1);

	/* dir */
	assertIsDir("dir", 0775);
}

static void
basic_cpio(const char *target,
    const char *pack_options,
    const char *unpack_options,
    const char *se, const char *se2)
{
	int r;

	if (!assertMakeDir(target, 0775))
	    return;

	/* Use the cpio program to create an archive. */
	r = systemf("%s -R 1000:1000 -o %s < filelist >%s/archive 2>%s/pack.err",
	    testprog, pack_options, target, target);
	failure("Error invoking %s -o %s", testprog, pack_options);
	assertEqualInt(r, 0);

	assertChdir(target);

	/* Verify stderr. */
	failure("Expected: %s, options=%s", se, pack_options);
	assertTextFileContents(se, "pack.err");

	/*
	 * Use cpio to unpack the archive into another directory.
	 */
	r = systemf("%s -i %s< archive >unpack.out 2>unpack.err",
	    testprog, unpack_options);
	failure("Error invoking %s -i %s", testprog, unpack_options);
	assertEqualInt(r, 0);

	/* Verify stderr. */
	failure("Error invoking %s -i %s in dir %s", testprog, unpack_options, target);
	assertTextFileContents(se2, "unpack.err");

	verify_files(pack_options);

	assertChdir("..");
}

static void
passthrough(const char *target)
{
	int r;

	if (!assertMakeDir(target, 0775))
		return;

	/*
	 * Use cpio passthrough mode to copy files to another directory.
	 */
	r = systemf("%s -p %s <filelist >%s/stdout 2>%s/stderr",
	    testprog, target, target, target);
	failure("Error invoking %s -p", testprog);
	assertEqualInt(r, 0);

	assertChdir(target);

	/* Verify stderr. */
	failure("Error invoking %s -p in dir %s",
	    testprog, target);
	assertTextFileContents("1 block\n", "stderr");

	verify_files("passthrough");
	assertChdir("..");
}

DEFINE_TEST(test_basic)
{
	FILE *filelist;
	const char *msg;
	char result[1024];

	assertUmask(0);

	/*
	 * Create an assortment of files on disk.
	 */
	filelist = fopen("filelist", "w");
	memset(result, 0, sizeof(result));

	/* File with 10 bytes content. */
	assertMakeFile("file", 0644, "1234567890");
	fprintf(filelist, "file\n");
	if (is_LargeInode("file")) {
		strncat(result,
		    "bsdcpio: file: large inode number truncated: ",
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    strerror(ERANGE),
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    "\n",
		    sizeof(result) - strlen(result) -1);
	}

	/* hardlink to above file. */
	assertMakeHardlink("linkfile", "file");
	fprintf(filelist, "linkfile\n");
	if (is_LargeInode("linkfile")) {
		strncat(result,
		    "bsdcpio: linkfile: large inode number truncated: ",
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    strerror(ERANGE),
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    "\n",
		    sizeof(result) - strlen(result) -1);
	}

	/* Symlink to above file. */
	if (canSymlink()) {
		assertMakeSymlink("symlink", "file", 0);
		fprintf(filelist, "symlink\n");
		if (is_LargeInode("symlink")) {
			strncat(result,
			    "bsdcpio: symlink: large inode number truncated: ",
			    sizeof(result) - strlen(result) -1);
			strncat(result,
			    strerror(ERANGE),
			    sizeof(result) - strlen(result) -1);
			strncat(result,
			    "\n",
			    sizeof(result) - strlen(result) -1);
		}
	}

	/* Another file with different permissions. */
	assertMakeFile("file2", 0777, "1234567890");
	fprintf(filelist, "file2\n");
	if (is_LargeInode("file2")) {
		strncat(result,
		    "bsdcpio: file2: large inode number truncated: ",
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    strerror(ERANGE),
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    "\n",
		    sizeof(result) - strlen(result) -1);
	}

	/* Directory. */
	assertMakeDir("dir", 0775);
	fprintf(filelist, "dir\n");
	if (is_LargeInode("dir")) {
		strncat(result,
		    "bsdcpio: dir: large inode number truncated: ",
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    strerror(ERANGE),
		    sizeof(result) - strlen(result) -1);
		strncat(result,
		    "\n",
		    sizeof(result) - strlen(result) -1);
	}
	strncat(result, "2 blocks\n", sizeof(result) - strlen(result) -1);

	/* All done. */
	fclose(filelist);

	assertUmask(022);

	/* Archive/dearchive with a variety of options. */
	msg = canSymlink() ? "2 blocks\n" : "1 block\n";
	basic_cpio("copy", "", "", msg, msg);
	basic_cpio("copy_odc", "--format=odc", "", msg, msg);
	basic_cpio("copy_newc", "-H newc", "", result, "2 blocks\n");
	basic_cpio("copy_cpio", "-H odc", "", msg, msg);
	msg = "1 block\n";
	basic_cpio("copy_bin", "-H bin", "", msg, msg);
	msg = canSymlink() ? "9 blocks\n" : "8 blocks\n";
	basic_cpio("copy_ustar", "-H ustar", "", msg, msg);

	/* Copy in one step using -p */
	passthrough("passthrough");
}
