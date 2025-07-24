/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/mman.h>

#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>

#include <atf-c.h>

ATF_TC(dbm_open_missing_test);
ATF_TC_HEAD(dbm_open_missing_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test dbm_open when creating a new database");
}

ATF_TC_BODY(dbm_open_missing_test, tc)
{
	const char *path = "tmp";
	const char *dbname = "tmp.db";

	/*
	 * POSIX.1 specifies that a missing database file should
	 * always get created if O_CREAT is present, except when
	 * O_EXCL is specified as well.
	 */
	ATF_CHECK(dbm_open(path, O_RDONLY, _PROT_ALL) == NULL);
	ATF_REQUIRE(!atf_utils_file_exists(dbname));
	ATF_CHECK(dbm_open(path, O_RDONLY | O_CREAT, _PROT_ALL) != NULL);
	ATF_REQUIRE(atf_utils_file_exists(dbname));
	ATF_CHECK(dbm_open(path, O_RDONLY | O_CREAT | O_EXCL, _PROT_ALL) == NULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dbm_open_missing_test);
	return (atf_no_error());
}
