/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
	"@(#)$Header: print-sl.c,v 1.17 91/10/07 20:18:35 leres Exp $ (LBL)";
#endif

#ifdef CSLIP
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/mbuf.h>
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

#include <net/slcompress.h>
#include <net/slip.h>
#include <net/bpf.h>

#include "interface.h"
#include "addrtoname.h"

static int lastlen[2][256];
static int lastconn = 255;

static void compressed_sl_print();

void
sl_if_print(p, tvp, length, caplen)
	u_char *p;
	struct timeval *tvp;
	int length;
	int caplen;
{
	struct ip *ip;

	ts_print(tvp);

	if (caplen < SLIP_HDRLEN) {
		printf("[|slip]");
		goto out;
	}
	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = (u_char *)p;
	snapend = (u_char *)p + caplen;

	length -= SLIP_HDRLEN;

	ip = (struct ip *)(p + SLIP_HDRLEN);

	if (eflag)
		sliplink_print(p, ip, length);

	ip_print(ip, length);

	if (xflag)
		default_print((u_short *)ip, caplen - SLIP_HDRLEN);
 out:
	putchar('\n');
}

sliplink_print(p, ip, length)
	u_char *p;
	struct ip *ip;
	int length;
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
			compressed_sl_print(&p[SLX_CHDR], ip, length, dir);
			printf(": ");
		} else
			printf("slip-%d!: ", p[SLX_CHDR]);
	}
}

static u_char *
print_sl_change(str, cp)
	char *str;
	register u_char *cp;
{
	register u_int i;

	if ((i = *cp++) == 0) {
		i = (cp[0] << 8) | cp[1];
		cp += 2;
	}
	printf(" %s%d", str, i);
	return (cp);
}

static u_char *
print_sl_winchange(cp)
	register u_char *cp;
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
compressed_sl_print(chdr, ip, length, dir)
	u_char *chdr;
	int length;
	struct ip *ip;
	int dir;
{
	register u_char *cp = chdr;
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
	 * 'hlen' is the length of the uncompressed TCP/IP header (in longs).
	 * 'cp - chdr' is the length of the compressed header.
	 * 'length - hlen' is the amount of data in the packet.
	 */
	hlen = ip->ip_hl;
	hlen += ((struct tcphdr *)&((long *)ip)[hlen])->th_off;
	lastlen[dir][lastconn] = length - (hlen << 2);
	printf(" %d (%d)", lastlen[dir][lastconn], cp - chdr);
}
#else
#include <stdio.h>
void
sl_if_print()
{
	void error();

	error("not configured for slip");
	/* NOTREACHED */
}
#endif
