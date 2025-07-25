/*-
 * Copyright (c) 2025 Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <errno.h>
#include <fcntl.h>
#include <ndbm.h>
#include <stdio.h>

#include <atf-c.h>

static const char *path = "tmp";
static const char *dbname = "tmp.db";

static void
create_db(void)
{
	DB *db;
	datum data, key;

	data.dptr = "bar";
	data.dsize = strlen("bar");
	key.dptr = "foo";
	key.dsize = strlen("foo");

	db = dbm_open(path, O_RDWR | O_CREAT, 0755);
	ATF_CHECK(db != NULL);
	ATF_REQUIRE(atf_utils_file_exists(dbname));
	ATF_REQUIRE(dbm_store(db, key, data, DBM_INSERT) != -1);
	dbm_close(db);
}

ATF_TC_WITHOUT_HEAD(dbm_rdonly_test);
ATF_TC_BODY(dbm_rdonly_test, tc)
{
	DB *db;
	datum data, key;

	bzero(&data, sizeof(data));
	key.dptr = "foo";
	key.dsize = strlen("foo");
	create_db();

	db = dbm_open(path, O_RDONLY, 0755);
	data = dbm_fetch(db, key);
	ATF_REQUIRE(data.dptr != NULL);
	ATF_REQUIRE(strncmp((const char*)data.dptr, "bar", data.dsize) == 0);
	ATF_REQUIRE(dbm_store(db, key, data, DBM_REPLACE) == -1);
	ATF_REQUIRE(errno == EPERM);
}

ATF_TC_WITHOUT_HEAD(dbm_wronly_test);
ATF_TC_BODY(dbm_wronly_test, tc)
{
	DB *db;
	datum data, key;

	key.dptr = "foo";
	key.dsize = strlen("foo");
	data.dptr = "baz";
	data.dsize = strlen("baz");
	create_db();

	db = dbm_open(path, O_WRONLY, 0755);
	data = dbm_fetch(db, key);
	ATF_REQUIRE(data.dptr == NULL);
	ATF_REQUIRE(errno == EPERM);
	ATF_REQUIRE(dbm_store(db, key, data, DBM_REPLACE) != -1);
}

ATF_TC_WITHOUT_HEAD(dbm_rdwr_test);
ATF_TC_BODY(dbm_rdwr_test, tc)
{
	DB *db;
	datum data, key;

	key.dptr = "foo";
	key.dsize = strlen("foo");
	create_db();

	db = dbm_open(path, O_RDWR, 0755);
	data = dbm_fetch(db, key);
	ATF_REQUIRE(data.dptr != NULL);
	data.dptr = "baz";
	data.dsize = strlen("baz");
	ATF_REQUIRE(dbm_store(db, key, data, DBM_REPLACE) != -1);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, dbm_rdonly_test);
	ATF_TP_ADD_TC(tp, dbm_wronly_test);
	ATF_TP_ADD_TC(tp, dbm_rdwr_test);

	return (atf_no_error());
}
