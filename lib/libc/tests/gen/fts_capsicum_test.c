/*
 * Copyright (c) 2026 Jitendra Bhati
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Tests for fts(3) in Capsicum capability mode using fts_dirfd.
 */

#include <sys/capsicum.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

/*
 * Verify that fts_dirfd is set to a valid file descriptor for every
 * entry returned by fts_read(), and that openat(fts_dirfd, fts_name)
 * correctly identifies the same file as fts_accpath.
 */
ATF_TC(fts_dirfd_valid);
ATF_TC_HEAD(fts_dirfd_valid, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_dirfd is valid for all non-root entries");
}
ATF_TC_BODY(fts_dirfd_valid, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	struct stat sb_accpath, sb_dirfd;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/sub", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/sub/file", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/other", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	while ((ent = fts_read(fts)) != NULL) {
		/* Root entry has no parent directory fd. */
		if (ent->fts_level == FTS_ROOTLEVEL)
			continue;

		ATF_CHECK_MSG(ent->fts_dirfd >= 0,
		    "fts_dirfd must be valid for '%s' at level %ld info=%d",
		    ent->fts_name, ent->fts_level, ent->fts_info);

		if (ent->fts_dirfd < 0)
			continue;

		/* Skip post-order — same entry as pre-order. */
		if (ent->fts_info == FTS_DP)
			continue;

		ATF_REQUIRE_EQ_MSG(0,
		    fstatat(ent->fts_dirfd, ent->fts_name, &sb_dirfd,
		    AT_SYMLINK_NOFOLLOW),
		    "fstatat(fts_dirfd, fts_name) failed for '%s': %m",
		    ent->fts_name);
		ATF_REQUIRE_EQ_MSG(0,
		    lstat(ent->fts_accpath, &sb_accpath),
		    "lstat(fts_accpath) failed for '%s': %m",
		    ent->fts_accpath);

		ATF_CHECK_EQ_MSG(sb_accpath.st_ino, sb_dirfd.st_ino,
		    "inode mismatch for '%s': "
		    "fts_accpath ino=%ju fts_dirfd ino=%ju",
		    ent->fts_name,
		    (uintmax_t)sb_accpath.st_ino,
		    (uintmax_t)sb_dirfd.st_ino);
		ATF_CHECK_EQ_MSG(sb_accpath.st_dev, sb_dirfd.st_dev,
		    "device mismatch for '%s'", ent->fts_name);
	}

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * Verify that fts traversal works correctly in Capsicum capability
 * mode using openat(fts_dirfd, fts_name) to access files.
 */
ATF_TC(fts_dirfd_capsicum);
ATF_TC_HEAD(fts_dirfd_capsicum, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts traversal using fts_dirfd works in Capsicum capability mode");
}
ATF_TC_BODY(fts_dirfd_capsicum, tc)
{
	if (!feature_present("security_capabilities") ||
	    !feature_present("security_capability_mode"))
		atf_tc_skip("Capsicum not available");

	char *paths[] = { ".", NULL };
	FTS *fts;
	FTSENT *ent;
	int dirfd;
	bool saw_file, saw_sub;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/sub", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/sub/file", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/other", 0644)));

	ATF_REQUIRE((dirfd = open("dir", O_RDONLY | O_DIRECTORY)) >= 0);

	/*
	 * Open fts before entering capability mode — fts_openat
	 * will dup dirfd internally so we can close our copy.
	 */
	ATF_REQUIRE((fts = fts_openat(dirfd, paths,
	    FTS_PHYSICAL | FTS_NOCHDIR, NULL)) != NULL);

	/* Enter capability mode — no more path-based syscalls. */
	ATF_REQUIRE_EQ(0, cap_enter());

	saw_file = false;
	saw_sub = false;

	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_DP)
			continue;

		if (strcmp(ent->fts_name, "sub") == 0 &&
		    ent->fts_info == FTS_D)
			saw_sub = true;

		if (strcmp(ent->fts_name, "file") == 0 &&
		    ent->fts_info == FTS_F) {
			saw_file = true;

			struct stat sb;
			ATF_CHECK_EQ_MSG(0,
			    fstatat(ent->fts_dirfd, ent->fts_name, &sb,
			    AT_SYMLINK_NOFOLLOW),
			    "fstatat(fts_dirfd, fts_name) failed for "
			    "'%s' in capability mode: %m",
			    ent->fts_name);
		}
	}

	ATF_CHECK_MSG(saw_sub, "must have visited 'sub' directory");
	ATF_CHECK_MSG(saw_file, "must have visited 'file'");
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * Verify that fts_dirfd + fts_name correctly identifies files even
 * when fts has internally chdir'd into subdirectories.
 */
ATF_TC(fts_dirfd_deep_tree);
ATF_TC_HEAD(fts_dirfd_deep_tree, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_dirfd + fts_name is correct at all directory depths");
}
ATF_TC_BODY(fts_dirfd_deep_tree, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	struct stat sb_dirfd, sb_accpath;
	int depth_checked = 0;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/a", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/a/b", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/a/b/c", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/a/b/c/deep", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/a/b/mid", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/a/top", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/root", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_DP ||
		    ent->fts_level == FTS_ROOTLEVEL)
			continue;

		ATF_REQUIRE_MSG(ent->fts_dirfd >= 0,
		    "fts_dirfd must be valid for '%s' at level %ld",
		    ent->fts_name, ent->fts_level);

		ATF_REQUIRE_EQ_MSG(0,
		    fstatat(ent->fts_dirfd, ent->fts_name, &sb_dirfd,
		    AT_SYMLINK_NOFOLLOW),
		    "fstatat failed for '%s': %m", ent->fts_name);
		ATF_REQUIRE_EQ_MSG(0,
		    lstat(ent->fts_accpath, &sb_accpath),
		    "lstat failed for '%s': %m", ent->fts_accpath);

		ATF_CHECK_EQ_MSG(sb_accpath.st_ino, sb_dirfd.st_ino,
		    "inode mismatch at depth %ld for '%s'",
		    ent->fts_level, ent->fts_name);

		depth_checked++;
	}

	/* 4 files (deep, mid, top, root) + 3 dirs (a, b, c) = 7 entries */
	ATF_CHECK_EQ_MSG(7, depth_checked,
	    "expected 7 entries, got %d", depth_checked);

	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fts_dirfd_valid);
	ATF_TP_ADD_TC(tp, fts_dirfd_capsicum);
	ATF_TP_ADD_TC(tp, fts_dirfd_deep_tree);

	return (atf_no_error());
}
