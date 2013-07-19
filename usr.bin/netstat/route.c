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
#include <net/route.h>

#include <netinet/in.h>
#include <netipx/ipx.h>
#include <netatalk/at.h>
#include <netgraph/ng_socket.h>

#include <sys/sysctl.h>

#include <arpa/inet.h>
#include <libutil.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <err.h>
#include "netstat.h"

#define	kget(p, d) (kread((u_long)(p), (char *)&(d), sizeof (d)))

/*
 * Definitions for showing gateway flags.
 */
struct bits {
	u_long	b_mask;
	char	b_val;
} bits[] = {
	{ RTF_UP,	'U' },
	{ RTF_GATEWAY,	'G' },
	{ RTF_HOST,	'H' },
	{ RTF_REJECT,	'R' },
	{ RTF_DYNAMIC,	'D' },
	{ RTF_MODIFIED,	'M' },
	{ RTF_DONE,	'd' }, /* Completed -- for routing messages only */
	{ RTF_XRESOLVE,	'X' },
	{ RTF_STATIC,	'S' },
	{ RTF_PROTO1,	'1' },
	{ RTF_PROTO2,	'2' },
	{ RTF_PRCLONING,'c' },
	{ RTF_PROTO3,	'3' },
	{ RTF_BLACKHOLE,'B' },
	{ RTF_BROADCAST,'b' },
#ifdef RTF_LLINFO
	{ RTF_LLINFO,	'L' },
#endif
#ifdef RTF_WASCLONED
	{ RTF_WASCLONED,'W' },
#endif
#ifdef RTF_CLONING
	{ RTF_CLONING,	'C' },
#endif
	{ 0 , 0 }
};

typedef union {
	long	dummy;		/* Helps align structure. */
	struct	sockaddr u_sa;
	u_short	u_data[128];
} sa_u;

static sa_u pt_u;

int	do_rtent = 0;
struct	rtentry rtentry;
struct	radix_node rnode;
struct	radix_mask rmask;
struct	radix_node_head **rt_tables;

int	NewTree = 0;

struct	timespec uptime;

static struct sockaddr *kgetsa(struct sockaddr *);
static void size_cols(int ef, struct radix_node *rn);
static void size_cols_tree(struct radix_node *rn);
static void size_cols_rtentry(struct rtentry *rt);
static void p_tree(struct radix_node *);
static void p_rtnode(void);
static void ntreestuff(void);
static void np_rtentry(struct rt_msghdr *);
static void p_sockaddr(struct sockaddr *, struct sockaddr *, int, int);
static const char *fmt_sockaddr(struct sockaddr *sa, struct sockaddr *mask,
    int flags);
static void p_flags(int, const char *);
static const char *fmt_flags(int f);
static void p_rtentry(struct rtentry *);
static void domask(char *, in_addr_t, u_long);

/*
 * Print routing tables.
 */
void
routepr(u_long rtree, int fibnum)
{
	struct radix_node_head **rnhp, *rnh, head;
	size_t intsize;
	int fam, numfibs;

	intsize = sizeof(int);
	if (fibnum == -1 &&
	    sysctlbyname("net.my_fibnum", &fibnum, &intsize, NULL, 0) == -1)
		fibnum = 0;
	if (sysctlbyname("net.fibs", &numfibs, &intsize, NULL, 0) == -1)
		numfibs = 1;
	if (fibnum < 0 || fibnum > numfibs - 1)
		errx(EX_USAGE, "%d: invalid fib", fibnum);
	rt_tables = calloc(numfibs * (AF_MAX+1),
	    sizeof(struct radix_node_head *));
	if (rt_tables == NULL)
		err(EX_OSERR, "memory allocation failed");
	/*
	 * Since kernel & userland use different timebase
	 * (time_uptime vs time_second) and we are reading kernel memory
	 * directly we should do rt_rmx.rmx_expire --> expire_time conversion.
	 */
	if (clock_gettime(CLOCK_UPTIME, &uptime) < 0)
		err(EX_OSERR, "clock_gettime() failed");

	printf("Routing tables");
	if (fibnum)
		printf(" (fib: %d)", fibnum);
	printf("\n");

	if (Aflag == 0 && NewTree)
		ntreestuff();
	else {
		if (rtree == 0) {
			printf("rt_tables: symbol not in namelist\n");
			return;
		}

		if (kread((u_long)(rtree), (char *)(rt_tables), (numfibs *
		    (AF_MAX+1) * sizeof(struct radix_node_head *))) != 0)
			return;
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
			rnhp += tmpfib * (AF_MAX+1) + fam;
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
					printf("Netmasks:\n");
					p_tree(head.rnh_treetop);
				}
			} else if (af == AF_UNSPEC || af == fam) {
				size_cols(fam, head.rnh_treetop);
				pr_family(fam);
				do_rtent = 1;
				pr_rthdr(fam);
				p_tree(head.rnh_treetop);
			}
		}
	}
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
	case AF_IPX:
		afname = "IPX";
		break;
	case AF_ISO:
		afname = "ISO";
		break;
	case AF_APPLETALK:
		afname = "AppleTalk";
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
		printf("\n%s:\n", afname);
	else
		printf("\nProtocol Family %d:\n", af1);
}

/* column widths; each followed by one space */
#ifndef INET6
#define	WID_DST_DEFAULT(af) 	18	/* width of destination column */
#define	WID_GW_DEFAULT(af)	18	/* width of gateway column */
#define	WID_IF_DEFAULT(af)	(Wflag ? 8 : 6)	/* width of netif column */
#else
#define	WID_DST_DEFAULT(af) \
	((af) == AF_INET6 ? (numeric_addr ? 33: 18) : 18)
#define	WID_GW_DEFAULT(af) \
	((af) == AF_INET6 ? (numeric_addr ? 29 : 18) : 18)
#define	WID_IF_DEFAULT(af)	((af) == AF_INET6 ? 8 : (Wflag ? 8 : 6))
#endif /*INET6*/

static int wid_dst;
static int wid_gw;
static int wid_flags;
static int wid_refs;
static int wid_use;
static int wid_mtu;
static int wid_if;
static int wid_expire;

static void
size_cols(int ef __unused, struct radix_node *rn)
{
	wid_dst = WID_DST_DEFAULT(ef);
	wid_gw = WID_GW_DEFAULT(ef);
	wid_flags = 6;
	wid_refs = 6;
	wid_use = 8;
	wid_mtu = 6;
	wid_if = WID_IF_DEFAULT(ef);
	wid_expire = 6;

	if (Wflag)
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

	if (addr.u_sa.sa_family == AF_INET || Wflag) {
		len = snprintf(buffer, sizeof(buffer), "%d", rt->rt_refcnt);
		wid_refs = MAX(len, wid_refs);
		len = snprintf(buffer, sizeof(buffer), "%lu", rt->rt_use);
		wid_use = MAX(len, wid_use);
		if (Wflag && rt->rt_rmx.rmx_mtu != 0) {
			len = snprintf(buffer, sizeof(buffer),
				       "%lu", rt->rt_rmx.rmx_mtu);
			wid_mtu = MAX(len, wid_mtu);
		}
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
		if (rt->rt_rmx.rmx_expire) {
			time_t expire_time;

			if ((expire_time =
			    rt->rt_rmx.rmx_expire - uptime.tv_sec) > 0) {
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
		printf("%-8.8s ","Address");
	if (af1 == AF_INET || Wflag) {
		if (Wflag) {
			printf("%-*.*s %-*.*s %-*.*s %*.*s %*.*s %*.*s %*.*s %*s\n",
				wid_dst,	wid_dst,	"Destination",
				wid_gw,		wid_gw,		"Gateway",
				wid_flags,	wid_flags,	"Flags",
				wid_refs,	wid_refs,	"Refs",
				wid_use,	wid_use,	"Use",
				wid_mtu,	wid_mtu,	"Mtu",
				wid_if,		wid_if,		"Netif",
				wid_expire,			"Expire");
		} else {
			printf("%-*.*s %-*.*s %-*.*s %*.*s %*.*s %*.*s %*s\n",
				wid_dst,	wid_dst,	"Destination",
				wid_gw,		wid_gw,		"Gateway",
				wid_flags,	wid_flags,	"Flags",
				wid_refs,	wid_refs,	"Refs",
				wid_use,	wid_use,	"Use",
				wid_if,		wid_if,		"Netif",
				wid_expire,			"Expire");
		}
	} else {
		printf("%-*.*s %-*.*s %-*.*s  %*.*s %*s\n",
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

static void
p_tree(struct radix_node *rn)
{

again:
	if (kget(rn, rnode) != 0)
		return;
	if (!(rnode.rn_flags & RNF_ACTIVE))
		return;
	if (rnode.rn_bit < 0) {
		if (Aflag)
			printf("%-8.8lx ", (u_long)rn);
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag)
				printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		} else if (do_rtent) {
			if (kget(rn, rtentry) == 0) {
				p_rtentry(&rtentry);
				if (Aflag)
					p_rtnode();
			}
		} else {
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_key),
				   NULL, 0, 44);
			putchar('\n');
		}
		if ((rn = rnode.rn_dupedkey))
			goto again;
	} else {
		if (Aflag && do_rtent) {
			printf("%-8.8lx ", (u_long)rn);
			p_rtnode();
		}
		rn = rnode.rn_right;
		p_tree(rnode.rn_left);
		p_tree(rn);
	}
}

char	nbuf[20];

static void
p_rtnode(void)
{
	struct radix_mask *rm = rnode.rn_mklist;

	if (rnode.rn_bit < 0) {
		if (rnode.rn_mask) {
			printf("\t  mask ");
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_mask),
				   NULL, 0, -1);
		} else if (rm == 0)
			return;
	} else {
		sprintf(nbuf, "(%d)", rnode.rn_bit);
		printf("%6.6s %8.8lx : %8.8lx", nbuf, (u_long)rnode.rn_left, (u_long)rnode.rn_right);
	}
	while (rm) {
		if (kget(rm, rmask) != 0)
			break;
		sprintf(nbuf, " %d refs, ", rmask.rm_refs);
		printf(" mk = %8.8lx {(%d),%s",
			(u_long)rm, -1 - rmask.rm_bit, rmask.rm_refs ? nbuf : " ");
		if (rmask.rm_flags & RNF_NORMAL) {
			struct radix_node rnode_aux;
			printf(" <normal>, ");
			if (kget(rmask.rm_leaf, rnode_aux) == 0)
				p_sockaddr(kgetsa((struct sockaddr *)rnode_aux.rn_mask),
				    NULL, 0, -1);
			else
				p_sockaddr(NULL, NULL, 0, -1);
		} else
		    p_sockaddr(kgetsa((struct sockaddr *)rmask.rm_mask),
				NULL, 0, -1);
		putchar('}');
		if ((rm = rmask.rm_mklist))
			printf(" ->");
	}
	putchar('\n');
}

static void
ntreestuff(void)
{
	size_t needed;
	int mib[6];
	char *buf, *next, *lim;
	struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		err(1, "sysctl: net.route.0.0.dump estimate");
	}

	if ((buf = malloc(needed)) == 0) {
		errx(2, "malloc(%lu)", (unsigned long)needed);
	}
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		err(1, "sysctl: net.route.0.0.dump");
	}
	lim  = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		np_rtentry(rtm);
	}
}

static void
np_rtentry(struct rt_msghdr *rtm)
{
	struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
#ifdef notdef
	static int masks_done, banner_printed;
#endif
	static int old_af;
	int af1 = 0, interesting = RTF_UP | RTF_GATEWAY | RTF_HOST;

#ifdef notdef
	/* for the moment, netmasks are skipped over */
	if (!banner_printed) {
		printf("Netmasks:\n");
		banner_printed = 1;
	}
	if (masks_done == 0) {
		if (rtm->rtm_addrs != RTA_DST ) {
			masks_done = 1;
			af1 = sa->sa_family;
		}
	} else
#endif
		af1 = sa->sa_family;
	if (af1 != old_af) {
		pr_family(af1);
		old_af = af1;
	}
	if (rtm->rtm_addrs == RTA_DST)
		p_sockaddr(sa, NULL, 0, 36);
	else {
		p_sockaddr(sa, NULL, rtm->rtm_flags, 16);
		sa = (struct sockaddr *)(SA_SIZE(sa) + (char *)sa);
		p_sockaddr(sa, NULL, 0, 18);
	}
	p_flags(rtm->rtm_flags & interesting, "%-6.6s ");
	putchar('\n');
}

static void
p_sockaddr(struct sockaddr *sa, struct sockaddr *mask, int flags, int width)
{
	const char *cp;

	cp = fmt_sockaddr(sa, mask, flags);

	if (width < 0 )
		printf("%s ", cp);
	else {
		if (numeric_addr)
			printf("%-*s ", width, cp);
		else
			printf("%-*.*s ", width, width, cp);
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
				     ntohl(((struct sockaddr_in *)mask)
					   ->sin_addr.s_addr));
		else
			cp = netname(sockin->sin_addr.s_addr, 0L);
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

	case AF_IPX:
	    {
		struct ipx_addr work = ((struct sockaddr_ipx *)sa)->sipx_addr;
		if (ipx_nullnet(satoipx_addr(work)))
			cp = "default";
		else
			cp = ipx_print(sa);
		break;
	    }
	case AF_APPLETALK:
	    {
		if (!(flags & RTF_HOST) && mask)
			cp = atalk_print2(sa,mask,9);
		else
			cp = atalk_print(sa,11);
		break;
	    }
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
	printf(format, fmt_flags(f));
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
p_rtentry(struct rtentry *rt)
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
	p_sockaddr(&addr.u_sa, &mask.u_sa, rt->rt_flags, wid_dst);
	p_sockaddr(kgetsa(rt->rt_gateway), NULL, RTF_HOST, wid_gw);
	snprintf(buffer, sizeof(buffer), "%%-%d.%ds ", wid_flags, wid_flags);
	p_flags(rt->rt_flags, buffer);
	if (addr.u_sa.sa_family == AF_INET || Wflag) {
		printf("%*d %*lu ", wid_refs, rt->rt_refcnt,
				     wid_use, rt->rt_use);
		if (Wflag) {
			if (rt->rt_rmx.rmx_mtu != 0)
				printf("%*lu ", wid_mtu, rt->rt_rmx.rmx_mtu);
			else
				printf("%*s ", wid_mtu, "");
		}
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
		printf("%*.*s", wid_if, wid_if, prettyname);
		if (rt->rt_rmx.rmx_expire) {
			time_t expire_time;

			if ((expire_time =
			    rt->rt_rmx.rmx_expire - uptime.tv_sec) > 0)
				printf(" %*d", wid_expire, (int)expire_time);
		}
		if (rt->rt_nodes[0].rn_dupedkey)
			printf(" =>");
	}
	putchar('\n');
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
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(in_addr_t in, u_long mask)
{
	char *cp = 0;
	static char line[MAXHOSTNAMELEN];
	struct netent *np = 0;
	in_addr_t i;

	i = ntohl(in);
	if (!numeric_addr && i) {
		np = getnetbyaddr(i >> NSHIFT(mask), AF_INET);
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
	domask(line + strlen(line), i, mask);
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
		/* XXX: override is ok? */
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
			fprintf(stderr, "illegal prefixlen\n");
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
rt_stats(u_long rtsaddr, u_long rttaddr)
{
	struct rtstat rtstat;
	int rttrash;

	if (rtsaddr == 0) {
		printf("rtstat: symbol not in namelist\n");
		return;
	}
	if (rttaddr == 0) {
		printf("rttrash: symbol not in namelist\n");
		return;
	}
	kread(rtsaddr, (char *)&rtstat, sizeof (rtstat));
	kread(rttaddr, (char *)&rttrash, sizeof (rttrash));
	printf("routing:\n");

#define	p(f, m) if (rtstat.f || sflag <= 1) \
	printf(m, rtstat.f, plural(rtstat.f))

	p(rts_badredirect, "\t%hu bad routing redirect%s\n");
	p(rts_dynamic, "\t%hu dynamically created route%s\n");
	p(rts_newgateway, "\t%hu new gateway%s due to redirects\n");
	p(rts_unreach, "\t%hu destination%s found unreachable\n");
	p(rts_wildcard, "\t%hu use%s of a wildcard route\n");
#undef p

	if (rttrash || sflag <= 1)
		printf("\t%u route%s not in table but not freed\n",
		    rttrash, plural(rttrash));
}

char *
ipx_print(struct sockaddr *sa)
{
	u_short port;
	struct servent *sp = 0;
	const char *net = "", *host = "";
	char *p;
	u_char *q;
	struct ipx_addr work = ((struct sockaddr_ipx *)sa)->sipx_addr;
	static char mybuf[50];
	char cport[10], chost[15], cnet[15];

	port = ntohs(work.x_port);

	if (ipx_nullnet(work) && ipx_nullhost(work)) {

		if (port) {
			if (sp)
				sprintf(mybuf, "*.%s", sp->s_name);
			else
				sprintf(mybuf, "*.%x", port);
		} else
			sprintf(mybuf, "*.*");

		return (mybuf);
	}

	if (ipx_wildnet(work))
		net = "any";
	else if (ipx_nullnet(work))
		net = "*";
	else {
		q = work.x_net.c_net;
		sprintf(cnet, "%02x%02x%02x%02x",
			q[0], q[1], q[2], q[3]);
		for (p = cnet; *p == '0' && p < cnet + 8; p++)
			continue;
		net = p;
	}

	if (ipx_wildhost(work))
		host = "any";
	else if (ipx_nullhost(work))
		host = "*";
	else {
		q = work.x_host.c_host;
		sprintf(chost, "%02x%02x%02x%02x%02x%02x",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			continue;
		host = p;
	}

	if (port) {
		if (strcmp(host, "*") == 0)
			host = "";
		if (sp)
			snprintf(cport, sizeof(cport),
				"%s%s", *host ? "." : "", sp->s_name);
		else
			snprintf(cport, sizeof(cport),
				"%s%x", *host ? "." : "", port);
	} else
		*cport = 0;

	snprintf(mybuf, sizeof(mybuf), "%s.%s%s", net, host, cport);
	return(mybuf);
}

char *
ipx_phost(struct sockaddr *sa)
{
	struct sockaddr_ipx *sipx = (struct sockaddr_ipx *)sa;
	struct sockaddr_ipx work;
	static union ipx_net ipx_zeronet;
	char *p;

	work = *sipx;

	work.sipx_addr.x_port = 0;
	work.sipx_addr.x_net = ipx_zeronet;
	p = ipx_print((struct sockaddr *)&work);
	if (strncmp("*.", p, 2) == 0) p += 2;

	return(p);
}

void
upHex(char *p0)
{
	char *p = p0;

	for (; *p; p++)
		switch (*p) {

		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			*p += ('A' - 'a');
			break;
		}
}
