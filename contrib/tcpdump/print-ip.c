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
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-ip.c,v 1.128.2.6 2004/03/24 09:01:39 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "addrtoname.h"
#include "interface.h"
#include "extract.h"			/* must come after interface.h */

#include "ip.h"
#include "ipproto.h"

/*
 * print the recorded route in an IP RR, LSRR or SSRR option.
 */
static void
ip_printroute(const char *type, register const u_char *cp, u_int length)
{
	register u_int ptr;
	register u_int len;

	if (length < 3) {
		printf(" [bad length %u]", length);
		return;
	}
	printf(" %s{", type);
	if ((length + 1) & 3)
		printf(" [bad length %u]", length);
	ptr = cp[2] - 1;
	if (ptr < 3 || ((ptr + 1) & 3) || ptr > length + 1)
		printf(" [bad ptr %u]", cp[2]);

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
 * If source-routing is present, return the final destination.
 * Otherwise, return IP destination.
 *
 * This is used for UDP and TCP pseudo-header in the checksum
 * calculation.
 */
u_int32_t
ip_finddst(const struct ip *ip)
{
	int length;
	int len;
	const u_char *cp;
	u_int32_t retval;

	cp = (const u_char *)(ip + 1);
	length = (IP_HL(ip) << 2) - sizeof(struct ip);

	for (; length > 0; cp += len, length -= len) {
		int tt;

		TCHECK(*cp);
		tt = *cp;
		if (tt == IPOPT_NOP || tt == IPOPT_EOL)
			len = 1;
		else {
			TCHECK(cp[1]);
			len = cp[1];
		}
		if (len < 2) {
			return 0;
		}
		TCHECK2(*cp, len);
		switch (tt) {

		case IPOPT_SSRR:
		case IPOPT_LSRR:
			if (len < 7)
				return 0;
			memcpy(&retval, cp + len - 4, 4);
			return retval;
		}
	}
	return ip->ip_dst.s_addr;

trunc:
	return 0;
}

static void
ip_printts(register const u_char *cp, u_int length)
{
	register u_int ptr;
	register u_int len;
	int hoplen;
	const char *type;

	if (length < 4) {
		printf("[bad length %d]", length);
		return;
	}
	printf(" TS{");
	hoplen = ((cp[3]&0xF) != IPOPT_TS_TSONLY) ? 8 : 4;
	if ((length - 4) & (hoplen-1))
		printf("[bad length %d]", length);
	ptr = cp[2] - 1;
	len = 0;
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
		int tt;

		TCHECK(*cp);
		tt = *cp;
		if (tt == IPOPT_NOP || tt == IPOPT_EOL)
			len = 1;
		else {
			TCHECK(cp[1]);
			len = cp[1];
			if (len < 2) {
				printf("[|ip op len %d]", len);
				return;
			}
			TCHECK2(*cp, len);
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
			else {
				TCHECK(cp[3]);
				if (cp[2] || cp[3])
					printf("%d.%d", cp[2], cp[3]);
			}
			break;

		default:
			printf(" IPOPT-%d{%d}", cp[0], len);
			break;
		}
	}
	return;

trunc:
	printf("[|ip]");
}

/*
 * compute an IP header checksum.
 * don't modifiy the packet.
 */
u_short
in_cksum(const u_short *addr, register u_int len, int csum)
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
 * Given the host-byte-order value of the checksum field in a packet
 * header, and the network-byte-order computed checksum of the data
 * that the checksum covers (including the checksum itself), compute
 * what the checksum field *should* have been.
 */
u_int16_t
in_cksum_shouldbe(u_int16_t sum, u_int16_t computed_sum)
{
	u_int32_t shouldbe;

	/*
	 * The value that should have gone into the checksum field
	 * is the negative of the value gotten by summing up everything
	 * *but* the checksum field.
	 *
	 * We can compute that by subtracting the value of the checksum
	 * field from the sum of all the data in the packet, and then
	 * computing the negative of that value.
	 *
	 * "sum" is the value of the checksum field, and "computed_sum"
	 * is the negative of the sum of all the data in the packets,
	 * so that's -(-computed_sum - sum), or (sum + computed_sum).
	 *
	 * All the arithmetic in question is one's complement, so the
	 * addition must include an end-around carry; we do this by
	 * doing the arithmetic in 32 bits (with no sign-extension),
	 * and then adding the upper 16 bits of the sum, which contain
	 * the carry, to the lower 16 bits of the sum, and then do it
	 * again in case *that* sum produced a carry.
	 *
	 * As RFC 1071 notes, the checksum can be computed without
	 * byte-swapping the 16-bit words; summing 16-bit words
	 * on a big-endian machine gives a big-endian checksum, which
	 * can be directly stuffed into the big-endian checksum fields
	 * in protocol headers, and summing words on a little-endian
	 * machine gives a little-endian checksum, which must be
	 * byte-swapped before being stuffed into a big-endian checksum
	 * field.
	 *
	 * "computed_sum" is a network-byte-order value, so we must put
	 * it in host byte order before subtracting it from the
	 * host-byte-order value from the header; the adjusted checksum
	 * will be in host byte order, which is what we'll return.
	 */
	shouldbe = sum;
	shouldbe += ntohs(computed_sum);
	shouldbe = (shouldbe & 0xFFFF) + (shouldbe >> 16);
	shouldbe = (shouldbe & 0xFFFF) + (shouldbe >> 16);
	return shouldbe;
}

#ifndef IP_MF
#define IP_MF 0x2000
#endif /* IP_MF */
#ifndef IP_DF
#define IP_DF 0x4000
#endif /* IP_DF */
#define IP_RES 0x8000

static struct tok ip_frag_values[] = {
        { IP_MF,        "+" },
        { IP_DF,        "DF" },
	{ IP_RES,       "rsvd" }, /* The RFC3514 evil ;-) bit */
        { 0,            NULL }
};

/*
 * print an IP datagram.
 */
void
ip_print(register const u_char *bp, register u_int length)
{
	register const struct ip *ip;
	register u_int hlen, len, len0, off;
	const u_char *ipend;
	register const u_char *cp;
	u_char nh;
	int advance;
	struct protoent *proto;
	u_int16_t sum, ip_sum;

	ip = (const struct ip *)bp;
	if (IP_V(ip) != 4) { /* print version if != 4 */
	    printf("IP%u ", IP_V(ip));
	    if (IP_V(ip) == 6)
		printf(", wrong link-layer encapsulation");
	}
        else
	    printf("IP ");

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
		(void)printf("bad-hlen %u", hlen);
		return;
	}

	len = EXTRACT_16BITS(&ip->ip_len);
	if (length < len)
		(void)printf("truncated-ip - %u bytes missing! ",
			len - length);
	if (len < hlen) {
		(void)printf("bad-len %u", len);
		return;
	}

	/*
	 * Cut off the snapshot length to the end of the IP payload.
	 */
	ipend = bp + len;
	if (ipend < snapend)
		snapend = ipend;

	len -= hlen;
	len0 = len;

	off = EXTRACT_16BITS(&ip->ip_off);

        if (vflag) {
            (void)printf("(tos 0x%x", (int)ip->ip_tos);
            /* ECN bits */
            if (ip->ip_tos & 0x03) {
                switch (ip->ip_tos & 0x03) {
                case 1:
                    (void)printf(",ECT(1)");
                    break;
                case 2:
                    (void)printf(",ECT(0)");
                    break;
                case 3:
                    (void)printf(",CE");
                }
            }

            if (ip->ip_ttl >= 1)
                (void)printf(", ttl %3u", ip->ip_ttl);    

	    /*
	     * for the firewall guys, print id, offset.
             * On all but the last stick a "+" in the flags portion.
	     * For unfragmented datagrams, note the don't fragment flag.
	     */

	    (void)printf(", id %u, offset %u, flags [%s]",
			     EXTRACT_16BITS(&ip->ip_id),
			     (off & 0x1fff) * 8,
			     bittok2str(ip_frag_values, "none", off & 0xe000 ));

            (void)printf(", length: %u", EXTRACT_16BITS(&ip->ip_len));

            if ((hlen - sizeof(struct ip)) > 0) {
                (void)printf(", optlength: %u (", hlen - (u_int)sizeof(struct ip));
                ip_optprint((u_char *)(ip + 1), hlen - sizeof(struct ip));
                printf(" )");
            }

	    if ((u_char *)ip + hlen <= snapend) {
	        sum = in_cksum((const u_short *)ip, hlen, 0);
		if (sum != 0) {
		    ip_sum = EXTRACT_16BITS(&ip->ip_sum);
		    (void)printf(", bad cksum %x (->%x)!", ip_sum,
			     in_cksum_shouldbe(ip_sum, sum));
		}
	    }

            printf(") ");
	}

	/*
	 * If this is fragment zero, hand it to the next higher
	 * level protocol.
	 */
	if ((off & 0x1fff) == 0) {
		cp = (const u_char *)ip + hlen;
		nh = ip->ip_p;

		if (nh != IPPROTO_TCP && nh != IPPROTO_UDP &&
		    nh != IPPROTO_SCTP) {
			(void)printf("%s > %s: ", ipaddr_string(&ip->ip_src),
				ipaddr_string(&ip->ip_dst));
		}
again:
		switch (nh) {

		case IPPROTO_AH:
			nh = *cp;
			advance = ah_print(cp);
			if (advance <= 0)
				break;
			cp += advance;
			len -= advance;
			goto again;

		case IPPROTO_ESP:
		    {
			int enh, padlen;
			advance = esp_print(cp, (const u_char *)ip, &enh, &padlen);
			if (advance <= 0)
				break;
			cp += advance;
			len -= advance + padlen;
			nh = enh & 0xff;
			goto again;
		    }

		case IPPROTO_IPCOMP:
		    {
			int enh;
			advance = ipcomp_print(cp, &enh);
			if (advance <= 0)
				break;
			cp += advance;
			len -= advance;
			nh = enh & 0xff;
			goto again;
		    }

		case IPPROTO_SCTP:
			sctp_print(cp, (const u_char *)ip, len);
			break;

		case IPPROTO_TCP:
			tcp_print(cp, len, (const u_char *)ip, (off &~ 0x6000));
			break;

		case IPPROTO_UDP:
			udp_print(cp, len, (const u_char *)ip, (off &~ 0x6000));
			break;

		case IPPROTO_ICMP:
		        /* pass on the MF bit plus the offset to detect fragments */
			icmp_print(cp, len, (const u_char *)ip, (off & 0x3fff));
			break;

		case IPPROTO_IGRP:
			igrp_print(cp, len, (const u_char *)ip);
			break;

		case IPPROTO_ND:
			(void)printf(" nd %d", len);
			break;

		case IPPROTO_EGP:
			egp_print(cp);
			break;

		case IPPROTO_OSPF:
			ospf_print(cp, len, (const u_char *)ip);
			break;

		case IPPROTO_IGMP:
			igmp_print(cp, len);
			break;

		case IPPROTO_IPV4:
			/* DVMRP multicast tunnel (ip-in-ip encapsulation) */
			ip_print(cp, len);
			if (! vflag) {
				printf(" (ipip-proto-4)");
				return;
			}
			break;

#ifdef INET6
		case IPPROTO_IPV6:
			/* ip6-in-ip encapsulation */
			ip6_print(cp, len);
			break;
#endif /*INET6*/

		case IPPROTO_RSVP:
			rsvp_print(cp, len);
			break;

		case IPPROTO_GRE:
			/* do it */
			gre_print(cp, len);
			break;

		case IPPROTO_MOBILE:
			mobile_print(cp, len);
			break;

		case IPPROTO_PIM:
			pim_print(cp, len);
			break;

		case IPPROTO_VRRP:
			vrrp_print(cp, len, ip->ip_ttl);
			break;

		default:
			if ((proto = getprotobynumber(nh)) != NULL)
				(void)printf(" %s", proto->p_name);
			else
				(void)printf(" ip-proto-%d", nh);
			printf(" %d", len);
			break;
		}
	} else {
	    /* Ultra quiet now means that all this stuff should be suppressed */
	    if (qflag > 1) return;

	    /*
	     * if this isn't the first frag, we're missing the
	     * next level protocol header.  print the ip addr
	     * and the protocol.
	     */
	    if (off & 0x1fff) {
	        (void)printf("%s > %s:", ipaddr_string(&ip->ip_src),
			     ipaddr_string(&ip->ip_dst));
		if ((proto = getprotobynumber(ip->ip_p)) != NULL)
		    (void)printf(" %s", proto->p_name);
		else
		    (void)printf(" ip-proto-%d", ip->ip_p);
	    } 
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



