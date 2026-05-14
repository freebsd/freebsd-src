/*-
 * Copyright (c) 2026 Jitendra Bhati
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Tests for fts_open() error conditions and edge cases.
 *
 * The options validation in fts_open() is:
 *
 *     if (options & ~FTS_OPTIONMASK) { errno = EINVAL; return NULL; }
 *
 * This rejects bits OUTSIDE the mask, not missing required bits.
 * The ACTUAL EINVAL cases are:
 *   1. Option bits outside FTS_OPTIONMASK.
 *   2. Empty argv (NULL as first element).
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <atf-c.h>

#include "fts_test.h"

/*
 * fts_open() with option bits outside FTS_OPTIONMASK must fail with EINVAL.
 * FTS_OPTIONMASK is 0x000cff; any bit above that is invalid.
 */
ATF_TC(fts_open_invalid_options);
ATF_TC_HEAD(fts_open_invalid_options, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_open with out-of-mask option bits fails with EINVAL");
}
ATF_TC_BODY(fts_open_invalid_options, tc)
{
	char *paths[] = { ".", NULL };
	FTS *fts;

	/*
	 * 0x10000 has bits well outside FTS_OPTIONMASK (0x000cff).
	 * The options check in fts_open() rejects this immediately.
	 */
	fts = fts_open(paths, 0x10000, NULL);
	ATF_REQUIRE_MSG(fts == NULL,
	    "fts_open should fail with invalid option bits");
	ATF_REQUIRE_EQ_MSG(EINVAL, errno,
	    "expected EINVAL for invalid options, got %d", errno);
}

/*
 * fts_open() with an empty argv (NULL as first element) must fail with EINVAL.
 * This is the second validation guard in fts_open(), immediately after the
 * options check.
 */
ATF_TC(fts_open_empty_argv);
ATF_TC_HEAD(fts_open_empty_argv, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_open with NULL first argv element fails with EINVAL");
}
ATF_TC_BODY(fts_open_empty_argv, tc)
{
	char *paths[] = { NULL };
	FTS *fts;

	fts = fts_open(paths, FTS_PHYSICAL, NULL);
	ATF_REQUIRE_MSG(fts == NULL,
	    "fts_open should fail with empty argv");
	ATF_REQUIRE_EQ_MSG(EINVAL, errno,
	    "expected EINVAL for empty argv, got %d", errno);
}

/*
 * An empty string in argv is a valid (non-NULL) path but stat("") fails
 * with ENOENT.  fts_open() succeeds; the resulting FTSENT has
 * fts_info == FTS_NS and fts_errno == ENOENT.
 */
ATF_TC(fts_open_empty_path_string);
ATF_TC_HEAD(fts_open_empty_path_string, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "empty string in argv produces FTS_NS entry");
}
ATF_TC_BODY(fts_open_empty_path_string, tc)
{
	char *paths[] = { "", NULL };
	FTS *fts;
	FTSENT *ent;

	fts = fts_open(paths, FTS_PHYSICAL, NULL);
	ATF_REQUIRE_MSG(fts != NULL, "fts_open(): %m");

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_CHECK_EQ(FTS_NS, ent->fts_info);
	ATF_CHECK_EQ(ENOENT, ent->fts_errno);

	fts_close(fts);
}

/*
 * A nonexistent path in argv produces an FTS_NS entry (stat failed) rather
 * than causing fts_open() itself to fail.  fts_open() does not validate
 * whether the paths actually exist.
 */
ATF_TC(fts_open_nonexistent_path);
ATF_TC_HEAD(fts_open_nonexistent_path, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "nonexistent path produces FTS_NS entry, not fts_open failure");
}
ATF_TC_BODY(fts_open_nonexistent_path, tc)
{
	char *paths[] = { "this-path-does-not-exist", NULL };
	FTS *fts;
	FTSENT *ent;

	fts = fts_open(paths, FTS_PHYSICAL, NULL);
	ATF_REQUIRE_MSG(fts != NULL, "fts_open(): %m");

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_CHECK_EQ(FTS_NS, ent->fts_info);

	/*
	 * Next fts_read must return NULL with errno == 0 —
	 * end-of-traversal, not an error.
	 */
	errno = 1;	/* sentinel — fts_read must clear this */
	ATF_CHECK_EQ(NULL, fts_read(fts));
	ATF_CHECK_EQ(0, errno);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_open() with a path that contains a trailing slash must not crash
 * and must traverse the directory normally.  This was a crash bug fixed
 * in SVN r49851.  fts internally strips trailing slashes via fts_load()
 * when processing root-level entries.
 */
ATF_TC(fts_open_trailing_slash);
ATF_TC_HEAD(fts_open_trailing_slash, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "trailing slash on root path must not crash (SVN r49851)");
}
ATF_TC_BODY(fts_open_trailing_slash, tc)
{
	char *paths[] = { "dir/", NULL };
	FTS *fts;
	FTSENT *ent;
	int seen_dir, seen_file;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	fts = fts_open(paths, FTS_PHYSICAL, NULL);
	ATF_REQUIRE_MSG(fts != NULL, "fts_open(): %m");

	seen_dir = 0;
	seen_file = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_D || ent->fts_info == FTS_DP)
			seen_dir = 1;
		if (ent->fts_info == FTS_F)
			seen_file = 1;
	}

	ATF_CHECK_EQ_MSG(0, errno,
	    "fts_read loop should end with errno 0, not %d", errno);
	ATF_CHECK_MSG(seen_dir != 0, "directory was never visited");
	ATF_CHECK_MSG(seen_file != 0, "file inside dir was never visited");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_open() with an unreadable directory: fts_read must return FTS_D
 * (directory pre-order) and then FTS_DNR (directory not readable).  It
 * must NOT return FTS_DP after an unreadable directory, because fts never
 * successfully entered it.
 *
 * Requires an unprivileged user because root ignores directory permissions.
 */
ATF_TC(fts_open_unreadable_dir);
ATF_TC_HEAD(fts_open_unreadable_dir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "unreadable directory yields FTS_D then FTS_DNR, never FTS_DP");
	atf_tc_set_md_var(tc, "require.user", "unprivileged");
}
ATF_TC_BODY(fts_open_unreadable_dir, tc)
{
	ATF_REQUIRE_EQ(0, mkdir("unr", 0000));
	fts_test(tc, &(struct fts_testcase){
		    (char *[]){ "unr", NULL },
		    FTS_PHYSICAL,
		    (struct fts_expect[]){
			    { FTS_D,   "unr", "unr" },
			    { FTS_DNR, "unr", "unr" },
			    { 0 }
		    },
	    });
}

/*
 * fts_open() with multiple root paths: every root is visited in the order
 * given, and the tree under each root is traversed completely before moving
 * to the next root.
 */
ATF_TC(fts_open_multiple_roots);
ATF_TC_HEAD(fts_open_multiple_roots, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_open visits multiple root paths left-to-right");
}
ATF_TC_BODY(fts_open_multiple_roots, tc)
{
	ATF_REQUIRE_EQ(0, mkdir("a", 0755));
	ATF_REQUIRE_EQ(0, mkdir("b", 0755));
	ATF_REQUIRE_EQ(0, close(creat("a/x", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("b/y", 0644)));

	fts_test(tc, &(struct fts_testcase){
		    (char *[]){ "a", "b", NULL },
		    FTS_PHYSICAL,
		    (struct fts_expect[]){
			    { FTS_D,  "a", "a" },
			    { FTS_F,  "x", "x" },
			    { FTS_DP, "a", "a" },
			    { FTS_D,  "b", "b" },
			    { FTS_F,  "y", "y" },
			    { FTS_DP, "b", "b" },
			    { 0 }
		    },
	    });
}

ATF_TP_ADD_TCS(tp)
{
	fts_check_debug();
	ATF_TP_ADD_TC(tp, fts_open_invalid_options);
	ATF_TP_ADD_TC(tp, fts_open_empty_argv);
	ATF_TP_ADD_TC(tp, fts_open_empty_path_string);
	ATF_TP_ADD_TC(tp, fts_open_nonexistent_path);
	ATF_TP_ADD_TC(tp, fts_open_trailing_slash);
	ATF_TP_ADD_TC(tp, fts_open_unreadable_dir);
	ATF_TP_ADD_TC(tp, fts_open_multiple_roots);
	return (atf_no_error());
}
