/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"
#if defined(HAVE_UTIME_H)
#include <utime.h>
#elif defined(HAVE_SYS_UTIME_H)
#include <sys/utime.h>
#endif

DEFINE_TEST(test_option_u)
{
	struct utimbuf times;
	char *p;
	size_t s;
	int r;

	/* Create a file. */
	assertMakeFile("f", 0644, "a");

	/* Copy the file to the "copy" dir. */
	r = systemf("echo f| %s -pd copy >copy.out 2>copy.err",
	    testprog);
	assertEqualInt(r, 0);

	/* Check that the file contains only a single "a" */
	p = slurpfile(&s, "copy/f");
	assertEqualInt(s, 1);
	assertEqualMem(p, "a", 1);
	free(p);

	/* Recreate the file with a single "b" */
	assertMakeFile("f", 0644, "b");

	/* Set the mtime to the distant past. */
	memset(&times, 0, sizeof(times));
	times.actime = 1;
	times.modtime = 3;
	assertEqualInt(0, utime("f", &times));

	/* Copy the file to the "copy" dir. */
	r = systemf("echo f| %s -pd copy >copy.out 2>copy.err",
	    testprog);
	assertEqualInt(r, 0);

	/* Verify that the file hasn't changed (it wasn't overwritten) */
	p = slurpfile(&s, "copy/f");
	assertEqualInt(s, 1);
	assertEqualMem(p, "a", 1);
	free(p);

	/* Copy the file to the "copy" dir with -u (force) */
	r = systemf("echo f| %s -pud copy >copy.out 2>copy.err",
	    testprog);
	assertEqualInt(r, 0);

	/* Verify that the file has changed (it was overwritten) */
	p = slurpfile(&s, "copy/f");
	assertEqualInt(s, 1);
	assertEqualMem(p, "b", 1);
	free(p);
}
