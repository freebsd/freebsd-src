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

/* \summary: Marvell (Ethertype) Distributed Switch Architecture printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "ethertype.h"
#include "addrtoname.h"
#include "extract.h"

/*
 * Format of (Ethertyped or not) DSA tagged frames:
 *
 *      7   6   5   4   3   2   1   0
 *    .   .   .   .   .   .   .   .   .
 *  0 +---+---+---+---+---+---+---+---+
 *    |   Ether Destination Address   |
 * +6 +---+---+---+---+---+---+---+---+
 *    |     Ether Source Address      |
 * +6 +---+---+---+---+---+---+---+---+  +-
 *    |  Prog. DSA Ether Type [15:8]  |  | (8-byte) EDSA Tag
 * +1 +---+---+---+---+---+---+---+---+  | Contains a programmable Ether type,
 *    |  Prog. DSA Ether Type [7:0]   |  | two reserved bytes (always 0),
 * +1 +---+---+---+---+---+---+---+---+  | and a standard DSA tag.
 *    |     Reserved (0x00 0x00)      |  |
 * +2 +---+---+---+---+---+---+---+---+  |  +-
 *    | Mode  |b29|    Src/Trg Dev    |  |  | (4-byte) DSA Tag
 * +1 +---+---+---+---+---+---+---+---+  |  | Contains a DSA tag mode,
 *    |Src/Trg Port/Trunk |b18|b17|b16|  |  | source or target switch device,
 * +1 +---+---+---+---+---+---+---+---+  |  | source or target port or trunk,
 *    | PRI [2:0] |b12|  VID [11:8]   |  |  | and misc (IEEE and FPri) bits.
 * +1 +---+---+---+---+---+---+---+---+  |  |
 *    |           VID [7:0]           |  |  |
 * +1 +---+---+---+---+---+---+---+---+  +- +-
 *    |       Ether Length/Type       |
 * +2 +---+---+---+---+---+---+---+---+
 *    .   .   .   .   .   .   .   .   .
 *
 * Mode: Forward, To_CPU, From_CPU, To_Sniffer
 * b29: (Source or Target) IEEE Tag Mode
 * b18: Forward's Src_Is_Trunk, To_CPU's Code[2], To_Sniffer's Rx_Sniff
 * b17: To_CPU's Code[1]
 * b16: Original frame's CFI
 * b12: To_CPU's Code[0]
 */

#define TOK(tag, byte, mask, shift) ((GET_U_1(&(((const u_char *) tag)[byte])) & (mask)) >> (shift))

#define DSA_LEN 4
#define DSA_MODE(tag) TOK(tag, 0, 0xc0, 6)
#define  DSA_MODE_TO_CPU 0x0
#define  DSA_MODE_FROM_CPU 0x1
#define  DSA_MODE_TO_SNIFFER 0x2
#define  DSA_MODE_FORWARD 0x3
#define DSA_TAGGED(tag) TOK(tag, 0, 0x20, 5)
#define DSA_DEV(tag) TOK(tag, 0, 0x1f, 0)
#define DSA_PORT(tag) TOK(tag, 1, 0xf8, 3)
#define DSA_TRUNK(tag) TOK(tag, 1, 0x04, 2)
#define DSA_RX_SNIFF(tag) TOK(tag, 1, 0x04, 2)
#define DSA_CFI(tag) TOK(tag, 1, 0x01, 0)
#define DSA_PRI(tag) TOK(tag, 2, 0xe0, 5)
#define DSA_VID(tag) ((u_short)((TOK(tag, 2, 0x0f, 0) << 8) | (TOK(tag, 3, 0xff, 0))))
#define DSA_CODE(tag) ((TOK(tag, 1, 0x06, 1) << 1) | TOK(tag, 2, 0x10, 4))

#define EDSA_LEN 8

static const struct tok dsa_mode_values[] = {
	{ DSA_MODE_TO_CPU, "To CPU" },
	{ DSA_MODE_FROM_CPU, "From CPU" },
	{ DSA_MODE_TO_SNIFFER, "To Sniffer"},
	{ DSA_MODE_FORWARD, "Forward" },
	{ 0, NULL }
};

static const struct tok dsa_code_values[] = {
	{ 0x0, "BPDU (MGMT) Trap" },
	{ 0x1, "Frame2Reg" },
	{ 0x2, "IGMP/MLD Trap" },
	{ 0x3, "Policy Trap" },
	{ 0x4, "ARP Mirror" },
	{ 0x5, "Policy Mirror" },
	{ 0, NULL }
};

static void
tag_common_print(netdissect_options *ndo, const u_char *p)
{
	if (ndo->ndo_eflag) {
		ND_PRINT("mode %s, ", tok2str(dsa_mode_values, "unknown", DSA_MODE(p)));

		switch (DSA_MODE(p)) {
		case DSA_MODE_FORWARD:
			ND_PRINT("dev %u, %s %u, ", DSA_DEV(p),
				 DSA_TRUNK(p) ? "trunk" : "port", DSA_PORT(p));
			break;
		case DSA_MODE_FROM_CPU:
			ND_PRINT("target dev %u, port %u, ",
				 DSA_DEV(p), DSA_PORT(p));
			break;
		case DSA_MODE_TO_CPU:
			ND_PRINT("source dev %u, port %u, ",
				 DSA_DEV(p), DSA_PORT(p));
			ND_PRINT("code %s, ",
				 tok2str(dsa_code_values, "reserved", DSA_CODE(p)));
			break;
		case DSA_MODE_TO_SNIFFER:
			ND_PRINT("source dev %u, port %u, ",
				 DSA_DEV(p), DSA_PORT(p));
			ND_PRINT("%s sniff, ",
				 DSA_RX_SNIFF(p) ? "ingress" : "egress");
			break;
		default:
			break;
		}

		ND_PRINT("%s, ", DSA_TAGGED(p) ? "tagged" : "untagged");
		ND_PRINT("%s", DSA_CFI(p) ? "CFI, " : "");
		ND_PRINT("VID %u, ", DSA_VID(p));
		ND_PRINT("FPri %u, ", DSA_PRI(p));
	} else {
		switch (DSA_MODE(p)) {
		case DSA_MODE_FORWARD:
			ND_PRINT("Forward %s %u.%u, ",
				 DSA_TRUNK(p) ? "trunk" : "port",
				 DSA_DEV(p), DSA_PORT(p));
			break;
		case DSA_MODE_FROM_CPU:
			ND_PRINT("CPU > port %u.%u, ",
				 DSA_DEV(p), DSA_PORT(p));
			break;
		case DSA_MODE_TO_CPU:
			ND_PRINT("port %u.%u > CPU, ",
				 DSA_DEV(p), DSA_PORT(p));
			break;
		case DSA_MODE_TO_SNIFFER:
			ND_PRINT("port %u.%u > %s Sniffer, ",
				 DSA_DEV(p), DSA_PORT(p),
				 DSA_RX_SNIFF(p) ? "Rx" : "Tx");
			break;
		default:
			break;
		}

		ND_PRINT("VLAN %u%c, ", DSA_VID(p), DSA_TAGGED(p) ? 't' : 'u');
	}
}

static void
dsa_tag_print(netdissect_options *ndo, const u_char *bp)
{
	if (ndo->ndo_eflag)
		ND_PRINT("Marvell DSA ");
	else
		ND_PRINT("DSA ");
	tag_common_print(ndo, bp);
}

static void
edsa_tag_print(netdissect_options *ndo, const u_char *bp)
{
	const u_char *p = bp;
	uint16_t edsa_etype;

	edsa_etype = GET_BE_U_2(p);
	if (ndo->ndo_eflag) {
		ND_PRINT("Marvell EDSA ethertype 0x%04x (%s), ", edsa_etype,
			 tok2str(ethertype_values, "Unknown", edsa_etype));
		ND_PRINT("rsvd %u %u, ", GET_U_1(p + 2), GET_U_1(p + 3));
	} else
		ND_PRINT("EDSA 0x%04x, ", edsa_etype);
	p += 4;
	tag_common_print(ndo, p);
}

void
dsa_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;

	ndo->ndo_protocol = "dsa";
	ndo->ndo_ll_hdr_len +=
		ether_switch_tag_print(ndo, p, length, caplen, dsa_tag_print, DSA_LEN);
}

void
edsa_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;

	ndo->ndo_protocol = "edsa";
	ndo->ndo_ll_hdr_len +=
		ether_switch_tag_print(ndo, p, length, caplen, edsa_tag_print, EDSA_LEN);
}
