/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/stat.h>

#include <fcntl.h>
#include <fts.h>

#include <atf-c.h>

/*
 * Create two directories with three files each in lexicographical order,
 * then call FTS with a sort block that sorts in reverse lexicographical
 * order.  This has the least chance of getting a false positive due to
 * differing file system semantics.  UFS will return the files in the
 * order they were created while ZFS will sort them lexicographically; in
 * both cases, the order we expect is the reverse.
 */
ATF_TC(fts_blocks_test);
ATF_TC_HEAD(fts_blocks_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test FTS with a block in lieu of a comparison function");
}
ATF_TC_BODY(fts_blocks_test, tc)
{
	char *args[] = {
		"bar", "foo", NULL
	};
	char *paths[] = {
		"foo", "z", "y", "x", "foo",
		"bar", "c", "b", "a", "bar",
		NULL
	};
	char **expect = paths;
	FTS *fts;
	FTSENT *ftse;

	ATF_REQUIRE_EQ(0, mkdir("bar", 0755));
	ATF_REQUIRE_EQ(0, close(creat("bar/a", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("bar/b", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("bar/c", 0644)));
	ATF_REQUIRE_EQ(0, mkdir("foo", 0755));
	ATF_REQUIRE_EQ(0, close(creat("foo/x", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("foo/y", 0644)));
	ATF_REQUIRE_EQ(0, close(creat("foo/z", 0644)));
	fts = fts_open_b(args, 0,
	    ^(const FTSENT * const *a, const FTSENT * const *b) {
		    return (strcmp((*b)->fts_name, (*a)->fts_name));
	    });
	ATF_REQUIRE_MSG(fts != NULL, "fts_open_b(): %m");
	while ((ftse = fts_read(fts)) != NULL && *expect != NULL) {
		ATF_CHECK_STREQ(*expect, ftse->fts_name);
		expect++;
	}
	ATF_CHECK_EQ(NULL, ftse);
	ATF_CHECK_EQ(NULL, *expect);
	ATF_REQUIRE_EQ_MSG(0, fts_close(fts), "fts_close(): %m");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, fts_blocks_test);
	return (atf_no_error());
}
