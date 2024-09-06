/*
 * Copyright (C) 2001 WIDE Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* \summary: Multi-Protocol Label Switching (MPLS) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "mpls.h"

static const char *mpls_labelname[] = {
/*0*/	"IPv4 explicit NULL", "router alert", "IPv6 explicit NULL",
	"implicit NULL", "rsvd",
/*5*/	"rsvd", "rsvd", "rsvd", "rsvd", "rsvd",
/*10*/	"rsvd", "rsvd", "rsvd", "rsvd", "rsvd",
/*15*/	"rsvd",
};

enum mpls_packet_type {
	PT_UNKNOWN,
	PT_IPV4,
	PT_IPV6,
	PT_OSI
};

/*
 * RFC3032: MPLS label stack encoding
 */
void
mpls_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	const u_char *p;
	uint32_t label_entry;
	uint16_t label_stack_depth = 0;
	uint8_t first;
	enum mpls_packet_type pt = PT_UNKNOWN;

	ndo->ndo_protocol = "mpls";
	p = bp;
	nd_print_protocol_caps(ndo);
	do {
		if (length < sizeof(label_entry))
			goto invalid;
		label_entry = GET_BE_U_4(p);
		ND_PRINT("%s(label %u",
		       (label_stack_depth && ndo->ndo_vflag) ? "\n\t" : " ",
			MPLS_LABEL(label_entry));
		label_stack_depth++;
		if (ndo->ndo_vflag &&
		    MPLS_LABEL(label_entry) < sizeof(mpls_labelname) / sizeof(mpls_labelname[0]))
			ND_PRINT(" (%s)", mpls_labelname[MPLS_LABEL(label_entry)]);
		ND_PRINT(", tc %u", MPLS_TC(label_entry));
		if (MPLS_STACK(label_entry))
			ND_PRINT(", [S]");
		ND_PRINT(", ttl %u)", MPLS_TTL(label_entry));

		p += sizeof(label_entry);
		length -= sizeof(label_entry);
	} while (!MPLS_STACK(label_entry));

	/*
	 * Try to figure out the packet type.
	 */
	switch (MPLS_LABEL(label_entry)) {

	case 0:	/* IPv4 explicit NULL label */
	case 3:	/* IPv4 implicit NULL label */
		pt = PT_IPV4;
		break;

	case 2:	/* IPv6 explicit NULL label */
		pt = PT_IPV6;
		break;

	default:
		/*
		 * Generally there's no indication of protocol in MPLS label
		 * encoding.
		 *
		 * However, draft-hsmit-isis-aal5mux-00.txt describes a
		 * technique for encapsulating IS-IS and IP traffic on the
		 * same ATM virtual circuit; you look at the first payload
		 * byte to determine the network layer protocol, based on
		 * the fact that
		 *
		 *	1) the first byte of an IP header is 0x45-0x4f
		 *	   for IPv4 and 0x60-0x6f for IPv6;
		 *
		 *	2) the first byte of an OSI CLNP packet is 0x81,
		 *	   the first byte of an OSI ES-IS packet is 0x82,
		 *	   and the first byte of an OSI IS-IS packet is
		 *	   0x83;
		 *
		 * so the network layer protocol can be inferred from the
		 * first byte of the packet, if the protocol is one of the
		 * ones listed above.
		 *
		 * Cisco sends control-plane traffic MPLS-encapsulated in
		 * this fashion.
		 */
		if (length < 1) {
			/* nothing to print */
			return;
		}
		first = GET_U_1(p);
		pt =
			(first >= 0x45 && first <= 0x4f) ? PT_IPV4 :
			(first >= 0x60 && first <= 0x6f) ? PT_IPV6 :
			(first >= 0x81 && first <= 0x83) ? PT_OSI :
			/* ok bail out - we did not figure out what it is*/
			PT_UNKNOWN;
	}

	/*
	 * Print the payload.
	 */
	switch (pt) {
	case PT_UNKNOWN:
		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, length);
		break;

	case PT_IPV4:
		ND_PRINT(ndo->ndo_vflag ? "\n\t" : " ");
		ip_print(ndo, p, length);
		break;

	case PT_IPV6:
		ND_PRINT(ndo->ndo_vflag ? "\n\t" : " ");
		ip6_print(ndo, p, length);
		break;

	case PT_OSI:
		ND_PRINT(ndo->ndo_vflag ? "\n\t" : " ");
		isoclns_print(ndo, p, length);
		break;
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(p, length);
}
