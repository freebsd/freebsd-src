/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
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
    "@(#) $Header: print-ip.c,v 1.56 96/07/23 14:17:24 leres Exp $ (LBL)";
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
#include <netinet/tcpip.h>

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
#if defined(BYTE_ORDER) && (BYTE_ORDER == LITTLE_ENDIAN)
	struct {
		u_int	qid : 24;	/* traceroute query id */
		u_int	ttl : 8;	/* traceroute response ttl */
	} q;
#else
	struct {
		u_int   ttl : 8;	/* traceroute response ttl */
		u_int   qid : 24;	/* traceroute query id */
	} q;
#endif /* BYTE_ORDER */
};

#define tr_rttl q.ttl
#define tr_qid  q.qid

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
	register struct tr_query* tr = (struct tr_query*)(bp + 8);

	printf("mtrace %d: %s to %s reply-to %s", tr->tr_qid,
		ipaddr_string(&tr->tr_src), ipaddr_string(&tr->tr_dst),
		ipaddr_string(&tr->tr_raddr));
	if (IN_CLASSD(ntohl(tr->tr_raddr)))
		printf(" with-ttl %d", tr->tr_rttl);
}

static void print_mresp(register const u_char *bp, register u_int len)
{
	register struct tr_query* tr = (struct tr_query*)(bp + 8);

	printf("mresp %d: %s to %s reply-to %s", tr->tr_qid,
		ipaddr_string(&tr->tr_src), ipaddr_string(&tr->tr_dst),
		ipaddr_string(&tr->tr_raddr));
	if (IN_CLASSD(ntohl(tr->tr_raddr)))
		printf(" with-ttl %d", tr->tr_rttl);
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

	TCHECK2(bp[0], 8);
	switch (bp[0]) {
	case 0x11:
		(void)printf("igmp %s query", bp[1] ? "v2" : "v1");
		if (bp[1] && bp[1] != 100)
			(void)printf(" [intvl %d]", bp[1]);
		if (*(int *)&bp[4])
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
		(void)printf("igmp pim");
		pim_print(bp, len);
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

	TCHECK2(bp[0], len);
	if (vflag) {
		/* Check the IGMP checksum */
		u_int32_t sum = 0;
		int count;
		const u_short *sp = (u_short*)bp;
		
		for (count = len / 2; --count >= 0; )
			sum += *sp++;
		if (len & 1)
			sum += ntohs(*(unsigned char*) sp << 8);
		while (sum >> 16)
			sum = (sum & 0xffff) + (sum >> 16);
		sum = 0xffff & ~sum;
		if (sum != 0)
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

		case IPOPT_RA:
			printf(" RA{%d}", len);
			if (cp[2] != 0 || cp[3] != 0)
				printf(" [b23=0x04%x]", cp[2] << 8 | cp[3]);
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
	register u_int32_t sum = 0;
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
ip_print(register const u_char *bp, register u_int length)
{
	register const struct ip *ip;
	register u_int hlen, len, off;
	register const u_char *cp;

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

#ifndef IPPROTO_IGRP
#define IPPROTO_IGRP 9
#endif
		case IPPROTO_IGRP:
			igrp_print(cp, len, (const u_char *)ip);
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
		if ((u_char *)ip + hlen <= snapend) {
			sum = in_cksum(ip);
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
