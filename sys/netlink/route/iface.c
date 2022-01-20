/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");
#include "opt_inet.h"
#include "opt_inet6.h"
#include <sys/types.h>
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
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#include <netinet6/scope6_var.h> /* scope deembedding */

#define	DEBUG_MOD_NAME	nl_iface
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_DEBUG);

struct netlink_walkargs {
	struct nl_writer *nw;
	struct nlmsghdr hdr;
	struct nlpcb *so;
	uint32_t fibnum;
	int family;
	int error;
	int count;
	int dumped;
};

static eventhandler_tag ifdetach_event, ifattach_event, ifaddr_event;

static SLIST_HEAD(, nl_cloner) nl_cloners = SLIST_HEAD_INITIALIZER(nl_cloners);

static struct sx rtnl_cloner_lock;
SX_SYSINIT(rtnl_cloner_lock, &rtnl_cloner_lock, "rtnl cloner lock");

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
		get_operstate_ether(ifp, pstate);
		break;
	case IFT_LOOP:
		if (ifp->if_flags & IFF_UP) {
			pstate->ifla_operstate = IF_OPER_UP;
			pstate->ifla_carrier = 1;
		} else
			pstate->ifla_operstate = IF_OPER_DOWN;
		break;
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
        struct in6_addr addr6;

        if (sa == NULL)
                return (true);

        switch (sa->sa_family) {
        case AF_INET:
                addr_len = sizeof(struct in_addr);
                addr_data = &((const struct sockaddr_in *)sa)->sin_addr;
                break;
        case AF_INET6:
                in6_splitscope(&((const struct sockaddr_in6 *)sa)->sin6_addr, &addr6, &addr_len);
                addr_len = sizeof(struct in6_addr);
                addr_data = &addr6;
                break;
        case AF_LINK:
                addr_len = ((const struct sockaddr_dl *)sa)->sdl_alen;
                addr_data = LLADDR_CONST((const struct sockaddr_dl *)sa);
                break;
        default:
                NL_LOG(LOG_DEBUG, "unsupported family: %d, skipping", sa->sa_family);
                return (true);
        }

        return (nlattr_add(nw, attr, addr_len, addr_data));
}

/*
 * Dumps interface state, properties and metrics.
 * @nw: message writer
 * @ifp: target interface
 * @hdr: template header
 *
 * This function is called without epoch and MAY sleep.
 */
static bool
dump_iface(struct nl_writer *nw, struct ifnet *ifp, const struct nlmsghdr *hdr)
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
        ifinfo->ifi_change = 0;

        nlattr_add_string(nw, IFLA_IFNAME, if_name(ifp));

	struct if_state ifs = {};
	get_operstate(ifp, &ifs);

        nlattr_add_u8(nw, IFLA_OPERSTATE, ifs.ifla_operstate);
        nlattr_add_u8(nw, IFLA_CARRIER, ifs.ifla_carrier);

/*
        nlattr_add_u8(nw, IFLA_PROTO_DOWN, val);
        nlattr_add_u8(nw, IFLA_LINKMODE, val);
*/
        if ((ifp->if_addr != NULL)) {
                dump_sa(nw, IFLA_ADDRESS, ifp->if_addr->ifa_addr);
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
	get_stats(nw, ifp);

	uint32_t val = (ifp->if_flags & IFF_PROMISC) != 0;
        nlattr_add_u32(nw, IFLA_PROMISCUITY, val);

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
};

static const struct nlattr_parser nla_p_linfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(ifla_cloner), .cb = nlattr_get_stringn },
	{ .type = IFLA_INFO_DATA, .off = _OUT(ifla_idata), .cb = nlattr_get_nla },
};
NL_DECLARE_ATTR_PARSER(linfo_parser, nla_p_linfo);

static const struct nlattr_parser nla_p_if[] = {
	{ .type = IFLA_IFNAME, .off = _OUT(ifla_ifname), .cb = nlattr_get_string },
	{ .type = IFLA_MTU, .off = _OUT(ifla_mtu), .cb = nlattr_get_uint32 },
	{ .type = IFLA_LINK, .off = _OUT(ifi_index), .cb = nlattr_get_uint32 },
	{ .type = IFLA_LINKINFO, .arg = &linfo_parser, .cb = nlattr_get_nested },
	{ .type = IFLA_GROUP, .off = _OUT(ifla_group), .cb = nlattr_get_string },
	{ .type = IFLA_ALT_IFNAME, .off = _OUT(ifla_ifname), .cb = nlattr_get_string },
};
#undef _IN
#undef _OUT
NL_DECLARE_STRICT_PARSER(ifmsg_parser, struct ifinfomsg, check_ifmsg, nlf_p_if, nla_p_if);

static bool
match_iface(struct nl_parsed_link *attrs, struct ifnet *ifp)
{
	if (attrs->ifi_index != 0 && attrs->ifi_index != ifp->if_index)
		return (false);
	if (attrs->ifi_type != 0 && attrs->ifi_index != ifp->if_type)
		return (false);
	if (attrs->ifla_ifname != NULL && strcmp(attrs->ifla_ifname, if_name(ifp)))
		return (false);
	/* TODO: add group match */

	return (true);
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
		.hdr.nlmsg_flags = hdr->nlmsg_flags | NLM_F_MULTI,
		.hdr.nlmsg_type = NL_RTM_NEWLINK,
	};

	/* Fast track for an interface w/ explicit index match */
	if (attrs.ifi_index != 0) {
		NET_EPOCH_ENTER(et);
		ifp = ifnet_byindex_ref(attrs.ifi_index);
		NET_EPOCH_EXIT(et);
		NLP_LOG(LOG_DEBUG3, nlp, "fast track -> searching index %u", attrs.ifi_index);
		if (ifp != NULL) {
			if (match_iface(&attrs, ifp)) {
				if (!dump_iface(wa.nw, ifp, &wa.hdr))
					error = ENOMEM;
			} else
				error = ESRCH;
			if_rele(ifp);
		} else
			error = ESRCH;
		return (error);
	}

	/*
	 * Fetching some link properties require performing ioctl's that may be blocking.
	 * Address it by saving referenced pointers of the matching links,
	 * exiting from epoch and going through the list one-by-one.
	 */

	NL_LOG(LOG_DEBUG2, "Start dump");

	struct ifnet **match_array;
	int offset = 0, base_count = 16; /* start with 128 bytes */
	match_array = malloc(base_count * sizeof(void *), M_TEMP, M_NOWAIT);

	NLP_LOG(LOG_DEBUG3, nlp, "MATCHING: index=%u type=%d name=%s",
	    attrs.ifi_index, attrs.ifi_type, attrs.ifla_ifname);
	NET_EPOCH_ENTER(et);
        CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		wa.count++;
		if (match_iface(&attrs, ifp)) {
			if (offset < base_count) {
				if (!if_try_ref(ifp))
					continue;
				match_array[offset++] = ifp;
				continue;
			}
			/* Too many matches, need to reallocate */
			struct ifnet **new_array;
			int sz = base_count * sizeof(void *);
			base_count *= 2;
			new_array = malloc(sz * 2, M_TEMP, M_NOWAIT);
			if (new_array == NULL) {
				error = ENOMEM;
				break;
			}
			memcpy(new_array, match_array, sz);
			free(match_array, M_TEMP);
			match_array = new_array;
                }
        }
	NET_EPOCH_EXIT(et);

	NL_LOG(LOG_DEBUG2, "Matched %d interface(s), dumping", offset);
	for (int i = 0; error == 0 && i < offset; i++) {
		if (!dump_iface(wa.nw, match_array[i], &wa.hdr))
			error = ENOMEM;
	}
	for (int i = 0; i < offset; i++)
		if_rele(match_array[i]);
	free(match_array, M_TEMP);

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

static int
rtnl_handle_newlink(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct nl_cloner *cloner;
	int error;

	struct nl_parsed_link attrs = {};
	error = nl_parse_nlmsg(hdr, &ifmsg_parser, npt, &attrs);
	if (error != 0)
		return (error);

	if (attrs.ifla_ifname == NULL || strlen(attrs.ifla_ifname) == 0) {
		/* Applications like ip(8) verify RTM_NEWLINK existance
		 * by calling it with empty arguments. Always return "innocent"
		 * error.
		 */
		NLMSG_REPORT_ERR_MSG(npt, "empty IFLA_IFNAME attribute");
		return (EPERM);
	}

	if (attrs.ifla_cloner == NULL || strlen(attrs.ifla_cloner) == 0) {
		NLMSG_REPORT_ERR_MSG(npt, "empty IFLA_INFO_KIND attribute");
		return (EINVAL);
	}

	sx_slock(&rtnl_cloner_lock);
	SLIST_FOREACH(cloner, &nl_cloners, next) {
		if (!strcmp(attrs.ifla_cloner, cloner->name)) {
			error = cloner->create_f(&attrs, nlp, npt);
			sx_sunlock(&rtnl_cloner_lock);
			return (error);
		}
	}
	sx_sunlock(&rtnl_cloner_lock);

	/* TODO: load cloner module if not exists & privilege permits */
	NLMSG_REPORT_ERR_MSG(npt, "interface type %s not supported", attrs.ifla_cloner);
	return (ENOTSUP);

	return (error);
}

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
        }

        return (addr_scope);
}

static uint8_t
inet6_get_plen(const struct in6_addr *addr)
{

	return (bitcount32(addr->s6_addr32[0]) + bitcount32(addr->s6_addr32[1]) +
	    bitcount32(addr->s6_addr32[2]) + bitcount32(addr->s6_addr32[3]));
}

static uint8_t
get_sa_plen(const struct sockaddr *sa)
{
        const struct in6_addr *paddr6;
        const struct in_addr *paddr;

        switch (sa->sa_family) {
        case AF_INET:
                if (sa == NULL)
                        return (32);
                paddr = &(((const struct sockaddr_in *)sa)->sin_addr);
                return bitcount32(paddr->s_addr);;
        case AF_INET6:
                if (sa == NULL)
                        return (128);
                paddr6 = &(((const struct sockaddr_in6 *)sa)->sin6_addr);
                return inet6_get_plen(paddr6);
        }

        return (0);
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

        struct sockaddr *dst_sa = ifa->ifa_dstaddr;
        if ((dst_sa == NULL) || (dst_sa->sa_family != sa->sa_family))
                dst_sa = sa;
        dump_sa(nw, IFA_ADDRESS, dst_sa);
        dump_sa(nw, IFA_LOCAL, sa);
        nlattr_add_string(nw, IFA_LABEL, if_name(ifp));

        uint32_t val = 0; // ifa->ifa_flags;
        nlattr_add_u32(nw, IFA_FLAGS, val);

	if (nlmsg_end(nw))
		return (true);
enomem:
        NL_LOG(LOG_DEBUG, "Failed to dump ifa type %s(%d) for interface %s",
            rib_print_family(sa->sa_family), sa->sa_family, if_name(ifp));
        nlmsg_abort(nw);
        return (false);
}

static int
rtnl_handle_getaddr(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_pstate *npt)
{
        struct ifaddr *ifa;
        struct ifnet *ifp;
	int error = 0;

	struct netlink_walkargs wa = {
		.so = nlp,
		.nw = npt->nw,
		.hdr.nlmsg_pid = hdr->nlmsg_pid,
		.hdr.nlmsg_seq = hdr->nlmsg_seq,
		.hdr.nlmsg_flags = hdr->nlmsg_flags | NLM_F_MULTI,
		.hdr.nlmsg_type = NL_RTM_NEWADDR,
	};

	NL_LOG(LOG_DEBUG2, "Start dump");

        CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
                CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
                        if (wa.family != 0 && wa.family != ifa->ifa_addr->sa_family)
                                continue;
                        if (ifa->ifa_addr->sa_family == AF_LINK)
                                continue;
			wa.count++;
                        if (!dump_iface_addr(wa.nw, ifp, ifa, &wa.hdr)) {
                                error = ENOMEM;
                                break;
                        }
			wa.dumped++;
                }
                if (error != 0)
                        break;
        }

	NL_LOG(LOG_DEBUG2, "End dump, iterated %d dumped %d", wa.count, wa.dumped);

	if (!nlmsg_end_dump(wa.nw, error, &wa.hdr)) {
                NL_LOG(LOG_DEBUG, "Unable to finalize the dump");
                return (ENOMEM);
        }

	return (error);
}

static void
rtnl_handle_ifaddr(void *arg __unused, struct ifaddr *ifa, int cmd)
{
	struct nlmsghdr hdr = {};
	struct nl_writer nw = {};
	uint32_t group = 0;

	switch (ifa->ifa_addr->sa_family) {
	case AF_INET:
		group = RTNLGRP_IPV4_IFADDR;
		break;
	case AF_INET6:
		group = RTNLGRP_IPV6_IFADDR;
		break;
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
rtnl_handle_ifattach(void *arg, struct ifnet *ifp)
{
	struct nlmsghdr hdr = { .nlmsg_type = NL_RTM_NEWLINK };
	struct nl_writer nw = {};

	if (!nl_has_listeners(NETLINK_ROUTE, RTNLGRP_LINK))
		return;

	if (!nlmsg_get_group_writer(&nw, NLMSG_LARGE, NETLINK_ROUTE, RTNLGRP_LINK)) {
		NL_LOG(LOG_DEBUG, "error allocating mbuf");
		return;
	}
	dump_iface(&nw, ifp, &hdr);
        nlmsg_flush(&nw);
}

static void
rtnl_handle_ifdetach(void *arg, struct ifnet *ifp)
{
	struct nlmsghdr hdr = { .nlmsg_type = NL_RTM_DELLINK };
	struct nl_writer nw = {};

	if (!nl_has_listeners(NETLINK_ROUTE, RTNLGRP_LINK))
		return;

	if (!nlmsg_get_group_writer(&nw, NLMSG_LARGE, NETLINK_ROUTE, RTNLGRP_LINK)) {
		NL_LOG(LOG_DEBUG, "error allocating mbuf");
		return;
	}
	dump_iface(&nw, ifp, &hdr);
        nlmsg_flush(&nw);
}

static const struct rtnl_cmd_handler cmd_handlers[] = {
	{
		.cmd = NL_RTM_GETLINK,
		.name = "RTM_GETLINK",
		.cb = &rtnl_handle_getlink,
		.flags = RTNL_F_NOEPOCH,
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
	},
	{
		.cmd = NL_RTM_NEWADDR,
		.name = "RTM_NEWADDR",
		.cb = &rtnl_handle_getaddr,
	},
	{
		.cmd = NL_RTM_DELADDR,
		.name = "RTM_DELADDR",
		.cb = &rtnl_handle_getaddr,
	},
};

static const struct nlhdr_parser *all_parsers[] = { &ifmsg_parser };

void
rtnl_iface_add_cloner(struct nl_cloner *cloner)
{
	sx_xlock(&rtnl_cloner_lock);
	SLIST_INSERT_HEAD(&nl_cloners, cloner, next);
	sx_xunlock(&rtnl_cloner_lock);
}

void rtnl_iface_del_cloner(struct nl_cloner *cloner)
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
	NL_VERIFY_PARSERS(all_parsers);
	rtnl_iface_drivers_register();
	rtnl_register_messages(cmd_handlers, NL_ARRAY_LEN(cmd_handlers));
}

void
rtnl_ifaces_destroy(void)
{
	EVENTHANDLER_DEREGISTER(ifnet_arrival_event, ifattach_event);
	EVENTHANDLER_DEREGISTER(ifnet_departure_event, ifdetach_event);
	EVENTHANDLER_DEREGISTER(rt_addrmsg, ifaddr_event);
}
