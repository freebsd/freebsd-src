/*
 * Copyright (c) 2017 Dell EMC Isilon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * Derived from Russ Cox' pathological case test program used for the
 * https://research.swtch.com/glob article.
 */
ATF_TC_WITHOUT_HEAD(glob_pathological_test);
ATF_TC_BODY(glob_pathological_test, tc)
{
	struct timespec t, t2;
	glob_t g;
	const char *longname = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	char pattern[1000], *p;
	double dt;
	unsigned i, j, k, mul;
	int fd, rc;

	fd = open(longname, O_CREAT | O_RDWR, 0666);
	ATF_REQUIRE(fd >= 0);

	/*
	 * Test up to 100 a* groups.  Exponential implementations typically go
	 * bang at i=7 or 8.
	 */
	for (i = 0; i < 100; i++) {
		/*
		 * Create a*...b pattern with i 'a*' groups.
		 */
		p = pattern;
		for (k = 0; k < i; k++) {
			*p++ = 'a';
			*p++ = '*';
		}
		*p++ = 'b';
		*p = '\0';

		clock_gettime(CLOCK_REALTIME, &t);
		for (j = 0; j < mul; j++) {
			memset(&g, 0, sizeof g);
			rc = glob(pattern, 0, 0, &g);
			if (rc == GLOB_NOSPACE || rc == GLOB_ABORTED) {
				ATF_REQUIRE_MSG(rc == GLOB_NOMATCH,
				    "an unexpected error occurred: "
				    "rc=%d errno=%d", rc, errno);
				/* NORETURN */
			}

			ATF_CHECK_MSG(rc == GLOB_NOMATCH,
			    "A bogus match occurred: '%s' ~ '%s'", pattern,
			    g.gl_pathv[0]);
			globfree(&g);
		}
		clock_gettime(CLOCK_REALTIME, &t2);

		t2.tv_sec -= t.tv_sec;
		t2.tv_nsec -= t.tv_nsec;
		dt = t2.tv_sec + (double)t2.tv_nsec/1e9;
		dt /= mul;

		ATF_CHECK_MSG(dt < 1, "glob(3) took far too long: %d %.9f", i,
		    dt);

		if (dt >= 0.0001)
			mul = 1;
	}
}

ATF_TC(glob_period);
ATF_TC_HEAD(glob_period, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test behaviour when matching files that start with a period"
	    "(documented in the glob(3) CAVEATS section).");
}
ATF_TC_BODY(glob_period, tc)
{
	int i;
	glob_t g;

	atf_utils_create_file(".test", "");
	glob(".", 0, NULL, &g);
	ATF_REQUIRE_MSG(g.gl_matchc == 1,
	    "glob(3) shouldn't match files starting with a period when using '.'");
	for (i = 0; i < g.gl_matchc; i++)
		printf("%s\n", g.gl_pathv[i]);
	glob(".*", 0, NULL, &g);
	ATF_REQUIRE_MSG(g.gl_matchc == 3 && strcmp(g.gl_pathv[2], ".test") == 0,
	    "glob(3) should match files starting with a period when using '.*'");
}

static bool glob_callback_invoked;

static int
errfunc(const char *path, int err)
{
	ATF_CHECK_STREQ(path, "test/");
	ATF_CHECK(err == EACCES);
	glob_callback_invoked = true;
	/* Suppress EACCES errors. */
	return (0);
}

ATF_TC(glob_callback);
ATF_TC_HEAD(glob_callback, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test ability of callback function to suppress errors");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(glob_callback, tc)
{
	glob_t g;
	int rv;

	ATF_REQUIRE_EQ(0, mkdir("test", 0755));
	ATF_REQUIRE_EQ(0, symlink("foo", "test/foo"));
	ATF_REQUIRE_EQ(0, chmod("test", 0));

	glob_callback_invoked = false;
	rv = glob("test/*", 0, errfunc, &g);
	ATF_CHECK_MSG(glob_callback_invoked,
	    "glob(3) failed to invoke callback function");
	ATF_CHECK_EQ_MSG(GLOB_NOMATCH, rv,
	    "callback function failed to suppress EACCES");
	globfree(&g);

	/* GLOB_ERR should ignore the suppressed error. */
	glob_callback_invoked = false;
	rv = glob("test/*", GLOB_ERR, errfunc, &g);
	ATF_CHECK_MSG(glob_callback_invoked,
	    "glob(3) failed to invoke callback function");
	ATF_CHECK_EQ_MSG(GLOB_ABORTED, rv,
	    "GLOB_ERR didn't override callback function");
	globfree(&g);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, glob_pathological_test);
	ATF_TP_ADD_TC(tp, glob_period);
	ATF_TP_ADD_TC(tp, glob_callback);
	return (atf_no_error());
}
