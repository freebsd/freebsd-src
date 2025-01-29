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
 *
 * Hacked version of print-ether.c  Larry Lile <lile@stdio.com>
 *
 * Further tweaked to more closely resemble print-fddi.c
 *	Guy Harris <guy@alum.mit.edu>
 */

/* \summary: Token Ring printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"

/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define TOKEN_HDRLEN		14
#define ROUTING_SEGMENT_MAX	16
#define IS_SOURCE_ROUTED(trp)	((trp)->token_shost[0] & 0x80)
#define FRAME_TYPE(trp)		((GET_U_1((trp)->token_fc) & 0xC0) >> 6)
#define TOKEN_FC_LLC		1

#define BROADCAST(trp)		((GET_BE_U_2((trp)->token_rcf) & 0xE000) >> 13)
#define RIF_LENGTH(trp)		((GET_BE_U_2((trp)->token_rcf) & 0x1f00) >> 8)
#define DIRECTION(trp)		((GET_BE_U_2((trp)->token_rcf) & 0x0080) >> 7)
#define LARGEST_FRAME(trp)	((GET_BE_U_2((trp)->token_rcf) & 0x0070) >> 4)
#define RING_NUMBER(trp, x)	((GET_BE_U_2((trp)->token_rseg[x]) & 0xfff0) >> 4)
#define BRIDGE_NUMBER(trp, x)	(GET_BE_U_2((trp)->token_rseg[x]) & 0x000f)
#define SEGMENT_COUNT(trp)	((int)((RIF_LENGTH(trp) - 2) / 2))

struct token_header {
	nd_uint8_t   token_ac;
	nd_uint8_t   token_fc;
	nd_mac_addr  token_dhost;
	nd_mac_addr  token_shost;
	nd_uint16_t  token_rcf;
	nd_uint16_t  token_rseg[ROUTING_SEGMENT_MAX];
};


/* Extract src, dst addresses */
static void
extract_token_addrs(const struct token_header *trp, char *fsrc, char *fdst)
{
	memcpy(fdst, (const char *)trp->token_dhost, 6);
	memcpy(fsrc, (const char *)trp->token_shost, 6);
}

/*
 * Print the TR MAC header
 */
static void
token_hdr_print(netdissect_options *ndo,
                const struct token_header *trp, u_int length,
                const u_char *fsrc, const u_char *fdst)
{
	const char *srcname, *dstname;

	srcname = etheraddr_string(ndo, fsrc);
	dstname = etheraddr_string(ndo, fdst);

	if (!ndo->ndo_qflag)
		ND_PRINT("%02x %02x ",
		       GET_U_1(trp->token_ac),
		       GET_U_1(trp->token_fc));
	ND_PRINT("%s > %s, length %u: ",
	       srcname, dstname,
	       length);
}

static const char *broadcast_indicator[] = {
	"Non-Broadcast", "Non-Broadcast",
	"Non-Broadcast", "Non-Broadcast",
	"All-routes",    "All-routes",
	"Single-route",  "Single-route"
};

static const char *direction[] = {
	"Forward", "Backward"
};

static const char *largest_frame[] = {
	"516",
	"1500",
	"2052",
	"4472",
	"8144",
	"11407",
	"17800",
	"??"
};

u_int
token_print(netdissect_options *ndo, const u_char *p, u_int length, u_int caplen)
{
	const struct token_header *trp;
	int llc_hdrlen;
	nd_mac_addr srcmac, dstmac;
	struct lladdr_info src, dst;
	u_int route_len = 0, hdr_len = TOKEN_HDRLEN;
	int seg;

	ndo->ndo_protocol = "token-ring";
	trp = (const struct token_header *)p;

	if (caplen < TOKEN_HDRLEN) {
		nd_print_trunc(ndo);
		return hdr_len;
	}

	/*
	 * Get the TR addresses into a canonical form
	 */
	extract_token_addrs(trp, (char*)srcmac, (char*)dstmac);

	/* Adjust for source routing information in the MAC header */
	if (IS_SOURCE_ROUTED(trp)) {
		/* Clear source-routed bit */
		srcmac[0] &= 0x7f;

		if (ndo->ndo_eflag)
			token_hdr_print(ndo, trp, length, srcmac, dstmac);

		if (caplen < TOKEN_HDRLEN + 2) {
			nd_print_trunc(ndo);
			return hdr_len;
		}
		route_len = RIF_LENGTH(trp);
		hdr_len += route_len;
		if (caplen < hdr_len) {
			nd_print_trunc(ndo);
			return hdr_len;
		}
		if (ndo->ndo_vflag) {
			ND_PRINT("%s ", broadcast_indicator[BROADCAST(trp)]);
			ND_PRINT("%s", direction[DIRECTION(trp)]);

			for (seg = 0; seg < SEGMENT_COUNT(trp); seg++)
				ND_PRINT(" [%u:%u]", RING_NUMBER(trp, seg),
				    BRIDGE_NUMBER(trp, seg));
		} else {
			ND_PRINT("rt = %x", GET_BE_U_2(trp->token_rcf));

			for (seg = 0; seg < SEGMENT_COUNT(trp); seg++)
				ND_PRINT(":%x",
					 GET_BE_U_2(trp->token_rseg[seg]));
		}
		ND_PRINT(" (%s) ", largest_frame[LARGEST_FRAME(trp)]);
	} else {
		if (ndo->ndo_eflag)
			token_hdr_print(ndo, trp, length, srcmac, dstmac);
	}

	src.addr = srcmac;
	src.addr_string = etheraddr_string;
	dst.addr = dstmac;
	dst.addr_string = etheraddr_string;

	/* Skip over token ring MAC header and routing information */
	length -= hdr_len;
	p += hdr_len;
	caplen -= hdr_len;

	/* Frame Control field determines interpretation of packet */
	if (FRAME_TYPE(trp) == TOKEN_FC_LLC) {
		/* Try to print the LLC-layer header & higher layers */
		llc_hdrlen = llc_print(ndo, p, length, caplen, &src, &dst);
		if (llc_hdrlen < 0) {
			/* packet type not known, print raw packet */
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
			llc_hdrlen = -llc_hdrlen;
		}
		hdr_len += llc_hdrlen;
	} else {
		/* Some kinds of TR packet we cannot handle intelligently */
		/* XXX - dissect MAC packets if frame type is 0 */
		if (!ndo->ndo_eflag)
			token_hdr_print(ndo, trp, length + TOKEN_HDRLEN + route_len,
			    srcmac, dstmac);
		if (!ndo->ndo_suppress_default_print)
			ND_DEFAULTPRINT(p, caplen);
	}
	return (hdr_len);
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the TR header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
token_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	ndo->ndo_protocol = "token-ring";
	ndo->ndo_ll_hdr_len += token_print(ndo, p, h->len, h->caplen);
}
