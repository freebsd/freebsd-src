/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <atf-c++.hpp>
#include <sys/stat.h>
#include <libutil.h>

#include <libutil++.hh>

ATF_TEST_CASE_WITHOUT_HEAD(basic);
ATF_TEST_CASE_BODY(basic)
{
	pid_t other;
	struct pidfh *pfh = pidfile_open("test_pidfile", 0600, &other);
	ATF_REQUIRE(pfh != nullptr);
	ATF_REQUIRE(pidfile_fileno(pfh) >= 0);

	struct stat sb;
	ATF_REQUIRE(fstat(pidfile_fileno(pfh), &sb) == 0);
	ATF_REQUIRE_EQ(0, sb.st_size);

	freebsd::pidfile pf(pfh);
	ATF_REQUIRE_EQ(pidfile_fileno(pfh), pf.fileno());

	ATF_REQUIRE(pf.write() == 0);
	
	ATF_REQUIRE(fstat(pf.fileno(), &sb) == 0);
	ATF_REQUIRE(sb.st_size > 0);

	ATF_REQUIRE(pf.close() == 0);
	ATF_REQUIRE(pf.fileno() == -1);
	ATF_REQUIRE_EQ(EDOOFUS, errno);

	ATF_REQUIRE(unlink("test_pidfile") == 0);
}

ATF_INIT_TEST_CASES(tcs)
{
	ATF_ADD_TEST_CASE(tcs, basic);
}
