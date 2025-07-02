/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <fcntl.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#include "fts_test.h"

ATF_TC(fts_unrdir);
ATF_TC_HEAD(fts_unrdir, tc)
{
	atf_tc_set_md_var(tc, "descr", "unreadable directories");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(fts_unrdir, tc)
{
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/unr", 0100));
	ATF_REQUIRE_EQ(0, mkdir("dir/unx", 0400));
	fts_test(tc, &(struct fts_testcase){
		    (char *[]){ "dir", NULL },
		    FTS_PHYSICAL,
		    (struct fts_expect[]){
			    { FTS_D,	"dir",	"dir" },
			    { FTS_D,	"unr",	"unr" },
			    { FTS_DNR,	"unr",	"unr" },
			    { FTS_D,	"unx",	"unx" },
			    { FTS_DP,	"unx",	"unx" },
			    { FTS_DP,	"dir",	"dir" },
			    { 0 }
		    },
	    });
}

ATF_TC(fts_unrdir_nochdir);
ATF_TC_HEAD(fts_unrdir_nochdir, tc)
{
	atf_tc_set_md_var(tc, "descr", "unreadable directories (nochdir)");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(fts_unrdir_nochdir, tc)
{
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/unr", 0100));
	ATF_REQUIRE_EQ(0, mkdir("dir/unx", 0400));
	fts_test(tc, &(struct fts_testcase){
		    (char *[]){ "dir", NULL },
		    FTS_PHYSICAL | FTS_NOCHDIR,
		    (struct fts_expect[]){
			    { FTS_D,	"dir",	"dir" },
			    { FTS_D,	"unr",	"dir/unr" },
			    { FTS_DNR,	"unr",	"dir/unr" },
			    { FTS_D,	"unx",	"dir/unx" },
			    { FTS_DP,	"unx",	"dir/unx" },
			    { FTS_DP,	"dir",	"dir" },
			    { 0 }
		    },
	    });
}

ATF_TP_ADD_TCS(tp)
{
	fts_check_debug();
	ATF_TP_ADD_TC(tp, fts_unrdir);
	ATF_TP_ADD_TC(tp, fts_unrdir_nochdir);
	return (atf_no_error());
}
