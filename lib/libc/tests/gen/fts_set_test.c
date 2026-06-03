/*
 * Copyright (c) 2026 Jitendra Bhati
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Tests for fts_set(), fts_set_clientptr(), fts_get_clientptr(),
 * and fts_get_stream().
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "fts_test.h"

/*
 * fts_set with invalid options must return non-zero with EINVAL.
 * Note: fts_set returns 1 (not -1) on error.
 */
ATF_TC(invalid_options);
ATF_TC_HEAD(invalid_options, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_set with invalid options returns non-zero with EINVAL");
}
ATF_TC_BODY(invalid_options, tc)
{
	char *paths[] = { ".", NULL };
	FTS *fts;
	FTSENT *ent;

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_ERRNO(EINVAL, fts_set(fts, ent, 99) != 0);
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTS_AGAIN causes the current node to be re-stat()ed and returned
 * again on the next fts_read() call.
 */
ATF_TC(again);
ATF_TC_HEAD(again, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_AGAIN causes the current node to be returned once more");
}
ATF_TC_BODY(again, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	int revisit_count;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	revisit_count = 0;
	errno = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_F && revisit_count == 0) {
			ATF_REQUIRE_EQ_MSG(0,
			    fts_set(fts, ent, FTS_AGAIN),
			    "fts_set(FTS_AGAIN): %m");
			revisit_count++;
		} else if (ent->fts_info == FTS_F && revisit_count >= 1) {
			revisit_count++;
		}
	}
	ATF_CHECK_EQ_MSG(0, errno, "traversal ended with errno %d", errno);
	ATF_CHECK_EQ_MSG(2, revisit_count,
	    "expected file visited twice via FTS_AGAIN, saw %d",
	    revisit_count);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTS_AGAIN set twice in a row causes the node to be visited three
 * times total.  Each fts_read() clears fts_options, so the caller must
 * set FTS_AGAIN again explicitly each time.
 */
ATF_TC(again_consecutive);
ATF_TC_HEAD(again_consecutive, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_AGAIN set twice in a row visits the node three times");
}
ATF_TC_BODY(again_consecutive, tc)
{
	char *paths[] = { "file", NULL };
	FTS *fts;
	FTSENT *ent;
	int visit_count;

	ATF_REQUIRE_EQ(0, close(creat("file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	visit_count = 0;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_F) {
			visit_count++;
			if (visit_count < 3)
				ATF_REQUIRE_EQ(0,
				    fts_set(fts, ent, FTS_AGAIN));
		}
	}
	ATF_CHECK_EQ_MSG(3, visit_count,
	    "expected 3 visits with consecutive FTS_AGAIN, got %d",
	    visit_count);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTS_FOLLOW on an FTS_SL entry pointing to a regular file yields FTS_F.
 */
ATF_TC(follow_symlink_to_file);
ATF_TC_HEAD(follow_symlink_to_file, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_FOLLOW on FTS_SL to regular file yields FTS_F");
}
ATF_TC_BODY(follow_symlink_to_file, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	bool followed;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/target", 0644)));
	ATF_REQUIRE_EQ(0, symlink("target", "dir/link"));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	followed = false;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_SL &&
		    strcmp(ent->fts_name, "link") == 0)
			ATF_REQUIRE_EQ(0, fts_set(fts, ent, FTS_FOLLOW));
		else if (ent->fts_info == FTS_F &&
		    strcmp(ent->fts_name, "link") == 0)
			followed = true;
	}
	ATF_CHECK_MSG(followed,
	    "FTS_FOLLOW on symlink-to-file must yield FTS_F");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTS_FOLLOW on an FTS_SL entry pointing to a directory causes descent
 * into the target directory.
 */
ATF_TC(follow_symlink_to_dir);
ATF_TC_HEAD(follow_symlink_to_dir, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_FOLLOW on FTS_SL to directory causes descent");
}
ATF_TC_BODY(follow_symlink_to_dir, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	bool saw_inside;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/real", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/real/inside", 0644)));
	ATF_REQUIRE_EQ(0, symlink("real", "dir/link"));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	saw_inside = false;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_SL &&
		    strcmp(ent->fts_name, "link") == 0)
			ATF_REQUIRE_EQ(0, fts_set(fts, ent, FTS_FOLLOW));
		if (ent->fts_info == FTS_F &&
		    strcmp(ent->fts_name, "inside") == 0 &&
		    strcmp(ent->fts_path, "dir/link/inside") == 0)
		    saw_inside = true;
	}
	ATF_CHECK_MSG(saw_inside,
	    "FTS_FOLLOW on symlink-to-dir should descend and visit 'inside'");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTS_FOLLOW on a dangling symlink (FTS_SLNONE) yields FTS_SLNONE again.
 * FTS_SLNONE requires FTS_LOGICAL — under FTS_PHYSICAL a dangling
 * symlink is reported as FTS_SL.
 */
ATF_TC(follow_dead_symlink);
ATF_TC_HEAD(follow_dead_symlink, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_FOLLOW on dead symlink yields FTS_SLNONE");
}
ATF_TC_BODY(follow_dead_symlink, tc)
{
	char *paths[] = { "dead", NULL };
	FTS *fts;
	FTSENT *ent;

	ATF_REQUIRE_EQ(0, symlink("no-such-target", "dead"));

	ATF_REQUIRE((fts = fts_open(paths, FTS_LOGICAL, NULL)) != NULL);

	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_REQUIRE_EQ_MSG(FTS_SLNONE, ent->fts_info,
	    "expected FTS_SLNONE for dead symlink, got %d", ent->fts_info);

	ATF_REQUIRE_EQ(0, fts_set(fts, ent, FTS_FOLLOW));
	ent = fts_read(fts);
	ATF_REQUIRE(ent != NULL);
	ATF_CHECK_EQ_MSG(FTS_SLNONE, ent->fts_info,
	    "FTS_FOLLOW on dead symlink should still be FTS_SLNONE, got %d",
	    ent->fts_info);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * FTS_SKIP on an FTS_D node prevents descent into that directory.
 * The next fts_read() converts the node to FTS_DP without visiting
 * any children.
 */
ATF_TC(skip);
ATF_TC_HEAD(skip, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "FTS_SKIP prevents descent into a directory");
}
ATF_TC_BODY(skip, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	bool saw_inside;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/skip_me", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/skip_me/inside", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/sibling", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	saw_inside = false;
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_D &&
		    strcmp(ent->fts_name, "skip_me") == 0)
			ATF_REQUIRE_EQ(0, fts_set(fts, ent, FTS_SKIP));
		if (strcmp(ent->fts_name, "inside") == 0)
			saw_inside = true;
	}
	ATF_CHECK_MSG(!saw_inside,
	    "FTS_SKIP: 'inside' must not have been visited");

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_set_clientptr() and fts_get_clientptr() store and retrieve an
 * arbitrary pointer on the FTS stream.
 */
ATF_TC(clientptr_roundtrip);
ATF_TC_HEAD(clientptr_roundtrip, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_set_clientptr / fts_get_clientptr round-trip");
}
ATF_TC_BODY(clientptr_roundtrip, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	int value = 42;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	/* Initially NULL. */
	ATF_CHECK_EQ(NULL, fts_get_clientptr(fts));

	fts_set_clientptr(fts, &value);

	while ((ent = fts_read(fts)) != NULL) {
		/*
		 * Verify the pointer is accessible and correct
		 * while traversal is active.
		 */
		ATF_CHECK_EQ_MSG(&value, fts_get_clientptr(fts),
		    "fts_get_clientptr did not return the stored pointer "
		    "for entry '%s'", ent->fts_name);
	}

	/* Overwrite with NULL, verify. */
	fts_set_clientptr(fts, NULL);
	ATF_CHECK_EQ(NULL, fts_get_clientptr(fts));

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * fts_get_stream() returns the parent FTS* from any FTSENT* returned
 * by fts_read().
 */
ATF_TC(get_stream_backpointer);
ATF_TC_HEAD(get_stream_backpointer, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_get_stream returns the parent FTS* from an FTSENT*");
}
ATF_TC_BODY(get_stream_backpointer, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	while ((ent = fts_read(fts)) != NULL) {
		ATF_CHECK_EQ_MSG(fts, fts_get_stream(ent),
		    "fts_get_stream(ent) must return the parent FTS*, "
		    "entry: %s info: %d",
		    ent->fts_name, ent->fts_info);
	}

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, invalid_options);
	ATF_TP_ADD_TC(tp, again);
	ATF_TP_ADD_TC(tp, again_consecutive);
	ATF_TP_ADD_TC(tp, follow_symlink_to_file);
	ATF_TP_ADD_TC(tp, follow_symlink_to_dir);
	ATF_TP_ADD_TC(tp, follow_dead_symlink);
	ATF_TP_ADD_TC(tp, skip);
	ATF_TP_ADD_TC(tp, clientptr_roundtrip);
	ATF_TP_ADD_TC(tp, get_stream_backpointer);

	return (atf_no_error());
}
