/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-icmp.c,v 1.43 1999/11/22 04:28:21 fenner Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

/* rfc1700 */
#ifndef ICMP_UNREACH_NET_UNKNOWN
#define ICMP_UNREACH_NET_UNKNOWN	6	/* destination net unknown */
#endif
#ifndef ICMP_UNREACH_HOST_UNKNOWN
#define ICMP_UNREACH_HOST_UNKNOWN	7	/* destination host unknown */
#endif
#ifndef ICMP_UNREACH_ISOLATED
#define ICMP_UNREACH_ISOLATED		8	/* source host isolated */
#endif
#ifndef ICMP_UNREACH_NET_PROHIB
#define ICMP_UNREACH_NET_PROHIB		9	/* admin prohibited net */
#endif
#ifndef ICMP_UNREACH_HOST_PROHIB
#define ICMP_UNREACH_HOST_PROHIB	10	/* admin prohibited host */
#endif
#ifndef ICMP_UNREACH_TOSNET
#define ICMP_UNREACH_TOSNET		11	/* tos prohibited net */
#endif
#ifndef ICMP_UNREACH_TOSHOST
#define ICMP_UNREACH_TOSHOST		12	/* tos prohibited host */
#endif

/* rfc1716 */
#ifndef ICMP_UNREACH_FILTER_PROHIB
#define ICMP_UNREACH_FILTER_PROHIB	13	/* admin prohibited filter */
#endif
#ifndef ICMP_UNREACH_HOST_PRECEDENCE
#define ICMP_UNREACH_HOST_PRECEDENCE	14	/* host precedence violation */
#endif
#ifndef ICMP_UNREACH_PRECEDENCE_CUTOFF
#define ICMP_UNREACH_PRECEDENCE_CUTOFF	15	/* precedence cutoff */
#endif

/* rfc1256 */
#ifndef ICMP_ROUTERADVERT
#define ICMP_ROUTERADVERT		9	/* router advertisement */
#endif
#ifndef ICMP_ROUTERSOLICIT
#define ICMP_ROUTERSOLICIT		10	/* router solicitation */
#endif

/* Most of the icmp types */
static struct tok icmp2str[] = {
	{ ICMP_ECHOREPLY,		"echo reply" },
	{ ICMP_SOURCEQUENCH,		"source quench" },
	{ ICMP_ECHO,			"echo request" },
	{ ICMP_ROUTERSOLICIT,		"router solicitation" },
	{ ICMP_TSTAMP,			"time stamp request" },
	{ ICMP_TSTAMPREPLY,		"time stamp reply" },
	{ ICMP_IREQ,			"information request" },
	{ ICMP_IREQREPLY,		"information reply" },
	{ ICMP_MASKREQ,			"address mask request" },
	{ 0,				NULL }
};

/* Formats for most of the ICMP_UNREACH codes */
static struct tok unreach2str[] = {
	{ ICMP_UNREACH_NET,		"net %s unreachable" },
	{ ICMP_UNREACH_HOST,		"host %s unreachable" },
	{ ICMP_UNREACH_SRCFAIL,
	    "%s unreachable - source route failed" },
	{ ICMP_UNREACH_NET_UNKNOWN,	"net %s unreachable - unknown" },
	{ ICMP_UNREACH_HOST_UNKNOWN,	"host %s unreachable - unknown" },
	{ ICMP_UNREACH_ISOLATED,
	    "%s unreachable - source host isolated" },
	{ ICMP_UNREACH_NET_PROHIB,
	    "net %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_HOST_PROHIB,
	    "host %s unreachable - admin prohibited" },
	{ ICMP_UNREACH_TOSNET,
	    "net %s unreachable - tos prohibited" },
	{ ICMP_UNREACH_TOSHOST,
	    "host %s unreachable - tos prohibited" },
	{ ICMP_UNREACH_FILTER_PROHIB,
	    "host %s unreachable - admin prohibited filter" },
	{ ICMP_UNREACH_HOST_PRECEDENCE,
	    "host %s unreachable - host precedence violation" },
	{ ICMP_UNREACH_PRECEDENCE_CUTOFF,
	    "host %s unreachable - precedence cutoff" },
	{ 0,				NULL }
};

/* Formats for the ICMP_REDIRECT codes */
static struct tok type2str[] = {
	{ ICMP_REDIRECT_NET,		"redirect %s to net %s" },
	{ ICMP_REDIRECT_HOST,		"redirect %s to host %s" },
	{ ICMP_REDIRECT_TOSNET,		"redirect-tos %s to net %s" },
	{ ICMP_REDIRECT_TOSHOST,	"redirect-tos %s to net %s" },
	{ 0,				NULL }
};

/* rfc1191 */
struct mtu_discovery {
	short unused;
	short nexthopmtu;
};

/* rfc1256 */
struct ih_rdiscovery {
	u_char ird_addrnum;
	u_char ird_addrsiz;
	u_short ird_lifetime;
};

struct id_rdiscovery {
	u_int32_t ird_addr;
	u_int32_t ird_pref;
};

void
icmp_print(register const u_char *bp, u_int plen, register const u_char *bp2)
{
	register char *cp;
	register const struct icmp *dp;
	register const struct ip *ip;
	register const char *str, *fmt;
	register const struct ip *oip;
	register const struct udphdr *ouh;
	register u_int hlen, dport, mtu;
	char buf[MAXHOSTNAMELEN + 100];

	dp = (struct icmp *)bp;
	ip = (struct ip *)bp2;
	str = buf;

#if 0
        (void)printf("%s > %s: ",
		ipaddr_string(&ip->ip_src),
		ipaddr_string(&ip->ip_dst));
#endif

	TCHECK(dp->icmp_code);
	switch (dp->icmp_type) {

	case ICMP_UNREACH:
		TCHECK(dp->icmp_ip.ip_dst);
		switch (dp->icmp_code) {

		case ICMP_UNREACH_PROTOCOL:
			TCHECK(dp->icmp_ip.ip_p);
			(void)snprintf(buf, sizeof(buf), "%s protocol %d unreachable",
				       ipaddr_string(&dp->icmp_ip.ip_dst),
				       dp->icmp_ip.ip_p);
			break;

		case ICMP_UNREACH_PORT:
			TCHECK(dp->icmp_ip.ip_p);
			oip = &dp->icmp_ip;
			hlen = oip->ip_hl * 4;
			ouh = (struct udphdr *)(((u_char *)oip) + hlen);
			dport = ntohs(ouh->uh_dport);
			switch (oip->ip_p) {

			case IPPROTO_TCP:
				(void)snprintf(buf, sizeof(buf),
					"%s tcp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					tcpport_string(dport));
				break;

			case IPPROTO_UDP:
				(void)snprintf(buf, sizeof(buf),
					"%s udp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					udpport_string(dport));
				break;

			default:
				(void)snprintf(buf, sizeof(buf),
					"%s protocol %d port %d unreachable",
					ipaddr_string(&oip->ip_dst),
					oip->ip_p, dport);
				break;
			}
			break;

		case ICMP_UNREACH_NEEDFRAG:
			{
			register const struct mtu_discovery *mp;

			mp = (struct mtu_discovery *)&dp->icmp_void;
                        mtu = EXTRACT_16BITS(&mp->nexthopmtu);
                        if (mtu)
			    (void)snprintf(buf, sizeof(buf),
				"%s unreachable - need to frag (mtu %d)",
				ipaddr_string(&dp->icmp_ip.ip_dst), mtu);
                        else
			    (void)snprintf(buf, sizeof(buf),
				"%s unreachable - need to frag",
				ipaddr_string(&dp->icmp_ip.ip_dst));
			}
			break;

		default:
			fmt = tok2str(unreach2str, "#%d %%s unreachable",
			    dp->icmp_code);
			(void)snprintf(buf, sizeof(buf), fmt,
			    ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		}
		break;

	case ICMP_REDIRECT:
		TCHECK(dp->icmp_ip.ip_dst);
		fmt = tok2str(type2str, "redirect-#%d %%s to net %%s",
		    dp->icmp_code);
		(void)snprintf(buf, sizeof(buf), fmt,
		    ipaddr_string(&dp->icmp_ip.ip_dst),
		    ipaddr_string(&dp->icmp_gwaddr));
		break;

	case ICMP_ROUTERADVERT:
		{
		register const struct ih_rdiscovery *ihp;
		register const struct id_rdiscovery *idp;
		u_int lifetime, num, size;

		(void)strcpy(buf, "router advertisement");
		cp = buf + strlen(buf);

		ihp = (struct ih_rdiscovery *)&dp->icmp_void;
		TCHECK(*ihp);
		(void)strcpy(cp, " lifetime ");
		cp = buf + strlen(buf);
		lifetime = EXTRACT_16BITS(&ihp->ird_lifetime);
		if (lifetime < 60)
			(void)snprintf(cp, sizeof(buf) - strlen(buf), "%u", lifetime);
		else if (lifetime < 60 * 60)
			(void)snprintf(cp, sizeof(buf) - strlen(buf), "%u:%02u",
			    lifetime / 60, lifetime % 60);
		else
			(void)snprintf(cp, sizeof(buf) - strlen(buf), "%u:%02u:%02u",
			    lifetime / 3600,
			    (lifetime % 3600) / 60,
			    lifetime % 60);
		cp = buf + strlen(buf);

		num = ihp->ird_addrnum;
		(void)snprintf(cp, sizeof(buf) - strlen(buf), " %d:", num);
		cp = buf + strlen(buf);

		size = ihp->ird_addrsiz;
		if (size != 2) {
			(void)snprintf(cp, sizeof(buf) - strlen(buf), " [size %d]", size);
			break;
		}
		idp = (struct id_rdiscovery *)&dp->icmp_data;
		while (num-- > 0) {
			TCHECK(*idp);
			(void)snprintf(cp, sizeof(buf) - strlen(buf), " {%s %u}",
			    ipaddr_string(&idp->ird_addr),
			    EXTRACT_32BITS(&idp->ird_pref));
			cp = buf + strlen(buf);
		}
		}
		break;

	case ICMP_TIMXCEED:
		TCHECK(dp->icmp_ip.ip_dst);
		switch (dp->icmp_code) {

		case ICMP_TIMXCEED_INTRANS:
			str = "time exceeded in-transit";
			break;

		case ICMP_TIMXCEED_REASS:
			str = "ip reassembly time exceeded";
			break;

		default:
			(void)snprintf(buf, sizeof(buf), "time exceeded-#%d", dp->icmp_code);
			break;
		}
		break;

	case ICMP_PARAMPROB:
		if (dp->icmp_code)
			(void)snprintf(buf, sizeof(buf), "parameter problem - code %d",
					dp->icmp_code);
		else {
			TCHECK(dp->icmp_pptr);
			(void)snprintf(buf, sizeof(buf), "parameter problem - octet %d",
					dp->icmp_pptr);
		}
		break;

	case ICMP_MASKREPLY:
		TCHECK(dp->icmp_mask);
		(void)snprintf(buf, sizeof(buf), "address mask is 0x%08x",
		    (u_int32_t)ntohl(dp->icmp_mask));
		break;

	default:
		str = tok2str(icmp2str, "type-#%d", dp->icmp_type);
		break;
	}
        (void)printf("icmp: %s", str);
	if (vflag) {
		if (TTEST2(*bp, plen)) {
			if (in_cksum((u_short*)dp, plen, 0))
				printf(" (wrong icmp csum)");
		}
	}
 	if (vflag > 1 && !ICMP_INFOTYPE(dp->icmp_type)) {
 		bp += 8;
 		(void)printf(" for ");
 		ip = (struct ip *)bp;
 		snaplen = snapend - bp;
 		ip_print(bp, ntohs(ip->ip_len));
 	}
	return;
trunc:
	fputs("[|icmp]", stdout);
}
