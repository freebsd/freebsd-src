/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netlink/netlink.h>
#include <netlink/netlink_route.h>
#include <netlink/netlink_snl.h>
#include <netlink/netlink_snl_route.h>
#include <netlink/netlink_snl_route_parsers.h>
#include <netlink/netlink_snl_route_compat.h>

#include <netinet/in.h>
#include <netgraph/ng_socket.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <libutil.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <err.h>
#include <libxo/xo.h>
#include "netstat.h"
#include "common.h"
#include "nl_defs.h"


static void p_rtentry_netlink(struct snl_state *ss, const char *name, struct nlmsghdr *hdr);

static struct ifmap_entry *ifmap;
static size_t ifmap_size;

/* Generate ifmap using netlink */
static struct ifmap_entry *
prepare_ifmap_netlink(struct snl_state *ss, size_t *pifmap_size)
{
	struct {
		struct nlmsghdr hdr;
		struct ifinfomsg ifmsg;
	} msg = {
		.hdr.nlmsg_type = RTM_GETLINK,
		.hdr.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
		.hdr.nlmsg_seq = snl_get_seq(ss),
	};
	msg.hdr.nlmsg_len = sizeof(msg);

	if (!snl_send_message(ss, &msg.hdr))
		return (NULL);

	struct ifmap_entry *ifmap = NULL;
	uint32_t ifmap_size = 0;
	struct nlmsghdr *hdr;
	struct snl_errmsg_data e = {};

	while ((hdr = snl_read_reply_multi(ss, msg.hdr.nlmsg_seq, &e)) != NULL) {
		struct snl_parsed_link_simple link = {};

		if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_link_parser_simple, &link))
			continue;
		if (link.ifi_index >= ifmap_size) {
			size_t size = roundup2(link.ifi_index + 1, 32) * sizeof(struct ifmap_entry);
			if ((ifmap = realloc(ifmap, size)) == NULL)
				errx(2, "realloc(%zu) failed", size);
			memset(&ifmap[ifmap_size], 0,
			    size - ifmap_size *
			    sizeof(struct ifmap_entry));
			ifmap_size = roundup2(link.ifi_index + 1, 32);
		}
		if (*ifmap[link.ifi_index].ifname != '\0')
			continue;
		strlcpy(ifmap[link.ifi_index].ifname, link.ifla_ifname, IFNAMSIZ);
		ifmap[link.ifi_index].mtu = link.ifla_mtu;
	}
	*pifmap_size = ifmap_size;
	return (ifmap);
}

static void
ip6_writemask(struct in6_addr *addr6, uint8_t mask)
{
	uint32_t *cp;

	for (cp = (uint32_t *)addr6; mask >= 32; mask -= 32)
		*cp++ = 0xFFFFFFFF;
	if (mask > 0)
		*cp = htonl(mask ? ~((1 << (32 - mask)) - 1) : 0);
}

static void
gen_mask(int family, int plen, struct sockaddr *sa)
{
	if (family == AF_INET6) {
		struct sockaddr_in6 sin6 = {
			.sin6_family = AF_INET6,
			.sin6_len = sizeof(struct sockaddr_in6),
		};
		ip6_writemask(&sin6.sin6_addr, plen);
		*((struct sockaddr_in6 *)sa) = sin6;
	} else if (family == AF_INET) {
		struct sockaddr_in sin = {
			.sin_family = AF_INET,
			.sin_len = sizeof(struct sockaddr_in),
			.sin_addr.s_addr = htonl(plen ? ~((1 << (32 - plen)) - 1) : 0),
		};
		*((struct sockaddr_in *)sa) = sin;
	}
}

static void
add_scopeid(struct sockaddr *sa, int ifindex)
{
	if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
			sin6->sin6_scope_id = ifindex;
	}
}

static void
p_path(struct snl_parsed_route *rt, bool is_mpath)
{
	struct sockaddr_in6 mask6;
	struct sockaddr *pmask = (struct sockaddr *)&mask6;
	char buffer[128];
	char prettyname[128];
	int protrusion;

	gen_mask(rt->rtm_family, rt->rtm_dst_len, pmask);
	add_scopeid(rt->rta_dst, rt->rta_oif);
	add_scopeid(rt->rta_gw, rt->rta_oif);
	protrusion = p_sockaddr("destination", rt->rta_dst, pmask, rt->rta_rtflags, wid.dst);
	protrusion = p_sockaddr("gateway", rt->rta_gw, NULL, RTF_HOST,
	    wid.gw - protrusion);
	snprintf(buffer, sizeof(buffer), "{[:-%d}{:flags/%%s}{]:} ",
	    wid.flags - protrusion);
	p_flags(rt->rta_rtflags | RTF_UP, buffer);
	/* Output path weight as non-visual property */
	xo_emit("{e:weight/%u}", rt->rtax_weight);
	if (is_mpath)
		xo_emit("{e:nhg-kidx/%u}", rt->rta_knh_id);
	else
		xo_emit("{e:nhop-kidx/%u}", rt->rta_knh_id);
	if (rt->rta_nh_id != 0) {
		if (is_mpath)
			xo_emit("{e:nhg-uidx/%u}", rt->rta_nh_id);
		else
			xo_emit("{e:nhop-uidx/%u}", rt->rta_nh_id);
	}

	memset(prettyname, 0, sizeof(prettyname));
	if (rt->rta_oif < ifmap_size) {
		strlcpy(prettyname, ifmap[rt->rta_oif].ifname,
		    sizeof(prettyname));
		if (*prettyname == '\0')
			strlcpy(prettyname, "---", sizeof(prettyname));
		if (rt->rtax_mtu == 0)
			rt->rtax_mtu = ifmap[rt->rta_oif].mtu;
	}

	if (Wflag) {
		/* XXX: use=0? */
		xo_emit("{t:nhop/%*lu} ", wid.mtu, is_mpath ? 0 : rt->rta_knh_id);

		if (rt->rtax_mtu != 0)
			xo_emit("{t:mtu/%*lu} ", wid.mtu, rt->rtax_mtu);
		else {
			/* use interface mtu */
			xo_emit("{P:/%*s} ", wid.mtu, "");
		}

	}

	if (Wflag)
		xo_emit("{t:interface-name/%*s}", wid.iface, prettyname);
	else
		xo_emit("{t:interface-name/%*.*s}", wid.iface, wid.iface,
		    prettyname);
	if (rt->rta_expires > 0) {
		xo_emit(" {:expire-time/%*u}", wid.expire, rt->rta_expires);
	}
}

static void
p_rtentry_netlink(struct snl_state *ss, const char *name, struct nlmsghdr *hdr)
{

	struct snl_parsed_route rt = {};
	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_route_parser, &rt))
		return;
	if (rt.rtax_weight == 0)
		rt.rtax_weight = rt_default_weight;

	if (rt.rta_multipath.num_nhops != 0) {
		uint32_t orig_rtflags = rt.rta_rtflags;
		uint32_t orig_mtu = rt.rtax_mtu;
		for (uint32_t i = 0; i < rt.rta_multipath.num_nhops; i++) {
			struct rta_mpath_nh *nhop = rt.rta_multipath.nhops[i];

			rt.rta_gw = nhop->gw;
			rt.rta_oif = nhop->ifindex;
			rt.rtax_weight = nhop->rtnh_weight;
			rt.rta_rtflags = nhop->rta_rtflags ? nhop->rta_rtflags : orig_rtflags;
			rt.rtax_mtu = nhop->rtax_mtu ? nhop->rtax_mtu : orig_mtu;

			xo_open_instance(name);
			p_path(&rt, true);
			xo_emit("\n");
			xo_close_instance(name);
		}
		return;
	}

	struct sockaddr_dl sdl_gw = {
		.sdl_family = AF_LINK,
		.sdl_len = sizeof(struct sockaddr_dl),
		.sdl_index = rt.rta_oif,
	};
	if (rt.rta_gw == NULL)
		rt.rta_gw = (struct sockaddr *)&sdl_gw;

	xo_open_instance(name);
	p_path(&rt, false);
	xo_emit("\n");
	xo_close_instance(name);
}

bool
p_rtable_netlink(int fibnum, int af)
{
	int fam = AF_UNSPEC;
	int need_table_close = false;
	struct nlmsghdr *hdr;
	struct snl_errmsg_data e = {};
	struct snl_state ss = {};

	if (!snl_init(&ss, NETLINK_ROUTE))
		return (false);

	ifmap = prepare_ifmap_netlink(&ss, &ifmap_size);
	if (ifmap == NULL) {
		snl_free(&ss);
		return (false);
	}

	struct {
		struct nlmsghdr hdr;
		struct rtmsg rtmsg;
		struct nlattr nla_fibnum;
		uint32_t fibnum;
	} msg = {
		.hdr.nlmsg_type = RTM_GETROUTE,
		.hdr.nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST,
		.hdr.nlmsg_seq = snl_get_seq(&ss),
		.rtmsg.rtm_family = af,
		.nla_fibnum.nla_len = sizeof(struct nlattr) + sizeof(uint32_t),
		.nla_fibnum.nla_type = RTA_TABLE,
		.fibnum = fibnum,
	};
	msg.hdr.nlmsg_len = sizeof(msg);

	if (!snl_send_message(&ss, &msg.hdr)) {
		snl_free(&ss);
		return (false);
	}

	xo_open_container("route-table");
	xo_open_list("rt-family");
	while ((hdr = snl_read_reply_multi(&ss, msg.hdr.nlmsg_seq, &e)) != NULL) {
		struct rtmsg *rtm = (struct rtmsg *)(hdr + 1);
		/* Only print family first time. */
		if (fam != rtm->rtm_family) {
			if (need_table_close) {
				xo_close_list("rt-entry");
				xo_close_instance("rt-family");
			}
			need_table_close = true;
			fam = rtm->rtm_family;
			set_wid(fam);
			xo_open_instance("rt-family");
			pr_family(fam);
			xo_open_list("rt-entry");
			pr_rthdr(fam);
		}
		p_rtentry_netlink(&ss, "rt-entry", hdr);
		snl_clear_lb(&ss);
	}
	if (need_table_close) {
		xo_close_list("rt-entry");
		xo_close_instance("rt-family");
	}
	xo_close_list("rt-family");
	xo_close_container("route-table");
	snl_free(&ss);
	return (true);
}


