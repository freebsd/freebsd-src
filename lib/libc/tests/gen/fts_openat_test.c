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
#include <fcntl.h>
#include <errno.h>
#include <fts.h>
#include <string.h>
#include <unistd.h>

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

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, atfdcwd_matches_fts_open);
	return (atf_no_error());
}
