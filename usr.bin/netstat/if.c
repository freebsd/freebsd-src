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
	"$Id$";
#endif /* not lint */

#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#define KERNEL 1
#include <netinet/if_ether.h>
#undef KERNEL
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
#include <string.h>
#include <unistd.h>

#include "netstat.h"

#define	YES	1
#define	NO	0

static void sidewaysintpr __P((u_int, u_long));
static void catchalarm __P((int));

/*
 * Print a description of the network interfaces.
 */
void
intpr(interval, ifnetaddr)
	int interval;
	u_long ifnetaddr;
{
	struct ifnet ifnet;
	struct ifnethead ifnethead;
	union {
		struct ifaddr ifa;
		struct in_ifaddr in;
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
	struct sockaddr *sa;
	char name[32], tname[16];

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
	ifnetaddr = (u_long)ifnethead.tqh_first;
	if (kread(ifnetaddr, (char *)&ifnet, sizeof ifnet))
		return;

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
	ifaddraddr = 0;
	while (ifnetaddr || ifaddraddr) {
		struct sockaddr_in *sin;
		register char *cp;
		int n, m;

		if (ifaddraddr == 0) {
			ifnetfound = ifnetaddr;
			if (kread(ifnetaddr, (char *)&ifnet, sizeof ifnet) ||
			    kread((u_long)ifnet.if_name, tname, 16))
				return;
			tname[15] = '\0';
			ifnetaddr = (u_long)ifnet.if_link.tqe_next;
			snprintf(name, 32, "%s%d", tname, ifnet.if_unit);
			if (interface != 0 && (strcmp(name, interface) != 0))
				continue;
			cp = index(name, '\0');
			if ((ifnet.if_flags&IFF_UP) == 0)
				*cp++ = '*';
			*cp = '\0';
			ifaddraddr = (u_long)ifnet.if_addrhead.tqh_first;
		}
		printf("%-5.5s %-5lu ", name, ifnet.if_mtu);
		ifaddrfound = ifaddraddr;
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
				CP(&ifaddr); sa = (struct sockaddr *)cp;
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
				break;
			case AF_IPX:
				{
				struct sockaddr_ipx *sipx =
					(struct sockaddr_ipx *)sa;
				u_long net;
				char netnum[10];

				*(union ipx_net *) &net = sipx->sipx_addr.x_net;
				sprintf(netnum, "%lx", ntohl(net));
				printf("ipx:%-8s ", netnum);
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
				    cp = (char *)LLADDR(sdl);
				    n = sdl->sdl_alen;
				}
				m = printf("%-11.11s ", "<Link>");
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
						    n > 0 ? '.' : ' ');
				m = 30 - m;
				while (m-- > 0)
					putchar(' ');
				break;
			}
			ifaddraddr = (u_long)ifaddr.ifa.ifa_link.tqe_next;
		}
		printf("%8lu %5lu ",
		    ifnet.if_ipackets, ifnet.if_ierrors);
		if (bflag)
			printf("%10lu ", ifnet.if_ibytes);
		printf("%8lu %5lu ",
		    ifnet.if_opackets, ifnet.if_oerrors);
		if (bflag)
			printf("%10lu ", ifnet.if_obytes);
		printf("%5lu", ifnet.if_collisions);
		if (tflag)
			printf(" %3d", ifnet.if_timer);
		if (dflag)
			printf(" %3d", ifnet.if_snd.ifq_drops);
		putchar('\n');
		if (aflag && ifaddrfound) {
		    /*
		     * Print family's multicast addresses
		     */
		    switch (sa->sa_family) {
		    case AF_INET:
			{
			    u_long multiaddr;
			    struct in_multi inm;

			    multiaddr = (u_long)ifaddr.in.ia_multiaddrs.lh_first;
			    while (multiaddr != 0) {
				    kread(multiaddr, (char *)&inm,
							sizeof inm);
				    multiaddr = (u_long)inm.inm_entry.le_next;
				    printf("%23s %s\n", "",
					    routename(inm.inm_addr.s_addr));
			    }
			    break;
			}
		    case AF_LINK:
			    switch (ifnet.if_type) {
			    case IFT_ETHER:
			    case IFT_FDDI:	/*XXX*/
				{
				    off_t multiaddr;
				    struct arpcom ac;
				    struct ether_multi enm;

				    kread(ifnetfound, (char *)&ac, sizeof ac);
				    multiaddr = (u_long)ac.ac_multiaddrs;
				    while (multiaddr != 0) {
					    kread(multiaddr, (char *)&enm,
						    sizeof enm);
					    multiaddr = (u_long)enm.enm_next;
					    printf("%23s %s", "",
						ether_ntoa(&enm.enm_addrlo));
					    if (bcmp(&enm.enm_addrlo,
						     &enm.enm_addrhi, 6) != 0)
						printf(" to %s",
						    ether_ntoa(&enm.enm_addrhi));
					    printf("\n");
				    }
				    break;
				}
			    default:
				    break;
			    }
		    default:
			    break;
		    }
		}
	}
}

#define	MAXIF	10
struct	iftot {
	char	ift_name[16];		/* interface name */
	u_int	ift_ip;			/* input packets */
	u_int	ift_ie;			/* input errors */
	u_int	ift_op;			/* output packets */
	u_int	ift_oe;			/* output errors */
	u_int	ift_co;			/* collisions */
	u_int	ift_dr;			/* drops */
	u_int	ift_ib;			/* input bytes */
	u_int	ift_ob;			/* output bytes */
} iftot[MAXIF];

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 * XXX - should be rewritten to use ifmib(4).
 */
static void
sidewaysintpr(interval, off)
	unsigned interval;
	u_long off;
{
	struct ifnet ifnet;
	u_long firstifnet;
	struct ifnethead ifnethead;
	register struct iftot *ip, *total;
	register int line;
	struct iftot *lastif, *sum, *interesting;
	int oldmask, first;
	u_long interesting_off;

	if (kread(off, (char *)&ifnethead, sizeof ifnethead))
		return;
	firstifnet = (u_long)ifnethead.tqh_first;

	lastif = iftot;
	sum = iftot + MAXIF - 1;
	total = sum - 1;
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
		ip++;
		if (ip >= iftot + MAXIF - 2)
			break;
		off = (u_long) ifnet.if_link.tqe_next;
	}
	lastif = ip;

	(void)signal(SIGALRM, catchalarm);
	signalled = NO;
	(void)alarm(interval);
	for (ip = iftot; ip < iftot + MAXIF; ip++) {
		ip->ift_ip = 0;
		ip->ift_ie = 0;
		ip->ift_ib = 0;
		ip->ift_op = 0;
		ip->ift_oe = 0;
		ip->ift_ob = 0;
		ip->ift_co = 0;
		ip->ift_dr = 0;
	}
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
		for (off = firstifnet, ip = iftot; off && ip < lastif; ip++) {
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
			off = (u_long) ifnet.if_link.tqe_next;
		}
		if (!first) {
			printf("%10u %5u %10u %10u %5u %10u %5u",
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
catchalarm(signo)
	int signo;
{
	signalled = YES;
}
