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
/*
static char sccsid[] = "@(#)if.c	8.3 (Berkeley) 4/28/95";
*/
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/bridge.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif
#ifdef ISO
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#endif
#include <arpa/inet.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netstat.h"

#define	YES	1
#define	NO	0

static void sidewaysintpr (u_int, u_long);
static void catchalarm (int);

#ifdef INET6
char *netname6 (struct sockaddr_in6 *, struct in6_addr *);
static char ntop_buf[INET6_ADDRSTRLEN];		/* for inet_ntop() */
static int bdg_done;
#endif

/* print bridge statistics */
void
bdg_stats(u_long dummy __unused, char *name, int af __unused)
{
    int i;
    size_t slen ;
    struct bdg_stats s ;
    int mib[4] ;

    slen = sizeof(s);

    mib[0] = CTL_NET ;
    mib[1] = PF_LINK ;
    mib[2] = IFT_ETHER ;
    mib[3] = PF_BDG ;
    if (sysctl(mib,4, &s,&slen,NULL,0)==-1)
	return ; /* no bridging */
#ifdef INET6
    if (bdg_done != 0)
	return;
    else
	bdg_done = 1;
#endif
    printf("-- Bridging statistics (%s) --\n", name) ;
    printf(
"Name          In      Out  Forward     Drop    Bcast    Mcast    Local  Unknown\n");
    for (i = 0 ; i < 16 ; i++) {
	if (s.s[i].name[0])
	printf("%-6s %9ld%9ld%9ld%9ld%9ld%9ld%9ld%9ld\n",
	  s.s[i].name,
	  s.s[i].p_in[(int)BDG_IN],
	  s.s[i].p_in[(int)BDG_OUT],
	  s.s[i].p_in[(int)BDG_FORWARD],
	  s.s[i].p_in[(int)BDG_DROP],
	  s.s[i].p_in[(int)BDG_BCAST],
	  s.s[i].p_in[(int)BDG_MCAST],
	  s.s[i].p_in[(int)BDG_LOCAL],
	  s.s[i].p_in[(int)BDG_UNKNOWN] );
    }
}




/*
 * Display a formatted value, or a '-' in the same space.
 */
static void
show_stat(const char *fmt, int width, u_long value, short showvalue)
{
	char newfmt[32];

	/* Construct the format string */
	if (showvalue) {
		sprintf(newfmt, "%%%d%s", width, fmt);
		printf(newfmt, value);
	} else {
		sprintf(newfmt, "%%%ds", width);
		printf(newfmt, "-");
	}
}



/*
 * Print a description of the network interfaces.
 */
void
intpr(int interval, u_long ifnetaddr, void (*pfunc)(char *))
{
	struct ifnet ifnet;
	struct ifnethead ifnethead;
	union {
		struct ifaddr ifa;
		struct in_ifaddr in;
#ifdef INET6
		struct in6_ifaddr in6;
#endif
		struct ipx_ifaddr ipx;
#ifdef NS
		struct ns_ifaddr ns;
#endif
#ifdef ISO
		struct iso_ifaddr iso;
#endif
	} ifaddr;
	u_long ifaddraddr;
	u_long ifaddrfound;
	u_long ifnetfound;
	u_long opackets;
	u_long ipackets;
	u_long obytes;
	u_long ibytes;
	u_long oerrors;
	u_long ierrors;
	u_long collisions;
	short timer;
	int drops;
	struct sockaddr *sa = NULL;
	char name[32], tname[16];
	short network_layer;
	short link_layer;

	if (ifnetaddr == 0) {
		printf("ifnet: symbol not defined\n");
		return;
	}
	if (interval) {
		sidewaysintpr((unsigned)interval, ifnetaddr);
		return;
	}
	if (kread(ifnetaddr, (char *)&ifnethead, sizeof ifnethead))
		return;
	ifnetaddr = (u_long)TAILQ_FIRST(&ifnethead);
	if (kread(ifnetaddr, (char *)&ifnet, sizeof ifnet))
		return;

	if (!pfunc) {
		printf("%-5.5s %-5.5s %-13.13s %-15.15s %8.8s %5.5s",
		       "Name", "Mtu", "Network", "Address", "Ipkts", "Ierrs");
		if (bflag)
			printf(" %10.10s","Ibytes");
		printf(" %8.8s %5.5s", "Opkts", "Oerrs");
		if (bflag)
			printf(" %10.10s","Obytes");
		printf(" %5s", "Coll");
		if (tflag)
			printf(" %s", "Time");
		if (dflag)
			printf(" %s", "Drop");
		putchar('\n');
	}
	ifaddraddr = 0;
	while (ifnetaddr || ifaddraddr) {
		struct sockaddr_in *sin;
#ifdef INET6
		struct sockaddr_in6 *sin6;
#endif
		register char *cp;
		int n, m;

		network_layer = 0;
		link_layer = 0;

		if (ifaddraddr == 0) {
			ifnetfound = ifnetaddr;
			if (kread(ifnetaddr, (char *)&ifnet, sizeof ifnet) ||
			    kread((u_long)ifnet.if_name, tname, 16))
				return;
			tname[15] = '\0';
			ifnetaddr = (u_long)TAILQ_NEXT(&ifnet, if_link);
			snprintf(name, 32, "%s%d", tname, ifnet.if_unit);
			if (interface != 0 && (strcmp(name, interface) != 0))
				continue;
			cp = index(name, '\0');

			if (pfunc) {
				(*pfunc)(name);
				continue;
			}

			if ((ifnet.if_flags&IFF_UP) == 0)
				*cp++ = '*';
			*cp = '\0';
			ifaddraddr = (u_long)TAILQ_FIRST(&ifnet.if_addrhead);
		}
		printf("%-5.5s %-5lu ", name, ifnet.if_mtu);
		ifaddrfound = ifaddraddr;

		/*
		 * Get the interface stats.  These may get
		 * overriden below on a per-interface basis.
		 */
		opackets = ifnet.if_opackets;
		ipackets = ifnet.if_ipackets;
		obytes = ifnet.if_obytes;
		ibytes = ifnet.if_ibytes;
		oerrors = ifnet.if_oerrors;
		ierrors = ifnet.if_ierrors;
		collisions = ifnet.if_collisions;
		timer = ifnet.if_timer;
		drops = ifnet.if_snd.ifq_drops;

		if (ifaddraddr == 0) {
			printf("%-13.13s ", "none");
			printf("%-15.15s ", "none");
		} else {
			if (kread(ifaddraddr, (char *)&ifaddr, sizeof ifaddr)) {
				ifaddraddr = 0;
				continue;
			}
#define CP(x) ((char *)(x))
			cp = (CP(ifaddr.ifa.ifa_addr) - CP(ifaddraddr)) +
				CP(&ifaddr);
			sa = (struct sockaddr *)cp;
			switch (sa->sa_family) {
			case AF_UNSPEC:
				printf("%-13.13s ", "none");
				printf("%-15.15s ", "none");
				break;
			case AF_INET:
				sin = (struct sockaddr_in *)sa;
#ifdef notdef
				/* can't use inet_makeaddr because kernel
				 * keeps nets unshifted.
				 */
				in = inet_makeaddr(ifaddr.in.ia_subnet,
					INADDR_ANY);
				printf("%-13.13s ", netname(in.s_addr,
				    ifaddr.in.ia_subnetmask));
#else
				printf("%-13.13s ",
				    netname(htonl(ifaddr.in.ia_subnet),
				    ifaddr.in.ia_subnetmask));
#endif
				printf("%-15.15s ",
				    routename(sin->sin_addr.s_addr));

				network_layer = 1;
				break;
#ifdef INET6
			case AF_INET6:
				sin6 = (struct sockaddr_in6 *)sa;
				printf("%-11.11s ",
				       netname6(&ifaddr.in6.ia_addr,
						&ifaddr.in6.ia_prefixmask.sin6_addr));
				printf("%-17.17s ",
				    (char *)inet_ntop(AF_INET6,
					&sin6->sin6_addr,
					ntop_buf, sizeof(ntop_buf)));

				network_layer = 1;
				break;
#endif /*INET6*/
			case AF_IPX:
				{
				struct sockaddr_ipx *sipx =
					(struct sockaddr_ipx *)sa;
				u_long net;
				char netnum[10];

				*(union ipx_net *) &net = sipx->sipx_addr.x_net;
				sprintf(netnum, "%lx", (u_long)ntohl(net));
				printf("ipx:%-8s  ", netnum);
/*				printf("ipx:%-8s ", netname(net, 0L)); */
				printf("%-15s ",
				    ipx_phost((struct sockaddr *)sipx));
				}
				break;

			case AF_APPLETALK:
				printf("atalk:%-12.12s ",atalk_print(sa,0x10) );
				printf("%-9.9s  ",atalk_print(sa,0x0b) );
				break;
#ifdef NS
			case AF_NS:
				{
				struct sockaddr_ns *sns =
					(struct sockaddr_ns *)sa;
				u_long net;
				char netnum[10];

				*(union ns_net *) &net = sns->sns_addr.x_net;
				sprintf(netnum, "%lxH", ntohl(net));
				upHex(netnum);
				printf("ns:%-8s ", netnum);
				printf("%-15s ",
				    ns_phost((struct sockaddr *)sns));
				}
				break;
#endif
			case AF_LINK:
				{
				struct sockaddr_dl *sdl =
					(struct sockaddr_dl *)sa;
				char linknum[10];
				cp = (char *)LLADDR(sdl);
				n = sdl->sdl_alen;
				sprintf(linknum, "<Link#%d>", sdl->sdl_index);
				m = printf("%-11.11s ", linknum);
				}
				goto hexprint;
			default:
				m = printf("(%d)", sa->sa_family);
				for (cp = sa->sa_len + (char *)sa;
					--cp > sa->sa_data && (*cp == 0);) {}
				n = cp - sa->sa_data + 1;
				cp = sa->sa_data;
			hexprint:
				while (--n >= 0)
					m += printf("%02x%c", *cp++ & 0xff,
						    n > 0 ? ':' : ' ');
				m = 30 - m;
				while (m-- > 0)
					putchar(' ');

				link_layer = 1;
				break;
			}

			/*
			 * Fixup the statistics for interfaces that
			 * update stats for their network addresses
			 */
			if (network_layer) {
				opackets = ifaddr.in.ia_ifa.if_opackets;
				ipackets = ifaddr.in.ia_ifa.if_ipackets;
				obytes = ifaddr.in.ia_ifa.if_obytes;
				ibytes = ifaddr.in.ia_ifa.if_ibytes;
			}

			ifaddraddr = (u_long)TAILQ_NEXT(&ifaddr.ifa, ifa_link);
		}

		show_stat("lu", 8, ipackets, link_layer|network_layer);
		printf(" ");
		show_stat("lu", 5, ierrors, link_layer);
		printf(" ");
		if (bflag) {
			show_stat("lu", 10, ibytes, link_layer|network_layer);
			printf(" ");
		}
		show_stat("lu", 8, opackets, link_layer|network_layer);
		printf(" ");
		show_stat("lu", 5, oerrors, link_layer);
		printf(" ");
		if (bflag) {
			show_stat("lu", 10, obytes, link_layer|network_layer);
			printf(" ");
		}
		show_stat("lu", 5, collisions, link_layer);
		if (tflag) {
			printf(" ");
			show_stat("d", 3, timer, link_layer);
		}
		if (dflag) {
			printf(" ");
			show_stat("d", 3, drops, link_layer);
		}
		putchar('\n');
		if (aflag && ifaddrfound) {
			/*
			 * Print family's multicast addresses
			 */
			u_long multiaddr;
			struct ifmultiaddr ifma;
			union {
				struct sockaddr sa;
				struct sockaddr_in in;
#ifdef INET6
				struct sockaddr_in6 in6;
#endif /* INET6 */
				struct sockaddr_dl dl;
			} msa;
			const char *fmt;

			for(multiaddr = (u_long)ifnet.if_multiaddrs.lh_first;
			    multiaddr;
			    multiaddr = (u_long)ifma.ifma_link.le_next) {
				if (kread(multiaddr, (char *)&ifma,
					  sizeof ifma))
					break;
				if (kread((u_long)ifma.ifma_addr, (char *)&msa,
					  sizeof msa))
					break;
				if (msa.sa.sa_family != sa->sa_family)
					continue;
				
				fmt = 0;
				switch (msa.sa.sa_family) {
				case AF_INET:
					fmt = routename(msa.in.sin_addr.s_addr);
					break;
#ifdef INET6
				case AF_INET6:
					printf("%23s %-19.19s(refs: %d)\n", "",
					       inet_ntop(AF_INET6,
							 &msa.in6.sin6_addr,
							 ntop_buf,
							 sizeof(ntop_buf)),
					       ifma.ifma_refcount);
					break;
#endif /* INET6 */
				case AF_LINK:
					switch (msa.dl.sdl_type) {
					case IFT_ETHER:
					case IFT_FDDI:
						fmt = ether_ntoa(
							(struct ether_addr *)
							LLADDR(&msa.dl));
						break;
					}
					break;
				}
				if (fmt)
					printf("%23s %s\n", "", fmt);
			}
		}
	}
}

struct	iftot {
	SLIST_ENTRY(iftot) chain;
	char	ift_name[16];		/* interface name */
	u_long	ift_ip;			/* input packets */
	u_long	ift_ie;			/* input errors */
	u_long	ift_op;			/* output packets */
	u_long	ift_oe;			/* output errors */
	u_long	ift_co;			/* collisions */
	u_int	ift_dr;			/* drops */
	u_long	ift_ib;			/* input bytes */
	u_long	ift_ob;			/* output bytes */
};

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 * XXX - should be rewritten to use ifmib(4).
 */
static void
sidewaysintpr(unsigned interval, u_long off)
{
	struct ifnet ifnet;
	u_long firstifnet;
	struct ifnethead ifnethead;
	struct iftot *iftot, *ip, *ipn, *total, *sum, *interesting;
	register int line;
	int oldmask, first;
	u_long interesting_off;

	if (kread(off, (char *)&ifnethead, sizeof ifnethead))
		return;
	firstifnet = (u_long)TAILQ_FIRST(&ifnethead);

	if ((iftot = malloc(sizeof(struct iftot))) == NULL) {
		printf("malloc failed\n");
		exit(1);
	}
	memset(iftot, 0, sizeof(struct iftot));

	interesting = NULL;
	interesting_off = 0;
	for (off = firstifnet, ip = iftot; off;) {
		char name[16], tname[16];

		if (kread(off, (char *)&ifnet, sizeof ifnet))
			break;
		if (kread((u_long)ifnet.if_name, tname, 16))
			break;
		tname[15] = '\0';
		snprintf(name, 16, "%s%d", tname, ifnet.if_unit);
		if (interface && strcmp(name, interface) == 0) {
			interesting = ip;
			interesting_off = off;
		}
		snprintf(ip->ift_name, 16, "(%s)", name);;
		if ((ipn = malloc(sizeof(struct iftot))) == NULL) {
			printf("malloc failed\n");
			exit(1);
		}
		memset(ipn, 0, sizeof(struct iftot));
		SLIST_NEXT(ip, chain) = ipn;
		ip = ipn;
		off = (u_long)TAILQ_NEXT(&ifnet, if_link);
	}
	if ((total = malloc(sizeof(struct iftot))) == NULL) {
		printf("malloc failed\n");
		exit(1);
	}
	memset(total, 0, sizeof(struct iftot));
	if ((sum = malloc(sizeof(struct iftot))) == NULL) {
		printf("malloc failed\n");
		exit(1);
	}
	memset(sum, 0, sizeof(struct iftot));


	(void)signal(SIGALRM, catchalarm);
	signalled = NO;
	(void)alarm(interval);
	first = 1;
banner:
	printf("%17s %14s %16s", "input",
	    interesting ? interesting->ift_name : "(Total)", "output");
	putchar('\n');
	printf("%10s %5s %10s %10s %5s %10s %5s",
	    "packets", "errs", "bytes", "packets", "errs", "bytes", "colls");
	if (dflag)
		printf(" %5.5s", "drops");
	putchar('\n');
	fflush(stdout);
	line = 0;
loop:
	if (interesting != NULL) {
		ip = interesting;
		if (kread(interesting_off, (char *)&ifnet, sizeof ifnet)) {
			printf("???\n");
			exit(1);
		};
		if (!first) {
			printf("%10lu %5lu %10lu %10lu %5lu %10lu %5lu",
				ifnet.if_ipackets - ip->ift_ip,
				ifnet.if_ierrors - ip->ift_ie,
				ifnet.if_ibytes - ip->ift_ib,
				ifnet.if_opackets - ip->ift_op,
				ifnet.if_oerrors - ip->ift_oe,
				ifnet.if_obytes - ip->ift_ob,
				ifnet.if_collisions - ip->ift_co);
			if (dflag)
				printf(" %5u", ifnet.if_snd.ifq_drops - ip->ift_dr);
		}
		ip->ift_ip = ifnet.if_ipackets;
		ip->ift_ie = ifnet.if_ierrors;
		ip->ift_ib = ifnet.if_ibytes;
		ip->ift_op = ifnet.if_opackets;
		ip->ift_oe = ifnet.if_oerrors;
		ip->ift_ob = ifnet.if_obytes;
		ip->ift_co = ifnet.if_collisions;
		ip->ift_dr = ifnet.if_snd.ifq_drops;
	} else {
		sum->ift_ip = 0;
		sum->ift_ie = 0;
		sum->ift_ib = 0;
		sum->ift_op = 0;
		sum->ift_oe = 0;
		sum->ift_ob = 0;
		sum->ift_co = 0;
		sum->ift_dr = 0;
		for (off = firstifnet, ip = iftot;
		     off && SLIST_NEXT(ip, chain) != NULL;
		     ip = SLIST_NEXT(ip, chain)) {
			if (kread(off, (char *)&ifnet, sizeof ifnet)) {
				off = 0;
				continue;
			}
			sum->ift_ip += ifnet.if_ipackets;
			sum->ift_ie += ifnet.if_ierrors;
			sum->ift_ib += ifnet.if_ibytes;
			sum->ift_op += ifnet.if_opackets;
			sum->ift_oe += ifnet.if_oerrors;
			sum->ift_ob += ifnet.if_obytes;
			sum->ift_co += ifnet.if_collisions;
			sum->ift_dr += ifnet.if_snd.ifq_drops;
			off = (u_long)TAILQ_NEXT(&ifnet, if_link);
		}
		if (!first) {
			printf("%10lu %5lu %10lu %10lu %5lu %10lu %5lu",
				sum->ift_ip - total->ift_ip,
				sum->ift_ie - total->ift_ie,
				sum->ift_ib - total->ift_ib,
				sum->ift_op - total->ift_op,
				sum->ift_oe - total->ift_oe,
				sum->ift_ob - total->ift_ob,
				sum->ift_co - total->ift_co);
			if (dflag)
				printf(" %5u", sum->ift_dr - total->ift_dr);
		}
		*total = *sum;
	}
	if (!first)
		putchar('\n');
	fflush(stdout);
	oldmask = sigblock(sigmask(SIGALRM));
	if (! signalled) {
		sigpause(0);
	}
	sigsetmask(oldmask);
	signalled = NO;
	(void)alarm(interval);
	line++;
	first = 0;
	if (line == 21)
		goto banner;
	else
		goto loop;
	/*NOTREACHED*/
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
static void
catchalarm(int signo __unused)
{
	signalled = YES;
}
