/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994
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
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-icmp.c,v 1.2 1995/03/08 12:52:32 olah Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

void
icmp_print(register const u_char *bp, register const u_char *bp2)
{
	register const struct icmp *dp;
	register const struct ip *ip;
	register const char *str;
	register const struct ip *oip;
	register const struct udphdr *ouh;
	register int hlen, dport;
	register const u_char *ep;
	char buf[256];

#define TCHECK(var, l) if ((u_char *)&(var) > ep - l) goto trunc

	dp = (struct icmp *)bp;
	ip = (struct ip *)bp2;
	str = buf;
	/* 'ep' points to the end of avaible data. */
	ep = snapend;

        (void)printf("%s > %s: ",
		ipaddr_string(&ip->ip_src),
		ipaddr_string(&ip->ip_dst));

	TCHECK(dp->icmp_code, sizeof(dp->icmp_code));
	switch (dp->icmp_type) {
	case ICMP_ECHOREPLY:
		str = "echo reply";
		break;
	case ICMP_UNREACH:
		TCHECK(dp->icmp_ip.ip_dst, sizeof(dp->icmp_ip.ip_dst));
		switch (dp->icmp_code) {
		case ICMP_UNREACH_NET:
			(void)sprintf(buf, "net %s unreachable",
				       ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		case ICMP_UNREACH_HOST:
			(void)sprintf(buf, "host %s unreachable",
				       ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		case ICMP_UNREACH_PROTOCOL:
			TCHECK(dp->icmp_ip.ip_p, sizeof(dp->icmp_ip.ip_p));
			(void)sprintf(buf, "%s protocol %d unreachable",
				       ipaddr_string(&dp->icmp_ip.ip_dst),
				       dp->icmp_ip.ip_p);
			break;
		case ICMP_UNREACH_PORT:
			TCHECK(dp->icmp_ip.ip_p, sizeof(dp->icmp_ip.ip_p));
			oip = &dp->icmp_ip;
			hlen = oip->ip_hl * 4;
			ouh = (struct udphdr *)(((u_char *)oip) + hlen);
			dport = ntohs(ouh->uh_dport);
			switch (oip->ip_p) {
			case IPPROTO_TCP:
				(void)sprintf(buf,
					"%s tcp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					tcpport_string(dport));
				break;
			case IPPROTO_UDP:
				(void)sprintf(buf,
					"%s udp port %s unreachable",
					ipaddr_string(&oip->ip_dst),
					udpport_string(dport));
				break;
			default:
				(void)sprintf(buf,
					"%s protocol %d port %d unreachable",
					ipaddr_string(&oip->ip_dst),
					oip->ip_p, dport);
				break;
			}
			break;
		case ICMP_UNREACH_NEEDFRAG:
			(void)sprintf(buf, "%s unreachable - need to frag",
				       ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		case ICMP_UNREACH_SRCFAIL:
			(void)sprintf(buf,
				"%s unreachable - source route failed",
				ipaddr_string(&dp->icmp_ip.ip_dst));
			break;
		}
		break;
	case ICMP_SOURCEQUENCH:
		str = "source quench";
		break;
	case ICMP_REDIRECT:
		TCHECK(dp->icmp_ip.ip_dst, sizeof(dp->icmp_ip.ip_dst));
		switch (dp->icmp_code) {
		case ICMP_REDIRECT_NET:
			(void)sprintf(buf, "redirect %s to net %s",
				       ipaddr_string(&dp->icmp_ip.ip_dst),
				       ipaddr_string(&dp->icmp_gwaddr));
			break;
		case ICMP_REDIRECT_HOST:
			(void)sprintf(buf, "redirect %s to host %s",
				       ipaddr_string(&dp->icmp_ip.ip_dst),
				       ipaddr_string(&dp->icmp_gwaddr));
			break;
		case ICMP_REDIRECT_TOSNET:
			(void)sprintf(buf, "redirect-tos %s to net %s",
				       ipaddr_string(&dp->icmp_ip.ip_dst),
				       ipaddr_string(&dp->icmp_gwaddr));
			break;
		case ICMP_REDIRECT_TOSHOST:
			(void)sprintf(buf, "redirect-tos %s to host %s",
				       ipaddr_string(&dp->icmp_ip.ip_dst),
				       ipaddr_string(&dp->icmp_gwaddr));
			break;
		}
		break;
	case ICMP_ECHO:
		str = "echo request";
		break;
	case ICMP_TIMXCEED:
		TCHECK(dp->icmp_ip.ip_dst, sizeof(dp->icmp_ip.ip_dst));
		switch (dp->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			str = "time exceeded in-transit";
			break;
		case ICMP_TIMXCEED_REASS:
			str = "ip reassembly time exceeded";
			break;
		}
		break;
	case ICMP_PARAMPROB:
		if (dp->icmp_code)
			(void)sprintf(buf, "parameter problem - code %d",
					dp->icmp_code);
		else {
			TCHECK(dp->icmp_pptr, sizeof(dp->icmp_pptr));
			(void)sprintf(buf, "parameter problem - octet %d",
					dp->icmp_pptr);
		}
		break;
	case ICMP_TSTAMP:
		str = "time stamp request";
		break;
	case ICMP_TSTAMPREPLY:
		str = "time stamp reply";
		break;
	case ICMP_IREQ:
		str = "information request";
		break;
	case ICMP_IREQREPLY:
		str = "information reply";
		break;
	case ICMP_MASKREQ:
		str = "address mask request";
		break;
	case ICMP_MASKREPLY:
		TCHECK(dp->icmp_mask, sizeof(dp->icmp_mask));
		(void)sprintf(buf, "address mask is 0x%08x",
		    ntohl(dp->icmp_mask));
		break;
	default:
		(void)sprintf(buf, "type-#%d", dp->icmp_type);
		break;
	}
        (void)printf("icmp: %s", str);
	return;
trunc:
	fputs("[|icmp]", stdout);
#undef TCHECK
}
