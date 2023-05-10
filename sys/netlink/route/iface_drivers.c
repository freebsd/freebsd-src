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
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/syslog.h>
#include <sys/socketvar.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/if_clone.h>
#include <net/if_vlan_var.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/route/route_ctl.h>
#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#include <netinet6/scope6_var.h> /* scope deembedding */

#define	DEBUG_MOD_NAME	nl_iface_drivers
#define	DEBUG_MAX_LEVEL	LOG_DEBUG3
#include <netlink/netlink_debug.h>
_DECLARE_DEBUG(LOG_INFO);

/*
 * Generic modification interface handler.
 * Responsible for changing network stack interface attributes
 * such as state, mtu or description.
 */
int
_nl_modify_ifp_generic(struct ifnet *ifp, struct nl_parsed_link *lattrs,
    const struct nlattr_bmask *bm, struct nl_pstate *npt)
{
	int error;

	if (lattrs->ifla_ifalias != NULL) {
		if (nlp_has_priv(npt->nlp, PRIV_NET_SETIFDESCR)) {
			int len = strlen(lattrs->ifla_ifalias) + 1;
			char *buf = if_allocdescr(len, M_WAITOK);

			memcpy(buf, lattrs->ifla_ifalias, len);
			if_setdescr(ifp, buf);
			getmicrotime(&ifp->if_lastchange);
		} else {
			nlmsg_report_err_msg(npt, "Not enough privileges to set descr");
			return (EPERM);
		}
	}

	if ((lattrs->ifi_change & IFF_UP) && (lattrs->ifi_flags & IFF_UP) == 0) {
		/* Request to down the interface */
		if_down(ifp);
	}

	if (lattrs->ifla_mtu > 0) {
		if (nlp_has_priv(npt->nlp, PRIV_NET_SETIFMTU)) {
			struct ifreq ifr = { .ifr_mtu = lattrs->ifla_mtu };
			error = ifhwioctl(SIOCSIFMTU, ifp, (char *)&ifr, curthread);
		} else {
			nlmsg_report_err_msg(npt, "Not enough privileges to set mtu");
			return (EPERM);
		}
	}

	if (lattrs->ifi_change & IFF_PROMISC) {
		error = ifpromisc(ifp, lattrs->ifi_flags & IFF_PROMISC);
		if (error != 0) {
			nlmsg_report_err_msg(npt, "unable to set promisc");
			return (error);
		}
	}

	return (0);
}

/*
 * Saves the resulting ifindex and ifname to report them
 *  to userland along with the operation result.
 * NLA format:
 * NLMSGERR_ATTR_COOKIE(nested)
 *  IFLA_NEW_IFINDEX(u32)
 *  IFLA_IFNAME(string)
 */
void
_nl_store_ifp_cookie(struct nl_pstate *npt, struct ifnet *ifp)
{
	int ifname_len = strlen(if_name(ifp));
	uint32_t ifindex = (uint32_t)ifp->if_index;

	int nla_len = sizeof(struct nlattr) * 3 +
		sizeof(ifindex) + NL_ITEM_ALIGN(ifname_len + 1);
	struct nlattr *nla_cookie = npt_alloc(npt, nla_len);

	/* Nested TLV */
	nla_cookie->nla_len = nla_len;
	nla_cookie->nla_type = NLMSGERR_ATTR_COOKIE;

	struct nlattr *nla = nla_cookie + 1;
	nla->nla_len = sizeof(struct nlattr) + sizeof(ifindex);
	nla->nla_type = IFLA_NEW_IFINDEX;
	memcpy(NLA_DATA(nla), &ifindex, sizeof(ifindex));

	nla = NLA_NEXT(nla);
	nla->nla_len = sizeof(struct nlattr) + ifname_len + 1;
	nla->nla_type = IFLA_IFNAME;
	strlcpy(NLA_DATA(nla), if_name(ifp), ifname_len + 1);

	nlmsg_report_cookie(npt, nla_cookie);
}

