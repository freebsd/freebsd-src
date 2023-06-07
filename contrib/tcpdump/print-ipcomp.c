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

/* \summary: IP Payload Compression Protocol (IPComp) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

struct ipcomp {
	nd_uint8_t  comp_nxt;	/* Next Header */
	nd_uint8_t  comp_flags;	/* Length of data, in 32bit */
	nd_uint16_t comp_cpi;	/* Compression parameter index */
};

void
ipcomp_print(netdissect_options *ndo, const u_char *bp)
{
	const struct ipcomp *ipcomp;
	uint16_t cpi;

	ndo->ndo_protocol = "ipcomp";
	ipcomp = (const struct ipcomp *)bp;
	cpi = GET_BE_U_2(ipcomp->comp_cpi);

	ND_PRINT("IPComp(cpi=0x%04x)", cpi);

	/*
	 * XXX - based on the CPI, we could decompress the packet here.
	 * Packet buffer management is a headache (if we decompress,
	 * packet will become larger).
	 *
	 * We would decompress the packet and then call a routine that,
	 * based on ipcomp->comp_nxt, dissects the decompressed data.
	 *
	 * Until we do that, however, we just return -1, so that
	 * the loop that processes "protocol"/"next header" types
	 * stops - there's nothing more it can do with a compressed
	 * payload.
	 */
}
