/*
 * Copyright (c) 1991, 1993, 1994
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
    "@(#)$Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-null.c,v 1.2 1995/03/08 12:52:38 olah Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>


#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "pcap.h"

#define	NULL_HDRLEN 4

static void
null_print(const u_char *p, const struct ip *ip, int length)
{
	u_int family;

	bcopy(p, &family, sizeof(family));

	if (nflag) {
		/* XXX just dump the header */
		return;
	}
	switch (family) {

	case AF_INET:
		printf("ip: ");
		break;

	case AF_NS:
		printf("ns: ");
		break;

	default:
		printf("AF %d: ", family);
		break;
	}
}

void
null_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	int length = h->len;
	int caplen = h->caplen;
	const struct ip *ip;

	ts_print(&h->ts);

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	length -= NULL_HDRLEN;

	ip = (struct ip *)(p + NULL_HDRLEN);

	if (eflag)
		null_print(p, ip, length);

	ip_print((const u_char *)ip, length);

	if (xflag)
		default_print((const u_char *)ip, caplen - NULL_HDRLEN);
	putchar('\n');
}

