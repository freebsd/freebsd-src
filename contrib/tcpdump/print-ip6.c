/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
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
 * $FreeBSD: src/contrib/tcpdump/print-ip6.c,v 1.2 2000/02/17 03:30:03 fenner Exp $
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-ip6.c,v 1.2.2.1 2000/01/11 06:58:25 fenner Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef INET6

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>

#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <unistd.h>

#include "interface.h"
#include "addrtoname.h"

#include <netinet/ip6.h>

/*
 * print an IP6 datagram.
 */
void
ip6_print(register const u_char *bp, register int length)
{
	register const struct ip6_hdr *ip6;
	register int hlen;
	register int len;
	register const u_char *cp;
	int nh;
	u_int flow;
	
	ip6 = (const struct ip6_hdr *)bp;

#ifdef TCPDUMP_ALIGN
	/*
	 * The IP header is not word aligned, so copy into abuf.
	 * This will never happen with BPF.  It does happen raw packet
	 * dumps from -r.
	 */
	if ((int)ip & (sizeof(long)-1)) {
		static u_char *abuf;

		if (abuf == 0)
			abuf = (u_char *)malloc(snaplen);
		bcopy((char *)ip, (char *)abuf, min(length, snaplen));
		snapend += abuf - (u_char *)ip;
		packetp = abuf;
		ip = (struct ip6_hdr *)abuf;
	}
#endif
	if ((u_char *)(ip6 + 1) > snapend) {
		printf("[|ip6]");
		return;
	}
	if (length < sizeof (struct ip6_hdr)) {
		(void)printf("truncated-ip6 %d", length);
		return;
	}
	hlen = sizeof(struct ip6_hdr);

	len = ntohs(ip6->ip6_plen);
	if (length < len + hlen)
		(void)printf("truncated-ip6 - %d bytes missing!",
			len + hlen - length);

	cp = (const u_char *)ip6;
	nh = ip6->ip6_nxt;
	while (cp < snapend) {
		cp += hlen;

		if (cp == (u_char *)(ip6 + 1)
		 && nh != IPPROTO_TCP && nh != IPPROTO_UDP) {
			(void)printf("%s > %s: ", ip6addr_string(&ip6->ip6_src),
				     ip6addr_string(&ip6->ip6_dst));
		}

		switch (nh) {
		case IPPROTO_HOPOPTS:
			hlen = hbhopt_print(cp);
			nh = *cp;
			break;
		case IPPROTO_DSTOPTS:
			hlen = dstopt_print(cp);
			nh = *cp;
			break;
		case IPPROTO_FRAGMENT:
			hlen = frag6_print(cp, (const u_char *)ip6);
			if (snapend <= cp + hlen)
				goto end;
			nh = *cp;
			break;
		case IPPROTO_ROUTING:
			hlen = rt6_print(cp, (const u_char *)ip6);
			nh = *cp;
			break;
		case IPPROTO_TCP:
			tcp_print(cp, len + sizeof(struct ip6_hdr) - (cp - bp),
				(const u_char *)ip6);
			goto end;
		case IPPROTO_UDP:
			udp_print(cp, len + sizeof(struct ip6_hdr) - (cp - bp),
				(const u_char *)ip6);
			goto end;
		case IPPROTO_ICMPV6:
			icmp6_print(cp, (const u_char *)ip6);
			goto end;
		case IPPROTO_AH:
			hlen = ah_print(cp, (const u_char *)ip6);
			nh = *cp;
			break;
		case IPPROTO_ESP:
		    {
			int enh;
			cp += esp_print(cp, (const u_char *)ip6, &enh);
			if (enh < 0)
				goto end;
			nh = enh & 0xff;
			break;
		    }
#ifndef IPPROTO_IPCOMP
#define IPPROTO_IPCOMP	108
#endif
		case IPPROTO_IPCOMP:
		    {
			int enh;
			cp += ipcomp_print(cp, (const u_char *)ip6, &enh);
			if (enh < 0)
				goto end;
			nh = enh & 0xff;
			break;
		    }
		case IPPROTO_PIM:
			pim_print(cp, len);
			goto end;
#ifndef IPPROTO_OSPF
#define IPPROTO_OSPF 89
#endif
		case IPPROTO_OSPF:
			ospf6_print(cp, len);
			goto end;
		case IPPROTO_IPV6:
			ip6_print(cp, len);
			goto end;
#ifndef IPPROTO_IPV4
#define IPPROTO_IPV4	4
#endif
		case IPPROTO_IPV4:
			ip_print(cp, len);
			goto end;
		case IPPROTO_NONE:
			(void)printf("no next header");
			goto end;

		default:
			(void)printf("ip-proto-%d %d", ip6->ip6_nxt, len);
			goto end;
		}
	}

 end:
	
	flow = ntohl(ip6->ip6_flow);
#if 0
	/* rfc1883 */
	if (flow & 0x0f000000)
		(void)printf(" [pri 0x%x]", (flow & 0x0f000000) >> 24);
	if (flow & 0x00ffffff)
		(void)printf(" [flowlabel 0x%x]", flow & 0x00ffffff);
#else
	/* RFC 2460 */
	if (flow & 0x0ff00000)
		(void)printf(" [class 0x%x]", (flow & 0x0ff00000) >> 20);
	if (flow & 0x000fffff)
		(void)printf(" [flowlabel 0x%x]", flow & 0x000fffff);
#endif

	if (ip6->ip6_hlim <= 1)
		(void)printf(" [hlim %d]", (int)ip6->ip6_hlim);

	if (vflag) {
		printf(" (");
		(void)printf("len %d", len);
		if (ip6->ip6_hlim > 1)
			(void)printf(", hlim %d", (int)ip6->ip6_hlim);
		printf(")");
	}
}

#endif /* INET6 */
