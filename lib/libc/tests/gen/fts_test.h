/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef FTS_TEST_H_INCLUDED
#define FTS_TEST_H_INCLUDED

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

/* shorter name for dead links */
#define FTS_DL FTS_SLNONE

/* are we being debugged? */
static bool fts_test_debug;

/*
 * Set debug flag if appropriate.
 */
static void
fts_check_debug(void)
{
	fts_test_debug = !getenv("__RUNNING_INSIDE_ATF_RUN") &&
	    isatty(STDERR_FILENO);
}

/*
 * Lexical order for reproducability.
 */
static int
fts_lexical_compar(const FTSENT * const *a, const FTSENT * const *b)
{
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

/*
 * Run FTS with the specified paths and options and verify that it
 * produces the expected result in the correct order.
 */
static void
fts_test(const struct atf_tc *tc, const struct fts_testcase *fts_tc)
{
	FTS *fts;
	FTSENT *ftse;
	const struct fts_expect *expect = fts_tc->fts_expect;
	long level = 0;

	fts = fts_open(fts_tc->paths, fts_tc->fts_options, fts_lexical_compar);
	ATF_REQUIRE_MSG(fts != NULL, "fts_open(): %m");
	while ((ftse = fts_read(fts)) != NULL && expect->fts_name != NULL) {
		if (expect->fts_info == FTS_DP || expect->fts_info == FTS_DNR)
			level--;
		if (fts_test_debug) {
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

#endif /* FTS_TEST_H_INCLUDED */
