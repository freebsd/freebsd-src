/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Arshan Khanifar <arshankhanifar@gmail.com>
 * under sponsorship from the FreeBSD Foundation.
 */
#include "test.h"

DEFINE_TEST(test_option_C_mtree)
{
	char *p0;
	size_t s;
	int r;
	p0 = NULL;
	char *content = "./foo type=file uname=root gname=root mode=0755\n";
	char *filename = "output.tar";
#if defined(_WIN32) && !defined(CYGWIN)
	char *p;
#endif

	/* an absolute path to mtree file */
	char *mtree_file = "/METALOG.mtree";
	char *absolute_path = malloc(strlen(testworkdir) + strlen(mtree_file) + 1);
	strcpy(absolute_path, testworkdir);
	strcat(absolute_path, mtree_file );

	/* Create an archive using an mtree file. */
	assertMakeFile(absolute_path, 0777, content);
	assertMakeDir("bar", 0775);
	assertMakeFile("bar/foo", 0777, "abc");

#if defined(_WIN32) && !defined(CYGWIN)
	p = absolute_path;
	while(*p != '\0') {
		if (*p == '/')
			*p = '\\';
		p++;
	}

	r = systemf("%s -cf %s -C bar @%s >step1.out 2>step1.err", testprog, filename, absolute_path);
	failure("Error invoking %s -cf %s -C bar @%s", testprog, filename, absolute_path);
#else
	r = systemf("%s -cf %s -C bar \"@%s\" >step1.out 2>step1.err", testprog, filename, absolute_path);
	failure("Error invoking %s -cf %s -C bar \"@%s\"", testprog, filename, absolute_path);
#endif

	assertEqualInt(r, 0);
	assertEmptyFile("step1.out");
	assertEmptyFile("step1.err");

	/* Do validation of the constructed archive. */

	p0 = slurpfile(&s, "output.tar");
	if (!assert(p0 != NULL))
		goto done;
	if (!assert(s >= 2048))
		goto done;
	assertEqualMem(p0 + 0, "./foo", 5);
	assertEqualMem(p0 + 512, "abc", 3);
	assertEqualMem(p0 + 1024, "\0\0\0\0\0\0\0\0", 8);
	assertEqualMem(p0 + 1536, "\0\0\0\0\0\0\0\0", 8);
done:
	free(p0);
	free(absolute_path);
}


