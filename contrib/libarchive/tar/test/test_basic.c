/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

static const char *
make_files(void)
{
	FILE *f;

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
		assertMakeSymlink("symlink", "file", 0);

	/* Directory. */
	assertMakeDir("dir", 0775);

	return canSymlink()
	    ? "file linkfile symlink dir"
	    : "file linkfile dir";
}

static void
verify_files(const char *target)
{
	assertChdir(target);

	/* Regular file with 2 links. */
	failure("%s", target);
	assertIsReg("file", -1);
	failure("%s", target);
	assertFileSize("file", 10);
	failure("%s", target);
	assertFileContents("123456789", 10, "file");
	failure("%s", target);
	assertFileNLinks("file", 2);

	/* Another name for the same file. */
	failure("%s", target);
	assertIsReg("linkfile", -1);
	failure("%s", target);
	assertFileSize("linkfile", 10);
	assertFileContents("123456789", 10, "linkfile");
	assertFileNLinks("linkfile", 2);
	assertIsHardlink("file", "linkfile");

	/* Symlink */
	if (canSymlink())
		assertIsSymlink("symlink", "file", 0);

	/* dir */
	failure("%s", target);
	assertIsDir("dir", 0775);
	assertChdir("..");
}

static void
run_tar(const char *target, const char *pack_options,
    const char *unpack_options, const char *flist)
{
	int r;

	assertMakeDir(target, 0775);

	/* Use the tar program to create an archive. */
	r = systemf("%s cf - %s %s >%s/archive 2>%s/pack.err", testprog, pack_options, flist, target, target);
	failure("Error invoking %s cf -%s", testprog, pack_options);
	assertEqualInt(r, 0);

	assertChdir(target);

	/* Verify that nothing went to stderr. */
	assertEmptyFile("pack.err");

	/*
	 * Use tar to unpack the archive into another directory.
	 */
	r = systemf("%s xf archive %s >unpack.out 2>unpack.err",
	    testprog, unpack_options);
	failure("Error invoking %s xf archive %s", testprog, unpack_options);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stderr. */
	assertEmptyFile("unpack.err");
	assertChdir("..");
}

DEFINE_TEST(test_basic)
{
	const char *flist;

	assertUmask(0);
	flist = make_files();
	/* Archive/dearchive with a variety of options. */
	run_tar("copy", "", "", flist);
	verify_files("copy");

	run_tar("copy_ustar", "--format=ustar", "", flist);
	verify_files("copy_ustar");

	/* tar doesn't handle cpio symlinks correctly */
	/* run_tar("copy_odc", "--format=odc", ""); */
}
