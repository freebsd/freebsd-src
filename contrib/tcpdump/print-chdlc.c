/* maybe it should be merged into print-ppp.c */
/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
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
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-chdlc.c,v 1.13 2001/09/17 21:57:57 fenner Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "extract.h"
#include "ppp.h"
#include "chdlc.h"

static void chdlc_slarp_print(const u_char *, u_int);

/* Standard CHDLC printer */
void
chdlc_if_print(u_char *user, const struct pcap_pkthdr *h,
	     register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;

	++infodelay;
	ts_print(&h->ts);

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	chdlc_print(p, length, caplen);

	putchar('\n');
	--infodelay;
	if (infoprint)
		info(0);
}

void
chdlc_print(register const u_char *p, u_int length, u_int caplen)
{
	const struct ip *ip;
	u_int proto;

	if (caplen < CHDLC_HDRLEN) {
		printf("[|chdlc]");
		return;
	}

	proto = EXTRACT_16BITS(&p[2]);
	if (eflag) {
		switch (p[0]) {
		case CHDLC_UNICAST:
			printf("unicast ");
			break;
		case CHDLC_BCAST:
			printf("bcast ");
			break;
		default:
			printf("0x%02x ", p[0]);
			break;
		}
		printf("%d %04x: ", length, proto);
	}

	length -= CHDLC_HDRLEN;
	ip = (const struct ip *)(p + CHDLC_HDRLEN);
	switch (proto) {
	case ETHERTYPE_IP:
		ip_print((const u_char *)ip, length);
		break;
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6_print((const u_char *)ip, length);
		break;
#endif
	case CHDLC_TYPE_SLARP:
		chdlc_slarp_print((const u_char *)ip, length);
		break;
#if 0
	case CHDLC_TYPE_CDP:
		chdlc_cdp_print((const u_char *)ip, length);
		break;
#endif
	}
	if (xflag)
		default_print((const u_char *)ip, caplen - CHDLC_HDRLEN);
}

struct cisco_slarp {
	u_int32_t code;
#define SLARP_REQUEST	0
#define SLARP_REPLY	1
#define SLARP_KEEPALIVE	2
	union {
		struct {
			struct in_addr addr;
			struct in_addr mask;
			u_int16_t unused[3];
		} addr;
		struct {
			u_int32_t myseq;
			u_int32_t yourseq;
			u_int16_t rel;
			u_int16_t t1;
			u_int16_t t2;
		} keep;
	} un;
};

#define SLARP_LEN	18

static void
chdlc_slarp_print(const u_char *cp, u_int length)
{
	const struct cisco_slarp *slarp;

	if (length < SLARP_LEN) {
		printf("[|slarp]");
		return;
	}

	slarp = (const struct cisco_slarp *)cp;
	switch (ntohl(slarp->code)) {
	case SLARP_REQUEST:
		printf("slarp-request");
		break;
	case SLARP_REPLY:
		printf("slarp-reply %s/%s",
			ipaddr_string(&slarp->un.addr.addr),
			ipaddr_string(&slarp->un.addr.mask));
		break;
	case SLARP_KEEPALIVE:
		printf("slarp-keepalive my=0x%x your=0x%x ",
			(u_int32_t)ntohl(slarp->un.keep.myseq),
			(u_int32_t)ntohl(slarp->un.keep.yourseq));
		printf("reliability=0x%04x t1=%d.%d",
			ntohs(slarp->un.keep.rel), ntohs(slarp->un.keep.t1),
			ntohs(slarp->un.keep.t2));
		break;
	default:
		printf("slarp-0x%x unknown", (u_int32_t)ntohl(slarp->code));
		break;
	}

	if (SLARP_LEN < length && vflag)
		printf("(trailing junk: %d bytes)", length - SLARP_LEN);
}
