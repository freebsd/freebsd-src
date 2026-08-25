/*
 * Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_gif.h>

#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_route.h>
#include <netlink/netlink_snl_route_compat.h>
#include <netlink/netlink_snl_route_parsers.h>

#include <atf-c.h>

struct nl_parsed_gif {
	struct sockaddr		*ifla_local;
	struct sockaddr		*ifla_remote;
	uint32_t		ifla_flags;
};

struct nla_gif_info {
	const char		*kind;
	struct nl_parsed_gif	data;
};

struct nla_gif_link {
	uint32_t		ifi_index;
	struct nla_gif_info	linkinfo;
};

#define _OUT(_field)	offsetof(struct nl_parsed_gif, _field)
static const struct snl_attr_parser nla_p_gif[] = {
	{ .type = IFLA_IPTUN_LOCAL, .off = _OUT(ifla_local), .cb = snl_attr_get_ip },
	{ .type = IFLA_IPTUN_REMOTE, .off = _OUT(ifla_remote), .cb = snl_attr_get_ip },
	{ .type = IFLA_IPTUN_FLAGS, .off = _OUT(ifla_flags), .cb = snl_attr_get_uint32 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(gif_linkinfo_data_parser, nla_p_gif);

#define _OUT(_field)	offsetof(struct nla_gif_info, _field)
static const struct snl_attr_parser ap_gif_linkinfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(kind), .cb = snl_attr_get_string },
	{ .type = IFLA_INFO_DATA, .off = _OUT(data),
		.arg = &gif_linkinfo_data_parser, .cb = snl_attr_get_nested },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(gif_linkinfo_parser, ap_gif_linkinfo);

#define _IN(_field)	offsetof(struct ifinfomsg, _field)
#define _OUT(_field)	offsetof(struct nla_gif_link, _field)
static const struct snl_attr_parser ap_gif_link[] = {
	{ .type = IFLA_LINKINFO, .off = _OUT(linkinfo),
		.arg = &gif_linkinfo_parser, .cb = snl_attr_get_nested },
};

static const struct snl_field_parser fp_gif_link[] = {
	{ .off_in = _IN(ifi_index), .off_out = _OUT(ifi_index), .cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(gif_parser, struct ifinfomsg, fp_gif_link, ap_gif_link);

ATF_TC(test_rtnl_gif);
ATF_TC_HEAD(test_rtnl_gif, tc)
{
	atf_tc_set_md_var(tc, "descr", "test gif interface using netlink");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.kmods", "netlink if_gif");
}

ATF_TC_BODY(test_rtnl_gif, tc)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr, *rx_hdr;
	struct in_addr src, dst;
	struct nla_gif_link lattrs = {};
	struct nl_parsed_gif attrs = {};
	struct snl_errmsg_data e = {};
	struct ifinfomsg *ifmsg;
	int off, off2;

	ATF_REQUIRE_MSG(snl_init(&ss, NETLINK_ROUTE), "snl_init() failed");

	/* Create gif interface */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_NEWLINK)) != NULL);
	hdr->nlmsg_flags |= (NLM_F_CREATE | NLM_F_EXCL | NLM_F_REQUEST | NLM_F_ACK);
	snl_reserve_msg_object(&nw, struct ifinfomsg);

	/* Create parameters */
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, "gif10");
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "gif");
	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

	inet_pton(AF_INET, "127.0.0.1", &src);
	inet_pton(AF_INET, "127.0.0.2", &dst);
	snl_add_msg_attr_ip4(&nw, IFLA_IPTUN_LOCAL, &src);
	snl_add_msg_attr_ip4(&nw, IFLA_IPTUN_REMOTE, &dst);
	snl_add_msg_attr_u32(&nw, IFLA_IPTUN_FLAGS, (GIF_NOCLAMP | GIF_IGNORE_SOURCE));

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_REQUIRE(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_REQUIRE_INTEQ(e.error, 0);

	/* Dump gif interface */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_GETLINK)) != NULL);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, "gif10");
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "gif");
	snl_end_attr_nested(&nw, off);

	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));

	/* Check parameters */
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_CHECK(snl_parse_nlmsg(&ss, rx_hdr, &gif_parser, &lattrs));
	attrs = lattrs.linkinfo.data;
	ATF_CHECK_STREQ(lattrs.linkinfo.kind, "gif");
	ATF_CHECK_INTEQ(attrs.ifla_flags, (GIF_NOCLAMP | GIF_IGNORE_SOURCE));

	/* Delete gif interface */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_DELLINK)) != NULL);
	hdr->nlmsg_flags |= (NLM_F_ACK | NLM_F_REQUEST);
	ATF_REQUIRE((ifmsg = snl_reserve_msg_object(&nw, struct ifinfomsg)) != NULL);
	ifmsg->ifi_index = lattrs.ifi_index;
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_REQUIRE(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_REQUIRE_INTEQ(e.error, 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, test_rtnl_gif);

	return (atf_no_error());
}

