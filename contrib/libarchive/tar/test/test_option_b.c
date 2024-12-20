/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

#define USTAR_OPT " --format=ustar"

DEFINE_TEST(test_option_b)
{
	char *testprog_ustar;
	size_t testprog_ustar_len;

	assertMakeFile("file1", 0644, "file1");
	if (systemf("cat file1 > test_cat.out 2> test_cat.err") != 0) {
		skipping("This test requires a `cat` program");
		return;
	}
	testprog_ustar_len = strlen(testprog) + sizeof(USTAR_OPT) + 1;
	testprog_ustar = malloc(testprog_ustar_len);
	strncpy(testprog_ustar, testprog, testprog_ustar_len);
	strncat(testprog_ustar, USTAR_OPT, testprog_ustar_len);

	/*
	 * Bsdtar does not pad if the output is going directly to a disk file.
	 */
	assertEqualInt(0, systemf("%s -cf archive1.tar file1 >test1.out 2>test1.err", testprog_ustar));
	failure("bsdtar does not pad archives written directly to regular files");
	assertFileSize("archive1.tar", 2048);
	assertEmptyFile("test1.out");
	assertEmptyFile("test1.err");

	/*
	 * Bsdtar does pad to the block size if the output is going to a socket.
	 */
	/* Default is -b 20 */
	assertEqualInt(0, systemf("%s -cf - file1 2>test2.err | cat >archive2.tar ", testprog_ustar));
	failure("bsdtar does pad archives written to pipes");
	assertFileSize("archive2.tar", 10240);
	assertEmptyFile("test2.err");

	assertEqualInt(0, systemf("%s -cf - -b 20 file1 2>test3.err | cat >archive3.tar ", testprog_ustar));
	assertFileSize("archive3.tar", 10240);
	assertEmptyFile("test3.err");

	assertEqualInt(0, systemf("%s -cf - -b 10 file1 2>test4.err | cat >archive4.tar ", testprog_ustar));
	assertFileSize("archive4.tar", 5120);
	assertEmptyFile("test4.err");

	assertEqualInt(0, systemf("%s -cf - -b 1 file1 2>test5.err | cat >archive5.tar ", testprog_ustar));
	assertFileSize("archive5.tar", 2048);
	assertEmptyFile("test5.err");

	assertEqualInt(0, systemf("%s -cf - -b 8192 file1 2>test6.err | cat >archive6.tar ", testprog_ustar));
	assertFileSize("archive6.tar", 4194304);
	assertEmptyFile("test6.err");

	/*
	 * Note: It's not possible to verify at this level that blocks
	 * are getting written with the
	 */

	free(testprog_ustar);
}
