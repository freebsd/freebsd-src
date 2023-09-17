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

/* \summary: Broadcom Ethernet switches tag (4 bytes) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "ethertype.h"
#include "addrtoname.h"
#include "extract.h"

#define ETHER_TYPE_LEN		2

#define BRCM_TAG_LEN		4
#define BRCM_OPCODE_SHIFT	5
#define BRCM_OPCODE_MASK	0x7

/* Ingress fields */
#define BRCM_IG_TC_SHIFT	2
#define BRCM_IG_TC_MASK		0x7
#define BRCM_IG_TE_MASK		0x3
#define BRCM_IG_TS_SHIFT	7
#define BRCM_IG_DSTMAP_MASK	0x1ff

/* Egress fields */
#define BRCM_EG_CID_MASK	0xff
#define BRCM_EG_RC_MASK		0xff
#define  BRCM_EG_RC_RSVD	(3 << 6)
#define  BRCM_EG_RC_EXCEPTION	(1 << 5)
#define  BRCM_EG_RC_PROT_SNOOP	(1 << 4)
#define  BRCM_EG_RC_PROT_TERM	(1 << 3)
#define  BRCM_EG_RC_SWITCH	(1 << 2)
#define  BRCM_EG_RC_MAC_LEARN	(1 << 1)
#define  BRCM_EG_RC_MIRROR	(1 << 0)
#define BRCM_EG_TC_SHIFT	5
#define BRCM_EG_TC_MASK		0x7
#define BRCM_EG_PID_MASK	0x1f

static const struct tok brcm_tag_te_values[] = {
	{ 0, "None" },
	{ 1, "Untag" },
	{ 2, "Header"},
	{ 3, "Reserved" },
	{ 0, NULL }
};

static const struct tok brcm_tag_rc_values[] = {
	{ 1, "mirror" },
	{ 2, "MAC learning" },
	{ 4, "switching" },
	{ 8, "prot term" },
	{ 16, "prot snoop" },
	{ 32, "exception" },
	{ 0, NULL }
};

static void
brcm_tag_print(netdissect_options *ndo, const u_char *bp)
{
	uint8_t tag[BRCM_TAG_LEN];
	uint16_t dst_map;
	unsigned int i;

	for (i = 0; i < BRCM_TAG_LEN; i++)
		tag[i] = GET_U_1(bp + i);

	ND_PRINT("BRCM tag OP: %s", tag[0] ? "IG" : "EG");
	if (tag[0] & (1 << BRCM_OPCODE_SHIFT)) {
		/* Ingress Broadcom tag */
		ND_PRINT(", TC: %d", (tag[1] >> BRCM_IG_TC_SHIFT) &
			 BRCM_IG_TC_MASK);
		ND_PRINT(", TE: %s",
			 tok2str(brcm_tag_te_values, "unknown",
				 (tag[1] & BRCM_IG_TE_MASK)));
		ND_PRINT(", TS: %d", tag[1] >> BRCM_IG_TS_SHIFT);
		dst_map = (uint16_t)tag[2] << 8 | tag[3];
		ND_PRINT(", DST map: 0x%04x", dst_map & BRCM_IG_DSTMAP_MASK);
	} else {
		/* Egress Broadcom tag */
		ND_PRINT(", CID: %d", tag[1]);
		ND_PRINT(", RC: %s", tok2str(brcm_tag_rc_values,
			 "reserved", tag[2]));
		ND_PRINT(", TC: %d", (tag[3] >> BRCM_EG_TC_SHIFT) &
			 BRCM_EG_TC_MASK);
		ND_PRINT(", port: %d", tag[3] & BRCM_EG_PID_MASK);
	}
	ND_PRINT(", ");
}

void
brcm_tag_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h,
		  const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;

	ndo->ndo_protocol = "brcm-tag";
	ndo->ndo_ll_hdr_len +=
		ether_switch_tag_print(ndo, p, length, caplen,
				       brcm_tag_print, BRCM_TAG_LEN);
}

void
brcm_tag_prepend_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h,
			  const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;

	ndo->ndo_protocol = "brcm-tag-prepend";
	ND_TCHECK_LEN(p, BRCM_TAG_LEN);
	ndo->ndo_ll_hdr_len += BRCM_TAG_LEN;

	if (ndo->ndo_eflag) {
		/* Print the prepended Broadcom tag. */
		brcm_tag_print(ndo, p);
	}
	p += BRCM_TAG_LEN;
	length -= BRCM_TAG_LEN;
	caplen -= BRCM_TAG_LEN;

	/*
	 * Now print the Ethernet frame following it.
	 */
	ndo->ndo_ll_hdr_len +=
		ether_print(ndo, p, length, caplen, NULL, NULL);
}
