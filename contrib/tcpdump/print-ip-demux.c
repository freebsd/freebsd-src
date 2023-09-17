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

/* \summary: IPv4/IPv6 payload printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#include "ip.h"
#include "ipproto.h"

void
ip_demux_print(netdissect_options *ndo,
	       const u_char *bp,
	       u_int length, u_int ver, int fragmented, u_int ttl_hl,
	       uint8_t nh, const u_char *iph)
{
	int advance;
	const char *p_name;

	advance = 0;

again:
	switch (nh) {

	case IPPROTO_AH:
		if (!ND_TTEST_1(bp)) {
			ndo->ndo_protocol = "ah";
			nd_print_trunc(ndo);
			break;
		}
		nh = GET_U_1(bp);
		advance = ah_print(ndo, bp);
		if (advance <= 0)
			break;
		bp += advance;
		length -= advance;
		goto again;

	case IPPROTO_ESP:
	{
		esp_print(ndo, bp, length, iph, ver, fragmented, ttl_hl);
		/*
		 * Either this has decrypted the payload and
		 * printed it, in which case there's nothing more
		 * to do, or it hasn't, in which case there's
		 * nothing more to do.
		 */
		break;
	}

	case IPPROTO_IPCOMP:
	{
		ipcomp_print(ndo, bp);
		/*
		 * Either this has decompressed the payload and
		 * printed it, in which case there's nothing more
		 * to do, or it hasn't, in which case there's
		 * nothing more to do.
		 */
		break;
	}

	case IPPROTO_SCTP:
		sctp_print(ndo, bp, iph, length);
		break;

	case IPPROTO_DCCP:
		dccp_print(ndo, bp, iph, length);
		break;

	case IPPROTO_TCP:
		tcp_print(ndo, bp, length, iph, fragmented);
		break;

	case IPPROTO_UDP:
		udp_print(ndo, bp, length, iph, fragmented, ttl_hl);
		break;

	case IPPROTO_ICMP:
		if (ver == 4)
			icmp_print(ndo, bp, length, iph, fragmented);
		else {
			ND_PRINT("[%s requires IPv4]",
				 tok2str(ipproto_values,"unknown",nh));
			nd_print_invalid(ndo);
		}
		break;

	case IPPROTO_ICMPV6:
		if (ver == 6)
			icmp6_print(ndo, bp, length, iph, fragmented);
		else {
			ND_PRINT("[%s requires IPv6]",
				 tok2str(ipproto_values,"unknown",nh));
			nd_print_invalid(ndo);
		}
		break;

	case IPPROTO_PIGP:
		/*
		 * XXX - the current IANA protocol number assignments
		 * page lists 9 as "any private interior gateway
		 * (used by Cisco for their IGRP)" and 88 as
		 * "EIGRP" from Cisco.
		 *
		 * Recent BSD <netinet/in.h> headers define
		 * IP_PROTO_PIGP as 9 and IP_PROTO_IGRP as 88.
		 * We define IP_PROTO_PIGP as 9 and
		 * IP_PROTO_EIGRP as 88; those names better
		 * match was the current protocol number
		 * assignments say.
		 */
		igrp_print(ndo, bp, length);
		break;

	case IPPROTO_EIGRP:
		eigrp_print(ndo, bp, length);
		break;

	case IPPROTO_ND:
		ND_PRINT(" nd %u", length);
		break;

	case IPPROTO_EGP:
		egp_print(ndo, bp, length);
		break;

	case IPPROTO_OSPF:
		if (ver == 6)
			ospf6_print(ndo, bp, length);
		else
			ospf_print(ndo, bp, length, iph);
		break;

	case IPPROTO_IGMP:
		if (ver == 4)
			igmp_print(ndo, bp, length);
		else {
			ND_PRINT("[%s requires IPv4]",
				 tok2str(ipproto_values,"unknown",nh));
			nd_print_invalid(ndo);
		}
		break;

	case IPPROTO_IPV4:
		/* ipv4-in-ip encapsulation */
		ip_print(ndo, bp, length);
		break;

	case IPPROTO_IPV6:
		/* ip6-in-ip encapsulation */
		ip6_print(ndo, bp, length);
		break;

	case IPPROTO_RSVP:
		rsvp_print(ndo, bp, length);
		break;

	case IPPROTO_GRE:
		gre_print(ndo, bp, length);
		break;

	case IPPROTO_MOBILE:
		mobile_print(ndo, bp, length);
		break;

	case IPPROTO_PIM:
		pim_print(ndo, bp, length, iph);
		break;

	case IPPROTO_VRRP:
		if (ndo->ndo_packettype == PT_CARP) {
			carp_print(ndo, bp, length, ttl_hl);
		} else {
			vrrp_print(ndo, bp, length, iph, ttl_hl, ver);
		}
		break;

	case IPPROTO_PGM:
		pgm_print(ndo, bp, length, iph);
		break;

	case IPPROTO_ETHERNET:
		if (ver == 6)
			ether_print(ndo, bp, length, ND_BYTES_AVAILABLE_AFTER(bp), NULL, NULL);
		else {
			ND_PRINT("[%s requires IPv6]",
				 tok2str(ipproto_values,"unknown",nh));
			nd_print_invalid(ndo);
		}
		break;

	case IPPROTO_NONE:
		ND_PRINT("no next header");
		break;

	default:
		if (ndo->ndo_nflag==0 && (p_name = netdb_protoname(nh)) != NULL)
			ND_PRINT(" %s", p_name);
		else
			ND_PRINT(" ip-proto-%u", nh);
		ND_PRINT(" %u", length);
		break;
	}
}
