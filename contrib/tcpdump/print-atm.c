/*
 * Copyright (c) 1994, 1995, 1996
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
static char rcsid[] =
    "@(#) $Header: print-atm.c,v 1.7 96/07/23 14:17:21 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>
#include <pcap.h>

#include "addrtoname.h"
#include "ethertype.h"
#include "interface.h"

/*
 * This is the top level routine of the printer.  'p' is the points
 * to the LLC/SNAP header of the packet, 'tvp' is the timestamp,
 * 'length' is the length of the packet off the wire, and 'caplen'
 * is the number of bytes actually captured.
 */
void
atm_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	u_short ethertype;

	ts_print(&h->ts);

	if (caplen < 8) {
		printf("[|atm]");
		goto out;
	}
	if (p[0] != 0xaa || p[1] != 0xaa || p[2] != 0x03) {
		/*XXX assume 802.6 MAC header from fore driver */
		if (eflag)
			printf("%04x%04x %04x%04x ",
			       p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3],
			       p[4] << 24 | p[5] << 16 | p[6] << 8 | p[7],
			       p[8] << 24 | p[9] << 16 | p[10] << 8 | p[11],
			       p[12] << 24 | p[13] << 16 | p[14] << 8 | p[15]);
		p += 20;
		length -= 20;
		caplen -= 20;
	}
	ethertype = p[6] << 8 | p[7];
	if (eflag)
		printf("%02x %02x %02x %02x-%02x-%02x %04x: ",
		       p[0], p[1], p[2], /* dsap/ssap/ctrl */
		       p[3], p[4], p[5], /* manufacturer's code */
		       ethertype);

	/*
	 * Some printers want to get back at the ethernet addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	length -= 8;
	caplen -= 8;
	p += 8;

	switch (ethertype) {

	case ETHERTYPE_IP:
		ip_print(p, length);
		break;

		/*XXX this probably isn't right */
	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		arp_print(p, length, caplen);
		break;
#ifdef notyet
	case ETHERTYPE_DN:
		decnet_print(p, length, caplen);
		break;

	case ETHERTYPE_ATALK:
		if (vflag)
			fputs("et1 ", stdout);
		atalk_print(p, length);
		break;

	case ETHERTYPE_AARP:
		aarp_print(p, length);
		break;

	case ETHERTYPE_LAT:
	case ETHERTYPE_MOPRC:
	case ETHERTYPE_MOPDL:
		/* default_print for now */
#endif
	default:
		/* ether_type not known, print raw packet */
		if (!eflag)
			printf("%02x %02x %02x %02x-%02x-%02x %04x: ",
			       p[0], p[1], p[2], /* dsap/ssap/ctrl */
			       p[3], p[4], p[5], /* manufacturer's code */
			       ethertype);
		if (!xflag && !qflag)
			default_print(p, caplen);
	}
	if (xflag)
		default_print(p, caplen);
 out:
	putchar('\n');
}
