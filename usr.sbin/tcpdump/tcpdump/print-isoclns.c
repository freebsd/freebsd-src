/*
 * Copyright (c) 1992, 1993, 1994
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

/*
 * Original code by Matt Thomas, Digital Equipment Corporation
 */

#ifndef lint
static char rcsid[] =
    "@(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-isoclns.c,v 1.1 1995/03/08 12:52:35 olah Exp $ (LBL)";
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"

#define	CLNS	129
#define	ESIS	130
#define	ISIS	131
#define	NULLNS	0

static int osi_cksum(const u_char *, int, const u_char *, u_char *, u_char *);
static void esis_print(const u_char *, int);

void
isoclns_print(const u_char *p, int length, int caplen,
	      const u_char *esrc, const u_char *edst)
{
	if (caplen < 1) {
		printf("[|iso-clns] ");
		if (!eflag)
			printf("%s > %s",
			       etheraddr_string(esrc),
			       etheraddr_string(edst));
		return;
	}

	switch (*p) {

	case CLNS:
		/* esis_print(&p, &length); */
		printf("iso-clns");
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		break;

	case ESIS:
		printf("iso-esis");
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		esis_print(p, length);
		return;

	case ISIS:
		printf("iso-isis");
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		/* isis_print(&p, &length); */
		(void)printf(" len=%d ", length);
		if (caplen > 1)
			default_print_unaligned(p, caplen);
		break;

	case NULLNS:
		printf("iso-nullns");
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		break;

	default:
		printf("iso-clns %02x", p[0]);
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		(void)printf(" len=%d ", length);
		if (caplen > 1)
			default_print_unaligned(p, caplen);
		break;
	}
}

#define	ESIS_REDIRECT	6
#define	ESIS_ESH	2
#define	ESIS_ISH	4

struct esis_hdr {
	u_char version;
	u_char reserved;
	u_char type;
	u_char tmo[2];
	u_char cksum[2];
};

static void
esis_print(const u_char *p, int length)
{
	const u_char *ep;
	int li = p[1];
	const struct esis_hdr *eh = (const struct esis_hdr *) &p[2];
	u_char cksum[2];
	u_char off[2];

	if (length == 2) {
		if (qflag)
			printf(" bad pkt!");
		else
			printf(" no header at all!");
		return;
	}
	ep = p + li;
	if (li > length) {
		if (qflag)
			printf(" bad pkt!");
		else
			printf(" LI(%d) > PDU size (%d)!", li, length);
		return;
	}
	if (li < sizeof(struct esis_hdr) + 2) {
		if (qflag)
			printf(" bad pkt!");
		else {
			printf(" too short for esis header %d:", li);
			while (--length >= 0)
				printf("%02X", *p++);
		}
		return;
	}
	switch (eh->type & 0x1f) {

	case ESIS_REDIRECT:
		printf(" redirect");
		break;

	case ESIS_ESH:
		printf(" esh");
		break;

	case ESIS_ISH:
		printf(" ish");
		break;

	default:
		printf(" type %d", eh->type & 0x1f);
		break;
	}
	off[0] = eh->cksum[0];
	off[1] = eh->cksum[1];
	if (vflag && osi_cksum(p, li, eh->cksum, cksum, off)) {
		printf(" bad cksum (got %02x%02x want %02x%02x)",
		       eh->cksum[1], eh->cksum[0], cksum[1], cksum[0]);
		return;
	}
	if (eh->version != 1) {
		printf(" unsupported version %d", eh->version);
		return;
	}
	p += sizeof(*eh) + 2;
	li -= sizeof(*eh) + 2;	/* protoid * li */

	switch (eh->type & 0x1f) {
	case ESIS_REDIRECT: {
		const u_char *dst, *snpa, *is;

		dst = p; p += *p + 1;
		if (p > snapend)
			return;
		printf(" %s", isonsap_string(dst));
		snpa = p; p += *p + 1;
		is = p;   p += *p + 1;
		if (p > snapend)
			return;
		if (p > ep) {
			printf(" [bad li]");
			return;
		}
		if (is[0] == 0)
			printf(" > %s", etheraddr_string(&snpa[1]));
		else
			printf(" > %s", isonsap_string(is));
		li = ep - p;
		break;
	}
#if 0
	case ESIS_ESH:
		printf(" esh");
		break;
#endif
	case ESIS_ISH: {
		const u_char *is;

		is = p; p += *p + 1;
		if (p > ep) {
			printf(" [bad li]");
			return;
		}
		if (p > snapend)
			return;
		printf(" %s", isonsap_string(is));
		li = ep - p;
		break;
	}

	default:
		(void)printf(" len=%d", length);
		if (length && p < snapend) {
			length = snapend - p;
			default_print(p, length);
		}
		return;
	}
	if (vflag)
		while (p < ep && li) {
			int op, opli;
			const u_char *q;

			if (snapend - p < 2)
				return;
			if (li < 2) {
				printf(" bad opts/li");
				return;
			}
			op = *p++;
			opli = *p++;
			li -= 2;
			if (opli > li) {
				printf(" opt (%d) too long", op);
				return;
			}
			li -= opli;
			q = p;
			p += opli;
			if (snapend < p)
				return;
			if (op == 198 && opli == 2) {
				printf(" tmo=%d", q[0] * 256 + q[1]);
				continue;
			}
			printf (" %d:<", op);
			while (--opli >= 0)
				printf("%02x", *q++);
			printf (">");
		}
}

static int
osi_cksum(register const u_char *p, register int len,
	  const u_char *toff, u_char *cksum, u_char *off)
{
	int x, y, f = (len - ((toff - p) + 1));
	long c0 = 0, c1 = 0;

	if ((cksum[0] = off[0]) == 0 && (cksum[1] = off[1]) == 0)
		return 0;

	off[0] = off[1] = 0;
	while (--len >= 0) {
		c0 += *p++;
		c1 += c0;
		c0 %= 255;
		c1 %= 255;
	}
	x = (c0 * f - c1);
	if (x < 0)
		x = 255 - (-x % 255);
	else
		x %= 255;
	y = -1 * (x + c0);
	if (y < 0)
		y = 255 - (-y % 255);
	else
		y %= 255;

	off[0] = x;
	off[1] = y;

	return (off[0] != cksum[0] || off[1] != cksum[1]);
}
