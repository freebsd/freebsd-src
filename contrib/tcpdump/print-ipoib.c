/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996,
 *	1997, 2000, 2011, 2012
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
/*
 * Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
 */

/* \summary: IP-over-InfiniBand (IPoIB) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"


extern const struct tok ethertype_values[];

#define	IPOIB_HDRLEN	44

static inline void
ipoib_hdr_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	uint16_t ether_type;

	ether_type = GET_BE_U_2(bp + 40);
	if (!ndo->ndo_qflag) {
		ND_PRINT(", ethertype %s (0x%04x)",
			     tok2str(ethertype_values,"Unknown", ether_type),
			     ether_type);
	} else {
		ND_PRINT(", ethertype %s",
			     tok2str(ethertype_values,"Unknown", ether_type));
	}

	ND_PRINT(", length %u: ", length);
}

/*
 * Print an InfiniBand frame.
 * This might be encapsulated within another frame; we might be passed
 * a pointer to a function that can print header information for that
 * frame's protocol, and an argument to pass to that function.
 */
static void
ipoib_print(netdissect_options *ndo, const u_char *p, u_int length, u_int caplen,
    void (*print_encap_header)(const u_char *), const u_char *encap_header_arg)
{
	const u_char *orig_hdr = p;
	u_int orig_length;
	u_short ether_type;

	if (caplen < IPOIB_HDRLEN) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}

	if (length < IPOIB_HDRLEN) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += length;
		return;
	}

	if (ndo->ndo_eflag) {
		nd_print_protocol_caps(ndo);
		if (print_encap_header != NULL)
			(*print_encap_header)(encap_header_arg);
		ipoib_hdr_print(ndo, p, length);
	}
	orig_length = length;

	ndo->ndo_ll_hdr_len += IPOIB_HDRLEN;
	length -= IPOIB_HDRLEN;
	caplen -= IPOIB_HDRLEN;
	ether_type = GET_BE_U_2(p + 40);
	p += IPOIB_HDRLEN;

	if (ethertype_print(ndo, ether_type, p, length, caplen, NULL, NULL) == 0) {
		/* ether_type not known, print raw packet */
		if (!ndo->ndo_eflag) {
			if (print_encap_header != NULL)
				(*print_encap_header)(encap_header_arg);
			ipoib_hdr_print(ndo, orig_hdr , orig_length);
		}

		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
	}
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ether header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
ipoib_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	ndo->ndo_protocol = "ipoib";
	ipoib_print(ndo, p, h->len, h->caplen, NULL, NULL);
}
