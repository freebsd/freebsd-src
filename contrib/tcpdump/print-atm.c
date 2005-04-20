/*
 * Copyright (c) 1994, 1995, 1996, 1997
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
    "@(#) $Header: /tcpdump/master/tcpdump/print-atm.c,v 1.33.2.2 2003/11/16 08:51:11 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <pcap.h>
#include <string.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "atm.h"
#include "atmuni31.h"
#include "llc.h"

#include "ether.h"

/*
 * Print an RFC 1483 LLC-encapsulated ATM frame.
 */
static void
atm_llc_print(const u_char *p, int length, int caplen)
{
	u_short extracted_ethertype;

	if (!llc_print(p, length, caplen, NULL, NULL,
	    &extracted_ethertype)) {
		/* ether_type not known, print raw packet */
		if (extracted_ethertype) {
			printf("(LLC %s) ",
		etherproto_string(htons(extracted_ethertype)));
		}
		if (!xflag && !qflag)
			default_print(p, caplen);
	}
}

/*
 * Given a SAP value, generate the LLC header value for a UI packet
 * with that SAP as the source and destination SAP.
 */
#define LLC_UI_HDR(sap)	((sap)<<16 | (sap<<8) | 0x03)

/*
 * This is the top level routine of the printer.  'p' points
 * to the LLC/SNAP header of the packet, 'h->ts' is the timestamp,
 * 'h->length' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
u_int
atm_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	u_int32_t llchdr;
	u_int hdrlen = 0;

	if (caplen < 8) {
		printf("[|atm]");
		return (caplen);
	}
	/*
	 * Extract the presumed LLC header into a variable, for quick
	 * testing.
	 * Then check for a header that's neither a header for a SNAP
	 * packet nor an RFC 2684 routed NLPID-formatted PDU nor
	 * an 802.2-but-no-SNAP IP packet.
	 */
	llchdr = EXTRACT_24BITS(p);
	if (llchdr != LLC_UI_HDR(LLCSAP_SNAP) &&
	    llchdr != LLC_UI_HDR(LLCSAP_ISONS) &&
	    llchdr != LLC_UI_HDR(LLCSAP_IP)) {
		/*
		 * XXX - assume 802.6 MAC header from Fore driver.
		 *
		 * Unfortunately, the above list doesn't check for
		 * all known SAPs, doesn't check for headers where
		 * the source and destination SAP aren't the same,
		 * and doesn't check for non-UI frames.  It also
		 * runs the risk of an 802.6 MAC header that happens
		 * to begin with one of those values being
		 * incorrectly treated as an 802.2 header.
		 *
		 * So is that Fore driver still around?  And, if so,
		 * is it still putting 802.6 MAC headers on ATM
		 * packets?  If so, could it be changed to use a
		 * new DLT_IEEE802_6 value if we added it?
		 */
		if (eflag)
			printf("%08x%08x %08x%08x ",
			       EXTRACT_32BITS(p),
			       EXTRACT_32BITS(p+4),
			       EXTRACT_32BITS(p+8),
			       EXTRACT_32BITS(p+12));
		p += 20;
		length -= 20;
		caplen -= 20;
		hdrlen += 20;
	}
	atm_llc_print(p, length, caplen);
	return (hdrlen);
}

/*
 * ATM signalling.
 */
static struct tok msgtype2str[] = {
	{ CALL_PROCEED,		"Call_proceeding" },
	{ CONNECT,		"Connect" },
	{ CONNECT_ACK,		"Connect_ack" },
	{ SETUP,		"Setup" },
	{ RELEASE,		"Release" },
	{ RELEASE_DONE,		"Release_complete" },
	{ RESTART,		"Restart" },
	{ RESTART_ACK,		"Restart_ack" },
	{ STATUS,		"Status" },
	{ STATUS_ENQ,		"Status_enquiry" },
	{ ADD_PARTY,		"Add_party" },
	{ ADD_PARTY_ACK,	"Add_party_ack" },
	{ ADD_PARTY_REJ,	"Add_party_reject" },
	{ DROP_PARTY,		"Drop_party" },
	{ DROP_PARTY_ACK,	"Drop_party_ack" },
	{ 0,			NULL }
};

static void
sig_print(const u_char *p, int caplen)
{
	bpf_u_int32 call_ref;

	if (caplen < PROTO_POS) {
		printf("[|atm]");
		return;
	}
	if (p[PROTO_POS] == Q2931) {
		/*
		 * protocol:Q.2931 for User to Network Interface 
		 * (UNI 3.1) signalling
		 */
		printf("Q.2931");
		if (caplen < MSG_TYPE_POS) {
			printf(" [|atm]");
			return;
		}
		printf(":%s ",
		    tok2str(msgtype2str, "msgtype#%d", p[MSG_TYPE_POS]));

		if (caplen < CALL_REF_POS+3) {
			printf("[|atm]");
			return;
		}
		call_ref = EXTRACT_24BITS(&p[CALL_REF_POS]);
		printf("CALL_REF:0x%06x", call_ref);
	} else {
		/* SCCOP with some unknown protocol atop it */
		printf("SSCOP, proto %d ", p[PROTO_POS]);
	}
}

/*
 * Print an ATM PDU (such as an AAL5 PDU).
 */
void
atm_print(u_int vpi, u_int vci, u_int traftype, const u_char *p, u_int length,
    u_int caplen)
{
	if (eflag)
		printf("VPI:%u VCI:%u ", vpi, vci);

	if (vpi == 0) {
		switch (vci) {

		case PPC:
			sig_print(p, caplen);
			return;

		case BCC:
			printf("broadcast sig: ");
			return;

		case OAMF4SC:
			printf("oamF4(segment): ");
			return;

		case OAMF4EC:
			printf("oamF4(end): ");
			return;

		case METAC:
			printf("meta: ");
			return;

		case ILMIC:
			printf("ilmi: ");
			snmp_print(p, length);
			return;
		}
	}

	switch (traftype) {

	case ATM_LLC:
	default:
		/*
		 * Assumes traffic is LLC if unknown.
		 */
		atm_llc_print(p, length, caplen);
		break;

	case ATM_LANE:
		lane_print(p, length, caplen);
		break;
	}
}
