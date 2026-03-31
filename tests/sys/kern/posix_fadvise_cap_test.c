/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kory Heard <koryheard@icloud.com>
 */

/*
 * Test Capsicum capability rights for posix_fadvise(2).
 * Tests CAP_POSIX_FADVISE right.
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
 * Test CAP_POSIX_FADVISE: verify it permits posix_fadvise and denies without it.
 */
ATF_TC(cap_posix_fadvise);
ATF_TC_HEAD(cap_posix_fadvise, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test CAP_POSIX_FADVISE permits posix_fadvise and denies without it");
}
ATF_TC_BODY(cap_posix_fadvise, tc)
{
	cap_rights_t rights;
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = open("testfile", O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "open failed: %s", strerror(errno));

	/* Write some data so fadvise has something to work with */
	ATF_REQUIRE(write(fd, "test", 4) == 4);

	/* Limit to CAP_POSIX_FADVISE */
	cap_rights_init(&rights, CAP_POSIX_FADVISE);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* posix_fadvise should succeed */
	error = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	ATF_REQUIRE_MSG(error == 0,
	    "posix_fadvise should succeed with CAP_POSIX_FADVISE, got %s",
	    strerror(error));

	/* Now limit to empty rights */
	cap_rights_init(&rights);
	error = cap_rights_limit(fd, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* posix_fadvise should fail with ENOTCAPABLE */
	error = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	ATF_REQUIRE_MSG(error == ENOTCAPABLE,
	    "posix_fadvise should fail with ENOTCAPABLE, got %s",
	    error == 0 ? "success" : strerror(error));

	close(fd);
}

/*
 * Test that an unlimited descriptor allows posix_fadvise.
 */
ATF_TC(cap_posix_fadvise_unlimited);
ATF_TC_HEAD(cap_posix_fadvise_unlimited, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test unlimited descriptor permits posix_fadvise");
}
ATF_TC_BODY(cap_posix_fadvise_unlimited, tc)
{
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = open("testfile", O_RDWR | O_CREAT, 0600);
	ATF_REQUIRE_MSG(fd >= 0, "open failed: %s", strerror(errno));

	ATF_REQUIRE(write(fd, "test", 4) == 4);

	/* posix_fadvise should succeed */
	error = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
	ATF_REQUIRE_MSG(error == 0,
	    "posix_fadvise should succeed on unlimited descriptor, got %s",
	    strerror(error));

	close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_posix_fadvise);
	ATF_TP_ADD_TC(tp, cap_posix_fadvise_unlimited);

	return (atf_no_error());
}
