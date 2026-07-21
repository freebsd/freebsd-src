/*-
 * Copyright (c) 2026 Dag-Erling Smørgrav
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/wait.h>

#include <libutil.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define INVALID_PREFIX "/dev/null/"

#define X31 "/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define X32 "/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define X64 X32 X32
#define X128 X64 X64
#define X256 X128 X128
#define X512 X256 X256
#define X1024 X512 X512
#define X1023 X512 X256 X128 X64 X32 X31

static struct {
	const char *in, *out;
} tests[] = {
	{ "", _PATH_LOCALBASE },
	{ "/", "/" },
	{ "//", "/" },
	{ "///", "/" },
	{ "foo", INVALID_PREFIX },
	{ "/foo", "/foo" },
	{ "//foo", "/foo" },
	{ "/foo/", "/foo" },
	{ "/foo//", "/foo" },
	{ "//foo//", "/foo" },
	{ "/foo/bar", "/foo/bar" },
	{ "/foo/bar/", "/foo/bar" },
	{ "/foo//bar", "/foo/bar" },
	{ "/foo//bar/", "/foo/bar" },
	{ "//foo/bar", "/foo/bar" },
	{ "//foo/bar/", "/foo/bar" },
	{ "//foo//bar", "/foo/bar" },
	{ "//foo//bar/", "/foo/bar" },
	{ _PATH_LOCALBASE, _PATH_LOCALBASE },
	{ X31, X31 },
	{ X32, X32 },
	{ X64, X64 },
	{ X128, X128 },
	{ X256, X256 },
	{ X512, X512 },
	{ X1023, X1023 },
	{ X1024, INVALID_PREFIX },
	{ 0 }
};
_Static_assert(sizeof(X1023) == MAXPATHLEN, "Unexpected MAXPATHLEN value");

ATF_TC(environment);
ATF_TC_HEAD(environment, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests getting the local base from the "
	    "environment and path normalization");
}

ATF_TC_BODY(environment, tc)
{
	pid_t pid;
	int i, wstatus;

	for (i = 0; tests[i].in != NULL; i++) {
		ATF_REQUIRE((pid = fork()) >= 0);
		if (pid == 0) {
			ATF_REQUIRE_EQ(setenv("LOCALBASE", tests[i].in, 1), 0);
			ATF_REQUIRE_STREQ(getlocalbase(), tests[i].out);
			_exit(0);
		}
		ATF_REQUIRE_EQ(waitpid(0, &wstatus, 0), pid);
		ATF_CHECK(WIFEXITED(wstatus));
		ATF_CHECK_EQ(WEXITSTATUS(wstatus), 0);
	}
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, environment);
	return (atf_no_error());
}
