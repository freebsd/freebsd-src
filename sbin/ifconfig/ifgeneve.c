/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025-2026 Pouria Mousavizadeh Tehrani <pouria@FreeBSD.org>
 * All rights reserved.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/nv.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <netdb.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_strings.h>
#include <netinet/in.h>
#include <net/if_geneve.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>

#include "ifconfig.h"
#include "ifconfig_netlink.h"

struct nl_parsed_geneve {
	/* essential */
	uint32_t			ifla_vni;
	uint16_t			ifla_proto;
	struct sockaddr			*ifla_local;
	struct sockaddr			*ifla_remote;
	uint16_t			ifla_local_port;
	uint16_t			ifla_remote_port;

	/* optional */
	struct ifla_geneve_port_range	*ifla_port_range;
	enum ifla_geneve_df		ifla_df;
	uint8_t				ifla_ttl;
	bool				ifla_ttl_inherit;
	bool				ifla_dscp_inherit;
	bool				ifla_external;

	/* l2 specific */
	bool				ifla_ftable_learn;
	bool				ifla_ftable_flush;
	uint32_t			ifla_ftable_max;
	uint32_t			ifla_ftable_timeout;
	uint32_t			ifla_ftable_count;
	uint32_t			ifla_ftable_nospace;
	uint32_t			ifla_ftable_lock_upgrade_failed;

	/* multicast specific */
	char				*ifla_mc_ifname;
	uint32_t			ifla_mc_ifindex;

	/* csum info */
	uint64_t			ifla_stats_txcsum;
	uint64_t			ifla_stats_tso;
	uint64_t			ifla_stats_rxcsum;
};

static struct geneve_params gnvp = {
	.ifla_proto		=	GENEVE_PROTO_ETHER,
};

static int
get_proto(const char *cp, uint16_t *valp)
{
	uint16_t val;

	if (!strcmp(cp, "l2"))
		val = GENEVE_PROTO_ETHER;
	else if (!strcmp(cp, "l3"))
		val = GENEVE_PROTO_INHERIT;
	else
		return (-1);

	*valp = val;
	return (0);
}

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
get_df(const char *cp, enum ifla_geneve_df *valp)
{
	enum ifla_geneve_df df;

	if (!strcmp(cp, "set"))
		df = IFLA_GENEVE_DF_SET;
	else if (!strcmp(cp, "inherit"))
		df = IFLA_GENEVE_DF_INHERIT;
	else if (!strcmp(cp, "unset"))
		df = IFLA_GENEVE_DF_UNSET;
	else
		return (-1);

	*valp = df;
	return (0);
}

static bool
is_multicast(struct addrinfo *ai)
{
#if (defined INET || defined INET6)
	struct sockaddr *sa;
	sa = ai->ai_addr;
#endif

	switch (ai->ai_family) {
#ifdef INET
	case AF_INET: {
		struct sockaddr_in *sin = satosin(sa);

		return (IN_MULTICAST(ntohl(sin->sin_addr.s_addr)));
	}
#endif
#ifdef INET6
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = satosin6(sa);

		return (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr));
	}
#endif
	default:
		errx(1, "address family not supported");
	}
}

/*
 * geneve mode is read-only after creation,
 * therefore there is no need for separate netlink implementation
 */
static void
setgeneve_mode_clone(if_ctx *ctx __unused, const char *arg, int dummy __unused)
{
	uint16_t val;

	if (get_proto(arg, &val) < 0)
		errx(1, "invalid inner protocol: %s", arg);

	gnvp.ifla_proto = val;
}

struct nla_geneve_info {
	const char		*kind;
	struct nl_parsed_geneve	data;
};

struct nla_geneve_link {
	uint32_t		ifi_index;
	struct nla_geneve_info	linkinfo;
};

static inline void
geneve_nl_init(if_ctx *ctx, struct snl_writer *nw, uint32_t flags)
{
	struct nlmsghdr *hdr;

	snl_init_writer(ctx->io_ss, nw);
	hdr = snl_create_msg_request(nw, NL_RTM_NEWLINK);
	hdr->nlmsg_flags |= flags;
	snl_reserve_msg_object(nw, struct ifinfomsg);
        snl_add_msg_attr_string(nw, IFLA_IFNAME, ctx->ifname);
}

static inline void
geneve_nl_fini(if_ctx *ctx, struct snl_writer *nw)
{
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg = {};

	hdr = snl_finalize_msg(nw);
	if (hdr == NULL || !snl_send_message(ctx->io_ss, hdr))
		err(1, "unable to send netlink message");

	if (!snl_read_reply_code(ctx->io_ss, hdr->nlmsg_seq, &errmsg))
		errx(errmsg.error, "%s", errmsg.error_str);
}

#define _OUT(_field)	offsetof(struct nl_parsed_geneve, _field)
static const struct snl_attr_parser nla_geneve_linkinfo_data[] = {
	{ .type = IFLA_GENEVE_ID, .off = _OUT(ifla_vni), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GENEVE_PROTOCOL, .off = _OUT(ifla_proto), .cb = snl_attr_get_uint16 },
	{ .type = IFLA_GENEVE_LOCAL, .off = _OUT(ifla_local), .cb = snl_attr_get_ip },
	{ .type = IFLA_GENEVE_REMOTE, .off = _OUT(ifla_remote), .cb = snl_attr_get_ip },
	{ .type = IFLA_GENEVE_LOCAL_PORT, .off = _OUT(ifla_local_port), .cb = snl_attr_get_uint16 },
	{ .type = IFLA_GENEVE_PORT, .off = _OUT(ifla_remote_port), .cb = snl_attr_get_uint16 },
	{ .type = IFLA_GENEVE_PORT_RANGE, .off = _OUT(ifla_port_range), .cb = snl_attr_dup_struct },
	{ .type = IFLA_GENEVE_DF, .off = _OUT(ifla_df), .cb = snl_attr_get_uint8 },
	{ .type = IFLA_GENEVE_TTL, .off = _OUT(ifla_ttl), .cb = snl_attr_get_uint8 },
	{ .type = IFLA_GENEVE_TTL_INHERIT, .off = _OUT(ifla_ttl_inherit), .cb = snl_attr_get_bool },
	{ .type = IFLA_GENEVE_DSCP_INHERIT, .off = _OUT(ifla_dscp_inherit), .cb = snl_attr_get_bool },
	{ .type = IFLA_GENEVE_COLLECT_METADATA, .off = _OUT(ifla_external), .cb = snl_attr_get_bool },
	{ .type = IFLA_GENEVE_FTABLE_LEARN, .off = _OUT(ifla_ftable_learn), .cb = snl_attr_get_bool },
	{ .type = IFLA_GENEVE_FTABLE_FLUSH, .off = _OUT(ifla_ftable_flush), .cb = snl_attr_get_bool },
	{ .type = IFLA_GENEVE_FTABLE_MAX, .off = _OUT(ifla_ftable_max), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GENEVE_FTABLE_TIMEOUT, .off = _OUT(ifla_ftable_timeout), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GENEVE_FTABLE_COUNT, .off = _OUT(ifla_ftable_count), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GENEVE_FTABLE_NOSPACE_CNT, .off = _OUT(ifla_ftable_nospace), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GENEVE_FTABLE_LOCK_UP_FAIL_CNT, .off = _OUT(ifla_ftable_lock_upgrade_failed), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GENEVE_MC_IFNAME, .off = _OUT(ifla_mc_ifname), .cb = snl_attr_get_string },
	{ .type = IFLA_GENEVE_MC_IFINDEX, .off = _OUT(ifla_mc_ifindex), .cb = snl_attr_get_uint32 },
	{ .type = IFLA_GENEVE_TXCSUM_CNT, .off = _OUT(ifla_stats_txcsum), .cb = snl_attr_get_uint64 },
	{ .type = IFLA_GENEVE_TSO_CNT, .off = _OUT(ifla_stats_tso), .cb = snl_attr_get_uint64 },
	{ .type = IFLA_GENEVE_RXCSUM_CNT, .off = _OUT(ifla_stats_rxcsum), .cb = snl_attr_get_uint64 },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(geneve_linkinfo_data_parser, nla_geneve_linkinfo_data);

#define _OUT(_field)	offsetof(struct nla_geneve_info, _field)
static const struct snl_attr_parser ap_geneve_linkinfo[] = {
	{ .type = IFLA_INFO_KIND, .off = _OUT(kind), .cb = snl_attr_get_string },
	{ .type = IFLA_INFO_DATA, .off = _OUT(data),
		.arg = &geneve_linkinfo_data_parser, .cb = snl_attr_get_nested },
};
#undef _OUT
SNL_DECLARE_ATTR_PARSER(geneve_linkinfo_parser, ap_geneve_linkinfo);

#define _IN(_field)	offsetof(struct ifinfomsg, _field)
#define _OUT(_field)	offsetof(struct nla_geneve_link, _field)
static const struct snl_attr_parser ap_geneve_link[] = {
	{ .type = IFLA_LINKINFO, .off = _OUT(linkinfo),
		.arg = &geneve_linkinfo_parser, .cb = snl_attr_get_nested },
};

static const struct snl_field_parser fp_geneve_link[] = {
	{ .off_in = _IN(ifi_index), .off_out = _OUT(ifi_index), .cb = snl_field_get_uint32 },
};
#undef _IN
#undef _OUT
SNL_DECLARE_PARSER(geneve_parser, struct ifinfomsg, fp_geneve_link, ap_geneve_link);

static const struct snl_hdr_parser *all_parsers[] = {
	&geneve_linkinfo_data_parser,
	&geneve_linkinfo_parser,
	&geneve_parser,
};

static void
geneve_status_nl(if_ctx *ctx)
{
	struct snl_writer nw;
	struct nlmsghdr *hdr;
	struct snl_errmsg_data errmsg;
	struct nla_geneve_link geneve_link;
	char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];
	struct sockaddr *lsa, *rsa;
	int mc;
	bool ipv6 = false;

	if (strncmp(ctx->ifname, "geneve", sizeof("geneve") - 1) != 0)
		return;

	snl_init_writer(ctx->io_ss, &nw);
	hdr = snl_create_msg_request(&nw, NL_RTM_GETLINK);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_reserve_msg_object(&nw, struct ifinfomsg);
        snl_add_msg_attr_string(&nw, IFLA_IFNAME, ctx->ifname);

	if (!(hdr = snl_finalize_msg(&nw)) || (!snl_send_message(ctx->io_ss, hdr)))
		return;

	hdr = snl_read_reply(ctx->io_ss, hdr->nlmsg_seq);
	if (hdr->nlmsg_type != NL_RTM_NEWLINK) {
		if (!snl_parse_errmsg(ctx->io_ss, hdr, &errmsg))
			errx(EINVAL, "(NETLINK)");
		if (errmsg.error_str != NULL)
			errx(errmsg.error, "(NETLINK) %s", errmsg.error_str);
	}

	if (!snl_parse_nlmsg(ctx->io_ss, hdr, &geneve_parser, &geneve_link))
		return;

	struct nla_geneve_info geneve_info = geneve_link.linkinfo;
	struct nl_parsed_geneve geneve_data = geneve_info.data;

	printf("\tgeneve mode: ");
	switch (geneve_data.ifla_proto) {
	case GENEVE_PROTO_INHERIT:
		printf("l3");
		break;
	case GENEVE_PROTO_ETHER:
	default:
		printf("l2");
		break;
	}

	printf("\n\tgeneve config:\n");
	/* Just report nothing if the network identity isn't set yet. */
	if (geneve_data.ifla_vni >= GENEVE_VNI_MAX) {
		printf("\t\tvirtual network identifier (vni): not configured\n");
		return;
	}

	lsa = geneve_data.ifla_local;
	rsa = geneve_data.ifla_remote;

	if ((lsa == NULL) ||
	    (getnameinfo(lsa, lsa->sa_len, src, sizeof(src),
	    NULL, 0, NI_NUMERICHOST) != 0))
		src[0] = '\0';
	if ((rsa == NULL) ||
	    (getnameinfo(rsa, rsa->sa_len, dst, sizeof(dst),
	    NULL, 0, NI_NUMERICHOST) != 0))
		dst[0] = '\0';
	else {
		ipv6 = rsa->sa_family == AF_INET6;
		if (!ipv6) {
			struct sockaddr_in *sin = satosin(rsa);
			mc = IN_MULTICAST(ntohl(sin->sin_addr.s_addr));
		} else {
			struct sockaddr_in6 *sin6 = satosin6(rsa);
			mc = IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr);
		}
	}

	printf("\t\tvirtual network identifier (vni): %d", geneve_data.ifla_vni);
	if (src[0] != '\0')
		printf("\n\t\tlocal: %s%s%s:%u", ipv6 ? "[" : "", src, ipv6 ? "]" : "",
		    geneve_data.ifla_local_port);
	if (dst[0] != '\0') {
		printf("\n\t\t%s: %s%s%s:%u", mc ? "group" : "remote", ipv6 ? "[" : "",
		    dst, ipv6 ? "]" : "", geneve_data.ifla_local_port);
		if (mc)
			printf(", dev: %s", geneve_data.ifla_mc_ifname);
	}

	if (ctx->args->verbose) {
		printf("\n\t\tportrange: %u-%u",
		    geneve_data.ifla_port_range->low,
		    geneve_data.ifla_port_range->high);

		if (geneve_data.ifla_ttl_inherit)
			printf(", ttl: inherit");
		else
			printf(", ttl: %d", geneve_data.ifla_ttl);

		if (geneve_data.ifla_dscp_inherit)
			printf(", dscp: inherit");

		if (geneve_data.ifla_df == IFLA_GENEVE_DF_INHERIT)
			printf(", df: inherit");
		else if (geneve_data.ifla_df == IFLA_GENEVE_DF_SET)
			printf(", df: set");
		else if (geneve_data.ifla_df == IFLA_GENEVE_DF_UNSET)
			printf(", df: unset");

		if (geneve_data.ifla_external)
			printf(", externally controlled");

		if (geneve_data.ifla_proto == GENEVE_PROTO_ETHER) {
			printf("\n\t\tftable mode: %slearning",
			    geneve_data.ifla_ftable_learn ? "" : "no");
			printf(", count: %d, max: %d, timeout: %d",
			    geneve_data.ifla_ftable_count,
			    geneve_data.ifla_ftable_max,
			    geneve_data.ifla_ftable_timeout);
			printf(", nospace: %u",
			    geneve_data.ifla_ftable_nospace);
		}

		printf("\n\t\tstats: tso %lu, txcsum %lu, rxcsum %lu",
		    geneve_data.ifla_stats_tso,
		    geneve_data.ifla_stats_txcsum,
		    geneve_data.ifla_stats_rxcsum);
	}

	putchar('\n');
}


static void
geneve_create_nl(if_ctx *ctx, struct ifreq *ifr)
{
	struct snl_writer nw = {};
	struct nlmsghdr *hdr;
	int off, off2;

	snl_init_writer(ctx->io_ss, &nw);
	hdr = snl_create_msg_request(&nw, RTM_NEWLINK);
	hdr->nlmsg_flags |= (NLM_F_CREATE | NLM_F_EXCL);
	snl_reserve_msg_object(&nw, struct ifinfomsg);
        snl_add_msg_attr_string(&nw, IFLA_IFNAME, ifr->ifr_name);

	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
        snl_add_msg_attr_u16(&nw, IFLA_GENEVE_PROTOCOL, gnvp.ifla_proto);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_vni_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	u_long val;

	if (get_val(arg, &val) < 0 || val >= GENEVE_VNI_MAX)
		errx(1, "invalid network identifier: %s", arg);

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
        snl_add_msg_attr_u32(&nw, IFLA_GENEVE_ID, val);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_local_nl(if_ctx *ctx, const char *addr, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	struct addrinfo *ai;
	const struct sockaddr *sa;
	int error;

	if ((error = getaddrinfo(addr, NULL, NULL, &ai)) != 0)
		errx(1, "error in parsing local address string: %s",
		    gai_strerror(error));

	if (is_multicast(ai))
		errx(1, "local address cannot be multicast");

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

	sa = ai->ai_addr;
        snl_add_msg_attr_ip(&nw, IFLA_GENEVE_LOCAL, sa);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_remote_nl(if_ctx *ctx, const char *addr, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	struct addrinfo *ai;
	const struct sockaddr *sa;
	int error;

	if ((error = getaddrinfo(addr, NULL, NULL, &ai)) != 0)
		errx(1, "error in parsing remote address string: %s",
		    gai_strerror(error));

	if (is_multicast(ai))
		errx(1, "remote address cannot be multicast");

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

	sa = ai->ai_addr;
        snl_add_msg_attr_ip(&nw, IFLA_GENEVE_REMOTE, sa);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_group_nl(if_ctx *ctx, const char *addr, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	struct addrinfo *ai;
	struct sockaddr *sa;
	int error;

	if ((error = getaddrinfo(addr, NULL, NULL, &ai)) != 0)
		errx(1, "error in parsing local address string: %s",
		    gai_strerror(error));

	if (!is_multicast(ai))
		errx(1, "group address must be multicast");

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

	sa = ai->ai_addr;
        snl_add_msg_attr_ip(&nw, IFLA_GENEVE_REMOTE, sa);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}


static void
setgeneve_local_port_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	u_long val;

	if (get_val(arg, &val) < 0 || val >= UINT16_MAX)
		errx(1, "invalid local port: %s", arg);

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_u16(&nw, IFLA_GENEVE_LOCAL_PORT, val);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_remote_port_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	u_long val;

	if (get_val(arg, &val) < 0 || val >= UINT16_MAX)
		errx(1, "invalid remote port: %s", arg);

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_u16(&nw, IFLA_GENEVE_PORT, val);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_port_range_nl(if_ctx *ctx, const char *arg1, const char *arg2)
{
	struct snl_writer nw = {};
	int off, off2;
	u_long min, max;

	if (get_val(arg1, &min) < 0 || min >= UINT16_MAX)
		errx(1, "invalid port range minimum: %s", arg1);
	if (get_val(arg2, &max) < 0 || max >= UINT16_MAX)
		errx(1, "invalid port range maximum: %s", arg2);
	if (max < min)
		errx(1, "invalid port range");

	const struct ifla_geneve_port_range port_range = {
		.low = min,
		.high = max
	};

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr(&nw, IFLA_GENEVE_PORT_RANGE,
			sizeof(port_range), (const void *)&port_range);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_timeout_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xFFFFFFFF) != 0)
		errx(1, "invalid timeout value: %s", arg);

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_u32(&nw, IFLA_GENEVE_FTABLE_TIMEOUT, val);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_maxaddr_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	u_long val;

	if (get_val(arg, &val) < 0 || (val & ~0xFFFFFFFF) != 0)
		errx(1, "invalid maxaddr value: %s",  arg);

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_u32(&nw, IFLA_GENEVE_FTABLE_MAX, val);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_dev_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_string(&nw, IFLA_GENEVE_MC_IFNAME, arg);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_ttl_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	u_long val;

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);
	if ((get_val(arg, &val) < 0 || val > 256) == 0) {
		snl_add_msg_attr_u8(&nw, IFLA_GENEVE_TTL, val);
		snl_add_msg_attr_bool(&nw, IFLA_GENEVE_TTL_INHERIT, false);
	} else if (!strcmp(arg, "inherit")) {
		snl_add_msg_attr_bool(&nw, IFLA_GENEVE_TTL_INHERIT, true);
	} else
		errx(1, "invalid TTL value: %s", arg);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_df_nl(if_ctx *ctx, const char *arg, int dummy __unused)
{
	struct snl_writer nw = {};
	int off, off2;
	enum ifla_geneve_df df;

	if (get_df(arg, &df) < 0)
		errx(1, "invalid df value: %s", arg);

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_u8(&nw, IFLA_GENEVE_DF, df);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_inherit_dscp_nl(if_ctx *ctx, const char *arg __unused, int d)
{
	struct snl_writer nw = {};
	int off, off2;

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_bool(&nw, IFLA_GENEVE_DSCP_INHERIT, d != 0);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_learn_nl(if_ctx *ctx, const char *arg __unused, int d)
{
	struct snl_writer nw = {};
	int off, off2;

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_bool(&nw, IFLA_GENEVE_FTABLE_LEARN, d != 0);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_flush_nl(if_ctx *ctx, const char *val __unused, int d)
{
	struct snl_writer nw = {};
	int off, off2;

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_bool(&nw, IFLA_GENEVE_FTABLE_FLUSH, d != 0);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static void
setgeneve_external_nl(if_ctx *ctx, const char *val __unused, int d)
{
	struct snl_writer nw = {};
	int off, off2;

	geneve_nl_init(ctx, &nw, 0);
	off = snl_add_msg_attr_nested(&nw, IFLA_LINKINFO);
        snl_add_msg_attr_string(&nw, IFLA_INFO_KIND, "geneve");

	off2 = snl_add_msg_attr_nested(&nw, IFLA_INFO_DATA);

        snl_add_msg_attr_bool(&nw, IFLA_GENEVE_COLLECT_METADATA, d != 0);

	snl_end_attr_nested(&nw, off2);
	snl_end_attr_nested(&nw, off);

	geneve_nl_fini(ctx, &nw);
}

static struct cmd geneve_cmds[] = {

	DEF_CLONE_CMD_ARG("genevemode",		setgeneve_mode_clone),

	DEF_CMD_ARG("geneveid",			setgeneve_vni_nl),
	DEF_CMD_ARG("genevelocal",		setgeneve_local_nl),
	DEF_CMD_ARG("geneveremote",		setgeneve_remote_nl),
	DEF_CMD_ARG("genevegroup",		setgeneve_group_nl),
	DEF_CMD_ARG("genevelocalport",		setgeneve_local_port_nl),
	DEF_CMD_ARG("geneveremoteport",		setgeneve_remote_port_nl),
	DEF_CMD_ARG2("geneveportrange",		setgeneve_port_range_nl),
	DEF_CMD_ARG("genevetimeout",		setgeneve_timeout_nl),
	DEF_CMD_ARG("genevemaxaddr",		setgeneve_maxaddr_nl),
	DEF_CMD_ARG("genevedev",		setgeneve_dev_nl),
	DEF_CMD_ARG("genevettl",		setgeneve_ttl_nl),
	DEF_CMD_ARG("genevedf",			setgeneve_df_nl),
	DEF_CMD("genevedscpinherit", 1,		setgeneve_inherit_dscp_nl),
	DEF_CMD("-genevedscpinherit", 0,	setgeneve_inherit_dscp_nl),
	DEF_CMD("genevelearn", 1,		setgeneve_learn_nl),
	DEF_CMD("-genevelearn", 0,		setgeneve_learn_nl),
	DEF_CMD("geneveflushall", 0,		setgeneve_flush_nl),
	DEF_CMD("geneveflush", 1,		setgeneve_flush_nl),
	DEF_CMD("geneveexternal", 1,		setgeneve_external_nl),
	DEF_CMD("-geneveexternal", 0,		setgeneve_external_nl),

	DEF_CMD_SARG("genevehwcsum",	IFCAP2_GENEVE_HWCSUM_NAME,
	    setifcapnv),
	DEF_CMD_SARG("-genevehwcsum",	"-"IFCAP2_GENEVE_HWCSUM_NAME,
	    setifcapnv),
	DEF_CMD_SARG("genevehwtso",	IFCAP2_GENEVE_HWTSO_NAME,
	    setifcapnv),
	DEF_CMD_SARG("-genevehwtso",	"-"IFCAP2_GENEVE_HWTSO_NAME,
	    setifcapnv),
};

static struct afswtch af_geneve = {
	.af_name		= "af_geneve",
	.af_af			= AF_UNSPEC,
	.af_other_status	= geneve_status_nl,
};

static __constructor void
geneve_ctor(void)
{
	size_t i;

	for (i = 0; i < nitems(geneve_cmds); i++)
		cmd_register(&geneve_cmds[i]);
	af_register(&af_geneve);
	clone_setdefcallback_prefix("geneve", geneve_create_nl);
	SNL_VERIFY_PARSERS(all_parsers);
}
