/*
 * Copyright (c) 1983, 1988 Regents of the University of California.
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
static char sccsid[] = "@(#)route.c	5.20 (Berkeley) 11/29/90";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/if.h>
#define  KERNEL
#include <net/route.h>
#undef KERNEL
#include <netinet/in.h>

#ifdef NS
#include <netns/ns.h>
#endif

#include <netdb.h>
#include <sys/kinfo.h>

#include <stdio.h>
#include <string.h>

extern	int nflag, aflag, Aflag, af;
int do_rtent;
extern	char *routename(), *netname(), *plural();
#ifdef NS
extern	char *ns_print();
#endif
extern	char *malloc();
#define kget(p, d) \
	(kvm_read((off_t)(p), (char *)&(d), sizeof (d)))

/*
 * Definitions for showing gateway flags.
 */
struct bits {
	short	b_mask;
	char	b_val;
} bits[] = {
	{ RTF_UP,	'U' },
	{ RTF_GATEWAY,	'G' },
	{ RTF_HOST,	'H' },
	{ RTF_DYNAMIC,	'D' },
	{ RTF_MODIFIED,	'M' },
	{ RTF_CLONING,	'C' },
	{ RTF_XRESOLVE,	'X' },
	{ RTF_LLINFO,	'L' },
	{ RTF_REJECT,	'R' },
	{ 0 }
};

/*
 * Print address family.
 */
void
p_proto(proto)
	int	proto;
{
	switch (proto)
	{
	case AF_INET:
		printf("inet");
		break;
	default:
		printf("%d", proto);
		break;
	}
}


/*
 * Print routing tables.
 */
routepr(hostaddr, netaddr, hashsizeaddr, treeaddr)
	off_t hostaddr, netaddr, hashsizeaddr, treeaddr;
{
	struct mbuf mb;
	register struct ortentry *rt;
	register struct mbuf *m;
	char name[16], *flags;
	struct mbuf **routehash;
	int hashsize;
	int i, doinghost = 1;

	printf("Routing tables\n");
	if (Aflag)
		printf("%-8.8s ","Address");
	printf("%-16.16s %-18.18s %-6.6s  %6.6s%8.8s  %s\n",
		"Destination", "Gateway",
		"Flags", "Refs", "Use", "Interface");
	if (treeaddr)
		return treestuff(treeaddr);
	if (hostaddr == 0) {
		printf("rthost: symbol not in namelist\n");
		return;
	}
	if (netaddr == 0) {
		printf("rtnet: symbol not in namelist\n");
		return;
	}
	if (hashsizeaddr == 0) {
		printf("rthashsize: symbol not in namelist\n");
		return;
	}
	kget(hashsizeaddr, hashsize);
	routehash = (struct mbuf **)malloc( hashsize*sizeof (struct mbuf *) );
	kvm_read(hostaddr, (char *)routehash, hashsize*sizeof (struct mbuf *));
again:
	for (i = 0; i < hashsize; i++) {
		if (routehash[i] == 0)
			continue;
		m = routehash[i];
		while (m) {
			kget(m, mb);
			if (Aflag)
				printf("%8.8x ", m);
			p_ortentry((struct ortentry *)(mb.m_dat));
			m = mb.m_next;
		}
	}
	if (doinghost) {
		kvm_read(netaddr, (char *)routehash,
			hashsize*sizeof (struct mbuf *));
		doinghost = 0;
		goto again;
	}
	free((char *)routehash);
	return;
}

static union {
	struct	sockaddr u_sa;
	u_short	u_data[128];
} pt_u;
int do_rtent = 0;
struct rtentry rtentry;
struct radix_node rnode;
struct radix_mask rmask;

int NewTree = 0;
treestuff(rtree)
off_t rtree;
{
	struct radix_node_head *rnh, head;

	if (Aflag == 0 && NewTree)
		return(ntreestuff());
	for (kget(rtree, rnh); rnh; rnh = head.rnh_next) {
		kget(rnh, head);
		if (head.rnh_af == 0) {
			if (Aflag || af == AF_UNSPEC) { 
				printf("Netmasks:\n");
				p_tree(head.rnh_treetop);
			}
		} else if (af == AF_UNSPEC || af == head.rnh_af) {
			printf("\nRoute Tree for Protocol Family ");
			p_proto(head.rnh_af);
			printf(":\n");
			do_rtent = 1;
			p_tree(head.rnh_treetop);
		}
	}
}

struct sockaddr *
kgetsa(dst)
register struct sockaddr *dst;
{
	kget(dst, pt_u.u_sa);
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa)) {
		kvm_read((off_t)dst, pt_u.u_data, pt_u.u_sa.sa_len);
	}
	return (&pt_u.u_sa);
}

p_tree(rn)
struct radix_node *rn;
{

again:
	kget(rn, rnode);
	if (rnode.rn_b < 0) {
		if (Aflag)
			printf("%-8.8x ", rn);
		if (rnode.rn_flags & RNF_ROOT)
			printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		else if (do_rtent) {
			kget(rn, rtentry);
			p_rtentry(&rtentry);
			if (Aflag)
				p_rtnode();
		} else {
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_key),
				    0, 44);
			putchar('\n');
		}
		if (rn = rnode.rn_dupedkey)
			goto again;
	} else {
		if (Aflag && do_rtent) {
			printf("%-8.8x ", rn);
			p_rtnode();
		}
		rn = rnode.rn_r;
		p_tree(rnode.rn_l);
		p_tree(rn);
	}
}
char nbuf[20];

p_rtnode()
{

	struct radix_mask *rm = rnode.rn_mklist;
	if (rnode.rn_b < 0) {
		if (rnode.rn_mask) {
			printf("\t  mask ");
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_mask),
				    0, -1);
		} else if (rm == 0)
			return;
	} else {
		sprintf(nbuf, "(%d)", rnode.rn_b);
		printf("%6.6s %8.8x : %8.8x", nbuf, rnode.rn_l, rnode.rn_r);
	}
	while (rm) {
		kget(rm, rmask);
		sprintf(nbuf, " %d refs, ", rmask.rm_refs);
		printf(" mk = %8.8x {(%d),%s",
			rm, -1 - rmask.rm_b, rmask.rm_refs ? nbuf : " ");
		p_sockaddr(kgetsa((struct sockaddr *)rmask.rm_mask), 0, -1);
		putchar('}');
		if (rm = rmask.rm_mklist)
			printf(" ->");
	}
	putchar('\n');
}

ntreestuff()
{
	int needed;
	char *buf, *next, *lim;
	register struct rt_msghdr *rtm;

	if ((needed = getkerninfo(KINFO_RT_DUMP, 0, 0, 0)) < 0)
		{ perror("route-getkerninfo-estimate"); exit(1);}
	if ((buf = malloc(needed)) == 0)
		{ printf("out of space\n"); exit(1);}
	if (getkerninfo(KINFO_RT_DUMP, buf, &needed, 0) < 0)
		{ perror("actual retrieval of routing table"); exit(1);}
	lim  = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		np_rtentry(rtm);
	}
}

np_rtentry(rtm)
register struct rt_msghdr *rtm;
{
	register struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
	static int masks_done, old_af, banner_printed;
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
		printf("\nRoute Tree for Protocol Family %d:\n", af);
		old_af = af;
	}
	if (rtm->rtm_addrs == RTA_DST)
		p_sockaddr(sa, 0, 36);
	else {
		p_sockaddr(sa, rtm->rtm_flags, 16);
		if (sa->sa_len == 0)
			sa->sa_len = sizeof(long);
		sa = (struct sockaddr *)(sa->sa_len + (char *)sa);
		p_sockaddr(sa, 0, 18);
	}
	p_flags(rtm->rtm_flags & interesting, "%-6.6s ");
	putchar('\n');
}

p_sockaddr(sa, flags, width)
struct sockaddr *sa;
int flags, width;
{
	char format[20], workbuf[128], *cp, *cplim;
	register char *cpout;

	switch(sa->sa_family) {
	case AF_INET:
	    {
		register struct sockaddr_in *sin = (struct sockaddr_in *)sa;

		cp = (sin->sin_addr.s_addr == 0) ? "default" :
		      ((flags & RTF_HOST) ?
			routename(sin->sin_addr) : netname(sin->sin_addr, 0L));
	    }
		break;

#ifdef NS
	case AF_NS:
		cp = ns_print((struct sockaddr_ns *)sa);
		break;
#endif

	default:
	    {
		register u_short *s = ((u_short *)sa->sa_data), *slim;

		slim = (u_short *) sa + ((sa->sa_len + sizeof(u_short) - 1) /
		    sizeof(u_short));
		cp = workbuf;
		cplim = cp + sizeof(workbuf) - 6;
		cp += sprintf(cp, "(%d)", sa->sa_family);
		while (s < slim && cp < cplim)
			cp += sprintf(cp, " %x", *s++);
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

p_flags(f, format)
register int f;
char *format;
{
	char name[33], *flags;
	register struct bits *p = bits;
	for (flags = name; p->b_mask; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	printf(format, name);
}

p_rtentry(rt)
register struct rtentry *rt;
{
	char name[16];
	register struct sockaddr *sa;
	struct ifnet ifnet;

	p_sockaddr(kgetsa(rt_key(rt)), rt->rt_flags, 16);
	p_sockaddr(kgetsa(rt->rt_gateway), RTF_HOST, 18);
	p_flags(rt->rt_flags, "%-6.6s ");
	printf("%6d %8d ", rt->rt_refcnt, rt->rt_use);
	if (rt->rt_ifp == 0) {
		putchar('\n');
		return;
	}
	kget(rt->rt_ifp, ifnet);
	kvm_read((off_t)ifnet.if_name, name, 16);
	printf(" %.15s%d%s", name, ifnet.if_unit,
		rt->rt_nodes[0].rn_dupedkey ? " =>\n" : "\n");
}

p_ortentry(rt)
register struct ortentry *rt;
{
	char name[16], *flags;
	register struct bits *p;
	register struct sockaddr_in *sin;
	struct ifnet ifnet;

	p_sockaddr(&rt->rt_dst, rt->rt_flags, 16);
	p_sockaddr(&rt->rt_gateway, 0, 18);
	p_flags(rt->rt_flags, "%-6.6s ");
	printf("%6d %8d ", rt->rt_refcnt, rt->rt_use);
	if (rt->rt_ifp == 0) {
		putchar('\n');
		return;
	}
	kget(rt->rt_ifp, ifnet);
	kvm_read((off_t)ifnet.if_name, name, 16);
	printf(" %.15s%d\n", name, ifnet.if_unit);
}

char *
routename(in)
	struct in_addr in;
{
	register char *cp;
	static char line[MAXHOSTNAMELEN + 1];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN + 1];
	static int first = 1;
	char *index();

	if (first) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = index(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag) {
		hp = gethostbyaddr((char *)&in, sizeof (struct in_addr),
			AF_INET);
		if (hp) {
			if ((cp = index(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (cp)
		strncpy(line, cp, sizeof(line) - 1);
	else {
#define C(x)	((x) & 0xff)
		in.s_addr = ntohl(in.s_addr);
		sprintf(line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
	}
	return (line);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(in, mask)
	struct in_addr in;
	u_long mask;
{
	char *cp = 0;
	static char line[MAXHOSTNAMELEN + 1];
	struct netent *np = 0;
	u_long net;
	register i;
	int subnetshift;

	i = ntohl(in.s_addr);
	if (!nflag && i) {
		if (mask == 0) {
			if (IN_CLASSA(i)) {
				mask = IN_CLASSA_NET;
				subnetshift = 8;
			} else if (IN_CLASSB(i)) {
				mask = IN_CLASSB_NET;
				subnetshift = 8;
			} else {
				mask = IN_CLASSC_NET;
				subnetshift = 4;
			}
			/*
			 * If there are more bits than the standard mask
			 * would suggest, subnets must be in use.
			 * Guess at the subnet mask, assuming reasonable
			 * width subnet fields.
			 */
			while (i &~ mask)
				mask = (long)mask >> subnetshift;
		}
		net = i & mask;
		while ((mask & 1) == 0)
			mask >>= 1, net >>= 1;
		np = getnetbyaddr(net, AF_INET);
		if (np)
			cp = np->n_name;
	}	
	if (cp)
		strncpy(line, cp, sizeof(line) - 1);
	else if ((i & 0xffffff) == 0)
		sprintf(line, "%u", C(i >> 24));
	else if ((i & 0xffff) == 0)
		sprintf(line, "%u.%u", C(i >> 24) , C(i >> 16));
	else if ((i & 0xff) == 0)
		sprintf(line, "%u.%u.%u", C(i >> 24), C(i >> 16), C(i >> 8));
	else
		sprintf(line, "%u.%u.%u.%u", C(i >> 24),
			C(i >> 16), C(i >> 8), C(i));
	return (line);
}

/*
 * Print routing statistics
 */
rt_stats(off)
	off_t off;
{
	struct rtstat rtstat;

	if (off == 0) {
		printf("rtstat: symbol not in namelist\n");
		return;
	}
	kvm_read(off, (char *)&rtstat, sizeof (rtstat));
	printf("routing:\n");
	printf("\t%u bad routing redirect%s\n",
		rtstat.rts_badredirect, plural(rtstat.rts_badredirect));
	printf("\t%u dynamically created route%s\n",
		rtstat.rts_dynamic, plural(rtstat.rts_dynamic));
	printf("\t%u new gateway%s due to redirects\n",
		rtstat.rts_newgateway, plural(rtstat.rts_newgateway));
	printf("\t%u destination%s found unreachable\n",
		rtstat.rts_unreach, plural(rtstat.rts_unreach));
	printf("\t%u use%s of a wildcard route\n",
		rtstat.rts_wildcard, plural(rtstat.rts_wildcard));
}
#ifdef NS
short ns_nullh[] = {0,0,0};
short ns_bh[] = {-1,-1,-1};

char *
ns_print(sns)
struct sockaddr_ns *sns;
{
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
		for (p = chost; *p == '0' && p < chost + 12; p++);
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
ns_phost(sns)
struct sockaddr_ns *sns;
{
	struct sockaddr_ns work;
	static union ns_net ns_zeronet;
	char *p;
	
	work = *sns;
	work.sns_addr.x_port = 0;
	work.sns_addr.x_net = ns_zeronet;

	p = ns_print(&work);
	if (strncmp("0H.", p, 3) == 0) p += 3;
	return(p);
}
upHex(p0)
char *p0;
{
	register char *p = p0;
	for (; *p; p++) switch (*p) {

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		*p += ('A' - 'a');
	}
}
#endif /* NS */
