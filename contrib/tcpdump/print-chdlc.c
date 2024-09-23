/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
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

/* \summary: Cisco HDLC printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "extract.h"
#include "chdlc.h"
#include "nlpid.h"

static void chdlc_slarp_print(netdissect_options *, const u_char *, u_int);

static const struct tok chdlc_cast_values[] = {
    { CHDLC_UNICAST, "unicast" },
    { CHDLC_BCAST, "bcast" },
    { 0, NULL}
};


/* Standard CHDLC printer */
void
chdlc_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	ndo->ndo_protocol = "chdlc";
	ndo->ndo_ll_hdr_len += chdlc_print(ndo, p, h->len);
}

u_int
chdlc_print(netdissect_options *ndo, const u_char *p, u_int length)
{
	u_int proto;
	const u_char *bp = p;

	ndo->ndo_protocol = "chdlc";
	if (length < CHDLC_HDRLEN)
		goto trunc;
	proto = GET_BE_U_2(p + 2);
	if (ndo->ndo_eflag) {
                ND_PRINT("%s, ethertype %s (0x%04x), length %u: ",
                       tok2str(chdlc_cast_values, "0x%02x", GET_U_1(p)),
                       tok2str(ethertype_values, "Unknown", proto),
                       proto,
                       length);
	}

	length -= CHDLC_HDRLEN;
	p += CHDLC_HDRLEN;

	switch (proto) {
	case ETHERTYPE_IP:
		ip_print(ndo, p, length);
		break;
	case ETHERTYPE_IPV6:
		ip6_print(ndo, p, length);
		break;
	case CHDLC_TYPE_SLARP:
		chdlc_slarp_print(ndo, p, length);
		break;
        case ETHERTYPE_MPLS:
        case ETHERTYPE_MPLS_MULTI:
                mpls_print(ndo, p, length);
		break;
        case ETHERTYPE_ISO:
                /* is the fudge byte set ? lets verify by spotting ISO headers */
                if (length < 2)
                    goto trunc;
                if (GET_U_1(p + 1) == NLPID_CLNP ||
                    GET_U_1(p + 1) == NLPID_ESIS ||
                    GET_U_1(p + 1) == NLPID_ISIS)
                    isoclns_print(ndo, p + 1, length - 1);
                else
                    isoclns_print(ndo, p, length);
                break;
	default:
                if (!ndo->ndo_eflag)
                        ND_PRINT("unknown CHDLC protocol (0x%04x)", proto);
                break;
	}

	return (CHDLC_HDRLEN);

trunc:
	nd_print_trunc(ndo);
	return (ND_BYTES_AVAILABLE_AFTER(bp));
}

/*
 * The fixed-length portion of a SLARP packet.
 */
struct cisco_slarp {
	nd_uint32_t code;
#define SLARP_REQUEST	0
#define SLARP_REPLY	1
#define SLARP_KEEPALIVE	2
	union {
		struct {
			uint8_t addr[4];
			uint8_t mask[4];
		} addr;
		struct {
			nd_uint32_t myseq;
			nd_uint32_t yourseq;
			nd_uint16_t rel;
		} keep;
	} un;
};

#define SLARP_MIN_LEN	14
#define SLARP_MAX_LEN	18

static void
chdlc_slarp_print(netdissect_options *ndo, const u_char *cp, u_int length)
{
	const struct cisco_slarp *slarp;
        u_int sec,min,hrs,days;

	ndo->ndo_protocol = "chdlc_slarp";
	ND_PRINT("SLARP (length: %u), ",length);
	if (length < SLARP_MIN_LEN)
		goto trunc;

	slarp = (const struct cisco_slarp *)cp;
	ND_TCHECK_LEN(slarp, SLARP_MIN_LEN);
	switch (GET_BE_U_4(slarp->code)) {
	case SLARP_REQUEST:
		ND_PRINT("request");
		/*
		 * At least according to William "Chops" Westfield's
		 * message in
		 *
		 *	https://web.archive.org/web/20190725151313/www.nethelp.no/net/cisco-hdlc.txt
		 *
		 * the address and mask aren't used in requests -
		 * they're just zero.
		 */
		break;
	case SLARP_REPLY:
		ND_PRINT("reply %s/%s",
			GET_IPADDR_STRING(slarp->un.addr.addr),
			GET_IPADDR_STRING(slarp->un.addr.mask));
		break;
	case SLARP_KEEPALIVE:
		ND_PRINT("keepalive: mineseen=0x%08x, yourseen=0x%08x, reliability=0x%04x",
                       GET_BE_U_4(slarp->un.keep.myseq),
                       GET_BE_U_4(slarp->un.keep.yourseq),
                       GET_BE_U_2(slarp->un.keep.rel));

                if (length >= SLARP_MAX_LEN) { /* uptime-stamp is optional */
                        cp += SLARP_MIN_LEN;
                        sec = GET_BE_U_4(cp) / 1000;
                        min = sec / 60; sec -= min * 60;
                        hrs = min / 60; min -= hrs * 60;
                        days = hrs / 24; hrs -= days * 24;
                        ND_PRINT(", link uptime=%ud%uh%um%us",days,hrs,min,sec);
                }
		break;
	default:
		ND_PRINT("0x%02x unknown", GET_BE_U_4(slarp->code));
                if (ndo->ndo_vflag <= 1)
                    print_unknown_data(ndo,cp+4,"\n\t",length-4);
		break;
	}

	if (SLARP_MAX_LEN < length && ndo->ndo_vflag)
		ND_PRINT(", (trailing junk: %u bytes)", length - SLARP_MAX_LEN);
        if (ndo->ndo_vflag > 1)
            print_unknown_data(ndo,cp+4,"\n\t",length-4);
	return;

trunc:
	nd_print_trunc(ndo);
}
