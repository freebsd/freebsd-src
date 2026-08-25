/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2008 Andrew Thompson. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <net/if_gre.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#include "ifconfig.h"

#ifndef WITHOUT_NETLINK
#include "ifconfig_netlink.h"
#endif

static const char *GREBITS[] = {
	[0] = "ENABLE_CSUM",
	[1] = "ENABLE_SEQ",
	[2] = "UDPENCAP",
};

#ifndef WITHOUT_NETLINK
struct nl_parsed_gre {
	uint32_t		ifla_flags;
	uint32_t		ifla_okey;
	uint32_t		ifla_encap_type;
	uint16_t		ifla_encap_sport;
};

struct nla_gre_info {
	const char		*kind;
	struct nl_parsed_gre	data;
};

struct nla_gre_link {
	uint32_t		ifi_index;
	struct nla_gre_info	linkinfo;
};

static inline void
gre_nl_init(if_ctx *ctx, struct snl_writer *nw, uint32_t flags)
{
	struct nlmsghdr *hdr;

	snl_init_writer(ctx->io_ss, nw);
	hdr = snl_create_msg_request(nw, NL_RTM_NEWLINK);
	hdr->nlmsg_flags |= flags;
	snl_reserve_msg_object(nw, struct ifinfomsg);
	snl_add_msg_attr_string(nw, IFLA_IFNAME, ctx->ifname);
}

static inline void
gre_nl_fini(if_ctx *ctx, struct snl_writer *nw)
{
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg = {};

	hdr = snl_finalize_msg(nw);
	if (hdr == NULL || !snl_send_message(ctx->io_ss, hdr))
		err(1, "unable to send netlink message");

	if (!snl_read_reply_code(ctx->io_ss, hdr->nlmsg_seq, &errmsg))
		errx(errmsg.error, "%s", errmsg.error_str);
}

#define _OUT(_field)	offsetof(struct nl_parsed_gre, _field)
static const struct snl_attr_parser nla_p_gre[] = {
	{ .type = IFLA_GRE_FLAGS, .off = _OUT(ifla_flags), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GRE_OKEY, .off = _OUT(ifla_okey), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GRE_ENCAP_TYPE, .off = _OUT(ifla_encap_type),
		.cb = snl_attr_get_uint32 },
	{ .type = IFLA_GRE_ENCAP_SPORT, .off = _OUT(ifla_encap_sport),
		.cb = snl_attr_get_uint16 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(gre_linkinfo_data_parser, nla_p_gre);

#define _OUT(_field)	offsetof(struct nla_gre_info, _field)
static const struct snl_attr_parser ap_gre_linkinfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(kind), .cb = snl_attr_get_string },
	{ .type = IFLA_INFO_DATA, .off = _OUT(data),
		.arg = &gre_linkinfo_data_parser, .cb = snl_attr_get_nested },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(gre_linkinfo_parser, ap_gre_linkinfo);

#define _IN(_field)	offsetof(struct ifinfomsg, _field)
#define _OUT(_field)	offsetof(struct nla_gre_link, _field)
static const struct snl_attr_parser ap_gre_link[] = {
	{ .type = IFLA_LINKINFO, .off = _OUT(linkinfo),
		.arg = &gre_linkinfo_parser, .cb = snl_attr_get_nested },
};

static const struct snl_field_parser fp_geneve_link[] = {
	{ .off_in = _IN(ifi_index), .off_out = _OUT(ifi_index),
		.cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(gre_parser, struct ifinfomsg, fp_geneve_link, ap_gre_link);

static const struct snl_hdr_parser *all_parsers[] = {
	&gre_linkinfo_data_parser,
	&gre_linkinfo_parser,
	&gre_parser,
};

static void
gre_status_nl(if_ctx *ctx)
{
	struct snl_writer nw = {};
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg = {};
	struct nla_gre_link gre_link = {};

	if (strncmp(ctx->ifname, "gre", sizeof("gre") - 1) != 0)
		return;

	snl_init_writer(ctx->io_ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_GETLINK);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, ctx->ifname);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL || !snl_send_message(ctx->io_ss, hdr))
		err(1, "unable to send netlink message");

	hdr = snl_read_reply(ctx->io_ss, hdr->nlmsg_seq);
	if (hdr->nlmsg_type != NL_RTM_NEWLINK) {
		if (!snl_parse_errmsg(ctx->io_ss, hdr, &errmsg))
			errx(EINVAL, "(NETLINK)");
		if (errmsg.error_str != NULL)
			errx(errmsg.error, "(NETLINK) %s", errmsg.error_str);
	}

	if (!snl_parse_nlmsg(ctx->io_ss, hdr, &gre_parser, &gre_link))
		return;

	struct nla_gre_info gre_info = gre_link.linkinfo;
	struct nl_parsed_gre gre_data = gre_info.data;

	if (gre_data.ifla_okey != 0)
		printf("\tgrekey: 0x%x (%u)\n",
		    gre_data.ifla_okey, gre_data.ifla_okey);

	if (gre_data.ifla_flags == 0)
		return;

	if (gre_data.ifla_encap_sport != 0)
		printf("\tudpport: %u\n", gre_data.ifla_encap_sport);

	printf("\toptions=%x", gre_data.ifla_flags);
	print_bits("options", &gre_data.ifla_flags, 1, GREBITS, nitems(GREBITS));
	putchar('\n');
}

static void
setifgrekey_nl(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	uint32_t grekey = strtol(val, NULL, 0);

	gre_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "gre");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_u32(&nw, IFLA_GRE_OKEY, grekey);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	gre_nl_fini(ctx, &nw);
}

static void
setifgreport_nl(if_ctx *ctx, const char *val, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	uint16_t greport = strtol(val, NULL, 0);

	gre_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "gre");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_u16(&nw, IFLA_GRE_ENCAP_SPORT, greport);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	gre_nl_fini(ctx, &nw);
}

static void
setifgreopts_nl(if_ctx *ctx, const char *val __unused, int d)
{
	struct snl_writer nw = {};
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg;
	struct nla_gre_link gre_link;
	int off, off2;

	snl_init_writer(ctx->io_ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_GETLINK);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, ctx->ifname);

	hdr = snl_finalize_msg(&nw);
	if (hdr == NULL || (!snl_send_message(ctx->io_ss, hdr)))
		err(1, "unable to send netlink message");

	hdr = snl_read_reply(ctx->io_ss, hdr->nlmsg_seq);
	if (hdr->nlmsg_type != NL_RTM_NEWLINK) {
		if (!snl_parse_errmsg(ctx->io_ss, hdr, &errmsg))
			errx(EINVAL, "(NETLINK)");
		if (errmsg.error_str != NULL)
			errx(errmsg.error, "(NETLINK) %s", errmsg.error_str);
	}

	if (!snl_parse_nlmsg(ctx->io_ss, hdr, &gre_parser, &gre_link))
		return;

	struct nla_gre_info gre_info = gre_link.linkinfo;
	struct nl_parsed_gre gre_data = gre_info.data;

	if (d < 0)
		gre_data.ifla_flags &= ~(-d);
	else
		gre_data.ifla_flags |= d;

	gre_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "gre");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_u32(&nw, IFLA_GRE_FLAGS, gre_data.ifla_flags);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	gre_nl_fini(ctx, &nw);
}

static void
setifgretype_nl(if_ctx *ctx, const char *val __unused, int d)
{
	struct snl_writer nw = {};
	int off, off2;
	uint32_t type;

	gre_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "gre");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	type = d < 0 ? IFLA_TUNNEL_NONE : IFLA_TUNNEL_GRE_UDP;
	snl_add_msg_attr_u32(&nw, IFLA_GRE_ENCAP_TYPE, type);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	gre_nl_fini(ctx, &nw);
}


static struct cmd gre_cmds[] = {
	DEF_CMD_ARG("grekey",			setifgrekey_nl),
	DEF_CMD_ARG("udpport",			setifgreport_nl),
	DEF_CMD("enable_csum", GRE_ENABLE_CSUM,	setifgreopts_nl),
	DEF_CMD("-enable_csum",-GRE_ENABLE_CSUM,setifgreopts_nl),
	DEF_CMD("enable_seq", GRE_ENABLE_SEQ,	setifgreopts_nl),
	DEF_CMD("-enable_seq",-GRE_ENABLE_SEQ,	setifgreopts_nl),
	DEF_CMD("udpencap", GRE_UDPENCAP,	setifgretype_nl),
	DEF_CMD("-udpencap",-GRE_UDPENCAP,	setifgretype_nl),
};

#else
static void
gre_status(if_ctx *ctx)
{
	uint32_t opts = 0, port;
	struct ifreq ifr = { .ifr_data = (caddr_t)&opts };

	if (ioctl_ctx_ifr(ctx, GREGKEY, &ifr) == 0)
		if (opts != 0)
			printf("\tgrekey: 0x%x (%u)\n", opts, opts);
	opts = 0;
	if (ioctl_ctx_ifr(ctx, GREGOPTS, &ifr) != 0 || opts == 0)
		return;

	port = 0;
	ifr.ifr_data = (caddr_t)&port;
	if (ioctl_ctx_ifr(ctx, GREGPORT, &ifr) == 0 && port != 0)
		printf("\tudpport: %u\n", port);
	printf("\toptions=%x", opts);
	print_bits("options", &opts, 1, GREBITS, nitems(GREBITS));
	putchar('\n');
}

static void
setifgrekey(if_ctx *ctx, const char *val, int dummy __unused)
{
	uint32_t grekey = strtol(val, NULL, 0);
	struct ifreq ifr = { .ifr_data = (caddr_t)&grekey };

	ifr.ifr_data = (caddr_t)&grekey;
	if (ioctl_ctx_ifr(ctx, GRESKEY, &ifr) < 0)
		warn("ioctl (set grekey)");
}

static void
setifgreport(if_ctx *ctx, const char *val, int dummy __unused)
{
	uint32_t udpport = strtol(val, NULL, 0);
	struct ifreq ifr = { .ifr_data = (caddr_t)&udpport };

	if (ioctl_ctx_ifr(ctx, GRESPORT, &ifr) < 0)
		warn("ioctl (set udpport)");
}

static void
setifgreopts(if_ctx *ctx, const char *val __unused, int d)
{
	uint32_t opts;
	struct ifreq ifr = { .ifr_data = (caddr_t)&opts };

	if (ioctl_ctx_ifr(ctx, GREGOPTS, &ifr) == -1) {
		warn("ioctl(GREGOPTS)");
		return;
	}

	if (d < 0)
		opts &= ~(-d);
	else
		opts |= d;

	if (ioctl_ctx(ctx, GRESOPTS, &ifr) == -1) {
		warn("ioctl(GIFSOPTS)");
		return;
	}
}


static struct cmd gre_cmds[] = {
	DEF_CMD_ARG("grekey",			setifgrekey),
	DEF_CMD_ARG("udpport",			setifgreport),
	DEF_CMD("enable_csum", GRE_ENABLE_CSUM,	setifgreopts),
	DEF_CMD("-enable_csum",-GRE_ENABLE_CSUM,setifgreopts),
	DEF_CMD("enable_seq", GRE_ENABLE_SEQ,	setifgreopts),
	DEF_CMD("-enable_seq",-GRE_ENABLE_SEQ,	setifgreopts),
	DEF_CMD("udpencap", GRE_UDPENCAP,	setifgreopts),
	DEF_CMD("-udpencap",-GRE_UDPENCAP,	setifgreopts),
};
#endif /* !WITHOUT_NETLINK */

static struct afswtch af_gre = {
	.af_name	= "af_gre",
	.af_af		= AF_UNSPEC,
#ifndef WITHOUT_NETLINK
	.af_other_status = gre_status_nl,
#else
	.af_other_status = gre_status,
#endif
};

static __constructor void
gre_ctor(void)
{
	size_t i;

	for (i = 0; i < nitems(gre_cmds);  i++)
		cmd_register(&gre_cmds[i]);
	af_register(&af_gre);
#ifndef WITHOUT_NETLINK
	SNL_VERIFY_PARSERS(all_parsers);
#endif
}
