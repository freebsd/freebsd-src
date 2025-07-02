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

struct fts_expect {
	int fts_info;
	const char *fts_name;
	const char *fts_accpath;
};

struct fts_testcase {
	char **paths;
	int fts_options;
	struct fts_expect *fts_expect;
};

static char *all_paths[] = {
	"dir",
	"dirl",
	"file",
	"filel",
	"dead",
	"noent",
	NULL
};

/* shorter name for dead links */
#define FTS_DL FTS_SLNONE

/* are we being debugged? */
static bool debug;

/*
 * Prepare the files and directories we will be inspecting.
 */
static void
fts_options_prepare(const struct atf_tc *tc)
{
	debug = !getenv("__RUNNING_INSIDE_ATF_RUN") &&
	    isatty(STDERR_FILENO);
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("file", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));
	ATF_REQUIRE_EQ(0, symlink("..", "dir/up"));
	ATF_REQUIRE_EQ(0, symlink("dir", "dirl"));
	ATF_REQUIRE_EQ(0, symlink("file", "filel"));
	ATF_REQUIRE_EQ(0, symlink("noent", "dead"));
}

/*
 * Lexical order for reproducability.
 */
static int
fts_options_compar(const FTSENT * const *a, const FTSENT * const *b)
{
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

/*
 * Run FTS with the specified paths and options and verify that it
 * produces the expected result in the correct order.
 */
static void
fts_options_test(const struct atf_tc *tc, const struct fts_testcase *fts_tc)
{
	FTS *fts;
	FTSENT *ftse;
	const struct fts_expect *expect = fts_tc->fts_expect;
	long level = 0;

	fts = fts_open(fts_tc->paths, fts_tc->fts_options, fts_options_compar);
	ATF_REQUIRE_MSG(fts != NULL, "fts_open(): %m");
	while ((ftse = fts_read(fts)) != NULL && expect->fts_name != NULL) {
		if (expect->fts_info == FTS_DP)
			level--;
		if (debug) {
			fprintf(stderr, "%2ld %2d %s\n", level,
			    ftse->fts_info, ftse->fts_name);
		}
		ATF_CHECK_STREQ(expect->fts_name, ftse->fts_name);
		ATF_CHECK_STREQ(expect->fts_accpath, ftse->fts_accpath);
		ATF_CHECK_INTEQ(expect->fts_info, ftse->fts_info);
		ATF_CHECK_INTEQ(level, ftse->fts_level);
		if (expect->fts_info == FTS_D)
			level++;
		expect++;
	}
	ATF_CHECK_EQ(NULL, ftse);
	ATF_CHECK_EQ(NULL, expect->fts_name);
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

ATF_TC(fts_options_logical);
ATF_TC_HEAD(fts_options_logical, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_LOGICAL");
}
ATF_TC_BODY(fts_options_logical, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_LOGICAL,
		    (struct fts_expect[]){
			    { FTS_DL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_F,	"file",		"dir/file"	},
			    { FTS_D,	"up",		"dir/up"	},
			    { FTS_DL,	"dead",		"dir/up/dead"	},
			    { FTS_DC,	"dir",		"dir/up/dir"	},
			    { FTS_DC,	"dirl",		"dir/up/dirl"	},
			    { FTS_F,	"file",		"dir/up/file"	},
			    { FTS_F,	"filel",	"dir/up/filel"	},
			    { FTS_DP,	"up",		"dir/up"	},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_D,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"dirl/file"	},
			    { FTS_D,	"up",		"dirl/up"	},
			    { FTS_DL,	"dead",		"dirl/up/dead"	},
			    { FTS_DC,	"dir",		"dirl/up/dir"	},
			    { FTS_DC,	"dirl",		"dirl/up/dirl"	},
			    { FTS_F,	"file",		"dirl/up/file"	},
			    { FTS_F,	"filel",	"dirl/up/filel"	},
			    { FTS_DP,	"up",		"dirl/up"	},
			    { FTS_DP,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_F,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_logical_nostat);
ATF_TC_HEAD(fts_options_logical_nostat, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_LOGICAL | FTS_NOSTAT");
}
ATF_TC_BODY(fts_options_logical_nostat, tc)
{
	/*
	 * While FTS_LOGICAL is not documented as being incompatible with
	 * FTS_NOSTAT, and FTS does not clear FTS_NOSTAT if FTS_LOGICAL is
	 * set, FTS_LOGICAL effectively nullifies FTS_NOSTAT by overriding
	 * the follow check in fts_stat().  In theory, FTS could easily be
	 * changed to only stat links (to check what they point to) in the
	 * FTS_LOGICAL | FTS_NOSTAT case, which would produce a different
	 * result here, so keep the test around in case that ever happens.
	 */
	atf_tc_expect_fail("FTS_LOGICAL nullifies FTS_NOSTAT");
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_LOGICAL | FTS_NOSTAT,
		    (struct fts_expect[]){
			    { FTS_DL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_NSOK,	"file",		"dir/file"	},
			    { FTS_D,	"up",		"dir/up"	},
			    { FTS_DL,	"dead",		"dir/up/dead"	},
			    { FTS_DC,	"dir",		"dir/up/dir"	},
			    { FTS_DC,	"dirl",		"dir/up/dirl"	},
			    { FTS_NSOK,	"file",		"dir/up/file"	},
			    { FTS_NSOK,	"filel",	"dir/up/filel"	},
			    { FTS_DP,	"up",		"dir/up"	},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_D,	"dirl",		"dirl"		},
			    { FTS_NSOK,	"file",		"dirl/file"	},
			    { FTS_D,	"up",		"dirl/up"	},
			    { FTS_DL,	"dead",		"dirl/up/dead"	},
			    { FTS_DC,	"dir",		"dirl/up/dir"	},
			    { FTS_DC,	"dirl",		"dirl/up/dirl"	},
			    { FTS_NSOK,	"file",		"dirl/up/file"	},
			    { FTS_NSOK,	"filel",	"dirl/up/filel"	},
			    { FTS_DP,	"up",		"dirl/up"	},
			    { FTS_DP,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_F,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_logical_seedot);
ATF_TC_HEAD(fts_options_logical_seedot, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_LOGICAL | FTS_SEEDOT");
}
ATF_TC_BODY(fts_options_logical_seedot, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_LOGICAL | FTS_SEEDOT,
		    (struct fts_expect[]){
			    { FTS_DL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_DOT,	".",		"dir/."		},
			    { FTS_DOT,	"..",		"dir/.."	},
			    { FTS_F,	"file",		"dir/file"	},
			    { FTS_D,	"up",		"dir/up"	},
			    { FTS_DOT,	".",		"dir/up/."	},
			    { FTS_DOT,	"..",		"dir/up/.."	},
			    { FTS_DL,	"dead",		"dir/up/dead"	},
			    { FTS_DC,	"dir",		"dir/up/dir"	},
			    { FTS_DC,	"dirl",		"dir/up/dirl"	},
			    { FTS_F,	"file",		"dir/up/file"	},
			    { FTS_F,	"filel",	"dir/up/filel"	},
			    { FTS_DP,	"up",		"dir/up"	},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_D,	"dirl",		"dirl"		},
			    { FTS_DOT,	".",		"dirl/."	},
			    { FTS_DOT,	"..",		"dirl/.."	},
			    { FTS_F,	"file",		"dirl/file"	},
			    { FTS_D,	"up",		"dirl/up"	},
			    { FTS_DOT,	".",		"dirl/up/."	},
			    { FTS_DOT,	"..",		"dirl/up/.."	},
			    { FTS_DL,	"dead",		"dirl/up/dead"	},
			    { FTS_DC,	"dir",		"dirl/up/dir"	},
			    { FTS_DC,	"dirl",		"dirl/up/dirl"	},
			    { FTS_F,	"file",		"dirl/up/file"	},
			    { FTS_F,	"filel",	"dirl/up/filel"	},
			    { FTS_DP,	"up",		"dirl/up"	},
			    { FTS_DP,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_F,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_physical);
ATF_TC_HEAD(fts_options_physical, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_PHYSICAL");
}
ATF_TC_BODY(fts_options_physical, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_PHYSICAL,
		    (struct fts_expect[]){
			    { FTS_SL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"up",		"up"		},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_SL,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_physical_nochdir);
ATF_TC_HEAD(fts_options_physical_nochdir, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_PHYSICAL | FTS_NOCHDIR");
}
ATF_TC_BODY(fts_options_physical_nochdir, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_PHYSICAL | FTS_NOCHDIR,
		    (struct fts_expect[]){
			    { FTS_SL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_F,	"file",		"dir/file"	},
			    { FTS_SL,	"up",		"dir/up"	},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_SL,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_physical_comfollow);
ATF_TC_HEAD(fts_options_physical_comfollow, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_PHYSICAL | FTS_COMFOLLOW");
}
ATF_TC_BODY(fts_options_physical_comfollow, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_PHYSICAL | FTS_COMFOLLOW,
		    (struct fts_expect[]){
			    { FTS_DL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"up",		"up"		},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_D,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"up",		"up"		},
			    { FTS_DP,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_F,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_physical_comfollowdir);
ATF_TC_HEAD(fts_options_physical_comfollowdir, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_PHYSICAL | FTS_COMFOLLOWDIR");
}
ATF_TC_BODY(fts_options_physical_comfollowdir, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_PHYSICAL | FTS_COMFOLLOWDIR,
		    (struct fts_expect[]){
			    { FTS_DL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"up",		"up"		},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_D,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"up",		"up"		},
			    { FTS_DP,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_physical_nostat);
ATF_TC_HEAD(fts_options_physical_nostat, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_PHYSICAL | FTS_NOSTAT");
}
ATF_TC_BODY(fts_options_physical_nostat, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_PHYSICAL | FTS_NOSTAT,
		    (struct fts_expect[]){
			    { FTS_SL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_NSOK,	"file",		"file"		},
			    { FTS_NSOK,	"up",		"up"		},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_SL,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_physical_nostat_type);
ATF_TC_HEAD(fts_options_physical_nostat_type, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_PHYSICAL | FTS_NOSTAT_TYPE");
}
ATF_TC_BODY(fts_options_physical_nostat_type, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_PHYSICAL | FTS_NOSTAT_TYPE,
		    (struct fts_expect[]){
			    { FTS_SL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"up",		"up"		},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_SL,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

ATF_TC(fts_options_physical_seedot);
ATF_TC_HEAD(fts_options_physical_seedot, tc)
{
	atf_tc_set_md_var(tc, "descr", "FTS_PHYSICAL | FTS_SEEDOT");
}
ATF_TC_BODY(fts_options_physical_seedot, tc)
{
	fts_options_prepare(tc);
	fts_options_test(tc, &(struct fts_testcase){
		    all_paths,
		    FTS_PHYSICAL | FTS_SEEDOT,
		    (struct fts_expect[]){
			    { FTS_SL,	"dead",		"dead"		},
			    { FTS_D,	"dir",		"dir"		},
			    { FTS_DOT,	".",		"."		},
			    { FTS_DOT,	"..",		".."		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"up",		"up"		},
			    { FTS_DP,	"dir",		"dir"		},
			    { FTS_SL,	"dirl",		"dirl"		},
			    { FTS_F,	"file",		"file"		},
			    { FTS_SL,	"filel",	"filel"		},
			    { FTS_NS,	"noent",	"noent"		},
			    { 0 }
		    },
	    });
}

/*
 * TODO: Add tests for FTS_XDEV and FTS_WHITEOUT
 */

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fts_options_logical);
	ATF_TP_ADD_TC(tp, fts_options_logical_nostat);
	ATF_TP_ADD_TC(tp, fts_options_logical_seedot);
	ATF_TP_ADD_TC(tp, fts_options_physical);
	ATF_TP_ADD_TC(tp, fts_options_physical_nochdir);
	ATF_TP_ADD_TC(tp, fts_options_physical_comfollow);
	ATF_TP_ADD_TC(tp, fts_options_physical_comfollowdir);
	ATF_TP_ADD_TC(tp, fts_options_physical_nostat);
	ATF_TP_ADD_TC(tp, fts_options_physical_nostat_type);
	ATF_TP_ADD_TC(tp, fts_options_physical_seedot);
	return (atf_no_error());
}
