/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/route/nhop.h>

#include <netinet/in.h>
#include <netgraph/ng_socket.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <libutil.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdint.h>
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

/* column widths; each followed by one space */
#ifndef INET6
#define	WID_DST_DEFAULT(af) 	18	/* width of destination column */
#define	WID_GW_DEFAULT(af)	18	/* width of gateway column */
#define	WID_IF_DEFAULT(af)	(Wflag ? 10 : 8) /* width of netif column */
#else
#define	WID_DST_DEFAULT(af) \
	((af) == AF_INET6 ? (numeric_addr ? 33: 18) : 18)
#define	WID_GW_DEFAULT(af) \
	((af) == AF_INET6 ? (numeric_addr ? 29 : 18) : 18)
#define	WID_IF_DEFAULT(af)	((af) == AF_INET6 ? 8 : (Wflag ? 10 : 8))
#endif /*INET6*/
static int wid_dst;
static int wid_gw;
static int wid_flags;
static int wid_pksent;
static int wid_mtu;
static int wid_if;
static int wid_nhidx;
static int wid_nhtype;
static int wid_refcnt;
static int wid_prepend;

static struct bits nh_bits[] = {
	{ NHF_REJECT,	'R', "reject" },
	{ NHF_BLACKHOLE,'B', "blackhole" },
	{ NHF_REDIRECT,	'r', "redirect" },
	{ NHF_GATEWAY,	'G', "gateway" },
	{ NHF_DEFAULT,	'd', "default" },
	{ NHF_BROADCAST,'b', "broadcast" },
	{ 0 , 0, NULL }
};

static char *nh_types[] = {
	"empty", /* 0 */
	"v4/resolve", /* 1 */
	"v4/gw",
	"v6/resolve",
	"v6/gw"
};

struct nhop_entry {
	char	gw[64];
	char	ifname[IFNAMSIZ];
};

struct nhop_map {
	struct nhop_entry	*ptr;
	size_t			size;
};
static struct nhop_map global_nhop_map;

static struct nhop_entry *nhop_get(struct nhop_map *map, uint32_t idx);


static struct ifmap_entry *ifmap;
static size_t ifmap_size;

static void
print_sockaddr_buf(char *buf, size_t bufsize, const struct sockaddr *sa)
{

	switch (sa->sa_family) {
	case AF_INET:
		inet_ntop(AF_INET, &((struct sockaddr_in *)sa)->sin_addr,
		    buf, bufsize);
		break;
	case AF_INET6:
		inet_ntop(AF_INET6, &((struct sockaddr_in6 *)sa)->sin6_addr,
		    buf, bufsize);
		break;
	default:
		snprintf(buf, bufsize, "unknown:%d", sa->sa_family);
		break;
	}
}

static int
print_addr(const char *name, const char *addr, int width)
{
	char buf[128];
	int protrusion;

	if (width < 0) {
		snprintf(buf, sizeof(buf), "{:%s/%%s} ", name);
		xo_emit(buf, addr);
		protrusion = 0;
	} else {
		if (Wflag != 0 || numeric_addr) {
			snprintf(buf, sizeof(buf), "{[:%d}{:%s/%%s}{]:} ",
			    -width, name);
			xo_emit(buf, addr);
			protrusion = strlen(addr) - width;
			if (protrusion < 0)
				protrusion = 0;
		} else {
			snprintf(buf, sizeof(buf), "{[:%d}{:%s/%%-.*s}{]:} ",
			    -width, name);
			xo_emit(buf, width, addr);
			protrusion = 0;
		}
	}
	return (protrusion);
}


static void
print_nhop_header(int af1 __unused)
{

	if (Wflag) {
		xo_emit("{T:/%-*.*s} {T:/%-*.*s} {T:/%-*.*s} {T:/%-*.*s} {T:/%*.*s} "
		    "{T:/%*.*s} {T:/%-*.*s} {T:/%*.*s} {T:/%*.*s} {T:/%*.*s} {T:/%*s}\n",
			wid_nhidx,	wid_nhidx,	"Idx",
			wid_nhtype,	wid_nhtype,	"Type",
			wid_dst,	wid_dst,	"IFA",
			wid_gw,		wid_gw,		"Gateway",
			wid_flags,	wid_flags,	"Flags",
			wid_pksent,	wid_pksent,	"Use",
			wid_mtu,	wid_mtu,	"Mtu",
			wid_if,		wid_if,		"Netif",
			wid_if,		wid_if,		"Addrif",
			wid_refcnt,	wid_refcnt,	"Refcnt",
			wid_prepend,			"Prepend");
	} else {
		xo_emit("{T:/%-*.*s} {T:/%-*.*s} {T:/%-*.*s} {T:/%-*.*s} {T:/%*.*s} "
		    " {T:/%*s}\n",
			wid_nhidx,	wid_nhidx,	"Idx",
			wid_dst,	wid_dst,	"IFA",
			wid_gw,		wid_gw,		"Gateway",
			wid_flags,	wid_flags,	"Flags",
			wid_if,		wid_if,		"Netif",
			wid_prepend,			"Refcnt");
	}
}

void
nhop_map_update(struct nhop_map *map, uint32_t idx, char *gw, char *ifname)
{
	if (idx >= map->size) {
		uint32_t new_size;
		size_t sz;
		if (map->size  == 0)
			new_size = 32;
		else
			new_size = map->size * 2;
		if (new_size <= idx)
			new_size = roundup(idx + 1, 32);

		sz = new_size * (sizeof(struct nhop_entry));
		if ((map->ptr = realloc(map->ptr, sz)) == NULL)
			errx(2, "realloc(%zu) failed", sz);

		memset(&map->ptr[map->size], 0, (new_size - map->size) * sizeof(struct nhop_entry));
		map->size = new_size;
	}

	strlcpy(map->ptr[idx].ifname, ifname, sizeof(map->ptr[idx].ifname));
	strlcpy(map->ptr[idx].gw, gw, sizeof(map->ptr[idx].gw));
}

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
print_nhop_entry_sysctl(const char *name, struct rt_msghdr *rtm, struct nhop_external *nh)
{
	char buffer[128];
	char iface_name[128];
	int protrusion;
	char gw_addr[64];
	struct nhop_addrs *na;
	struct sockaddr *sa_gw, *sa_ifa;

	xo_open_instance(name);

	snprintf(buffer, sizeof(buffer), "{[:-%d}{:index/%%lu}{]:} ", wid_nhidx);
	//xo_emit("{t:index/%-lu} ", wid_nhidx, nh->nh_idx);
	xo_emit(buffer, nh->nh_idx);

	if (Wflag) {
		char *cp = nh_types[nh->nh_type];
		xo_emit("{t:type_str/%*s} ", wid_nhtype, cp);
	}
	memset(iface_name, 0, sizeof(iface_name));
	if (nh->ifindex < (uint32_t)ifmap_size) {
		strlcpy(iface_name, ifmap[nh->ifindex].ifname,
		    sizeof(iface_name));
		if (*iface_name == '\0')
			strlcpy(iface_name, "---", sizeof(iface_name));
	}

	na = (struct nhop_addrs *)((char *)nh + nh->nh_len);
	//inet_ntop(nh->nh_family, &nh->nh_src, src_addr, sizeof(src_addr));
	//protrusion = p_addr("ifa", src_addr, wid_dst);
	sa_gw = (struct sockaddr *)((char *)na + na->gw_sa_off);
	sa_ifa = (struct sockaddr *)((char *)na + na->src_sa_off);
	protrusion = p_sockaddr("ifa", sa_ifa, NULL, RTF_HOST, wid_dst);

	if (nh->nh_flags & NHF_GATEWAY) {
		const char *cp;
		cp = fmt_sockaddr(sa_gw, NULL, RTF_HOST);
		strlcpy(gw_addr, cp, sizeof(gw_addr));
	} else
		snprintf(gw_addr, sizeof(gw_addr), "%s/resolve", iface_name);
	protrusion = print_addr("gateway", gw_addr, wid_dst - protrusion);

	nhop_map_update(&global_nhop_map, nh->nh_idx, gw_addr, iface_name);

	snprintf(buffer, sizeof(buffer), "{[:-%d}{:flags/%%s}{]:} ",
	    wid_flags - protrusion);

	//p_nhflags(nh->nh_flags, buffer);
	print_flags_generic(rtm->rtm_flags, rt_bits, buffer, "rt_flags_pretty");

	if (Wflag) {
		xo_emit("{t:use/%*lu} ", wid_pksent, nh->nh_pksent);
		xo_emit("{t:mtu/%*lu} ", wid_mtu, nh->nh_mtu);
	}
	//printf("IDX: %d IFACE: %s FAMILY: %d TYPE: %d FLAGS: %X GW \n");

	if (Wflag)
		xo_emit("{t:interface-name/%*s}", wid_if, iface_name);
	else
		xo_emit("{t:interface-name/%*.*s}", wid_if, wid_if, iface_name);

	memset(iface_name, 0, sizeof(iface_name));
	if (nh->aifindex < (uint32_t)ifmap_size && nh->ifindex != nh->aifindex) {
		strlcpy(iface_name, ifmap[nh->aifindex].ifname,
		    sizeof(iface_name));
		if (*iface_name == '\0')
			strlcpy(iface_name, "---", sizeof(iface_name));
	}
	if (Wflag)
		xo_emit("{t:address-interface-name/%*s}", wid_if, iface_name);

	xo_emit("{t:refcount/%*lu} ", wid_refcnt, nh->nh_refcount);
	if (Wflag && nh->prepend_len) {
		char *prepend_hex = "AABBCCDDEE";
		xo_emit(" {:nhop-prepend/%*s}", wid_prepend, prepend_hex);
	}

	xo_emit("\n");
	xo_close_instance(name);
}

static int
cmp_nh_idx(const void *_a, const void *_b)
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

void
dump_nhops_sysctl(int fibnum, int af, struct nhops_dump *nd)
{
	size_t needed;
	int mib[7];
	char *buf, *next, *lim;
	struct rt_msghdr *rtm;
	struct nhop_external *nh;
	struct nhops_map *nh_map;
	size_t nh_count, nh_size;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = af;
	mib[4] = NET_RT_NHOP;
	mib[5] = 0;
	mib[6] = fibnum;
	if (sysctl(mib, nitems(mib), NULL, &needed, NULL, 0) < 0)
		err(EX_OSERR, "sysctl: net.route.0.%d.nhdump.%d estimate", af,
		    fibnum);
	if ((buf = malloc(needed)) == NULL)
		errx(2, "malloc(%lu)", (unsigned long)needed);
	if (sysctl(mib, nitems(mib), buf, &needed, NULL, 0) < 0)
		err(1, "sysctl: net.route.0.%d.nhdump.%d", af, fibnum);
	lim  = buf + needed;

	/*
	 * nexhops are received unsorted. Collect everything first, sort and then display
	 * sorted.
	 */
	nh_count = 0;
	nh_size = 16;
	nh_map = calloc(nh_size, sizeof(struct nhops_map));
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		if (nh_count >= nh_size) {
			nh_size *= 2;
			nh_map = realloc(nh_map, nh_size * sizeof(struct nhops_map));
		}

		nh = (struct nhop_external *)(rtm + 1); 
		nh_map[nh_count].idx = nh->nh_idx;
		nh_map[nh_count].rtm = rtm;
		nh_count++;
	}

	if (nh_count > 0)
		qsort(nh_map, nh_count, sizeof(struct nhops_map), cmp_nh_idx);
	nd->nh_buf = buf;
	nd->nh_count = nh_count;
	nd->nh_map = nh_map;
}

static void
print_nhops_sysctl(int fibnum, int af)
{
	struct nhops_dump nd;
	struct nhop_external *nh;
	int fam;
	struct rt_msghdr *rtm;

	dump_nhops_sysctl(fibnum, af, &nd);

	xo_open_container("nhop-table");
	xo_open_list("rt-family");
	if (nd.nh_count > 0) {
		nh = (struct nhop_external *)(nd.nh_map[0].rtm + 1);
		fam = nh->nh_family;

		wid_dst = WID_GW_DEFAULT(fam);
		wid_gw = WID_GW_DEFAULT(fam);
		wid_nhidx = 5;
		wid_nhtype = 12;
		wid_refcnt = 6;
		wid_flags = 6;
		wid_pksent = 8;
		wid_mtu = 6;
		wid_if = WID_IF_DEFAULT(fam);
		xo_open_instance("rt-family");
		pr_family(fam);
		xo_open_list("nh-entry");

		print_nhop_header(fam);

		for (size_t i = 0; i < nd.nh_count; i++) {
			rtm = nd.nh_map[i].rtm;
			nh = (struct nhop_external *)(rtm + 1);
			print_nhop_entry_sysctl("nh-entry", rtm, nh);
		}

		xo_close_list("nh-entry");
		xo_close_instance("rt-family");
	}
	xo_close_list("rt-family");
	xo_close_container("nhop-table");
	free(nd.nh_buf);
}

static void
p_nhflags(int f, const char *format)
{
	struct bits *p;
	char *pretty_name = "nh_flags_pretty";

	xo_emit(format, fmt_flags(nh_bits, f));

	xo_open_list(pretty_name);
	for (p = nh_bits; p->b_mask; p++)
		if (p->b_mask & f)
			xo_emit("{le:nh_flags_pretty/%s}", p->b_name);
	xo_close_list(pretty_name);
}

void
nhops_print(int fibnum, int af)
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

	xo_open_container("route-nhop-information");
	xo_emit("{T:Nexthop data}");
	if (fibnum)
		xo_emit(" ({L:fib}: {:fib/%d})", fibnum);
	xo_emit("\n");
	print_nhops_sysctl(fibnum, af);
	xo_close_container("route-nhop-information");
}

