/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <errno.h>
#include <glob.h>

#include <atf-c.h>

static int glob_callback_invoked;

ATF_TC_WITHOUT_HEAD(glob_b_callback_test);
ATF_TC_BODY(glob_b_callback_test, tc)
{
	int rv;
	glob_t g;

	glob_callback_invoked = 0;
	ATF_REQUIRE_EQ(0, mkdir("test", 0007));
	int (^errblk)(const char *, int) =
	    ^(const char *path, int err) {
		ATF_REQUIRE_STREQ(path, "test/");
		ATF_REQUIRE(err == EACCES);
		glob_callback_invoked = 1;
		/* Suppress EACCES errors. */
		return (0);
	};

	rv = glob_b("test/*", 0, errblk, &g);
	ATF_REQUIRE_MSG(glob_callback_invoked == 1,
	    "glob(3) failed to invoke callback block");
	ATF_REQUIRE_MSG(rv == GLOB_NOMATCH,
	    "error callback function failed to suppress EACCES");

	/* GLOB_ERR should ignore the suppressed error. */
	rv = glob_b("test/*", GLOB_ERR, errblk, &g);
	ATF_REQUIRE_MSG(rv == GLOB_ABORTED,
	    "GLOB_ERR didn't override error callback block");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, glob_b_callback_test);
	return (atf_no_error());
}
