/*
 * Copyright (c) 1989, 1990, 1991, 1993, 1994
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
static  char rcsid[] =
	"@(#)$Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-sl.c,v 1.2 1995/03/08 12:52:41 olah Exp $ (LBL)";
#endif

#ifdef CSLIP
#include <sys/param.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

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

#include <net/slcompress.h>
#include <net/slip.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"

static int lastlen[2][256];
static int lastconn = 255;

static void sliplink_print(const u_char *, const struct ip *, int);
static void compressed_sl_print(const u_char *, const struct ip *, int, int);

void
sl_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	register int caplen = h->caplen;
	register int length = h->len;
	register const struct ip *ip;

	ts_print(&h->ts);

	if (caplen < SLIP_HDRLEN) {
		printf("[|slip]");
		goto out;
	}
	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	length -= SLIP_HDRLEN;

	ip = (struct ip *)(p + SLIP_HDRLEN);

	if (eflag)
		sliplink_print(p, ip, length);

	ip_print((u_char *)ip, length);

	if (xflag)
		default_print((u_char *)ip, caplen - SLIP_HDRLEN);
 out:
	putchar('\n');
}

static void
sliplink_print(register const u_char *p, register const struct ip *ip,
	       register int length)
{
	int dir;
	int hlen;

	dir = p[SLX_DIR];
	putchar(dir == SLIPDIR_IN ? 'I' : 'O');
	putchar(' ');

	if (nflag) {
		/* XXX just dump the header */
		int i;

		for (i = 0; i < 15; ++i)
			printf("%02x.", p[SLX_CHDR + i]);
		printf("%02x: ", p[SLX_CHDR + 15]);
		return;
	}
	switch (p[SLX_CHDR] & 0xf0) {

	case TYPE_IP:
		printf("ip %d: ", length + SLIP_HDRLEN);
		break;

	case TYPE_UNCOMPRESSED_TCP:
		/*
		 * The connection id is stode in the IP protcol field.
		 */
		lastconn = ip->ip_p;
		hlen = ip->ip_hl;
		hlen += ((struct tcphdr *)&((int *)ip)[hlen])->th_off;
		lastlen[dir][lastconn] = length - (hlen << 2);
		printf("utcp %d: ", lastconn);
		break;

	default:
		if (p[SLX_CHDR] & TYPE_COMPRESSED_TCP) {
			compressed_sl_print(&p[SLX_CHDR], ip,
			    length, dir);
			printf(": ");
		} else
			printf("slip-%d!: ", p[SLX_CHDR]);
	}
}

static const u_char *
print_sl_change(const char *str, register const u_char *cp)
{
	register u_int i;

	if ((i = *cp++) == 0) {
		i = (cp[0] << 8) | cp[1];
		cp += 2;
	}
	printf(" %s%d", str, i);
	return (cp);
}

static const u_char *
print_sl_winchange(register const u_char *cp)
{
	register short i;

	if ((i = *cp++) == 0) {
		i = (cp[0] << 8) | cp[1];
		cp += 2;
	}
	if (i >= 0)
		printf(" W+%d", i);
	else
		printf(" W%d", i);
	return (cp);
}

static void
compressed_sl_print(const u_char *chdr, const struct ip *ip,
		    int length, int dir)
{
	register const u_char *cp = chdr;
	register u_int flags;
	int hlen;

	flags = *cp++;
	if (flags & NEW_C) {
		lastconn = *cp++;
		printf("ctcp %d", lastconn);
	} else
		printf("ctcp *");

	/* skip tcp checksum */
	cp += 2;

	switch (flags & SPECIALS_MASK) {
	case SPECIAL_I:
		printf(" *SA+%d", lastlen[dir][lastconn]);
		break;

	case SPECIAL_D:
		printf(" *S+%d", lastlen[dir][lastconn]);
		break;

	default:
		if (flags & NEW_U)
			cp = print_sl_change("U=", cp);
		if (flags & NEW_W)
			cp = print_sl_winchange(cp);
		if (flags & NEW_A)
			cp = print_sl_change("A+", cp);
		if (flags & NEW_S)
			cp = print_sl_change("S+", cp);
		break;
	}
	if (flags & NEW_I)
		cp = print_sl_change("I+", cp);

	/*
	 * 'hlen' is the length of the uncompressed TCP/IP header (in words).
	 * 'cp - chdr' is the length of the compressed header.
	 * 'length - hlen' is the amount of data in the packet.
	 */
	hlen = ip->ip_hl;
	hlen += ((struct tcphdr *)&((int32 *)ip)[hlen])->th_off;
	lastlen[dir][lastconn] = length - (hlen << 2);
	printf(" %d (%d)", lastlen[dir][lastconn], cp - chdr);
}
#else
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>

#include "interface.h"
void
sl_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{

	error("not configured for slip");
	/* NOTREACHED */
}
#endif
