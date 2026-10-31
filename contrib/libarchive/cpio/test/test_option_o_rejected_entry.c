/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Martin Mirchev
 * All rights reserved.
 */
#include "test.h"

/*
 * If the archive format cannot store an entry, bsdcpio -o warns and omits
 * the entry; the exit status must report that the archive is incomplete.
 */
DEFINE_TEST(test_option_o_rejected_entry)
{
	char name[121];
	char filelist[sizeof(name) + 8];
	int r;

	/* A file name of 120 characters does not fit a ustar header. */
	memset(name, 'f', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';

	assertMakeFile("small", 0644, "hello");
	assertMakeFile(name, 0644, "x");
	snprintf(filelist, sizeof(filelist), "small\n%s\n", name);
	assertMakeFile("filelist", 0644, filelist);

	r = systemf("%s -o -H ustar <filelist >archive.out 2>archive.err",
	    testprog);
	failure("The omitted entry must be reported in the exit status");
	assert(r != 0);
	assertNonEmptyFile("archive.err");

	/* The other entry is still written. */
	r = systemf("%s -it <archive.out >list.out 2>list.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("small\n", "list.out");
}
