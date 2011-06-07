/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 2000
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
    "@(#) $Header: /tcpdump/master/tcpdump/print-ether.c,v 1.106 2008-02-06 10:47:53 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <pcap.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"
#include "ethertype.h"

#include "ether.h"

const struct tok ethertype_values[] = { 
    { ETHERTYPE_IP,		"IPv4" },
    { ETHERTYPE_MPLS,		"MPLS unicast" },
    { ETHERTYPE_MPLS_MULTI,	"MPLS multicast" },
    { ETHERTYPE_IPV6,		"IPv6" },
    { ETHERTYPE_8021Q,		"802.1Q" },
    { ETHERTYPE_VMAN,		"VMAN" },
    { ETHERTYPE_PUP,            "PUP" },
    { ETHERTYPE_ARP,            "ARP"},
    { ETHERTYPE_REVARP,         "Reverse ARP"},
    { ETHERTYPE_NS,             "NS" },
    { ETHERTYPE_SPRITE,         "Sprite" },
    { ETHERTYPE_TRAIL,          "Trail" },
    { ETHERTYPE_MOPDL,          "MOP DL" },
    { ETHERTYPE_MOPRC,          "MOP RC" },
    { ETHERTYPE_DN,             "DN" },
    { ETHERTYPE_LAT,            "LAT" },
    { ETHERTYPE_SCA,            "SCA" },
    { ETHERTYPE_TEB,            "TEB" },
    { ETHERTYPE_LANBRIDGE,      "Lanbridge" },
    { ETHERTYPE_DECDNS,         "DEC DNS" },
    { ETHERTYPE_DECDTS,         "DEC DTS" },
    { ETHERTYPE_VEXP,           "VEXP" },
    { ETHERTYPE_VPROD,          "VPROD" },
    { ETHERTYPE_ATALK,          "Appletalk" },
    { ETHERTYPE_AARP,           "Appletalk ARP" },
    { ETHERTYPE_IPX,            "IPX" },
    { ETHERTYPE_PPP,            "PPP" },
    { ETHERTYPE_MPCP,           "MPCP" },
    { ETHERTYPE_SLOW,           "Slow Protocols" },
    { ETHERTYPE_PPPOED,         "PPPoE D" },
    { ETHERTYPE_PPPOES,         "PPPoE S" },
    { ETHERTYPE_EAPOL,          "EAPOL" },
    { ETHERTYPE_RRCP,           "RRCP" },
    { ETHERTYPE_JUMBO,          "Jumbo" },
    { ETHERTYPE_LOOPBACK,       "Loopback" },
    { ETHERTYPE_ISO,            "OSI" },
    { ETHERTYPE_GRE_ISO,        "GRE-OSI" },
    { ETHERTYPE_CFM_OLD,        "CFM (old)" },
    { ETHERTYPE_CFM,            "CFM" },
    { ETHERTYPE_LLDP,           "LLDP" },
    { 0, NULL}
};

static inline void
ether_hdr_print(register const u_char *bp, u_int length)
{
	register const struct ether_header *ep;
	u_int16_t ether_type;

	ep = (const struct ether_header *)bp;

	(void)printf("%s > %s",
		     etheraddr_string(ESRC(ep)),
		     etheraddr_string(EDST(ep)));

	ether_type = EXTRACT_16BITS(&ep->ether_type);
	if (!qflag) {
	        if (ether_type <= ETHERMTU)
		          (void)printf(", 802.3");
                else 
		          (void)printf(", ethertype %s (0x%04x)",
				       tok2str(ethertype_values,"Unknown", ether_type),
                                       ether_type);
        } else {
                if (ether_type <= ETHERMTU)
                          (void)printf(", 802.3");
                else 
                          (void)printf(", %s", tok2str(ethertype_values,"Unknown Ethertype (0x%04x)", ether_type));
        }

	(void)printf(", length %u: ", length);
}

/*
 * Print an Ethernet frame.
 * This might be encapsulated within another frame; we might be passed
 * a pointer to a function that can print header information for that
 * frame's protocol, and an argument to pass to that function.
 */
void
ether_print(const u_char *p, u_int length, u_int caplen,
    void (*print_encap_header)(const u_char *), const u_char *encap_header_arg)
{
	struct ether_header *ep;
	u_int orig_length;
	u_short ether_type;
	u_short extracted_ether_type;

	if (caplen < ETHER_HDRLEN || length < ETHER_HDRLEN) {
		printf("[|ether]");
		return;
	}

	if (eflag) {
		if (print_encap_header != NULL)
			(*print_encap_header)(encap_header_arg);
		ether_hdr_print(p, length);
	}
	orig_length = length;

	length -= ETHER_HDRLEN;
	caplen -= ETHER_HDRLEN;
	ep = (struct ether_header *)p;
	p += ETHER_HDRLEN;

	ether_type = EXTRACT_16BITS(&ep->ether_type);

recurse:
	/*
	 * Is it (gag) an 802.3 encapsulation?
	 */
	if (ether_type <= ETHERMTU) {
		/* Try to print the LLC-layer header & higher layers */
		if (llc_print(p, length, caplen, ESRC(ep), EDST(ep),
		    &extracted_ether_type) == 0) {
			/* ether_type not known, print raw packet */
			if (!eflag) {
				if (print_encap_header != NULL)
					(*print_encap_header)(encap_header_arg);
				ether_hdr_print((u_char *)ep, orig_length);
			}

			if (!suppress_default_print)
				default_print(p, caplen);
		}
	} else if (ether_type == ETHERTYPE_8021Q) {
		/*
		 * Print VLAN information, and then go back and process
		 * the enclosed type field.
		 */
		if (caplen < 4 || length < 4) {
			printf("[|vlan]");
			return;
		}
	        if (eflag) {
	        	u_int16_t tag = EXTRACT_16BITS(p);

			printf("vlan %u, p %u%s, ",
			    tag & 0xfff,
			    tag >> 13,
			    (tag & 0x1000) ? ", CFI" : "");
		}

		ether_type = EXTRACT_16BITS(p + 2);
		if (eflag && ether_type > ETHERMTU)
			printf("ethertype %s, ", tok2str(ethertype_values,"0x%04x", ether_type));
		p += 4;
		length -= 4;
		caplen -= 4;
		goto recurse;
	} else if (ether_type == ETHERTYPE_JUMBO) {
		/*
		 * Alteon jumbo frames.
		 * See
		 *
		 *	http://tools.ietf.org/html/draft-ietf-isis-ext-eth-01
		 *
		 * which indicates that, following the type field,
		 * there's an LLC header and payload.
		 */
		/* Try to print the LLC-layer header & higher layers */
		if (llc_print(p, length, caplen, ESRC(ep), EDST(ep),
		    &extracted_ether_type) == 0) {
			/* ether_type not known, print raw packet */
			if (!eflag) {
				if (print_encap_header != NULL)
					(*print_encap_header)(encap_header_arg);
				ether_hdr_print((u_char *)ep, orig_length);
			}

			if (!suppress_default_print)
				default_print(p, caplen);
		}
	} else {
		if (ethertype_print(ether_type, p, length, caplen) == 0) {
			/* ether_type not known, print raw packet */
			if (!eflag) {
				if (print_encap_header != NULL)
					(*print_encap_header)(encap_header_arg);
				ether_hdr_print((u_char *)ep, orig_length);
			}

			if (!suppress_default_print)
				default_print(p, caplen);
		}
	}
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ether header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
u_int
ether_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	ether_print(p, h->len, h->caplen, NULL, NULL);

	return (ETHER_HDRLEN);
}

/*
 * Prints the packet payload, given an Ethernet type code for the payload's
 * protocol.
 *
 * Returns non-zero if it can do so, zero if the ethertype is unknown.
 */

int
ethertype_print(u_short ether_type, const u_char *p, u_int length, u_int caplen)
{
	switch (ether_type) {

	case ETHERTYPE_IP:
	        ip_print(gndo, p, length);
		return (1);

#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6_print(p, length);
		return (1);
#endif /*INET6*/

	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
  	        arp_print(gndo, p, length, caplen);
		return (1);

	case ETHERTYPE_DN:
		decnet_print(p, length, caplen);
		return (1);

	case ETHERTYPE_ATALK:
		if (vflag)
			fputs("et1 ", stdout);
		atalk_print(p, length);
		return (1);

	case ETHERTYPE_AARP:
		aarp_print(p, length);
		return (1);

	case ETHERTYPE_IPX:
		printf("(NOV-ETHII) ");
		ipx_print(p, length);
		return (1);

        case ETHERTYPE_ISO:
                isoclns_print(p+1, length-1, length-1);
                return(1);

	case ETHERTYPE_PPPOED:
	case ETHERTYPE_PPPOES:
	case ETHERTYPE_PPPOED2:
	case ETHERTYPE_PPPOES2:
		pppoe_print(p, length);
		return (1);

	case ETHERTYPE_EAPOL:
	        eap_print(gndo, p, length);
		return (1);

	case ETHERTYPE_RRCP:
	        rrcp_print(gndo, p - 14 , length + 14);
		return (1);

	case ETHERTYPE_PPP:
		if (length) {
			printf(": ");
			ppp_print(p, length);
		}
		return (1);

	case ETHERTYPE_MPCP:
	        mpcp_print(p, length);
		return (1);

	case ETHERTYPE_SLOW:
	        slow_print(p, length);
		return (1);

	case ETHERTYPE_CFM:
	case ETHERTYPE_CFM_OLD:
	        cfm_print(p, length);
		return (1);

	case ETHERTYPE_LLDP:
	        lldp_print(p, length);
		return (1);

        case ETHERTYPE_LOOPBACK:
                return (1);

	case ETHERTYPE_MPLS:
	case ETHERTYPE_MPLS_MULTI:
		mpls_print(p, length);
		return (1);

	case ETHERTYPE_LAT:
	case ETHERTYPE_SCA:
	case ETHERTYPE_MOPRC:
	case ETHERTYPE_MOPDL:
		/* default_print for now */
	default:
		return (0);
	}
}


/*
 * Local Variables:
 * c-style: whitesmith
 * c-basic-offset: 8
 * End:
 */

