/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
#if 0
static char sccsid[] = "From: @(#)route.c	8.6 (Berkeley) 4/28/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netipx/ipx.h>
#include <netatalk/at.h>
#include <netgraph/ng_socket.h>

#ifdef NS
#include <netns/ns.h>
#endif

#include <sys/sysctl.h>

#include <arpa/inet.h>
#include <libutil.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <time.h>
#include "netstat.h"

#define kget(p, d) (kread((u_long)(p), (char *)&(d), sizeof (d)))


/* alignment constraint for routing socket */
#define ROUNDUP(a) \
       ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

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
	{ RTF_CLONING,	'C' },
	{ RTF_XRESOLVE,	'X' },
	{ RTF_LLINFO,	'L' },
	{ RTF_STATIC,	'S' },
	{ RTF_PROTO1,	'1' },
	{ RTF_PROTO2,	'2' },
	{ RTF_WASCLONED,'W' },
	{ RTF_PRCLONING,'c' },
	{ RTF_PROTO3,	'3' },
	{ RTF_BLACKHOLE,'B' },
	{ RTF_BROADCAST,'b' },
	{ 0 }
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
struct	radix_node_head *rt_tables[AF_MAX+1];

int	NewTree = 0;

static struct sockaddr *kgetsa (struct sockaddr *);
static void p_tree (struct radix_node *);
static void p_rtnode (void);
static void ntreestuff (void);
static void np_rtentry (struct rt_msghdr *);
static void p_sockaddr (struct sockaddr *, struct sockaddr *, int, int);
static void p_flags (int, char *);
static void p_rtentry (struct rtentry *);
static u_long forgemask (u_long);
static void domask (char *, u_long, u_long);

/*
 * Print routing tables.
 */
void
routepr(u_long rtree)
{
	struct radix_node_head *rnh, head;
	int i;

	printf("Routing tables\n");

	if (Aflag == 0 && NewTree)
		ntreestuff();
	else {
		if (rtree == 0) {
			printf("rt_tables: symbol not in namelist\n");
			return;
		}

		kget(rtree, rt_tables);
		for (i = 0; i <= AF_MAX; i++) {
			if ((rnh = rt_tables[i]) == 0)
				continue;
			kget(rnh, head);
			if (i == AF_UNSPEC) {
				if (Aflag && af == 0) {
					printf("Netmasks:\n");
					p_tree(head.rnh_treetop);
				}
			} else if (af == AF_UNSPEC || af == i) {
				pr_family(i);
				do_rtent = 1;
				pr_rthdr(i);
				p_tree(head.rnh_treetop);
			}
		}
	}
}

/*
 * Print address family header before a section of the routing table.
 */
void
pr_family(int af)
{
	char *afname;

	switch (af) {
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
#ifdef NS
	case AF_NS:
		afname = "XNS";
		break;
#endif
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
		printf("\nProtocol Family %d:\n", af);
}

/* column widths; each followed by one space */
#ifndef INET6
#define	WID_DST(af) 	18	/* width of destination column */
#define	WID_GW(af)	18	/* width of gateway column */
#define	WID_IF(af)	6	/* width of netif column */
#else
#define	WID_DST(af) \
	((af) == AF_INET6 ? (lflag ? 39 : (nflag ? 33: 18)) : 18)
#define	WID_GW(af) \
	((af) == AF_INET6 ? (lflag ? 31 : (nflag ? 29 : 18)) : 18)
#define	WID_IF(af)	((af) == AF_INET6 ? 8 : 6)
#endif /*INET6*/

/*
 * Print header for routing table columns.
 */
void
pr_rthdr(int af)
{

	if (Aflag)
		printf("%-8.8s ","Address");
	if (af == AF_INET || lflag)
		if (lflag)
			printf("%-*.*s %-*.*s %-6.6s %6.6s %8.8s %6.6s %*.*s %6s\n",
				WID_DST(af), WID_DST(af), "Destination",
				WID_GW(af), WID_GW(af), "Gateway",
				"Flags", "Refs", "Use", "Mtu",
				WID_IF(af), WID_IF(af), "Netif", "Expire");
		else
			printf("%-*.*s %-*.*s %-6.6s %6.6s %8.8s %*.*s %6s\n",
				WID_DST(af), WID_DST(af), "Destination",
				WID_GW(af), WID_GW(af), "Gateway",
				"Flags", "Refs", "Use",
				WID_IF(af), WID_IF(af), "Netif", "Expire");
	else
		printf("%-*.*s %-*.*s %-6.6s  %8.8s %6s\n",
			WID_DST(af), WID_DST(af), "Destination",
			WID_GW(af), WID_GW(af), "Gateway",
			"Flags", "Netif", "Expire");
}

static struct sockaddr *
kgetsa(struct sockaddr *dst)
{

	kget(dst, pt_u.u_sa);
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, (char *)pt_u.u_data, pt_u.u_sa.sa_len);
	return (&pt_u.u_sa);
}

static void
p_tree(struct radix_node *rn)
{

again:
	kget(rn, rnode);
	if (rnode.rn_bit < 0) {
		if (Aflag)
			printf("%-8.8lx ", (u_long)rn);
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag)
				printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		} else if (do_rtent) {
			kget(rn, rtentry);
			p_rtentry(&rtentry);
			if (Aflag)
				p_rtnode();
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
		kget(rm, rmask);
		sprintf(nbuf, " %d refs, ", rmask.rm_refs);
		printf(" mk = %8.8lx {(%d),%s",
			(u_long)rm, -1 - rmask.rm_bit, rmask.rm_refs ? nbuf : " ");
		if (rmask.rm_flags & RNF_NORMAL) {
			struct radix_node rnode_aux;
			printf(" <normal>, ");
			kget(rmask.rm_leaf, rnode_aux);
			p_sockaddr(kgetsa((struct sockaddr *)rnode_aux.rn_mask),
				    NULL, 0, -1);
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
	register struct rt_msghdr *rtm;

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
		err(2, "malloc(%lu)", (unsigned long)needed);
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
	register struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
#ifdef notdef
	static int masks_done, banner_printed;
#endif
	static int old_af;
	int af = 0, interesting = RTF_UP | RTF_GATEWAY | RTF_HOST;

#ifdef notdef
	/* for the moment, netmasks are skipped over */
	if (!banner_printed) {
		printf("Netmasks:\n");
		banner_printed = 1;
	}
	if (masks_done == 0) {
		if (rtm->rtm_addrs != RTA_DST ) {
			masks_done = 1;
			af = sa->sa_family;
		}
	} else
#endif
		af = sa->sa_family;
	if (af != old_af) {
		pr_family(af);
		old_af = af;
	}
	if (rtm->rtm_addrs == RTA_DST)
		p_sockaddr(sa, NULL, 0, 36);
	else {
		p_sockaddr(sa, NULL, rtm->rtm_flags, 16);
		sa = (struct sockaddr *)(ROUNDUP(sa->sa_len) + (char *)sa);
		p_sockaddr(sa, NULL, 0, 18);
	}
	p_flags(rtm->rtm_flags & interesting, "%-6.6s ");
	putchar('\n');
}

static void
p_sockaddr(struct sockaddr *sa, struct sockaddr *mask, int flags, int width)
{
	char workbuf[128], *cplim;
	register char *cp = workbuf;

	switch(sa->sa_family) {
	case AF_INET:
	    {
		register struct sockaddr_in *sin = (struct sockaddr_in *)sa;

		if ((sin->sin_addr.s_addr == INADDR_ANY) &&
			mask &&
			ntohl(((struct sockaddr_in *)mask)->sin_addr.s_addr)
				==0L)
				cp = "default" ;
		else if (flags & RTF_HOST)
			cp = routename(sin->sin_addr.s_addr);
		else if (mask)
			cp = netname(sin->sin_addr.s_addr,
				     ntohl(((struct sockaddr_in *)mask)
					   ->sin_addr.s_addr));
		else
			cp = netname(sin->sin_addr.s_addr, 0L);
		break;
	    }

#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
		struct in6_addr *in6 = &sa6->sin6_addr;

		/*
		 * XXX: This is a special workaround for KAME kernels.
		 * sin6_scope_id field of SA should be set in the future.
		 */
		if (IN6_IS_ADDR_LINKLOCAL(in6) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6)) {
		    /* XXX: override is ok? */
		    sa6->sin6_scope_id = (u_int32_t)ntohs(*(u_short *)&in6->s6_addr[2]);
		    *(u_short *)&in6->s6_addr[2] = 0;
		}

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
		printf("%s", ((struct sockaddr_ng *)sa)->sg_data);
		break;
	    }
#ifdef NS
	case AF_NS:
		cp = ns_print(sa);
		break;
#endif

	case AF_LINK:
	    {
		register struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

		if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 &&
		    sdl->sdl_slen == 0)
			(void) sprintf(workbuf, "link#%d", sdl->sdl_index);
		else
			switch (sdl->sdl_type) {

			case IFT_ETHER:
			    {
				register int i;
				register u_char *lla = (u_char *)sdl->sdl_data +
				    sdl->sdl_nlen;

				cplim = "";
				for (i = 0; i < sdl->sdl_alen; i++, lla++) {
					cp += sprintf(cp, "%s%x", cplim, *lla);
					cplim = ":";
				}
				cp = workbuf;
				break;
			    }

			default:
				cp = link_ntoa(sdl);
				break;
			}
		break;
	    }

	default:
	    {
		register u_char *s = (u_char *)sa->sa_data, *slim;

		slim =  sa->sa_len + (u_char *) sa;
		cplim = cp + sizeof(workbuf) - 6;
		cp += sprintf(cp, "(%d)", sa->sa_family);
		while (s < slim && cp < cplim) {
			cp += sprintf(cp, " %02x", *s++);
			if (s < slim)
			    cp += sprintf(cp, "%02x", *s++);
		}
		cp = workbuf;
	    }
	}
	if (width < 0 )
		printf("%s ", cp);
	else {
		if (nflag)
			printf("%-*s ", width, cp);
		else
			printf("%-*.*s ", width, width, cp);
	}
}

static void
p_flags(int f, char *format)
{
	char name[33], *flags;
	register struct bits *p = bits;

	for (flags = name; p->b_mask; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	printf(format, name);
}

static void
p_rtentry(struct rtentry *rt)
{
	static struct ifnet ifnet, *lastif;
	struct rtentry parent;
	static char name[16];
	static char prettyname[9];
	struct sockaddr *sa;
	sa_u addr, mask;

	/*
	 * Don't print protocol-cloned routes unless -a.
	 */
	if (rt->rt_flags & RTF_WASCLONED && !aflag) {
		kget(rt->rt_parent, parent);
		if (parent.rt_flags & RTF_PRCLONING)
			return;
	}

	bzero(&addr, sizeof(addr));
	if ((sa = kgetsa(rt_key(rt))))
		bcopy(sa, &addr, sa->sa_len);
	bzero(&mask, sizeof(mask));
	if (rt_mask(rt) && (sa = kgetsa(rt_mask(rt))))
		bcopy(sa, &mask, sa->sa_len);
	p_sockaddr(&addr.u_sa, &mask.u_sa, rt->rt_flags,
	    WID_DST(addr.u_sa.sa_family));
	p_sockaddr(kgetsa(rt->rt_gateway), NULL, RTF_HOST,
	    WID_GW(addr.u_sa.sa_family));
	p_flags(rt->rt_flags, "%-6.6s ");
	if (addr.u_sa.sa_family == AF_INET || lflag) {
		printf("%6ld %8ld ", rt->rt_refcnt, rt->rt_use);
		if (lflag) {
			if (rt->rt_rmx.rmx_mtu != 0)
				printf("%6lu ", rt->rt_rmx.rmx_mtu);
			else
				printf("%6s ", "");
		}
	}
	if (rt->rt_ifp) {
		if (rt->rt_ifp != lastif) {
			kget(rt->rt_ifp, ifnet);
			kread((u_long)ifnet.if_name, name, 16);
			lastif = rt->rt_ifp;
			snprintf(prettyname, sizeof prettyname,
				 "%s%d", name, ifnet.if_unit);
		}
		printf("%*.*s", WID_IF(addr.u_sa.sa_family),
		    WID_IF(addr.u_sa.sa_family), prettyname);
		if (rt->rt_rmx.rmx_expire) {
			time_t expire_time;

			if ((expire_time =
			    rt->rt_rmx.rmx_expire - time((time_t *)0)) > 0)
				printf(" %6d", (int)expire_time);
		}
		if (rt->rt_nodes[0].rn_dupedkey)
			printf(" =>");
	}
	putchar('\n');
}

char *
routename(u_long in)
{
	register char *cp;
	static char line[MAXHOSTNAMELEN];
	struct hostent *hp;

	cp = 0;
	if (!nflag) {
		hp = gethostbyaddr((char *)&in, sizeof (struct in_addr),
			AF_INET);
		if (hp) {
			cp = hp->h_name;
			trimdomain(cp, strlen(cp));
		}
	}
	if (cp) {
		strncpy(line, cp, sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
	} else {
#define C(x)	((x) & 0xff)
		in = ntohl(in);
		sprintf(line, "%lu.%lu.%lu.%lu",
		    C(in >> 24), C(in >> 16), C(in >> 8), C(in));
	}
	return (line);
}

static u_long
forgemask(u_long a)
{
	u_long m;

	if (IN_CLASSA(a))
		m = IN_CLASSA_NET;
	else if (IN_CLASSB(a))
		m = IN_CLASSB_NET;
	else
		m = IN_CLASSC_NET;
	return (m);
}

static void
domask(char *dst, u_long addr, u_long mask)
{
	register int b, i;

	if (!mask || (forgemask(addr) == mask)) {
		*dst = '\0';
		return;
	}
	i = 0;
	for (b = 0; b < 32; b++)
		if (mask & (1 << b)) {
			register int bb;

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
netname(u_long in, u_long mask)
{
	char *cp = 0;
	static char line[MAXHOSTNAMELEN];
	struct netent *np = 0;
	u_long net, omask, dmask;
	register u_long i;

	i = ntohl(in);
	dmask = forgemask(i);
	omask = mask;
	if (!nflag && i) {
		net = i & dmask;
		if (!(np = getnetbyaddr(i, AF_INET)) && net != i)
			np = getnetbyaddr(net, AF_INET);
		if (np) {
			cp = np->n_name;
			trimdomain(cp, strlen(cp));
		}
	}
	if (cp)
		strncpy(line, cp, sizeof(line) - 1);
	else {
		switch (dmask) {
		case IN_CLASSA_NET:
			if ((i & IN_CLASSA_HOST) == 0) {
				sprintf(line, "%lu", C(i >> 24));
				break;
			}
			/* FALLTHROUGH */
		case IN_CLASSB_NET:
			if ((i & IN_CLASSB_HOST) == 0) {
				sprintf(line, "%lu.%lu",
					C(i >> 24), C(i >> 16));
				break;
			}
			/* FALLTHROUGH */
		case IN_CLASSC_NET:
			if ((i & IN_CLASSC_HOST) == 0) {
				sprintf(line, "%lu.%lu.%lu",
					C(i >> 24), C(i >> 16), C(i >> 8));
				break;
			}
			/* FALLTHROUGH */
		default:
			sprintf(line, "%lu.%lu.%lu.%lu",
				C(i >> 24), C(i >> 16), C(i >> 8), C(i));
			break;
		}
	}
	domask(line+strlen(line), i, omask);
	return (line);
}

#ifdef INET6
char *
netname6(struct sockaddr_in6 *sa6, struct in6_addr *mask)
{
	static char line[MAXHOSTNAMELEN];
	u_char *p = (u_char *)mask;
	u_char *lim;
	int masklen, illegal = 0, flag = NI_WITHSCOPEID;

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

	if (nflag)
		flag |= NI_NUMERICHOST;
	getnameinfo((struct sockaddr *)sa6, sa6->sin6_len, line, sizeof(line),
		    NULL, 0, flag);

	if (nflag)
		sprintf(&line[strlen(line)], "/%d", masklen);

	return line;
}

char *
routename6(struct sockaddr_in6 *sa6)
{
	static char line[MAXHOSTNAMELEN];
	int flag = NI_WITHSCOPEID;
	/* use local variable for safety */
	struct sockaddr_in6 sa6_local = {AF_INET6, sizeof(sa6_local),};

	sa6_local.sin6_addr = sa6->sin6_addr;
	sa6_local.sin6_scope_id = sa6->sin6_scope_id;

	if (nflag)
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

	p(rts_badredirect, "\t%u bad routing redirect%s\n");
	p(rts_dynamic, "\t%u dynamically created route%s\n");
	p(rts_newgateway, "\t%u new gateway%s due to redirects\n");
	p(rts_unreach, "\t%u destination%s found unreachable\n");
	p(rts_wildcard, "\t%u use%s of a wildcard route\n");
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
	char *net = "", *host = "";
	register char *p;
	register u_char *q;
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
	register struct sockaddr_ipx *sipx = (struct sockaddr_ipx *)sa;
	struct sockaddr_ipx work;
	static union ipx_net ipx_zeronet;
	char *p;
	struct ipx_addr in;

	work = *sipx;
	in = work.sipx_addr;

	work.sipx_addr.x_port = 0;
	work.sipx_addr.x_net = ipx_zeronet;
	p = ipx_print((struct sockaddr *)&work);
	if (strncmp("*.", p, 2) == 0) p += 2;

	return(p);
}

#ifdef NS
short ns_nullh[] = {0,0,0};
short ns_bh[] = {-1,-1,-1};

char *
ns_print(struct sockaddr *sa)
{
	register struct sockaddr_ns *sns = (struct sockaddr_ns*)sa;
	struct ns_addr work;
	union { union ns_net net_e; u_long long_e; } net;
	u_short port;
	static char mybuf[50], cport[10], chost[25];
	char *host = "";
	register char *p; register u_char *q;

	work = sns->sns_addr;
	port = ntohs(work.x_port);
	work.x_port = 0;
	net.net_e  = work.x_net;
	if (ns_nullhost(work) && net.long_e == 0) {
		if (port ) {
			sprintf(mybuf, "*.%xH", port);
			upHex(mybuf);
		} else
			sprintf(mybuf, "*.*");
		return (mybuf);
	}

	if (bcmp(ns_bh, work.x_host.c_host, 6) == 0) {
		host = "any";
	} else if (bcmp(ns_nullh, work.x_host.c_host, 6) == 0) {
		host = "*";
	} else {
		q = work.x_host.c_host;
		sprintf(chost, "%02x%02x%02x%02x%02x%02xH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			continue;
		host = p;
	}
	if (port)
		sprintf(cport, ".%xH", htons(port));
	else
		*cport = 0;

	sprintf(mybuf,"%xH.%s%s", ntohl(net.long_e), host, cport);
	upHex(mybuf);
	return(mybuf);
}

char *
ns_phost(struct sockaddr *sa)
{
	register struct sockaddr_ns *sns = (struct sockaddr_ns *)sa;
	struct sockaddr_ns work;
	static union ns_net ns_zeronet;
	char *p;

	work = *sns;
	work.sns_addr.x_port = 0;
	work.sns_addr.x_net = ns_zeronet;

	p = ns_print((struct sockaddr *)&work);
	if (strncmp("0H.", p, 3) == 0)
		p += 3;
	return(p);
}
#endif

void
upHex(char *p0)
{
	register char *p = p0;

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
