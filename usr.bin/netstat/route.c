/*-
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
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#if 0
#ifndef lint
static char sccsid[] = "From: @(#)route.c	8.6 (Berkeley) 4/28/95";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/radix.h>
#define	_WANT_RTENTRY
#include <net/route.h>

#include <netinet/in.h>
#include <netgraph/ng_socket.h>

#include <sys/sysctl.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <libutil.h>
#include <netdb.h>
#include <nlist.h>
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

#define	kget(p, d) (kread((u_long)(p), (char *)&(d), sizeof (d)))

/*
 * Definitions for showing gateway flags.
 */
struct bits {
	u_long	b_mask;
	char	b_val;
	const char *b_name;
} bits[] = {
	{ RTF_UP,	'U', "up" },
	{ RTF_GATEWAY,	'G', "gateway" },
	{ RTF_HOST,	'H', "host" },
	{ RTF_REJECT,	'R', "reject" },
	{ RTF_DYNAMIC,	'D', "dynamic" },
	{ RTF_MODIFIED,	'M', "modified" },
	{ RTF_DONE,	'd', "done" }, /* Completed -- for routing msgs only */
	{ RTF_XRESOLVE,	'X', "xresolve" },
	{ RTF_STATIC,	'S', "static" },
	{ RTF_PROTO1,	'1', "proto1" },
	{ RTF_PROTO2,	'2', "proto2" },
	{ RTF_PROTO3,	'3', "proto3" },
	{ RTF_BLACKHOLE,'B', "blackhole" },
	{ RTF_BROADCAST,'b', "broadcast" },
#ifdef RTF_LLINFO
	{ RTF_LLINFO,	'L', "llinfo" },
#endif
	{ 0 , 0, NULL }
};

/*
 * kvm(3) bindings for every needed symbol
 */
static struct nlist rl[] = {
#define	N_RTSTAT	0
	{ .n_name = "_rtstat" },
#define	N_RTREE		1
	{ .n_name = "_rt_tables"},
#define	N_RTTRASH	2
	{ .n_name = "_rttrash" },
	{ .n_name = NULL },
};

typedef union {
	long	dummy;		/* Helps align structure. */
	struct	sockaddr u_sa;
	u_short	u_data[128];
} sa_u;

static sa_u pt_u;

struct ifmap_entry {
	char ifname[IFNAMSIZ];
};

static struct ifmap_entry *ifmap;
static int ifmap_size;

int	do_rtent = 0;
struct	rtentry rtentry;
struct	radix_node rnode;
struct	radix_mask rmask;

int	NewTree = 1;

struct	timespec uptime;

static struct sockaddr *kgetsa(struct sockaddr *);
static void size_cols(int ef, struct radix_node *rn);
static void size_cols_tree(struct radix_node *rn);
static void size_cols_rtentry(struct rtentry *rt);
static void p_rtnode_kvm(void);
static void p_rtable_sysctl(int, int);
static void p_rtable_kvm(int, int );
static void p_rtree_kvm(const char *name, struct radix_node *);
static void p_rtentry_kvm(const char *name, struct rtentry *);
static void p_rtentry_sysctl(const char *name, struct rt_msghdr *);
static void p_sockaddr(const char *name, struct sockaddr *, struct sockaddr *,
    int, int);
static const char *fmt_sockaddr(struct sockaddr *sa, struct sockaddr *mask,
    int flags);
static void p_flags(int, const char *);
static const char *fmt_flags(int f);
static void domask(char *, in_addr_t, u_long);

/*
 * Print routing tables.
 */
void
routepr(int fibnum, int af)
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
	/*
	 * Since kernel & userland use different timebase
	 * (time_uptime vs time_second) and we are reading kernel memory
	 * directly we should do rt_expire --> expire_time conversion.
	 */
	if (clock_gettime(CLOCK_UPTIME, &uptime) < 0)
		err(EX_OSERR, "clock_gettime() failed");

	xo_open_container("route-information");
	xo_emit("{T:Routing tables}");
	if (fibnum)
		xo_emit(" ({L:fib}: {:fib/%d})", fibnum);
	xo_emit("\n");

	if (Aflag == 0 && live != 0 && NewTree)
		p_rtable_sysctl(fibnum, af);
	else
		p_rtable_kvm(fibnum, af);
	xo_close_container("route-information");
}


/*
 * Print address family header before a section of the routing table.
 */
void
pr_family(int af1)
{
	const char *afname;

	switch (af1) {
	case AF_INET:
		afname = "Internet";
		break;
#ifdef INET6
	case AF_INET6:
		afname = "Internet6";
		break;
#endif /*INET6*/
	case AF_ISO:
		afname = "ISO";
		break;
	case AF_CCITT:
		afname = "X.25";
		break;
	case AF_NETGRAPH:
		afname = "Netgraph";
		break;
	default:
		afname = NULL;
		break;
	}
	if (afname)
		xo_emit("\n{k:address-family/%s}:\n", afname);
	else
		xo_emit("\n{L:Protocol Family} {k:address-family/%d}:\n", af1);
}

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
static int wid_expire;

static void
size_cols(int ef, struct radix_node *rn)
{
	wid_dst = WID_DST_DEFAULT(ef);
	wid_gw = WID_GW_DEFAULT(ef);
	wid_flags = 6;
	wid_pksent = 8;
	wid_mtu = 6;
	wid_if = WID_IF_DEFAULT(ef);
	wid_expire = 6;

	if (Wflag && rn != NULL)
		size_cols_tree(rn);
}

static void
size_cols_tree(struct radix_node *rn)
{
again:
	if (kget(rn, rnode) != 0)
		return;
	if (!(rnode.rn_flags & RNF_ACTIVE))
		return;
	if (rnode.rn_bit < 0) {
		if ((rnode.rn_flags & RNF_ROOT) == 0) {
			if (kget(rn, rtentry) != 0)
				return;
			size_cols_rtentry(&rtentry);
		}
		if ((rn = rnode.rn_dupedkey))
			goto again;
	} else {
		rn = rnode.rn_right;
		size_cols_tree(rnode.rn_left);
		size_cols_tree(rn);
	}
}

static void
size_cols_rtentry(struct rtentry *rt)
{
	static struct ifnet ifnet, *lastif;
	static char buffer[100];
	const char *bp;
	struct sockaddr *sa;
	sa_u addr, mask;
	int len;

	bzero(&addr, sizeof(addr));
	if ((sa = kgetsa(rt_key(rt))))
		bcopy(sa, &addr, sa->sa_len);
	bzero(&mask, sizeof(mask));
	if (rt_mask(rt) && (sa = kgetsa(rt_mask(rt))))
		bcopy(sa, &mask, sa->sa_len);
	bp = fmt_sockaddr(&addr.u_sa, &mask.u_sa, rt->rt_flags);
	len = strlen(bp);
	wid_dst = MAX(len, wid_dst);

	bp = fmt_sockaddr(kgetsa(rt->rt_gateway), NULL, RTF_HOST);
	len = strlen(bp);
	wid_gw = MAX(len, wid_gw);

	bp = fmt_flags(rt->rt_flags);
	len = strlen(bp);
	wid_flags = MAX(len, wid_flags);

	if (Wflag) {
		len = snprintf(buffer, sizeof(buffer), "%ju",
		    (uintmax_t )kread_counter((u_long )rt->rt_pksent));
		wid_pksent = MAX(len, wid_pksent);
	}
	if (rt->rt_ifp) {
		if (rt->rt_ifp != lastif) {
			if (kget(rt->rt_ifp, ifnet) == 0)
				len = strlen(ifnet.if_xname);
			else
				len = strlen("---");
			lastif = rt->rt_ifp;
			wid_if = MAX(len, wid_if);
		}
		if (rt->rt_expire) {
			time_t expire_time;

			if ((expire_time =
			    rt->rt_expire - uptime.tv_sec) > 0) {
				len = snprintf(buffer, sizeof(buffer), "%d",
				    (int)expire_time);
				wid_expire = MAX(len, wid_expire);
			}
		}
	}
}


/*
 * Print header for routing table columns.
 */
void
pr_rthdr(int af1)
{

	if (Aflag)
		xo_emit("{T:/%-8.8s} ","Address");
	if (Wflag) {
		xo_emit("{T:/%-*.*s} {T:/%-*.*s} {T:/%-*.*s} {T:/%*.*s} "
		    "{T:/%*.*s} {T:/%*.*s} {T:/%*.*s} {T:/%*s}\n",
			wid_dst,	wid_dst,	"Destination",
			wid_gw,		wid_gw,		"Gateway",
			wid_flags,	wid_flags,	"Flags",
			wid_pksent,	wid_pksent,	"Use",
			wid_mtu,	wid_mtu,	"Mtu",
			wid_if,		wid_if,		"Netif",
			wid_expire,			"Expire");
	} else {
		xo_emit("{T:/%-*.*s} {T:/%-*.*s} {T:/%-*.*s}  {T:/%*.*s} "
		    "{T:/%*s}\n",
			wid_dst,	wid_dst,	"Destination",
			wid_gw,		wid_gw,		"Gateway",
			wid_flags,	wid_flags,	"Flags",
			wid_if,		wid_if,		"Netif",
			wid_expire,			"Expire");
	}
}

static struct sockaddr *
kgetsa(struct sockaddr *dst)
{

	if (kget(dst, pt_u.u_sa) != 0)
		return (NULL);
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, (char *)pt_u.u_data, pt_u.u_sa.sa_len);
	return (&pt_u.u_sa);
}

/*
 * Print kernel routing tables for given fib
 * using debugging kvm(3) interface.
 */
static void
p_rtable_kvm(int fibnum, int af)
{
	struct radix_node_head **rnhp, *rnh, head;
	struct radix_node_head **rt_tables;
	u_long rtree;
	int fam, af_size;
	bool did_rt_family = false;

	kresolve_list(rl);
	if ((rtree = rl[N_RTREE].n_value) == 0) {
		xo_emit("rt_tables: symbol not in namelist\n");
		return;
	}

	af_size = (AF_MAX + 1) * sizeof(struct radix_node_head *);
	rt_tables = calloc(1, af_size);
	if (rt_tables == NULL)
		err(EX_OSERR, "memory allocation failed");

	if (kread((u_long)(rtree), (char *)(rt_tables) + fibnum * af_size,
	    af_size) != 0)
		err(EX_OSERR, "error retrieving radix pointers");
	xo_open_container("route-table");
	for (fam = 0; fam <= AF_MAX; fam++) {
		int tmpfib;

		switch (fam) {
		case AF_INET6:
		case AF_INET:
			tmpfib = fibnum;
			break;
		default:
			tmpfib = 0;
		}
		rnhp = (struct radix_node_head **)*rt_tables;
		/* Calculate the in-kernel address. */
		rnhp += tmpfib * (AF_MAX + 1) + fam;
		/* Read the in kernel rhn pointer. */
		if (kget(rnhp, rnh) != 0)
			continue;
		if (rnh == NULL)
			continue;
		/* Read the rnh data. */
		if (kget(rnh, head) != 0)
			continue;
		if (fam == AF_UNSPEC) {
			if (Aflag && af == 0) {
				xo_emit("{T:Netmasks}:\n");
				xo_open_list("netmasks");
				p_rtree_kvm("netmasks", head.rnh_treetop);
				xo_close_list("netmasks");
			}
		} else if (af == AF_UNSPEC || af == fam) {
			if (!did_rt_family) {
				xo_open_list("rt-family");
				did_rt_family = true;
			}
			size_cols(fam, head.rnh_treetop);
			xo_open_instance("rt-family");
			pr_family(fam);
			do_rtent = 1;
			xo_open_list("rt-entry");
			pr_rthdr(fam);
			p_rtree_kvm("rt-entry", head.rnh_treetop);
			xo_close_list("rt-entry");
			xo_close_instance("rt-family");
		}
	}
	if (did_rt_family)
		xo_close_list("rt-family");
	xo_close_container("route-table");

	free(rt_tables);
}

/*
 * Print given kernel radix tree using
 * debugging kvm(3) interface.
 */
static void
p_rtree_kvm(const char *name, struct radix_node *rn)
{
	bool opened;

	opened = false;

#define DOOPEN()    do { \
	if (!opened) { xo_open_instance(name); opened = true; } \
    } while (0)
#define DOCLOSE()   do { \
	if (opened) { opened = false; xo_close_instance(name); } \
    } while(0)

again:
	if (kget(rn, rnode) != 0)
		return;
	if (!(rnode.rn_flags & RNF_ACTIVE))
		return;
	if (rnode.rn_bit < 0) {
		if (Aflag) {
			DOOPEN();
			xo_emit("{q:radix-node/%-8.8lx} ", (u_long)rn);
		}
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag) {
				DOOPEN();
				xo_emit("({:root/root} node){L:/%s}",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
			}
		} else if (do_rtent) {
			if (kget(rn, rtentry) == 0) {
				DOOPEN();
				p_rtentry_kvm(name, &rtentry);
				if (Aflag) {
					DOOPEN();
					p_rtnode_kvm();
					DOCLOSE();
				}
			}
		} else {
			DOOPEN();
			p_sockaddr("address",
			    kgetsa((struct sockaddr *)rnode.rn_key),
			    NULL, 0, 44);
			xo_emit("\n");
		}
		DOCLOSE();
		if ((rn = rnode.rn_dupedkey))
			goto again;
	} else {
		if (Aflag && do_rtent) {
			DOOPEN();
			xo_emit("{q:radix-node/%-8.8lx} ", (u_long)rn);
			p_rtnode_kvm();
			DOCLOSE();
		}
		rn = rnode.rn_right;
		p_rtree_kvm(name, rnode.rn_left);
		p_rtree_kvm(name, rn);
	}
}

char	nbuf[20];

static void
p_rtnode_kvm(void)
{
	struct radix_mask *rm = rnode.rn_mklist;

	if (rnode.rn_bit < 0) {
		if (rnode.rn_mask) {
			xo_emit("\t  {L:mask} ");
			p_sockaddr("netmask",
			    kgetsa((struct sockaddr *)rnode.rn_mask),
			    NULL, 0, -1);
		} else if (rm == 0)
			return;
	} else {
		xo_emit("{[:6}{:bit/(%d)}{]:} {q:left-node/%8.8lx} "
		    ": {q:right-node/%8.8lx}", rnode.rn_bit,
		    (u_long)rnode.rn_left, (u_long)rnode.rn_right);
	}
	while (rm) {
		if (kget(rm, rmask) != 0)
			break;
		sprintf(nbuf, " %d refs, ", rmask.rm_refs);
		xo_emit(" mk = {q:node/%8.8lx} \\{({:bit/%d}),{nbufs/%s}",
		    (u_long)rm, -1 - rmask.rm_bit, rmask.rm_refs ? nbuf : " ");
		if (rmask.rm_flags & RNF_NORMAL) {
			struct radix_node rnode_aux;
			xo_emit(" <{:mode/normal}>, ");
			if (kget(rmask.rm_leaf, rnode_aux) == 0)
				p_sockaddr("netmask",
				    kgetsa(/*XXX*/(void *)rnode_aux.rn_mask),
				    NULL, 0, -1);
			else
				p_sockaddr(NULL, NULL, NULL, 0, -1);
		} else
			p_sockaddr("netmask",
			    kgetsa((struct sockaddr *)rmask.rm_mask),
			    NULL, 0, -1);
		xo_emit("\\}");
		if ((rm = rmask.rm_mklist))
			xo_emit(" {D:->}");
	}
	xo_emit("\n");
}

static void
p_rtable_sysctl(int fibnum, int af)
{
	size_t needed;
	int mib[7];
	char *buf, *next, *lim;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	int fam = AF_UNSPEC, ifindex = 0, size;
	int need_table_close = false;

	struct ifaddrs *ifap, *ifa;
	struct sockaddr_dl *sdl;

	/*
	 * Retrieve interface list at first
	 * since we need #ifindex -> if_xname match
	 */
	if (getifaddrs(&ifap) != 0)
		err(EX_OSERR, "getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		ifindex = sdl->sdl_index;

		if (ifindex >= ifmap_size) {
			size = roundup(ifindex + 1, 32) *
			    sizeof(struct ifmap_entry);
			if ((ifmap = realloc(ifmap, size)) == NULL)
				errx(2, "realloc(%d) failed", size);
			memset(&ifmap[ifmap_size], 0,
			    size - ifmap_size *
			    sizeof(struct ifmap_entry));

			ifmap_size = roundup(ifindex + 1, 32);
		}

		if (*ifmap[ifindex].ifname != '\0')
			continue;

		strlcpy(ifmap[ifindex].ifname, ifa->ifa_name, IFNAMSIZ);
	}

	freeifaddrs(ifap);

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = af;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = fibnum;
	if (sysctl(mib, nitems(mib), NULL, &needed, NULL, 0) < 0)
		err(EX_OSERR, "sysctl: net.route.0.%d.dump.%d estimate", af,
		    fibnum);
	if ((buf = malloc(needed)) == NULL)
		errx(2, "malloc(%lu)", (unsigned long)needed);
	if (sysctl(mib, nitems(mib), buf, &needed, NULL, 0) < 0)
		err(1, "sysctl: net.route.0.%d.dump.%d", af, fibnum);
	lim  = buf + needed;
	xo_open_container("route-table");
	xo_open_list("rt-family");
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		/*
		 * Peek inside header to determine AF
		 */
		sa = (struct sockaddr *)(rtm + 1);
		/* Only print family first time. */
		if (fam != sa->sa_family) {
			if (need_table_close) {
				xo_close_list("rt-entry");
				xo_close_instance("rt-family");
			}
			need_table_close = true;

			fam = sa->sa_family;
			size_cols(fam, NULL);
			xo_open_instance("rt-family");
			pr_family(fam);
			xo_open_list("rt-entry");

			pr_rthdr(fam);
		}
		p_rtentry_sysctl("rt-entry", rtm);
	}
	if (need_table_close) {
		xo_close_list("rt-entry");
		xo_close_instance("rt-family");
	}
	xo_close_list("rt-family");
	xo_close_container("route-table");
	free(buf);
}

static void
p_rtentry_sysctl(const char *name, struct rt_msghdr *rtm)
{
	struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
	char buffer[128];
	char prettyname[128];
	sa_u addr, mask, gw;
	unsigned int l;

	xo_open_instance(name);

#define	GETSA(_s, _f)	{ \
	bzero(&(_s), sizeof(_s)); \
	if (rtm->rtm_addrs & _f) { \
		l = roundup(sa->sa_len, sizeof(long)); \
		memcpy(&(_s), sa, (l > sizeof(_s)) ? sizeof(_s) : l); \
		sa = (struct sockaddr *)((char *)sa + l); \
	} \
}

	GETSA(addr, RTA_DST);
	GETSA(gw, RTA_GATEWAY);
	GETSA(mask, RTA_NETMASK);

	p_sockaddr("destination", &addr.u_sa, &mask.u_sa, rtm->rtm_flags,
	    wid_dst);
	p_sockaddr("gateway", &gw.u_sa, NULL, RTF_HOST, wid_gw);
	snprintf(buffer, sizeof(buffer), "{[:-%d}{:flags/%%s}{]:}",
	    wid_flags);
	p_flags(rtm->rtm_flags, buffer);
	if (Wflag) {
		xo_emit("{t:use/%*lu} ", wid_pksent, rtm->rtm_rmx.rmx_pksent);

		if (rtm->rtm_rmx.rmx_mtu != 0)
			xo_emit("{t:mtu/%*lu} ", wid_mtu, rtm->rtm_rmx.rmx_mtu);
		else
			xo_emit("{P:/%*s} ", wid_mtu, "");
	}

	memset(prettyname, 0, sizeof(prettyname));
	if (rtm->rtm_index < ifmap_size) {
		strlcpy(prettyname, ifmap[rtm->rtm_index].ifname,
		    sizeof(prettyname));
		if (*prettyname == '\0')
			strlcpy(prettyname, "---", sizeof(prettyname));
	}

	xo_emit("{t:interface-name/%*.*s}", wid_if, wid_if, prettyname);
	if (rtm->rtm_rmx.rmx_expire) {
		time_t expire_time;

		if ((expire_time = rtm->rtm_rmx.rmx_expire - uptime.tv_sec) > 0)
			xo_emit(" {:expire-time/%*d}", wid_expire,
			    (int)expire_time);
	}

	xo_emit("\n");
	xo_close_instance(name);
}

static void
p_sockaddr(const char *name, struct sockaddr *sa, struct sockaddr *mask,
    int flags, int width)
{
	const char *cp;
	char buf[128];

	cp = fmt_sockaddr(sa, mask, flags);

	if (width < 0) {
		snprintf(buf, sizeof(buf), "{:%s/%%s} ", name);
		xo_emit(buf, cp);
	} else {
		if (numeric_addr) {
			snprintf(buf, sizeof(buf), "{[:%d}{:%s/%%s}{]:} ",
			    -width, name);
			xo_emit(buf, cp);
		} else {
			snprintf(buf, sizeof(buf), "{[:%d}{:%s/%%-.*s}{]:} ",
			    -width, name);
			xo_emit(buf, width, cp);
		}
	}
}

static const char *
fmt_sockaddr(struct sockaddr *sa, struct sockaddr *mask, int flags)
{
	static char workbuf[128];
	const char *cp;

	if (sa == NULL)
		return ("null");

	switch(sa->sa_family) {
	case AF_INET:
	    {
		struct sockaddr_in *sockin = (struct sockaddr_in *)sa;

		if ((sockin->sin_addr.s_addr == INADDR_ANY) &&
			mask &&
			ntohl(((struct sockaddr_in *)mask)->sin_addr.s_addr)
				==0L)
				cp = "default" ;
		else if (flags & RTF_HOST)
			cp = routename(sockin->sin_addr.s_addr);
		else if (mask)
			cp = netname(sockin->sin_addr.s_addr,
			    ((struct sockaddr_in *)mask)->sin_addr.s_addr);
		else
			cp = netname(sockin->sin_addr.s_addr, INADDR_ANY);
		break;
	    }

#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;

		/*
		 * The sa6->sin6_scope_id must be filled here because
		 * this sockaddr is extracted from kmem(4) directly
		 * and has KAME-specific embedded scope id in
		 * sa6->sin6_addr.s6_addr[2].
		 */
		in6_fillscopeid(sa6);

		if (flags & RTF_HOST)
		    cp = routename6(sa6);
		else if (mask)
		    cp = netname6(sa6,
				  &((struct sockaddr_in6 *)mask)->sin6_addr);
		else {
		    cp = netname6(sa6, NULL);
		}
		break;
	    }
#endif /*INET6*/

	case AF_NETGRAPH:
	    {
		strlcpy(workbuf, ((struct sockaddr_ng *)sa)->sg_data,
		    sizeof(workbuf));
		cp = workbuf;
		break;
	    }

	case AF_LINK:
	    {
		struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

		if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 &&
		    sdl->sdl_slen == 0) {
			(void) sprintf(workbuf, "link#%d", sdl->sdl_index);
			cp = workbuf;
		} else
			switch (sdl->sdl_type) {

			case IFT_ETHER:
			case IFT_L2VLAN:
			case IFT_BRIDGE:
				if (sdl->sdl_alen == ETHER_ADDR_LEN) {
					cp = ether_ntoa((struct ether_addr *)
					    (sdl->sdl_data + sdl->sdl_nlen));
					break;
				}
				/* FALLTHROUGH */
			default:
				cp = link_ntoa(sdl);
				break;
			}
		break;
	    }

	default:
	    {
		u_char *s = (u_char *)sa->sa_data, *slim;
		char *cq, *cqlim;

		cq = workbuf;
		slim =  sa->sa_len + (u_char *) sa;
		cqlim = cq + sizeof(workbuf) - 6;
		cq += sprintf(cq, "(%d)", sa->sa_family);
		while (s < slim && cq < cqlim) {
			cq += sprintf(cq, " %02x", *s++);
			if (s < slim)
			    cq += sprintf(cq, "%02x", *s++);
		}
		cp = workbuf;
	    }
	}

	return (cp);
}

static void
p_flags(int f, const char *format)
{
	struct bits *p;

	xo_emit(format, fmt_flags(f));

	xo_open_list("flags_pretty");
	for (p = bits; p->b_mask; p++)
		if (p->b_mask & f)
			xo_emit("{le:flags_pretty/%s}", p->b_name);
	xo_close_list("flags_pretty");
}

static const char *
fmt_flags(int f)
{
	static char name[33];
	char *flags;
	struct bits *p = bits;

	for (flags = name; p->b_mask; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	return (name);
}

static void
p_rtentry_kvm(const char *name, struct rtentry *rt)
{
	static struct ifnet ifnet, *lastif;
	static char buffer[128];
	static char prettyname[128];
	struct sockaddr *sa;
	sa_u addr, mask;

	bzero(&addr, sizeof(addr));
	if ((sa = kgetsa(rt_key(rt))))
		bcopy(sa, &addr, sa->sa_len);
	bzero(&mask, sizeof(mask));
	if (rt_mask(rt) && (sa = kgetsa(rt_mask(rt))))
		bcopy(sa, &mask, sa->sa_len);

	p_sockaddr("destination", &addr.u_sa, &mask.u_sa, rt->rt_flags,
	    wid_dst);
	p_sockaddr("gateway", kgetsa(rt->rt_gateway), NULL, RTF_HOST, wid_gw);
	snprintf(buffer, sizeof(buffer), "{[:-%d}{:flags/%%s}{]:}",
	    wid_flags);
	p_flags(rt->rt_flags, buffer);
	if (Wflag) {
		xo_emit("{[:%d}{t:use/%ju}{]:} ", -wid_pksent,
		    (uintmax_t )kread_counter((u_long )rt->rt_pksent));

		if (rt->rt_mtu != 0)
			xo_emit("{t:mtu/%*lu} ", wid_mtu, rt->rt_mtu);
		else
			xo_emit("{P:/%*s} ", wid_mtu, "");
	}
	if (rt->rt_ifp) {
		if (rt->rt_ifp != lastif) {
			if (kget(rt->rt_ifp, ifnet) == 0)
				strlcpy(prettyname, ifnet.if_xname,
				    sizeof(prettyname));
			else
				strlcpy(prettyname, "---", sizeof(prettyname));
			lastif = rt->rt_ifp;
		}
		xo_emit("{t:interface-name/%*.*s}", wid_if, wid_if, prettyname);
		if (rt->rt_expire) {
			time_t expire_time;

			if ((expire_time =
			    rt->rt_expire - uptime.tv_sec) > 0)
				xo_emit(" {:expire-time/%*d}",
				    wid_expire, (int)expire_time);
		}
		if (rt->rt_nodes[0].rn_dupedkey)
			xo_emit(" =>");
	}
	xo_emit("\n");
}

char *
routename(in_addr_t in)
{
	char *cp;
	static char line[MAXHOSTNAMELEN];
	struct hostent *hp;

	cp = 0;
	if (!numeric_addr) {
		hp = gethostbyaddr(&in, sizeof (struct in_addr), AF_INET);
		if (hp) {
			cp = hp->h_name;
			trimdomain(cp, strlen(cp));
		}
	}
	if (cp) {
		strlcpy(line, cp, sizeof(line));
	} else {
#define	C(x)	((x) & 0xff)
		in = ntohl(in);
		sprintf(line, "%u.%u.%u.%u",
		    C(in >> 24), C(in >> 16), C(in >> 8), C(in));
	}
	return (line);
}

#define	NSHIFT(m) (							\
	(m) == IN_CLASSA_NET ? IN_CLASSA_NSHIFT :			\
	(m) == IN_CLASSB_NET ? IN_CLASSB_NSHIFT :			\
	(m) == IN_CLASSC_NET ? IN_CLASSC_NSHIFT :			\
	0)

static void
domask(char *dst, in_addr_t addr __unused, u_long mask)
{
	int b, i;

	if (mask == 0 || (!numeric_addr && NSHIFT(mask) != 0)) {
		*dst = '\0';
		return;
	}
	i = 0;
	for (b = 0; b < 32; b++)
		if (mask & (1 << b)) {
			int bb;

			i = b;
			for (bb = b+1; bb < 32; bb++)
				if (!(mask & (1 << bb))) {
					i = -1;	/* noncontig */
					break;
				}
			break;
		}
	if (i == -1)
		sprintf(dst, "&0x%lx", mask);
	else
		sprintf(dst, "/%d", 32-i);
}

/*
 * Return the name of the network whose address is given.
 */
char *
netname(in_addr_t in, in_addr_t mask)
{
	char *cp = 0;
	static char line[MAXHOSTNAMELEN];
	struct netent *np = 0;
	in_addr_t i;

	/* It is ok to supply host address. */
	in &= mask;

	i = ntohl(in);
	if (!numeric_addr && i) {
		np = getnetbyaddr(i >> NSHIFT(ntohl(mask)), AF_INET);
		if (np != NULL) {
			cp = np->n_name;
			trimdomain(cp, strlen(cp));
		}
	}
	if (cp != NULL) {
		strlcpy(line, cp, sizeof(line));
	} else {
		inet_ntop(AF_INET, &in, line, sizeof(line) - 1);
	}
	domask(line + strlen(line), i, ntohl(mask));
	return (line);
}

#undef NSHIFT

#ifdef INET6
void
in6_fillscopeid(struct sockaddr_in6 *sa6)
{
#if defined(__KAME__)
	/*
	 * XXX: This is a special workaround for KAME kernels.
	 * sin6_scope_id field of SA should be set in the future.
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&sa6->sin6_addr) ||
	    IN6_IS_ADDR_MC_NODELOCAL(&sa6->sin6_addr) ||
	    IN6_IS_ADDR_MC_LINKLOCAL(&sa6->sin6_addr)) {
		if (sa6->sin6_scope_id == 0)
			sa6->sin6_scope_id =
			    ntohs(*(u_int16_t *)&sa6->sin6_addr.s6_addr[2]);
		sa6->sin6_addr.s6_addr[2] = sa6->sin6_addr.s6_addr[3] = 0;
	}
#endif
}

const char *
netname6(struct sockaddr_in6 *sa6, struct in6_addr *mask)
{
	static char line[MAXHOSTNAMELEN];
	u_char *p = (u_char *)mask;
	u_char *lim;
	int masklen, illegal = 0, flag = 0;

	if (mask) {
		for (masklen = 0, lim = p + 16; p < lim; p++) {
			switch (*p) {
			 case 0xff:
				 masklen += 8;
				 break;
			 case 0xfe:
				 masklen += 7;
				 break;
			 case 0xfc:
				 masklen += 6;
				 break;
			 case 0xf8:
				 masklen += 5;
				 break;
			 case 0xf0:
				 masklen += 4;
				 break;
			 case 0xe0:
				 masklen += 3;
				 break;
			 case 0xc0:
				 masklen += 2;
				 break;
			 case 0x80:
				 masklen += 1;
				 break;
			 case 0x00:
				 break;
			 default:
				 illegal ++;
				 break;
			}
		}
		if (illegal)
			xo_error("illegal prefixlen\n");
	}
	else
		masklen = 128;

	if (masklen == 0 && IN6_IS_ADDR_UNSPECIFIED(&sa6->sin6_addr))
		return("default");

	if (numeric_addr)
		flag |= NI_NUMERICHOST;
	getnameinfo((struct sockaddr *)sa6, sa6->sin6_len, line, sizeof(line),
		    NULL, 0, flag);

	if (numeric_addr)
		sprintf(&line[strlen(line)], "/%d", masklen);

	return line;
}

char *
routename6(struct sockaddr_in6 *sa6)
{
	static char line[MAXHOSTNAMELEN];
	int flag = 0;
	/* use local variable for safety */
	struct sockaddr_in6 sa6_local;

	sa6_local.sin6_family = AF_INET6;
	sa6_local.sin6_len = sizeof(sa6_local);
	sa6_local.sin6_addr = sa6->sin6_addr;
	sa6_local.sin6_scope_id = sa6->sin6_scope_id;

	if (numeric_addr)
		flag |= NI_NUMERICHOST;

	getnameinfo((struct sockaddr *)&sa6_local, sa6_local.sin6_len,
		    line, sizeof(line), NULL, 0, flag);

	return line;
}
#endif /*INET6*/

/*
 * Print routing statistics
 */
void
rt_stats(void)
{
	struct rtstat rtstat;
	u_long rtsaddr, rttaddr;
	int rttrash;

	kresolve_list(rl);

	if ((rtsaddr = rl[N_RTSTAT].n_value) == 0) {
		xo_emit("{W:rtstat: symbol not in namelist}\n");
		return;
	}
	if ((rttaddr = rl[N_RTTRASH].n_value) == 0) {
		xo_emit("{W:rttrash: symbol not in namelist}\n");
		return;
	}
	kread(rtsaddr, (char *)&rtstat, sizeof (rtstat));
	kread(rttaddr, (char *)&rttrash, sizeof (rttrash));
	xo_emit("{T:routing}:\n");

#define	p(f, m) if (rtstat.f || sflag <= 1) \
	xo_emit(m, rtstat.f, plural(rtstat.f))

	p(rts_badredirect, "\t{:bad-redirects/%hu} "
	    "{N:/bad routing redirect%s}\n");
	p(rts_dynamic, "\t{:dynamically-created/%hu} "
	    "{N:/dynamically created route%s}\n");
	p(rts_newgateway, "\t{:new-gateways/%hu} "
	    "{N:/new gateway%s due to redirects}\n");
	p(rts_unreach, "\t{:unreachable-destination/%hu} "
	    "{N:/destination%s found unreachable}\n");
	p(rts_wildcard, "\t{:wildcard-uses/%hu} "
	    "{N:/use%s of a wildcard route}\n");
#undef p

	if (rttrash || sflag <= 1)
		xo_emit("\t{:unused-but-not-freed/%u} "
		    "{N:/route%s not in table but not freed}\n",
		    rttrash, plural(rttrash));
}
