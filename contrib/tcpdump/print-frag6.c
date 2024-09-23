/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994
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

/* \summary: IPv6 fragmentation header printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

#include "ip6.h"

int
frag6_print(netdissect_options *ndo, const u_char *bp, const u_char *bp2)
{
	const struct ip6_frag *dp;
	const struct ip6_hdr *ip6;

	ndo->ndo_protocol = "frag6";
	dp = (const struct ip6_frag *)bp;
	ip6 = (const struct ip6_hdr *)bp2;

	ND_PRINT("frag (");
	if (ndo->ndo_vflag)
		ND_PRINT("0x%08x:", GET_BE_U_4(dp->ip6f_ident));
	ND_PRINT("%u|", GET_BE_U_2(dp->ip6f_offlg) & IP6F_OFF_MASK);
	if ((bp - bp2) + sizeof(struct ip6_frag) >
	    sizeof(struct ip6_hdr) + GET_BE_U_2(ip6->ip6_plen))
		ND_PRINT("[length < 0] (invalid))");
	else
		ND_PRINT("%zu)",
			 sizeof(struct ip6_hdr) + GET_BE_U_2(ip6->ip6_plen) -
			 (bp - bp2) - sizeof(struct ip6_frag));

	/* it is meaningless to decode non-first fragment */
	if ((GET_BE_U_2(dp->ip6f_offlg) & IP6F_OFF_MASK) != 0)
		return -1;
	else {
		ND_PRINT(" ");
		return sizeof(struct ip6_frag);
	}
}
