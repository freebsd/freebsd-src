/*
 * Copyright (c) 1996, 1997
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
 * Initial contribution from Francis Dupont (francis.dupont@inria.fr)
 */

/* \summary: Interior Gateway Routing Protocol (IGRP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

/* Cisco IGRP definitions */

/* IGRP Header */

struct igrphdr {
	nd_uint8_t ig_vop;	/* protocol version number / opcode */
#define IGRP_V(x)	(((x) & 0xf0) >> 4)
#define IGRP_OP(x)	((x) & 0x0f)
	nd_uint8_t ig_ed;	/* edition number */
	nd_uint16_t ig_as;	/* autonomous system number */
	nd_uint16_t ig_ni;	/* number of subnet in local net */
	nd_uint16_t ig_ns;	/* number of networks in AS */
	nd_uint16_t ig_nx;	/* number of networks outside AS */
	nd_uint16_t ig_sum;	/* checksum of IGRP header & data */
};

#define IGRP_UPDATE	1
#define IGRP_REQUEST	2

/* IGRP routing entry */

struct igrprte {
	nd_byte igr_net[3];	/* 3 significant octets of IP address */
	nd_uint24_t igr_dly;	/* delay in tens of microseconds */
	nd_uint24_t igr_bw;	/* bandwidth in units of 1 kb/s */
	nd_uint16_t igr_mtu;	/* MTU in octets */
	nd_uint8_t igr_rel;	/* percent packets successfully tx/rx */
	nd_uint8_t igr_ld;	/* percent of channel occupied */
	nd_uint8_t igr_hct;	/* hop count */
};

#define IGRP_RTE_SIZE	14	/* sizeof() is accurate now */

static void
igrp_entry_print(netdissect_options *ndo, const struct igrprte *igr)
{
	u_int delay, bandwidth;
	u_int metric, mtu;

	delay = GET_BE_U_3(igr->igr_dly);
	bandwidth = GET_BE_U_3(igr->igr_bw);
	metric = ND_MIN(bandwidth + delay, 0xffffff);
	mtu = GET_BE_U_2(igr->igr_mtu);

	ND_PRINT(" d=%u b=%u r=%u l=%u M=%u mtu=%u in %u hops",
	    10 * delay, bandwidth == 0 ? 0 : 10000000 / bandwidth,
	    GET_U_1(igr->igr_rel), GET_U_1(igr->igr_ld), metric,
	    mtu, GET_U_1(igr->igr_hct));
}

static const struct tok op2str[] = {
	{ IGRP_UPDATE,		"update" },
	{ IGRP_REQUEST,		"request" },
	{ 0,			NULL }
};

void
igrp_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
	const struct igrphdr *hdr;
	const u_char *cp;
	u_int nint, nsys, next;
	uint16_t cksum;

	ndo->ndo_protocol = "igrp";
	hdr = (const struct igrphdr *)bp;
	cp = (const u_char *)(hdr + 1);
	ND_PRINT("igrp:");

	/* Header */
	nint = GET_BE_U_2(hdr->ig_ni);
	nsys = GET_BE_U_2(hdr->ig_ns);
	next = GET_BE_U_2(hdr->ig_nx);

	ND_PRINT(" %s V%u edit=%u AS=%u (%u/%u/%u)",
	    tok2str(op2str, "op-#%u", IGRP_OP(GET_U_1(hdr->ig_vop))),
	    IGRP_V(GET_U_1(hdr->ig_vop)),
	    GET_U_1(hdr->ig_ed),
	    GET_BE_U_2(hdr->ig_as),
	    nint,
	    nsys,
	    next);
	cksum = GET_BE_U_2(hdr->ig_sum);
	if (ndo->ndo_vflag)
		ND_PRINT(" checksum=0x%04x", cksum);

	length -= sizeof(*hdr);
	while (length >= IGRP_RTE_SIZE) {
		const struct igrprte *igr = (const struct igrprte *)cp;
		uint8_t net0 = GET_U_1(&igr->igr_net[0]);
		uint8_t net1 = GET_U_1(&igr->igr_net[1]);
		uint8_t net2 = GET_U_1(&igr->igr_net[2]);

		if (nint > 0) {
			ND_PRINT(" *.%u.%u.%u", net0, net1, net2);
			igrp_entry_print(ndo, igr);
			--nint;
		} else if (nsys > 0) {
			ND_PRINT(" %u.%u.%u.0", net0, net1, net2);
			igrp_entry_print(ndo, igr);
			--nsys;
		} else if (next > 0) {
			ND_PRINT(" X%u.%u.%u.0", net0, net1, net2);
			igrp_entry_print(ndo, igr);
			--next;
		} else {
			ND_PRINT(" [extra bytes %u]", length);
			break;
		}
		cp += IGRP_RTE_SIZE;
		length -= IGRP_RTE_SIZE;
	}
	if (nint || nsys || next || length)
		nd_print_invalid(ndo);
}
