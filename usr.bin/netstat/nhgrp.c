/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alexander V. Chernikov
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
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/route/nhop.h>

#include <netinet/in.h>

#include <arpa/inet.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <err.h>
#include <libxo/xo.h>
#include "netstat.h"
#include "common.h"

#define	WID_GW_DEFAULT(af)	(((af) == AF_INET6) ? 40 : 18)

static int wid_gw;
static int wid_if = 10;
static int wid_nhidx = 8;
static int wid_refcnt = 8;

struct nhop_entry {
	char	gw[64];
	char	ifname[IFNAMSIZ];
};

struct nhop_map {
	struct nhop_entry	*ptr;
	size_t			size;
};
static struct nhop_map global_nhop_map;

static struct ifmap_entry *ifmap;
static size_t ifmap_size;

static struct nhop_entry *
nhop_get(struct nhop_map *map, uint32_t idx)
{

	if (idx >= map->size)
		return (NULL);
	if (*map->ptr[idx].ifname == '\0')
		return (NULL);
	return &map->ptr[idx];
}

static void
print_nhgroup_header(int af1 __unused)
{

	xo_emit("{T:/%-*.*s}{T:/%-*.*s}{T:/%*.*s}{T:/%*.*s}{T:/%*.*s}"
	    "{T:/%*.*s}{T:/%*s}\n",
		wid_nhidx,	wid_nhidx,	"GrpIdx",
		wid_nhidx,	wid_nhidx,	"NhIdx",
		wid_nhidx,	wid_nhidx,	"Weight",
		wid_nhidx,	wid_nhidx,	"Slots",
		wid_gw,		wid_gw,		"Gateway",
		wid_if,		wid_if,		"Netif",
		wid_refcnt,			"Refcnt");
}

static void
print_padding(char sym, int len)
{
	char buffer[56];

	memset(buffer, sym, sizeof(buffer));
	buffer[0] = '{';
	buffer[1] = 'P';
	buffer[2] = ':';
	buffer[3] = ' ';
	buffer[len + 3] = '}';
	buffer[len + 4] = '\0';
	xo_emit(buffer);
}


static void
print_nhgroup_entry_sysctl(const char *name, struct rt_msghdr *rtm,
    struct nhgrp_external *nhge)
{
	char buffer[128];
	struct nhop_entry *ne;
	struct nhgrp_nhop_external *ext_cp, *ext_dp;
	struct nhgrp_container *nhg_cp, *nhg_dp;

	nhg_cp = (struct nhgrp_container *)(nhge + 1);
	if (nhg_cp->nhgc_type != NHG_C_TYPE_CNHOPS || nhg_cp->nhgc_subtype != 0)
		return;
	ext_cp = (struct nhgrp_nhop_external *)(nhg_cp + 1);

	nhg_dp = (struct nhgrp_container *)((char *)nhg_cp + nhg_cp->nhgc_len);
	if (nhg_dp->nhgc_type != NHG_C_TYPE_DNHOPS || nhg_dp->nhgc_subtype != 0)
		return;
	ext_dp = (struct nhgrp_nhop_external *)(nhg_dp + 1);

	xo_open_instance(name);

	snprintf(buffer, sizeof(buffer), "{[:-%d}{:nhgrp-index/%%lu}{]:} ", wid_nhidx);

	xo_emit(buffer, nhge->nhg_idx);

	/* nhidx */
	print_padding('-', wid_nhidx);
	/* weight */
	print_padding('-', wid_nhidx);
	/* slots */
	print_padding('-', wid_nhidx);
	print_padding('-', wid_gw);
	print_padding('-', wid_if);
	xo_emit("{t:nhg-refcnt/%*lu}", wid_refcnt, nhge->nhg_refcount);
	xo_emit("\n");

	xo_open_list("nhop-weights");
	for (uint32_t i = 0; i < nhg_cp->nhgc_count; i++) {
		/* TODO: optimize slots calculations */
		uint32_t slots = 0;
		for (uint32_t sidx = 0; sidx < nhg_dp->nhgc_count; sidx++) {
			if (ext_dp[sidx].nh_idx == ext_cp[i].nh_idx)
				slots++;
		}
		xo_open_instance("nhop-weight");
		print_padding(' ', wid_nhidx);
		// nh index
		xo_emit("{t:nh-index/%*lu}", wid_nhidx, ext_cp[i].nh_idx);
		xo_emit("{t:nh-weight/%*lu}", wid_nhidx, ext_cp[i].nh_weight);
		xo_emit("{t:nh-slots/%*lu}", wid_nhidx, slots);
		ne = nhop_get(&global_nhop_map, ext_cp[i].nh_idx);
		if (ne != NULL) {
			xo_emit("{t:nh-gw/%*.*s}", wid_gw, wid_gw, ne->gw);
			xo_emit("{t:nh-interface/%*.*s}", wid_if, wid_if, ne->ifname);
		}
		xo_emit("\n");
		xo_close_instance("nhop-weight");
	}
	xo_close_list("nhop-weights");
	xo_close_instance(name);
}

static int
cmp_nhg_idx(const void *_a, const void *_b)
{
	const struct nhops_map *a, *b;

	a = _a;
	b = _b;

	if (a->idx > b->idx)
		return (1);
	else if (a->idx < b->idx)
		return (-1);
	return (0);
}

static void
dump_nhgrp_sysctl(int fibnum, int af, struct nhops_dump *nd)
{
	size_t needed;
	int mib[7];
	char *buf, *next, *lim;
	struct rt_msghdr *rtm;
	struct nhgrp_external *nhg;
	struct nhops_map *nhg_map;
	size_t nhg_count, nhg_size;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = af;
	mib[4] = NET_RT_NHGRP;
	mib[5] = 0;
	mib[6] = fibnum;
	if (sysctl(mib, nitems(mib), NULL, &needed, NULL, 0) < 0)
		err(EX_OSERR, "sysctl: net.route.0.%d.nhgrpdump.%d estimate",
		    af, fibnum);
	if ((buf = malloc(needed)) == NULL)
		errx(2, "malloc(%lu)", (unsigned long)needed);
	if (sysctl(mib, nitems(mib), buf, &needed, NULL, 0) < 0)
		err(1, "sysctl: net.route.0.%d.nhgrpdump.%d", af, fibnum);
	lim  = buf + needed;

	/*
	 * nexhops groups are received unsorted. Collect everything first,
	 * and sort prior displaying.
	 */
	nhg_count = 0;
	nhg_size = 16;
	nhg_map = calloc(nhg_size, sizeof(struct nhops_map));
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		if (nhg_count >= nhg_size) {
			nhg_size *= 2;
			nhg_map = realloc(nhg_map, nhg_size * sizeof(struct nhops_map));
		}

		nhg = (struct nhgrp_external *)(rtm + 1);
		nhg_map[nhg_count].idx = nhg->nhg_idx;
		nhg_map[nhg_count].rtm = rtm;
		nhg_count++;
	}

	if (nhg_count > 0)
		qsort(nhg_map, nhg_count, sizeof(struct nhops_map), cmp_nhg_idx);
	nd->nh_buf = buf;
	nd->nh_count = nhg_count;
	nd->nh_map = nhg_map;
}

static void
print_nhgrp_sysctl(int fibnum, int af)
{
	struct nhops_dump nd;
	struct nhgrp_external *nhg;
	struct rt_msghdr *rtm;

	dump_nhgrp_sysctl(fibnum, af, &nd);

	xo_open_container("nhgrp-table");
	xo_open_list("rt-family");
	if (nd.nh_count > 0) {
		wid_gw = WID_GW_DEFAULT(af);
		xo_open_instance("rt-family");
		pr_family(af);
		xo_open_list("nhgrp-entry");

		print_nhgroup_header(af);

		for (size_t i = 0; i < nd.nh_count; i++) {
			rtm = nd.nh_map[i].rtm;
			nhg = (struct nhgrp_external *)(rtm + 1);
			print_nhgroup_entry_sysctl("nhgrp-entry", rtm, nhg);
		}
	}
	xo_close_list("rt-family");
	xo_close_container("nhgrp-table");
	free(nd.nh_buf);
}

static void
update_global_map(struct nhop_external *nh)
{
	char iface_name[128];
	char gw_addr[64];
	struct nhop_addrs *na;
	struct sockaddr *sa_gw;

	na = (struct nhop_addrs *)((char *)nh + nh->nh_len);
	sa_gw = (struct sockaddr *)((char *)na + na->gw_sa_off);

	memset(iface_name, 0, sizeof(iface_name));
	if (nh->ifindex < (uint32_t)ifmap_size) {
		strlcpy(iface_name, ifmap[nh->ifindex].ifname,
		    sizeof(iface_name));
		if (*iface_name == '\0')
			strlcpy(iface_name, "---", sizeof(iface_name));
	}

	if (nh->nh_flags & NHF_GATEWAY) {
		const char *cp;
		cp = fmt_sockaddr(sa_gw, NULL, RTF_HOST);
		strlcpy(gw_addr, cp, sizeof(gw_addr));
	} else
		snprintf(gw_addr, sizeof(gw_addr), "%s/resolve", iface_name);

	nhop_map_update(&global_nhop_map, nh->nh_idx, gw_addr, iface_name);
}

static void
prepare_nh_map(int fibnum, int af)
{
	struct nhops_dump nd;
	struct nhop_external *nh;
	struct rt_msghdr *rtm;

	dump_nhops_sysctl(fibnum, af, &nd);

	for (size_t i = 0; i < nd.nh_count; i++) {
		rtm = nd.nh_map[i].rtm;
		nh = (struct nhop_external *)(rtm + 1);
		update_global_map(nh);
	}

	free(nd.nh_buf);
}

void
nhgrp_print(int fibnum, int af)
{
	size_t intsize;
	int numfibs;

	intsize = sizeof(int);
	if (fibnum == -1 &&
	    sysctlbyname("net.my_fibnum", &fibnum, &intsize, NULL, 0) == -1)
		fibnum = 0;
	if (sysctlbyname("net.fibs", &numfibs, &intsize, NULL, 0) == -1)
		numfibs = 1;
	if (fibnum < 0 || fibnum > numfibs - 1)
		errx(EX_USAGE, "%d: invalid fib", fibnum);

	ifmap = prepare_ifmap(&ifmap_size);
	prepare_nh_map(fibnum, af);

	xo_open_container("route-nhgrp-information");
	xo_emit("{T:Nexthop groups data}");
	if (fibnum)
		xo_emit(" ({L:fib}: {:fib/%d})", fibnum);
	xo_emit("\n");
	print_nhgrp_sysctl(fibnum, af);
	xo_close_container("route-nhgrp-information");
}

