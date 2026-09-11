/*
 * Copyright (c) 2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#ifndef WITHOUT_NETLINK
#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_route.h>
#include <netlink/netlink_snl_route_compat.h>
#include <netlink/netlink_snl_route_parsers.h>
#endif

#include "dhcpd.h"

#ifndef WITHOUT_NETLINK
/*
 * Check if the interface has a routable IPv6 address, if true
 * assume it has IPv6 connectivity.
 * Returns true if a unicast IPv6 address with non-link-local scope
 * is found, false otherwise.
 */
static bool
check_ipv6_address(struct snl_state *ss, uint16_t ifindex)
{
	struct snl_writer nw;
	struct snl_errmsg_data e = {};
	struct snl_parsed_addr addr = {};
	struct nlmsghdr *hdr, *rx_hdr;
	struct ifaddrmsg *ifahdr;

	snl_init_writer(ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_GETADDR);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	ifahdr = snl_reserve_msg_object(&nw, struct ifaddrmsg);
	ifahdr->ifa_family = AF_INET6;
	ifahdr->ifa_index = ifindex;

	if ((hdr = snl_finalize_msg(&nw)) == NULL || !snl_send_message(ss, hdr))
		return (false);

	while ((rx_hdr = snl_read_reply_multi(ss, hdr->nlmsg_seq, &e)) != NULL) {
		struct sockaddr_in6 *sin6;

		if (!snl_parse_nlmsg(ss, rx_hdr, &snl_rtm_addr_parser, &addr))
			continue;

		if (addr.ifa_address != NULL) {
			sin6 = (struct sockaddr_in6 *)addr.ifa_address;
			if (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				return (true);
		}
	}
	return (false);
}

/*
 * If a default route is found, assume IPv6 connectivity.
 * Returns true if found, false otherwise.
 */
static bool
check_ipv6_defaultroute(struct snl_state *ss)
{
	struct snl_writer nw;
	struct snl_parsed_route r = {};
	struct in6_addr in6 = IN6ADDR_ANY_INIT;
	struct nlmsghdr *hdr, *rx_hdr;
	struct rtmsg *rtmsg;

	snl_init_writer(ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_GETROUTE);
	rtmsg = snl_reserve_msg_object(&nw, struct rtmsg);
	rtmsg->rtm_family = AF_INET6;
	rtmsg->rtm_dst_len = 0;
	rtmsg->rtm_type = RTN_UNICAST;
	rtmsg->rtm_flags = RTM_F_PREFIX;
	snl_add_msg_attr_ip6(&nw, RTA_DST, &in6);

	if ((hdr = snl_finalize_msg(&nw)) == NULL || !snl_send_message(ss, hdr))
		return (false);

	rx_hdr = snl_read_reply(ss, hdr->nlmsg_seq);
	if (rx_hdr == NULL || rx_hdr->nlmsg_type != NL_RTM_NEWROUTE)
		return (false);

	if (!snl_parse_nlmsg(ss, rx_hdr, &snl_rtm_route_parser, &r))
		return (false);

	if (r.rta_gw == NULL)
		return (false);

	return (true);
}
#endif

bool
check_ipv6_connectivity(uint16_t ifindex __unused)
{
	bool ret = false;

#ifndef WITHOUT_NETLINK
	/* Return true if the interface has a routable ipv6 address */
	ret = check_ipv6_address(&nl_ss, ifindex);

	/* A default route using a link-local address may exist. */
	if (!ret)
		ret = check_ipv6_defaultroute(&nl_ss);
	snl_clear_lb(&nl_ss);
#endif

	return (ret);
}
