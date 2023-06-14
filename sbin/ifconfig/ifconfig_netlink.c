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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>

#include <sys/bitcount.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include "ifconfig.h"
#include "ifconfig_netlink.h"

static const char	*IFFBITS[] = {
	"UP",			/* 00:0x1 IFF_UP*/
	"BROADCAST",		/* 01:0x2 IFF_BROADCAST*/
	"DEBUG",		/* 02:0x4 IFF_DEBUG*/
	"LOOPBACK",		/* 03:0x8 IFF_LOOPBACK*/
	"POINTOPOINT",		/* 04:0x10 IFF_POINTOPOINT*/
	"NEEDSEPOCH",		/* 05:0x20 IFF_NEEDSEPOCH*/
	"RUNNING",		/* 06:0x40 IFF_DRV_RUNNING*/
	"NOARP",		/* 07:0x80 IFF_NOARP*/
	"PROMISC",		/* 08:0x100 IFF_PROMISC*/
	"ALLMULTI",		/* 09:0x200 IFF_ALLMULTI*/
	"DRV_OACTIVE",		/* 10:0x400 IFF_DRV_OACTIVE*/
	"SIMPLEX",		/* 11:0x800 IFF_SIMPLEX*/
	"LINK0",		/* 12:0x1000 IFF_LINK0*/
	"LINK1",		/* 13:0x2000 IFF_LINK1*/
	"LINK2",		/* 14:0x4000 IFF_LINK2*/
	"MULTICAST",		/* 15:0x8000 IFF_MULTICAST*/
	"CANTCONFIG",		/* 16:0x10000 IFF_CANTCONFIG*/
	"PPROMISC",		/* 17:0x20000 IFF_PPROMISC*/
	"MONITOR",		/* 18:0x40000 IFF_MONITOR*/
	"STATICARP",		/* 19:0x80000 IFF_STATICARP*/
	"STICKYARP",		/* 20:0x100000 IFF_STICKYARP*/
	"DYING",		/* 21:0x200000 IFF_DYING*/
	"RENAMING",		/* 22:0x400000 IFF_RENAMING*/
	"NOGROUP",		/* 23:0x800000 IFF_NOGROUP*/
	"LOWER_UP",		/* 24:0x1000000 IFF_NETLINK_1*/
};

static void
print_bits(const char *btype, uint32_t *v, const int v_count,
    const char **names, const int n_count)
{
	int num = 0;

	for (int i = 0; i < v_count * 32; i++) {
		bool is_set = v[i / 32] & (1 << (i % 32));
		if (i == 31)
			v++;
		if (is_set) {
			if (num++ == 0)
				printf("<");
			if (num != 1)
				printf(",");
			if (i < n_count)
				printf("%s", names[i]);
			else
				printf("%s_%d", btype, i);
		}
	}
	if (num > 0)
		printf(">");
}	

static void
nl_init_socket(struct snl_state *ss)
{
	if (snl_init(ss, NETLINK_ROUTE))
		return;

	if (modfind("netlink") == -1 && errno == ENOENT) {
		/* Try to load */
		if (kldload("netlink") == -1)
			err(1, "netlink is not loaded and load attempt failed");
		if (snl_init(ss, NETLINK_ROUTE))
			return;
	}

	err(1, "unable to open netlink socket");
}

int
ifconfig_nl(if_ctx *ctx, int iscreate,
    const struct afswtch *uafp)
{
	struct snl_state ss = {};

	nl_init_socket(&ss);
	ctx->io_ss = &ss;

	int error = ifconfig_ioctl(ctx, iscreate, uafp);

	snl_free(&ss);
	ctx->io_ss = NULL;

	return (error);
}

struct ifa {
	struct ifa		*next;
	uint32_t		idx;
	struct snl_parsed_addr	addr;
};

struct iface {
	struct snl_parsed_link	link;
	struct ifa		*ifa;
	uint32_t		ifa_count;
	uint32_t		idx;
};

struct ifmap {
	uint32_t		size;
	uint32_t		count;
	struct iface		**ifaces;
};

/*
 * Returns ifmap ifindex->snl_parsed_link.
 * Memory is allocated using snl temporary buffers
 */
static struct ifmap *
prepare_ifmap(struct snl_state *ss)
{
	struct snl_writer nw = {};

	snl_init_writer(ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETLINK);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_reserve_msg_object(&nw, struct ifinfomsg);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
		return (NULL);

	uint32_t nlmsg_seq = hdr->nlmsg_seq;
	struct ifmap *ifmap = snl_allocz(ss, sizeof(*ifmap));
	struct snl_errmsg_data e = {};

	while ((hdr = snl_read_reply_multi(ss, nlmsg_seq, &e)) != NULL) {
		struct iface *iface = snl_allocz(ss, sizeof(*iface));

		if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_link_parser, &iface->link))
			continue;
		if (iface->link.ifi_index >= ifmap->size) {
			size_t new_size = MAX(ifmap->size, 32);

			while (new_size <= iface->link.ifi_index + 1)
				new_size *= 2;

			struct iface **ifaces= snl_allocz(ss, new_size * sizeof(void *));
			memcpy(ifaces, ifmap->ifaces, ifmap->size * sizeof(void *));
			ifmap->ifaces = ifaces;
			ifmap->size = new_size;
		}
		ifmap->ifaces[iface->link.ifi_index] = iface;
		ifmap->count++;
		iface->idx = ifmap->count;
	}
	return (ifmap);
}

uint32_t
if_nametoindex_nl(struct snl_state *ss, const char *ifname)
{
	struct snl_writer nw = {};
	struct snl_parsed_link_simple link = {};

	snl_init_writer(ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETLINK);
	snl_reserve_msg_object(&nw, struct ifinfomsg);
	snl_add_msg_attr_string(&nw, IFLA_IFNAME, ifname);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
		return (0);

	hdr = snl_read_reply(ss, hdr->nlmsg_seq);
	if (hdr->nlmsg_type != NL_RTM_NEWLINK)
		return (0);
	if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_link_parser_simple, &link))
		return (0);

	return (link.ifi_index);
}

static void
prepare_ifaddrs(struct snl_state *ss, struct ifmap *ifmap)
{
	struct snl_writer nw = {};

	snl_init_writer(ss, &nw);
	struct nlmsghdr *hdr = snl_create_msg_request(&nw, RTM_GETADDR);
	hdr->nlmsg_flags |= NLM_F_DUMP;
	snl_reserve_msg_object(&nw, struct ifaddrmsg);

	if (!snl_finalize_msg(&nw) || !snl_send_message(ss, hdr))
		return;

	uint32_t nlmsg_seq = hdr->nlmsg_seq;
	struct snl_errmsg_data e = {};
	uint32_t count = 0;

	while ((hdr = snl_read_reply_multi(ss, nlmsg_seq, &e)) != NULL) {
		struct ifa *ifa = snl_allocz(ss, sizeof(*ifa));

		if (!snl_parse_nlmsg(ss, hdr, &snl_rtm_addr_parser, &ifa->addr))
			continue;

		const uint32_t ifindex = ifa->addr.ifa_index;
		if (ifindex >= ifmap->size || ifmap->ifaces[ifindex] == NULL)
			continue;
		struct iface *iface = ifmap->ifaces[ifindex];
		ifa->next = iface->ifa;
		ifa->idx = ++count;
		iface->ifa = ifa;
		iface->ifa_count++;
	}
}

static bool
match_iface(struct ifconfig_args *args, struct iface *iface)
{
	if_link_t *link = &iface->link;

	if (args->ifname != NULL && strcmp(args->ifname, link->ifla_ifname))
		return (false);

	if (!match_if_flags(args, link->ifi_flags))
		return (false);

	if (!group_member(link->ifla_ifname, args->matchgroup, args->nogroup))
		return (false);

	if (args->afp == NULL)
		return (true);

	if (!strcmp(args->afp->af_name, "ether")) {
		if (link->ifla_address == NULL)
			return (false);

		struct sockaddr_dl sdl = {
			.sdl_len = sizeof(struct sockaddr_dl),
			.sdl_family = AF_LINK,
			.sdl_type = link->ifi_type,
			.sdl_alen = NLA_DATA_LEN(link->ifla_address),
		};
		return (match_ether(&sdl));
	}
	
	for (struct ifa *ifa = iface->ifa; ifa != NULL; ifa = ifa->next) {
		if (args->afp->af_af == ifa->addr.ifa_family)
			return (true);
	}

	return (false);
}

/* Sort according to the kernel-provided order */
static int
cmp_iface(const void *_a, const void *_b)
{
	const struct iface *a = *((const void * const *)_a);
	const struct iface *b = *((const void * const *)_b);

	return ((a->idx > b->idx) * 2 - 1);
}

static int
cmp_ifaddr(const void *_a, const void *_b)
{
	const struct ifa *a = *((const void * const *)_a);
	const struct ifa *b = *((const void * const *)_b);

	if (a->addr.ifa_family != b->addr.ifa_family)
		return ((a->addr.ifa_family > b->addr.ifa_family) * 2 - 1);
	return ((a->idx > b->idx) * 2 - 1);
}

static void
sort_iface_ifaddrs(struct snl_state *ss, struct iface *iface)
{
	if (iface->ifa_count == 0)
		return;

	struct ifa **sorted_ifaddrs = snl_allocz(ss, iface->ifa_count * sizeof(void *));
	struct ifa *ifa = iface->ifa;

	for (uint32_t i = 0; i < iface->ifa_count; i++) {
		struct ifa *ifa_next = ifa->next;

		sorted_ifaddrs[i] = ifa;
		ifa->next = NULL;
		ifa = ifa_next;
	}
	qsort(sorted_ifaddrs, iface->ifa_count, sizeof(void *), cmp_ifaddr);
	ifa = sorted_ifaddrs[0];
	iface->ifa = ifa;
	for (uint32_t i = 1; i < iface->ifa_count; i++) {
		ifa->next = sorted_ifaddrs[i];
		ifa = sorted_ifaddrs[i];
	}
}

static void
status_nl(if_ctx *ctx, struct iface *iface)
{
	if_link_t *link = &iface->link;
	struct ifconfig_args *args = ctx->args;

	printf("%s: ", link->ifla_ifname);

	printf("flags=%x", link->ifi_flags);
	print_bits("IFF", &link->ifi_flags, 1, IFFBITS, nitems(IFFBITS));

	print_metric(ctx->io_s);
	printf(" mtu %d\n", link->ifla_mtu);

	if (link->ifla_ifalias != NULL)
		printf("\tdescription: %s\n", link->ifla_ifalias);

	/* TODO: convert to netlink */
	strlcpy(ifr.ifr_name, link->ifla_ifname, sizeof(ifr.ifr_name));
	print_ifcap(args, ctx->io_s);
	tunnel_status(ctx);

	if (args->allfamilies | (args->afp != NULL && args->afp->af_af == AF_LINK)) {
		/* Start with link-level */
		const struct afswtch *p = af_getbyfamily(AF_LINK);
		if (p != NULL && link->ifla_address != NULL)
			p->af_status(ctx, link, NULL);
	}

	sort_iface_ifaddrs(ctx->io_ss, iface);

	for (struct ifa *ifa = iface->ifa; ifa != NULL; ifa = ifa->next) {
		if (args->allfamilies) {
			const struct afswtch *p = af_getbyfamily(ifa->addr.ifa_family);

			if (p != NULL)
				p->af_status(ctx, link, &ifa->addr);
		} else if (args->afp->af_af == ifa->addr.ifa_family) {
			const struct afswtch *p = args->afp;

			p->af_status(ctx, link, &ifa->addr);
		}
	}

	/* TODO: convert to netlink */
	if (args->allfamilies)
		af_other_status(ctx);
	else if (args->afp->af_other_status != NULL)
		args->afp->af_other_status(ctx);

	print_ifstatus(ctx);
	if (args->verbose > 0)
		sfp_status(ctx);
}

static int
get_local_socket(void)
{
	int s = socket(AF_LOCAL, SOCK_DGRAM, 0);
	
	if (s < 0)
		err(1, "socket(family %u,SOCK_DGRAM)", AF_LOCAL);
	return (s);
}

void
list_interfaces_nl(struct ifconfig_args *args)
{
	struct snl_state ss = {};
	struct ifconfig_context _ctx = {
		.args = args,
		.io_s = get_local_socket(),
		.io_ss = &ss,
	};
	struct ifconfig_context *ctx = &_ctx;

	nl_init_socket(&ss);

	struct ifmap *ifmap = prepare_ifmap(&ss);
	struct iface **sorted_ifaces = snl_allocz(&ss, ifmap->count * sizeof(void *));
	for (uint32_t i = 0, num = 0; i < ifmap->size; i++) {
		if (ifmap->ifaces[i] != NULL) {
			sorted_ifaces[num++] = ifmap->ifaces[i];
			if (num == ifmap->count)
				break;
		}
	}
	qsort(sorted_ifaces, ifmap->count, sizeof(void *), cmp_iface);
	prepare_ifaddrs(&ss, ifmap);

	for (uint32_t i = 0, num = 0; i < ifmap->count; i++) {
		struct iface *iface = sorted_ifaces[i];

		if (!match_iface(args, iface))
			continue;

		ctx->ifname = iface->link.ifla_ifname;

		if (args->namesonly) {
			if (num++ != 0)
				printf(" ");
			fputs(iface->link.ifla_ifname, stdout);
		} else if (args->argc == 0)
			status_nl(ctx, iface);
		else
			ifconfig_ioctl(ctx, 0, args->afp);
	}
	if (args->namesonly)
		printf("\n");

	close(ctx->io_s);
	snl_free(&ss);
}

