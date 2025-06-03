/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

/*
 * This data would fit onto one line, but it's easier to understand when it's
 * on mulitple lines (and thus matches the output files).
 */
static const char *cvf_err =
"a f\n"
"a l\n";

static const char *tf_out =
"f\n"
"l\n";

static const char *xvf_err =
"x f\n"
"x l\n";

/*
 * This string should appear in the verbose listing regardless of platform,
 * locale, username, or groupname.
 */
static const char * tvf_contains = "l link to f";

DEFINE_TEST(test_stdio)
{
	FILE *filelist;
	char *p;
	size_t s;
	int r;

	assertUmask(0);

	/*
	 * Create a couple of files on disk.
	 */
	/* File */
	assertMakeFile("f", 0755, "abc");
	/* Link to above file. */
	assertMakeHardlink("l", "f");

	/* Create file list (text mode here) */
	filelist = fopen("filelist", "w");
	assert(filelist != NULL);
	fprintf(filelist, "f\n");
	fprintf(filelist, "l\n");
	fclose(filelist);

	/*
	 * Archive/dearchive with a variety of options, verifying
	 * stdio paths.
	 */

	/* 'cf' should generate no output unless there's an error. */
	r = systemf("%s cf archive f l >cf.out 2>cf.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("cf.out");
	assertEmptyFile("cf.err");

	/* 'cvf' should generate file list on stderr, empty stdout. */
	r = systemf("%s cvf archive f l >cvf.out 2>cvf.err", testprog);
	assertEqualInt(r, 0);
	failure("'cv' writes filenames to stderr, nothing to stdout (SUSv2)\n"
	    "Note that GNU tar writes the file list to stdout by default.");
	assertEmptyFile("cvf.out");
	assertTextFileContents(cvf_err, "cvf.err");

	/* 'cvf -' should generate file list on stderr, archive on stdout. */
	r = systemf("%s cvf - f l >cvf-.out 2>cvf-.err", testprog);
	assertEqualInt(r, 0);
	failure("cvf - should write archive to stdout");
	failure("cvf - should write file list to stderr (SUSv2)");
	assertEqualFile("cvf.err", "cvf-.err");
	/* Check that stdout from 'cvf -' was a valid archive. */
	r = systemf("%s tf cvf-.out >cvf-tf.out 2>cvf-tf.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("cvf-tf.err");
	assertTextFileContents(tf_out, "cvf-tf.out");

	/* 'tf' should generate file list on stdout, empty stderr. */
	r = systemf("%s tf archive >tf.out 2>tf.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("tf.err");
	failure("'t' mode should write results to stdout");
	assertTextFileContents(tf_out, "tf.out");

	/* 'tvf' should generate file list on stdout, empty stderr. */
	r = systemf("%s tvf archive >tvf.out 2>tvf.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("tvf.err");
	failure("'tv' mode should write results to stdout");
	/* Check that it contains a string only found in the verbose listing. */
	p = slurpfile(&s, "%s", "tvf.out");
	assert(strstr(p, tvf_contains) != NULL);
	free(p);

	/* 'tvf -' uses stdin, file list on stdout, empty stderr. */
	r = systemf("%s tvf - < archive >tvf-.out 2>tvf-.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("tvf-.err");
	failure("'tvf-' mode should write the same results as 'tvf'");
	assertEqualFile("tvf.out", "tvf-.out");

	/* Basic 'xf' should generate no output on stdout or stderr. */
	r = systemf("%s xf archive >xf.out 2>xf.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("xf.err");
	assertEmptyFile("xf.out");

	/* 'xvf' should generate list on stderr, empty stdout. */
	r = systemf("%s xvf archive >xvf.out 2>xvf.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("xvf.out");
	assertTextFileContents(xvf_err, "xvf.err");

	/* 'xvOf' should generate list on stderr, file contents on stdout. */
	r = systemf("%s xvOf archive >xvOf.out 2>xvOf.err", testprog);
	assertEqualInt(r, 0);
	/* Verify xvOf.out is the file contents */
	p = slurpfile(&s, "xvOf.out");
	assertEqualInt((int)s, 3);
	assertEqualMem(p, "abc", 3);
	assertEqualFile("xvf.err", "xvOf.err");
	free(p);

	/* 'xvf -' should generate list on stderr, empty stdout. */
	r = systemf("%s xvf - < archive >xvf-.out 2>xvf-.err", testprog);
	assertEqualInt(r, 0);
	assertEmptyFile("xvf-.out");
	assertEqualFile("xvf.err", "xvf-.err");
}
