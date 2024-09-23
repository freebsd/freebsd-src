/*	$NetBSD: print-ah.c,v 1.4 1996/05/20 00:41:16 fvdl Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994
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

/* \summary: IPSEC Authentication Header printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

#include "ah.h"

int
ah_print(netdissect_options *ndo, const u_char *bp)
{
	const struct ah *ah;
	uint8_t ah_len;
	u_int ah_hdr_len;
	uint16_t reserved;
	const u_char *p;

	ndo->ndo_protocol = "ah";
	ah = (const struct ah *)bp;

	nd_print_protocol_caps(ndo);
/*
 * RFC4302
 *
 * 2.2.  Payload Length
 *
 *    This 8-bit field specifies the length of AH in 32-bit words (4-byte
 *    units), minus "2".
 */
	ah_len = GET_U_1(ah->ah_len);
	ah_hdr_len = (ah_len + 2) * 4;

	ND_PRINT("(");
	if (ndo->ndo_vflag)
		ND_PRINT("length=%u(%u-bytes),", ah_len, ah_hdr_len);
	reserved = GET_BE_U_2(ah->ah_reserved);
	if (reserved)
		ND_PRINT("reserved=0x%x[MustBeZero],", reserved);
	ND_PRINT("spi=0x%08x,", GET_BE_U_4(ah->ah_spi));
	ND_PRINT("seq=0x%x,", GET_BE_U_4(ah->ah_seq));
	ND_PRINT("icv=0x");
	for (p = (const u_char *)(ah + 1); p < bp + ah_hdr_len; p++)
		ND_PRINT("%02x", GET_U_1(p));
	ND_PRINT("): ");

	return ah_hdr_len;
}
