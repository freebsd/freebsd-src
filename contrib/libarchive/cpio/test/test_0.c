/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * This first test does basic sanity checks on the environment.  For
 * most of these, we just exit on failure.
 */
#if !defined(_WIN32) || defined(__CYGWIN__)
#define DEV_NULL "/dev/null"
#else
#define DEV_NULL "NUL"
#endif

DEFINE_TEST(test_0)
{
	struct stat st;

	failure("File %s does not exist?!", testprogfile);
	if (!assertEqualInt(0, stat(testprogfile, &st))) {
		fprintf(stderr,
		    "\nFile %s does not exist; aborting test.\n\n",
		    testprog);
		exit(1);
	}

	failure("%s is not executable?!", testprogfile);
	if (!assert((st.st_mode & 0111) != 0)) {
		fprintf(stderr,
		    "\nFile %s not executable; aborting test.\n\n",
		    testprog);
		exit(1);
	}

	/*
	 * Try to successfully run the program; this requires that
	 * we know some option that will succeed.
	 */
	if (0 == systemf("%s --version >" DEV_NULL, testprog)) {
		/* This worked. */
	} else if (0 == systemf("%s -W version >" DEV_NULL, testprog)) {
		/* This worked. */
	} else {
		failure("Unable to successfully run any of the following:\n"
		    "  * %s --version\n"
		    "  * %s -W version\n",
		    testprog, testprog);
		assert(0);
	}

	/* TODO: Ensure that our reference files are available. */
}
