/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Vladimir Grebenshchikov <vova@zote.me>
 */

#include <atf-c.h>
#include "linux_test_support.h"

ATF_TC(ip_pktinfo);
ATF_TC_HEAD(ip_pktinfo, tc)
{

	atf_tc_set_md_var(tc, "descr",
	    "Receive Linux IP_PKTINFO ancillary data");
	atf_tc_set_md_var(tc, "require.kmods", "linux64");
}
ATF_TC_BODY(ip_pktinfo, tc)
{
	linux_test_run(tc, "ip_pktinfo_linux");
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, ip_pktinfo);
	return (atf_no_error());
}
