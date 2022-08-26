/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Alexander V. Chernikov
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
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/ck.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_linux.h>
#include <netlink/netlink_route.h>

#include <compat/linux/linux.h>
#include <compat/linux/linux_common.h>
#include <compat/linux/linux_util.h>

#define	DEBUG_MOD_NAME	nl_linux
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_DEBUG);

static bool
valid_rta_size(const struct rtattr *rta, int sz)
{
	return (NL_RTA_DATA_LEN(rta) == sz);
}

static bool
valid_rta_u32(const struct rtattr *rta)
{
	return (valid_rta_size(rta, sizeof(uint32_t)));
}

static uint32_t
_rta_get_uint32(const struct rtattr *rta)
{
	return (*((const uint32_t *)NL_RTA_DATA_CONST(rta)));
}

static struct nlmsghdr *
rtnl_neigh_from_linux(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct ndmsg *ndm = (struct ndmsg *)(hdr + 1);

	if (hdr->nlmsg_len >= sizeof(struct nlmsghdr) + sizeof(struct ndmsg))
		ndm->ndm_family = linux_to_bsd_domain(ndm->ndm_family);

	return (hdr);
}

static struct nlmsghdr *
rtnl_ifaddr_from_linux(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	struct ifaddrmsg *ifam = (struct ifaddrmsg *)(hdr + 1);

	if (hdr->nlmsg_len >= sizeof(struct nlmsghdr) + sizeof(struct ifaddrmsg))
		ifam->ifa_family = linux_to_bsd_domain(ifam->ifa_family);

	return (hdr);
}

static struct nlmsghdr *
rtnl_route_from_linux(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	/* Tweak address families and default fib only */
	struct rtmsg *rtm = (struct rtmsg *)(hdr + 1);
	struct nlattr *nla, *nla_head;
	int attrs_len;

	rtm->rtm_family = linux_to_bsd_domain(rtm->rtm_family);

	if (rtm->rtm_table == 254)
		rtm->rtm_table = 0;

	attrs_len = hdr->nlmsg_len - sizeof(struct nlmsghdr);
	attrs_len -= NETLINK_ALIGN(sizeof(struct rtmsg));
	nla_head = (struct nlattr *)((char *)rtm + NETLINK_ALIGN(sizeof(struct rtmsg)));

	NLA_FOREACH(nla, nla_head, attrs_len) {
		RT_LOG(LOG_DEBUG3, "GOT type %d len %d total %d",
		    nla->nla_type, nla->nla_len, attrs_len);
		struct rtattr *rta = (struct rtattr *)nla;
		if (rta->rta_len < sizeof(struct rtattr)) {
			break;
		}
		switch (rta->rta_type) {
		case NL_RTA_TABLE:
			if (!valid_rta_u32(rta))
				goto done;
			rtm->rtm_table = 0;
			uint32_t fibnum = _rta_get_uint32(rta);
			RT_LOG(LOG_DEBUG3, "GET RTABLE: %u", fibnum);
			if (fibnum == 254) {
				*((uint32_t *)NL_RTA_DATA(rta)) = 0;
			}
			break;
		}
	}

done:
	return (hdr);
}

static struct nlmsghdr *
rtnl_from_linux(struct nlmsghdr *hdr, struct nl_pstate *npt)
{
	switch (hdr->nlmsg_type) {
	case NL_RTM_GETROUTE:
	case NL_RTM_NEWROUTE:
	case NL_RTM_DELROUTE:
		return (rtnl_route_from_linux(hdr, npt));
	case NL_RTM_GETNEIGH:
		return (rtnl_neigh_from_linux(hdr, npt));
	case NL_RTM_GETADDR:
		return (rtnl_ifaddr_from_linux(hdr, npt));
	/* Silence warning for the messages where no translation is required */
	case NL_RTM_NEWLINK:
	case NL_RTM_DELLINK:
	case NL_RTM_GETLINK:
		break;
	default:
		RT_LOG(LOG_DEBUG, "Passing message type %d untranslated",
		    hdr->nlmsg_type);
	}

	return (hdr);
}

static struct nlmsghdr *
nlmsg_from_linux(int netlink_family, struct nlmsghdr *hdr,
    struct nl_pstate *npt)
{
	switch (netlink_family) {
	case NETLINK_ROUTE:
		return (rtnl_from_linux(hdr, npt));
	}

	return (hdr);
}


/************************************************************
 * Kernel -> Linux
 ************************************************************/

static bool
handle_default_out(struct nlmsghdr *hdr, struct nl_writer *nw)
{
	char *out_hdr;
	out_hdr = nlmsg_reserve_data(nw, NLMSG_ALIGN(hdr->nlmsg_len), char);

	if (out_hdr != NULL) {
		memcpy(out_hdr, hdr, hdr->nlmsg_len);
		return (true);
	}
	return (false);
}

static bool
nlmsg_copy_header(struct nlmsghdr *hdr, struct nl_writer *nw)
{
	return (nlmsg_add(nw, hdr->nlmsg_pid, hdr->nlmsg_seq, hdr->nlmsg_type,
	    hdr->nlmsg_flags, 0));
}

static void *
_nlmsg_copy_next_header(struct nlmsghdr *hdr, struct nl_writer *nw, int sz)
{
	void *next_hdr = nlmsg_reserve_data(nw, sz, void);
	memcpy(next_hdr, hdr + 1, NLMSG_ALIGN(sz));

	return (next_hdr);
}
#define	nlmsg_copy_next_header(_hdr, _ns, _t)	\
	((_t *)(_nlmsg_copy_next_header(_hdr, _ns, sizeof(_t))))

static bool
nlmsg_copy_nla(const struct nlattr *nla_orig, struct nl_writer *nw)
{
	struct nlattr *nla = nlmsg_reserve_data(nw, nla_orig->nla_len, struct nlattr);
	if (nla != NULL) {
		memcpy(nla, nla_orig, nla_orig->nla_len);
		return (true);
	}
	return (false);
}

static bool
nlmsg_copy_all_nla(struct nlmsghdr *hdr, int raw_hdrlen, struct nl_writer *nw)
{
	struct nlattr *nla;

	int hdrlen = NETLINK_ALIGN(raw_hdrlen);
	int attrs_len = hdr->nlmsg_len - sizeof(struct nlmsghdr) - hdrlen;
	struct nlattr *nla_head = (struct nlattr *)((char *)(hdr + 1) + hdrlen);

	NLA_FOREACH(nla, nla_head, attrs_len) {
		RT_LOG(LOG_DEBUG3, "reading attr %d len %d", nla->nla_type, nla->nla_len);
		if (nla->nla_len < sizeof(struct nlattr)) {
			return (false);
		}
		if (!nlmsg_copy_nla(nla, nw))
			return (false);
	}
	return (true);
}

static unsigned int
rtnl_if_flags_to_linux(unsigned int if_flags)
{
	unsigned int result = 0;

	for (int i = 0; i < 31; i++) {
		unsigned int flag = 1 << i;
		if (!(flag & if_flags))
			continue;
		switch (flag) {
		case IFF_UP:
		case IFF_BROADCAST:
		case IFF_DEBUG:
		case IFF_LOOPBACK:
		case IFF_POINTOPOINT:
		case IFF_DRV_RUNNING:
		case IFF_NOARP:
		case IFF_PROMISC:
		case IFF_ALLMULTI:
			result |= flag;
			break;
		case IFF_KNOWSEPOCH:
		case IFF_DRV_OACTIVE:
		case IFF_SIMPLEX:
		case IFF_LINK0:
		case IFF_LINK1:
		case IFF_LINK2:
		case IFF_CANTCONFIG:
		case IFF_PPROMISC:
		case IFF_MONITOR:
		case IFF_STATICARP:
		case IFF_STICKYARP:
		case IFF_DYING:
		case IFF_RENAMING:
		case IFF_NOGROUP:
			/* No Linux analogue */
			break;
		case IFF_MULTICAST:
			result |= 1 << 12;
		}
	}
	return (result);
}

static bool
rtnl_newlink_to_linux(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_writer *nw)
{
	if (!nlmsg_copy_header(hdr, nw))
		return (false);

	struct ifinfomsg *ifinfo;
	ifinfo = nlmsg_copy_next_header(hdr, nw, struct ifinfomsg);

	ifinfo->ifi_family = bsd_to_linux_domain(ifinfo->ifi_family);
	/* Convert interface type */
	switch (ifinfo->ifi_type) {
	case IFT_ETHER:
		ifinfo->ifi_type = 1; // ARPHRD_ETHER
		break;
	}
	ifinfo->ifi_flags = rtnl_if_flags_to_linux(ifinfo->ifi_flags);

	/* Copy attributes unchanged */
	if (!nlmsg_copy_all_nla(hdr, sizeof(struct ifinfomsg), nw))
		return (false);

	/* make ip(8) happy */
	if (!nlattr_add_string(nw, IFLA_QDISC, "noqueue"))
		return (false);

	if (!nlattr_add_u32(nw, IFLA_TXQLEN, 1000))
		return (false);

	nlmsg_end(nw);
	RT_LOG(LOG_DEBUG2, "done processing nw %p", nw);
	return (true);
}

static bool
rtnl_newaddr_to_linux(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_writer *nw)
{
	if (!nlmsg_copy_header(hdr, nw))
		return (false);

	struct ifaddrmsg *ifamsg;
	ifamsg = nlmsg_copy_next_header(hdr, nw, struct ifaddrmsg);

	ifamsg->ifa_family = bsd_to_linux_domain(ifamsg->ifa_family);
	/* XXX: fake ifa_flags? */

	/* Copy attributes unchanged */
	if (!nlmsg_copy_all_nla(hdr, sizeof(struct ifaddrmsg), nw))
		return (false);

	nlmsg_end(nw);
	RT_LOG(LOG_DEBUG2, "done processing nw %p", nw);
	return (true);
}

static bool
rtnl_newneigh_to_linux(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_writer *nw)
{
	if (!nlmsg_copy_header(hdr, nw))
		return (false);

	struct ndmsg *ndm;
	ndm = nlmsg_copy_next_header(hdr, nw, struct ndmsg);

	ndm->ndm_family = bsd_to_linux_domain(ndm->ndm_family);

	/* Copy attributes unchanged */
	if (!nlmsg_copy_all_nla(hdr, sizeof(struct ndmsg), nw))
		return (false);

	nlmsg_end(nw);
	RT_LOG(LOG_DEBUG2, "done processing nw %p", nw);
	return (true);
}

static bool
rtnl_newroute_to_linux(struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_writer *nw)
{
	if (!nlmsg_copy_header(hdr, nw))
		return (false);

	struct rtmsg *rtm;
	rtm = nlmsg_copy_next_header(hdr, nw, struct rtmsg);
	rtm->rtm_family = bsd_to_linux_domain(rtm->rtm_family);

	struct nlattr *nla;

	int hdrlen = NETLINK_ALIGN(sizeof(struct rtmsg));
	int attrs_len = hdr->nlmsg_len - sizeof(struct nlmsghdr) - hdrlen;
	struct nlattr *nla_head = (struct nlattr *)((char *)(hdr + 1) + hdrlen);

	NLA_FOREACH(nla, nla_head, attrs_len) {
		struct rtattr *rta = (struct rtattr *)nla;
		//RT_LOG(LOG_DEBUG, "READING attr %d len %d", nla->nla_type, nla->nla_len);
		if (rta->rta_len < sizeof(struct rtattr)) {
			break;
		}

		switch (rta->rta_type) {
		case NL_RTA_TABLE:
			{
				uint32_t fibnum;
				fibnum = _rta_get_uint32(rta);
				if (fibnum == 0)
					fibnum = 254;
				RT_LOG(LOG_DEBUG3, "XFIBNUM %u", fibnum);
				if (!nlattr_add_u32(nw, NL_RTA_TABLE, fibnum))
					return (false);
			}
			break;
		default:
			if (!nlmsg_copy_nla(nla, nw))
				return (false);
			break;
		}
	}

	nlmsg_end(nw);
	RT_LOG(LOG_DEBUG2, "done processing nw %p", nw);
	return (true);
}

static bool
rtnl_to_linux(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_writer *nw)
{
	RT_LOG(LOG_DEBUG2, "Got message type %d", hdr->nlmsg_type);

	switch (hdr->nlmsg_type) {
	case NL_RTM_NEWLINK:
	case NL_RTM_DELLINK:
	case NL_RTM_GETLINK:
		return (rtnl_newlink_to_linux(hdr, nlp, nw));
	case NL_RTM_NEWADDR:
	case NL_RTM_DELADDR:
		return (rtnl_newaddr_to_linux(hdr, nlp, nw));
	case NL_RTM_NEWROUTE:
	case NL_RTM_DELROUTE:
		return (rtnl_newroute_to_linux(hdr, nlp, nw));
	case NL_RTM_NEWNEIGH:
	case NL_RTM_DELNEIGH:
	case NL_RTM_GETNEIGH:
		return (rtnl_newneigh_to_linux(hdr, nlp, nw));
	default:
		RT_LOG(LOG_DEBUG, "[WARN] Passing message type %d untranslated",
		    hdr->nlmsg_type);
		return (handle_default_out(hdr, nw));
	}
}

static bool
nlmsg_error_to_linux(struct nlmsghdr *hdr, struct nlpcb *nlp, struct nl_writer *nw)
{
	if (!nlmsg_copy_header(hdr, nw))
		return (false);

	struct nlmsgerr *nlerr;
	nlerr = nlmsg_copy_next_header(hdr, nw, struct nlmsgerr);
	nlerr->error = bsd_to_linux_errno(nlerr->error);

	int copied_len = sizeof(struct nlmsghdr) + sizeof(struct nlmsgerr);
	if (hdr->nlmsg_len == copied_len) {
		nlmsg_end(nw);
		return (true);
	}

	/*
	 * CAP_ACK was not set. Original request needs to be translated.
	 * XXX: implement translation of the original message
	 */
	RT_LOG(LOG_DEBUG, "[WARN] Passing ack message type %d untranslated",
	    nlerr->msg.nlmsg_type);
	char *dst_payload, *src_payload;
	int copy_len = hdr->nlmsg_len - copied_len;
	dst_payload = nlmsg_reserve_data(nw, NLMSG_ALIGN(copy_len), char);

	src_payload = (char *)hdr + copied_len;

	memcpy(dst_payload, src_payload, copy_len);
	nlmsg_end(nw);

	return (true);
}

static bool
nlmsg_to_linux(int netlink_family, struct nlmsghdr *hdr, struct nlpcb *nlp,
    struct nl_writer *nw)
{
	if (hdr->nlmsg_type < NLMSG_MIN_TYPE) {
		switch (hdr->nlmsg_type) {
		case NLMSG_ERROR:
			return (nlmsg_error_to_linux(hdr, nlp, nw));
		case NLMSG_NOOP:
		case NLMSG_DONE:
		case NLMSG_OVERRUN:
			return (handle_default_out(hdr, nw));
		default:
			RT_LOG(LOG_DEBUG, "[WARN] Passing message type %d untranslated",
			    hdr->nlmsg_type);
			return (handle_default_out(hdr, nw));
		}
	}

	switch (netlink_family) {
	case NETLINK_ROUTE:
		return (rtnl_to_linux(hdr, nlp, nw));
	default:
		return (handle_default_out(hdr, nw));
	}
}

static struct mbuf *
nlmsgs_to_linux(int netlink_family, char *buf, int data_length, struct nlpcb *nlp)
{
	RT_LOG(LOG_DEBUG3, "LINUX: get %p size %d", buf, data_length);
	struct nl_writer nw = {};

	struct mbuf *m = NULL;
	if (!nlmsg_get_chain_writer(&nw, data_length, &m)) {
		RT_LOG(LOG_DEBUG, "unable to setup chain writer for size %d",
		    data_length);
		return (NULL);
	}

	/* Assume correct headers. Buffer IS mutable */
	int count = 0;
	for (int offset = 0; offset + sizeof(struct nlmsghdr) <= data_length;) {
		struct nlmsghdr *hdr = (struct nlmsghdr *)&buf[offset];
		int msglen = NLMSG_ALIGN(hdr->nlmsg_len);
		count++;

		if (!nlmsg_to_linux(netlink_family, hdr, nlp, &nw)) {
			RT_LOG(LOG_DEBUG, "failed to process msg type %d",
			    hdr->nlmsg_type);
			m_freem(m);
			return (NULL);
		}
		offset += msglen;
	}
	nlmsg_flush(&nw);
	RT_LOG(LOG_DEBUG3, "Processed %d messages, chain size %d", count,
	    m ? m_length(m, NULL) : 0);

	return (m);
}

static struct mbuf *
mbufs_to_linux(int netlink_family, struct mbuf *m, struct nlpcb *nlp)
{
	/* XXX: easiest solution, not optimized for performance */
	int data_length = m_length(m, NULL);
	char *buf = malloc(data_length, M_LINUX, M_NOWAIT);
	if (buf == NULL) {
		RT_LOG(LOG_DEBUG, "unable to allocate %d bytes, dropping message",
		    data_length);
		m_freem(m);
		return (NULL);
	}
	m_copydata(m, 0, data_length, buf);
	m_freem(m);

	m = nlmsgs_to_linux(netlink_family, buf, data_length, nlp);
	free(buf, M_LINUX);

	return (m);
}

static struct linux_netlink_provider linux_netlink_v1 = {
	.mbufs_to_linux = mbufs_to_linux,
	.msgs_to_linux = nlmsgs_to_linux,
	.msg_from_linux = nlmsg_from_linux,
};

void
linux_netlink_register()
{
	linux_netlink_p = &linux_netlink_v1;
}

void
linux_netlink_deregister()
{
	linux_netlink_p = NULL;
}
