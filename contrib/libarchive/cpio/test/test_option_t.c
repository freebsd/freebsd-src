/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "test.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

DEFINE_TEST(test_option_t)
{
	char *p;
	int r;
	time_t mtime;
	char date[48];
	char date2[32];
	struct tm *tmptr;
#if defined(HAVE_LOCALTIME_R) || defined(HAVE_LOCALTIME_S)
	struct tm tmbuf;
#endif

	/* List reference archive, make sure the TOC is correct. */
	extract_reference_file("test_option_t.cpio");
	r = systemf("%s -it < test_option_t.cpio >it.out 2>it.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "it.err");
	extract_reference_file("test_option_t.stdout");
	p = slurpfile(NULL, "test_option_t.stdout");
	assertTextFileContents(p, "it.out");
	free(p);

	/* We accept plain "-t" as a synonym for "-it" */
	r = systemf("%s -t < test_option_t.cpio >t.out 2>t.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "t.err");
	extract_reference_file("test_option_t.stdout");
	p = slurpfile(NULL, "test_option_t.stdout");
	assertTextFileContents(p, "t.out");
	free(p);

	/* But "-ot" is an error. */
	assert(0 != systemf("%s -ot < test_option_t.cpio >ot.out 2>ot.err",
			    testprog));
	assertEmptyFile("ot.out");

	/* List reference archive verbosely, make sure the TOC is correct. */
	r = systemf("%s -itv < test_option_t.cpio >tv.out 2>tv.err", testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "tv.err");
	extract_reference_file("test_option_tv.stdout");

	/* This doesn't work because the usernames on different systems
	 * are different and cpio now looks up numeric UIDs on
	 * the local system. */
	/* assertEqualFile("tv.out", "test_option_tv.stdout"); */

	/* List reference archive with numeric IDs, verify TOC is correct. */
	r = systemf("%s -itnv < test_option_t.cpio >itnv.out 2>itnv.err",
		    testprog);
	assertEqualInt(r, 0);
	assertTextFileContents("1 block\n", "itnv.err");
	p = slurpfile(NULL, "itnv.out");
	/* Since -n uses numeric UID/GID, this part should be the
	 * same on every system. */
	assertEqualMem(p, "-rw-r--r--   1 1000     1000            0 ",42);

	/* Date varies depending on local timezone and locale. */
	mtime = 1;
#ifdef HAVE_LOCALE_H
	setlocale(LC_ALL, "");
	setlocale(LC_TIME, "");
#endif
#if defined(HAVE_LOCALTIME_S)
        tmptr = localtime_s(&tmbuf, &mtime) ? NULL : &tmbuf;
#elif defined(HAVE_LOCALTIME_R)
        tmptr = localtime_r(&mtime, &tmbuf);
#else
        tmptr = localtime(&mtime);
#endif
#if defined(_WIN32) && !defined(__CYGWIN__)
	strftime(date2, sizeof(date2)-1, "%b %d  %Y", tmptr);
	_snprintf(date, sizeof(date)-1, "%12s file", date2);
#else
	strftime(date2, sizeof(date2)-1, "%b %e  %Y", tmptr);
	snprintf(date, sizeof(date)-1, "%12s file", date2);
#endif
	assertEqualMem(p + 42, date, strlen(date));
	free(p);

	/* But "-n" without "-t" is an error. */
	assert(0 != systemf("%s -in < test_option_t.cpio >in.out 2>in.err",
			    testprog));
	assertEmptyFile("in.out");
}
