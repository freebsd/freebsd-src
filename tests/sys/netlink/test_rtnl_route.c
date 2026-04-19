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

#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include "netlink/netlink_snl.h"
#include <netlink/netlink_snl_route.h>
#include <netlink/netlink_snl_route_compat.h>
#include <netlink/netlink_snl_route_parsers.h>

#include <unistd.h>
#include <time.h>

#include <atf-c.h>

static struct rtmsg *
prepare_rtm_by_dst(struct snl_writer *nw, char *dst)
{
	struct rtmsg *rtm;
	struct in_addr in_dst;

	inet_pton(AF_INET, dst, &in_dst);
	rtm = snl_reserve_msg_object(nw, struct rtmsg);
	if (rtm == NULL)
		return (NULL);

	rtm->rtm_family = AF_INET;
	rtm->rtm_protocol = RTPROT_STATIC;
	rtm->rtm_type = RTN_UNICAST;
	rtm->rtm_dst_len = 24;
	rtm->rtm_flags = RTF_GATEWAY;
	snl_add_msg_attr_ip4(nw, RTA_DST, &in_dst);

	return (rtm);
}

static void
cleanup_route_by_dst(struct snl_state *ss, struct snl_writer *nw, char *dst)
{
	struct nlmsghdr *hdr, *rx_hdr;
	struct snl_errmsg_data e = {};

	/* Delete route */
	snl_init_writer(ss, nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(nw, RTM_DELROUTE)) != NULL);
	ATF_REQUIRE(prepare_rtm_by_dst(nw, dst) != NULL);
	ATF_REQUIRE((hdr = snl_finalize_msg(nw)) != NULL);
	ATF_REQUIRE(snl_send_message(ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(ss, hdr->nlmsg_seq)) != NULL);
}

ATF_TC(rtnl_nhgrp);
ATF_TC_HEAD(rtnl_nhgrp, tc)
{
	atf_tc_set_md_var(tc, "descr", "test nexthop group using netlink");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(rtnl_nhgrp, tc)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr, *rx_hdr;
	struct in_addr gw1, gw2;
	struct snl_errmsg_data e = {};
	struct snl_parsed_route r = { .rtax_weight = RT_DEFAULT_WEIGHT };
	struct rtmsg *rtm;
	struct rtnexthop *rtnh;
	int off, off2;

	ATF_REQUIRE_MSG(snl_init(&ss, NETLINK_ROUTE), "snl_init() failed");

	inet_pton(AF_INET, "127.0.0.1", &gw1);
	inet_pton(AF_INET, "127.0.0.2", &gw2);

	/* Create new multipath route */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_NEWROUTE)) != NULL);
	hdr->nlmsg_flags |= NLM_F_CREATE;
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "192.0.2.0")) != NULL);

	off = snl_add_msg_attr_nested(&nw, RTA_MULTIPATH);
	/* first nexthop */
	off2 = snl_get_msg_offset(&nw);
	rtnh = snl_reserve_msg_object(&nw, struct rtnexthop);
	rtnh->rtnh_flags = 0;
	rtnh->rtnh_hops = 1;
	rtnh->rtnh_ifindex = 0;
	snl_add_msg_attr_ip4(&nw, RTA_GATEWAY, &gw1);
	rtnh = snl_restore_msg_offset(&nw, off2, struct rtnexthop);
	rtnh->rtnh_len = snl_get_msg_offset(&nw) - off2;

	/* second nexthop */
	off2 = snl_get_msg_offset(&nw);
	rtnh = snl_reserve_msg_object(&nw, struct rtnexthop);
	rtnh->rtnh_flags = 0;
	rtnh->rtnh_hops = 1;
	rtnh->rtnh_ifindex = 0;
	snl_add_msg_attr_ip4(&nw, RTA_GATEWAY, &gw2);
	rtnh = snl_restore_msg_offset(&nw, off2, struct rtnexthop);
	rtnh->rtnh_len = snl_get_msg_offset(&nw) - off2;

	snl_end_attr_nested(&nw, off);

	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_REQUIRE(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_REQUIRE_INTEQ(e.error, 0);

	/* Get route and check for its nexthop group */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_GETROUTE)) != NULL);
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "192.0.2.0")) != NULL);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_CHECK(snl_parse_nlmsg(&ss, rx_hdr, &snl_rtm_route_parser, &r));
	ATF_CHECK(r.rta_knh_id != 0);
	ATF_CHECK_INTEQ(r.rta_multipath.num_nhops, 2);

	cleanup_route_by_dst(&ss, &nw, "192.0.2.0");
}

ATF_TC(rtnl_nhop_merge);
ATF_TC_HEAD(rtnl_nhop_merge, tc)
{
	atf_tc_set_md_var(tc, "descr", "test merge of two independent nexthop using netlink");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(rtnl_nhop_merge, tc)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr, *rx_hdr;
	struct in_addr gw1, gw2;
	struct snl_errmsg_data e = {};
	struct snl_parsed_route r = { .rtax_weight = RT_DEFAULT_WEIGHT };
	struct rtmsg *rtm;
	struct rtnexthop *rtnh;

	ATF_REQUIRE_MSG(snl_init(&ss, NETLINK_ROUTE), "snl_init() failed");

	inet_pton(AF_INET, "127.0.1.1", &gw1);
	inet_pton(AF_INET, "127.0.1.2", &gw2);

	/* Create new route with single nhop */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_NEWROUTE)) != NULL);
	hdr->nlmsg_flags |= NLM_F_CREATE;
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "198.51.100.0")) != NULL);
	snl_add_msg_attr_ip4(&nw, RTA_GATEWAY, &gw1);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_REQUIRE(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_REQUIRE_INTEQ(e.error, 0);

	/* Get route and verify it's NOT a nexthop group */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_GETROUTE)) != NULL);
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "198.51.100.0")) != NULL);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_CHECK(snl_parse_nlmsg(&ss, rx_hdr, &snl_rtm_route_parser, &r));
	ATF_CHECK(r.rta_knh_id != 0);
	ATF_CHECK_INTEQ(r.rta_multipath.num_nhops, 0);

	/* Append anoher nhop */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_NEWROUTE)) != NULL);
	hdr->nlmsg_flags |= NLM_F_APPEND;
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "198.51.100.0")) != NULL);
	snl_add_msg_attr_ip4(&nw, RTA_GATEWAY, &gw2);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_REQUIRE(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_REQUIRE_INTEQ(e.error, 0);

	/* Get route and verify it became a nexthop group */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_GETROUTE)) != NULL);
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "198.51.100.0")) != NULL);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_CHECK(snl_parse_nlmsg(&ss, rx_hdr, &snl_rtm_route_parser, &r));
	ATF_CHECK(r.rta_knh_id != 0);
	ATF_CHECK_INTEQ(r.rta_multipath.num_nhops, 2);

	cleanup_route_by_dst(&ss, &nw, "198.51.100.0");
}

ATF_TC(rtnl_nhgrp_expire);
ATF_TC_HEAD(rtnl_nhgrp_expire, tc)
{
	atf_tc_set_md_var(tc, "descr", "test nhop expiration of a member inside nhgrp using netlink");
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.kmods", "netlink");
}

ATF_TC_BODY(rtnl_nhgrp_expire, tc)
{
	struct snl_state ss;
	struct snl_writer nw;
	struct nlmsghdr *hdr, *rx_hdr;
	struct in_addr gw1, gw2, gw3;
	struct snl_errmsg_data e = {};
	struct snl_parsed_route r = { .rtax_weight = RT_DEFAULT_WEIGHT };
	struct rtmsg *rtm;
	struct rtnexthop *rtnh;
	struct timespec ts;
	int off, off2;

	ATF_REQUIRE_MSG(snl_init(&ss, NETLINK_ROUTE), "snl_init() failed");

	inet_pton(AF_INET, "127.0.2.1", &gw1);
	inet_pton(AF_INET, "127.0.2.2", &gw2);
	inet_pton(AF_INET, "127.0.2.3", &gw3);

	/* create new multipath route */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_NEWROUTE)) != NULL);
	hdr->nlmsg_flags |= NLM_F_CREATE;
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "203.0.113.0")) != NULL);

	off = snl_add_msg_attr_nested(&nw, RTA_MULTIPATH);
	/* first nexthop */
	off2 = snl_get_msg_offset(&nw);
	rtnh = snl_reserve_msg_object(&nw, struct rtnexthop);
	rtnh->rtnh_flags = 0;
	rtnh->rtnh_hops = 1;
	rtnh->rtnh_ifindex = 0;
	snl_add_msg_attr_ip4(&nw, RTA_GATEWAY, &gw1);
	rtnh = snl_restore_msg_offset(&nw, off2, struct rtnexthop);
	rtnh->rtnh_len = snl_get_msg_offset(&nw) - off2;

	/* second nexthop */
	off2 = snl_get_msg_offset(&nw);
	rtnh = snl_reserve_msg_object(&nw, struct rtnexthop);
	rtnh->rtnh_flags = 0;
	rtnh->rtnh_hops = 1;
	rtnh->rtnh_ifindex = 0;
	snl_add_msg_attr_ip4(&nw, RTA_GATEWAY, &gw2);
	rtnh = snl_restore_msg_offset(&nw, off2, struct rtnexthop);
	rtnh->rtnh_len = snl_get_msg_offset(&nw) - off2;

	snl_end_attr_nested(&nw, off);

	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_REQUIRE(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_REQUIRE_INTEQ(e.error, 0);

	/* append anoher nhop with expiration time */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_NEWROUTE)) != NULL);
	hdr->nlmsg_flags |= NLM_F_APPEND;
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "203.0.113.0")) != NULL);
	snl_add_msg_attr_ip4(&nw, RTA_GATEWAY, &gw3);
	/* expire after 1 seconds */
	clock_gettime(CLOCK_REALTIME_FAST, &ts);
	snl_add_msg_attr_u32(&nw, RTA_EXPIRES, ts.tv_sec + 1);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_REQUIRE(snl_parse_errmsg(&ss, rx_hdr, &e));
	ATF_REQUIRE_INTEQ(e.error, 0);

	/* get route and check for number of nhops */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_GETROUTE)) != NULL);
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "203.0.113.0")) != NULL);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_CHECK(snl_parse_nlmsg(&ss, rx_hdr, &snl_rtm_route_parser, &r));
	ATF_CHECK(r.rta_knh_id != 0);
	ATF_CHECK_INTEQ(r.rta_multipath.num_nhops, 3);

	/* wait for 2 seconds and try again */
	sleep(2);

	/* get route and check for number of nhops */
	snl_init_writer(&ss, &nw);
	ATF_REQUIRE((hdr = snl_create_msg_request(&nw, RTM_GETROUTE)) != NULL);
	ATF_REQUIRE((rtm = prepare_rtm_by_dst(&nw, "203.0.113.0")) != NULL);
	ATF_REQUIRE((hdr = snl_finalize_msg(&nw)) != NULL);
	ATF_REQUIRE(snl_send_message(&ss, hdr));
	ATF_REQUIRE((rx_hdr = snl_read_reply(&ss, hdr->nlmsg_seq)) != NULL);
	ATF_CHECK(snl_parse_nlmsg(&ss, rx_hdr, &snl_rtm_route_parser, &r));
	ATF_CHECK_INTEQ(r.rta_multipath.num_nhops, 2);

	cleanup_route_by_dst(&ss, &nw, "203.0.113.0");
}


ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rtnl_nhgrp);
	ATF_TP_ADD_TC(tp, rtnl_nhgrp_expire);
	ATF_TP_ADD_TC(tp, rtnl_nhop_merge);

	return (atf_no_error());
}
