/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Ng Peng Nam Sean
 * Copyright (c) 2022 Alexander V. Chernikov <melifaro@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_route.h"
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/rmlock.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#define	DEBUG_MOD_NAME	nl_route
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_DEBUG);

static unsigned char
get_rtm_type(const struct nhop_object *nh)
{
	int nh_flags = nh->nh_flags;

	/* Use the fact that nhg runtime flags are only NHF_MULTIPATH */
	if (nh_flags & NHF_BLACKHOLE)
		return (RTN_BLACKHOLE);
	else if (nh_flags & NHF_REJECT)
		return (RTN_PROHIBIT);
	return (RTN_UNICAST);
}

static uint8_t
nl_get_rtm_protocol(const struct nhop_object *nh)
{
#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh)) {
		const struct nhgrp_object *nhg = (const struct nhgrp_object *)nh;
		uint8_t origin = nhgrp_get_origin(nhg);
		if (origin != RTPROT_UNSPEC)
			return (origin);
		nh = nhg->nhops[0];
	}
#endif
	uint8_t origin = nhop_get_origin(nh);
	if (origin != RTPROT_UNSPEC)
		return (origin);
	/* TODO: remove guesswork once all kernel users fill in origin */
	int rt_flags = nhop_get_rtflags(nh);
	if (rt_flags & RTF_PROTO1)
		return (RTPROT_ZEBRA);
	if (rt_flags & RTF_STATIC)
		return (RTPROT_STATIC);
	return (RTPROT_KERNEL);
}

static int
get_rtmsg_type_from_rtsock(int cmd)
{
	switch (cmd) {
	case RTM_ADD:
	case RTM_CHANGE:
	case RTM_GET:
		return NL_RTM_NEWROUTE;
	case RTM_DELETE:
		return NL_RTM_DELROUTE;
	}

	return (0);
}

/*
 * fibnum heuristics
 *
 * if (dump && rtm_table == 0 && !rta_table) RT_ALL_FIBS
 * msg                rtm_table     RTA_TABLE            result
 * RTM_GETROUTE/dump          0             -       RT_ALL_FIBS
 * RTM_GETROUTE/dump          1             -                 1
 * RTM_GETROUTE/get           0             -                 0
 *
 */

static struct nhop_object *
rc_get_nhop(const struct rib_cmd_info *rc)
{
	return ((rc->rc_cmd == RTM_DELETE) ? rc->rc_nh_old : rc->rc_nh_new);
}

static void
dump_rc_nhop_gw(struct nl_writer *nw, const struct nhop_object *nh)
{
	int upper_family;

	switch (nhop_get_neigh_family(nh)) {
	case AF_LINK:
		/* onlink prefix, skip */
		break;
	case AF_INET:
		nlattr_add(nw, NL_RTA_GATEWAY, 4, &nh->gw4_sa.sin_addr);
		break;
	case AF_INET6:
		upper_family = nhop_get_upper_family(nh);
		if (upper_family == AF_INET6) {
			nlattr_add(nw, NL_RTA_GATEWAY, 16, &nh->gw6_sa.sin6_addr);
		} else if (upper_family == AF_INET) {
			/* IPv4 over IPv6 */
			char buf[20];
			struct rtvia *via = (struct rtvia *)&buf[0];
			via->rtvia_family = AF_INET6;
			memcpy(via->rtvia_addr, &nh->gw6_sa.sin6_addr, 16);
			nlattr_add(nw, NL_RTA_VIA, 17, via);
		}
		break;
	}
}

static void
dump_rc_nhop_mtu(struct nl_writer *nw, const struct nhop_object *nh)
{
	int nla_len = sizeof(struct nlattr) * 2 + sizeof(uint32_t);
	struct nlattr *nla = nlmsg_reserve_data(nw, nla_len, struct nlattr);

	if (nla == NULL)
		return;
	nla->nla_type = NL_RTA_METRICS;
	nla->nla_len = nla_len;
	nla++;
	nla->nla_type = NL_RTAX_MTU;
	nla->nla_len = sizeof(struct nlattr) + sizeof(uint32_t);
	*((uint32_t *)(nla + 1)) = nh->nh_mtu;
}

#ifdef ROUTE_MPATH
static void
dump_rc_nhg(struct nl_writer *nw, const struct nhgrp_object *nhg, struct rtmsg *rtm)
{
	uint32_t uidx = nhgrp_get_uidx(nhg);
	uint32_t num_nhops;
	const struct weightened_nhop *wn = nhgrp_get_nhops(nhg, &num_nhops);
	uint32_t base_rtflags = nhop_get_rtflags(wn[0].nh);

	if (uidx != 0)
		nlattr_add_u32(nw, NL_RTA_NH_ID, uidx);

	nlattr_add_u32(nw, NL_RTA_RTFLAGS, base_rtflags);
	int off = nlattr_add_nested(nw, NL_RTA_MULTIPATH);
	if (off == 0)
		return;

	for (int i = 0; i < num_nhops; i++) {
		int nh_off = nlattr_save_offset(nw);
		struct rtnexthop *rtnh = nlmsg_reserve_object(nw, struct rtnexthop);
		if (rtnh == NULL)
			return;
		rtnh->rtnh_flags = 0;
		rtnh->rtnh_ifindex = wn[i].nh->nh_ifp->if_index;
		rtnh->rtnh_hops = wn[i].weight;
		dump_rc_nhop_gw(nw, wn[i].nh);
		uint32_t rtflags = nhop_get_rtflags(wn[i].nh);
		if (rtflags != base_rtflags)
			nlattr_add_u32(nw, NL_RTA_RTFLAGS, rtflags);
		if (rtflags & RTF_FIXEDMTU)
			dump_rc_nhop_mtu(nw, wn[i].nh);
		rtnh = nlattr_restore_offset(nw, nh_off, struct rtnexthop);
		/*
		 * nlattr_add() allocates 4-byte aligned storage, no need to aligh
		 * length here
		 * */
		rtnh->rtnh_len = nlattr_save_offset(nw) - nh_off;
	}
	nlattr_set_len(nw, off);
}
#endif

static void
dump_rc_nhop(struct nl_writer *nw, const struct nhop_object *nh, struct rtmsg *rtm)
{
#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh)) {
		dump_rc_nhg(nw, (const struct nhgrp_object *)nh, rtm);
		return;
	}
#endif
	uint32_t rtflags = nhop_get_rtflags(nh);

	/*
	 * IPv4 over IPv6
	 *    ('RTA_VIA', {'family': 10, 'addr': 'fe80::20c:29ff:fe67:2dd'}), ('RTA_OIF', 2),
	 * IPv4 w/ gw
	 *    ('RTA_GATEWAY', '172.16.107.131'), ('RTA_OIF', 2)],
	 * Direct route:
	 *    ('RTA_OIF', 2)
	 */
	if (nh->nh_flags & NHF_GATEWAY)
		dump_rc_nhop_gw(nw, nh);

	uint32_t uidx = nhop_get_uidx(nh);
	if (uidx != 0)
		nlattr_add_u32(nw, NL_RTA_NH_ID, uidx);
	nlattr_add_u32(nw, NL_RTA_KNH_ID, nhop_get_idx(nh));
	nlattr_add_u32(nw, NL_RTA_RTFLAGS, rtflags);

	if (rtflags & RTF_FIXEDMTU)
		dump_rc_nhop_mtu(nw, nh);
	uint32_t nh_expire = nhop_get_expire(nh);
	if (nh_expire > 0)
		nlattr_add_u32(nw, NL_RTA_EXPIRES, nh_expire - time_uptime);

	/* In any case, fill outgoing interface */
	nlattr_add_u32(nw, NL_RTA_OIF, nh->nh_ifp->if_index);
}

/*
 * Dumps output from a rib command into an rtmsg
 */

static int
dump_px(uint32_t fibnum, const struct nlmsghdr *hdr,
    const struct rtentry *rt, struct route_nhop_data *rnd,
    struct nl_writer *nw)
{
	struct rtmsg *rtm;
	int error = 0;

	NET_EPOCH_ASSERT();

	if (!nlmsg_reply(nw, hdr, sizeof(struct rtmsg)))
		goto enomem;

	int family = rt_get_family(rt);
	int rtm_off = nlattr_save_offset(nw);
	rtm = nlmsg_reserve_object(nw, struct rtmsg);
	rtm->rtm_family = family;
	rtm->rtm_dst_len = 0;
	rtm->rtm_src_len = 0;
	rtm->rtm_tos = 0;
	if (fibnum < 255)
		rtm->rtm_table = (unsigned char)fibnum;
	rtm->rtm_scope = RT_SCOPE_UNIVERSE;
	if (!NH_IS_NHGRP(rnd->rnd_nhop)) {
		rtm->rtm_protocol = nl_get_rtm_protocol(rnd->rnd_nhop);
		rtm->rtm_type = get_rtm_type(rnd->rnd_nhop);
	} else {
		rtm->rtm_protocol = RTPROT_UNSPEC; /* TODO: protocol from nhg? */
		rtm->rtm_type = RTN_UNICAST;
	}

	nlattr_add_u32(nw, NL_RTA_TABLE, fibnum);

	int plen = 0;
	uint32_t scopeid = 0;
	switch (family) {
#ifdef INET
	case AF_INET:
		{
			struct in_addr addr;
			rt_get_inet_prefix_plen(rt, &addr, &plen, &scopeid);
			nlattr_add(nw, NL_RTA_DST, 4, &addr);
			break;
		}
#endif
#ifdef INET6
	case AF_INET6:
		{
			struct in6_addr addr;
			rt_get_inet6_prefix_plen(rt, &addr, &plen, &scopeid);
			nlattr_add(nw, NL_RTA_DST, 16, &addr);
			break;
		}
#endif
	default:
		FIB_LOG(LOG_NOTICE, fibnum, family, "unsupported rt family: %d", family);
		error = EAFNOSUPPORT;
		goto flush;
	}

	rtm = nlattr_restore_offset(nw, rtm_off, struct rtmsg);
	if (plen > 0)
		rtm->rtm_dst_len = plen;
	dump_rc_nhop(nw, rnd->rnd_nhop, rtm);

	if (nlmsg_end(nw))
		return (0);
enomem:
	error = ENOMEM;
flush:
	nlmsg_abort(nw);
	return (error);
}

static int
family_to_group(int family)
{
	switch (family) {
	case AF_INET:
		return (RTNLGRP_IPV4_ROUTE);
	case AF_INET6:
		return (RTNLGRP_IPV6_ROUTE);
	}
	return (0);
}


static void
report_operation(uint32_t fibnum, struct rib_cmd_info *rc,
    struct nlpcb *nlp, struct nlmsghdr *hdr)
{
	struct nl_writer nw;

	uint32_t group_id = family_to_group(rt_get_family(rc->rc_rt));
	if (nlmsg_get_group_writer(&nw, NLMSG_SMALL, NETLINK_ROUTE, group_id)) {
		struct route_nhop_data rnd = {
			.rnd_nhop = rc_get_nhop(rc),
			.rnd_weight = rc->rc_nh_weight,
		};
		hdr->nlmsg_flags &= ~(NLM_F_REPLACE | NLM_F_CREATE);
		hdr->nlmsg_flags &= ~(NLM_F_EXCL | NLM_F_APPEND);
		switch (rc->rc_cmd) {
		case RTM_ADD:
			hdr->nlmsg_type = NL_RTM_NEWROUTE;
			hdr->nlmsg_flags |= NLM_F_CREATE | NLM_F_EXCL;
			break;
		case RTM_CHANGE:
			hdr->nlmsg_type = NL_RTM_NEWROUTE;
			hdr->nlmsg_flags |= NLM_F_REPLACE;
			break;
		case RTM_DELETE:
			hdr->nlmsg_type = NL_RTM_DELROUTE;
			break;
		}
		dump_px(fibnum, hdr, rc->rc_rt, &rnd, &nw);
		nlmsg_flush(&nw);
	}

	rtsock_callback_p->route_f(fibnum, rc);
}

struct rta_mpath_nh {
	struct sockaddr	*gw;
	struct ifnet	*ifp;
	uint8_t		rtnh_flags;
	uint8_t		rtnh_weight;
};

#define	_IN(_field)	offsetof(struct rtnexthop, _field)
#define	_OUT(_field)	offsetof(struct rta_mpath_nh, _field)
const static struct nlattr_parser nla_p_rtnh[] = {
	{ .type = NL_RTA_GATEWAY, .off = _OUT(gw), .cb = nlattr_get_ip },
	{ .type = NL_RTA_VIA, .off = _OUT(gw), .cb = nlattr_get_ipvia },
};
const static struct nlfield_parser nlf_p_rtnh[] = {
	{ .off_in = _IN(rtnh_flags), .off_out = _OUT(rtnh_flags), .cb = nlf_get_u8 },
	{ .off_in = _IN(rtnh_hops), .off_out = _OUT(rtnh_weight), .cb = nlf_get_u8 },
	{ .off_in = _IN(rtnh_ifindex), .off_out = _OUT(ifp), .cb = nlf_get_ifpz },
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(mpath_parser, struct rtnexthop, nlf_p_rtnh, nla_p_rtnh);

struct rta_mpath {
	int num_nhops;
	struct rta_mpath_nh nhops[0];
};

static int
nlattr_get_multipath(struct nlattr *nla, struct nl_pstate *npt, const void *arg, void *target)
{
	int data_len = nla->nla_len - sizeof(struct nlattr);
	struct rtnexthop *rtnh;

	int max_nhops = data_len / sizeof(struct rtnexthop);

	struct rta_mpath *mp = npt_alloc(npt, (max_nhops + 2) * sizeof(struct rta_mpath_nh));
	mp->num_nhops = 0;

	for (rtnh = (struct rtnexthop *)(nla + 1); data_len > 0; ) {
		struct rta_mpath_nh *mpnh = &mp->nhops[mp->num_nhops++];

		int error = nl_parse_header(rtnh, rtnh->rtnh_len, &mpath_parser,
		    npt, mpnh);
		if (error != 0) {
			NLMSG_REPORT_ERR_MSG(npt, "RTA_MULTIPATH: nexhop %d: parse failed",
			    mp->num_nhops - 1);
			return (error);
		}

		int len = NL_ITEM_ALIGN(rtnh->rtnh_len);
		data_len -= len;
		rtnh = (struct rtnexthop *)((char *)rtnh + len);
	}
	if (data_len != 0 || mp->num_nhops == 0) {
		NLMSG_REPORT_ERR_MSG(npt, "invalid RTA_MULTIPATH attr");
		return (EINVAL);
	}

	*((struct rta_mpath **)target) = mp;
	return (0);
}


struct nl_parsed_route {
	struct sockaddr		*rta_dst;
	struct sockaddr		*rta_gw;
	struct ifnet		*rta_oif;
	struct rta_mpath	*rta_multipath;
	uint32_t		rta_table;
	uint32_t		rta_rtflags;
	uint32_t		rta_nh_id;
	uint32_t		rtax_mtu;
	uint8_t			rtm_family;
	uint8_t			rtm_dst_len;
};

#define	_IN(_field)	offsetof(struct rtmsg, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_route, _field)
static struct nlattr_parser nla_p_rtmetrics[] = {
	{ .type = NL_RTAX_MTU, .off = _OUT(rtax_mtu), .cb = nlattr_get_uint32 },
};
NL_DECLARE_ATTR_PARSER(metrics_parser, nla_p_rtmetrics);

static const struct nlattr_parser nla_p_rtmsg[] = {
	{ .type = NL_RTA_DST, .off = _OUT(rta_dst), .cb = nlattr_get_ip },
	{ .type = NL_RTA_OIF, .off = _OUT(rta_oif), .cb = nlattr_get_ifp },
	{ .type = NL_RTA_GATEWAY, .off = _OUT(rta_gw), .cb = nlattr_get_ip },
	{ .type = NL_RTA_METRICS, .arg = &metrics_parser, .cb = nlattr_get_nested },
	{ .type = NL_RTA_MULTIPATH, .off = _OUT(rta_multipath), .cb = nlattr_get_multipath },
	{ .type = NL_RTA_RTFLAGS, .off = _OUT(rta_rtflags), .cb = nlattr_get_uint32 },
	{ .type = NL_RTA_TABLE, .off = _OUT(rta_table), .cb = nlattr_get_uint32 },
	{ .type = NL_RTA_VIA, .off = _OUT(rta_gw), .cb = nlattr_get_ipvia },
	{ .type = NL_RTA_NH_ID, .off = _OUT(rta_nh_id), .cb = nlattr_get_uint32 },
};

static const struct nlfield_parser nlf_p_rtmsg[] = {
	{.off_in = _IN(rtm_family), .off_out = _OUT(rtm_family), .cb = nlf_get_u8 },
	{.off_in = _IN(rtm_dst_len), .off_out = _OUT(rtm_dst_len), .cb = nlf_get_u8 },
};
#undef _IN
#undef _OUT
NL_DECLARE_PARSER(rtm_parser, struct rtmsg, nlf_p_rtmsg, nla_p_rtmsg);

struct netlink_walkargs {
	struct nl_writer *nw;
	struct route_nhop_data rnd;
	struct nlmsghdr hdr;
	struct nlpcb *nlp;
	uint32_t fibnum;
	int family;
	int error;
	int count;
	int dumped;
	int dumped_tables;
};

static int
dump_rtentry(struct rtentry *rt, void *_arg)
{
	struct netlink_walkargs *wa = (struct netlink_walkargs *)_arg;
	int error;

	wa->count++;
	if (wa->error != 0)
		return (0);
	wa->dumped++;

	rt_get_rnd(rt, &wa->rnd);

	error = dump_px(wa->fibnum, &wa->hdr, rt, &wa->rnd, wa->nw);

	IF_DEBUG_LEVEL(LOG_DEBUG3) {
		char rtbuf[INET6_ADDRSTRLEN + 5];
		FIB_LOG(LOG_DEBUG3, wa->fibnum, wa->family,
		    "Dump %s, offset %u, error %d",
		    rt_print_buf(rt, rtbuf, sizeof(rtbuf)),
		    wa->nw->offset, error);
	}
	wa->error = error;

	return (0);
}

static void
dump_rtable_one(struct netlink_walkargs *wa, uint32_t fibnum, int family)
{
	FIB_LOG(LOG_DEBUG2, fibnum, family, "Start dump");
	wa->count = 0;
	wa->dumped = 0;

	rib_walk(fibnum, family, false, dump_rtentry, wa);

	wa->dumped_tables++;

	FIB_LOG(LOG_DEBUG2, fibnum, family, "End dump, iterated %d dumped %d",
	    wa->count, wa->dumped);
	NL_LOG(LOG_DEBUG2, "Current offset: %d", wa->nw->offset);
}

static int
dump_rtable_fib(struct netlink_walkargs *wa, uint32_t fibnum, int family)
{
	wa->fibnum = fibnum;

	if (family == AF_UNSPEC) {
		for (int i = 0; i < AF_MAX; i++) {
			if (rt_tables_get_rnh(fibnum, i) != 0) {
				wa->family = i;
				dump_rtable_one(wa, fibnum, i);
				if (wa->error != 0)
					break;
			}
		}
	} else {
		if (rt_tables_get_rnh(fibnum, family) != 0) {
			wa->family = family;
			dump_rtable_one(wa, fibnum, family);
		}
	}

	return (wa->error);
}

static int
handle_rtm_getroute(struct nlpcb *nlp, struct nl_parsed_route *attrs,
    struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rnh;
	struct rtentry *rt;
	uint32_t fibnum = attrs->rta_table;
	sa_family_t family = attrs->rtm_family;

	if (attrs->rta_dst == NULL) {
		NLMSG_REPORT_ERR_MSG(npt, "No RTA_DST supplied");
			return (EINVAL);
	}

	FIB_LOG(LOG_DEBUG, fibnum, family, "getroute called");

	rnh = rt_tables_get_rnh(fibnum, family);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	RIB_RLOCK(rnh);

	rt = (struct rtentry *)rnh->rnh_matchaddr(attrs->rta_dst, &rnh->head);
	if (rt == NULL) {
		RIB_RUNLOCK(rnh);
		return (ESRCH);
	}

	struct route_nhop_data rnd;
	rt_get_rnd(rt, &rnd);
	rnd.rnd_nhop = nhop_select_func(rnd.rnd_nhop, 0);

	RIB_RUNLOCK(rnh);

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		char rtbuf[NHOP_PRINT_BUFSIZE] __unused, nhbuf[NHOP_PRINT_BUFSIZE] __unused;
		FIB_LOG(LOG_DEBUG2, fibnum, family, "getroute completed: got %s for %s",
		    nhop_print_buf_any(rnd.rnd_nhop, nhbuf, sizeof(nhbuf)),
		    rt_print_buf(rt, rtbuf, sizeof(rtbuf)));
	}

	hdr->nlmsg_type = NL_RTM_NEWROUTE;
	dump_px(fibnum, hdr, rt, &rnd, npt->nw);

	return (0);
}

static int
handle_rtm_dump(struct nlpcb *nlp, uint32_t fibnum, int family,
    struct nlmsghdr *hdr, struct nl_writer *nw)
{
	struct netlink_walkargs wa = {
		.nlp = nlp,
		.nw = nw,
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_type = NL_RTM_NEWROUTE,
		.hdr.nlmsg_flags = hdr->nlmsg_flags | NLM_F_MULTI,
	};

	if (fibnum == RT_TABLE_UNSPEC) {
		for (int i = 0; i < V_rt_numfibs; i++) {
			dump_rtable_fib(&wa, fibnum, family);
			if (wa.error != 0)
				break;
		}
	} else
		dump_rtable_fib(&wa, fibnum, family);

	if (wa.error == 0 && wa.dumped_tables == 0) {
		FIB_LOG(LOG_DEBUG, fibnum, family, "incorrect fibnum/family");
		wa.error = ESRCH;
		// How do we propagate it?
	}

	if (!nlmsg_end_dump(wa.nw, wa.error, &wa.hdr)) {
                NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
                return (ENOMEM);
        }

	return (wa.error);
}

static struct nhop_object *
finalize_nhop(struct nhop_object *nh, int *perror)
{
	/*
	 * The following MUST be filled:
	 *  nh_ifp, nh_ifa, nh_gw
	 */
	if (nh->gw_sa.sa_family == 0) {
		/*
		 * Empty gateway. Can be direct route with RTA_OIF set.
		 */
		if (nh->nh_ifp != NULL)
			nhop_set_direct_gw(nh, nh->nh_ifp);
		else {
			NL_LOG(LOG_DEBUG, "empty gateway and interface, skipping");
			*perror = EINVAL;
			return (NULL);
		}
		/* Both nh_ifp and gateway are set */
	} else {
		/* Gateway is set up, we can derive ifp if not set */
		if (nh->nh_ifp == NULL) {
			struct ifaddr *ifa = ifa_ifwithnet(&nh->gw_sa, 1, nhop_get_fibnum(nh));
			if (ifa == NULL) {
				NL_LOG(LOG_DEBUG, "Unable to determine ifp, skipping");
				*perror = EINVAL;
				return (NULL);
			}
			nhop_set_transmit_ifp(nh, ifa->ifa_ifp);
		}
	}
	/* Both nh_ifp and gateway are set */
	if (nh->nh_ifa == NULL) {
		struct ifaddr *ifa = ifaof_ifpforaddr(&nh->gw_sa, nh->nh_ifp);
		if (ifa == NULL) {
			NL_LOG(LOG_DEBUG, "Unable to determine ifa, skipping");
			*perror = EINVAL;
			return (NULL);
		}
		nhop_set_src(nh, ifa);
	}

	return (nhop_get_nhop(nh, perror));
}

static int
get_pxflag(const struct nl_parsed_route *attrs)
{
	int pxflag = 0;
	switch (attrs->rtm_family) {
	case AF_INET:
		if (attrs->rtm_dst_len == 32)
			pxflag = NHF_HOST;
		else if (attrs->rtm_dst_len == 0)
			pxflag = NHF_DEFAULT;
		break;
	case AF_INET6:
		if (attrs->rtm_dst_len == 32)
			pxflag = NHF_HOST;
		else if (attrs->rtm_dst_len == 0)
			pxflag = NHF_DEFAULT;
		break;
	}

	return (pxflag);
}

static int
get_op_flags(int nlm_flags)
{
	int op_flags = 0;

	op_flags |= (nlm_flags & NLM_F_REPLACE) ? RTM_F_REPLACE : 0;
	op_flags |= (nlm_flags & NLM_F_EXCL) ? RTM_F_EXCL : 0;
	op_flags |= (nlm_flags & NLM_F_CREATE) ? RTM_F_CREATE : 0;
	op_flags |= (nlm_flags & NLM_F_APPEND) ? RTM_F_APPEND : 0;

	return (op_flags);
}

#ifdef ROUTE_MPATH
static int
create_nexthop_one(struct nl_parsed_route *attrs, struct rta_mpath_nh *mpnh,
    struct nl_pstate *npt, struct nhop_object **pnh)
{
	int error;

	if (mpnh->gw == NULL)
		return (EINVAL);

	struct nhop_object *nh = nhop_alloc(attrs->rta_table, attrs->rtm_family);
	if (nh == NULL)
		return (ENOMEM);

	nhop_set_gw(nh, mpnh->gw, true);
	if (mpnh->ifp != NULL)
		nhop_set_transmit_ifp(nh, mpnh->ifp);
	nhop_set_rtflags(nh, attrs->rta_rtflags);

	*pnh = finalize_nhop(nh, &error);

	return (error);
}
#endif

static struct nhop_object *
create_nexthop_from_attrs(struct nl_parsed_route *attrs,
    struct nl_pstate *npt, int *perror)
{
	struct nhop_object *nh = NULL;
	int error = 0;

	if (attrs->rta_multipath != NULL) {
#ifdef ROUTE_MPATH
		/* Multipath w/o explicit nexthops */
		int num_nhops = attrs->rta_multipath->num_nhops;
		struct weightened_nhop *wn = npt_alloc(npt, sizeof(*wn) * num_nhops);

		for (int i = 0; i < num_nhops; i++) {
			struct rta_mpath_nh *mpnh = &attrs->rta_multipath->nhops[i];

			error = create_nexthop_one(attrs, mpnh, npt, &wn[i].nh);
			if (error != 0) {
				for (int j = 0; j < i; j++)
					nhop_free(wn[j].nh);
				break;
			}
			wn[i].weight = mpnh->rtnh_weight > 0 ? mpnh->rtnh_weight : 1;
		}
		if (error == 0) {
			struct rib_head *rh = nhop_get_rh(wn[0].nh);

			error = nhgrp_get_group(rh, wn, num_nhops, 0,
			    (struct nhgrp_object **)&nh);

			for (int i = 0; i < num_nhops; i++)
				nhop_free(wn[i].nh);
		}
#else
		error = ENOTSUP;
#endif
		*perror = error;
	} else {
		nh = nhop_alloc(attrs->rta_table, attrs->rtm_family);
		if (nh == NULL) {
			*perror = ENOMEM;
			return (NULL);
		}
		if (attrs->rta_gw != NULL)
			nhop_set_gw(nh, attrs->rta_gw, true);
		if (attrs->rta_oif != NULL)
			nhop_set_transmit_ifp(nh, attrs->rta_oif);
		if (attrs->rtax_mtu != 0)
			nhop_set_mtu(nh, attrs->rtax_mtu, true);
		if (attrs->rta_rtflags & RTF_BROADCAST)
			nhop_set_broadcast(nh, true);
		if (attrs->rta_rtflags & RTF_BLACKHOLE)
			nhop_set_blackhole(nh, NHF_BLACKHOLE);
		if (attrs->rta_rtflags & RTF_REJECT)
			nhop_set_blackhole(nh, NHF_REJECT);
		nhop_set_rtflags(nh, attrs->rta_rtflags);
		nh = finalize_nhop(nh, perror);
	}

	return (nh);
}

static int
rtnl_handle_newroute(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_pstate *npt)
{
	struct rib_cmd_info rc = {};
	struct nhop_object *nh = NULL;
	int error;

	struct nl_parsed_route attrs = {};
	error = nl_parse_nlmsg(hdr, &rtm_parser, npt, &attrs);
	if (error != 0)
		return (error);

	/* Check if we have enough data */
	if (attrs.rta_dst == NULL) {
		NL_LOG(LOG_DEBUG, "missing RTA_DST");
		return (EINVAL);
	}

	if (attrs.rta_nh_id != 0) {
		/* Referenced uindex */
		int pxflag = get_pxflag(&attrs);
		nh = nl_find_nhop(attrs.rta_table, attrs.rtm_family, attrs.rta_nh_id,
		    pxflag, &error);
		if (error != 0)
			return (error);
	} else {
		nh = create_nexthop_from_attrs(&attrs, npt, &error);
		if (error != 0) {
			NL_LOG(LOG_DEBUG, "Error creating nexthop");
			return (error);
		}
	}

	int weight = NH_IS_NHGRP(nh) ? 0 : RT_DEFAULT_WEIGHT;
	struct route_nhop_data rnd = { .rnd_nhop = nh, .rnd_weight = weight };
	int op_flags = get_op_flags(hdr->nlmsg_flags);

	error = rib_add_route_px(attrs.rta_table, attrs.rta_dst, attrs.rtm_dst_len,
	    &rnd, op_flags, &rc);
	if (error == 0)
		report_operation(attrs.rta_table, &rc, nlp, hdr);
	return (error);
}

static int
path_match_func(const struct rtentry *rt, const struct nhop_object *nh, void *_data)
{
	struct nl_parsed_route *attrs = (struct nl_parsed_route *)_data;

	if ((attrs->rta_gw != NULL) && !rib_match_gw(rt, nh, attrs->rta_gw))
		return (0);

	if ((attrs->rta_oif != NULL) && (attrs->rta_oif != nh->nh_ifp))
		return (0);

	return (1);
}

static int
rtnl_handle_delroute(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_pstate *npt)
{
	struct rib_cmd_info rc;
	int error;

	struct nl_parsed_route attrs = {};
	error = nl_parse_nlmsg(hdr, &rtm_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.rta_dst == NULL) {
		NLMSG_REPORT_ERR_MSG(npt, "RTA_DST is not set");
		return (ESRCH);
	}

	error = rib_del_route_px(attrs.rta_table, attrs.rta_dst,
	    attrs.rtm_dst_len, path_match_func, &attrs, 0, &rc);
	if (error == 0)
		report_operation(attrs.rta_table, &rc, nlp, hdr);
	return (error);
}

static int
rtnl_handle_getroute(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	int error;

	struct nl_parsed_route attrs = {};
	error = nl_parse_nlmsg(hdr, &rtm_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (hdr->nlmsg_flags & NLM_F_DUMP)
		error = handle_rtm_dump(nlp, attrs.rta_table, attrs.rtm_family, hdr, npt->nw);
	else
		error = handle_rtm_getroute(nlp, &attrs, hdr, npt);

	return (error);
}

void
rtnl_handle_route_event(uint32_t fibnum, const struct rib_cmd_info *rc)
{
	int family, nlm_flags = 0;

	struct nl_writer nw;

	family = rt_get_family(rc->rc_rt);

	/* XXX: check if there are active listeners first */

	/* TODO: consider passing PID/type/seq */
	switch (rc->rc_cmd) {
	case RTM_ADD:
		nlm_flags = NLM_F_EXCL | NLM_F_CREATE;
		break;
	case RTM_CHANGE:
		nlm_flags = NLM_F_REPLACE;
		break;
	case RTM_DELETE:
		nlm_flags = 0;
		break;
	}
	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		char rtbuf[NHOP_PRINT_BUFSIZE] __unused;
		FIB_LOG(LOG_DEBUG2, fibnum, family,
		    "received event %s for %s / nlm_flags=%X",
		    rib_print_cmd(rc->rc_cmd),
		    rt_print_buf(rc->rc_rt, rtbuf, sizeof(rtbuf)),
		    nlm_flags);
	}

	struct nlmsghdr hdr = {
		.nlmsg_flags = nlm_flags,
		.nlmsg_type = get_rtmsg_type_from_rtsock(rc->rc_cmd),
	};

	struct route_nhop_data rnd = {
		.rnd_nhop = rc_get_nhop(rc),
		.rnd_weight = rc->rc_nh_weight,
	};

	uint32_t group_id = family_to_group(family);
	if (!nlmsg_get_group_writer(&nw, NLMSG_SMALL, NETLINK_ROUTE, group_id)) {
		NL_LOG(LOG_DEBUG, "error allocating event buffer");
		return;
	}

	dump_px(fibnum, &hdr, rc->rc_rt, &rnd, &nw);
	nlmsg_flush(&nw);
}

static const struct rtnl_cmd_handler cmd_handlers[] = {
	{
		.cmd = NL_RTM_GETROUTE,
		.name = "RTM_GETROUTE",
		.cb = &rtnl_handle_getroute,
	},
	{
		.cmd = NL_RTM_DELROUTE,
		.name = "RTM_DELROUTE",
		.cb = &rtnl_handle_delroute,
		.priv = PRIV_NET_ROUTE,
	},
	{
		.cmd = NL_RTM_NEWROUTE,
		.name = "RTM_NEWROUTE",
		.cb = &rtnl_handle_newroute,
		.priv = PRIV_NET_ROUTE,
	}
};

static const struct nlhdr_parser *all_parsers[] = {&mpath_parser, &metrics_parser, &rtm_parser};

void
rtnl_routes_init()
{
	NL_VERIFY_PARSERS(all_parsers);
	rtnl_register_messages(cmd_handlers, NL_ARRAY_LEN(cmd_handlers));
}
