/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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
    "@(#) $Header: /tcpdump/master/tcpdump/print-ip.c,v 1.92 2001/01/02 23:00:01 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "addrtoname.h"
#include "interface.h"
#include "extract.h"			/* must come after interface.h */

#include "ip.h"

/* Compatibility */
#ifndef	IPPROTO_ND
#define	IPPROTO_ND	77
#endif

/*
 * print the recorded route in an IP RR, LSRR or SSRR option.
 */
static void
ip_printroute(const char *type, register const u_char *cp, u_int length)
{
	register u_int ptr = cp[2] - 1;
	register u_int len;

	printf(" %s{", type);
	if ((length + 1) & 3)
		printf(" [bad length %d]", length);
	if (ptr < 3 || ((ptr + 1) & 3) || ptr > length + 1)
		printf(" [bad ptr %d]", cp[2]);

	type = "";
	for (len = 3; len < length; len += 4) {
		if (ptr == len)
			type = "#";
		printf("%s%s", type, ipaddr_string(&cp[len]));
		type = " ";
	}
	printf("%s}", ptr == len? "#" : "");
}

static void
ip_printts(register const u_char *cp, u_int length)
{
	register u_int ptr = cp[2] - 1;
	register u_int len = 0;
	int hoplen;
	char *type;

	printf(" TS{");
	hoplen = ((cp[3]&0xF) != IPOPT_TS_TSONLY) ? 8 : 4;
	if ((length - 4) & (hoplen-1))
		printf("[bad length %d]", length);
	if (ptr < 4 || ((ptr - 4) & (hoplen-1)) || ptr > length + 1)
		printf("[bad ptr %d]", cp[2]);
	switch (cp[3]&0xF) {
	case IPOPT_TS_TSONLY:
		printf("TSONLY");
		break;
	case IPOPT_TS_TSANDADDR:
		printf("TS+ADDR");
		break;
	/*
	 * prespecified should really be 3, but some ones might send 2
	 * instead, and the IPOPT_TS_PRESPEC constant can apparently
	 * have both values, so we have to hard-code it here.
	 */

	case 2:
		printf("PRESPEC2.0");
		break;
	case 3:			/* IPOPT_TS_PRESPEC */
		printf("PRESPEC");
		break;
	default:	
		printf("[bad ts type %d]", cp[3]&0xF);
		goto done;
	}

	type = " ";
	for (len = 4; len < length; len += hoplen) {
		if (ptr == len)
			type = " ^ ";
		printf("%s%d@%s", type, EXTRACT_32BITS(&cp[len+hoplen-4]),
		       hoplen!=8 ? "" : ipaddr_string(&cp[len]));
		type = " ";
	}

done:
	printf("%s", ptr == len ? " ^ " : "");

	if (cp[3]>>4)
		printf(" [%d hops not recorded]} ", cp[3]>>4);
	else
		printf("}");
}

/*
 * print IP options.
 */
static void
ip_optprint(register const u_char *cp, u_int length)
{
	register u_int len;

	for (; length > 0; cp += len, length -= len) {
		int tt = *cp;

		if (tt == IPOPT_NOP || tt == IPOPT_EOL)
			len = 1;
		else {
			if (&cp[1] >= snapend) {
				printf("[|ip]");
				return;
			}
			len = cp[1];
		}
		if (len <= 0) {
			printf("[|ip op len %d]", len);
			return;
		}
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
			ip_printts(cp, len);
			break;

#ifndef IPOPT_SECURITY
#define IPOPT_SECURITY 130
#endif /* IPOPT_SECURITY */
		case IPOPT_SECURITY:
			printf(" SECURITY{%d}", len);
			break;

		case IPOPT_RR:
			ip_printroute("RR", cp, len);
			break;

		case IPOPT_SSRR:
			ip_printroute("SSRR", cp, len);
			break;

		case IPOPT_LSRR:
			ip_printroute("LSRR", cp, len);
			break;

#ifndef IPOPT_RA
#define IPOPT_RA 148		/* router alert */
#endif
		case IPOPT_RA:
			printf(" RA");
			if (len != 4)
				printf("{%d}", len);
			else if (cp[2] || cp[3])
				printf("%d.%d", cp[2], cp[3]);
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
u_short
in_cksum(const u_short *addr, register int len, u_short csum)
{
	int nleft = len;
	const u_short *w = addr;
	u_short answer;
	int sum = csum;

 	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
 	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}
	if (nleft == 1)
		sum += htons(*(u_char *)w<<8);

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/*
 * print an IP datagram.
 */
void
ip_print(register const u_char *bp, register u_int length)
{
	register const struct ip *ip;
	register u_int hlen, len, len0, off;
	register const u_char *cp;
	u_char nh;
	int advance;

	ip = (const struct ip *)bp;
#ifdef LBL_ALIGN
	/*
	 * If the IP header is not aligned, copy into abuf.
	 * This will never happen with BPF.  It does happen raw packet
	 * dumps from -r.
	 */
	if ((long)ip & 3) {
		static u_char *abuf = NULL;
		static int didwarn = 0;

		if (abuf == NULL) {
			abuf = (u_char *)malloc(snaplen);
			if (abuf == NULL)
				error("ip_print: malloc");
		}
		memcpy((char *)abuf, (char *)ip, min(length, snaplen));
		snapend += abuf - (u_char *)ip;
		packetp = abuf;
		ip = (struct ip *)abuf;
		/* We really want libpcap to give us aligned packets */
		if (!didwarn) {
			warning("compensating for unaligned libpcap packets");
			++didwarn;
		}
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
	hlen = IP_HL(ip) * 4;
	if (hlen < sizeof (struct ip)) {
		(void)printf("bad-hlen %d", hlen);
		return;
	}

	len = ntohs(ip->ip_len);
	if (length < len)
		(void)printf("truncated-ip - %d bytes missing!",
			len - length);
	len -= hlen;
	len0 = len;

	/*
	 * If this is fragment zero, hand it to the next higher
	 * level protocol.
	 */
	off = ntohs(ip->ip_off);
	if ((off & 0x1fff) == 0) {
		cp = (const u_char *)ip + hlen;
		nh = ip->ip_p;

		if (nh != IPPROTO_TCP && nh != IPPROTO_UDP) {
			(void)printf("%s > %s: ", ipaddr_string(&ip->ip_src),
				ipaddr_string(&ip->ip_dst));
		}
again:
		switch (nh) {

#ifndef IPPROTO_AH
#define IPPROTO_AH	51
#endif
		case IPPROTO_AH:
			nh = *cp;
			advance = ah_print(cp, (const u_char *)ip);
			cp += advance;
			len -= advance;
			goto again;

#ifndef IPPROTO_ESP
#define IPPROTO_ESP	50
#endif
		case IPPROTO_ESP:
		    {
			int enh;
			advance = esp_print(cp, (const u_char *)ip, &enh);
			cp += advance;
			len -= advance;
			if (enh < 0)
				break;
			nh = enh & 0xff;
			goto again;
		    }

#ifndef IPPROTO_IPCOMP
#define IPPROTO_IPCOMP	108
#endif
		case IPPROTO_IPCOMP:
		    {
			int enh;
			advance = ipcomp_print(cp, (const u_char *)ip, &enh);
			cp += advance;
			len -= advance;
			if (enh < 0)
				break;
			nh = enh & 0xff;
			goto again;
		    }

		case IPPROTO_TCP:
			tcp_print(cp, len, (const u_char *)ip, (off &~ 0x6000));
			break;

		case IPPROTO_UDP:
			udp_print(cp, len, (const u_char *)ip, (off &~ 0x6000));
			break;

		case IPPROTO_ICMP:
			icmp_print(cp, len, (const u_char *)ip);
			break;

#ifndef IPPROTO_IGRP
#define IPPROTO_IGRP 9
#endif
		case IPPROTO_IGRP:
			igrp_print(cp, len, (const u_char *)ip);
			break;

		case IPPROTO_ND:
#if 0
			(void)printf("%s > %s:", ipaddr_string(&ip->ip_src),
				ipaddr_string(&ip->ip_dst));
#endif
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

		case 4:
			/* DVMRP multicast tunnel (ip-in-ip encapsulation) */
#if 0
			if (vflag)
				(void)printf("%s > %s: ",
					     ipaddr_string(&ip->ip_src),
					     ipaddr_string(&ip->ip_dst));
#endif
			ip_print(cp, len);
			if (! vflag) {
				printf(" (ipip)");
				return;
			}
			break;

#ifdef INET6
#ifndef IP6PROTO_ENCAP
#define IP6PROTO_ENCAP 41
#endif
		case IP6PROTO_ENCAP:
			/* ip6-in-ip encapsulation */
#if 0
			if (vflag)
				(void)printf("%s > %s: ",
					     ipaddr_string(&ip->ip_src),
					     ipaddr_string(&ip->ip_dst));
#endif
			ip6_print(cp, len);
			if (! vflag) {
				printf(" (encap)");
				return;
			}
			break;
#endif /*INET6*/


#ifndef IPPROTO_GRE
#define IPPROTO_GRE 47
#endif
		case IPPROTO_GRE:
			if (vflag)
				(void)printf("gre %s > %s: ",
					     ipaddr_string(&ip->ip_src),
					     ipaddr_string(&ip->ip_dst));
			/* do it */
			gre_print(cp, len);
			if (! vflag) {
				printf(" (gre encap)");
				return;
  			}
  			break;

#ifndef IPPROTO_MOBILE
#define IPPROTO_MOBILE 55
#endif
		case IPPROTO_MOBILE:
			if (vflag)
				(void)printf("mobile %s > %s: ",
					     ipaddr_string(&ip->ip_src),
					     ipaddr_string(&ip->ip_dst));
			mobile_print(cp, len);
			if (! vflag) {
				printf(" (mobile encap)");
				return;
			}
			break;

#ifndef IPPROTO_PIM
#define IPPROTO_PIM	103
#endif
		case IPPROTO_PIM:
			pim_print(cp, len);
			break;

#ifndef IPPROTO_VRRP
#define IPPROTO_VRRP	112
#endif
		case IPPROTO_VRRP:
			if (vflag)
				(void)printf("vrrp %s > %s: ",
					     ipaddr_string(&ip->ip_src),
					     ipaddr_string(&ip->ip_dst));
			vrrp_print(cp, len, ip->ip_ttl);
			break;

		default:
#if 0
			(void)printf("%s > %s:", ipaddr_string(&ip->ip_src),
				ipaddr_string(&ip->ip_dst));
#endif
			(void)printf(" ip-proto-%d %d", nh, len);
			break;
		}
	}

 	/* Ultra quiet now means that all this stuff should be suppressed */
 	/* res 3-Nov-98 */
 	if (qflag > 1) return;


	/*
	 * for fragmented datagrams, print id:size@offset.  On all
	 * but the last stick a "+".  For unfragmented datagrams, note
	 * the don't fragment flag.
	 */
	len = len0;	/* get the original length */
	if (off & 0x3fff) {
		/*
		 * if this isn't the first frag, we're missing the
		 * next level protocol header.  print the ip addr.
		 */
		if (off & 0x1fff)
			(void)printf("%s > %s:", ipaddr_string(&ip->ip_src),
				      ipaddr_string(&ip->ip_dst));
#ifndef IP_MF
#define IP_MF 0x2000
#endif /* IP_MF */
#ifndef IP_DF
#define IP_DF 0x4000
#endif /* IP_DF */
		(void)printf(" (frag %d:%u@%d%s)", ntohs(ip->ip_id), len,
			(off & 0x1fff) * 8,
			(off & IP_MF)? "+" : "");

	} else if (off & IP_DF)
		(void)printf(" (DF)");

	if (ip->ip_tos) {
		(void)printf(" [tos 0x%x", (int)ip->ip_tos);
		/* ECN bits */
		if (ip->ip_tos&0x02) {
			(void)printf(",ECT");
			if (ip->ip_tos&0x01)
				(void)printf(",CE");
		}
		(void)printf("] ");
	}

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
		(void)printf("%slen %d", sep, (int)ntohs(ip->ip_len));
		sep = ", ";
		if ((u_char *)ip + hlen <= snapend) {
			sum = in_cksum((const u_short *)ip, hlen, 0);
			if (sum != 0) {
				(void)printf("%sbad cksum %x!", sep,
					     ntohs(ip->ip_sum));
				sep = ", ";
			}
		}
		if ((hlen -= sizeof(struct ip)) > 0) {
			(void)printf("%soptlen=%d", sep, hlen);
			ip_optprint((u_char *)(ip + 1), hlen);
		}
		printf(")");
	}
}

void
ipN_print(register const u_char *bp, register u_int length)
{
	struct ip *ip, hdr;

	ip = (struct ip *)bp;
	if (length < 4) {
		(void)printf("truncated-ip %d", length);
		return;
	}
	memcpy (&hdr, (char *)ip, 4);
	switch (IP_V(&hdr)) {
	case 4:
	    ip_print (bp, length);
	    return;
#ifdef INET6
	case 6:
	    ip6_print (bp, length);
	    return;
#endif
	default:
	    (void)printf("unknown ip %d", IP_V(&hdr));
	    return;
	}
}
