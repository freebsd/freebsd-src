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

ATF_TC(test_snl_get_genl_family_groups);
ATF_TC_HEAD(test_snl_get_genl_family_groups, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests getting 'nlctrl' groups");
}

ATF_TC_BODY(test_snl_get_genl_family_groups, tc)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;

	require_netlink();

	if (!snl_init(&ss, NETLINK_GENERIC))
		atf_tc_fail("snl_init() failed");

	snl_init_writer(&ss, &nw);
	hdr = snl_create_genl_msg_request(&nw, GENL_ID_CTRL, CTRL_CMD_GETFAMILY);
	snl_add_msg_attr_string(&nw, CTRL_ATTR_FAMILY_NAME, "nlctrl");
	snl_finalize_msg(&nw);
	snl_send_message(&ss, hdr);

	hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(hdr->nlmsg_type != NLMSG_ERROR);

	struct _getfamily_attrs attrs = {};

	ATF_CHECK(snl_parse_nlmsg(&ss, hdr, &_genl_ctrl_getfam_parser, &attrs));
	ATF_CHECK_EQ(attrs.mcast_groups.num_groups, 1);

	struct snl_genl_ctrl_mcast_group *group = attrs.mcast_groups.groups[0];

	ATF_CHECK(group->mcast_grp_id > 0);
	ATF_CHECK(!strcmp(group->mcast_grp_name, "notify"));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, snl_verify_genl_parsers);
	ATF_TP_ADD_TC(tp, test_snl_get_genl_family_success);
	ATF_TP_ADD_TC(tp, test_snl_get_genl_family_failure);
	ATF_TP_ADD_TC(tp, test_snl_get_genl_family_groups);

	return (atf_no_error());
}

