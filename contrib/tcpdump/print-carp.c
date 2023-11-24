/*	$OpenBSD: print-carp.c,v 1.6 2009/10/27 23:59:55 deraadt Exp $	*/

/*
 * Copyright (c) 2000 William C. Fenner.
 *                All rights reserved.
 *
 * Kevin Steves <ks@hp.se> July 2000
 * Modified to:
 * - print version, type string and packet length
 * - print IP address count if > 1 (-v)
 * - verify checksum (-v)
 * - print authentication string (-v)
 *
 * Copyright (c) 2011 Advanced Computing Technologies
 * George V. Neille-Neil
 *
 * Modified to:
 * - work correctly with CARP
 * - compile into the latest tcpdump
 * - print out the counter
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * The name of William C. Fenner may not be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.  THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 */

/* \summary: Common Address Redundancy Protocol (CARP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h" /* for checksum structure and functions */
#include "extract.h"

void
carp_print(netdissect_options *ndo, const u_char *bp, u_int len, u_int ttl)
{
	u_int version, type;
	const char *type_s;

	ndo->ndo_protocol = "carp";
	version = (GET_U_1(bp) & 0xf0) >> 4;
	type = GET_U_1(bp) & 0x0f;
	if (type == 1)
		type_s = "advertise";
	else
		type_s = "unknown";
	ND_PRINT("CARPv%u-%s %u: ", version, type_s, len);
	if (ttl != 255)
		ND_PRINT("[ttl=%u!] ", ttl);
	if (version != 2 || type != 1)
		return;
	ND_PRINT("vhid=%u advbase=%u advskew=%u authlen=%u ",
	    GET_U_1(bp + 1), GET_U_1(bp + 5), GET_U_1(bp + 2),
	    GET_U_1(bp + 3));
	if (ndo->ndo_vflag) {
		struct cksum_vec vec[1];
		vec[0].ptr = (const uint8_t *)bp;
		vec[0].len = len;
		if (ND_TTEST_LEN(bp, len) && in_cksum(vec, 1))
			ND_PRINT(" (bad carp cksum %x!)",
				GET_BE_U_2(bp + 6));
	}
	ND_PRINT("counter=%" PRIu64, GET_BE_U_8(bp + 8));
}
