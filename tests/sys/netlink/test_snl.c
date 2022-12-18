#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/param.h>
#include <sys/module.h>

#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include "netlink/netlink_snl.h"
#include "netlink/netlink_snl_route.h"

#include <atf-c.h>

static void
require_netlink(void)
{
	if (modfind("netlink") == -1)
		atf_tc_skip("netlink module not loaded");
}


ATF_TC(snl_list_ifaces);
ATF_TC_HEAD(snl_list_ifaces, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) listing interfaces");
}

struct nl_parsed_link {
	uint32_t		ifi_index;
	uint32_t		ifla_mtu;
	char			*ifla_ifname;
};

#define	_IN(_field)	offsetof(struct ifinfomsg, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_link, _field)
static struct snl_attr_parser ap_link[] = {
	{ .type = IFLA_IFNAME, .off = _OUT(ifla_ifname), .cb = snl_attr_get_string },
	{ .type = IFLA_MTU, .off = _OUT(ifla_mtu), .cb = snl_attr_get_uint32 },
};
static struct snl_field_parser fp_link[] = {
	{.off_in = _IN(ifi_index), .off_out = _OUT(ifi_index), .cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(link_parser, struct ifinfomsg, fp_link, ap_link);


ATF_TC_BODY(snl_list_ifaces, tc)
{
	struct snl_state ss;

	require_netlink();

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	struct {
		struct nlmsghdr hdr;
		struct ifinfomsg ifmsg;
	} msg = {
		.hdr.nlmsg_type = RTM_GETLINK,
		.hdr.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
		.hdr.nlmsg_seq = snl_get_seq(&ss),
	};
	msg.hdr.nlmsg_len = sizeof(msg);

	if (!snl_send(&ss, &msg, sizeof(msg))) {
		snl_free(&ss);
		atf_tc_fail("snl_send() failed");
	}

	struct nlmsghdr *hdr;
	int count = 0;
	while ((hdr = snl_read_message(&ss)) != NULL && hdr->nlmsg_type != NLMSG_DONE) {
		if (hdr->nlmsg_seq != msg.hdr.nlmsg_seq)
			continue;

		struct nl_parsed_link link = {};
		if (!snl_parse_nlmsg(&ss, hdr, &link_parser, &link))
			continue;
		count++;
	}
	ATF_REQUIRE_MSG(count > 0, "Empty interface list");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, snl_list_ifaces);

	return (atf_no_error());
}
