/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <errno.h>
#include <glob.h>
#include <stdbool.h>

#include <atf-c.h>

ATF_TC(glob_b_callback);
ATF_TC_HEAD(glob_b_callback, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test ability of callback block to suppress errors");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(glob_b_callback, tc)
{
	static bool glob_callback_invoked;
	static int (^errblk)(const char *, int) =
	    ^(const char *path, int err) {
		ATF_CHECK_STREQ(path, "test/");
		ATF_CHECK(err == EACCES);
		glob_callback_invoked = true;
		/* Suppress EACCES errors. */
		return (0);
	};
	glob_t g;
	int rv;

	ATF_REQUIRE_EQ(0, mkdir("test", 0755));
	ATF_REQUIRE_EQ(0, symlink("foo", "test/foo"));
	ATF_REQUIRE_EQ(0, chmod("test", 0));

	glob_callback_invoked = false;
	rv = glob_b("test/*", 0, errblk, &g);
	ATF_CHECK_MSG(glob_callback_invoked,
	    "glob(3) failed to invoke callback block");
	ATF_CHECK_EQ_MSG(GLOB_NOMATCH, rv,
	    "callback function failed to suppress EACCES");
	globfree(&g);

	/* GLOB_ERR should ignore the suppressed error. */
	glob_callback_invoked = false;
	rv = glob_b("test/*", GLOB_ERR, errblk, &g);
	ATF_CHECK_MSG(glob_callback_invoked,
	    "glob(3) failed to invoke callback block");
	ATF_CHECK_EQ_MSG(GLOB_ABORTED, rv,
	    "GLOB_ERR didn't override callback block");
	globfree(&g);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, glob_b_callback);
	return (atf_no_error());
}
