/*
 * Copyright (c) 2026 Jitendra Bhati
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Basic tests for fts_openat().  When called with AT_FDCWD the
 * behaviour must be identical to fts_open().
 */

#include <sys/stat.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <fts.h>
#include <string.h>
#include <unistd.h>
#include <sys/capsicum.h>

#include <atf-c.h>

#define	FTS_TEST_MAXENTRIES 64

static int
fts_lexical_compar(const FTSENT * const *a, const FTSENT * const *b)
{
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

/*
 * fts_openat(AT_FDCWD, ...) must behave identically to fts_open().
 */
ATF_TC(atfdcwd_matches_fts_open);
ATF_TC_HEAD(atfdcwd_matches_fts_open, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_openat(AT_FDCWD) behaves identically to fts_open");
}

ATF_TC_BODY(atfdcwd_matches_fts_open, tc)
{
	char *cwd, *abspath;
	char *paths[2];
	FTS *fts;
	FTSENT *ent;

	int *info1, *info2;
	char (*names1)[NAME_MAX + 1], (*names2)[NAME_MAX + 1];
	int n1, n2, i;
	
	ATF_REQUIRE((info1 = malloc(FTS_TEST_MAXENTRIES *
            sizeof(*info1))) != NULL);
        ATF_REQUIRE((info2 = malloc(FTS_TEST_MAXENTRIES *
            sizeof(*info2))) != NULL);
        ATF_REQUIRE((names1 = malloc(FTS_TEST_MAXENTRIES *
            sizeof(*names1))) != NULL);
        ATF_REQUIRE((names2 = malloc(FTS_TEST_MAXENTRIES *
            sizeof(*names2))) != NULL);

	cwd = malloc(PATH_MAX);
	ATF_REQUIRE(cwd != NULL);
	abspath = malloc(PATH_MAX * 2);
	ATF_REQUIRE(abspath != NULL);

	ATF_REQUIRE(getcwd(cwd, PATH_MAX) != NULL);
	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/sub", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/sub/file", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/other", 0644)));

	snprintf(abspath, PATH_MAX * 2, "%s/dir", cwd);
	paths[0] = abspath;
	paths[1] = NULL;

	/* Collect fts_open results. */
	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);
	for (n1 = 0;
	    (ent = fts_read(fts)) != NULL && n1 < FTS_TEST_MAXENTRIES;
	    n1++) {
		info1[n1] = ent->fts_info;
		strlcpy(names1[n1], ent->fts_name, NAME_MAX + 1);
	}
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close: %m");

	/* Collect fts_openat results. */
	ATF_REQUIRE((fts = fts_openat(AT_FDCWD, paths, FTS_PHYSICAL,
	    fts_lexical_compar)) != NULL);
	for (n2 = 0;
	    (ent = fts_read(fts)) != NULL && n2 < FTS_TEST_MAXENTRIES;
	    n2++) {
		info2[n2] = ent->fts_info;
		strlcpy(names2[n2], ent->fts_name, NAME_MAX + 1);
	}
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close: %m");

	/* Compare. */
	ATF_CHECK_EQ_MSG(n1, n2,
	    "entry count mismatch: fts_open=%d fts_openat=%d", n1, n2);
	for (i = 0; i < n1 && i < n2; i++) {
		ATF_CHECK_EQ_MSG(info1[i], info2[i],
		    "fts_info mismatch at entry %d: "
		    "fts_open=%d fts_openat=%d name=%s",
		    i, info1[i], info2[i], names1[i]);
		ATF_CHECK_STREQ_MSG(names1[i], names2[i],
		    "fts_name mismatch at entry %d: "
		    "fts_open='%s' fts_openat='%s'",
		    i, names1[i], names2[i]);
	}

	free(cwd);
	free(abspath);
	free(info1);
	free(info2);
	free(names1);
	free(names2);
}

/*
 * fts_openat() with a real dirfd must work in Capsicum capability mode.
 */
ATF_TC(openat_capsicum);
ATF_TC_HEAD(openat_capsicum, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_openat() with dirfd works in Capsicum capability mode");
}
ATF_TC_BODY(openat_capsicum, tc)
{
	char *paths[] = { ".", NULL };
	FTS *fts;
	FTSENT *ent;
	int dirfd;
	bool saw_file = false, saw_sub = false;

	if (!feature_present("security_capabilities") ||
	    !feature_present("security_capability_mode"))
		atf_tc_skip("Capsicum not available");

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/sub", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/sub/file", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("dir/other", 0644)));

	ATF_REQUIRE((dirfd = open("dir", O_RDONLY | O_DIRECTORY)) >= 0);

	ATF_REQUIRE((fts = fts_openat(dirfd, paths,
	    FTS_PHYSICAL | FTS_NOCHDIR, NULL)) != NULL);

	ATF_REQUIRE_EQ(0, cap_enter());

	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info == FTS_DP)
			continue;
		if (strcmp(ent->fts_name, "sub") == 0 &&
		    ent->fts_info == FTS_D)
			saw_sub = true;
		if (strcmp(ent->fts_name, "file") == 0 &&
		    ent->fts_info == FTS_F)
			saw_file = true;
	}

	ATF_CHECK_MSG(saw_sub, "must have visited 'sub' directory");
	ATF_CHECK_MSG(saw_file, "must have visited 'file'");
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

/*
 * Demonstrate the intended use of fts_dirfd: use
 * fts_parent->fts_dirfd + fts_name to access files without
 * relying on path-based operations.
 */
ATF_TC(fts_dirfd_openat);
ATF_TC_HEAD(fts_dirfd_openat, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "fts_parent->fts_dirfd + fts_name can be used with openat(2)");
}
ATF_TC_BODY(fts_dirfd_openat, tc)
{
	char *paths[] = { "dir", NULL };
	FTS *fts;
	FTSENT *ent;
	struct stat sb_path, sb_dirfd;
	bool saw_file = false;

	ATF_REQUIRE_EQ(0, mkdir("dir", 0755));
	ATF_REQUIRE_EQ(0, mkdir("dir/sub", 0755));
	ATF_REQUIRE_EQ(0, close(creat("dir/sub/file", 0644)));

	ATF_REQUIRE((fts = fts_open(paths, FTS_PHYSICAL, NULL)) != NULL);

	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info != FTS_F)
			continue;
		if (strcmp(ent->fts_name, "file") != 0)
			continue;

		saw_file = true;

		/*
		 * fts_dirfd is set only on directory entries.
		 * Use fts_parent->fts_dirfd + fts_name to access
		 * the file via openat(2) without path-based operations.
		 */
		ATF_REQUIRE_MSG(ent->fts_parent->fts_dirfd >= 0,
		    "fts_parent->fts_dirfd must be valid for '%s'",
		    ent->fts_name);

		ATF_REQUIRE_EQ_MSG(0,
		    fstatat(ent->fts_parent->fts_dirfd, ent->fts_name,
		    &sb_dirfd, AT_SYMLINK_NOFOLLOW),
		    "fstatat(fts_parent->fts_dirfd, fts_name) failed: %m");

		ATF_REQUIRE_EQ_MSG(0,
		    lstat(ent->fts_accpath, &sb_path),
		    "lstat(fts_accpath) failed: %m");

		ATF_CHECK_EQ_MSG(sb_path.st_ino, sb_dirfd.st_ino,
		    "inode mismatch: fts_accpath=%ju fts_parent->fts_dirfd=%ju",
		    (uintmax_t)sb_path.st_ino,
		    (uintmax_t)sb_dirfd.st_ino);
	}

	ATF_CHECK_MSG(saw_file, "did not visit 'file'");
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, atfdcwd_matches_fts_open);
	ATF_TP_ADD_TC(tp, openat_capsicum);
	ATF_TP_ADD_TC(tp, fts_dirfd_openat);
	return (atf_no_error());
}
