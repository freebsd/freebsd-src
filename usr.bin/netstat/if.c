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

#if 0
#ifndef lint
static char sccsid[] = "@(#)if.c	8.3 (Berkeley) 4/28/95";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <net/pfvar.h>
#include <net/if_pfsync.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <libutil.h>
#ifdef INET6
#include <netdb.h>
#endif
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "netstat.h"

#define	YES	1
#define	NO	0

static void sidewaysintpr(int, u_long);
static void catchalarm(int);

#ifdef INET6
static char addr_buf[NI_MAXHOST];		/* for getnameinfo() */
#endif

/*
 * Dump pfsync statistics structure.
 */
void
pfsync_stats(u_long off, const char *name, int af1 __unused, int proto __unused)
{
	struct pfsyncstats pfsyncstat, zerostat;
	size_t len = sizeof(struct pfsyncstats);

	if (live) {
		if (zflag)
			memset(&zerostat, 0, len);
		if (sysctlbyname("net.inet.pfsync.stats", &pfsyncstat, &len,
		    zflag ? &zerostat : NULL, zflag ? len : 0) < 0) {
			if (errno != ENOENT)
				warn("sysctl: net.inet.pfsync.stats");
			return;
		}
	} else
		kread(off, &pfsyncstat, len);

	printf("%s:\n", name);

#define	p(f, m) if (pfsyncstat.f || sflag <= 1) \
	printf(m, (uintmax_t)pfsyncstat.f, plural(pfsyncstat.f))
#define	p2(f, m) if (pfsyncstat.f || sflag <= 1) \
	printf(m, (uintmax_t)pfsyncstat.f)

	p(pfsyncs_ipackets, "\t%ju packet%s received (IPv4)\n");
	p(pfsyncs_ipackets6, "\t%ju packet%s received (IPv6)\n");
	p(pfsyncs_badif, "\t\t%ju packet%s discarded for bad interface\n");
	p(pfsyncs_badttl, "\t\t%ju packet%s discarded for bad ttl\n");
	p(pfsyncs_hdrops, "\t\t%ju packet%s shorter than header\n");
	p(pfsyncs_badver, "\t\t%ju packet%s discarded for bad version\n");
	p(pfsyncs_badauth, "\t\t%ju packet%s discarded for bad HMAC\n");
	p(pfsyncs_badact,"\t\t%ju packet%s discarded for bad action\n");
	p(pfsyncs_badlen, "\t\t%ju packet%s discarded for short packet\n");
	p(pfsyncs_badval, "\t\t%ju state%s discarded for bad values\n");
	p(pfsyncs_stale, "\t\t%ju stale state%s\n");
	p(pfsyncs_badstate, "\t\t%ju failed state lookup/insert%s\n");
	p(pfsyncs_opackets, "\t%ju packet%s sent (IPv4)\n");
	p(pfsyncs_opackets6, "\t%ju packet%s sent (IPv6)\n");
	p2(pfsyncs_onomem, "\t\t%ju send failed due to mbuf memory error\n");
	p2(pfsyncs_oerrors, "\t\t%ju send error\n");
#undef p
#undef p2
}

/*
 * Display a formatted value, or a '-' in the same space.
 */
static void
show_stat(const char *fmt, int width, u_long value, short showvalue)
{
	const char *lsep, *rsep;
	char newfmt[32];

	lsep = "";
	if (strncmp(fmt, "LS", 2) == 0) {
		lsep = " ";
		fmt += 2;
	}
	rsep = " ";
	if (strncmp(fmt, "NRS", 3) == 0) {
		rsep = "";
		fmt += 3;
	}
	if (showvalue == 0) {
		/* Print just dash. */
		sprintf(newfmt, "%s%%%ds%s", lsep, width, rsep);
		printf(newfmt, "-");
		return;
	}

	if (hflag) {
		char buf[5];

		/* Format in human readable form. */
		humanize_number(buf, sizeof(buf), (int64_t)value, "",
		    HN_AUTOSCALE, HN_NOSPACE | HN_DECIMAL);
		sprintf(newfmt, "%s%%%ds%s", lsep, width, rsep);
		printf(newfmt, buf);
	} else {
		/* Construct the format string. */
		sprintf(newfmt, "%s%%%d%s%s", lsep, width, fmt, rsep);
		printf(newfmt, value);
	}
}

/*
 * Print a description of the network interfaces.
 */
void
intpr(int interval1, u_long ifnetaddr, void (*pfunc)(char *))
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
	} ifaddr;
	u_long ifaddraddr;
	u_long ifaddrfound;
	u_long ifnetfound;
	u_long opackets;
	u_long ipackets;
	u_long obytes;
	u_long ibytes;
	u_long omcasts;
	u_long imcasts;
	u_long oerrors;
	u_long ierrors;
	u_long idrops;
	u_long collisions;
	short timer;
	int drops;
	struct sockaddr *sa = NULL;
	char name[IFNAMSIZ];
	short network_layer;
	short link_layer;

	if (ifnetaddr == 0) {
		printf("ifnet: symbol not defined\n");
		return;
	}
	if (interval1) {
		sidewaysintpr(interval1, ifnetaddr);
		return;
	}
	if (kread(ifnetaddr, (char *)&ifnethead, sizeof ifnethead) != 0)
		return;
	ifnetaddr = (u_long)TAILQ_FIRST(&ifnethead);
	if (kread(ifnetaddr, (char *)&ifnet, sizeof ifnet) != 0)
		return;

	if (!pfunc) {
		if (Wflag)
			printf("%-7.7s", "Name");
		else
			printf("%-5.5s", "Name");
		printf(" %5.5s %-13.13s %-17.17s %8.8s %5.5s %5.5s",
		    "Mtu", "Network", "Address", "Ipkts", "Ierrs", "Idrop");
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
		struct sockaddr_in *sockin;
#ifdef INET6
		struct sockaddr_in6 *sockin6;
#endif
		char *cp;
		int n, m;

		network_layer = 0;
		link_layer = 0;

		if (ifaddraddr == 0) {
			ifnetfound = ifnetaddr;
			if (kread(ifnetaddr, (char *)&ifnet, sizeof ifnet) != 0)
				return;
			strlcpy(name, ifnet.if_xname, sizeof(name));
			ifnetaddr = (u_long)TAILQ_NEXT(&ifnet, if_link);
			if (interface != 0 && strcmp(name, interface) != 0)
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
		ifaddrfound = ifaddraddr;

		/*
		 * Get the interface stats.  These may get
		 * overriden below on a per-interface basis.
		 */
		opackets = ifnet.if_opackets;
		ipackets = ifnet.if_ipackets;
		obytes = ifnet.if_obytes;
		ibytes = ifnet.if_ibytes;
		omcasts = ifnet.if_omcasts;
		imcasts = ifnet.if_imcasts;
		oerrors = ifnet.if_oerrors;
		ierrors = ifnet.if_ierrors;
		idrops = ifnet.if_iqdrops;
		collisions = ifnet.if_collisions;
		timer = ifnet.if_timer;
		drops = ifnet.if_snd.ifq_drops;

		if (ifaddraddr == 0) {
			if (Wflag)
				printf("%-7.7s", name);
			else
				printf("%-5.5s", name);
			printf(" %5lu ", ifnet.if_mtu);
			printf("%-13.13s ", "none");
			printf("%-17.17s ", "none");
		} else {
			if (kread(ifaddraddr, (char *)&ifaddr, sizeof ifaddr)
			    != 0) {
				ifaddraddr = 0;
				continue;
			}
#define	CP(x) ((char *)(x))
			cp = (CP(ifaddr.ifa.ifa_addr) - CP(ifaddraddr)) +
				CP(&ifaddr);
			sa = (struct sockaddr *)cp;
			if (af != AF_UNSPEC && sa->sa_family != af) {
				ifaddraddr =
				    (u_long)TAILQ_NEXT(&ifaddr.ifa, ifa_link);
				continue;
			}
			if (Wflag)
				printf("%-7.7s", name);
			else
				printf("%-5.5s", name);
			printf(" %5lu ", ifnet.if_mtu);
			switch (sa->sa_family) {
			case AF_UNSPEC:
				printf("%-13.13s ", "none");
				printf("%-15.15s ", "none");
				break;
			case AF_INET:
				sockin = (struct sockaddr_in *)sa;
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
				printf("%-17.17s ",
				    routename(sockin->sin_addr.s_addr));

				network_layer = 1;
				break;
#ifdef INET6
			case AF_INET6:
				sockin6 = (struct sockaddr_in6 *)sa;
				in6_fillscopeid(&ifaddr.in6.ia_addr);
				printf("%-13.13s ",
				       netname6(&ifaddr.in6.ia_addr,
						&ifaddr.in6.ia_prefixmask.sin6_addr));
				in6_fillscopeid(sockin6);
				getnameinfo(sa, sa->sa_len, addr_buf,
				    sizeof(addr_buf), 0, 0, NI_NUMERICHOST);
				printf("%-17.17s ", addr_buf);

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
				printf("%-17s ",
				    ipx_phost((struct sockaddr *)sipx));
				}

				network_layer = 1;
				break;

			case AF_APPLETALK:
				printf("atalk:%-12.12s ",atalk_print(sa,0x10) );
				printf("%-11.11s  ",atalk_print(sa,0x0b) );
				break;
			case AF_LINK:
				{
				struct sockaddr_dl *sdl =
					(struct sockaddr_dl *)sa;
				char linknum[10];
				cp = (char *)LLADDR(sdl);
				n = sdl->sdl_alen;
				sprintf(linknum, "<Link#%d>", sdl->sdl_index);
				m = printf("%-13.13s ", linknum);
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
				m = 32 - m;
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
		show_stat("lu", 5, ierrors, link_layer);
		show_stat("lu", 5, idrops, link_layer);
		if (bflag)
			show_stat("lu", 10, ibytes, link_layer|network_layer);

		show_stat("lu", 8, opackets, link_layer|network_layer);
		show_stat("lu", 5, oerrors, link_layer);
		if (bflag)
			show_stat("lu", 10, obytes, link_layer|network_layer);

		show_stat("NRSlu", 5, collisions, link_layer);
		if (tflag)
			show_stat("LSd", 4, timer, link_layer);
		if (dflag)
			show_stat("LSd", 4, drops, link_layer);
		putchar('\n');

		if (aflag && ifaddrfound) {
			/*
			 * Print family's multicast addresses
			 */
			struct ifmultiaddr *multiaddr;
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

			TAILQ_FOREACH(multiaddr, &ifnet.if_multiaddrs, ifma_link) {
				if (kread((u_long)multiaddr, (char *)&ifma,
					  sizeof ifma) != 0)
					break;
				multiaddr = &ifma;
				if (kread((u_long)ifma.ifma_addr, (char *)&msa,
					  sizeof msa) != 0)
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
					in6_fillscopeid(&msa.in6);
					getnameinfo(&msa.sa, msa.sa.sa_len,
					    addr_buf, sizeof(addr_buf), 0, 0,
					    NI_NUMERICHOST);
					printf("%*s %-19.19s(refs: %d)\n",
					       Wflag ? 27 : 25, "",
					       addr_buf, ifma.ifma_refcount);
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
				if (fmt) {
					printf("%*s %-17.17s",
					    Wflag ? 27 : 25, "", fmt);
					if (msa.sa.sa_family == AF_LINK) {
						printf(" %8lu", imcasts);
						printf("%*s",
						    bflag ? 17 : 6, "");
						printf(" %8lu", omcasts);
					}
					putchar('\n');
				}
			}
		}
	}
}

struct	iftot {
	SLIST_ENTRY(iftot) chain;
	char	ift_name[IFNAMSIZ];	/* interface name */
	u_long	ift_ip;			/* input packets */
	u_long	ift_ie;			/* input errors */
	u_long	ift_id;			/* input drops */
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
 * Repeat display every interval1 seconds, showing statistics
 * collected over that interval.  Assumes that interval1 is non-zero.
 * First line printed at top of screen is always cumulative.
 * XXX - should be rewritten to use ifmib(4).
 */
static void
sidewaysintpr(int interval1, u_long off)
{
	struct ifnet ifnet;
	u_long firstifnet;
	struct ifnethead ifnethead;
	struct itimerval interval_it;
	struct iftot *iftot, *ip, *ipn, *total, *sum, *interesting;
	int line;
	int oldmask, first;
	u_long interesting_off;

	if (kread(off, (char *)&ifnethead, sizeof ifnethead) != 0)
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
		char name[IFNAMSIZ];

		if (kread(off, (char *)&ifnet, sizeof ifnet) != 0)
			break;
		strlcpy(name, ifnet.if_xname, sizeof(name));
		if (interface && strcmp(name, interface) == 0) {
			interesting = ip;
			interesting_off = off;
		}
		snprintf(ip->ift_name, sizeof(ip->ift_name), "(%s)", name);;
		if ((ipn = malloc(sizeof(struct iftot))) == NULL) {
			printf("malloc failed\n");
			exit(1);
		}
		memset(ipn, 0, sizeof(struct iftot));
		SLIST_NEXT(ip, chain) = ipn;
		ip = ipn;
		off = (u_long)TAILQ_NEXT(&ifnet, if_link);
	}
	if (interface && interesting == NULL)
		errx(1, "%s: unknown interface", interface);
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
	interval_it.it_interval.tv_sec = interval1;
	interval_it.it_interval.tv_usec = 0;
	interval_it.it_value = interval_it.it_interval;
	setitimer(ITIMER_REAL, &interval_it, NULL);
	first = 1;
banner:
	printf("%17s %14s %16s", "input",
	    interesting ? interesting->ift_name : "(Total)", "output");
	putchar('\n');
	printf("%10s %5s %5s %10s %10s %5s %10s %5s",
	    "packets", "errs", "idrops", "bytes", "packets", "errs", "bytes",
	    "colls");
	if (dflag)
		printf(" %5.5s", "drops");
	putchar('\n');
	fflush(stdout);
	line = 0;
loop:
	if (interesting != NULL) {
		ip = interesting;
		if (kread(interesting_off, (char *)&ifnet, sizeof ifnet) != 0) {
			printf("???\n");
			exit(1);
		};
		if (!first) {
			show_stat("lu", 10, ifnet.if_ipackets - ip->ift_ip, 1);
			show_stat("lu", 5, ifnet.if_ierrors - ip->ift_ie, 1);
			show_stat("lu", 5, ifnet.if_iqdrops - ip->ift_id, 1);
			show_stat("lu", 10, ifnet.if_ibytes - ip->ift_ib, 1);
			show_stat("lu", 10, ifnet.if_opackets - ip->ift_op, 1);
			show_stat("lu", 5, ifnet.if_oerrors - ip->ift_oe, 1);
			show_stat("lu", 10, ifnet.if_obytes - ip->ift_ob, 1);
			show_stat("NRSlu", 5,
			    ifnet.if_collisions - ip->ift_co, 1);
			if (dflag)
				show_stat("LSu", 5,
				    ifnet.if_snd.ifq_drops - ip->ift_dr, 1);
		}
		ip->ift_ip = ifnet.if_ipackets;
		ip->ift_ie = ifnet.if_ierrors;
		ip->ift_id = ifnet.if_iqdrops;
		ip->ift_ib = ifnet.if_ibytes;
		ip->ift_op = ifnet.if_opackets;
		ip->ift_oe = ifnet.if_oerrors;
		ip->ift_ob = ifnet.if_obytes;
		ip->ift_co = ifnet.if_collisions;
		ip->ift_dr = ifnet.if_snd.ifq_drops;
	} else {
		sum->ift_ip = 0;
		sum->ift_ie = 0;
		sum->ift_id = 0;
		sum->ift_ib = 0;
		sum->ift_op = 0;
		sum->ift_oe = 0;
		sum->ift_ob = 0;
		sum->ift_co = 0;
		sum->ift_dr = 0;
		for (off = firstifnet, ip = iftot;
		     off && SLIST_NEXT(ip, chain) != NULL;
		     ip = SLIST_NEXT(ip, chain)) {
			if (kread(off, (char *)&ifnet, sizeof ifnet) != 0) {
				off = 0;
				continue;
			}
			sum->ift_ip += ifnet.if_ipackets;
			sum->ift_ie += ifnet.if_ierrors;
			sum->ift_id += ifnet.if_iqdrops;
			sum->ift_ib += ifnet.if_ibytes;
			sum->ift_op += ifnet.if_opackets;
			sum->ift_oe += ifnet.if_oerrors;
			sum->ift_ob += ifnet.if_obytes;
			sum->ift_co += ifnet.if_collisions;
			sum->ift_dr += ifnet.if_snd.ifq_drops;
			off = (u_long)TAILQ_NEXT(&ifnet, if_link);
		}
		if (!first) {
			show_stat("lu", 10, sum->ift_ip - total->ift_ip, 1);
			show_stat("lu", 5, sum->ift_ie - total->ift_ie, 1);
			show_stat("lu", 5, sum->ift_id - total->ift_id, 1);
			show_stat("lu", 10, sum->ift_ib - total->ift_ib, 1);
			show_stat("lu", 10, sum->ift_op - total->ift_op, 1);
			show_stat("lu", 5, sum->ift_oe - total->ift_oe, 1);
			show_stat("lu", 10, sum->ift_ob - total->ift_ob, 1);
			show_stat("NRSlu", 5, sum->ift_co - total->ift_co, 1);
			if (dflag)
				show_stat("LSu", 5,
				    sum->ift_dr - total->ift_dr, 1);
		}
		*total = *sum;
	}
	if (!first)
		putchar('\n');
	fflush(stdout);
	if ((noutputs != 0) && (--noutputs == 0))
		exit(0);
	oldmask = sigblock(sigmask(SIGALRM));
	while (!signalled)
		sigpause(0);
	signalled = NO;
	sigsetmask(oldmask);
	line++;
	first = 0;
	if (line == 21)
		goto banner;
	else
		goto loop;
	/*NOTREACHED*/
}

/*
 * Set a flag to indicate that a signal from the periodic itimer has been
 * caught.
 */
static void
catchalarm(int signo __unused)
{
	signalled = YES;
}
