/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * Test that "--help", "-h", and "-W help" options all work and
 * generate reasonable output.
 */

static int
in_first_line(const char *p, const char *substring)
{
	size_t l = strlen(substring);

	while (*p != '\0' && *p != '\n') {
		if (memcmp(p, substring, l) == 0)
			return (1);
		++p;
	}
	return (0);
}

DEFINE_TEST(test_help)
{
	int r;
	char *p;
	size_t plen;

	/* Exercise --help option. */
	r = systemf("%s --help >help.stdout 2>help.stderr", testprog);
	assertEqualInt(r, 0);
	failure("--help should generate nothing to stderr.");
	assertEmptyFile("help.stderr");
	/* Help message should start with name of program. */
	p = slurpfile(&plen, "help.stdout");
	failure("Help output should be long enough.");
	assert(plen >= 6);
	failure("First line of help output should contain 'bsdcat': %s", p);
	assert(in_first_line(p, "bsdcat"));
	/*
	 * TODO: Extend this check to further verify that --help output
	 * looks approximately right.
	 */
	free(p);

	/* -h option should generate the same output. */
	r = systemf("%s -h >h.stdout 2>h.stderr", testprog);
	assertEqualInt(r, 0);
	failure("-h should generate nothing to stderr.");
	assertEmptyFile("h.stderr");
	failure("stdout should be same for -h and --help");
	assertEqualFile("h.stdout", "help.stdout");
}
