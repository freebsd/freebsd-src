/*
 * Copyright (c) 1989, 1990, 1991, 1993, 1994, 1995, 1996, 1997
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

/* \summary: Compressed Serial Line Internet Protocol printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"

#include "ip.h"
#include "tcp.h"
#include "slcompress.h"

/*
 * definitions of the pseudo- link-level header attached to slip
 * packets grabbed by the packet filter (bpf) traffic monitor.
 */
#define SLIP_HDRLEN 16

#define SLX_DIR 0
#define SLX_CHDR 1

#define SLIPDIR_IN 0
#define SLIPDIR_OUT 1


static u_int lastlen[2][256];
static u_int lastconn = 255;

static void sliplink_print(netdissect_options *, const u_char *, const struct ip *, u_int);
static void compressed_sl_print(netdissect_options *, const u_char *, const struct ip *, u_int, int);

void
sl_if_print(netdissect_options *ndo,
            const struct pcap_pkthdr *h, const u_char *p)
{
	u_int length = h->len;
	const struct ip *ip;

	ndo->ndo_protocol = "slip";
	ND_TCHECK_LEN(p, SLIP_HDRLEN);
	ndo->ndo_ll_hdr_len += SLIP_HDRLEN;

	length -= SLIP_HDRLEN;

	ip = (const struct ip *)(p + SLIP_HDRLEN);

	if (ndo->ndo_eflag)
		sliplink_print(ndo, p, ip, length);

	switch (IP_V(ip)) {
	case 4:
	        ip_print(ndo, (const u_char *)ip, length);
		break;
	case 6:
		ip6_print(ndo, (const u_char *)ip, length);
		break;
	default:
		ND_PRINT("ip v%u", IP_V(ip));
	}
}

void
sl_bsdos_if_print(netdissect_options *ndo,
                  const struct pcap_pkthdr *h, const u_char *p)
{
	u_int length = h->len;
	const struct ip *ip;

	ndo->ndo_protocol = "slip_bsdos";
	ND_TCHECK_LEN(p, SLIP_HDRLEN);
	ndo->ndo_ll_hdr_len += SLIP_HDRLEN;

	length -= SLIP_HDRLEN;

	ip = (const struct ip *)(p + SLIP_HDRLEN);

#ifdef notdef
	if (ndo->ndo_eflag)
		sliplink_print(ndo, p, ip, length);
#endif

	ip_print(ndo, (const u_char *)ip, length);
}

static void
sliplink_print(netdissect_options *ndo,
               const u_char *p, const struct ip *ip,
               u_int length)
{
	int dir;
	u_int hlen;

	dir = GET_U_1(p + SLX_DIR);
	switch (dir) {

	case SLIPDIR_IN:
		ND_PRINT("I ");
		break;

	case SLIPDIR_OUT:
		ND_PRINT("O ");
		break;

	default:
		ND_PRINT("Invalid direction %d ", dir);
		dir = -1;
		break;
	}
	switch (GET_U_1(p + SLX_CHDR) & 0xf0) {

	case TYPE_IP:
		ND_PRINT("ip %u: ", length + SLIP_HDRLEN);
		break;

	case TYPE_UNCOMPRESSED_TCP:
		/*
		 * The connection id is stored in the IP protocol field.
		 * Get it from the link layer since sl_uncompress_tcp()
		 * has restored the IP header copy to IPPROTO_TCP.
		 */
		lastconn = GET_U_1(((const struct ip *)(p + SLX_CHDR))->ip_p);
		ND_PRINT("utcp %u: ", lastconn);
		if (dir == -1) {
			/* Direction is bogus, don't use it */
			return;
		}
		ND_TCHECK_SIZE(ip);
		hlen = IP_HL(ip);
		ND_TCHECK_SIZE((const struct tcphdr *)&((const int *)ip)[hlen]);
		hlen += TH_OFF((const struct tcphdr *)&((const int *)ip)[hlen]);
		lastlen[dir][lastconn] = length - (hlen << 2);
		break;

	default:
		if (dir == -1) {
			/* Direction is bogus, don't use it */
			return;
		}
		if (GET_U_1(p + SLX_CHDR) & TYPE_COMPRESSED_TCP) {
			compressed_sl_print(ndo, p + SLX_CHDR, ip, length, dir);
			ND_PRINT(": ");
		} else
			ND_PRINT("slip-%u!: ", GET_U_1(p + SLX_CHDR));
	}
}

static const u_char *
print_sl_change(netdissect_options *ndo,
                const char *str, const u_char *cp)
{
	u_int i;

	if ((i = GET_U_1(cp)) == 0) {
		cp++;
		i = GET_BE_U_2(cp);
		cp += 2;
	}
	ND_PRINT(" %s%u", str, i);
	return (cp);
}

static const u_char *
print_sl_winchange(netdissect_options *ndo,
                   const u_char *cp)
{
	int16_t i;

	if ((i = GET_U_1(cp)) == 0) {
		cp++;
		i = GET_BE_S_2(cp);
		cp += 2;
	}
	if (i >= 0)
		ND_PRINT(" W+%d", i);
	else
		ND_PRINT(" W%d", i);
	return (cp);
}

static void
compressed_sl_print(netdissect_options *ndo,
                    const u_char *chdr, const struct ip *ip,
                    u_int length, int dir)
{
	const u_char *cp = chdr;
	u_int flags, hlen;

	flags = GET_U_1(cp);
	cp++;
	if (flags & NEW_C) {
		lastconn = GET_U_1(cp);
		cp++;
		ND_PRINT("ctcp %u", lastconn);
	} else
		ND_PRINT("ctcp *");

	/* skip tcp checksum */
	cp += 2;

	switch (flags & SPECIALS_MASK) {
	case SPECIAL_I:
		ND_PRINT(" *SA+%u", lastlen[dir][lastconn]);
		break;

	case SPECIAL_D:
		ND_PRINT(" *S+%u", lastlen[dir][lastconn]);
		break;

	default:
		if (flags & NEW_U)
			cp = print_sl_change(ndo, "U=", cp);
		if (flags & NEW_W)
			cp = print_sl_winchange(ndo, cp);
		if (flags & NEW_A)
			cp = print_sl_change(ndo, "A+", cp);
		if (flags & NEW_S)
			cp = print_sl_change(ndo, "S+", cp);
		break;
	}
	if (flags & NEW_I)
		cp = print_sl_change(ndo, "I+", cp);

	/*
	 * 'hlen' is the length of the uncompressed TCP/IP header (in words).
	 * 'cp - chdr' is the length of the compressed header.
	 * 'length - hlen' is the amount of data in the packet.
	 */
	ND_TCHECK_SIZE(ip);
	hlen = IP_HL(ip);
	ND_TCHECK_SIZE((const struct tcphdr *)&((const int32_t *)ip)[hlen]);
	hlen += TH_OFF((const struct tcphdr *)&((const int32_t *)ip)[hlen]);
	lastlen[dir][lastconn] = length - (hlen << 2);
	ND_PRINT(" %u (%ld)", lastlen[dir][lastconn], (long)(cp - chdr));
}
