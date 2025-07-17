/*-
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This software were developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <errno.h>
#include <unistd.h>

#include <atf-c.h>

ATF_TC(errno_basic);
ATF_TC_HEAD(errno_basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Verify basic functionality of errno");
}

ATF_TC_BODY(errno_basic, tc)
{
	int res;

	res = unlink("/non/existent/file");
	ATF_REQUIRE(res == -1);
	ATF_REQUIRE(errno == ENOENT);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, errno_basic);

	return (atf_no_error());
}
