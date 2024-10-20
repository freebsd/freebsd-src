/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Aaron Lindros
 * All rights reserved.
 */
#include "test.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

/* Test I arg - file name encoding */
DEFINE_TEST(test_I)
{
	const char *reffile = "test_I.zip";
#if !defined(_WIN32) || defined(__CYGWIN__)
	const char *envstr = "env LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8 "
	    "LC_CTYPE=en_US.UTF-8";
#else
	const char *envstr = "";
#endif
	int r;

#if HAVE_SETLOCALE
	if (NULL == setlocale(LC_ALL, "en_US.UTF-8")) {
		skipping("en_US.UTF-8 locale not available on this system.");
		return;
	}
#else
	skipping("setlocale() not available on this system.");
#endif

	extract_reference_file(reffile);
	r = systemf("%s %s -I UTF-8 %s >test.out 2>test.err", envstr, testprog,
	    reffile);
	assertEqualInt(0, r);
	assertNonEmptyFile("test.out");
	assertEmptyFile("test.err");

	assertTextFileContents("Hello, World!\n", "Γειά σου Κόσμε.txt");
}
