/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Hiroki Sato.  All rights reserved.
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

#include <stdlib.h>
#include <unistd.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_gif.h>
#ifdef WITHOUT_NETLINK
#include <net/route.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"

#ifndef WITHOUT_NETLINK
#include "ifconfig_netlink.h"
#endif

static const char *GIFBITS[] = {
	[0] = "NOCLAMP",
	[1] = "IGNORE_SOURCE",
};

#ifndef WITHOUT_NETLINK
struct nl_parsed_gif {
	uint32_t	ifla_flags;
};

struct nla_gif_info {
	const char		*kind;
	struct nl_parsed_gif	data;
};

struct nla_gif_link {
	uint32_t		ifi_index;
	struct nla_gif_info	linkinfo;
};

static inline void
gif_nl_init(if_ctx *ctx, struct snl_writer *nw, uint32_t flags)
{
	struct nlmsghdr *hdr;

	snl_init_writer(ctx->io_ss, nw);
	hdr = snl_create_msg_request(nw, NL_RTM_NEWLINK);
	hdr->nlmsg_flags |= flags;
	snl_reserve_msg_object(nw, struct ifinfomsg);
	snl_add_msg_attr_string(nw, IFLA_IFNAME, ctx->ifname);
}

static inline void
gif_nl_fini(if_ctx *ctx, struct snl_writer *nw)
{
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg = {};

	hdr = snl_finalize_msg(nw);
	if (hdr == NULL || !snl_send_message(ctx->io_ss, hdr))
		err(1, "unable to send netlink message");

	if (!snl_read_reply_code(ctx->io_ss, hdr->nlmsg_seq, &errmsg))
		errx(errmsg.error, "%s", errmsg.error_str);
}

#define _OUT(_field)   offsetof(struct nl_parsed_gif, _field)
static const struct snl_attr_parser nla_p_gif[] = {
	{ .type = IFLA_IPTUN_FLAGS, .off = _OUT(ifla_flags), .cb = snl_attr_get_uint32 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(gif_linkinfo_data_parser, nla_p_gif);

#define _OUT(_field)   offsetof(struct nla_gif_info, _field)
static const struct snl_attr_parser ap_gif_linkinfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(kind), .cb = snl_attr_get_string },
	{ .type = IFLA_INFO_DATA, .off = _OUT(data),
		.arg = &gif_linkinfo_data_parser, .cb = snl_attr_get_nested },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(gif_linkinfo_parser, ap_gif_linkinfo);

#define _IN(_field)    offsetof(struct ifinfomsg, _field)
#define _OUT(_field)   offsetof(struct nla_gif_link, _field)
static const struct snl_attr_parser ap_gif_link[] = {
	{ .type = IFLA_LINKINFO, .off = _OUT(linkinfo),
		.arg = &gif_linkinfo_parser, .cb = snl_attr_get_nested },
};

static const struct snl_field_parser fp_geneve_link[] = {
	{ .off_in = _IN(ifi_index), .off_out = _OUT(ifi_index),
		.cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(gif_parser, struct ifinfomsg, fp_geneve_link, ap_gif_link);

static const struct snl_hdr_parser *all_parsers[] = {
	&gif_linkinfo_data_parser,
	&gif_linkinfo_parser,
	&gif_parser,
};

static void
gif_status_nl(if_ctx *ctx)
{
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg = {};
	struct nla_gif_link gif_link;

	if (strncmp(ctx->ifname, "gif", sizeof("gif") - 1) != 0)
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

	if (!snl_parse_nlmsg(ctx->io_ss, hdr, &gif_parser, &gif_link))
		 return;

	struct nla_gif_info gif_info = gif_link.linkinfo;
	struct nl_parsed_gif gif_data = gif_info.data;

	if (gif_data.ifla_flags == 0)
		 return;

	printf("\toptions=%x", gif_data.ifla_flags);
	print_bits("options", &gif_data.ifla_flags, 1, GIFBITS, nitems(GIFBITS));
	putchar('\n');
}

static void
setgifopts_nl(if_ctx *ctx, const char *val __unused, int d)
{
	struct snl_writer nw = {};
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg = {};
	struct nla_gif_link gif_link = {};
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
	if (hdr->nlmsg_type != NL_RTM_GETLINK) {
		if (!snl_parse_errmsg(ctx->io_ss, hdr, &errmsg))
			errx(EINVAL, "(NETLINK)");
		if (errmsg.error_str != NULL)
			errx(errmsg.error, "(NETLINK) %s", errmsg.error_str);
	}

	if (!snl_parse_nlmsg(ctx->io_ss, hdr, &gif_parser, &gif_link))
		return;

	struct nla_gif_info gif_info = gif_link.linkinfo;
	struct nl_parsed_gif gif_data = gif_info.data;

	if (d < 0)
		gif_data.ifla_flags &= ~(-d);
	else
		gif_data.ifla_flags |= d;

	gif_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
	snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "gif");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	snl_add_msg_attr_u32(&nw, IFLA_IPTUN_FLAGS, gif_data.ifla_flags);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	gif_nl_fini(ctx, &nw);
}

static struct cmd gif_cmds[] = {
	DEF_CMD("noclamp",		GIF_NOCLAMP,		setgifopts_nl),
	DEF_CMD("-noclamp",		-GIF_NOCLAMP,		setgifopts_nl),
	DEF_CMD("ignore_source",	GIF_IGNORE_SOURCE,	setgifopts_nl),
	DEF_CMD("-ignore_source",	-GIF_IGNORE_SOURCE,	setgifopts_nl),
};

#else
static void
gif_status(if_ctx *ctx)
{
	int opts;
	struct ifreq ifr = { .ifr_data = (caddr_t)&opts };

	if (ioctl_ctx_ifr(ctx, GIFGOPTS, &ifr) == -1)
		return;
	if (opts == 0)
		return;
	printf("\toptions=%x", opts);
	print_bits("options", &opts, 1, GIFBITS, nitems(GIFBITS));
	putchar('\n');
}

static void
setgifopts(if_ctx *ctx, const char *val __unused, int d)
{
	int opts;
	struct ifreq ifr = { .ifr_data = (caddr_t)&opts };

	if (ioctl_ctx_ifr(ctx, GIFGOPTS, &ifr) == -1) {
		warn("ioctl(GIFGOPTS)");
		return;
	}

	if (d < 0)
		opts &= ~(-d);
	else
		opts |= d;

	if (ioctl_ctx(ctx, GIFSOPTS, &ifr) == -1) {
		warn("ioctl(GIFSOPTS)");
		return;
	}
}

static struct cmd gif_cmds[] = {
	DEF_CMD("noclamp",		GIF_NOCLAMP,		setgifopts),
	DEF_CMD("-noclamp",		-GIF_NOCLAMP,		setgifopts),
	DEF_CMD("ignore_source",	GIF_IGNORE_SOURCE,	setgifopts),
	DEF_CMD("-ignore_source",	-GIF_IGNORE_SOURCE,	setgifopts),
};
#endif /* !WITHOUT_NETLINK */

static struct afswtch af_gif = {
	.af_name	= "af_gif",
	.af_af		= AF_UNSPEC,
#ifndef WITHOUT_NETLINK
	.af_other_status = gif_status_nl,
#else
	.af_other_status = gif_status,
#endif
};

static __constructor void
gif_ctor(void)
{
	size_t i;

	for (i = 0; i < nitems(gif_cmds); i++)
		cmd_register(&gif_cmds[i]);
	af_register(&af_gif);
#ifndef WITHOUT_NETLINK
	SNL_VERIFY_PARSERS(all_parsers);
#endif
}
