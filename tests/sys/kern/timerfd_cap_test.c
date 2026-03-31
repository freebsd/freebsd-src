/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Kory Heard <koryheard@icloud.com>
 */

/*
 * Test Capsicum capability rights for timer file descriptors.
 * Tests CAP_TIMERFD_GETTIME and CAP_TIMERFD_SETTIME rights.
 */

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/timerfd.h>

#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <atf-c.h>

#include "freebsd_test_suite/macros.h"

/*
 * Test that CAP_TIMERFD_GETTIME allows timerfd_gettime but blocks
 * timerfd_settime.
 */
ATF_TC(cap_timerfd_gettime_only);
ATF_TC_HEAD(cap_timerfd_gettime_only, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that CAP_TIMERFD_GETTIME permits gettime but denies settime");
}
ATF_TC_BODY(cap_timerfd_gettime_only, tc)
{
	struct itimerspec its, curr;
	cap_rights_t rights;
	int fd, fd_limited, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = timerfd_create(CLOCK_MONOTONIC, 0);
	ATF_REQUIRE_MSG(fd >= 0, "timerfd_create failed: %s", strerror(errno));

	fd_limited = dup(fd);
	ATF_REQUIRE(fd_limited >= 0);

	cap_rights_init(&rights, CAP_TIMERFD_GETTIME);
	error = cap_rights_limit(fd_limited, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* timerfd_gettime should succeed */
	error = timerfd_gettime(fd_limited, &curr);
	ATF_REQUIRE_MSG(error == 0,
	    "timerfd_gettime should succeed, got %s", strerror(errno));

	/* timerfd_settime should fail with ENOTCAPABLE */
	its.it_value.tv_sec = 1;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	error = timerfd_settime(fd_limited, 0, &its, NULL);
	ATF_REQUIRE_MSG(error == -1 && errno == ENOTCAPABLE,
	    "timerfd_settime should fail with ENOTCAPABLE, got %s",
	    error == 0 ? "success" : strerror(errno));

	close(fd_limited);
	close(fd);
}

/*
 * Test that CAP_TIMERFD_SETTIME allows timerfd_settime but blocks
 * timerfd_gettime.
 */
ATF_TC(cap_timerfd_settime_only);
ATF_TC_HEAD(cap_timerfd_settime_only, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that CAP_TIMERFD_SETTIME permits settime but denies gettime");
}
ATF_TC_BODY(cap_timerfd_settime_only, tc)
{
	struct itimerspec its, curr;
	cap_rights_t rights;
	int fd, fd_limited, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = timerfd_create(CLOCK_MONOTONIC, 0);
	ATF_REQUIRE_MSG(fd >= 0, "timerfd_create failed: %s", strerror(errno));

	fd_limited = dup(fd);
	ATF_REQUIRE(fd_limited >= 0);

	cap_rights_init(&rights, CAP_TIMERFD_SETTIME);
	error = cap_rights_limit(fd_limited, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* timerfd_settime should succeed */
	its.it_value.tv_sec = 1;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	error = timerfd_settime(fd_limited, 0, &its, NULL);
	ATF_REQUIRE_MSG(error == 0,
	    "timerfd_settime should succeed, got %s", strerror(errno));

	/* timerfd_gettime should fail with ENOTCAPABLE */
	error = timerfd_gettime(fd_limited, &curr);
	ATF_REQUIRE_MSG(error == -1 && errno == ENOTCAPABLE,
	    "timerfd_gettime should fail with ENOTCAPABLE, got %s",
	    error == 0 ? "success" : strerror(errno));

	close(fd_limited);
	close(fd);
}

/*
 * Test that combined CAP_TIMERFD_GETTIME | CAP_TIMERFD_SETTIME allows both
 * operations.
 */
ATF_TC(cap_timerfd_both);
ATF_TC_HEAD(cap_timerfd_both, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that combined rights permit both gettime and settime");
}
ATF_TC_BODY(cap_timerfd_both, tc)
{
	struct itimerspec its, curr;
	cap_rights_t rights;
	int fd, fd_limited, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = timerfd_create(CLOCK_MONOTONIC, 0);
	ATF_REQUIRE_MSG(fd >= 0, "timerfd_create failed: %s", strerror(errno));

	fd_limited = dup(fd);
	ATF_REQUIRE(fd_limited >= 0);

	cap_rights_init(&rights, CAP_TIMERFD_GETTIME, CAP_TIMERFD_SETTIME);
	error = cap_rights_limit(fd_limited, &rights);
	ATF_REQUIRE_MSG(error == 0, "cap_rights_limit failed: %s",
	    strerror(errno));

	/* timerfd_settime should succeed */
	its.it_value.tv_sec = 1;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	error = timerfd_settime(fd_limited, 0, &its, NULL);
	ATF_REQUIRE_MSG(error == 0,
	    "timerfd_settime should succeed with combined rights, got %s",
	    strerror(errno));

	/* timerfd_gettime should succeed */
	error = timerfd_gettime(fd_limited, &curr);
	ATF_REQUIRE_MSG(error == 0,
	    "timerfd_gettime should succeed with combined rights, got %s",
	    strerror(errno));

	close(fd_limited);
	close(fd);
}

/*
 * Test that an unlimited timerfd descriptor allows all operations.
 */
ATF_TC(cap_timerfd_unlimited);
ATF_TC_HEAD(cap_timerfd_unlimited, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test that unlimited descriptor permits all operations");
}
ATF_TC_BODY(cap_timerfd_unlimited, tc)
{
	struct itimerspec its, curr;
	int fd, error;

	ATF_REQUIRE_FEATURE("security_capabilities");

	fd = timerfd_create(CLOCK_MONOTONIC, 0);
	ATF_REQUIRE_MSG(fd >= 0, "timerfd_create failed: %s", strerror(errno));

	/* timerfd_settime should succeed */
	its.it_value.tv_sec = 1;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	error = timerfd_settime(fd, 0, &its, NULL);
	ATF_REQUIRE_MSG(error == 0,
	    "timerfd_settime should succeed on unlimited descriptor, got %s",
	    strerror(errno));

	/* timerfd_gettime should succeed */
	error = timerfd_gettime(fd, &curr);
	ATF_REQUIRE_MSG(error == 0,
	    "timerfd_gettime should succeed on unlimited descriptor, got %s",
	    strerror(errno));

	close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, cap_timerfd_gettime_only);
	ATF_TP_ADD_TC(tp, cap_timerfd_settime_only);
	ATF_TP_ADD_TC(tp, cap_timerfd_both);
	ATF_TP_ADD_TC(tp, cap_timerfd_unlimited);

	return (atf_no_error());
}
