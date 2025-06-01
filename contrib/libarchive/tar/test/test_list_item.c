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

DEFINE_TEST(test_list_item)
{
	extract_reference_file("test_list_item.tar");

	/* Run 'tf' and check output. */
	assertEqualInt(0,
	    systemf("%s tf test_list_item.tar >tf.out 2>tf.err", testprog));
	failure("'t' mode should write results to stdout");
	assertTextFileContents(tf_out, "tf.out");
	assertEmptyFile("tf.err");

	/* Run 'tvf' and check output. */
	assertEqualInt(0,
	    systemf("%s tvf test_list_item.tar >tvf.out 2>tvf.err", testprog));
	failure("'t' mode with 'v' should write more results to stdout");
	assertTextFileContents(tvf_out, "tvf.out");
	assertEmptyFile("tvf.err");
}
