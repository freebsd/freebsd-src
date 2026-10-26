/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * Also see test_option_q for additional validation of -r support.
 */
DEFINE_TEST(test_option_r)
{
	char *buff;
	char *p0, *p1;
	size_t buff_size = 35000;
	size_t s, buff_size_rounded, filtered_size;
	int r, i;

	buff = NULL;
	p0 = NULL;
	p1 = NULL;

	/* Create an archive with one file. */
	assertMakeFile("f1", 0644, "abc");
	r = systemf("%s cf archive.tar --uid=0 --gid=0 --format=ustar f1 "
		">step1.out 2>step1.err", testprog);
	failure("Error invoking %s cf archive.tar f1", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("step1.out");
	assertEmptyFile("step1.err");

	/* Do some basic validation of the constructed archive. */
	p0 = slurpfile(&s, "archive.tar");
	if (!assert(p0 != NULL))
		goto done;
	if (!assert(s >= 2048))
		goto done;
	assertEqualMem(p0 + 0, "f1", 3);
	assertEqualMem(p0 + 512, "abc", 3);
	assertEqualMem(p0 + 1024, "\0\0\0\0\0\0\0\0", 8);
	assertEqualMem(p0 + 1536, "\0\0\0\0\0\0\0\0", 8);

	/* Edit that file with a lot more data and update the archive with a new copy. */
	buff = malloc(buff_size);
	assert(buff != NULL);
	if (buff == NULL)
		goto done;

	for (i = 0; i < (int)buff_size; ++i)
		buff[i] = "abcdefghijklmnopqrstuvwxyz"[rand() % 26];
	buff[buff_size - 1] = '\0';
	assertMakeFile("f1", 0644, buff);
	r = systemf("%s rf archive.tar --uid=0 --gid=0 --format=ustar f1 "
		">step2.out 2>step2.err", testprog);
	failure("Error invoking %s rf archive.tar f1", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("step2.out");
	assertEmptyFile("step2.err");

	/* The constructed archive should just have the new entry appended. */
	p1 = slurpfile(&s, "archive.tar");
	if (!assert(p1 != NULL))
		goto done;
	buff_size_rounded = ((buff_size + 511) / 512) * 512;
	assert(s >= 2560 + buff_size_rounded);
	/* Verify first entry is unchanged. */
	assertEqualMem(p0, p1, 1024);
	/* Verify that second entry is correct. */
	assertEqualMem(p1 + 1024, "f1", 3);
	assertEqualMem(p1 + 1536, buff, buff_size);
	/* Verify end-of-archive marker. */
	assertEqualMem(p1 + 1536 + buff_size_rounded, "\0\0\0\0\0\0\0\0", 8);
	assertEqualMem(p1 + 2048 + buff_size_rounded, "\0\0\0\0\0\0\0\0", 8);

	free(p0);
	p0 = p1;

	/* Update the archive by adding a different file. */
	assertMakeFile("f2", 0644, "f2");
	r = systemf("%s rf archive.tar --uid=0 --gid=0 --format=ustar f2 "
		">step3.out 2>step3.err", testprog);
	failure("Error invoking %s rf archive.tar f2", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("step3.out");
	assertEmptyFile("step3.err");

	/* Validate the constructed archive. */
	p1 = slurpfile(&s, "archive.tar");
	if (!assert(p1 != NULL))
		goto done;
	assert(s >= 3584 + buff_size_rounded);
	/* Verify first two entries are unchanged. */
	assertEqualMem(p0, p1, 1536 + buff_size_rounded);
	/* Verify that new entry is correct. */
	assertEqualMem(p1 + 1536 + buff_size_rounded, "f2", 3);
	assertEqualMem(p1 + 2048 + buff_size_rounded, "f2", 3);
	/* Verify end-of-archive marker. */
	assertEqualMem(p1 + 2560 + buff_size_rounded, "\0\0\0\0\0\0\0\0", 8);
	assertEqualMem(p1 + 3072 + buff_size_rounded, "\0\0\0\0\0\0\0\0", 8);
	free(p1);

	/* Unpack everything */
	assertMakeDir("extract", 0775);
	assertChdir("extract");
	r = systemf("%s xf ../archive.tar >extract.out 2>extract.err", testprog);
	failure("Error invoking %s xf archive.tar", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("extract.out");
	assertEmptyFile("extract.err");

	/* Verify that the second copy of f1 overwrote the first. */
	assertFileContents(buff, (int)strlen(buff), "f1");

	/* Appending to an empty filtered archive must fail. */
	free(p0);
	p0 = NULL;
	assertMakeFile("empty", 0644, "");
	r = systemf("%s cf empty.tar.uu --uid=0 --gid=0 --format=ustar "
		"--uuencode -T empty >empty-create.out 2>empty-create.err",
		testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("empty-create.out");
	assertEmptyFile("empty-create.err");
	p0 = slurpfile(&filtered_size, "empty.tar.uu");
	if (!assert(p0 != NULL))
		goto done;
	r = systemf("%s rf empty.tar.uu f2 "
	    ">empty-append.out 2>empty-append.err", testprog);
	assert(r != 0);
	assertEmptyFile("empty-append.out");
	assertNonEmptyFile("empty-append.err");
	p1 = slurpfile(&s, "empty.tar.uu");
	if (!assert(p1 != NULL))
		goto done;
	assertEqualInt(filtered_size, s);
	assertEqualMem(p0, p1, s);
	free(p1);
done:
	free(buff);
	free(p0);
}
