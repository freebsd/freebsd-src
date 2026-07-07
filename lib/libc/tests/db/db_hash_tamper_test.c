/*-
 * Copyright (c) 2026. Klara, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <atf-c.h>
#include <db.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * The internal db/hash/hash.h header is needed to
 * avoid hardcoding header structure offsets.
 */
#include "hash.h"

#define SET_HDR_VAR(hdr, field, val) (hdr)->field = htonl((uint32_t)val)
#define GET_HDR_VAR(hdr, field)	     ((uint32_t)ntohl((hdr)->field);)

static const char *dbname = "tmp.db";

/* Create a database file with one entry. */
static void
create_db(void)
{
	DB *db;
	DBT key, val;

	key.data = "foo";
	key.size = strlen("foo");

	val.data = "bar";
	val.size = strlen("bar");

	if (atf_utils_file_exists(dbname))
		unlink(dbname);
	db = dbopen(dbname, O_CREAT | O_RDWR | O_TRUNC, 0755, DB_HASH, NULL);
	ATF_CHECK(db != NULL);
	ATF_REQUIRE(atf_utils_file_exists(dbname));

	ATF_REQUIRE(db->put(db, &key, &key, 0) == 0);

	db->close(db);
}

static void
read_hdr(HASHHDR *hdr)
{
	int fd;

	ATF_REQUIRE(atf_utils_file_exists(dbname));
	fd = open(dbname, O_RDONLY);
	ATF_CHECK(fd != -1);
	ATF_CHECK(read(fd, hdr, sizeof(*hdr)) == sizeof(*hdr));
	close(fd);
}

static void
write_hdr(HASHHDR *hdr)
{
	int fd;

	ATF_REQUIRE(atf_utils_file_exists(dbname));
	fd = open(dbname, O_WRONLY);
	ATF_CHECK(fd != -1);
	ATF_CHECK(write(fd, hdr, sizeof(*hdr)) == sizeof(*hdr));
	close(fd);
}

ATF_TC(db_hash_ovflw_point_test);
ATF_TC_HEAD(db_hash_ovflw_point_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test hash(3) operations with a corrupted 'ovfl_point' header variable.");
}

ATF_TC_BODY(db_hash_ovflw_point_test, tc)
{
	HASHHDR hdr;

	create_db();

	read_hdr(&hdr);
	/*
	 * An unvalidated 'ovfl_point' variable may trigger
	 * an OOB read from the SPARES field.
	 */
	SET_HDR_VAR(&hdr, ovfl_point, NCACHED + 1);
	write_hdr(&hdr);

	ATF_REQUIRE(dbopen(dbname, O_RDONLY, 0755, DB_HASH, NULL) == NULL);
}

ATF_TC(db_hash_bpages_test);
ATF_TC_HEAD(db_hash_bpages_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test hash(3) operations with a corrupted 'spares' header variable.");
}

ATF_TC_BODY(db_hash_bpages_test, tc)
{
	HASHHDR hdr;

	create_db();

	read_hdr(&hdr);
	/*
	 * An unvalidated combination of the 'ovfl_point' variable
	 * and the 'spares' array may be used to manipulate
	 * a memset in _hash_open.
	 */
	SET_HDR_VAR(&hdr, ovfl_point, 0);
	hdr.spares[0] = htonl(0x10000000UL);
	write_hdr(&hdr);

	ATF_REQUIRE(dbopen(dbname, O_RDONLY, 0755, DB_HASH, NULL) == NULL);
}

ATF_TC(db_hash_bsize_test);
ATF_TC_HEAD(db_hash_bsize_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test hash(3) operations with a corrupted 'bsize' header variable.");
}

ATF_TC_BODY(db_hash_bsize_test, tc)
{
	HASHHDR hdr;

	create_db();

	read_hdr(&hdr);
	/*
	 * An unvalidated 'bsize' variable may be
	 * used to manipulate a memset in _hash_open.
	 */
	SET_HDR_VAR(&hdr, bsize, 0x100000);
	write_hdr(&hdr);

	ATF_REQUIRE(dbopen(dbname, O_RDONLY, 0755, DB_HASH, NULL) == NULL);
}

ATF_TC(db_hash_masks_test);
ATF_TC_HEAD(db_hash_masks_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test hash(3) operations with corrupted '{high,low}_mask' header variables.");
}

ATF_TC_BODY(db_hash_masks_test, tc)
{
	HASHHDR hdr;

	/* 'high_mask' must be greater than 'low_mask'. */
	create_db();
	read_hdr(&hdr);
	SET_HDR_VAR(&hdr, high_mask, 0x1);
	SET_HDR_VAR(&hdr, low_mask, 0xF);
	write_hdr(&hdr);
	ATF_REQUIRE(dbopen(dbname, O_RDONLY, 0755, DB_HASH, NULL) == NULL);

	/* 'high_mask' and 'low_mask' must be derived from power-of-2 values. */
	create_db();
	read_hdr(&hdr);
	SET_HDR_VAR(&hdr, high_mask, 0x13);
	write_hdr(&hdr);
	ATF_REQUIRE(dbopen(dbname, O_RDONLY, 0755, DB_HASH, NULL) == NULL);

	create_db();
	read_hdr(&hdr);
	SET_HDR_VAR(&hdr, high_mask, 0xFF);
	SET_HDR_VAR(&hdr, low_mask, 0x13);
	write_hdr(&hdr);
	ATF_REQUIRE(dbopen(dbname, O_RDONLY, 0755, DB_HASH, NULL) == NULL);
}

ATF_TC(db_hash_call_hash_oob_test);
ATF_TC_HEAD(db_hash_call_hash_oob_test, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Attempt to trigger an OOB read with corrupted '{high,low}_mask' header variables.");
}

ATF_TC_BODY(db_hash_call_hash_oob_test, tc)
{
	DBT key, val;
	HASHHDR hdr;
	DB *db;

	key.data = "foo";
	key.size = strlen("foo");

	/*
	 * Invalid values of the '{high,low}_mask' header variables
	 * will cause __call_hash to return OOB bucket indices.
	 */
	create_db();
	read_hdr(&hdr);
	SET_HDR_VAR(&hdr, low_mask, 0xFFFF);
	SET_HDR_VAR(&hdr, high_mask, 0xFFFFF);
	write_hdr(&hdr);
	db = dbopen(dbname, O_RDONLY, 0755, DB_HASH, NULL);
	ATF_REQUIRE(db != NULL);
	/* Attempt to trigger an OOB read. */
	ATF_REQUIRE(db->get(db, &key, &val, 0) != 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, db_hash_ovflw_point_test);
	ATF_TP_ADD_TC(tp, db_hash_bpages_test);
	ATF_TP_ADD_TC(tp, db_hash_bsize_test);
	ATF_TP_ADD_TC(tp, db_hash_masks_test);
	ATF_TP_ADD_TC(tp, db_hash_call_hash_oob_test);

	return (atf_no_error());
}
