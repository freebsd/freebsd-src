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
 */

#ifndef lint
/* From: Header: print-ip.c,v 1.28 92/05/25 14:29:02 mccanne Exp $ (LBL) */
static char rcsid[] =
    "$Id: print-ip.c,v 1.5 1995/06/13 17:39:23 wollman Exp $";
#endif

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
#include <netinet/tcpip.h>

#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <unistd.h>

#include "interface.h"
#include "addrtoname.h"

static void
igmp_print(register const u_char *bp, register int len,
	   register const u_char *bp2)
{
	register const struct ip *ip;
	register const u_char *ep;

	ip = (const struct ip *)bp2;
	ep = (const u_char *)snapend;
        (void)printf("%s > %s: ",
		ipaddr_string(&ip->ip_src),
		ipaddr_string(&ip->ip_dst));

	if (bp + 7 > ep) {
		(void)printf("[|igmp]");
		return;
	}
	switch (bp[0] & 0xf) {
	case 1:
		(void)printf("igmp %squery", bp[1] ? "new " : "");
		if (bp[1] != 100)
			printf(" [intvl %d]", bp[1]);
		if (*(int *)&bp[4])
			(void)printf(" [gaddr %s]", ipaddr_string(&bp[4]));
		if (len != 8)
			(void)printf(" [len %d]", len);
		break;
	case 2:
	case 6:
		(void)printf("igmp %sreport %s", 
			     (bp[0] & 0xf) == 6 ? "new " : "",
			     ipaddr_string(&bp[4]));
		if (len != 8)
			(void)printf(" [len %d]", len);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	case 7:
		(void)printf("igmp leave %s", ipaddr_string(&bp[4]));
		if (len != 8)
			(void)printf(" [len %d]", len);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	case 3:
		(void)printf("igmp dvmrp");
		switch(bp[1]) {
		case 1:
			printf(" probe");
			if (len < 8)
				(void)printf(" [len %d]", len);
			if (len > 12) {
				int i;
				for (i = 12; i + 3 < len; i += 4) {
					printf("\n\t%s", 
					       ipaddr_string(&bp[i]));
				}
			}
			break;
		case 2:
			printf(" report");
			if (len < 8)
				(void)printf(" [len %d]", len);
			break;
		case 3:
		case 5:
			printf(" %sneighbor query", bp[1] == 5 ? "new " : "");
			if (len < 8)
				(void)printf(" [len %d]", len);
			break;
		case 4:
		case 6:
			printf(" %sneighbor list", bp[1] == 6 ? "new " : "");
			if (len < 8)
				(void)printf(" [len %d]", len);
			break;
		case 7:
			printf(" prune %s from ", ipaddr_string(&bp[12]));
			printf(" %s timer %d", ipaddr_string(&bp[8]),
			       ntohl(*(int *)&bp[16]));
			if (len != 20)
				(void)printf(" [len %d]", len);
			break;
		case 8:
			printf(" graft %s from ", ipaddr_string(&bp[12]));
			printf(" %s", ipaddr_string(&bp[8]));
			if (len != 16)
				(void)printf(" [len %d]", len);
			break;
		case 9:
			printf(" graft ack %s from ", 
			       ipaddr_string(&bp[12]));
			printf(" %s", ipaddr_string(&bp[8]));

			if (len != 16)
				(void)printf(" [len %d]", len);
			break;
		default:
			printf("-%d", bp[1]);
			if (len < 8)
				(void)printf(" [len %d]", len);
			break;
		}

		if (bp[7] != 3 
		    || (bp[7] == 3 && (bp[6] > 5 || bp[6] < 4))) {
			printf(" [v%d.%d]", bp[7], bp[6]);
		}

		break;
	case 4:
		printf("igmp pim %s", ipaddr_string(&bp[4]));
		if (len < 8)
			(void)printf(" [len %d]", len);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	case 15:
		(void)printf("igmp mtrace %s", ipaddr_string(&bp[4]));
		if (len < 8)
			(void)printf(" [len %d]", len);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	case 14:
		(void)printf("igmp mtrace-resp %s", ipaddr_string(&bp[4]));
		if (len < 8)
			(void)printf(" [len %d]", len);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	default:
		(void)printf("igmp-%d", bp[0] & 0xf);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	}
	if ((bp[0] >> 4) != 1)
		(void)printf(" [v%d]", bp[0] >> 4);
}

/*
 * print the recorded route in an IP RR, LSRR or SSRR option.
 */
static void
ip_printroute(const char *type, register const u_char *cp, int length)
{
	int ptr = cp[2] - 1;
	int len;

	printf(" %s{", type);
	if ((length + 1) & 3)
		printf(" [bad length %d]", length);
	if (ptr < 3 || ((ptr + 1) & 3) || ptr > length + 1)
		printf(" [bad ptr %d]", cp[2]);

	type = "";
	for (len = 3; len < length; len += 4) {
		if (ptr == len)
			type = "#";
#ifdef TCPDUMP_ALIGN
		{
		struct in_addr addr;
		bcopy((char *)&cp[len], (char *)&addr, sizeof(addr));
		printf("%s%s", type, ipaddr_string(&addr));
		}
#else
		printf("%s%s", type, ipaddr_string(&cp[len]));
#endif
		type = " ";
	}
	printf("%s}", ptr == len? "#" : "");
}

/*
 * print IP options.
 */
static void
ip_optprint(register const u_char *cp, int length)
{
	int len;

	for (; length > 0; cp += len, length -= len) {
		int tt = *cp;

		len = (tt == IPOPT_NOP || tt == IPOPT_EOL) ? 1 : cp[1];
		if (&cp[1] >= snapend || cp + len > snapend) {
			printf("[|ip]");
			return;
		}
		switch (tt) {

		case IPOPT_EOL:
			printf(" EOL");
			if (length > 1)
				printf("-%d", length - 1);
			return;

		case IPOPT_NOP:
			printf(" NOP");
			break;

		case IPOPT_TS:
			printf(" TS{%d}", len);
			break;

		case IPOPT_SECURITY:
			printf(" SECURITY{%d}", len);
			break;

		case IPOPT_RR:
			printf(" RR{%d}=", len);
			ip_printroute("RR", cp, len);
			break;

		case IPOPT_SSRR:
			ip_printroute("SSRR", cp, len);
			break;

		case IPOPT_LSRR:
			ip_printroute("LSRR", cp, len);
			break;

		default:
			printf(" IPOPT-%d{%d}", cp[0], len);
			break;
		}
	}
}

/*
 * compute an IP header checksum.
 * don't modifiy the packet.
 */
static int
in_cksum(const struct ip *ip)
{
	register const u_short *sp = (u_short *)ip;
	register u_int32 sum = 0;
	register int count;

	/*
	 * No need for endian conversions.
	 */
	for (count = ip->ip_hl * 2; --count >= 0; )
		sum += *sp++;
	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);
	sum = ~sum & 0xffff;

	return (sum);
}

/*
 * print an IP datagram.
 */
void
ip_print(register const u_char *bp, register int length)
{
	register const struct ip *ip;
	register int hlen;
	register int len;
	register int off;
	register const u_char *cp;

	ip = (const struct ip *)bp;
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
		ip = (struct ip *)abuf;
	}
#endif
	if ((u_char *)(ip + 1) > snapend) {
		printf("[|ip]");
		return;
	}
	if (length < sizeof (struct ip)) {
		(void)printf("truncated-ip %d", length);
		return;
	}
	hlen = ip->ip_hl * 4;

	len = ntohs(ip->ip_len);
	if (length < len)
		(void)printf("truncated-ip - %d bytes missing!",
			len - length);
	len -= hlen;

	/*
	 * If this is fragment zero, hand it to the next higher
	 * level protocol.
	 */
	off = ntohs(ip->ip_off);
	if ((off & 0x1fff) == 0) {
		cp = (const u_char *)ip + hlen;
		switch (ip->ip_p) {

		case IPPROTO_TCP:
			tcp_print(cp, len, (const u_char *)ip);
			break;
		case IPPROTO_UDP:
			udp_print(cp, len, (const u_char *)ip);
			break;
		case IPPROTO_ICMP:
			icmp_print(cp, (const u_char *)ip);
			break;
		case IPPROTO_ND:
			(void)printf("%s > %s:", ipaddr_string(&ip->ip_src),
				ipaddr_string(&ip->ip_dst));
			(void)printf(" nd %d", len);
			break;
		case IPPROTO_EGP:
			egp_print(cp, len, (const u_char *)ip);
			break;
#ifndef IPPROTO_OSPF
#define IPPROTO_OSPF 89
#endif
		case IPPROTO_OSPF:
			ospf_print(cp, len, (const u_char *)ip);
			break;
#ifndef IPPROTO_IGMP
#define IPPROTO_IGMP 2
#endif
		case IPPROTO_IGMP:
			igmp_print(cp, len, (const u_char *)ip);
			break;
#ifndef IPPROTO_ENCAP
#define IPPROTO_ENCAP 4
#endif
		case IPPROTO_ENCAP:
			/* ip-in-ip encapsulation */
			if (vflag)
				(void)printf("%s > %s: ",
					     ipaddr_string(&ip->ip_src),
					     ipaddr_string(&ip->ip_dst));
			ip_print(cp, len);
			if (! vflag) {
				printf(" (encap)");
				return;
			}
			break;
		default:
			(void)printf("%s > %s:", ipaddr_string(&ip->ip_src),
				ipaddr_string(&ip->ip_dst));
			(void)printf(" ip-proto-%d %d", ip->ip_p, len);
			break;
		}
	}
	/*
	 * for fragmented datagrams, print id:size@offset.  On all
	 * but the last stick a "+".  For unfragmented datagrams, note
	 * the don't fragment flag.
	 */
	if (off & 0x3fff) {
		/*
		 * if this isn't the first frag, we're missing the
		 * next level protocol header.  print the ip addr.
		 */
		if (off & 0x1fff)
			(void)printf("%s > %s:", ipaddr_string(&ip->ip_src),
				      ipaddr_string(&ip->ip_dst));
		(void)printf(" (frag %d:%d@%d%s)", ntohs(ip->ip_id), len,
			(off & 0x1fff) * 8,
			(off & IP_MF)? "+" : "");
	} else if (off & IP_DF)
		(void)printf(" (DF)");

	if (ip->ip_tos)
		(void)printf(" [tos 0x%x]", (int)ip->ip_tos);
	if (ip->ip_ttl <= 1)
		(void)printf(" [ttl %d]", (int)ip->ip_ttl);

	if (vflag) {
		int sum;
		char *sep = "";

		printf(" (");
		if (ip->ip_ttl > 1) {
			(void)printf("%sttl %d", sep, (int)ip->ip_ttl);
			sep = ", ";
		}
		if ((off & 0x3fff) == 0) {
			(void)printf("%sid %d", sep, (int)ntohs(ip->ip_id));
			sep = ", ";
		}
		sum = in_cksum(ip);
		if (sum != 0) {
			(void)printf("%sbad cksum %x!", sep,
				     ntohs(ip->ip_sum));
			sep = ", ";
		}
		if ((hlen -= sizeof(struct ip)) > 0) {
			(void)printf("%soptlen=%d", sep, hlen);
			ip_optprint((u_char *)(ip + 1), hlen);
		}
		printf(")");
	}
}
