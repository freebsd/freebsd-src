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

#include "opt_netlink.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_inet6.h"
#include <sys/types.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet6/scope6_var.h> /* scope deembedding */
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#define	DEBUG_MOD_NAME	nl_iface
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

struct netlink_walkargs {
	struct nl_writer *nw;
	struct nlmsghdr hdr;
	struct nlpcb *so;
	struct ucred *cred;
	uint32_t fibnum;
	int family;
	int error;
	int count;
	int dumped;
};

static eventhandler_tag ifdetach_event, ifattach_event, iflink_event, ifaddr_event;

static SLIST_HEAD(, nl_cloner) nl_cloners = SLIST_HEAD_INITIALIZER(nl_cloners);

static struct sx rtnl_cloner_lock;
SX_SYSINIT(rtnl_cloner_lock, &rtnl_cloner_lock, "rtnl cloner lock");

/* These are external hooks for CARP. */
extern int	(*carp_get_vhid_p)(struct ifaddr *);

/*
 * RTM_GETLINK request
 * sendto(3, {{len=32, type=RTM_GETLINK, flags=NLM_F_REQUEST|NLM_F_DUMP, seq=1641940952, pid=0},
 *  {ifi_family=AF_INET, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0}}, 32, 0, NULL, 0) = 32
 *
 * Reply:
 * {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_ETHER, ifi_index=if_nametoindex("enp0s31f6"), ifi_flags=IFF_UP|IFF_BROADCAST|IFF_RUNNING|IFF_MULTICAST|IFF_LOWER_UP, ifi_change=0},
{{nla_len=10, nla_type=IFLA_ADDRESS}, "\xfe\x54\x00\x52\x3e\x90"}

[
{{nla_len=14, nla_type=IFLA_IFNAME}, "enp0s31f6"},
{{nla_len=8, nla_type=IFLA_TXQLEN}, 1000},
{{nla_len=5, nla_type=IFLA_OPERSTATE}, 6},
{{nla_len=5, nla_type=IFLA_LINKMODE}, 0},
{{nla_len=8, nla_type=IFLA_MTU}, 1500},
{{nla_len=8, nla_type=IFLA_MIN_MTU}, 68},
 {{nla_len=8, nla_type=IFLA_MAX_MTU}, 9000},
{{nla_len=8, nla_type=IFLA_GROUP}, 0},
{{nla_len=8, nla_type=IFLA_PROMISCUITY}, 0},
{{nla_len=8, nla_type=IFLA_NUM_TX_QUEUES}, 1},
{{nla_len=8, nla_type=IFLA_GSO_MAX_SEGS}, 65535},
{{nla_len=8, nla_type=IFLA_GSO_MAX_SIZE}, 65536},
{{nla_len=8, nla_type=IFLA_NUM_RX_QUEUES}, 1},
{{nla_len=5, nla_type=IFLA_CARRIER}, 1},
{{nla_len=13, nla_type=IFLA_QDISC}, "fq_codel"},
{{nla_len=8, nla_type=IFLA_CARRIER_CHANGES}, 2},
{{nla_len=5, nla_type=IFLA_PROTO_DOWN}, 0},
{{nla_len=8, nla_type=IFLA_CARRIER_UP_COUNT}, 1},
{{nla_len=8, nla_type=IFLA_CARRIER_DOWN_COUNT}, 1},
 */

struct if_state {
	uint8_t		ifla_operstate;
	uint8_t		ifla_carrier;
};

static void
get_operstate_ether(struct ifnet *ifp, struct if_state *pstate)
{
	struct ifmediareq ifmr = {};
	int error;
	error = (*ifp->if_ioctl)(ifp, SIOCGIFMEDIA, (void *)&ifmr);

	if (error != 0) {
		NL_LOG(LOG_DEBUG, "error calling SIOCGIFMEDIA on %s: %d",
		    if_name(ifp), error);
		return;
	}

	switch (IFM_TYPE(ifmr.ifm_active)) {
	case IFM_ETHER:
		if (ifmr.ifm_status & IFM_ACTIVE) {
			pstate->ifla_carrier = 1;
			if (ifp->if_flags & IFF_MONITOR)
				pstate->ifla_operstate = IF_OPER_DORMANT;
			else
				pstate->ifla_operstate = IF_OPER_UP;
		} else
			pstate->ifla_operstate = IF_OPER_DOWN;
	}
}

static bool
get_stats(struct nl_writer *nw, struct ifnet *ifp)
{
	struct rtnl_link_stats64 *stats;

	int nla_len = sizeof(struct nlattr) + sizeof(*stats);
	struct nlattr *nla = nlmsg_reserve_data(nw, nla_len, struct nlattr);
	if (nla == NULL)
		return (false);
	nla->nla_type = IFLA_STATS64;
	nla->nla_len = nla_len;
	stats = (struct rtnl_link_stats64 *)(nla + 1);

	stats->rx_packets = ifp->if_get_counter(ifp, IFCOUNTER_IPACKETS);
	stats->tx_packets = ifp->if_get_counter(ifp, IFCOUNTER_OPACKETS);
	stats->rx_bytes = ifp->if_get_counter(ifp, IFCOUNTER_IBYTES);
	stats->tx_bytes = ifp->if_get_counter(ifp, IFCOUNTER_OBYTES);
	stats->rx_errors = ifp->if_get_counter(ifp, IFCOUNTER_IERRORS);
	stats->tx_errors = ifp->if_get_counter(ifp, IFCOUNTER_OERRORS);
	stats->rx_dropped = ifp->if_get_counter(ifp, IFCOUNTER_IQDROPS);
	stats->tx_dropped = ifp->if_get_counter(ifp, IFCOUNTER_OQDROPS);
	stats->multicast = ifp->if_get_counter(ifp, IFCOUNTER_IMCASTS);
	stats->rx_nohandler = ifp->if_get_counter(ifp, IFCOUNTER_NOPROTO);

	return (true);
}

static void
get_operstate(struct ifnet *ifp, struct if_state *pstate)
{
	pstate->ifla_operstate = IF_OPER_UNKNOWN;
	pstate->ifla_carrier = 0; /* no carrier */

	switch (ifp->if_type) {
	case IFT_ETHER:
	case IFT_L2VLAN:
		get_operstate_ether(ifp, pstate);
		break;
	default:
		/* Map admin state to the operstate */
		if (ifp->if_flags & IFF_UP) {
			pstate->ifla_operstate = IF_OPER_UP;
			pstate->ifla_carrier = 1;
		} else
			pstate->ifla_operstate = IF_OPER_DOWN;
		break;
	}
}

static void
get_hwaddr(struct nl_writer *nw, struct ifnet *ifp)
{
	struct ifreq ifr = {};

	if (if_gethwaddr(ifp, &ifr) == 0) {
		nlattr_add(nw, IFLAF_ORIG_HWADDR, if_getaddrlen(ifp),
		    ifr.ifr_addr.sa_data);
	}
}

static unsigned
ifp_flags_to_netlink(const struct ifnet *ifp)
{
        return (ifp->if_flags | ifp->if_drv_flags);
}

#define LLADDR_CONST(s) ((const void *)((s)->sdl_data + (s)->sdl_nlen))
static bool
dump_sa(struct nl_writer *nw, int attr, const struct sockaddr *sa)
{
        uint32_t addr_len = 0;
        const void *addr_data = NULL;
#ifdef INET6
        struct in6_addr addr6;
#endif

        if (sa == NULL)
                return (true);

        switch (sa->sa_family) {
#ifdef INET
        case AF_INET:
                addr_len = sizeof(struct in_addr);
                addr_data = &((const struct sockaddr_in *)sa)->sin_addr;
                break;
#endif
#ifdef INET6
        case AF_INET6:
                in6_splitscope(&((const struct sockaddr_in6 *)sa)->sin6_addr, &addr6, &addr_len);
                addr_len = sizeof(struct in6_addr);
                addr_data = &addr6;
                break;
#endif
        case AF_LINK:
                addr_len = ((const struct sockaddr_dl *)sa)->sdl_alen;
                addr_data = LLADDR_CONST((const struct sockaddr_dl *)sa);
                break;
	case AF_UNSPEC:
		/* Ignore empty SAs without warning */
		return (true);
        default:
                NL_LOG(LOG_DEBUG2, "unsupported family: %d, skipping", sa->sa_family);
                return (true);
        }

        return (nlattr_add(nw, attr, addr_len, addr_data));
}

/*
 * Dumps interface state, properties and metrics.
 * @nw: message writer
 * @ifp: target interface
 * @hdr: template header
 * @if_flags_mask: changed if_[drv]_flags bitmask
 *
 * This function is called without epoch and MAY sleep.
 */
static bool
dump_iface(struct nl_writer *nw, struct ifnet *ifp, const struct nlmsghdr *hdr,
    int if_flags_mask)
{
        struct ifinfomsg *ifinfo;

        NL_LOG(LOG_DEBUG3, "dumping interface %s data", if_name(ifp));

	if (!nlmsg_reply(nw, hdr, sizeof(struct ifinfomsg)))
		goto enomem;

        ifinfo = nlmsg_reserve_object(nw, struct ifinfomsg);
        ifinfo->ifi_family = AF_UNSPEC;
        ifinfo->__ifi_pad = 0;
        ifinfo->ifi_type = ifp->if_type;
        ifinfo->ifi_index = ifp->if_index;
        ifinfo->ifi_flags = ifp_flags_to_netlink(ifp);
        ifinfo->ifi_change = if_flags_mask;

	struct if_state ifs = {};
	get_operstate(ifp, &ifs);

	if (ifs.ifla_operstate == IF_OPER_UP)
		ifinfo->ifi_flags |= IFF_LOWER_UP;

        nlattr_add_string(nw, IFLA_IFNAME, if_name(ifp));
        nlattr_add_u8(nw, IFLA_OPERSTATE, ifs.ifla_operstate);
        nlattr_add_u8(nw, IFLA_CARRIER, ifs.ifla_carrier);

/*
        nlattr_add_u8(nw, IFLA_PROTO_DOWN, val);
        nlattr_add_u8(nw, IFLA_LINKMODE, val);
*/
        if (if_getaddrlen(ifp) != 0) {
		struct ifaddr *ifa = if_getifaddr(ifp);

                dump_sa(nw, IFLA_ADDRESS, ifa->ifa_addr);
        }

        if ((ifp->if_broadcastaddr != NULL)) {
		nlattr_add(nw, IFLA_BROADCAST, ifp->if_addrlen,
		    ifp->if_broadcastaddr);
        }

        nlattr_add_u32(nw, IFLA_MTU, ifp->if_mtu);
/*
        nlattr_add_u32(nw, IFLA_MIN_MTU, 60);
        nlattr_add_u32(nw, IFLA_MAX_MTU, 9000);
        nlattr_add_u32(nw, IFLA_GROUP, 0);
*/

	if (ifp->if_description != NULL)
		nlattr_add_string(nw, IFLA_IFALIAS, ifp->if_description);

	/* Store FreeBSD-specific attributes */
	int off = nlattr_add_nested(nw, IFLA_FREEBSD);
	if (off != 0) {
		get_hwaddr(nw, ifp);

		nlattr_set_len(nw, off);
	}

	get_stats(nw, ifp);

	uint32_t val = (ifp->if_flags & IFF_PROMISC) != 0;
        nlattr_add_u32(nw, IFLA_PROMISCUITY, val);

	ifc_dump_ifp_nl(ifp, nw);

        if (nlmsg_end(nw))
		return (true);

enomem:
        NL_LOG(LOG_DEBUG, "unable to dump interface %s state (ENOMEM)", if_name(ifp));
        nlmsg_abort(nw);
        return (false);
}

static bool
check_ifmsg(void *hdr, struct nl_pstate *npt)
{
	struct ifinfomsg *ifm = hdr;

	if (ifm->__ifi_pad != 0 || ifm->ifi_type != 0 ||
	    ifm->ifi_flags != 0 || ifm->ifi_change != 0) {
		nlmsg_report_err_msg(npt,
		    "strict checking: non-zero values in ifinfomsg header");
		return (false);
	}

	return (true);
}

#define	_IN(_field)	offsetof(struct ifinfomsg, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_link, _field)
static const struct nlfield_parser nlf_p_if[] = {
	{ .off_in = _IN(ifi_type), .off_out = _OUT(ifi_type), .cb = nlf_get_u16 },
	{ .off_in = _IN(ifi_index), .off_out = _OUT(ifi_index), .cb = nlf_get_u32 },
	{ .off_in = _IN(ifi_flags), .off_out = _OUT(ifi_flags), .cb = nlf_get_u32 },
	{ .off_in = _IN(ifi_change), .off_out = _OUT(ifi_change), .cb = nlf_get_u32 },
};

static const struct nlattr_parser nla_p_linfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(ifla_cloner), .cb = nlattr_get_stringn },
	{ .type = IFLA_INFO_DATA, .off = _OUT(ifla_idata), .cb = nlattr_get_nla },
};
NL_DECLARE_ATTR_PARSER(linfo_parser, nla_p_linfo);

static const struct nlattr_parser nla_p_if[] = {
	{ .type = IFLA_IFNAME, .off = _OUT(ifla_ifname), .cb = nlattr_get_string },
	{ .type = IFLA_MTU, .off = _OUT(ifla_mtu), .cb = nlattr_get_uint32 },
	{ .type = IFLA_LINK, .off = _OUT(ifla_link), .cb = nlattr_get_uint32 },
	{ .type = IFLA_LINKINFO, .arg = &linfo_parser, .cb = nlattr_get_nested },
	{ .type = IFLA_IFALIAS, .off = _OUT(ifla_ifalias), .cb = nlattr_get_string },
	{ .type = IFLA_GROUP, .off = _OUT(ifla_group), .cb = nlattr_get_string },
	{ .type = IFLA_ALT_IFNAME, .off = _OUT(ifla_ifname), .cb = nlattr_get_string },
};
#undef _IN
#undef _OUT
NL_DECLARE_STRICT_PARSER(ifmsg_parser, struct ifinfomsg, check_ifmsg, nlf_p_if, nla_p_if);

static bool
match_iface(struct ifnet *ifp, void *_arg)
{
	struct nl_parsed_link *attrs = (struct nl_parsed_link *)_arg;

	if (attrs->ifi_index != 0 && attrs->ifi_index != ifp->if_index)
		return (false);
	if (attrs->ifi_type != 0 && attrs->ifi_index != ifp->if_type)
		return (false);
	if (attrs->ifla_ifname != NULL && strcmp(attrs->ifla_ifname, if_name(ifp)))
		return (false);
	/* TODO: add group match */

	return (true);
}

static int
dump_cb(struct ifnet *ifp, void *_arg)
{
	struct netlink_walkargs *wa = (struct netlink_walkargs *)_arg;
	if (!dump_iface(wa->nw, ifp, &wa->hdr, 0))
		return (ENOMEM);
	return (0);
}

/*
 * {nlmsg_len=52, nlmsg_type=RTM_GETLINK, nlmsg_flags=NLM_F_REQUEST, nlmsg_seq=1662842818, nlmsg_pid=0},
 *  {ifi_family=AF_PACKET, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0},
 *   [
 *    [{nla_len=10, nla_type=IFLA_IFNAME}, "vnet9"],
 *    [{nla_len=8, nla_type=IFLA_EXT_MASK}, RTEXT_FILTER_VF]
 *   ]
 */
static int
rtnl_handle_getlink(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct epoch_tracker et;
        struct ifnet *ifp;
	int error = 0;

	struct nl_parsed_link attrs = {};
	error = nl_parse_nlmsg(hdr, &ifmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	struct netlink_walkargs wa = {
		.so = nlp,
		.nw = npt->nw,
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_flags = hdr->nlmsg_flags,
		.hdr.nlmsg_type = NL_RTM_NEWLINK,
	};

	/* Fast track for an interface w/ explicit name or index match */
	if ((attrs.ifi_index != 0) || (attrs.ifla_ifname != NULL)) {
		if (attrs.ifi_index != 0) {
			NLP_LOG(LOG_DEBUG3, nlp, "fast track -> searching index %u",
			    attrs.ifi_index);
			NET_EPOCH_ENTER(et);
			ifp = ifnet_byindex_ref(attrs.ifi_index);
			NET_EPOCH_EXIT(et);
		} else {
			NLP_LOG(LOG_DEBUG3, nlp, "fast track -> searching name %s",
			    attrs.ifla_ifname);
			ifp = ifunit_ref(attrs.ifla_ifname);
		}

		if (ifp != NULL) {
			if (match_iface(ifp, &attrs)) {
				if (!dump_iface(wa.nw, ifp, &wa.hdr, 0))
					error = ENOMEM;
			} else
				error = ENODEV;
			if_rele(ifp);
		} else
			error = ENODEV;
		return (error);
	}

	/* Always treat non-direct-match as a multipart message */
	wa.hdr.nlmsg_flags |= NLM_F_MULTI;

	/*
	 * Fetching some link properties require performing ioctl's that may be blocking.
	 * Address it by saving referenced pointers of the matching links,
	 * exiting from epoch and going through the list one-by-one.
	 */

	NL_LOG(LOG_DEBUG2, "Start dump");
	if_foreach_sleep(match_iface, &attrs, dump_cb, &wa);
	NL_LOG(LOG_DEBUG2, "End dump, iterated %d dumped %d", wa.count, wa.dumped);

	if (!nlmsg_end_dump(wa.nw, error, &wa.hdr)) {
                NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
                return (ENOMEM);
        }

	return (error);
}

/*
 * sendmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[
 * {nlmsg_len=60, nlmsg_type=RTM_NEWLINK, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL|NLM_F_CREATE, nlmsg_seq=1662715618, nlmsg_pid=0},
 *  {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0},
 *   {nla_len=11, nla_type=IFLA_IFNAME}, "dummy0"],
 *   [
 *    {nla_len=16, nla_type=IFLA_LINKINFO},
 *     [
 *      {nla_len=9, nla_type=IFLA_INFO_KIND}, "dummy"...
 *     ]
 *    ]
 */

static int
rtnl_handle_dellink(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct epoch_tracker et;
        struct ifnet *ifp;
	int error;

	struct nl_parsed_link attrs = {};
	error = nl_parse_nlmsg(hdr, &ifmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	NET_EPOCH_ENTER(et);
	ifp = ifnet_byindex_ref(attrs.ifi_index);
	NET_EPOCH_EXIT(et);
	if (ifp == NULL) {
		NLP_LOG(LOG_DEBUG, nlp, "unable to find interface %u", attrs.ifi_index);
		return (ENOENT);
	}
	NLP_LOG(LOG_DEBUG3, nlp, "mapped ifindex %u to %s", attrs.ifi_index, if_name(ifp));

	sx_xlock(&ifnet_detach_sxlock);
	error = if_clone_destroy(if_name(ifp));
	sx_xunlock(&ifnet_detach_sxlock);

	NLP_LOG(LOG_DEBUG2, nlp, "deleting interface %s returned %d", if_name(ifp), error);

	if_rele(ifp);
	return (error);
}

/*
 * New link:
 * type=RTM_NEWLINK, flags=NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL|NLM_F_CREATE, seq=1668185590, pid=0},
 *   {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0}
 *    [
 *     {{nla_len=8, nla_type=IFLA_MTU}, 123},
 *     {{nla_len=10, nla_type=IFLA_IFNAME}, "vlan1"},
 *     {{nla_len=24, nla_type=IFLA_LINKINFO},
 *      [
 *       {{nla_len=8, nla_type=IFLA_INFO_KIND}, "vlan"...},
 *       {{nla_len=12, nla_type=IFLA_INFO_DATA}, "\x06\x00\x01\x00\x7b\x00\x00\x00"}]}]}
 *
 * Update link:
 * type=RTM_NEWLINK, flags=NLM_F_REQUEST|NLM_F_ACK, seq=1668185923, pid=0},
 * {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=if_nametoindex("lo"), ifi_flags=0, ifi_change=0},
 * {{nla_len=8, nla_type=IFLA_MTU}, 123}}
 *
 *
 * Check command availability:
 * type=RTM_NEWLINK, flags=NLM_F_REQUEST|NLM_F_ACK, seq=0, pid=0},
 *  {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0}
 */


static int
create_link(struct nlmsghdr *hdr, struct nl_parsed_link *lattrs,
    struct nlattr_bmask *bm, struct nlpcb *nlp, struct nl_pstate *npt)
{
	if (lattrs->ifla_ifname == NULL || strlen(lattrs->ifla_ifname) == 0) {
		NLMSG_REPORT_ERR_MSG(npt, "empty IFLA_IFNAME attribute");
		return (EINVAL);
	}
	if (lattrs->ifla_cloner == NULL || strlen(lattrs->ifla_cloner) == 0) {
		NLMSG_REPORT_ERR_MSG(npt, "empty IFLA_INFO_KIND attribute");
		return (EINVAL);
	}

	struct ifc_data_nl ifd = {
		.flags = IFC_F_CREATE,
		.lattrs = lattrs,
		.bm = bm,
		.npt = npt,
	};
	if (ifc_create_ifp_nl(lattrs->ifla_ifname, &ifd) && ifd.error == 0)
		nl_store_ifp_cookie(npt, ifd.ifp);

	return (ifd.error);
}

static int
modify_link(struct nlmsghdr *hdr, struct nl_parsed_link *lattrs,
    struct nlattr_bmask *bm, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct ifnet *ifp = NULL;
	struct epoch_tracker et;

	if (lattrs->ifi_index == 0 && lattrs->ifla_ifname == NULL) {
		/*
		 * Applications like ip(8) verify RTM_NEWLINK command
		 * existence by calling it with empty arguments. Always
		 * return "innocent" error in that case.
		 */
		NLMSG_REPORT_ERR_MSG(npt, "empty ifi_index field");
		return (EPERM);
	}

	if (lattrs->ifi_index != 0) {
		NET_EPOCH_ENTER(et);
		ifp = ifnet_byindex_ref(lattrs->ifi_index);
		NET_EPOCH_EXIT(et);
		if (ifp == NULL) {
			NLMSG_REPORT_ERR_MSG(npt, "unable to find interface #%u",
			    lattrs->ifi_index);
			return (ENOENT);
		}
	}

	if (ifp == NULL && lattrs->ifla_ifname != NULL) {
		ifp = ifunit_ref(lattrs->ifla_ifname);
		if (ifp == NULL) {
			NLMSG_REPORT_ERR_MSG(npt, "unable to find interface %s",
			    lattrs->ifla_ifname);
			return (ENOENT);
		}
	}

	MPASS(ifp != NULL);

	/*
	 * Modification request can address either
	 * 1) cloned interface, in which case we call the cloner-specific
	 *  modification routine
	 * or
	 * 2) non-cloned (e.g. "physical") interface, in which case we call
	 *  generic modification routine
	 */
	struct ifc_data_nl ifd = { .lattrs = lattrs, .bm = bm, .npt = npt };
	if (!ifc_modify_ifp_nl(ifp, &ifd))
		ifd.error = nl_modify_ifp_generic(ifp, lattrs, bm, npt);

	if_rele(ifp);

	return (ifd.error);
}


static int
rtnl_handle_newlink(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct nlattr_bmask bm;
	int error;

	struct nl_parsed_link attrs = {};
	error = nl_parse_nlmsg(hdr, &ifmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);
	nl_get_attrs_bmask_nlmsg(hdr, &ifmsg_parser, &bm);

	if (hdr->nlmsg_flags & NLM_F_CREATE)
		return (create_link(hdr, &attrs, &bm, nlp, npt));
	else
		return (modify_link(hdr, &attrs, &bm, nlp, npt));
}

static void
set_scope6(struct sockaddr *sa, uint32_t ifindex)
{
#ifdef INET6
	if (sa != NULL && sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;

		if (IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr))
			in6_set_unicast_scopeid(&sa6->sin6_addr, ifindex);
	}
#endif
}

static bool
check_sa_family(const struct sockaddr *sa, int family, const char *attr_name,
    struct nl_pstate *npt)
{
	if (sa == NULL || sa->sa_family == family)
		return (true);

	nlmsg_report_err_msg(npt, "wrong family for %s attribute: %d != %d",
	    attr_name, family, sa->sa_family);
	return (false);
}

struct nl_parsed_ifa {
	uint8_t			ifa_family;
	uint8_t			ifa_prefixlen;
	uint8_t			ifa_scope;
	uint32_t		ifa_index;
	uint32_t		ifa_flags;
	uint32_t		ifaf_vhid;
	uint32_t		ifaf_flags;
	struct sockaddr		*ifa_address;
	struct sockaddr		*ifa_local;
	struct sockaddr		*ifa_broadcast;
	struct ifa_cacheinfo	*ifa_cacheinfo;
	struct sockaddr		*f_ifa_addr;
	struct sockaddr		*f_ifa_dst;
};

static int
nlattr_get_cinfo(struct nlattr *nla, struct nl_pstate *npt,
    const void *arg __unused, void *target)
{
	if (__predict_false(NLA_DATA_LEN(nla) != sizeof(struct ifa_cacheinfo))) {
		NLMSG_REPORT_ERR_MSG(npt, "nla type %d size(%u) is not ifa_cacheinfo",
		    nla->nla_type, NLA_DATA_LEN(nla));
		return (EINVAL);
	}
	*((struct ifa_cacheinfo **)target) = (struct ifa_cacheinfo *)NL_RTA_DATA(nla);
	return (0);
}

#define	_IN(_field)	offsetof(struct ifaddrmsg, _field)
#define	_OUT(_field)	offsetof(struct nl_parsed_ifa, _field)
static const struct nlfield_parser nlf_p_ifa[] = {
	{ .off_in = _IN(ifa_family), .off_out = _OUT(ifa_family), .cb = nlf_get_u8 },
	{ .off_in = _IN(ifa_prefixlen), .off_out = _OUT(ifa_prefixlen), .cb = nlf_get_u8 },
	{ .off_in = _IN(ifa_scope), .off_out = _OUT(ifa_scope), .cb = nlf_get_u8 },
	{ .off_in = _IN(ifa_flags), .off_out = _OUT(ifa_flags), .cb = nlf_get_u8_u32 },
	{ .off_in = _IN(ifa_index), .off_out = _OUT(ifa_index), .cb = nlf_get_u32 },
};

static const struct nlattr_parser nla_p_ifa_fbsd[] = {
	{ .type = IFAF_VHID, .off = _OUT(ifaf_vhid), .cb = nlattr_get_uint32 },
	{ .type = IFAF_FLAGS, .off = _OUT(ifaf_flags), .cb = nlattr_get_uint32 },
};
NL_DECLARE_ATTR_PARSER(ifa_fbsd_parser, nla_p_ifa_fbsd);

static const struct nlattr_parser nla_p_ifa[] = {
	{ .type = IFA_ADDRESS, .off = _OUT(ifa_address), .cb = nlattr_get_ip },
	{ .type = IFA_LOCAL, .off = _OUT(ifa_local), .cb = nlattr_get_ip },
	{ .type = IFA_BROADCAST, .off = _OUT(ifa_broadcast), .cb = nlattr_get_ip },
	{ .type = IFA_CACHEINFO, .off = _OUT(ifa_cacheinfo), .cb = nlattr_get_cinfo },
	{ .type = IFA_FLAGS, .off = _OUT(ifa_flags), .cb = nlattr_get_uint32 },
	{ .type = IFA_FREEBSD, .arg = &ifa_fbsd_parser, .cb = nlattr_get_nested },
};
#undef _IN
#undef _OUT

static bool
post_p_ifa(void *_attrs, struct nl_pstate *npt)
{
	struct nl_parsed_ifa *attrs = (struct nl_parsed_ifa *)_attrs;

	if (!check_sa_family(attrs->ifa_address, attrs->ifa_family, "IFA_ADDRESS", npt))
		return (false);
	if (!check_sa_family(attrs->ifa_local, attrs->ifa_family, "IFA_LOCAL", npt))
		return (false);
	if (!check_sa_family(attrs->ifa_broadcast, attrs->ifa_family, "IFA_BROADADDR", npt))
		return (false);

	set_scope6(attrs->ifa_address, attrs->ifa_index);
	set_scope6(attrs->ifa_local, attrs->ifa_index);

	return (true);
}

NL_DECLARE_PARSER_EXT(ifa_parser, struct ifaddrmsg, NULL, nlf_p_ifa, nla_p_ifa, post_p_ifa);


/*

{ifa_family=AF_INET, ifa_prefixlen=8, ifa_flags=IFA_F_PERMANENT, ifa_scope=RT_SCOPE_HOST, ifa_index=if_nametoindex("lo")},
 [
        {{nla_len=8, nla_type=IFA_ADDRESS}, inet_addr("127.0.0.1")},
        {{nla_len=8, nla_type=IFA_LOCAL}, inet_addr("127.0.0.1")},
        {{nla_len=7, nla_type=IFA_LABEL}, "lo"},
        {{nla_len=8, nla_type=IFA_FLAGS}, IFA_F_PERMANENT},
        {{nla_len=20, nla_type=IFA_CACHEINFO}, {ifa_prefered=4294967295, ifa_valid=4294967295, cstamp=3619, tstamp=3619}}]},
---

{{len=72, type=RTM_NEWADDR, flags=NLM_F_MULTI, seq=1642191126, pid=566735},
 {ifa_family=AF_INET6, ifa_prefixlen=96, ifa_flags=IFA_F_PERMANENT, ifa_scope=RT_SCOPE_UNIVERSE, ifa_index=if_nametoindex("virbr0")},
   [
    {{nla_len=20, nla_type=IFA_ADDRESS}, inet_pton(AF_INET6, "2a01:4f8:13a:70c:ffff::1")},
   {{nla_len=20, nla_type=IFA_CACHEINFO}, {ifa_prefered=4294967295, ifa_valid=4294967295, cstamp=4283, tstamp=4283}},
   {{nla_len=8, nla_type=IFA_FLAGS}, IFA_F_PERMANENT}]},
*/

static uint8_t
ifa_get_scope(const struct ifaddr *ifa)
{
        const struct sockaddr *sa;
        uint8_t addr_scope = RT_SCOPE_UNIVERSE;

        sa = ifa->ifa_addr;
        switch (sa->sa_family) {
#ifdef INET
        case AF_INET:
                {
                        struct in_addr addr;
                        addr = ((const struct sockaddr_in *)sa)->sin_addr;
                        if (IN_LOOPBACK(addr.s_addr))
                                addr_scope = RT_SCOPE_HOST;
                        else if (IN_LINKLOCAL(addr.s_addr))
                                addr_scope = RT_SCOPE_LINK;
                        break;
                }
#endif
#ifdef INET6
        case AF_INET6:
                {
                        const struct in6_addr *addr;
                        addr = &((const struct sockaddr_in6 *)sa)->sin6_addr;
                        if (IN6_IS_ADDR_LOOPBACK(addr))
                                addr_scope = RT_SCOPE_HOST;
                        else if (IN6_IS_ADDR_LINKLOCAL(addr))
                                addr_scope = RT_SCOPE_LINK;
                        break;
                }
#endif
        }

        return (addr_scope);
}

#ifdef INET6
static uint8_t
inet6_get_plen(const struct in6_addr *addr)
{

	return (bitcount32(addr->s6_addr32[0]) + bitcount32(addr->s6_addr32[1]) +
	    bitcount32(addr->s6_addr32[2]) + bitcount32(addr->s6_addr32[3]));
}
#endif

static uint8_t
get_sa_plen(const struct sockaddr *sa)
{
#ifdef INET
        const struct in_addr *paddr;
#endif
#ifdef INET6
        const struct in6_addr *paddr6;
#endif

        switch (sa->sa_family) {
#ifdef INET
        case AF_INET:
                paddr = &(((const struct sockaddr_in *)sa)->sin_addr);
                return bitcount32(paddr->s_addr);;
#endif
#ifdef INET6
        case AF_INET6:
                paddr6 = &(((const struct sockaddr_in6 *)sa)->sin6_addr);
                return inet6_get_plen(paddr6);
#endif
        }

        return (0);
}

#ifdef INET6
static uint32_t
in6_flags_to_nl(uint32_t flags)
{
	uint32_t nl_flags = 0;

	if (flags & IN6_IFF_TEMPORARY)
		nl_flags |= IFA_F_TEMPORARY;
	if (flags & IN6_IFF_NODAD)
		nl_flags |= IFA_F_NODAD;
	if (flags & IN6_IFF_DEPRECATED)
		nl_flags |= IFA_F_DEPRECATED;
	if (flags & IN6_IFF_TENTATIVE)
		nl_flags |= IFA_F_TENTATIVE;
	if ((flags & (IN6_IFF_AUTOCONF|IN6_IFF_TEMPORARY)) == 0)
		flags |= IFA_F_PERMANENT;
	if (flags & IN6_IFF_DUPLICATED)
		flags |= IFA_F_DADFAILED;
	return (nl_flags);
}

static uint32_t
nl_flags_to_in6(uint32_t flags)
{
	uint32_t in6_flags = 0;

	if (flags & IFA_F_TEMPORARY)
		in6_flags |= IN6_IFF_TEMPORARY;
	if (flags & IFA_F_NODAD)
		in6_flags |= IN6_IFF_NODAD;
	if (flags & IFA_F_DEPRECATED)
		in6_flags |= IN6_IFF_DEPRECATED;
	if (flags & IFA_F_TENTATIVE)
		in6_flags |= IN6_IFF_TENTATIVE;
	if (flags & IFA_F_DADFAILED)
		in6_flags |= IN6_IFF_DUPLICATED;

	return (in6_flags);
}

static void
export_cache_info6(struct nl_writer *nw, const struct in6_ifaddr *ia)
{
	struct ifa_cacheinfo ci = {
		.cstamp = ia->ia6_createtime * 1000,
		.tstamp = ia->ia6_updatetime * 1000,
		.ifa_prefered = ia->ia6_lifetime.ia6t_pltime,
		.ifa_valid = ia->ia6_lifetime.ia6t_vltime,
	};

	nlattr_add(nw, IFA_CACHEINFO, sizeof(ci), &ci);
}
#endif

static void
export_cache_info(struct nl_writer *nw, struct ifaddr *ifa)
{
	switch (ifa->ifa_addr->sa_family) {
#ifdef INET6
	case AF_INET6:
		export_cache_info6(nw, (struct in6_ifaddr *)ifa);
		break;
#endif
	}
}

/*
 * {'attrs': [('IFA_ADDRESS', '12.0.0.1'),
           ('IFA_LOCAL', '12.0.0.1'),
           ('IFA_LABEL', 'eth10'),
           ('IFA_FLAGS', 128),
           ('IFA_CACHEINFO', {'ifa_preferred': 4294967295, 'ifa_valid': 4294967295, 'cstamp': 63745746, 'tstamp': 63745746})],
 */
static bool
dump_iface_addr(struct nl_writer *nw, struct ifnet *ifp, struct ifaddr *ifa,
    const struct nlmsghdr *hdr)
{
        struct ifaddrmsg *ifamsg;
        struct sockaddr *sa = ifa->ifa_addr;
        struct sockaddr *sa_dst = ifa->ifa_dstaddr;

        NL_LOG(LOG_DEBUG3, "dumping ifa %p type %s(%d) for interface %s",
            ifa, rib_print_family(sa->sa_family), sa->sa_family, if_name(ifp));

	if (!nlmsg_reply(nw, hdr, sizeof(struct ifaddrmsg)))
		goto enomem;

        ifamsg = nlmsg_reserve_object(nw, struct ifaddrmsg);
        ifamsg->ifa_family = sa->sa_family;
        ifamsg->ifa_prefixlen = get_sa_plen(ifa->ifa_netmask);
        ifamsg->ifa_flags = 0; // ifa_flags is useless
        ifamsg->ifa_scope = ifa_get_scope(ifa);
        ifamsg->ifa_index = ifp->if_index;

	if ((ifp->if_flags & IFF_POINTOPOINT) && sa_dst != NULL && sa_dst->sa_family != 0) {
		/* P2P interface may have IPv6 LL with no dst address */
		dump_sa(nw, IFA_ADDRESS, sa_dst);
		dump_sa(nw, IFA_LOCAL, sa);
	} else {
		dump_sa(nw, IFA_ADDRESS, sa);
#ifdef INET
		/*
		 * In most cases, IFA_ADDRESS == IFA_LOCAL
		 * Skip IFA_LOCAL for anything except INET
		 */
		if (sa->sa_family == AF_INET)
			dump_sa(nw, IFA_LOCAL, sa);
#endif
	}
	if (ifp->if_flags & IFF_BROADCAST)
		dump_sa(nw, IFA_BROADCAST, ifa->ifa_broadaddr);

        nlattr_add_string(nw, IFA_LABEL, if_name(ifp));

        uint32_t nl_ifa_flags = 0;
#ifdef INET6
	if (sa->sa_family == AF_INET6) {
		struct in6_ifaddr *ia = (struct in6_ifaddr *)ifa;
		nl_ifa_flags = in6_flags_to_nl(ia->ia6_flags);
	}
#endif
        nlattr_add_u32(nw, IFA_FLAGS, nl_ifa_flags);

	export_cache_info(nw, ifa);

	/* Store FreeBSD-specific attributes */
	int off = nlattr_add_nested(nw, IFA_FREEBSD);
	if (off != 0) {
		if (ifa->ifa_carp != NULL && carp_get_vhid_p != NULL) {
			uint32_t vhid  = (uint32_t)(*carp_get_vhid_p)(ifa);
			nlattr_add_u32(nw, IFAF_VHID, vhid);
		}
#ifdef INET6
		if (sa->sa_family == AF_INET6) {
			uint32_t ifa_flags = ((struct in6_ifaddr *)ifa)->ia6_flags;

			nlattr_add_u32(nw, IFAF_FLAGS, ifa_flags);
		}
#endif

		nlattr_set_len(nw, off);
	}

	if (nlmsg_end(nw))
		return (true);
enomem:
        NL_LOG(LOG_DEBUG, "Failed to dump ifa type %s(%d) for interface %s",
            rib_print_family(sa->sa_family), sa->sa_family, if_name(ifp));
        nlmsg_abort(nw);
        return (false);
}

static int
dump_iface_addrs(struct netlink_walkargs *wa, struct ifnet *ifp)
{
        struct ifaddr *ifa;

	CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (wa->family != 0 && wa->family != ifa->ifa_addr->sa_family)
			continue;
		if (ifa->ifa_addr->sa_family == AF_LINK)
			continue;
		if (prison_if(wa->cred, ifa->ifa_addr) != 0)
			continue;
		wa->count++;
		if (!dump_iface_addr(wa->nw, ifp, ifa, &wa->hdr))
			return (ENOMEM);
		wa->dumped++;
	}

	return (0);
}

static int
rtnl_handle_getaddr(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
        struct ifnet *ifp;
	int error = 0;

	struct nl_parsed_ifa attrs = {};
	error = nl_parse_nlmsg(hdr, &ifa_parser, npt, &attrs);
	if (error != 0)
		return (error);

	struct netlink_walkargs wa = {
		.so = nlp,
		.nw = npt->nw,
		.cred = nlp_get_cred(nlp),
		.family = attrs.ifa_family,
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_flags = hdr->nlmsg_flags | NLM_F_MULTI,
		.hdr.nlmsg_type = NL_RTM_NEWADDR,
	};

	NL_LOG(LOG_DEBUG2, "Start dump");

	if (attrs.ifa_index != 0) {
		ifp = ifnet_byindex(attrs.ifa_index);
		if (ifp == NULL)
			error = ENOENT;
		else
			error = dump_iface_addrs(&wa, ifp);
	} else {
		CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
			error = dump_iface_addrs(&wa, ifp);
			if (error != 0)
				break;
		}
	}

	NL_LOG(LOG_DEBUG2, "End dump, iterated %d dumped %d", wa.count, wa.dumped);

	if (!nlmsg_end_dump(wa.nw, error, &wa.hdr)) {
                NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
                return (ENOMEM);
        }

	return (error);
}

#ifdef INET
static int
handle_newaddr_inet(struct nlmsghdr *hdr, struct nl_parsed_ifa *attrs,
    struct ifnet *ifp, struct nlpcb *nlp, struct nl_pstate *npt)
{
	int plen = attrs->ifa_prefixlen;
	int if_flags = if_getflags(ifp);
	struct sockaddr_in *addr, *dst;

	if (plen > 32) {
		nlmsg_report_err_msg(npt, "invalid ifa_prefixlen");
		return (EINVAL);
	};

	if (if_flags & IFF_POINTOPOINT) {
		/*
		 * Only P2P IFAs are allowed by the implementation.
		 */
		if (attrs->ifa_address == NULL || attrs->ifa_local == NULL) {
			nlmsg_report_err_msg(npt, "Empty IFA_LOCAL/IFA_ADDRESS");
			return (EINVAL);
		}
		addr = (struct sockaddr_in *)attrs->ifa_local;
		dst = (struct sockaddr_in *)attrs->ifa_address;
	} else {
		/*
		 * Map the Netlink attributes to FreeBSD ifa layout.
		 * If only IFA_ADDRESS or IFA_LOCAL is set OR
		 * both are set to the same value => ifa is not p2p
		 * and the attribute value contains interface address.
		 *
		 * Otherwise (both IFA_ADDRESS and IFA_LOCAL are set and
		 * different), IFA_LOCAL contains an interface address and
		 * IFA_ADDRESS contains peer address.
		 */
		addr = (struct sockaddr_in *)attrs->ifa_local;
		if (addr == NULL)
			addr = (struct sockaddr_in *)attrs->ifa_address;

		if (addr == NULL) {
			nlmsg_report_err_msg(npt, "Empty IFA_LOCAL/IFA_ADDRESS");
			return (EINVAL);
		}

		/* Generate broadcast address if not set */
		if ((if_flags & IFF_BROADCAST) && attrs->ifa_broadcast == NULL) {
			uint32_t s_baddr;
			struct sockaddr_in *sin_brd;

			if (plen == 31)
				s_baddr = INADDR_BROADCAST; /* RFC 3021 */
			else {
				uint32_t s_mask;

				s_mask = htonl(plen ? ~((1 << (32 - plen)) - 1) : 0);
				s_baddr = addr->sin_addr.s_addr | ~s_mask;
			}

			sin_brd = (struct sockaddr_in *)npt_alloc(npt, sizeof(*sin_brd));
			if (sin_brd == NULL)
				return (ENOMEM);
			sin_brd->sin_family = AF_INET;
			sin_brd->sin_len = sizeof(*sin_brd);
			sin_brd->sin_addr.s_addr = s_baddr;
			attrs->ifa_broadcast = (struct sockaddr *)sin_brd;
		}
		dst = (struct sockaddr_in *)attrs->ifa_broadcast;
	}

	struct sockaddr_in mask = {
		.sin_len = sizeof(struct sockaddr_in),
		.sin_family = AF_INET,
		.sin_addr.s_addr = htonl(plen ? ~((1 << (32 - plen)) - 1) : 0),
	};
	struct in_aliasreq req = {
		.ifra_addr = *addr,
		.ifra_mask = mask,
		.ifra_vhid = attrs->ifaf_vhid,
	};
	if (dst != NULL)
		req.ifra_dstaddr = *dst;

	return (in_control(NULL, SIOCAIFADDR, &req, ifp, curthread));
}

static int
handle_deladdr_inet(struct nlmsghdr *hdr, struct nl_parsed_ifa *attrs,
    struct ifnet *ifp, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct sockaddr_in *addr = (struct sockaddr_in *)attrs->ifa_local;

	if (addr == NULL)
		addr = (struct sockaddr_in *)attrs->ifa_address;

	if (addr == NULL) {
		nlmsg_report_err_msg(npt, "empty IFA_ADDRESS/IFA_LOCAL");
		return (EINVAL);
	}

	struct in_aliasreq req = { .ifra_addr = *addr };

	return (in_control(NULL, SIOCDIFADDR, &req, ifp, curthread));
}
#endif

#ifdef INET6
static int
handle_newaddr_inet6(struct nlmsghdr *hdr, struct nl_parsed_ifa *attrs,
    struct ifnet *ifp, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct sockaddr_in6 *addr, *dst;

	if (attrs->ifa_prefixlen > 128) {
		nlmsg_report_err_msg(npt, "invalid ifa_prefixlen");
		return (EINVAL);
	}

	/*
	 * In IPv6 implementation, adding non-P2P address to the P2P interface
	 * is allowed.
	 */
	addr = (struct sockaddr_in6 *)(attrs->ifa_local);
	dst = (struct sockaddr_in6 *)(attrs->ifa_address);

	if (addr == NULL) {
		addr = dst;
		dst = NULL;
	} else if (dst != NULL) {
		if (IN6_ARE_ADDR_EQUAL(&addr->sin6_addr, &dst->sin6_addr)) {
			/*
			 * Sometimes Netlink users fills in both attributes
			 * with the same address. It still means "non-p2p".
			 */
			dst = NULL;
		}
	}

	if (addr == NULL) {
		nlmsg_report_err_msg(npt, "Empty IFA_LOCAL/IFA_ADDRESS");
		return (EINVAL);
	}

	uint32_t flags = nl_flags_to_in6(attrs->ifa_flags) | attrs->ifaf_flags;

	uint32_t pltime = 0, vltime = 0;
	if (attrs->ifa_cacheinfo != 0) {
		pltime = attrs->ifa_cacheinfo->ifa_prefered;
		vltime = attrs->ifa_cacheinfo->ifa_valid;
	}

	struct sockaddr_in6 mask = {
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_family = AF_INET6,
	};
	ip6_writemask(&mask.sin6_addr, attrs->ifa_prefixlen);

	struct in6_aliasreq req = {
		.ifra_addr = *addr,
		.ifra_prefixmask = mask,
		.ifra_flags = flags,
		.ifra_lifetime = { .ia6t_vltime = vltime, .ia6t_pltime = pltime },
		.ifra_vhid = attrs->ifaf_vhid,
	};
	if (dst != NULL)
		req.ifra_dstaddr = *dst;

	return (in6_control(NULL, SIOCAIFADDR_IN6, &req, ifp, curthread));
}

static int
handle_deladdr_inet6(struct nlmsghdr *hdr, struct nl_parsed_ifa *attrs,
    struct ifnet *ifp, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct sockaddr_in6 *addr = (struct sockaddr_in6 *)attrs->ifa_local;

	if (addr == NULL)
		addr = (struct sockaddr_in6 *)(attrs->ifa_address);

	if (addr == NULL) {
		nlmsg_report_err_msg(npt, "Empty IFA_LOCAL/IFA_ADDRESS");
		return (EINVAL);
	}

	struct in6_aliasreq req = { .ifra_addr = *addr };

	return (in6_control(NULL, SIOCDIFADDR_IN6, &req, ifp, curthread));
}
#endif


static int
rtnl_handle_addr(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct epoch_tracker et;
	int error;

	struct nl_parsed_ifa attrs = {};
	error = nl_parse_nlmsg(hdr, &ifa_parser, npt, &attrs);
	if (error != 0)
		return (error);

	NET_EPOCH_ENTER(et);
	struct ifnet *ifp = ifnet_byindex_ref(attrs.ifa_index);
	NET_EPOCH_EXIT(et);

	if (ifp == NULL) {
		nlmsg_report_err_msg(npt, "Unable to find interface with index %u",
		    attrs.ifa_index);
		return (ENOENT);
	}
	int if_flags = if_getflags(ifp);

#if defined(INET) || defined(INET6)
	bool new = hdr->nlmsg_type == NL_RTM_NEWADDR;
#endif

	/*
	 * TODO: Properly handle NLM_F_CREATE / NLM_F_EXCL.
	 * The current ioctl-based KPI always does an implicit create-or-replace.
	 * It is not possible to specify fine-grained options.
	 */

	switch (attrs.ifa_family) {
#ifdef INET
	case AF_INET:
		if (new)
			error = handle_newaddr_inet(hdr, &attrs, ifp, nlp, npt);
		else
			error = handle_deladdr_inet(hdr, &attrs, ifp, nlp, npt);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if (new)
			error = handle_newaddr_inet6(hdr, &attrs, ifp, nlp, npt);
		else
			error = handle_deladdr_inet6(hdr, &attrs, ifp, nlp, npt);
		break;
#endif
	default:
		error = EAFNOSUPPORT;
	}

	if (error == 0 && !(if_flags & IFF_UP) && (if_getflags(ifp) & IFF_UP))
		if_up(ifp);

	if_rele(ifp);

	return (error);
}


static void
rtnl_handle_ifaddr(void *arg __unused, struct ifaddr *ifa, int cmd)
{
	struct nlmsghdr hdr = {};
	struct nl_writer nw = {};
	uint32_t group = 0;

	switch (ifa->ifa_addr->sa_family) {
#ifdef INET
	case AF_INET:
		group = RTNLGRP_IPV4_IFADDR;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		group = RTNLGRP_IPV6_IFADDR;
		break;
#endif
	default:
		NL_LOG(LOG_DEBUG2, "ifa notification for unknown AF: %d",
		    ifa->ifa_addr->sa_family);
		return;
	}

	if (!nl_has_listeners(NETLINK_ROUTE, group))
		return;

	if (!nlmsg_get_group_writer(&nw, NLMSG_LARGE, NETLINK_ROUTE, group)) {
		NL_LOG(LOG_DEBUG, "error allocating group writer");
		return;
	}

	hdr.nlmsg_type = (cmd == RTM_DELETE) ? NL_RTM_DELADDR : NL_RTM_NEWADDR;

	dump_iface_addr(&nw, ifa->ifa_ifp, ifa, &hdr);
	nlmsg_flush(&nw);
}

static void
rtnl_handle_ifevent(struct ifnet *ifp, int nlmsg_type, int if_flags_mask)
{
	struct nlmsghdr hdr = { .nlmsg_type = nlmsg_type };
	struct nl_writer nw = {};

	if (!nl_has_listeners(NETLINK_ROUTE, RTNLGRP_LINK))
		return;

	if (!nlmsg_get_group_writer(&nw, NLMSG_LARGE, NETLINK_ROUTE, RTNLGRP_LINK)) {
		NL_LOG(LOG_DEBUG, "error allocating mbuf");
		return;
	}
	dump_iface(&nw, ifp, &hdr, if_flags_mask);
        nlmsg_flush(&nw);
}

static void
rtnl_handle_ifattach(void *arg, struct ifnet *ifp)
{
	NL_LOG(LOG_DEBUG2, "ifnet %s", if_name(ifp));
	rtnl_handle_ifevent(ifp, NL_RTM_NEWLINK, 0);
}

static void
rtnl_handle_ifdetach(void *arg, struct ifnet *ifp)
{
	NL_LOG(LOG_DEBUG2, "ifnet %s", if_name(ifp));
	rtnl_handle_ifevent(ifp, NL_RTM_DELLINK, 0);
}

static void
rtnl_handle_iflink(void *arg, struct ifnet *ifp)
{
	NL_LOG(LOG_DEBUG2, "ifnet %s", if_name(ifp));
	rtnl_handle_ifevent(ifp, NL_RTM_NEWLINK, 0);
}

void
rtnl_handle_ifnet_event(struct ifnet *ifp, int if_flags_mask)
{
	NL_LOG(LOG_DEBUG2, "ifnet %s", if_name(ifp));
	rtnl_handle_ifevent(ifp, NL_RTM_NEWLINK, if_flags_mask);
}

static const struct rtnl_cmd_handler cmd_handlers[] = {
	{
		.cmd = NL_RTM_GETLINK,
		.name = "RTM_GETLINK",
		.cb = &rtnl_handle_getlink,
		.flags = RTNL_F_NOEPOCH | RTNL_F_ALLOW_NONVNET_JAIL,
	},
	{
		.cmd = NL_RTM_DELLINK,
		.name = "RTM_DELLINK",
		.cb = &rtnl_handle_dellink,
		.priv = PRIV_NET_IFDESTROY,
		.flags = RTNL_F_NOEPOCH,
	},
	{
		.cmd = NL_RTM_NEWLINK,
		.name = "RTM_NEWLINK",
		.cb = &rtnl_handle_newlink,
		.priv = PRIV_NET_IFCREATE,
		.flags = RTNL_F_NOEPOCH,
	},
	{
		.cmd = NL_RTM_GETADDR,
		.name = "RTM_GETADDR",
		.cb = &rtnl_handle_getaddr,
		.flags = RTNL_F_ALLOW_NONVNET_JAIL,
	},
	{
		.cmd = NL_RTM_NEWADDR,
		.name = "RTM_NEWADDR",
		.cb = &rtnl_handle_addr,
		.priv = PRIV_NET_ADDIFADDR,
		.flags = RTNL_F_NOEPOCH,
	},
	{
		.cmd = NL_RTM_DELADDR,
		.name = "RTM_DELADDR",
		.cb = &rtnl_handle_addr,
		.priv = PRIV_NET_DELIFADDR,
		.flags = RTNL_F_NOEPOCH,
	},
};

static const struct nlhdr_parser *all_parsers[] = {
	&ifmsg_parser, &ifa_parser, &ifa_fbsd_parser,
};

void
rtnl_iface_add_cloner(struct nl_cloner *cloner)
{
	sx_xlock(&rtnl_cloner_lock);
	SLIST_INSERT_HEAD(&nl_cloners, cloner, next);
	sx_xunlock(&rtnl_cloner_lock);
}

void
rtnl_iface_del_cloner(struct nl_cloner *cloner)
{
	sx_xlock(&rtnl_cloner_lock);
	SLIST_REMOVE(&nl_cloners, cloner, nl_cloner, next);
	sx_xunlock(&rtnl_cloner_lock);
}

void
rtnl_ifaces_init(void)
{
	ifattach_event = EVENTHANDLER_REGISTER(
	    ifnet_arrival_event, rtnl_handle_ifattach, NULL,
	    EVENTHANDLER_PRI_ANY);
	ifdetach_event = EVENTHANDLER_REGISTER(
	    ifnet_departure_event, rtnl_handle_ifdetach, NULL,
	    EVENTHANDLER_PRI_ANY);
	ifaddr_event = EVENTHANDLER_REGISTER(
	    rt_addrmsg, rtnl_handle_ifaddr, NULL,
	    EVENTHANDLER_PRI_ANY);
	iflink_event = EVENTHANDLER_REGISTER(
	    ifnet_link_event, rtnl_handle_iflink, NULL,
	    EVENTHANDLER_PRI_ANY);
	NL_VERIFY_PARSERS(all_parsers);
	rtnl_register_messages(cmd_handlers, NL_ARRAY_LEN(cmd_handlers));
}

void
rtnl_ifaces_destroy(void)
{
	EVENTHANDLER_DEREGISTER(ifnet_arrival_event, ifattach_event);
	EVENTHANDLER_DEREGISTER(ifnet_departure_event, ifdetach_event);
	EVENTHANDLER_DEREGISTER(rt_addrmsg, ifaddr_event);
	EVENTHANDLER_DEREGISTER(ifnet_link_event, iflink_event);
}
