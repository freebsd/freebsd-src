/*
 * Copyright (c) 1989, 1990, 1991, 1993, 1994, 1995, 1996
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
	"@(#)$Header: print-sl.c,v 1.38 96/07/15 18:23:25 leres Exp $ (LBL)";
#endif

#ifdef HAVE_NET_SLIP_H
#include <sys/param.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#if __STDC__
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/ethernet.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <net/slcompress.h>
#include <net/slip.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

static u_int lastlen[2][256];
static u_int lastconn = 255;

static void sliplink_print(const u_char *, const struct ip *, u_int);
static void compressed_sl_print(const u_char *, const struct ip *, u_int, int);

/* XXX BSD/OS 2.1 compatibility */
#if !defined(SLIP_HDRLEN) && defined(SLC_BPFHDR)
#define SLIP_HDRLEN SLC_BPFHDR
#define SLX_DIR 0
#define SLX_CHDR (SLC_BPFHDRLEN - 1)
#define CHDR_LEN (SLC_BPFHDR - SLC_BPFHDRLEN)
#endif

void
sl_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	register u_int caplen = h->caplen;
	register u_int length = h->len;
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
	       register u_int length)
{
	int dir;
	u_int hlen;

	dir = p[SLX_DIR];
	putchar(dir == SLIPDIR_IN ? 'I' : 'O');
	putchar(' ');

	if (nflag) {
		/* XXX just dump the header */
		register int i;

		for (i = SLX_CHDR; i < SLX_CHDR + CHDR_LEN - 1; ++i)
			printf("%02x.", p[i]);
		printf("%02x: ", p[SLX_CHDR + CHDR_LEN - 1]);
		return;
	}
	switch (p[SLX_CHDR] & 0xf0) {

	case TYPE_IP:
		printf("ip %d: ", length + SLIP_HDRLEN);
		break;

	case TYPE_UNCOMPRESSED_TCP:
		/*
		 * The connection id is stored in the IP protcol field.
		 * Get it from the link layer since sl_uncompress_tcp()
		 * has restored the IP header copy to IPPROTO_TCP.
		 */
		lastconn = ((struct ip *)&p[SLX_CHDR])->ip_p;
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
		i = EXTRACT_16BITS(cp);
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
		i = EXTRACT_16BITS(cp);
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
		    u_int length, int dir)
{
	register const u_char *cp = chdr;
	register u_int flags, hlen;

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
	hlen += ((struct tcphdr *)&((int32_t *)ip)[hlen])->th_off;
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
