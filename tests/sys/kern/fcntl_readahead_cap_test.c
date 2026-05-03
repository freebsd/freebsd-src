/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kory Heard <koryheard@icloud.com>
 */

/*
 * Test Capsicum capability rights for fcntl(F_READAHEAD) and fcntl(F_RDAHEAD).
 * Tests CAP_FCNTL_READAHEAD right.
 */

#include <sys/param.h>
#include <sys/capsicum.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include "freebsd_test_suite/macros.h"

/*
 * Test CAP_FCNTL_READAHEAD for fcntl(F_READAHEAD): verify it permits
 * the operation with the right and denies without it.
 */
ATF_TC(cap_fcntl_readahead);
ATF_TC_HEAD(cap_fcntl_readahead, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test CAP_FCNTL_READAHEAD permits F_READAHEAD and denies without it");
}
ATF_TC_BODY(cap_fcntl_readahead, tc)
{
	cap_rights_t rights;
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = open("testfile", O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "open failed: %s", strerror(errno));

	/* Write some data so readahead has something to work with */
	ATF_REQUIRE(write(fd, "test", 4) == 4);

	/* Limit to CAP_FCNTL_READAHEAD */
	cap_rights_init(&rights, CAP_FCNTL_READAHEAD);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* fcntl(F_READAHEAD) should succeed */
	error = fcntl(fd, F_READAHEAD, 0);
	ATF_REQUIRE_MSG(error != -1 || errno != ENOTCAPABLE,
	    "fcntl(F_READAHEAD) should succeed with CAP_FCNTL_READAHEAD, got %s",
	    strerror(errno));

	/* Now limit to empty rights */
	cap_rights_init(&rights);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* fcntl(F_READAHEAD) should fail with ENOTCAPABLE */
	error = fcntl(fd, F_READAHEAD, 0);
	ATF_REQUIRE_MSG(error == -1 && errno == ENOTCAPABLE,
	    "fcntl(F_READAHEAD) should fail with ENOTCAPABLE, got %s",
	    error == -1 ? strerror(errno) : "success");

	close(fd);
}

/*
 * Test CAP_FCNTL_READAHEAD for fcntl(F_RDAHEAD): verify it permits
 * the operation with the right.
 */
ATF_TC(cap_fcntl_rdahead);
ATF_TC_HEAD(cap_fcntl_rdahead, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test CAP_FCNTL_READAHEAD permits F_RDAHEAD");
}
ATF_TC_BODY(cap_fcntl_rdahead, tc)
{
	cap_rights_t rights;
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = open("testfile", O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "open failed: %s", strerror(errno));

	ATF_REQUIRE(write(fd, "test", 4) == 4);

	/* Limit to CAP_FCNTL_READAHEAD */
	cap_rights_init(&rights, CAP_FCNTL_READAHEAD);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* fcntl(F_RDAHEAD) should succeed */
	error = fcntl(fd, F_RDAHEAD, 1);
	ATF_REQUIRE_MSG(error != -1 || errno != ENOTCAPABLE,
	    "fcntl(F_RDAHEAD) should succeed with CAP_FCNTL_READAHEAD, got %s",
	    strerror(errno));

	close(fd);
}

/*
 * Test that an unlimited descriptor allows fcntl(F_READAHEAD).
 */
ATF_TC(cap_fcntl_readahead_unlimited);
ATF_TC_HEAD(cap_fcntl_readahead_unlimited, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test unlimited descriptor permits fcntl(F_READAHEAD)");
}
ATF_TC_BODY(cap_fcntl_readahead_unlimited, tc)
{
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = open("testfile", O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "open failed: %s", strerror(errno));

	ATF_REQUIRE(write(fd, "test", 4) == 4);

	/* fcntl(F_READAHEAD) should succeed */
	error = fcntl(fd, F_READAHEAD, 0);
	ATF_REQUIRE_MSG(error != -1 || errno != ENOTCAPABLE,
	    "fcntl(F_READAHEAD) should succeed on unlimited descriptor, got %s",
	    strerror(errno));

	close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_fcntl_readahead);
	ATF_TP_ADD_TC(tp, cap_fcntl_rdahead);
	ATF_TP_ADD_TC(tp, cap_fcntl_readahead_unlimited);

	return (atf_no_error());
}
