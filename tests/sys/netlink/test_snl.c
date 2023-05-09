#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/param.h>
#include <sys/module.h>

#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include "netlink/netlink_snl.h"
#include "netlink/netlink_snl_route.h"
#include "netlink/netlink_snl_route_parsers.h"

#include <atf-c.h>

static void
require_netlink(void)
{
	if (modfind("netlink") == -1)
		atf_tc_skip("netlink module not loaded");
}

ATF_TC(snl_verify_core_parsers);
ATF_TC_HEAD(snl_verify_core_parsers, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) core nlmsg parsers are correct");
}

ATF_TC_BODY(snl_verify_core_parsers, tc)
{
	SNL_VERIFY_PARSERS(snl_all_core_parsers);

}

ATF_TC(snl_verify_route_parsers);
ATF_TC_HEAD(snl_verify_route_parsers, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) route parsers are correct");
}

ATF_TC_BODY(snl_verify_route_parsers, tc)
{
	SNL_VERIFY_PARSERS(snl_all_route_parsers);

}

ATF_TC(snl_parse_errmsg_capped);
ATF_TC_HEAD(snl_parse_errmsg_capped, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) correctly parsing capped errors");
}

ATF_TC_BODY(snl_parse_errmsg_capped, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	require_netlink();

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	atf_tc_skip("does not work");

	int optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_CAP_ACK, &optval, sizeof(optval)) == 0);

	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, 255);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	snl_add_msg_attr_string(&nw, 143, "some random string");
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct nlmsghdr *rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	ATF_CHECK(rx_hdr != NULL);
	ATF_CHECK(rx_hdr->nlmsg_type == NLMSG_ERROR);

	struct snl_errmsg_data e = {};
	ATF_CHECK(rx_hdr->nlmsg_len == sizeof(struct nlmsghdr) + sizeof(struct nlmsgerr));
	ATF_CHECK(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_CHECK(e.error != 0);
	ATF_CHECK(!memcmp(hdr, e.orig_hdr, sizeof(struct nlmsghdr)));
}

ATF_TC(snl_parse_errmsg_capped_extack);
ATF_TC_HEAD(snl_parse_errmsg_capped_extack, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) correctly parsing capped errors with extack");
}

ATF_TC_BODY(snl_parse_errmsg_capped_extack, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	require_netlink();

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	int optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_CAP_ACK, &optval, sizeof(optval)) == 0);
	optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_EXT_ACK, &optval, sizeof(optval)) == 0);

	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, 255);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	snl_add_msg_attr_string(&nw, 143, "some random string");
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct nlmsghdr *rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	ATF_CHECK(rx_hdr != NULL);
	ATF_CHECK(rx_hdr->nlmsg_type == NLMSG_ERROR);

	struct snl_errmsg_data e = {};
	ATF_CHECK(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_CHECK(e.error != 0);
	ATF_CHECK(!memcmp(hdr, e.orig_hdr, sizeof(struct nlmsghdr)));

	ATF_CHECK(e.error_str != NULL);
}

ATF_TC(snl_parse_errmsg_uncapped_extack);
ATF_TC_HEAD(snl_parse_errmsg_uncapped_extack, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) correctly parsing errors with extack");
}

ATF_TC_BODY(snl_parse_errmsg_uncapped_extack, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	require_netlink();

	ATF_CHECK(snl_init(&ss, NETLINK_ROUTE));

	int optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_EXT_ACK, &optval, sizeof(optval)) == 0);

	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, 255);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	snl_add_msg_attr_string(&nw, 143, "some random string");
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct nlmsghdr *rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq);
	ATF_CHECK(rx_hdr != NULL);
	ATF_CHECK(rx_hdr->nlmsg_type == NLMSG_ERROR);

	struct snl_errmsg_data e = {};
	ATF_CHECK(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_CHECK(e.error != 0);
	ATF_CHECK(!memcmp(hdr, e.orig_hdr, hdr->nlmsg_len));

	ATF_CHECK(e.error_str != NULL);
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
	struct snl_writer nw;

	require_netlink();

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	snl_init_writer(&ss, &nw);

	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETLINK);
	ATF_CHECK(hdr != NULL);
	ATF_CHECK(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	ATF_CHECK(snl_finalize_msg(&nw) != NULL);
	uint32_t seq_id = hdr->nlmsg_seq;

	ATF_CHECK(snl_send_message(&ss, hdr));

	struct snl_errmsg_data e = {};
	int count = 0;

	while ((hdr = snl_read_reply_multi(&ss, seq_id, &e)) != NULL) {
		count++;
	}
	ATF_REQUIRE(e.error == 0);

	ATF_REQUIRE_MSG(count > 0, "Empty interface list");
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, snl_verify_core_parsers);
	ATF_TP_ADD_TC(tp, snl_verify_route_parsers);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_capped);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_capped_extack);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_uncapped_extack);
	ATF_TP_ADD_TC(tp, snl_list_ifaces);

	return (atf_no_error());
}
