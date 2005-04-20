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
#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-sll.c,v 1.12.2.2 2003/11/16 08:51:44 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>
#include <pcap.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"

#include "ether.h"
#include "sll.h"

static inline void
sll_print(register const struct sll_header *sllp, u_int length)
{
	u_short halen;

	switch (ntohs(sllp->sll_pkttype)) {

	case LINUX_SLL_HOST:
		(void)printf("< ");
		break;

	case LINUX_SLL_BROADCAST:
		(void)printf("B ");
		break;

	case LINUX_SLL_MULTICAST:
		(void)printf("M ");
		break;

	case LINUX_SLL_OTHERHOST:
		(void)printf("P ");
		break;

	case LINUX_SLL_OUTGOING:
		(void)printf("> ");
		break;

	default:
		(void)printf("? ");
		break;
	}

	/*
	 * XXX - check the link-layer address type value?
	 * For now, we just assume 6 means Ethernet.
	 * XXX - print others as strings of hex?
	 */
	halen = ntohs(sllp->sll_halen);
	if (halen == 6)
		(void)printf("%s ", etheraddr_string(sllp->sll_addr));

	if (!qflag)
		(void)printf("%s ", etherproto_string(sllp->sll_protocol));
	(void)printf("%d: ", length);
}

/*
 * This is the top level routine of the printer.  'p' points to the
 * Linux "cooked capture" header of the packet, 'h->ts' is the timestamp,
 * 'h->length' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
u_int
sll_if_print(const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	register const struct sll_header *sllp;
	u_short ether_type;
	u_short extracted_ethertype;

	if (caplen < SLL_HDR_LEN) {
		/*
		 * XXX - this "can't happen" because "pcap-linux.c" always
		 * adds this many bytes of header to every packet in a
		 * cooked socket capture.
		 */
		printf("[|sll]");
		return (caplen);
	}

	sllp = (const struct sll_header *)p;

	if (eflag)
		sll_print(sllp, length);

	/*
	 * Go past the cooked-mode header.
	 */
	length -= SLL_HDR_LEN;
	caplen -= SLL_HDR_LEN;
	p += SLL_HDR_LEN;

	ether_type = ntohs(sllp->sll_protocol);

	/*
	 * Is it (gag) an 802.3 encapsulation, or some non-Ethernet
	 * packet type?
	 */
	extracted_ethertype = 0;
	if (ether_type <= ETHERMTU) {
		/*
		 * Yes - what type is it?
		 */
		switch (ether_type) {

		case LINUX_SLL_P_802_3:
			/*
			 * Ethernet_802.3 IPX frame.
			 */
			ipx_print(p, length);
			break;

		case LINUX_SLL_P_802_2:
			/*
			 * 802.2.
			 * Try to print the LLC-layer header & higher layers.
			 */
			if (llc_print(p, length, caplen, NULL, NULL,
			    &extracted_ethertype) == 0)
				goto unknown;	/* unknown LLC type */
			break;

		default:
		unknown:
			/* ether_type not known, print raw packet */
			if (!eflag)
				sll_print(sllp, length + SLL_HDR_LEN);
			if (extracted_ethertype) {
				printf("(LLC %s) ",
			       etherproto_string(htons(extracted_ethertype)));
			}
			if (!xflag && !qflag)
				default_print(p, caplen);
			break;
		}
	} else if (ether_encap_print(ether_type, p, length, caplen,
	    &extracted_ethertype) == 0) {
		/* ether_type not known, print raw packet */
		if (!eflag)
			sll_print(sllp, length + SLL_HDR_LEN);
		if (!xflag && !qflag)
			default_print(p, caplen);
	}

	return (SLL_HDR_LEN);
}
