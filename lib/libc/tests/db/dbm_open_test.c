/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>

#include <atf-c.h>

static const char *path = "tmp";
static const char *dbname = "tmp.db";

ATF_TC(dbm_open_missing_test);
ATF_TC_HEAD(dbm_open_missing_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test dbm_open when creating a new database");
}

ATF_TC_BODY(dbm_open_missing_test, tc)
{

	/*
	 * POSIX.1 specifies that a missing database file should
	 * always get created if O_CREAT is present, except when
	 * O_EXCL is specified as well.
	 */
	ATF_CHECK(dbm_open(path, O_RDONLY, 0755) == NULL);
	ATF_REQUIRE(!atf_utils_file_exists(dbname));
	ATF_CHECK(dbm_open(path, O_RDONLY | O_CREAT, 0755) != NULL);
	ATF_REQUIRE(atf_utils_file_exists(dbname));
	ATF_CHECK(dbm_open(path, O_RDONLY | O_CREAT | O_EXCL, 0755) == NULL);
}

ATF_TC_WITHOUT_HEAD(dbm_open_wronly_test);
ATF_TC_BODY(dbm_open_wronly_test, tc)
{
	ATF_CHECK(dbm_open(path, O_WRONLY, 0755) == NULL);
	ATF_REQUIRE(!atf_utils_file_exists(dbname));
	ATF_CHECK(dbm_open(path, O_WRONLY | O_CREAT, 0755) != NULL);
	ATF_REQUIRE(atf_utils_file_exists(dbname));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dbm_open_missing_test);
	ATF_TP_ADD_TC(tp, dbm_open_wronly_test);
	return (atf_no_error());
}
