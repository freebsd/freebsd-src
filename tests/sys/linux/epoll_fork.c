/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include <atf-c.h>
#include "linux_test_support.h"

ATF_TC(epoll_descriptor);
ATF_TC_HEAD(epoll_descriptor, tc)
{

	atf_tc_set_md_var(tc, "descr", "Preserve Linux epoll descriptors and "
	    "registrations across fork");
	atf_tc_set_md_var(tc, "require.kmods", "linux64");
}
ATF_TC_BODY(epoll_descriptor, tc)
{
	linux_test_run(tc, "epoll_fork_linux");
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, epoll_descriptor);
	return (atf_no_error());
}
