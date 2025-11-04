/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_bridgevar.h>
#include <net/route.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include <libifconfig.h>

#include "ifconfig.h"

static int	parse_vlans(ifbvlan_set_t *set, const char *str);
static int	get_val(const char *cp, u_long *valp);
static int	get_vlan_id(const char *cp, ether_vlanid_t *valp);

static const char *stpstates[] = { STP_STATES };
static const char *stpproto[] = { STP_PROTOS };
static const char *stproles[] = { STP_ROLES };

static int
get_val(const char *cp, u_long *valp)
{
	char *endptr;
	u_long val;

	errno = 0;
	val = strtoul(cp, &endptr, 0);
	if (cp[0] == '\0' || endptr[0] != '\0' || errno == ERANGE)
		return (-1);

	*valp = val;
	return (0);
}

static int
get_vlan_id(const char *cp, ether_vlanid_t *valp)
{
	u_long val;

	if (get_val(cp, &val) == -1)
		return (-1);
	if (val < DOT1Q_VID_MIN || val > DOT1Q_VID_MAX)
		return (-1);

	*valp = (ether_vlanid_t)val;
	return (0);
}

static int
do_cmd(if_ctx *ctx, u_long op, void *arg, size_t argsize, int set)
{
	struct ifdrv ifd = {};

	strlcpy(ifd.ifd_name, ctx->ifname, sizeof(ifd.ifd_name));
	ifd.ifd_cmd = op;
	ifd.ifd_len = argsize;
	ifd.ifd_data = arg;

	return (ioctl_ctx(ctx, set ? SIOCSDRVSPEC : SIOCGDRVSPEC, &ifd));
}

static void
do_bridgeflag(if_ctx *ctx, const char *ifs, int flag, int set)
{
	struct ifbreq req;

	strlcpy(req.ifbr_ifsname, ifs, sizeof(req.ifbr_ifsname));

	if (do_cmd(ctx, BRDGGIFFLGS, &req, sizeof(req), 0) < 0)
		err(1, "unable to get bridge flags");

	if (set)
		req.ifbr_ifsflags |= flag;
	else
		req.ifbr_ifsflags &= ~flag;

	if (do_cmd(ctx, BRDGSIFFLGS, &req, sizeof(req), 1) < 0)
		err(1, "unable to set bridge flags");
}

static void
bridge_addresses(if_ctx *ctx, const char *prefix)
{
	struct ifbaconf ifbac;
	struct ifbareq *ifba;
	char *inbuf = NULL, *ninbuf;
	size_t len = 8192;
	struct ether_addr ea;

	for (;;) {
		ninbuf = realloc(inbuf, len);
		if (ninbuf == NULL)
			err(1, "unable to allocate address buffer");
		ifbac.ifbac_len = len;
		ifbac.ifbac_buf = inbuf = ninbuf;
		if (do_cmd(ctx, BRDGRTS, &ifbac, sizeof(ifbac), 0) < 0)
			err(1, "unable to get address cache");
		if ((ifbac.ifbac_len + sizeof(*ifba)) < len)
			break;
		len *= 2;
	}

	for (unsigned long i = 0; i < ifbac.ifbac_len / sizeof(*ifba); i++) {
		ifba = ifbac.ifbac_req + i;
		memcpy(ea.octet, ifba->ifba_dst,
		    sizeof(ea.octet));
		printf("%s%s Vlan%d %s %lu ", prefix, ether_ntoa(&ea),
		    ifba->ifba_vlan, ifba->ifba_ifsname, ifba->ifba_expire);
		printb("flags", ifba->ifba_flags, IFBAFBITS);
		printf("\n");
	}

	free(inbuf);
}

static void
print_vlans(ifbvlan_set_t *vlans)
{
	unsigned printed = 0;

	for (unsigned vlan = DOT1Q_VID_MIN; vlan <= DOT1Q_VID_MAX;) {
		unsigned last;

		if (!BRVLAN_TEST(vlans, vlan)) {
			++vlan;
			continue;
		}

		last = vlan;
		while (last < DOT1Q_VID_MAX && BRVLAN_TEST(vlans, last + 1))
			++last;

		if (printed == 0)
			printf(" tagged ");
		else
			printf(",");

		printf("%u", vlan);
		if (last != vlan)
			printf("-%u", last);
		++printed;
		vlan = last + 1;
	}
}

static char const *
vlan_proto_name(uint16_t proto)
{
	switch (proto) {
	case 0:
		return "none";
	case ETHERTYPE_VLAN:
		return "802.1q";
	case ETHERTYPE_QINQ:
		return "802.1ad";
	default:
		return "unknown";
	}
}

static void
bridge_status(if_ctx *ctx)
{
	struct ifconfig_bridge_status *bridge;
	struct ifbropreq *params;
	const char *pad, *prefix;
	uint8_t lladdr[ETHER_ADDR_LEN];
	uint16_t bprio;

	if (ifconfig_bridge_get_bridge_status(lifh, ctx->ifname, &bridge) == -1)
		return;

	params = bridge->params;

	PV2ID(params->ifbop_bridgeid, bprio, lladdr);
	printf("\tid %s priority %u hellotime %u fwddelay %u\n",
	    ether_ntoa((struct ether_addr *)lladdr),
	    params->ifbop_priority,
	    params->ifbop_hellotime,
	    params->ifbop_fwddelay);
	printf("\tmaxage %u holdcnt %u proto %s maxaddr %u timeout %u\n",
	    params->ifbop_maxage,
	    params->ifbop_holdcount,
	    stpproto[params->ifbop_protocol],
	    bridge->cache_size,
	    bridge->cache_lifetime);
	PV2ID(params->ifbop_designated_root, bprio, lladdr);
	printf("\troot id %s priority %d ifcost %u port %u\n",
	    ether_ntoa((struct ether_addr *)lladdr),
	    bprio,
	    params->ifbop_root_path_cost,
	    params->ifbop_root_port & 0xfff);

	printb("\tbridge flags", bridge->flags, IFBRFBITS);
	if (bridge->defpvid)
		printf(" defuntagged=%u", (unsigned) bridge->defpvid);
	printf("\n");

	prefix = "\tmember: ";
	pad    = "\t        ";
	for (size_t i = 0; i < bridge->members_count; ++i) {
		struct ifbreq *member = &bridge->members[i];

		printf("%s%s ", prefix, member->ifbr_ifsname);
		printb("flags", member->ifbr_ifsflags, IFBIFBITS);
		printf("\n%s", pad);
		if (member->ifbr_addrmax != 0)
			printf("ifmaxaddr %u ", member->ifbr_addrmax);
		printf("port %u priority %u path cost %u",
		    member->ifbr_portno,
		    member->ifbr_priority,
		    member->ifbr_path_cost);
		if (member->ifbr_ifsflags & IFBIF_STP) {
			uint8_t proto = member->ifbr_proto;
			uint8_t role = member->ifbr_role;
			uint8_t state = member->ifbr_state;

			if (proto < nitems(stpproto))
				printf(" proto %s", stpproto[proto]);
			else
				printf(" <unknown proto %d>", proto);
			printf("\n%s", pad);
			if (role < nitems(stproles))
				printf("role %s", stproles[role]);
			else
				printf("<unknown role %d>", role);
			if (state < nitems(stpstates))
				printf(" state %s", stpstates[state]);
			else
				printf(" <unknown state %d>", state);
		}
		if (member->ifbr_vlanproto != 0)
			printf(" vlan protocol %s",
			    vlan_proto_name(member->ifbr_vlanproto));
		if (member->ifbr_pvid != 0)
			printf(" untagged %u", (unsigned)member->ifbr_pvid);
		print_vlans(&bridge->member_vlans[i]);
		printf("\n");
	}

	ifconfig_bridge_free_bridge_status(bridge);
}

static int
setbridge_add(if_ctx *ctx, int argc, const char *const *argv)
{
	struct ifbreq req;
	struct ifbif_vlan_req vlreq;
	int oargc = argc;

	memset(&req, 0, sizeof(req));
	memset(&vlreq, 0, sizeof(vlreq));

	if (argc < 1)
		errx(1, "usage: addm <interface> [opts ...]");

	strlcpy(req.ifbr_ifsname, argv[0], sizeof(req.ifbr_ifsname));
	--argc; ++argv;

	while (argc) {
		if (strcmp(argv[0], "untagged") == 0) {
			if (argc < 2)
				errx(1, "usage: untagged <vlan id>");

			if (get_vlan_id(argv[1], &req.ifbr_pvid) < 0)
				errx(1, "invalid VLAN identifier: %s", argv[1]);

			argc -= 2;
			argv += 2;
		} else if (strcmp(argv[0], "tagged") == 0) {
			if (argc < 2)
				errx(1, "usage: tagged <vlan set>");

			vlreq.bv_op = BRDG_VLAN_OP_SET;
			strlcpy(vlreq.bv_ifname, req.ifbr_ifsname,
			    sizeof(vlreq.bv_ifname));
			if (parse_vlans(&vlreq.bv_set, argv[1]) != 0)
				errx(1, "invalid vlan set: %s", argv[1]);

			argc -= 2;
			argv += 2;
		} else {
			break;
		}
	}

	if (do_cmd(ctx, BRDGADD, &req, sizeof(req), 1) < 0)
		err(1, "BRDGADD %s", req.ifbr_ifsname);

	if (req.ifbr_pvid != 0 &&
	    do_cmd(ctx, BRDGSIFPVID, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFPVID %s %u", req.ifbr_ifsname,
		    (unsigned)req.ifbr_pvid);

	if (vlreq.bv_op != 0 &&
	    do_cmd(ctx, BRDGSIFVLANSET, &vlreq, sizeof(vlreq), 1) < 0)
		err(1, "BRDGSIFVLANSET %s", req.ifbr_ifsname);

	return (oargc - argc);
}

static void
setbridge_delete(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	strlcpy(req.ifbr_ifsname, val, sizeof(req.ifbr_ifsname));
	if (do_cmd(ctx, BRDGDEL, &req, sizeof(req), 1) < 0)
		err(1, "BRDGDEL %s",  val);
}

static void
setbridge_discover(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_DISCOVER, 1);
}

static void
unsetbridge_discover(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_DISCOVER, 0);
}

static void
setbridge_learn(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_LEARNING,  1);
}

static void
unsetbridge_learn(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_LEARNING,  0);
}

static void
setbridge_sticky(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_STICKY,  1);
}

static void
unsetbridge_sticky(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_STICKY,  0);
}

static void
setbridge_span(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	strlcpy(req.ifbr_ifsname, val, sizeof(req.ifbr_ifsname));
	if (do_cmd(ctx, BRDGADDS, &req, sizeof(req), 1) < 0)
		err(1, "BRDGADDS %s",  val);
}

static void
unsetbridge_span(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	strlcpy(req.ifbr_ifsname, val, sizeof(req.ifbr_ifsname));
	if (do_cmd(ctx, BRDGDELS, &req, sizeof(req), 1) < 0)
		err(1, "BRDGDELS %s",  val);
}

static void
setbridge_stp(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_STP, 1);
}

static void
unsetbridge_stp(if_ctx *ctx, const char *val, int dummy __unused)
{

	do_bridgeflag(ctx, val, IFBIF_STP, 0);
}

static void
setbridge_edge(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_EDGE, 1);
}

static void
unsetbridge_edge(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_EDGE, 0);
}

static void
setbridge_autoedge(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_AUTOEDGE, 1);
}

static void
unsetbridge_autoedge(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_AUTOEDGE, 0);
}

static void
setbridge_ptp(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_PTP, 1);
}

static void
unsetbridge_ptp(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_PTP, 0);
}

static void
setbridge_autoptp(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_AUTOPTP, 1);
}

static void
unsetbridge_autoptp(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_BSTP_AUTOPTP, 0);
}

static void
setbridge_flush(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	req.ifbr_ifsflags = IFBF_FLUSHDYN;
	if (do_cmd(ctx, BRDGFLUSH, &req, sizeof(req), 1) < 0)
		err(1, "BRDGFLUSH");
}

static void
setbridge_flushall(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	req.ifbr_ifsflags = IFBF_FLUSHALL;
	if (do_cmd(ctx, BRDGFLUSH, &req, sizeof(req), 1) < 0)
		err(1, "BRDGFLUSH");
}

static int
setbridge_static(if_ctx *ctx, int argc, const char *const *argv)
{
	struct ifbareq req;
	struct ether_addr *ea;
	int arg;

	if (argc < 2)
		errx(1, "usage: static <interface> <address> [vlan <id>]");
	arg = 0;

	memset(&req, 0, sizeof(req));
	req.ifba_flags = IFBAF_STATIC;

	strlcpy(req.ifba_ifsname, argv[arg], sizeof(req.ifba_ifsname));
	++arg;

	ea = ether_aton(argv[arg]);
	if (ea == NULL)
		errx(1, "invalid address: %s", argv[arg]);
	memcpy(req.ifba_dst, ea->octet, sizeof(req.ifba_dst));
	++arg;

	req.ifba_vlan = 0;
	if (argc > 2 && strcmp(argv[arg], "vlan") == 0) {
		if (argc < 3)
			errx(1, "usage: static <interface> <address> "
			    "[vlan <id>]");
		++arg;

		if (get_vlan_id(argv[arg], &req.ifba_vlan) < 0)
			errx(1, "invalid vlan id: %s", argv[arg]);
		++arg;
	}

	if (do_cmd(ctx, BRDGSADDR, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSADDR");
	return arg;
}

static int
setbridge_deladdr(if_ctx *ctx, int argc, const char *const *argv)
{
	struct ifbareq req;
	struct ether_addr *ea;
	int arg;

	if (argc < 1)
		errx(1, "usage: deladdr <address> [vlan <id>]");
	arg = 0;

	memset(&req, 0, sizeof(req));

	ea = ether_aton(argv[arg]);
	if (ea == NULL)
		errx(1, "invalid address: %s", argv[arg]);
	memcpy(req.ifba_dst, ea->octet, sizeof(req.ifba_dst));
	++arg;

	req.ifba_vlan = 0;
	if (argc >= 2 && strcmp(argv[arg], "vlan") == 0) {
		if (argc < 3)
			errx(1, "usage: deladdr <address> [vlan <id>]");
		++arg;

		if (get_vlan_id(argv[arg], &req.ifba_vlan) < 0)
			errx(1, "invalid vlan id: %s", argv[arg]);
		++arg;
	}

	if (do_cmd(ctx, BRDGDADDR, &req, sizeof(req), 1) < 0)
		err(1, "BRDGDADDR");

	return arg;
}

static void
setbridge_addr(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	bridge_addresses(ctx, "");
}

static void
setbridge_maxaddr(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xffffffff) != 0)
		errx(1, "invalid value: %s",  arg);

	param.ifbrp_csize = val & 0xffffffff;

	if (do_cmd(ctx, BRDGSCACHE, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSCACHE %s",  arg);
}

static void
setbridge_hellotime(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xff) != 0)
		errx(1, "invalid value: %s",  arg);

	param.ifbrp_hellotime = val & 0xff;

	if (do_cmd(ctx, BRDGSHT, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSHT %s",  arg);
}

static void
setbridge_fwddelay(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xff) != 0)
		errx(1, "invalid value: %s",  arg);

	param.ifbrp_fwddelay = val & 0xff;

	if (do_cmd(ctx, BRDGSFD, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSFD %s",  arg);
}

static void
setbridge_maxage(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xff) != 0)
		errx(1, "invalid value: %s",  arg);

	param.ifbrp_maxage = val & 0xff;

	if (do_cmd(ctx, BRDGSMA, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSMA %s",  arg);
}

static void
setbridge_priority(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xffff) != 0)
		errx(1, "invalid value: %s",  arg);

	param.ifbrp_prio = val & 0xffff;

	if (do_cmd(ctx, BRDGSPRI, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSPRI %s",  arg);
}

static void
setbridge_protocol(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;

	if (strcasecmp(arg, "stp") == 0) {
		param.ifbrp_proto = 0;
	} else if (strcasecmp(arg, "rstp") == 0) {
		param.ifbrp_proto = 2;
	} else {
		errx(1, "unknown stp protocol");
	}

	if (do_cmd(ctx, BRDGSPROTO, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSPROTO %s",  arg);
}

static void
setbridge_holdcount(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xff) != 0)
		errx(1, "invalid value: %s",  arg);

	param.ifbrp_txhc = val & 0xff;

	if (do_cmd(ctx, BRDGSTXHC, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSTXHC %s",  arg);
}

static void
setbridge_ifpriority(if_ctx *ctx, const char *ifn, const char *pri)
{
	struct ifbreq req;
	u_long val;

	memset(&req, 0, sizeof(req));

	if (get_val(pri, &val) < 0 || (val & ~0xff) != 0)
		errx(1, "invalid value: %s",  pri);

	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	req.ifbr_priority = val & 0xff;

	if (do_cmd(ctx, BRDGSIFPRIO, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFPRIO %s",  pri);
}

static void
setbridge_ifpathcost(if_ctx *ctx, const char *ifn, const char *cost)
{
	struct ifbreq req;
	u_long val;

	memset(&req, 0, sizeof(req));

	if (get_val(cost, &val) < 0)
		errx(1, "invalid value: %s",  cost);

	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	req.ifbr_path_cost = val;

	if (do_cmd(ctx, BRDGSIFCOST, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFCOST %s",  cost);
}

static void
setbridge_ifuntagged(if_ctx *ctx, const char *ifn, const char *vlanid)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));

	if (get_vlan_id(vlanid, &req.ifbr_pvid) < 0)
		errx(1, "invalid VLAN identifier: %s", vlanid);

	if (do_cmd(ctx, BRDGSIFPVID, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFPVID %s", vlanid);
}

static void
unsetbridge_ifuntagged(if_ctx *ctx, const char *ifn, int dummy __unused)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));

	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	req.ifbr_pvid = 0;

	if (do_cmd(ctx, BRDGSIFPVID, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFPVID");
}

static void
setbridge_ifmaxaddr(if_ctx *ctx, const char *ifn, const char *arg)
{
	struct ifbreq req;
	u_long val;

	memset(&req, 0, sizeof(req));

	if (get_val(arg, &val) < 0 || (val & ~0xffffffff) != 0)
		errx(1, "invalid value: %s",  arg);

	strlcpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	req.ifbr_addrmax = val & 0xffffffff;

	if (do_cmd(ctx, BRDGSIFAMAX, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFAMAX %s",  arg);
}

static void
setbridge_timeout(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam param;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xffffffff) != 0)
		errx(1, "invalid value: %s",  arg);

	param.ifbrp_ctime = val & 0xffffffff;

	if (do_cmd(ctx, BRDGSTO, &param, sizeof(param), 1) < 0)
		err(1, "BRDGSTO %s",  arg);
}

static void
setbridge_private(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_PRIVATE, 1);
}

static void
unsetbridge_private(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_PRIVATE, 0);
}

static int
parse_vlans(ifbvlan_set_t *set, const char *str)
{
	char *s, *free_s, *token;

	/* "none" means the empty vlan set */
	if (strcmp(str, "none") == 0) {
		__BIT_ZERO(BRVLAN_SETSIZE, set);
		return (0);
	}

	/* "all" means all vlans, except for 0 and 4095 which are reserved */
	if (strcmp(str, "all") == 0) {
		__BIT_FILL(BRVLAN_SETSIZE, set);
		BRVLAN_CLR(set, DOT1Q_VID_NULL);
		BRVLAN_CLR(set, DOT1Q_VID_RSVD_IMPL);
		return (0);
	}

	if ((s = strdup(str)) == NULL)
		return (-1);
	/* Keep the original value of s, since strsep() will modify it */
	free_s = s;

	while ((token = strsep(&s, ",")) != NULL) {
		unsigned long first, last;
		char *p, *lastp;

		if ((lastp = strchr(token, '-')) != NULL)
			*lastp++ = '\0';

		first = last = strtoul(token, &p, 10);
		if (*p != '\0')
			goto err;
		if (first < DOT1Q_VID_MIN || first > DOT1Q_VID_MAX)
			goto err;

		if (lastp) {
			last = strtoul(lastp, &p, 10);
			if (*p != '\0')
				goto err;
			if (last < DOT1Q_VID_MIN || last > DOT1Q_VID_MAX ||
			    last < first)
				goto err;
		}

		for (unsigned vlan = first; vlan <= last; ++vlan)
			BRVLAN_SET(set, vlan);
	}

	free(free_s);
	return (0);

err:
	free(free_s);
	return (-1);
}

static void
set_bridge_vlanset(if_ctx *ctx, const char *ifn, const char *vlans, int op)
{
	struct ifbif_vlan_req req;

	memset(&req, 0, sizeof(req));

	if (parse_vlans(&req.bv_set, vlans) != 0)
		errx(1, "invalid vlan set: %s", vlans);

	strlcpy(req.bv_ifname, ifn, sizeof(req.bv_ifname));
	req.bv_op = op;

	if (do_cmd(ctx, BRDGSIFVLANSET, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFVLANSET %s", vlans);
}

static void
setbridge_iftagged(if_ctx *ctx, const char *ifn, const char *vlans)
{
	set_bridge_vlanset(ctx, ifn, vlans, BRDG_VLAN_OP_SET);
}

static void
addbridge_iftagged(if_ctx *ctx, const char *ifn, const char *vlans)
{
	set_bridge_vlanset(ctx, ifn, vlans, BRDG_VLAN_OP_ADD);
}

static void
delbridge_iftagged(if_ctx *ctx, const char *ifn, const char *vlans)
{
	set_bridge_vlanset(ctx, ifn, vlans, BRDG_VLAN_OP_DEL);
}

static void
setbridge_flags(if_ctx *ctx, const char *val __unused, int newflags)
{
	struct ifbrparam req;

	if (do_cmd(ctx, BRDGGFLAGS, &req, sizeof(req), 0) < 0)
		err(1, "BRDGGFLAGS");

	req.ifbrp_flags |= (uint32_t)newflags;

	if (do_cmd(ctx, BRDGSFLAGS, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSFLAGS");
}

static void
unsetbridge_flags(if_ctx *ctx, const char *val __unused, int newflags)
{
	struct ifbrparam req;

	if (do_cmd(ctx, BRDGGFLAGS, &req, sizeof(req), 0) < 0)
		err(1, "BRDGGFLAGS");

	req.ifbrp_flags &= ~(uint32_t)newflags;

	if (do_cmd(ctx, BRDGSFLAGS, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSFLAGS");
}

static void
setbridge_defuntagged(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct ifbrparam req;

	memset(&req, 0, sizeof(req));
	if (get_vlan_id(arg, &req.ifbrp_defpvid) < 0)
		errx(1, "invalid vlan id: %s", arg);

	if (do_cmd(ctx, BRDGSDEFPVID, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSDEFPVID");
}

static void
unsetbridge_defuntagged(if_ctx *ctx, const char *val __unused, int dummy __unused)
{
	struct ifbrparam req;

	memset(&req, 0, sizeof(req));
	req.ifbrp_defpvid = 0;

	if (do_cmd(ctx, BRDGSDEFPVID, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSDEFPVID");
}

static void
setbridge_qinq(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_QINQ, 1);
}

static void
unsetbridge_qinq(if_ctx *ctx, const char *val, int dummy __unused)
{
	do_bridgeflag(ctx, val, IFBIF_QINQ, 0);
}

static void
setbridge_ifvlanproto(if_ctx *ctx, const char *ifname, const char *proto)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	strlcpy(req.ifbr_ifsname, ifname, sizeof(req.ifbr_ifsname));

	if (strcmp(proto, "802.1q") == 0)
		req.ifbr_vlanproto = ETHERTYPE_VLAN;
	else if (strcmp(proto, "802.1ad") == 0)
		req.ifbr_vlanproto = ETHERTYPE_QINQ;
	else
		errx(1, "unrecognised VLAN protocol: %s", proto);

	if (do_cmd(ctx, BRDGSIFVLANPROTO, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSIFVLANPROTO");
}

static struct cmd bridge_cmds[] = {
	DEF_CMD_VARG("addm",		setbridge_add),
	DEF_CMD_ARG("deletem",		setbridge_delete),
	DEF_CMD_ARG("discover",		setbridge_discover),
	DEF_CMD_ARG("-discover",	unsetbridge_discover),
	DEF_CMD_ARG("learn",		setbridge_learn),
	DEF_CMD_ARG("-learn",		unsetbridge_learn),
	DEF_CMD_ARG("sticky",		setbridge_sticky),
	DEF_CMD_ARG("-sticky",		unsetbridge_sticky),
	DEF_CMD_ARG("span",		setbridge_span),
	DEF_CMD_ARG("-span",		unsetbridge_span),
	DEF_CMD_ARG("stp",		setbridge_stp),
	DEF_CMD_ARG("-stp",		unsetbridge_stp),
	DEF_CMD_ARG("edge",		setbridge_edge),
	DEF_CMD_ARG("-edge",		unsetbridge_edge),
	DEF_CMD_ARG("autoedge",		setbridge_autoedge),
	DEF_CMD_ARG("-autoedge",	unsetbridge_autoedge),
	DEF_CMD_ARG("ptp",		setbridge_ptp),
	DEF_CMD_ARG("-ptp",		unsetbridge_ptp),
	DEF_CMD_ARG("autoptp",		setbridge_autoptp),
	DEF_CMD_ARG("-autoptp",		unsetbridge_autoptp),
	DEF_CMD("flush", 0,		setbridge_flush),
	DEF_CMD("flushall", 0,		setbridge_flushall),
	DEF_CMD_VARG("static",		setbridge_static),
	DEF_CMD_VARG("deladdr",		setbridge_deladdr),
	DEF_CMD("addr",	 1,		setbridge_addr),
	DEF_CMD_ARG("maxaddr",		setbridge_maxaddr),
	DEF_CMD_ARG("hellotime",	setbridge_hellotime),
	DEF_CMD_ARG("fwddelay",		setbridge_fwddelay),
	DEF_CMD_ARG("maxage",		setbridge_maxage),
	DEF_CMD_ARG("priority",		setbridge_priority),
	DEF_CMD_ARG("proto",		setbridge_protocol),
	DEF_CMD_ARG("holdcnt",		setbridge_holdcount),
	DEF_CMD_ARG2("ifpriority",	setbridge_ifpriority),
	DEF_CMD_ARG2("ifpathcost",	setbridge_ifpathcost),
	DEF_CMD_ARG2("ifmaxaddr",	setbridge_ifmaxaddr),
	DEF_CMD_ARG2("ifuntagged",	setbridge_ifuntagged),
	DEF_CMD_ARG("-ifuntagged",	unsetbridge_ifuntagged),
	DEF_CMD_ARG2("iftagged",	setbridge_iftagged),
	DEF_CMD_ARG2("+iftagged",	addbridge_iftagged),
	DEF_CMD_ARG2("-iftagged",	delbridge_iftagged),
	DEF_CMD_ARG2("ifvlanproto",	setbridge_ifvlanproto),
	DEF_CMD_ARG("timeout",		setbridge_timeout),
	DEF_CMD_ARG("private",		setbridge_private),
	DEF_CMD_ARG("-private",		unsetbridge_private),
	DEF_CMD("vlanfilter", (int32_t)IFBRF_VLANFILTER,
					setbridge_flags),
	DEF_CMD("-vlanfilter", (int32_t)IFBRF_VLANFILTER,
					unsetbridge_flags),
	DEF_CMD_ARG("defuntagged",	setbridge_defuntagged),
	DEF_CMD("-defuntagged", 0,	unsetbridge_defuntagged),
	DEF_CMD("defqinq", (int32_t)IFBRF_DEFQINQ,
					setbridge_flags),
	DEF_CMD("-defqinq", (int32_t)IFBRF_DEFQINQ,
					unsetbridge_flags),
	DEF_CMD_ARG("qinq",		setbridge_qinq),
	DEF_CMD_ARG("-qinq",		unsetbridge_qinq),
};

static struct afswtch af_bridge = {
	.af_name	= "af_bridge",
	.af_af		= AF_UNSPEC,
	.af_other_status = bridge_status,
};

static __constructor void
bridge_ctor(void)
{
	for (size_t i = 0; i < nitems(bridge_cmds);  i++)
		cmd_register(&bridge_cmds[i]);
	af_register(&af_bridge);
}
