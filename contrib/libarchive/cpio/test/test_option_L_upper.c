/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/* This is a little pointless, as Windows doesn't support symlinks
 * (except for the seriously crippled CreateSymbolicLink API) so these
 * tests won't run on Windows. */
#if defined(_WIN32) && !defined(__CYGWIN__)
#define CAT "type"
#define SEP "\\"
#else
#define CAT "cat"
#define SEP "/"
#endif

DEFINE_TEST(test_option_L_upper)
{
	FILE *filelist;
	int r;

	if (!canSymlink()) {
		skipping("Symlink tests");
		return;
	}

	filelist = fopen("filelist", "w");

	/* Create a file and a symlink to the file. */
	assertMakeFile("file", 0644, "1234567890");
	fprintf(filelist, "file\n");

	/* Symlink to above file. */
	assertMakeSymlink("symlink", "file", 0);
	fprintf(filelist, "symlink\n");

	fclose(filelist);

	r = systemf(CAT " filelist | %s -pd copy >copy.out 2>copy.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "copy.err");

	failure("Regular -p without -L should preserve symlinks.");
	assertIsSymlink("copy/symlink", NULL, 0);

	r = systemf(CAT " filelist | %s -pd -L copy-L >copy-L.out 2>copy-L.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("copy-L.out");
	assertTextFileContents("1 block\n", "copy-L.err");
	failure("-pdL should dereference symlinks and turn them into files.");
	assertIsReg("copy-L/symlink", -1);

	r = systemf(CAT " filelist | %s -o >archive.out 2>archive.err", testprog);
	failure("Error invoking %s -o ", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "archive.err");

	assertMakeDir("unpack", 0755);
	assertChdir("unpack");
	r = systemf(CAT " .." SEP "archive.out | %s -i >unpack.out 2>unpack.err", testprog);

	failure("Error invoking %s -i", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "unpack.err");
	assertChdir("..");

	assertIsSymlink("unpack/symlink", NULL, 0);

	r = systemf(CAT " filelist | %s -oL >archive-L.out 2>archive-L.err", testprog);
	failure("Error invoking %s -oL", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "archive-L.err");

	assertMakeDir("unpack-L", 0755);
	assertChdir("unpack-L");
	r = systemf(CAT " .." SEP "archive-L.out | %s -i >unpack-L.out 2>unpack-L.err", testprog);

	failure("Error invoking %s -i < archive-L.out", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "unpack-L.err");
	assertChdir("..");
	assertIsReg("unpack-L/symlink", -1);
}
