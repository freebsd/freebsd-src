/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 lilu
 * All rights reserved.
 */
#include "test.h"

/*
 * An entry whose hardlink target reduces to the empty string after
 * stripping the leading '/' is skipped by edit_pathname().  This used to
 * happen silently: the entry vanished from the output and bsdtar exited 0,
 * so a caller could not detect the incomplete result.  The entry must now be
 * named in a diagnostic and must cause a non-zero exit status.
 */
DEFINE_TEST(test_edit_pathname_hardlink)
{
	const char *reffile = "test_edit_pathname_hardlink.tar.Z";
	char *p;
	size_t s;
	const char *expected_errmsg =
	    "Skipping ./hardlink: hardlink target becomes empty "
	    "after removing leading '/'";

	extract_reference_file(reffile);

	/* Transforming the archive must report the dropped entry and fail. */
	assert(0 != systemf("%s -cf out.tar @%s >test.out 2>test.err",
	    testprog, reffile));

	/* out.tar must list ./a.txt and must not list the dropped ./hardlink. */
	assertEqualInt(0, systemf("%s -tf out.tar >list.out 2>list.err",
	    testprog));
	p = slurpfile(&s, "list.out");
	assert(p != NULL);
	assert(strstr(p, "a.txt") != NULL);
	assert(strstr(p, "hardlink") == NULL);
	free(p);

	/* The diagnostic must name the entry and the reason. */
	if (assertFileExists("test.err")) {
		p = slurpfile(&s, "test.err");
		assert(strstr(p, expected_errmsg) != NULL);
		free(p);
	}
}
