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

	failure("File %s does not exist?!", testprog);
	if (!assertEqualInt(0, stat(testprogfile, &st))) {
		fprintf(stderr,
		    "\nFile %s does not exist; aborting test.\n\n",
		    testprog);
		exit(1);
	}

	failure("%s is not executable?!", testprog);
	if (!assert((st.st_mode & 0111) != 0)) {
		fprintf(stderr,
		    "\nFile %s not executable; aborting test.\n\n",
		    testprog);
		exit(1);
	}

	/* TODO: Ensure that our reference files are available. */
}
