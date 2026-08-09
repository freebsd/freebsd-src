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

static const struct snl_hdr_parser *snl_all_core_parsers[] = {
	&snl_errmsg_parser, &snl_donemsg_parser,
	&_nla_bit_parser, &_nla_bitset_parser,
};

static const struct snl_hdr_parser *snl_all_route_parsers[] = {
	&_metrics_mp_nh_parser, &_mpath_nh_parser, &_metrics_parser, &snl_rtm_route_parser,
	&_vf_extension_parser, &_vf_parser, &_vf_status_parser,
	&_link_fbsd_parser, &snl_rtm_link_parser, &snl_rtm_link_parser_simple,
	&_neigh_fbsd_parser, &snl_rtm_neigh_parser,
	&_addr_fbsd_parser, &snl_rtm_addr_parser, &_nh_fbsd_parser, &snl_nhmsg_parser,
};

ATF_TC(snl_verify_core_parsers);
ATF_TC_HEAD(snl_verify_core_parsers, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) core nlmsg parsers are correct");
}

ATF_TC_BODY(snl_verify_core_parsers, tc)
{
	SNL_VERIFY_PARSERS(snl_all_core_parsers);

}

ATF_TC(snl_parse_vf_status);
ATF_TC_HEAD(snl_parse_vf_status, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Tests snl(3) parsing nested VF status");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_vf_status, tc)
{
	static const uint8_t mac[] = { 0x02, 0, 0, 0, 0, 1 };
	static const uint8_t extension_data[] = { 1, 2, 3, 4 };
	struct snl_parsed_vf_extension *extension;
	struct snl_parsed_vf_status *status;
	struct snl_parsed_link link = {};
	struct snl_parsed_vf *vf;
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	int entry_off, extension_off, extensions_off, fbsd_off, list_off;
	int status_off;

	ATF_REQUIRE(snl_init(&ss, NETLINK_ROUTE));
	snl_init_writer(&ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	ATF_REQUIRE(hdr != NULL);
	ATF_REQUIRE(snl_reserve_msg_object(&nw, struct ifinfomsg) != NULL);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLA_NUM_VF, 1));

	fbsd_off = snl_add_msg_attr_nested(&nw, IFLA_FREEBSD);
	ATF_REQUIRE(fbsd_off != 0);
	status_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_STATUS);
	ATF_REQUIRE(status_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VFS_VERSION, 1));
	ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VFS_PF_LINK_STATE,
	    IFLAF_VF_LINK_UP));
	ATF_REQUIRE(snl_add_msg_attr_u64(&nw, IFLAF_VFS_PF_LINK_SPEED,
	    10000000000ULL));

	list_off = snl_add_msg_attr_nested(&nw, IFLAF_VFS_LIST);
	ATF_REQUIRE(list_off != 0);
	entry_off = snl_add_msg_attr_nested(&nw, IFLAF_VF_LIST_ENTRY);
	ATF_REQUIRE(entry_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_u32(&nw, IFLAF_VF_INDEX, 7));
	ATF_REQUIRE(snl_add_msg_attr_u8(&nw, IFLAF_VF_CONFIGURED, 1));
	ATF_REQUIRE(snl_add_msg_attr(&nw, IFLAF_VF_MAC, sizeof(mac), mac));

	extensions_off = snl_add_msg_attr_nested(&nw,
	    IFLAF_VF_EXTENSIONS);
	ATF_REQUIRE(extensions_off != 0);
	extension_off = snl_add_msg_attr_nested(&nw,
	    IFLAF_VF_EXT_LIST_ENTRY);
	ATF_REQUIRE(extension_off != 0);
	ATF_REQUIRE(snl_add_msg_attr_string(&nw, IFLAF_VF_EXT_NAME,
	    "driver.test"));
	ATF_REQUIRE(snl_add_msg_attr(&nw, IFLAF_VF_EXT_DATA,
	    sizeof(extension_data), extension_data));
	snl_end_attr_nested(&nw, extension_off);
	snl_end_attr_nested(&nw, extensions_off);
	snl_end_attr_nested(&nw, entry_off);
	snl_end_attr_nested(&nw, list_off);
	snl_end_attr_nested(&nw, status_off);
	snl_end_attr_nested(&nw, fbsd_off);
	ATF_REQUIRE(snl_finalize_msg(&nw) != NULL);

	ATF_REQUIRE(snl_parse_nlmsg(&ss, hdr, &snl_rtm_link_parser, &link));
	ATF_CHECK_EQ(link.ifla_num_vf, 1);
	status = &link.iflaf_vf_status;
	ATF_CHECK_EQ(status->version, 1);
	ATF_CHECK_EQ(status->pf_link_state, IFLAF_VF_LINK_UP);
	ATF_CHECK_EQ(status->pf_link_speed, 10000000000ULL);
	ATF_REQUIRE_EQ(status->vfs.count, 1);
	vf = status->vfs.items[0];
	ATF_CHECK_EQ(vf->index, 7);
	ATF_CHECK_EQ(vf->configured, 1);
	ATF_REQUIRE(vf->mac != NULL);
	ATF_CHECK_EQ(NLA_DATA_LEN(vf->mac), sizeof(mac));
	ATF_CHECK(memcmp(NLA_DATA(vf->mac), mac, sizeof(mac)) == 0);
	ATF_REQUIRE_EQ(vf->extensions.count, 1);
	extension = vf->extensions.items[0];
	ATF_CHECK_STREQ(extension->name, "driver.test");
	ATF_REQUIRE(extension->data != NULL);
	ATF_CHECK_EQ(NLA_DATA_LEN(extension->data), sizeof(extension_data));
	ATF_CHECK(memcmp(NLA_DATA(extension->data), extension_data,
	    sizeof(extension_data)) == 0);
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
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_errmsg_capped, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

	if (!snl_init(&ss, NETLINK_ROUTE))
		atf_tc_fail("snl_init() failed");

	int optval = 1;
	ATF_CHECK(setsockopt(ss.fd, SOL_NETLINK, NETLINK_CAP_ACK, &optval, sizeof(optval)) == 0);

	optval = 0;
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
	ATF_CHECK(rx_hdr->nlmsg_len == sizeof(struct nlmsghdr) + sizeof(struct nlmsgerr));
	ATF_CHECK(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_CHECK(e.error != 0);
	ATF_CHECK(!memcmp(hdr, e.orig_hdr, sizeof(struct nlmsghdr)));
}

ATF_TC(snl_parse_errmsg_capped_extack);
ATF_TC_HEAD(snl_parse_errmsg_capped_extack, tc)
{
	atf_tc_set_md_var(tc, "descr", "Tests snl(3) correctly parsing capped errors with extack");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_errmsg_capped_extack, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

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
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(snl_parse_errmsg_uncapped_extack, tc)
{
	struct snl_state ss;
	struct snl_writer nw;

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
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
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
	ATF_TP_ADD_TC(tp, snl_parse_vf_status);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_capped);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_capped_extack);
	ATF_TP_ADD_TC(tp, snl_parse_errmsg_uncapped_extack);
	ATF_TP_ADD_TC(tp, snl_list_ifaces);

	return (atf_no_error());
}
