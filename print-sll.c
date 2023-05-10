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

/* \summary: Linux cooked sockets capture printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_NET_IF_H
/*
 * Include diag-control.h before <net/if.h>, which too defines a macro
 * named ND_UNREACHABLE.
 */
#include "diag-control.h"
#include <net/if.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "extract.h"

/*
 * For captures on Linux cooked sockets, we construct a fake header
 * that includes:
 *
 *	a 2-byte "packet type" which is one of:
 *
 *		LINUX_SLL_HOST		packet was sent to us
 *		LINUX_SLL_BROADCAST	packet was broadcast
 *		LINUX_SLL_MULTICAST	packet was multicast
 *		LINUX_SLL_OTHERHOST	packet was sent to somebody else
 *		LINUX_SLL_OUTGOING	packet was sent *by* us;
 *
 *	a 2-byte Ethernet protocol field;
 *
 *	a 2-byte link-layer type;
 *
 *	a 2-byte link-layer address length;
 *
 *	an 8-byte source link-layer address, whose actual length is
 *	specified by the previous value.
 *
 * All fields except for the link-layer address are in network byte order.
 *
 * DO NOT change the layout of this structure, or change any of the
 * LINUX_SLL_ values below.  If you must change the link-layer header
 * for a "cooked" Linux capture, introduce a new DLT_ type (ask
 * "tcpdump-workers@lists.tcpdump.org" for one, so that you don't give it
 * a value that collides with a value already being used), and use the
 * new header in captures of that type, so that programs that can
 * handle DLT_LINUX_SLL captures will continue to handle them correctly
 * without any change, and so that capture files with different headers
 * can be told apart and programs that read them can dissect the
 * packets in them.
 *
 * This structure, and the #defines below, must be the same in the
 * libpcap and tcpdump versions of "sll.h".
 */

/*
 * A DLT_LINUX_SLL fake link-layer header.
 */
#define SLL_HDR_LEN	16		/* total header length */
#define SLL_ADDRLEN	8		/* length of address field */

struct sll_header {
	nd_uint16_t	sll_pkttype;	/* packet type */
	nd_uint16_t	sll_hatype;	/* link-layer address type */
	nd_uint16_t	sll_halen;	/* link-layer address length */
	nd_byte		sll_addr[SLL_ADDRLEN];	/* link-layer address */
	nd_uint16_t	sll_protocol;	/* protocol */
};

/*
 * A DLT_LINUX_SLL2 fake link-layer header.
 */
#define SLL2_HDR_LEN	20		/* total header length */

struct sll2_header {
	nd_uint16_t	sll2_protocol;		/* protocol */
	nd_uint16_t	sll2_reserved_mbz;	/* reserved - must be zero */
	nd_uint32_t	sll2_if_index;		/* 1-based interface index */
	nd_uint16_t	sll2_hatype;		/* link-layer address type */
	nd_uint8_t	sll2_pkttype;		/* packet type */
	nd_uint8_t	sll2_halen;		/* link-layer address length */
	nd_byte		sll2_addr[SLL_ADDRLEN];	/* link-layer address */
};

/*
 * The LINUX_SLL_ values for "sll_pkttype"; these correspond to the
 * PACKET_ values on Linux, but are defined here so that they're
 * available even on systems other than Linux, and so that they
 * don't change even if the PACKET_ values change.
 */
#define LINUX_SLL_HOST		0
#define LINUX_SLL_BROADCAST	1
#define LINUX_SLL_MULTICAST	2
#define LINUX_SLL_OTHERHOST	3
#define LINUX_SLL_OUTGOING	4

/*
 * The LINUX_SLL_ values for "sll_protocol"; these correspond to the
 * ETH_P_ values on Linux, but are defined here so that they're
 * available even on systems other than Linux.  We assume, for now,
 * that the ETH_P_ values won't change in Linux; if they do, then:
 *
 *	if we don't translate them in "pcap-linux.c", capture files
 *	won't necessarily be readable if captured on a system that
 *	defines ETH_P_ values that don't match these values;
 *
 *	if we do translate them in "pcap-linux.c", that makes life
 *	unpleasant for the BPF code generator, as the values you test
 *	for in the kernel aren't the values that you test for when
 *	reading a capture file, so the fixup code run on BPF programs
 *	handed to the kernel ends up having to do more work.
 *
 * Add other values here as necessary, for handling packet types that
 * might show up on non-Ethernet, non-802.x networks.  (Not all the ones
 * in the Linux "if_ether.h" will, I suspect, actually show up in
 * captures.)
 */
#define LINUX_SLL_P_802_3	0x0001	/* Novell 802.3 frames without 802.2 LLC header */
#define LINUX_SLL_P_802_2	0x0004	/* 802.2 frames (not D/I/X Ethernet) */

static const struct tok sll_pkttype_values[] = {
    { LINUX_SLL_HOST, "In" },
    { LINUX_SLL_BROADCAST, "B" },
    { LINUX_SLL_MULTICAST, "M" },
    { LINUX_SLL_OTHERHOST, "P" },
    { LINUX_SLL_OUTGOING, "Out" },
    { 0, NULL}
};

static void
sll_print(netdissect_options *ndo, const struct sll_header *sllp, u_int length)
{
	u_short ether_type;

	ndo->ndo_protocol = "sll";
        ND_PRINT("%3s ",
		 tok2str(sll_pkttype_values,"?",GET_BE_U_2(sllp->sll_pkttype)));

	/*
	 * XXX - check the link-layer address type value?
	 * For now, we just assume 6 means Ethernet.
	 * XXX - print others as strings of hex?
	 */
	if (GET_BE_U_2(sllp->sll_halen) == MAC_ADDR_LEN)
		ND_PRINT("%s ", GET_ETHERADDR_STRING(sllp->sll_addr));

	if (!ndo->ndo_qflag) {
		ether_type = GET_BE_U_2(sllp->sll_protocol);

		if (ether_type <= MAX_ETHERNET_LENGTH_VAL) {
			/*
			 * Not an Ethernet type; what type is it?
			 */
			switch (ether_type) {

			case LINUX_SLL_P_802_3:
				/*
				 * Ethernet_802.3 IPX frame.
				 */
				ND_PRINT("802.3");
				break;

			case LINUX_SLL_P_802_2:
				/*
				 * 802.2.
				 */
				ND_PRINT("802.2");
				break;

			default:
				/*
				 * What is it?
				 */
				ND_PRINT("ethertype Unknown (0x%04x)",
				    ether_type);
				break;
			}
		} else {
			ND_PRINT("ethertype %s (0x%04x)",
			    tok2str(ethertype_values, "Unknown", ether_type),
			    ether_type);
		}
		ND_PRINT(", length %u: ", length);
	}
}

/*
 * This is the top level routine of the printer.  'p' points to the
 * Linux "cooked capture" header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
sll_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	const struct sll_header *sllp;
	u_short hatype;
	u_short ether_type;
	int llc_hdrlen;
	u_int hdrlen;

	ndo->ndo_protocol = "sll";
	ND_TCHECK_LEN(p, SLL_HDR_LEN);

	sllp = (const struct sll_header *)p;

	if (ndo->ndo_eflag)
		sll_print(ndo, sllp, length);

	/*
	 * Go past the cooked-mode header.
	 */
	length -= SLL_HDR_LEN;
	caplen -= SLL_HDR_LEN;
	p += SLL_HDR_LEN;
	hdrlen = SLL_HDR_LEN;

	hatype = GET_BE_U_2(sllp->sll_hatype);
	switch (hatype) {

	case 803:
		/*
		 * This is an packet with a radiotap header;
		 * just dissect the payload as such.
		 */
		ndo->ndo_ll_hdr_len += SLL_HDR_LEN;
		ndo->ndo_ll_hdr_len += ieee802_11_radio_print(ndo, p, length, caplen);
		return;
	}
	ether_type = GET_BE_U_2(sllp->sll_protocol);

recurse:
	/*
	 * Is it (gag) an 802.3 encapsulation, or some non-Ethernet
	 * packet type?
	 */
	if (ether_type <= MAX_ETHERNET_LENGTH_VAL) {
		/*
		 * Yes - what type is it?
		 */
		switch (ether_type) {

		case LINUX_SLL_P_802_3:
			/*
			 * Ethernet_802.3 IPX frame.
			 */
			ipx_print(ndo, p, length);
			break;

		case LINUX_SLL_P_802_2:
			/*
			 * 802.2.
			 * Try to print the LLC-layer header & higher layers.
			 */
			llc_hdrlen = llc_print(ndo, p, length, caplen, NULL, NULL);
			if (llc_hdrlen < 0)
				goto unknown;	/* unknown LLC type */
			hdrlen += llc_hdrlen;
			break;

		default:
			/*FALLTHROUGH*/

		unknown:
			/* packet type not known, print raw packet */
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
			break;
		}
	} else if (ether_type == ETHERTYPE_8021Q) {
		/*
		 * Print VLAN information, and then go back and process
		 * the enclosed type field.
		 */
		if (caplen < 4) {
			ndo->ndo_protocol = "vlan";
			nd_print_trunc(ndo);
			ndo->ndo_ll_hdr_len += hdrlen + caplen;
			return;
		}
	        if (ndo->ndo_eflag) {
			uint16_t tag = GET_BE_U_2(p);

			ND_PRINT("%s, ", ieee8021q_tci_string(tag));
		}

		ether_type = GET_BE_U_2(p + 2);
		if (ether_type <= MAX_ETHERNET_LENGTH_VAL)
			ether_type = LINUX_SLL_P_802_2;
		if (!ndo->ndo_qflag) {
			ND_PRINT("ethertype %s, ",
			    tok2str(ethertype_values, "Unknown", ether_type));
		}
		p += 4;
		length -= 4;
		caplen -= 4;
		hdrlen += 4;
		goto recurse;
	} else {
		if (ethertype_print(ndo, ether_type, p, length, caplen, NULL, NULL) == 0) {
			/* ether_type not known, print raw packet */
			if (!ndo->ndo_eflag)
				sll_print(ndo, sllp, length + SLL_HDR_LEN);
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
		}
	}

	ndo->ndo_ll_hdr_len += hdrlen;
}

static void
sll2_print(netdissect_options *ndo, const struct sll2_header *sllp, u_int length)
{
	u_short ether_type;

	ndo->ndo_protocol = "sll2";
	ND_PRINT("ifindex %u ", GET_BE_U_4(sllp->sll2_if_index));

	/*
	 * XXX - check the link-layer address type value?
	 * For now, we just assume 6 means Ethernet.
	 * XXX - print others as strings of hex?
	 */
	if (GET_U_1(sllp->sll2_halen) == MAC_ADDR_LEN)
		ND_PRINT("%s ", GET_ETHERADDR_STRING(sllp->sll2_addr));

	if (!ndo->ndo_qflag) {
		ether_type = GET_BE_U_2(sllp->sll2_protocol);

		if (ether_type <= MAX_ETHERNET_LENGTH_VAL) {
			/*
			 * Not an Ethernet type; what type is it?
			 */
			switch (ether_type) {

			case LINUX_SLL_P_802_3:
				/*
				 * Ethernet_802.3 IPX frame.
				 */
				ND_PRINT("802.3");
				break;

			case LINUX_SLL_P_802_2:
				/*
				 * 802.2.
				 */
				ND_PRINT("802.2");
				break;

			default:
				/*
				 * What is it?
				 */
				ND_PRINT("ethertype Unknown (0x%04x)",
				    ether_type);
				break;
			}
		} else {
			ND_PRINT("ethertype %s (0x%04x)",
			    tok2str(ethertype_values, "Unknown", ether_type),
			    ether_type);
		}
		ND_PRINT(", length %u: ", length);
	}
}

/*
 * This is the top level routine of the printer.  'p' points to the
 * Linux "cooked capture" header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
sll2_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	const struct sll2_header *sllp;
	u_short hatype;
	u_short ether_type;
	int llc_hdrlen;
	u_int hdrlen;
#ifdef HAVE_NET_IF_H
	uint32_t if_index;
	char ifname[IF_NAMESIZE];
#endif

	ndo->ndo_protocol = "sll2";
	ND_TCHECK_LEN(p, SLL2_HDR_LEN);

	sllp = (const struct sll2_header *)p;
#ifdef HAVE_NET_IF_H
	if_index = GET_BE_U_4(sllp->sll2_if_index);
	if (!if_indextoname(if_index, ifname))
		strncpy(ifname, "?", 2);
	ND_PRINT("%-5s ", ifname);
#endif

	ND_PRINT("%-3s ",
		 tok2str(sll_pkttype_values, "?", GET_U_1(sllp->sll2_pkttype)));

	if (ndo->ndo_eflag)
		sll2_print(ndo, sllp, length);

	/*
	 * Go past the cooked-mode header.
	 */
	length -= SLL2_HDR_LEN;
	caplen -= SLL2_HDR_LEN;
	p += SLL2_HDR_LEN;
	hdrlen = SLL2_HDR_LEN;

	hatype = GET_BE_U_2(sllp->sll2_hatype);
	switch (hatype) {

	case 803:
		/*
		 * This is an packet with a radiotap header;
		 * just dissect the payload as such.
		 */
		ndo->ndo_ll_hdr_len += SLL2_HDR_LEN;
		ndo->ndo_ll_hdr_len += ieee802_11_radio_print(ndo, p, length, caplen);
		return;
	}
	ether_type = GET_BE_U_2(sllp->sll2_protocol);

recurse:
	/*
	 * Is it (gag) an 802.3 encapsulation, or some non-Ethernet
	 * packet type?
	 */
	if (ether_type <= MAX_ETHERNET_LENGTH_VAL) {
		/*
		 * Yes - what type is it?
		 */
		switch (ether_type) {

		case LINUX_SLL_P_802_3:
			/*
			 * Ethernet_802.3 IPX frame.
			 */
			ipx_print(ndo, p, length);
			break;

		case LINUX_SLL_P_802_2:
			/*
			 * 802.2.
			 * Try to print the LLC-layer header & higher layers.
			 */
			llc_hdrlen = llc_print(ndo, p, length, caplen, NULL, NULL);
			if (llc_hdrlen < 0)
				goto unknown;	/* unknown LLC type */
			hdrlen += llc_hdrlen;
			break;

		default:
			/*FALLTHROUGH*/

		unknown:
			/* packet type not known, print raw packet */
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
			break;
		}
	} else if (ether_type == ETHERTYPE_8021Q) {
		/*
		 * Print VLAN information, and then go back and process
		 * the enclosed type field.
		 */
		if (caplen < 4) {
			ndo->ndo_protocol = "vlan";
			nd_print_trunc(ndo);
			ndo->ndo_ll_hdr_len += hdrlen + caplen;
			return;
		}
	        if (ndo->ndo_eflag) {
			uint16_t tag = GET_BE_U_2(p);

			ND_PRINT("%s, ", ieee8021q_tci_string(tag));
		}

		ether_type = GET_BE_U_2(p + 2);
		if (ether_type <= MAX_ETHERNET_LENGTH_VAL)
			ether_type = LINUX_SLL_P_802_2;
		if (!ndo->ndo_qflag) {
			ND_PRINT("ethertype %s, ",
			    tok2str(ethertype_values, "Unknown", ether_type));
		}
		p += 4;
		length -= 4;
		caplen -= 4;
		hdrlen += 4;
		goto recurse;
	} else {
		if (ethertype_print(ndo, ether_type, p, length, caplen, NULL, NULL) == 0) {
			/* ether_type not known, print raw packet */
			if (!ndo->ndo_eflag)
				sll2_print(ndo, sllp, length + SLL2_HDR_LEN);
			if (!ndo->ndo_suppress_default_print)
				ND_DEFAULTPRINT(p, caplen);
		}
	}

	ndo->ndo_ll_hdr_len += hdrlen;
}
