/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Dan Nelson
 * All rights reserved.
 */
#include "test.h"

/* Compare the mode column of tar's list output with an array of expected
 * values.
 */
static void check_modes(const char* testname, const char *buf, const char *lines[])
{
	int line = 1;
	while (*buf && *lines != NULL)
	{
		failure("On line %d of test %s", line, testname);
		assertEqualMem(buf, *lines, strlen(*lines));
		while (*buf && !(*buf == '\r'|| *buf == '\n'))
			buf++;
		while (*buf == '\r' || *buf == '\n')
			buf++;
		lines++;
		line++;
	}
	if (*lines && ! *buf)
	{
		failure("On line %d of test %s: unexpected end of file", line, testname);
		assert(0);
	}
}

DEFINE_TEST(test_option_mode)
{
	int rv;
	char *p;

	const char *test3mtree =
		"#mtree\n"
		"in/all mode=777 type=file\n"
		"in/minimal mode=500 type=file\n";

	const char *test3aExpected[] = {
		"-rw-r--r--",
		"-rw-r--r--",
		NULL
	};

	const char *test3bExpected[] = {
		"-rw-rwxr-x",
		"-rw---x---",
		NULL
	};

	assertMakeDir("in", 0755);

	/* Test invalid modes */
	rv = systemf("%s --mode 8 --format=pax -cf archive.tar in "
		"> 1.out 2> 1.err", testprog);
	assert(rv != 0);

	rv = systemf("%s --mode a+a --format=pax -cf archive.tar in "
		"> 2.out 2> 2.err", testprog);
	assert(rv != 0);

	/* Create some files with different modes */
	assertMakeFile("in/all", 0777, "");
	assertMakeFile("in/minimal", 0500, "");

	/* Archive and override using an absolute mode */
	assertEqualInt(0,
		systemf("%s --mode 644 --uid=0 --gid=0 --format=ustar "
			"-cf archive1.tar in/all in/minimal", testprog));

	/* Verify the modes */
	p = slurpfile(NULL, "archive1.tar");
	assertEqualString(p + 100,"000644 ");
	assertEqualString(p + 612,"000644 ");
	free(p);

/* Skip relative symbolic mode checks on Windows; on-disk files always have
 * mode 644.
 */
#if !defined(_WIN32) || defined(__CYGWIN__)
	/* Archive and override using a symbolic mode */
	assertEqualInt(0,
		systemf("%s --mode u+rw-x,g+X,o-w --uid=0 --gid=0 "
			"--format=ustar -cf archive2.tar in/all in/minimal",
			testprog));

	/* Verify the modes */
	p = slurpfile(NULL, "archive2.tar");
	assertEqualString(p + 100,"000675 ");
	assertEqualString(p + 612,"000610 ");
	free(p);
#endif

	/* Create an archive file with custom modes.  Using mtree here is
	 * simpler than building a tar file from files, and also works on
	 * Windows.
	 */
	assertMakeFile("archive3.mtree", 0644, test3mtree);

	/* list, overriding with absolute mode */
	assertEqualInt(0,
		systemf("%s --mode 644 -tvf archive3.mtree > 3a.out",
			testprog));
	p = slurpfile(NULL, "3a.out");
	check_modes("3a", p, test3aExpected);
	free(p);

	/* list, overriding with relative mode */
	assertEqualInt(0,
		systemf("%s --mode u+rw-x,g+X,o-w -tvf archive3.mtree > 3b.out",
			testprog));
	p = slurpfile(NULL, "3b.out");
	check_modes("3b", p, test3bExpected);
	free(p);

}
