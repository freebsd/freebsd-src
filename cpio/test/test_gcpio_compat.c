/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

static void
unpack_test(const char *from, const char *options, const char *se)
{
	int r;

	/* Create a work dir named after the file we're unpacking. */
	assertMakeDir(from, 0775);
	assertChdir(from);

	/*
	 * Use cpio to unpack the sample archive
	 */
	extract_reference_file(from);
	r = systemf("%s -i %s < %s >unpack.out 2>unpack.err",
	    testprog, options, from);
	failure("Error invoking %s -i %s < %s",
	    testprog, options, from);
	assertEqualInt(r, 0);

	/* Verify that nothing went to stderr. */
	if (canSymlink()) {
		failure("Error invoking %s -i %s < %s",
		    testprog, options, from);
		assertTextFileContents(se, "unpack.err");
	}

	/*
	 * Verify unpacked files.
	 */

	/* Regular file with 2 links. */
	assertIsReg("file", 0644);
	failure("%s", from);
	assertFileSize("file", 10);
	assertFileSize("linkfile", 10);
	failure("%s", from);
	assertFileNLinks("file", 2);

	/* Another name for the same file. */
	failure("%s", from);
	assertIsHardlink("linkfile", "file");
	assertFileSize("file", 10);
	assertFileSize("linkfile", 10);

	/* Symlink */
	if (canSymlink())
		assertIsSymlink("symlink", "file", 0);

	/* dir */
	assertIsDir("dir", 0775);

	assertChdir("..");
}

DEFINE_TEST(test_gcpio_compat)
{
	assertUmask(0);

	/* Dearchive sample files with a variety of options. */
	if (canSymlink()) {
		unpack_test("test_gcpio_compat_ref.bin",
		    "--no-preserve-owner", "1 block\n");
		unpack_test("test_gcpio_compat_ref.crc",
		    "--no-preserve-owner", "2 blocks\n");
		unpack_test("test_gcpio_compat_ref.newc",
		    "--no-preserve-owner", "2 blocks\n");
		/* gcpio-2.9 only reads 6 blocks here */
		unpack_test("test_gcpio_compat_ref.ustar",
		    "--no-preserve-owner", "7 blocks\n");
	} else {
		unpack_test("test_gcpio_compat_ref_nosym.bin",
		    "--no-preserve-owner", "1 block\n");
		unpack_test("test_gcpio_compat_ref_nosym.crc",
		    "--no-preserve-owner", "2 blocks\n");
		unpack_test("test_gcpio_compat_ref_nosym.newc",
		    "--no-preserve-owner", "2 blocks\n");
		/* gcpio-2.9 only reads 6 blocks here */
		unpack_test("test_gcpio_compat_ref_nosym.ustar",
		    "--no-preserve-owner", "7 blocks\n");
	}
}
