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
 */

/* \summary: IP printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip.h"
#include "ipproto.h"


static const struct tok ip_option_values[] = {
    { IPOPT_EOL, "EOL" },
    { IPOPT_NOP, "NOP" },
    { IPOPT_TS, "timestamp" },
    { IPOPT_SECURITY, "security" },
    { IPOPT_RR, "RR" },
    { IPOPT_SSRR, "SSRR" },
    { IPOPT_LSRR, "LSRR" },
    { IPOPT_RA, "RA" },
    { IPOPT_RFC1393, "traceroute" },
    { 0, NULL }
};

/*
 * print the recorded route in an IP RR, LSRR or SSRR option.
 */
static int
ip_printroute(netdissect_options *ndo,
              const u_char *cp, u_int length)
{
	u_int ptr;
	u_int len;

	if (length < 3) {
		ND_PRINT(" [bad length %u]", length);
		return (0);
	}
	if ((length + 1) & 3)
		ND_PRINT(" [bad length %u]", length);
	ptr = GET_U_1(cp + 2) - 1;
	if (ptr < 3 || ((ptr + 1) & 3) || ptr > length + 1)
		ND_PRINT(" [bad ptr %u]", GET_U_1(cp + 2));

	for (len = 3; len < length; len += 4) {
		ND_TCHECK_4(cp + len);	/* Needed to print the IP addresses */
		ND_PRINT(" %s", GET_IPADDR_STRING(cp + len));
		if (ptr > len)
			ND_PRINT(",");
	}
	return (0);

trunc:
	return (-1);
}

/*
 * If source-routing is present and valid, return the final destination.
 * Otherwise, return IP destination.
 *
 * This is used for UDP and TCP pseudo-header in the checksum
 * calculation.
 */
static uint32_t
ip_finddst(netdissect_options *ndo,
           const struct ip *ip)
{
	u_int length;
	u_int len;
	const u_char *cp;

	cp = (const u_char *)(ip + 1);
	length = IP_HL(ip) * 4;
	if (length < sizeof(struct ip))
		goto trunc;
	length -= sizeof(struct ip);

	for (; length != 0; cp += len, length -= len) {
		int tt;

		tt = GET_U_1(cp);
		if (tt == IPOPT_EOL)
			break;
		else if (tt == IPOPT_NOP)
			len = 1;
		else {
			len = GET_U_1(cp + 1);
			if (len < 2)
				break;
		}
		if (length < len)
			goto trunc;
		ND_TCHECK_LEN(cp, len);
		switch (tt) {

		case IPOPT_SSRR:
		case IPOPT_LSRR:
			if (len < 7)
				break;
			return (GET_IPV4_TO_NETWORK_ORDER(cp + len - 4));
		}
	}
trunc:
	return (GET_IPV4_TO_NETWORK_ORDER(ip->ip_dst));
}

/*
 * Compute a V4-style checksum by building a pseudoheader.
 */
uint16_t
nextproto4_cksum(netdissect_options *ndo,
                 const struct ip *ip, const uint8_t *data,
                 u_int len, u_int covlen, uint8_t next_proto)
{
	struct phdr {
		uint32_t src;
		uint32_t dst;
		uint8_t mbz;
		uint8_t proto;
		uint16_t len;
	} ph;
	struct cksum_vec vec[2];

	/* pseudo-header.. */
	ph.len = htons((uint16_t)len);
	ph.mbz = 0;
	ph.proto = next_proto;
	ph.src = GET_IPV4_TO_NETWORK_ORDER(ip->ip_src);
	if (IP_HL(ip) == 5)
		ph.dst = GET_IPV4_TO_NETWORK_ORDER(ip->ip_dst);
	else
		ph.dst = ip_finddst(ndo, ip);

	vec[0].ptr = (const uint8_t *)(void *)&ph;
	vec[0].len = sizeof(ph);
	vec[1].ptr = data;
	vec[1].len = covlen;
	return (in_cksum(vec, 2));
}

static int
ip_printts(netdissect_options *ndo,
           const u_char *cp, u_int length)
{
	u_int ptr;
	u_int len;
	u_int hoplen;
	const char *type;

	if (length < 4) {
		ND_PRINT("[bad length %u]", length);
		return (0);
	}
	ND_PRINT(" TS{");
	hoplen = ((GET_U_1(cp + 3) & 0xF) != IPOPT_TS_TSONLY) ? 8 : 4;
	if ((length - 4) & (hoplen-1))
		ND_PRINT("[bad length %u]", length);
	ptr = GET_U_1(cp + 2) - 1;
	len = 0;
	if (ptr < 4 || ((ptr - 4) & (hoplen-1)) || ptr > length + 1)
		ND_PRINT("[bad ptr %u]", GET_U_1(cp + 2));
	switch (GET_U_1(cp + 3)&0xF) {
	case IPOPT_TS_TSONLY:
		ND_PRINT("TSONLY");
		break;
	case IPOPT_TS_TSANDADDR:
		ND_PRINT("TS+ADDR");
		break;
	case IPOPT_TS_PRESPEC:
		ND_PRINT("PRESPEC");
		break;
	default:
		ND_PRINT("[bad ts type %u]", GET_U_1(cp + 3)&0xF);
		goto done;
	}

	type = " ";
	for (len = 4; len < length; len += hoplen) {
		if (ptr == len)
			type = " ^ ";
		ND_TCHECK_LEN(cp + len, hoplen);
		ND_PRINT("%s%u@%s", type, GET_BE_U_4(cp + len + hoplen - 4),
			  hoplen!=8 ? "" : GET_IPADDR_STRING(cp + len));
		type = " ";
	}

done:
	ND_PRINT("%s", ptr == len ? " ^ " : "");

	if (GET_U_1(cp + 3) >> 4)
		ND_PRINT(" [%u hops not recorded]} ", GET_U_1(cp + 3)>>4);
	else
		ND_PRINT("}");
	return (0);

trunc:
	return (-1);
}

/*
 * print IP options.
   If truncated return -1, else 0.
 */
static int
ip_optprint(netdissect_options *ndo,
            const u_char *cp, u_int length)
{
	u_int option_len;
	const char *sep = "";

	for (; length > 0; cp += option_len, length -= option_len) {
		u_int option_code;

		ND_PRINT("%s", sep);
		sep = ",";

		option_code = GET_U_1(cp);

		ND_PRINT("%s",
		          tok2str(ip_option_values,"unknown %u",option_code));

		if (option_code == IPOPT_NOP ||
                    option_code == IPOPT_EOL)
			option_len = 1;

		else {
			option_len = GET_U_1(cp + 1);
			if (option_len < 2) {
				ND_PRINT(" [bad length %u]", option_len);
				return 0;
			}
		}

		if (option_len > length) {
			ND_PRINT(" [bad length %u]", option_len);
			return 0;
		}

		ND_TCHECK_LEN(cp, option_len);

		switch (option_code) {
		case IPOPT_EOL:
			return 0;

		case IPOPT_TS:
			if (ip_printts(ndo, cp, option_len) == -1)
				goto trunc;
			break;

		case IPOPT_RR:       /* fall through */
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			if (ip_printroute(ndo, cp, option_len) == -1)
				goto trunc;
			break;

		case IPOPT_RA:
			if (option_len < 4) {
				ND_PRINT(" [bad length %u]", option_len);
				break;
			}
			ND_TCHECK_1(cp + 3);
			if (GET_BE_U_2(cp + 2) != 0)
				ND_PRINT(" value %u", GET_BE_U_2(cp + 2));
			break;

		case IPOPT_NOP:       /* nothing to print - fall through */
		case IPOPT_SECURITY:
		default:
			break;
		}
	}
	return 0;

trunc:
	return -1;
}

#define IP_RES 0x8000

static const struct tok ip_frag_values[] = {
        { IP_MF,        "+" },
        { IP_DF,        "DF" },
	{ IP_RES,       "rsvd" }, /* The RFC3514 evil ;-) bit */
        { 0,            NULL }
};


/*
 * print an IP datagram.
 */
void
ip_print(netdissect_options *ndo,
	 const u_char *bp,
	 const u_int length)
{
	const struct ip *ip;
	u_int off;
	u_int hlen;
	u_int len;
	struct cksum_vec vec[1];
	uint8_t ip_tos, ip_ttl, ip_proto;
	uint16_t sum, ip_sum;
	const char *p_name;
	int truncated = 0;
	int presumed_tso = 0;

	ndo->ndo_protocol = "ip";
	ip = (const struct ip *)bp;

	if (!ndo->ndo_eflag) {
		nd_print_protocol_caps(ndo);
		ND_PRINT(" ");
	}

	ND_ICHECK_ZU(length, <, sizeof (struct ip));
	ND_ICHECKMSG_U("version", IP_V(ip), !=, 4);

	hlen = IP_HL(ip) * 4;
	ND_ICHECKMSG_ZU("header length", hlen, <, sizeof (struct ip));

	len = GET_BE_U_2(ip->ip_len);
	if (len > length) {
		ND_PRINT("[total length %u > length %u]", len, length);
		nd_print_invalid(ndo);
		ND_PRINT(" ");
	}
	if (len == 0) {
		/* we guess that it is a TSO send */
		len = length;
		presumed_tso = 1;
	} else
		ND_ICHECKMSG_U("total length", len, <, hlen);

	ND_TCHECK_SIZE(ip);
	/*
	 * Cut off the snapshot length to the end of the IP payload.
	 */
	if (!nd_push_snaplen(ndo, bp, len)) {
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
			"%s: can't push snaplen on buffer stack", __func__);
	}

	len -= hlen;

	off = GET_BE_U_2(ip->ip_off);

        ip_proto = GET_U_1(ip->ip_p);

        if (ndo->ndo_vflag) {
            ip_tos = GET_U_1(ip->ip_tos);
            ND_PRINT("(tos 0x%x", ip_tos);
            /* ECN bits */
            switch (ip_tos & 0x03) {

            case 0:
                break;

            case 1:
                ND_PRINT(",ECT(1)");
                break;

            case 2:
                ND_PRINT(",ECT(0)");
                break;

            case 3:
                ND_PRINT(",CE");
                break;
            }

            ip_ttl = GET_U_1(ip->ip_ttl);
            if (ip_ttl >= 1)
                ND_PRINT(", ttl %u", ip_ttl);

	    /*
	     * for the firewall guys, print id, offset.
             * On all but the last stick a "+" in the flags portion.
	     * For unfragmented datagrams, note the don't fragment flag.
	     */
	    ND_PRINT(", id %u, offset %u, flags [%s], proto %s (%u)",
                         GET_BE_U_2(ip->ip_id),
                         (off & IP_OFFMASK) * 8,
                         bittok2str(ip_frag_values, "none", off & (IP_RES|IP_DF|IP_MF)),
                         tok2str(ipproto_values, "unknown", ip_proto),
                         ip_proto);

	    if (presumed_tso)
                ND_PRINT(", length %u [was 0, presumed TSO]", length);
	    else
                ND_PRINT(", length %u", GET_BE_U_2(ip->ip_len));

            if ((hlen - sizeof(struct ip)) > 0) {
                ND_PRINT(", options (");
                if (ip_optprint(ndo, (const u_char *)(ip + 1),
                    hlen - sizeof(struct ip)) == -1) {
                        ND_PRINT(" [truncated-option]");
			truncated = 1;
                }
                ND_PRINT(")");
            }

	    if (!ndo->ndo_Kflag && (const u_char *)ip + hlen <= ndo->ndo_snapend) {
	        vec[0].ptr = (const uint8_t *)(const void *)ip;
	        vec[0].len = hlen;
	        sum = in_cksum(vec, 1);
		if (sum != 0) {
		    ip_sum = GET_BE_U_2(ip->ip_sum);
		    ND_PRINT(", bad cksum %x (->%x)!", ip_sum,
			     in_cksum_shouldbe(ip_sum, sum));
		}
	    }

	    ND_PRINT(")\n    ");
	    if (truncated) {
		ND_PRINT("%s > %s: ",
			 GET_IPADDR_STRING(ip->ip_src),
			 GET_IPADDR_STRING(ip->ip_dst));
		nd_print_trunc(ndo);
		nd_pop_packet_info(ndo);
		return;
	    }
	}

	/*
	 * If this is fragment zero, hand it to the next higher
	 * level protocol.  Let them know whether there are more
	 * fragments.
	 */
	if ((off & IP_OFFMASK) == 0) {
		uint8_t nh = GET_U_1(ip->ip_p);

		if (nh != IPPROTO_TCP && nh != IPPROTO_UDP &&
		    nh != IPPROTO_SCTP && nh != IPPROTO_DCCP) {
			ND_PRINT("%s > %s: ",
				     GET_IPADDR_STRING(ip->ip_src),
				     GET_IPADDR_STRING(ip->ip_dst));
		}
		/*
		 * Do a bounds check before calling ip_demux_print().
		 * At least the header data is required.
		 */
		if (!ND_TTEST_LEN((const u_char *)ip, hlen)) {
			ND_PRINT(" [remaining caplen(%u) < header length(%u)]",
				 ND_BYTES_AVAILABLE_AFTER((const u_char *)ip),
				 hlen);
			nd_trunc_longjmp(ndo);
		}
		ip_demux_print(ndo, (const u_char *)ip + hlen, len, 4,
			       off & IP_MF, GET_U_1(ip->ip_ttl), nh, bp);
	} else {
		/*
		 * Ultra quiet now means that all this stuff should be
		 * suppressed.
		 */
		if (ndo->ndo_qflag > 1) {
			nd_pop_packet_info(ndo);
			return;
		}

		/*
		 * This isn't the first frag, so we're missing the
		 * next level protocol header.  print the ip addr
		 * and the protocol.
		 */
		ND_PRINT("%s > %s:", GET_IPADDR_STRING(ip->ip_src),
		          GET_IPADDR_STRING(ip->ip_dst));
		if (!ndo->ndo_nflag && (p_name = netdb_protoname(ip_proto)) != NULL)
			ND_PRINT(" %s", p_name);
		else
			ND_PRINT(" ip-proto-%u", ip_proto);
	}
	nd_pop_packet_info(ndo);
	return;

trunc:
	nd_print_trunc(ndo);
	return;

invalid:
	nd_print_invalid(ndo);
}

void
ipN_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	ndo->ndo_protocol = "ipn";
	if (length < 1) {
		ND_PRINT("truncated-ip %u", length);
		return;
	}

	switch (GET_U_1(bp) & 0xF0) {
	case 0x40:
		ip_print(ndo, bp, length);
		break;
	case 0x60:
		ip6_print(ndo, bp, length);
		break;
	default:
		ND_PRINT("unknown ip %u", (GET_U_1(bp) & 0xF0) >> 4);
		break;
	}
}
