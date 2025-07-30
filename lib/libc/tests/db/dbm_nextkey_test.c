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

ATF_TC(dbm_nextkey_test);
ATF_TC_HEAD(dbm_nextkey_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Check that dbm_nextkey always returns NULL after reaching the end of the database");
}

ATF_TC_BODY(dbm_nextkey_test, tc)
{
	DBM *db;
	datum key, data;

	data.dptr = "bar";
	data.dsize = strlen("bar");
	key.dptr = "foo";
	key.dsize = strlen("foo");

	db = dbm_open(path, O_RDWR | O_CREAT, 0755);
	ATF_CHECK(db != NULL);
	ATF_REQUIRE(atf_utils_file_exists(dbname));
	ATF_REQUIRE(dbm_store(db, key, data, DBM_INSERT) != -1);

	key = dbm_firstkey(db);
	ATF_REQUIRE(key.dptr != NULL);
	key = dbm_nextkey(db);
	ATF_REQUIRE(key.dptr == NULL);
	key = dbm_nextkey(db);
	ATF_REQUIRE(key.dptr == NULL);

	dbm_close(db);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dbm_nextkey_test);

	return (atf_no_error());
}
