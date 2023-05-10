/*
 * Copyright (c) 2001 William C. Fenner.
 *                All rights reserved.
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
 */

/* \summary: Multicast Source Discovery Protocol (MSDP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#define MSDP_TYPE_MAX	7

void
msdp_print(netdissect_options *ndo, const u_char *sp, u_int length)
{
	unsigned int type, len;

	ndo->ndo_protocol = "msdp";
	ND_PRINT(": ");
	nd_print_protocol(ndo);
	/* See if we think we're at the beginning of a compound packet */
	type = GET_U_1(sp);
	len = GET_BE_U_2(sp + 1);
	if (len > 1500 || len < 3 || type == 0 || type > MSDP_TYPE_MAX)
		goto trunc;	/* not really truncated, but still not decodable */
	while (length != 0) {
		type = GET_U_1(sp);
		len = GET_BE_U_2(sp + 1);
		if (len > 1400 || ndo->ndo_vflag)
			ND_PRINT(" [len %u]", len);
		if (len < 3)
			goto trunc;
		if (length < len)
			goto trunc;
		sp += 3;
		length -= 3;
		switch (type) {
		case 1:	/* IPv4 Source-Active */
		case 3: /* IPv4 Source-Active Response */
			if (type == 1)
				ND_PRINT(" SA");
			else
				ND_PRINT(" SA-Response");
			ND_PRINT(" %u entries", GET_U_1(sp));
			if ((u_int)((GET_U_1(sp) * 12) + 8) < len) {
				ND_PRINT(" [w/data]");
				if (ndo->ndo_vflag > 1) {
					ND_PRINT(" ");
					ip_print(ndo, sp +
						 GET_U_1(sp) * 12 + 8 - 3,
						 len - (GET_U_1(sp) * 12 + 8));
				}
			}
			break;
		case 2:
			ND_PRINT(" SA-Request");
			ND_PRINT(" for %s", GET_IPADDR_STRING(sp + 1));
			break;
		case 4:
			ND_PRINT(" Keepalive");
			if (len != 3)
				ND_PRINT("[len=%u] ", len);
			break;
		case 5:
			ND_PRINT(" Notification");
			break;
		default:
			ND_PRINT(" [type=%u len=%u]", type, len);
			break;
		}
		sp += (len - 3);
		length -= (len - 3);
	}
	return;
trunc:
	nd_print_trunc(ndo);
}
