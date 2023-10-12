/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
#include "opt_inet.h"
#include "opt_inet6.h"
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_llatbl.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#include <netinet6/in6_var.h>		/* nd6.h requires this */
#include <netinet6/nd6.h>		/* nd6 state machine */
#include <netinet6/scope6_var.h>	/* scope deembedding */

#define	DEBUG_MOD_NAME	nl_neigh
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

static int lle_families[] = { AF_INET, AF_INET6 };

static eventhandler_tag lle_event_p;

struct netlink_walkargs {
	struct nl_writer *nw;
	struct nlmsghdr hdr;
	struct nlpcb *so;
	if_t ifp;
	int family;
	int error;
	int count;
	int dumped;
};

static int
lle_state_to_nl_state(int family, struct llentry *lle)
{
	int state = lle->ln_state;

	switch (family) {
	case AF_INET:
		if (lle->la_flags & (LLE_STATIC | LLE_IFADDR))
			state = 1;
		switch (state) {
		case 0: /* ARP_LLINFO_INCOMPLETE */
			return (NUD_INCOMPLETE);
		case 1: /* ARP_LLINFO_REACHABLE  */
			return (NUD_REACHABLE);
		case 2: /* ARP_LLINFO_VERIFY */
			return (NUD_PROBE);
		}
		break;
	case AF_INET6:
		switch (state) {
		case ND6_LLINFO_INCOMPLETE:
			return (NUD_INCOMPLETE);
		case ND6_LLINFO_REACHABLE:
			return (NUD_REACHABLE);
		case ND6_LLINFO_STALE:
			return (NUD_STALE);
		case ND6_LLINFO_DELAY:
			return (NUD_DELAY);
		case ND6_LLINFO_PROBE:
			return (NUD_PROBE);
		}
		break;
	}

	return (NUD_NONE);
}

static uint32_t
lle_flags_to_nl_flags(const struct llentry *lle)
{
	uint32_t nl_flags = 0;

	if (lle->la_flags & LLE_IFADDR)
		nl_flags |= NTF_SELF;
	if (lle->la_flags & LLE_PUB)
		nl_flags |= NTF_PROXY;
	if (lle->la_flags & LLE_STATIC)
		nl_flags |= NTF_STICKY;
	if (lle->ln_router != 0)
		nl_flags |= NTF_ROUTER;

	return (nl_flags);
}

static uint32_t
get_lle_next_ts(const struct llentry *lle)
{
	if (lle->la_expire == 0)
		return (0);
	return (lle->la_expire + lle->lle_remtime / hz + time_second - time_uptime);
}

static int
dump_lle_locked(struct llentry *lle, void *arg)
{
	struct netlink_walkargs *wa = (struct netlink_walkargs *)arg;
	struct nlmsghdr *hdr = &wa->hdr;
	struct nl_writer *nw = wa->nw;
	struct ndmsg *ndm;
#if defined(INET) || defined(INET6)
	union {
		struct in_addr	in;
		struct in6_addr	in6;
	} addr;
#endif

	IF_DEBUG_LEVEL(LOG_DEBUG2) {
		char llebuf[NHOP_PRINT_BUFSIZE];
		llentry_print_buf_lltable(lle, llebuf, sizeof(llebuf));
		NL_LOG(LOG_DEBUG2, "dumping %s", llebuf);
	}

	if (!nlmsg_reply(nw, hdr, sizeof(struct ndmsg)))
		goto enomem;

	ndm = nlmsg_reserve_object(nw, struct ndmsg);
	ndm->ndm_family = wa->family;
	ndm->ndm_ifindex = if_getindex(wa->ifp);
	ndm->ndm_state = lle_state_to_nl_state(wa->family, lle);
	ndm->ndm_flags = lle_flags_to_nl_flags(lle);

	switch (wa->family) {
#ifdef INET
	case AF_INET:
		addr.in = lle->r_l3addr.addr4;
		nlattr_add(nw, NDA_DST, 4, &addr);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		addr.in6 = lle->r_l3addr.addr6;
		in6_clearscope(&addr.in6);
		nlattr_add(nw, NDA_DST, 16, &addr);
		break;
#endif
	}

	if (lle->r_flags & RLLE_VALID) {
		/* Has L2 */
		int addrlen = if_getaddrlen(wa->ifp);
		nlattr_add(nw, NDA_LLADDR, addrlen, lle->ll_addr);
	}

	nlattr_add_u32(nw, NDA_PROBES, lle->la_asked);

	struct nda_cacheinfo *cache;
	cache = nlmsg_reserve_attr(nw, NDA_CACHEINFO, struct nda_cacheinfo);
	if (cache == NULL)
		goto enomem;
	/* TODO: provide confirmed/updated */
	cache->ndm_refcnt = lle->lle_refcnt;

	int off = nlattr_add_nested(nw, NDA_FREEBSD);
	if (off != 0) {
		nlattr_add_u32(nw, NDAF_NEXT_STATE_TS, get_lle_next_ts(lle));

		nlattr_set_len(nw, off);
	}

        if (nlmsg_end(nw))
		return (0);
enomem:
        NL_LOG(LOG_DEBUG, "unable to dump lle state (ENOMEM)");
        nlmsg_abort(nw);
        return (ENOMEM);
}

static int
dump_lle(struct lltable *llt, struct llentry *lle, void *arg)
{
	int error;

	LLE_RLOCK(lle);
	error = dump_lle_locked(lle, arg);
	LLE_RUNLOCK(lle);
	return (error);
}

static bool
dump_llt(struct lltable *llt, struct netlink_walkargs *wa)
{
	lltable_foreach_lle(llt, dump_lle, wa);

	return (true);
}

static int
dump_llts_iface(struct netlink_walkargs *wa, if_t ifp, int family)
{
	int error = 0;

	wa->ifp = ifp;
	for (int i = 0; i < sizeof(lle_families) / sizeof(int); i++) {
		int fam = lle_families[i];
		struct lltable *llt = lltable_get(ifp, fam);
		if (llt != NULL && (family == 0 || family == fam)) {
			wa->count++;
			wa->family = fam;
			if (!dump_llt(llt, wa)) {
				error = ENOMEM;
				break;
			}
			wa->dumped++;
		}
	}
	return (error);
}

static int
dump_llts(struct netlink_walkargs *wa, if_t ifp, int family)
{
	NL_LOG(LOG_DEBUG2, "Start dump ifp=%s family=%d", ifp ? if_name(ifp) : "NULL", family);

	wa->hdr.nlmsg_flags |= NLM_F_MULTI;

	if (ifp != NULL) {
		dump_llts_iface(wa, ifp, family);
	} else {
		struct if_iter it;

		for (ifp = if_iter_start(&it); ifp != NULL; ifp = if_iter_next(&it)) {
			dump_llts_iface(wa, ifp, family);
		}
		if_iter_finish(&it);
	}

	NL_LOG(LOG_DEBUG2, "End dump, iterated %d dumped %d", wa->count, wa->dumped);

	if (!nlmsg_end_dump(wa->nw, wa->error, &wa->hdr)) {
                NL_LOG(LOG_DEBUG, "Unable to add new message");
                return (ENOMEM);
        }

	return (0);
}

static int
get_lle(struct netlink_walkargs *wa, if_t ifp, int family, struct sockaddr *dst)
{
	struct lltable *llt = lltable_get(ifp, family);
	if (llt == NULL)
		return (ESRCH);

	struct llentry *lle = lla_lookup(llt, LLE_UNLOCKED, dst);
	if (lle == NULL)
		return (ESRCH);

	wa->ifp = ifp;
	wa->family = family;

	return (dump_lle(llt, lle, wa));
}

static void
set_scope6(struct sockaddr *sa, if_t ifp)
{
#ifdef INET6
	if (sa != NULL && sa->sa_family == AF_INET6 && ifp != NULL) {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;

		if (IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr))
			in6_set_unicast_scopeid(&sa6->sin6_addr, if_getindex(ifp));
	}
#endif
}

struct nl_parsed_neigh {
	struct sockaddr	*nda_dst;
	struct ifnet	*nda_ifp;
	struct nlattr	*nda_lladdr;
	uint32_t	ndaf_next_ts;
	uint32_t	ndm_flags;
	uint16_t	ndm_state;
	uint8_t		ndm_family;
};

#define	_IN(_field)	offsetof(struct ndmsg, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_neigh, _field)
static const struct nlattr_parser nla_p_neigh_fbsd[] = {
	{ .type = NDAF_NEXT_STATE_TS, .off = _OUT(ndaf_next_ts), .cb = nlattr_get_uint32 },
};
NL_DECLARE_ATTR_PARSER(neigh_fbsd_parser, nla_p_neigh_fbsd);

static const struct nlfield_parser nlf_p_neigh[] = {
	{ .off_in = _IN(ndm_family), .off_out = _OUT(ndm_family), .cb = nlf_get_u8 },
	{ .off_in = _IN(ndm_flags), .off_out = _OUT(ndm_flags), .cb = nlf_get_u8_u32 },
	{ .off_in = _IN(ndm_state), .off_out = _OUT(ndm_state), .cb = nlf_get_u16 },
	{ .off_in = _IN(ndm_ifindex), .off_out = _OUT(nda_ifp), .cb = nlf_get_ifpz },
};

static const struct nlattr_parser nla_p_neigh[] = {
	{ .type = NDA_DST, .off = _OUT(nda_dst), .cb = nlattr_get_ip },
	{ .type = NDA_LLADDR, .off = _OUT(nda_lladdr), .cb = nlattr_get_nla },
	{ .type = NDA_IFINDEX, .off = _OUT(nda_ifp), .cb = nlattr_get_ifp },
	{ .type = NDA_FLAGS_EXT, .off = _OUT(ndm_flags), .cb = nlattr_get_uint32 },
	{ .type = NDA_FREEBSD, .arg = &neigh_fbsd_parser, .cb = nlattr_get_nested },
};
#undef _IN
#undef _OUT

static bool
post_p_neigh(void *_attrs, struct nl_pstate *npt __unused)
{
	struct nl_parsed_neigh *attrs = (struct nl_parsed_neigh *)_attrs;

	set_scope6(attrs->nda_dst, attrs->nda_ifp);
	return (true);
}
NL_DECLARE_PARSER_EXT(ndmsg_parser, struct ndmsg, NULL, nlf_p_neigh, nla_p_neigh, post_p_neigh);


/*
 * type=RTM_NEWNEIGH, flags=NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL|NLM_F_CREATE, seq=1661941473, pid=0},
 * {ndm_family=AF_INET6, ndm_ifindex=if_nametoindex("enp0s31f6"), ndm_state=NUD_PERMANENT, ndm_flags=0, ndm_type=RTN_UNSPEC},
 * [
 *  {{nla_len=20, nla_type=NDA_DST}, inet_pton(AF_INET6, "2a01:4f8:13a:70c::3")},
 *  {{nla_len=10, nla_type=NDA_LLADDR}, 20:4e:71:62:ae:f2}]}, iov_len=60}
 */

static int
rtnl_handle_newneigh(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	int error;

	struct nl_parsed_neigh attrs = {};
	error = nl_parse_nlmsg(hdr, &ndmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.nda_ifp == NULL || attrs.nda_dst == NULL || attrs.nda_lladdr == NULL) {
		if (attrs.nda_ifp == NULL)
			NLMSG_REPORT_ERR_MSG(npt, "NDA_IFINDEX / ndm_ifindex not set");
		if (attrs.nda_dst == NULL)
			NLMSG_REPORT_ERR_MSG(npt, "NDA_DST not set");
		if (attrs.nda_lladdr == NULL)
			NLMSG_REPORT_ERR_MSG(npt, "NDA_LLADDR not set");
		return (EINVAL);
	}

	if (attrs.nda_dst->sa_family != attrs.ndm_family) {
		NLMSG_REPORT_ERR_MSG(npt,
		    "NDA_DST family (%d) is different from ndm_family (%d)",
		    attrs.nda_dst->sa_family, attrs.ndm_family);
		return (EINVAL);
	}

	int addrlen = if_getaddrlen(attrs.nda_ifp);
	if (attrs.nda_lladdr->nla_len != sizeof(struct nlattr) + addrlen) {
		NLMSG_REPORT_ERR_MSG(npt,
		    "NDA_LLADDR address length (%d) is different from expected (%d)",
		    (int)attrs.nda_lladdr->nla_len - (int)sizeof(struct nlattr), addrlen);
		return (EINVAL);
	}

	const uint16_t supported_flags = NTF_PROXY | NTF_STICKY;
	if ((attrs.ndm_flags & supported_flags) != attrs.ndm_flags) {
		NLMSG_REPORT_ERR_MSG(npt, "ndm_flags %X not supported",
		    attrs.ndm_flags &~ supported_flags);
		return (ENOTSUP);
	}

	/* Replacement requires new entry creation anyway */
	if ((hdr->nlmsg_flags & (NLM_F_CREATE | NLM_F_REPLACE)) == 0)
		return (ENOTSUP);

	struct lltable *llt = lltable_get(attrs.nda_ifp, attrs.ndm_family);
	if (llt == NULL)
		return (EAFNOSUPPORT);


	uint8_t linkhdr[LLE_MAX_LINKHDR];
	size_t linkhdrsize = sizeof(linkhdr);
	int lladdr_off = 0;
	if (lltable_calc_llheader(attrs.nda_ifp, attrs.ndm_family,
	    (char *)(attrs.nda_lladdr + 1), linkhdr, &linkhdrsize, &lladdr_off) != 0) {
		NLMSG_REPORT_ERR_MSG(npt, "unable to calculate lle prepend data");
		return (EINVAL);
	}

	int lle_flags = (attrs.ndm_flags & NTF_PROXY) ? LLE_PUB : 0;
	if (attrs.ndm_flags & NTF_STICKY)
		lle_flags |= LLE_STATIC;
	struct llentry *lle = lltable_alloc_entry(llt, lle_flags, attrs.nda_dst);
	if (lle == NULL)
		return (ENOMEM);
	lltable_set_entry_addr(attrs.nda_ifp, lle, linkhdr, linkhdrsize, lladdr_off);

	if (attrs.ndm_flags & NTF_STICKY)
		lle->la_expire = 0;
	else
		lle->la_expire = attrs.ndaf_next_ts - time_second + time_uptime;

	/* llentry created, try to insert or update */
	IF_AFDATA_WLOCK(attrs.nda_ifp);
	LLE_WLOCK(lle);
	struct llentry *lle_tmp = lla_lookup(llt, LLE_EXCLUSIVE, attrs.nda_dst);
	if (lle_tmp != NULL) {
		error = EEXIST;
		if (hdr->nlmsg_flags & NLM_F_EXCL) {
			LLE_WUNLOCK(lle_tmp);
			lle_tmp = NULL;
		} else if (hdr->nlmsg_flags & NLM_F_REPLACE) {
			if ((lle_tmp->la_flags & LLE_IFADDR) == 0) {
				lltable_unlink_entry(llt, lle_tmp);
				lltable_link_entry(llt, lle);
				error = 0;
			} else
				error = EPERM;
		}
	} else {
		if (hdr->nlmsg_flags & NLM_F_CREATE)
			lltable_link_entry(llt, lle);
		else
			error = ENOENT;
	}
	IF_AFDATA_WUNLOCK(attrs.nda_ifp);

	if (error != 0) {
		if (lle != NULL)
			llentry_free(lle);
		return (error);
	}

	if (lle_tmp != NULL)
		llentry_free(lle_tmp);

	/* XXX: We're inside epoch */
	EVENTHANDLER_INVOKE(lle_event, lle, LLENTRY_RESOLVED);
	LLE_WUNLOCK(lle);
	llt->llt_post_resolved(llt, lle);

	return (0);
}

static int
rtnl_handle_delneigh(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	int error;

	struct nl_parsed_neigh attrs = {};
	error = nl_parse_nlmsg(hdr, &ndmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.nda_dst == NULL) {
		NLMSG_REPORT_ERR_MSG(npt, "NDA_DST not set");
		return (EINVAL);
	}

	if (attrs.nda_ifp == NULL) {
		NLMSG_REPORT_ERR_MSG(npt, "no ifindex provided");
		return (EINVAL);
	}

	struct lltable *llt = lltable_get(attrs.nda_ifp, attrs.ndm_family);
	if (llt == NULL)
		return (EAFNOSUPPORT);

	return (lltable_delete_addr(llt, 0, attrs.nda_dst));
}

static int
rtnl_handle_getneigh(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	int error;

	struct nl_parsed_neigh attrs = {};
	error = nl_parse_nlmsg(hdr, &ndmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.nda_dst != NULL && attrs.nda_ifp == NULL) {
		NLMSG_REPORT_ERR_MSG(npt, "has NDA_DST but no ifindex provided");
		return (EINVAL);
	}

	struct netlink_walkargs wa = {
		.so = nlp,
		.nw = npt->nw,
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_flags = hdr->nlmsg_flags,
		.hdr.nlmsg_type = NL_RTM_NEWNEIGH,
	};

	if (attrs.nda_dst == NULL)
		error = dump_llts(&wa, attrs.nda_ifp, attrs.ndm_family);
	else
		error = get_lle(&wa, attrs.nda_ifp, attrs.ndm_family, attrs.nda_dst);

	return (error);
}

static const struct rtnl_cmd_handler cmd_handlers[] = {
	{
		.cmd = NL_RTM_NEWNEIGH,
		.name = "RTM_NEWNEIGH",
		.cb = &rtnl_handle_newneigh,
		.priv = PRIV_NET_ROUTE,
	},
	{
		.cmd = NL_RTM_DELNEIGH,
		.name = "RTM_DELNEIGH",
		.cb = &rtnl_handle_delneigh,
		.priv = PRIV_NET_ROUTE,
	},
	{
		.cmd = NL_RTM_GETNEIGH,
		.name = "RTM_GETNEIGH",
		.cb = &rtnl_handle_getneigh,
	}
};

static void
rtnl_lle_event(void *arg __unused, struct llentry *lle, int evt)
{
	if_t ifp;
	int family;

	LLE_WLOCK_ASSERT(lle);

	ifp = lltable_get_ifp(lle->lle_tbl);
	family = lltable_get_af(lle->lle_tbl);

	if (family != AF_INET && family != AF_INET6)
		return;

	int nlmsgs_type = evt == LLENTRY_RESOLVED ? NL_RTM_NEWNEIGH : NL_RTM_DELNEIGH;

	struct nl_writer nw = {};
	if (!nlmsg_get_group_writer(&nw, NLMSG_SMALL, NETLINK_ROUTE, RTNLGRP_NEIGH)) {
		NL_LOG(LOG_DEBUG, "error allocating group writer");
		return;
	}

	struct netlink_walkargs wa = {
		.hdr.nlmsg_type = nlmsgs_type,
		.nw = &nw,
		.ifp = ifp,
		.family = family,
	};

	dump_lle_locked(lle, &wa);
	nlmsg_flush(&nw);
}

static const struct nlhdr_parser *all_parsers[] = { &ndmsg_parser, &neigh_fbsd_parser };

void
rtnl_neighs_init(void)
{
	NL_VERIFY_PARSERS(all_parsers);
	rtnl_register_messages(cmd_handlers, NL_ARRAY_LEN(cmd_handlers));
	lle_event_p = EVENTHANDLER_REGISTER(lle_event, rtnl_lle_event, NULL,
	    EVENTHANDLER_PRI_ANY);
}

void
rtnl_neighs_destroy(void)
{
	EVENTHANDLER_DEREGISTER(lle_event, lle_event_p);
}
