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
/* From: static char sccsid[] = "@(#)if.c	5.15 (Berkeley) 3/1/91"; */
static const char if_c_rcsid[] = 
	"$Id: if.c,v 1.4 1994/05/17 21:10:14 jkh Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif /* NS */

#ifdef ISO
#include <netiso/iso.h>
#include <netiso/iso_var.h>
#endif /* ISO */

#include <stdio.h>
#include <signal.h>

#define	YES	1
#define	NO	0

extern	int aflag;
extern	int tflag;
extern	int dflag;
extern	int nflag;
extern	char *interface;
extern	int unit;
extern	char *routename(), *netname(), *ns_phost();
char *index();

/*
 * Return a printable string representation of an Ethernet address.
 */
char *etherprint(enaddr)
	char enaddr[6];
{
	static char string[18];
	unsigned char *en = (unsigned char *)enaddr;

	sprintf(string, "%02x:%02x:%02x:%02x:%02x:%02x",
		en[0], en[1], en[2], en[3], en[4], en[5] );
	string[17] = '\0';
	return(string);
}

/*
 * Print a description of the network interfaces.
 */
intpr(interval, ifnetaddr)
	int interval;
	off_t ifnetaddr;
{
	struct ifnet ifnet;
	union {
		struct ifaddr ifa;
		struct in_ifaddr in;
#ifdef NS
		struct ns_ifaddr ns;
#endif
#ifdef ISO
		struct iso_ifaddr iso;
#endif
	} ifaddr;
	off_t ifaddraddr, ifaddrfound, ifnetfound;
	struct sockaddr *sa;
	char name[16];

	if (ifnetaddr == 0) {
		printf("ifnet: symbol not defined\n");
		return;
	}
	if (interval) {
		sidewaysintpr((unsigned)interval, ifnetaddr);
		return;
	}
	kvm_read(ifnetaddr, (char *)&ifnetaddr, sizeof ifnetaddr);
	printf("%-5.5s %-5.5s %-11.11s %-15.15s %8.8s %5.5s %8.8s %5.5s",
		"Name", "Mtu", "Network", "Address", "Ipkts", "Ierrs",
		"Opkts", "Oerrs");
	printf(" %5s", "Coll");
	if (tflag)
		printf(" %s", "Time");
	if (dflag)
		printf(" %s", "Drop");
	putchar('\n');
	ifaddraddr = 0;
	ifnetfound = 0;
	while (ifnetaddr || ifaddraddr) {
		struct sockaddr_in *sin;
		register char *cp;
		int n, m;
		struct in_addr inet_makeaddr();

		ifnetfound = ifnetaddr;
		if (ifaddraddr == 0) {
			kvm_read(ifnetaddr, (char *)&ifnet, sizeof ifnet);
			kvm_read((off_t)ifnet.if_name, name, 16);
			name[15] = '\0';
			ifnetaddr = (off_t) ifnet.if_next;
			if (interface != 0 &&
			    (strcmp(name, interface) != 0 || unit != ifnet.if_unit))
				continue;
			cp = index(name, '\0');
			cp += sprintf(cp, "%d", ifnet.if_unit);
			if ((ifnet.if_flags&IFF_UP) == 0)
				*cp++ = '*';
			*cp = '\0';
			ifaddraddr = (off_t)ifnet.if_addrlist;
		}
		printf("%-5.5s %-5d ", name, ifnet.if_mtu);
		ifaddrfound = ifaddraddr;
		if (ifaddraddr == 0) {
			printf("%-11.11s ", "none");
			printf("%-15.15s ", "none");
		} else {
			kvm_read(ifaddraddr, (char *)&ifaddr, sizeof ifaddr);
#define CP(x) ((char *)(x))
			cp = (CP(ifaddr.ifa.ifa_addr) - CP(ifaddraddr)) +
				CP(&ifaddr); sa = (struct sockaddr *)cp;
			switch (sa->sa_family) {
			case AF_UNSPEC:
				printf("%-11.11s ", "none");
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
				printf("%-11.11s ", netname(in));
#else
				printf("%-11.11s ",
					netname(htonl(ifaddr.in.ia_subnet),
						ifaddr.in.ia_subnetmask));
#endif
				printf("%-15.15s ", routename(sin->sin_addr));
				break;
#ifdef NS
			case AF_NS:
				{
				struct sockaddr_ns *sns =
					(struct sockaddr_ns *)sa;
				u_long net;
				char netnum[8];
				char *ns_phost();

				*(union ns_net *) &net = sns->sns_addr.x_net;
		sprintf(netnum, "%lxH", ntohl(net));
				upHex(netnum);
				printf("ns:%-8s ", netnum);
				printf("%-15s ", ns_phost(sns));
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
				m = printf("<Link>");
				goto hexprint;
			default:
				m = printf("(%d)", sa->sa_family);
				for (cp = sa->sa_len + (char *)sa;
					--cp > sa->sa_data && (*cp == 0);) {}
				n = cp - sa->sa_data + 1;
				cp = sa->sa_data;
			hexprint:
				while (--n >= 0)
					m += printf("%x%c", *cp++ & 0xff,
						    n > 0 ? '.' : ' ');
				m = 28 - m;
				while (m-- > 0)
					putchar(' ');
				break;
			}
			ifaddraddr = (off_t)ifaddr.ifa.ifa_next;
		}
		printf("%8d %5d %8d %5d %5d",
		    ifnet.if_ipackets, ifnet.if_ierrors,
		    ifnet.if_opackets, ifnet.if_oerrors,
		    ifnet.if_collisions);
		if (tflag)
			printf(" %3d", ifnet.if_timer);
		if (dflag)
			printf(" %3d", ifnet.if_snd.ifq_drops);
		putchar('\n');

		/*XXX this needs work for bsdi */
		if (aflag && ifaddrfound) {
			/*
			 * print any internet multicast addresses
			 */
			switch (sa->sa_family) {
			case AF_INET:
			    {
				off_t multiaddr;
				struct in_multi inm;

				multiaddr = (off_t)ifaddr.in.ia_multiaddrs;
				while (multiaddr != 0) {
					kvm_read(multiaddr, (char *)&inm,
						 sizeof inm);
					multiaddr = (off_t)inm.inm_next;
					printf("%23s %-19.19s\n", "",
					       routename(inm.inm_addr.s_addr));
				}
				break;
			    }
			default:
				break;
			}
		}
#ifdef notyet
		if (aflag && ifaddraddr == 0) {
			/*
			 * print link-level addresses
			 * (Is there a better way to determine
			 *  the type of network??)
			 */
			if (strncmp(name, "qe", 2) == 0 ||    /* Ethernet */
			    strncmp(name, "de", 2) == 0 ||
			    strncmp(name, "ex", 2) == 0 ||
			    strncmp(name, "il", 2) == 0 ||
			    strncmp(name, "le", 2) == 0 ||
			    strncmp(name, "se", 2) == 0 ||
			    strncmp(name, "ie", 2) == 0) {
			    strncmp(name, "we", 2) == 0) {
			    strncmp(name, "el", 2) == 0) {
			    /* "ec", the 3Com interface for Suns, is not    */
			    /* included, although it does handle multicast, */
			    /* because it does not filter specific ethernet */
			    /* multicast addresses, but just accepts all.   */
				off_t multiaddr;
				struct arpcom ac;
				struct ether_multi enm;

				kvm_read(ifnetfound, (char *)&ac, sizeof ac);
				printf("%23s %s\n", "",
					etherprint(&ac.ac_enaddr));
				multiaddr = (off_t)ac.ac_multiaddrs;
				while (multiaddr != 0) {
					kvm_read(multiaddr, (char *)&enm,
						 sizeof enm);
					multiaddr = (off_t)enm.enm_next;
					printf("%23s %s", "",
						etherprint(&enm.enm_addrlo));
					if (bcmp(&enm.enm_addrlo,
						 &enm.enm_addrhi, 6) != 0)
						printf(" to %s",
						etherprint(&enm.enm_addrhi));
					printf("\n");
				}
			}
		}
#endif
	}
}

#define	MAXIF	10
struct	iftot {
	char	ift_name[16];		/* interface name */
	int	ift_ip;			/* input packets */
	int	ift_ie;			/* input errors */
	int	ift_op;			/* output packets */
	int	ift_oe;			/* output errors */
	int	ift_co;			/* collisions */
	int	ift_dr;			/* drops */
} iftot[MAXIF];

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
sidewaysintpr(interval, off)
	unsigned interval;
	off_t off;
{
	struct ifnet ifnet;
	off_t firstifnet;
	register struct iftot *ip, *total;
	register int line;
	struct iftot *lastif, *sum, *interesting;
	int oldmask;
	void catchalarm();

	kvm_read(off, (char *)&firstifnet, sizeof (off_t));
	lastif = iftot;
	sum = iftot + MAXIF - 1;
	total = sum - 1;
	interesting = iftot;
	for (off = firstifnet, ip = iftot; off;) {
		char *cp;

		kvm_read(off, (char *)&ifnet, sizeof ifnet);
		ip->ift_name[0] = '(';
		kvm_read((off_t)ifnet.if_name, ip->ift_name + 1, 15);
		if (interface && strcmp(ip->ift_name + 1, interface) == 0 &&
		    unit == ifnet.if_unit)
			interesting = ip;
		ip->ift_name[15] = '\0';
		cp = index(ip->ift_name, '\0');
		sprintf(cp, "%d)", ifnet.if_unit);
		ip++;
		if (ip >= iftot + MAXIF - 2)
			break;
		off = (off_t) ifnet.if_next;
	}
	lastif = ip;

	(void)signal(SIGALRM, catchalarm);
	signalled = NO;
	(void)alarm(interval);
banner:
	printf("   input    %-6.6s    output       ", interesting->ift_name);
	if (lastif - iftot > 0) {
		if (dflag)
			printf("      ");
		printf("     input   (Total)    output");
	}
	for (ip = iftot; ip < iftot + MAXIF; ip++) {
		ip->ift_ip = 0;
		ip->ift_ie = 0;
		ip->ift_op = 0;
		ip->ift_oe = 0;
		ip->ift_co = 0;
		ip->ift_dr = 0;
	}
	putchar('\n');
	printf("%8.8s %5.5s %8.8s %5.5s %5.5s ",
		"packets", "errs", "packets", "errs", "colls");
	if (dflag)
		printf("%5.5s ", "drops");
	if (lastif - iftot > 0)
		printf(" %8.8s %5.5s %8.8s %5.5s %5.5s",
			"packets", "errs", "packets", "errs", "colls");
	if (dflag)
		printf(" %5.5s", "drops");
	putchar('\n');
	fflush(stdout);
	line = 0;
loop:
	sum->ift_ip = 0;
	sum->ift_ie = 0;
	sum->ift_op = 0;
	sum->ift_oe = 0;
	sum->ift_co = 0;
	sum->ift_dr = 0;
	for (off = firstifnet, ip = iftot; off && ip < lastif; ip++) {
		kvm_read(off, (char *)&ifnet, sizeof ifnet);
		if (ip == interesting) {
			printf("%8d %5d %8d %5d %5d",
				ifnet.if_ipackets - ip->ift_ip,
				ifnet.if_ierrors - ip->ift_ie,
				ifnet.if_opackets - ip->ift_op,
				ifnet.if_oerrors - ip->ift_oe,
				ifnet.if_collisions - ip->ift_co);
			if (dflag)
				printf(" %5d",
				    ifnet.if_snd.ifq_drops - ip->ift_dr);
		}
		ip->ift_ip = ifnet.if_ipackets;
		ip->ift_ie = ifnet.if_ierrors;
		ip->ift_op = ifnet.if_opackets;
		ip->ift_oe = ifnet.if_oerrors;
		ip->ift_co = ifnet.if_collisions;
		ip->ift_dr = ifnet.if_snd.ifq_drops;
		sum->ift_ip += ip->ift_ip;
		sum->ift_ie += ip->ift_ie;
		sum->ift_op += ip->ift_op;
		sum->ift_oe += ip->ift_oe;
		sum->ift_co += ip->ift_co;
		sum->ift_dr += ip->ift_dr;
		off = (off_t) ifnet.if_next;
	}
	if (lastif - iftot > 0) {
		printf("  %8d %5d %8d %5d %5d",
			sum->ift_ip - total->ift_ip,
			sum->ift_ie - total->ift_ie,
			sum->ift_op - total->ift_op,
			sum->ift_oe - total->ift_oe,
			sum->ift_co - total->ift_co);
		if (dflag)
			printf(" %5d", sum->ift_dr - total->ift_dr);
	}
	*total = *sum;
	putchar('\n');
	fflush(stdout);
	line++;
	oldmask = sigblock(sigmask(SIGALRM));
	if (! signalled) {
		sigpause(0);
	}
	sigsetmask(oldmask);
	signalled = NO;
	(void)alarm(interval);
	if (line == 21)
		goto banner;
	goto loop;
	/*NOTREACHED*/
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
void
catchalarm()
{
	signalled = YES;
}
