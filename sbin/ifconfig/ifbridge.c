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

	prefix = "\tmember: ";
	pad    = "\t        ";
	for (size_t i = 0; i < bridge->members_count; ++i) {
		struct ifbreq *member = &bridge->members[i];

		printf("%s%s ", prefix, member->ifbr_ifsname);
		printb("flags", member->ifbr_ifsflags, IFBIFBITS);
		printf("\n%s", pad);
		printf("ifmaxaddr %u port %u priority %u path cost %u",
		    member->ifbr_addrmax,
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
		printf("\n");
	}

	ifconfig_bridge_free_bridge_status(bridge);
}

static void
setbridge_add(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct ifbreq req;

	memset(&req, 0, sizeof(req));
	strlcpy(req.ifbr_ifsname, val, sizeof(req.ifbr_ifsname));
	if (do_cmd(ctx, BRDGADD, &req, sizeof(req), 1) < 0)
		err(1, "BRDGADD %s",  val);
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

static void
setbridge_static(if_ctx *ctx, const char *val, const char *mac)
{
	struct ifbareq req;
	struct ether_addr *ea;

	memset(&req, 0, sizeof(req));
	strlcpy(req.ifba_ifsname, val, sizeof(req.ifba_ifsname));

	ea = ether_aton(mac);
	if (ea == NULL)
		errx(1, "%s: invalid address: %s", val, mac);

	memcpy(req.ifba_dst, ea->octet, sizeof(req.ifba_dst));
	req.ifba_flags = IFBAF_STATIC;
	req.ifba_vlan = 1; /* XXX allow user to specify */

	if (do_cmd(ctx, BRDGSADDR, &req, sizeof(req), 1) < 0)
		err(1, "BRDGSADDR %s",  val);
}

static void
setbridge_deladdr(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct ifbareq req;
	struct ether_addr *ea;

	memset(&req, 0, sizeof(req));

	ea = ether_aton(val);
	if (ea == NULL)
		errx(1, "invalid address: %s",  val);

	memcpy(req.ifba_dst, ea->octet, sizeof(req.ifba_dst));

	if (do_cmd(ctx, BRDGDADDR, &req, sizeof(req), 1) < 0)
		err(1, "BRDGDADDR %s",  val);
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

static struct cmd bridge_cmds[] = {
	DEF_CMD_ARG("addm",		setbridge_add),
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
	DEF_CMD_ARG2("static",		setbridge_static),
	DEF_CMD_ARG("deladdr",		setbridge_deladdr),
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
	DEF_CMD_ARG("timeout",		setbridge_timeout),
	DEF_CMD_ARG("private",		setbridge_private),
	DEF_CMD_ARG("-private",		unsetbridge_private),
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
