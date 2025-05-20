/*-SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2024 Tarsnap Backup Inc.
 * All rights reserved.
 */
#include "test.h"

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <winbase.h>
#endif

/* These lists of files come from 'test_list_archive.tar.uu', which includes
 * the script which generated it. */

static const char *tf_out =
"f\n"
"hl\n"
"sl\n"
"d/\n"
"d/f\n"
"fake-username\n"
"fake-groupname\n"
"f\n";

#if defined(_WIN32) && !defined(__CYGWIN__)
static const char *tvf_out =
"-rw-r--r--  0 1000   1000        0 Jan 01  1980 f\n"
"hrw-r--r--  0 1000   1000        0 Jan 01  1980 hl link to f\n"
"lrwxr-xr-x  0 1000   1000        0 Jan 01  1980 sl -> f\n"
"drwxrwxrwx  0 1000   1000        0 Jan 01  1980 d/\n"
"-r--------  0 1000   1000        0 Jan 01  1980 d/f\n"
"-rw-r--r--  0 long-fake-uname 1000        0 Jan 01  1980 fake-username\n"
"-rw-r--r--  0 1000            long-fake-gname 0 Jan 01  1980 fake-groupname\n"
"-rw-r--r--  0 1000            1000            0 Jan 01  1980 f\n";
#else
static const char *tvf_out =
"-rw-r--r--  0 1000   1000        0 Jan  1  1980 f\n"
"hrw-r--r--  0 1000   1000        0 Jan  1  1980 hl link to f\n"
"lrwxr-xr-x  0 1000   1000        0 Jan  1  1980 sl -> f\n"
"drwxrwxrwx  0 1000   1000        0 Jan  1  1980 d/\n"
"-r--------  0 1000   1000        0 Jan  1  1980 d/f\n"
"-rw-r--r--  0 long-fake-uname 1000        0 Jan  1  1980 fake-username\n"
"-rw-r--r--  0 1000            long-fake-gname 0 Jan  1  1980 fake-groupname\n"
"-rw-r--r--  0 1000            1000            0 Jan  1  1980 f\n";
#endif

static void
set_lc_time(const char * str)
{

#if defined(_WIN32) && !defined(__CYGWIN__)
	if (!SetEnvironmentVariable("LC_TIME", str)) {
		fprintf(stderr, "SetEnvironmentVariable failed with %d\n",
		    (int)GetLastError());
	}
#else
	if (setenv("LC_TIME", str, 1) == -1)
		fprintf(stderr, "setenv: %s\n", strerror(errno));
#endif
}

static int
run_tvf(void)
{
	char * orig_lc_time;
	char * lc_time;
	int exact_tvf_check;

	orig_lc_time = getenv("LC_TIME");

	/* Try to set LC_TIME to known (English) dates. */
	set_lc_time("en_US.UTF-8");

	/* Check if we've got the right LC_TIME; if not, don't check output. */
	lc_time = getenv("LC_TIME");
	if ((lc_time != NULL) && strcmp(lc_time, "en_US.UTF-8") == 0)
		exact_tvf_check = 1;
	else
		exact_tvf_check = 0;

	assertEqualInt(0,
	    systemf("%s tvf test_list_item.tar >tvf.out 2>tvf.err", testprog));

	/* Restore the original date formatting. */
	if (orig_lc_time != NULL)
		set_lc_time(orig_lc_time);

	return (exact_tvf_check);
}

DEFINE_TEST(test_list_item)
{
	int exact_tvf_check;

	extract_reference_file("test_list_item.tar");

	/* Run 'tf' and check output. */
	assertEqualInt(0,
	    systemf("%s tf test_list_item.tar >tf.out 2>tf.err", testprog));
	failure("'t' mode should write results to stdout");
	assertTextFileContents(tf_out, "tf.out");
	assertEmptyFile("tf.err");

	/* Run 'tvf'. */
	exact_tvf_check = run_tvf();

	/* Check 'tvf' output. */
	failure("'t' mode with 'v' should write more results to stdout");
	assertEmptyFile("tvf.err");
	if (exact_tvf_check)
		assertTextFileContents(tvf_out, "tvf.out");
	else {
		/* The 'skipping' macro requires braces. */
		skipping("Can't check exact tvf output");
	}
}
