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
_DECLARE_DEBUG(LOG_DEBUG);

/*
 *
 * {len=76, type=RTM_NEWLINK, flags=NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL|NLM_F_CREATE, seq=1662892737, pid=0},
 *  {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0},
 *   [
 *    {{nla_len=8, nla_type=IFLA_LINK}, 2},
 *    {{nla_len=12, nla_type=IFLA_IFNAME}, "xvlan22"},
 *    {{nla_len=24, nla_type=IFLA_LINKINFO},
 *     [
 *      {{nla_len=8, nla_type=IFLA_INFO_KIND}, "vlan"...},
 *      {{nla_len=12, nla_type=IFLA_INFO_DATA}, "\x06\x00\x01\x00\x16\x00\x00\x00"}]}]}, iov_len=76}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 76
 */

struct nl_parsed_vlan {
	uint16_t vlan_id;
	uint16_t vlan_proto;
	struct ifla_vlan_flags vlan_flags;
};

#define	_OUT(_field)	offsetof(struct nl_parsed_vlan, _field)
static const struct nlattr_parser nla_p_vlan[] = {
	{ .type = IFLA_VLAN_ID, .off = _OUT(vlan_id), .cb = nlattr_get_uint16 },
	{ .type = IFLA_VLAN_FLAGS, .off = _OUT(vlan_flags), .cb = nlattr_get_nla },
	{ .type = IFLA_VLAN_PROTOCOL, .off = _OUT(vlan_proto), .cb = nlattr_get_uint16 },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(vlan_parser, nla_p_vlan);

static int
create_vlan(struct nl_parsed_link *lattrs, struct nlpcb *nlp, struct nl_pstate *npt)
{
	struct epoch_tracker et;
        struct ifnet *ifp;
	int error;

	/*
	 * lattrs.ifla_ifname is the new interface name
	 * lattrs.ifi_index contains parent interface index
	 * lattrs.ifla_idata contains un-parsed vlan data
	 */

	struct nl_parsed_vlan attrs = {
		.vlan_id = 0xFEFE,
		.vlan_proto = ETHERTYPE_VLAN
	};
	NLP_LOG(LOG_DEBUG3, nlp, "nested: %p len %d", lattrs->ifla_idata, lattrs->ifla_idata->nla_len);

	if (lattrs->ifla_idata == NULL) {
		NLMSG_REPORT_ERR_MSG(npt, "vlan id is required, guessing not supported");
		return (ENOTSUP);
	}

	error = nl_parse_nested(lattrs->ifla_idata, &vlan_parser, npt, &attrs);
	if (error != 0)
		return (error);
	if (attrs.vlan_id > 4095) {
		NLMSG_REPORT_ERR_MSG(npt, "Invalid VID: %d", attrs.vlan_id);
		return (EINVAL);
	}
	if (attrs.vlan_proto != ETHERTYPE_VLAN && attrs.vlan_proto != ETHERTYPE_QINQ) {
		NLMSG_REPORT_ERR_MSG(npt, "Unsupported ethertype: 0x%04X", attrs.vlan_proto);
		return (ENOTSUP);
	}

	NET_EPOCH_ENTER(et);
	ifp = ifnet_byindex_ref(lattrs->ifi_index);
	NET_EPOCH_EXIT(et);
	if (ifp == NULL) {
		NLP_LOG(LOG_DEBUG, nlp, "unable to find parent interface %u",
		    lattrs->ifi_index);
		return (ENOENT);
	}

	/* Waiting till if_clone changes lands */
/*
	struct vlanreq params = {
		.vlr_tag = attrs.vlan_id,
		.vlr_proto = attrs.vlan_proto,
	};
*/
	int ifname_len = strlen(lattrs->ifla_ifname) + 1;
	error = if_clone_create(lattrs->ifla_ifname, ifname_len, (char *)NULL);

	NLP_LOG(LOG_DEBUG2, nlp, "clone for %s returned %d", lattrs->ifla_ifname, error);

	if_rele(ifp);
	return (error);
}

static struct nl_cloner vlan_cloner = {
	.name = "vlan",
	.create_f = create_vlan,

};

static const struct nlhdr_parser *all_parsers[] = { &vlan_parser };

void
rtnl_iface_drivers_register(void)
{
	rtnl_iface_add_cloner(&vlan_cloner);
	NL_VERIFY_PARSERS(all_parsers);
}


