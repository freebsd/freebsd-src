/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <atf-c++.hpp>
#include <libutil.h>

#include <libutil++.hh>

ATF_TEST_CASE_WITHOUT_HEAD(FILE_up);
ATF_TEST_CASE_BODY(FILE_up)
{
	FILE *fp = fopen("/dev/null", "r");
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE(fileno(fp) != -1);

	freebsd::FILE_up f(fp);
	ATF_REQUIRE_EQ(fileno(fp), fileno(f.get()));

	f.reset();
	ATF_REQUIRE_EQ(f.get(), nullptr);

	ATF_REQUIRE_EQ(-1, fileno(fp));
	ATF_REQUIRE_EQ(EBADF, errno);
}

ATF_INIT_TEST_CASES(tcs)
{
	ATF_ADD_TEST_CASE(tcs, FILE_up);
}
