#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/param.h>
#include <sys/module.h>

#include <netlink/netlink.h>
#include "netlink/netlink_snl.h"
#include "netlink/netlink_snl_generic.h"

#include <atf-c.h>

static void
require_netlink(void)
{
	if (modfind("netlink") == -1)
		atf_tc_skip("netlink module not loaded");
}

ATF_TC(snl_verify_genl_parsers);
ATF_TC_HEAD(snl_verify_genl_parsers, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) generic parsers are correct");
}

ATF_TC_BODY(snl_verify_genl_parsers, tc)
{
	SNL_VERIFY_PARSERS(snl_all_genl_parsers);

}

ATF_TC(test_snl_get_genl_family_success);
ATF_TC_HEAD(test_snl_get_genl_family_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests successfull resolution of the 'nlctrl' family");
}

ATF_TC_BODY(test_snl_get_genl_family_success, tc)
{
	struct snl_state ss;

	require_netlink();

	if (!snl_init(&ss, NETLINK_GENERIC))
		atf_tc_fail("snl_init() failed");

	ATF_CHECK_EQ(snl_get_genl_family(&ss, "nlctrl"), GENL_ID_CTRL);
}

ATF_TC(test_snl_get_genl_family_failure);
ATF_TC_HEAD(test_snl_get_genl_family_failure, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests unsuccessfull resolution of 'no-such-family' family");
}

ATF_TC_BODY(test_snl_get_genl_family_failure, tc)
{
	struct snl_state ss;

	require_netlink();

	if (!snl_init(&ss, NETLINK_GENERIC))
		atf_tc_fail("snl_init() failed");

	ATF_CHECK_EQ(snl_get_genl_family(&ss, "no-such-family"), 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, snl_verify_genl_parsers);
	ATF_TP_ADD_TC(tp, test_snl_get_genl_family_success);
	ATF_TP_ADD_TC(tp, test_snl_get_genl_family_failure);

	return (atf_no_error());
}

