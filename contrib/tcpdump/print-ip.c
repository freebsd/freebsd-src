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
 * $FreeBSD: src/contrib/tcpdump/print-ip.c,v 1.7 2000/01/30 01:00:53 fenner Exp $
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-ip.c,v 1.79 1999/12/22 06:27:21 itojun Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "addrtoname.h"
#include "interface.h"
#include "extract.h"			/* must come after interface.h */

/* Compatibility */
#ifndef	IPPROTO_ND
#define	IPPROTO_ND	77
#endif

#ifndef IN_CLASSD
#define IN_CLASSD(i) (((int32_t)(i) & 0xf0000000) == 0xe0000000)
#endif

/* (following from ipmulti/mrouted/prune.h) */

/*
 * The packet format for a traceroute request.
 */
struct tr_query {
	u_int  tr_src;			/* traceroute source */
	u_int  tr_dst;			/* traceroute destination */
	u_int  tr_raddr;		/* traceroute response address */
 	u_int  tr_rttlqid;		/* response ttl and qid */
};

#define TR_GETTTL(x)		(int)(((x) >> 24) & 0xff)
#define TR_GETQID(x)		((x) & 0x00ffffff)

/*
 * Traceroute response format.  A traceroute response has a tr_query at the
 * beginning, followed by one tr_resp for each hop taken.
 */
struct tr_resp {
	u_int tr_qarr;			/* query arrival time */
	u_int tr_inaddr;		/* incoming interface address */
	u_int tr_outaddr;		/* outgoing interface address */
	u_int tr_rmtaddr;		/* parent address in source tree */
	u_int tr_vifin;			/* input packet count on interface */
	u_int tr_vifout;		/* output packet count on interface */
	u_int tr_pktcnt;		/* total incoming packets for src-grp */
	u_char  tr_rproto;		/* routing proto deployed on router */
	u_char  tr_fttl;		/* ttl required to forward on outvif */
	u_char  tr_smask;		/* subnet mask for src addr */
	u_char  tr_rflags;		/* forwarding error codes */
};

/* defs within mtrace */
#define TR_QUERY 1
#define TR_RESP	2

/* fields for tr_rflags (forwarding error codes) */
#define TR_NO_ERR	0
#define TR_WRONG_IF	1
#define TR_PRUNED	2
#define TR_OPRUNED	3
#define TR_SCOPED	4
#define TR_NO_RTE	5
#define TR_NO_FWD	7
#define TR_NO_SPACE	0x81
#define TR_OLD_ROUTER	0x82

/* fields for tr_rproto (routing protocol) */
#define TR_PROTO_DVMRP	1
#define TR_PROTO_MOSPF	2
#define TR_PROTO_PIM	3
#define TR_PROTO_CBT	4

static void print_mtrace(register const u_char *bp, register u_int len)
{
	register struct tr_query *tr = (struct tr_query *)(bp + 8);

	printf("mtrace %lu: %s to %s reply-to %s",
		(u_long)TR_GETQID(ntohl(tr->tr_rttlqid)),
		ipaddr_string(&tr->tr_src), ipaddr_string(&tr->tr_dst),
		ipaddr_string(&tr->tr_raddr));
	if (IN_CLASSD(ntohl(tr->tr_raddr)))
		printf(" with-ttl %d", TR_GETTTL(ntohl(tr->tr_rttlqid)));
}

static void print_mresp(register const u_char *bp, register u_int len)
{
	register struct tr_query *tr = (struct tr_query *)(bp + 8);

	printf("mresp %lu: %s to %s reply-to %s",
		(u_long)TR_GETQID(ntohl(tr->tr_rttlqid)),
		ipaddr_string(&tr->tr_src), ipaddr_string(&tr->tr_dst),
		ipaddr_string(&tr->tr_raddr));
	if (IN_CLASSD(ntohl(tr->tr_raddr)))
		printf(" with-ttl %d", TR_GETTTL(ntohl(tr->tr_rttlqid)));
}

static void
igmp_print(register const u_char *bp, register u_int len,
	   register const u_char *bp2)
{
	register const struct ip *ip;

	ip = (const struct ip *)bp2;
        (void)printf("%s > %s: ",
		ipaddr_string(&ip->ip_src),
		ipaddr_string(&ip->ip_dst));

	if (qflag) {
		(void)printf("igmp");
		return;
	}

	TCHECK2(bp[0], 8);
	switch (bp[0]) {
	case 0x11:
		(void)printf("igmp %s query", bp[1] ? "v2" : "v1");
		if (bp[1] && bp[1] != 100)
			(void)printf(" [intvl %d]", bp[1]);
		(void)printf("igmp query");
		if (EXTRACT_32BITS(&bp[4]))
			(void)printf(" [gaddr %s]", ipaddr_string(&bp[4]));
		if (len != 8)
			(void)printf(" [len %d]", len);
		break;
	case 0x12:
	case 0x16:
		(void)printf("igmp %s report %s",
			     (bp[0] & 0x0f) == 6 ? "v2" : "v1",
			     ipaddr_string(&bp[4]));
		if (len != 8)
			(void)printf(" [len %d]", len);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	case 0x17:
		(void)printf("igmp leave %s", ipaddr_string(&bp[4]));
		if (len != 8)
			(void)printf(" [len %d]", len);
		if (bp[1])
			(void)printf(" [b1=0x%x]", bp[1]);
		break;
	case 0x13:
		(void)printf("igmp dvmrp");
		if (len < 8)
			(void)printf(" [len %d]", len);
		else
			dvmrp_print(bp, len);
		break;
	case 0x14:
		(void)printf("igmp pimv1");
		pimv1_print(bp, len);
  		break;
	case 0x1e:
		print_mresp(bp, len);
		break;
	case 0x1f:
		print_mtrace(bp, len);
		break;
	default:
		(void)printf("igmp-%d", bp[0] & 0xf);
		if (bp[1])
			(void)printf(" [b1=0x%02x]", bp[1]);
		break;
	}

	if (vflag && TTEST2(bp[0], len)) {
		/* Check the IGMP checksum */
		if (in_cksum((const u_short*)bp, len, 0))
			printf(" bad igmp cksum %x!", EXTRACT_16BITS(&bp[2]));
	}
	return;
trunc:
	fputs("[|igmp]", stdout);
}

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

		len = (tt == IPOPT_NOP || tt == IPOPT_EOL) ? 1 : cp[1];
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
	hlen = ip->ip_hl * 4;

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
			tcp_print(cp, len, (const u_char *)ip);
			break;

		case IPPROTO_UDP:
			udp_print(cp, len, (const u_char *)ip);
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
