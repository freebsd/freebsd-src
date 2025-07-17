/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 */

/* \summary: Secure Shell (SSH) printer */

#include <config.h>

#include "netdissect-stdinc.h"
#include "netdissect-ctype.h"

#include "netdissect.h"
#include "extract.h"

static int
ssh_print_version(netdissect_options *ndo, const u_char *pptr, u_int len)
{
	u_int idx = 0;

	if ( GET_U_1(pptr+idx) != 'S' )
		return 0;
	idx++;
	if ( GET_U_1(pptr+idx) != 'S' )
		return 0;
	idx++;
	if ( GET_U_1(pptr+idx) != 'H' )
		return 0;
	idx++;
	if ( GET_U_1(pptr+idx) != '-' )
		return 0;
	idx++;

	while (idx < len) {
		u_char c;

		c = GET_U_1(pptr + idx);
		if (c == '\n') {
			/*
			 * LF without CR; end of line.
			 * Skip the LF and print the line, with the
			 * exception of the LF.
			 */
			goto print;
		} else if (c == '\r') {
			/* CR - any LF? */
			if ((idx+1) >= len) {
				/* not in this packet */
				goto trunc;
			}
			if (GET_U_1(pptr + idx + 1) == '\n') {
				/*
				 * CR-LF; end of line.
				 * Skip the CR-LF and print the line, with
				 * the exception of the CR-LF.
				 */
				goto print;
			}

			/*
			 * CR followed by something else; treat this as
			 * if it were binary data and don't print it.
			 */
			goto trunc;
		} else if (!ND_ASCII_ISPRINT(c) ) {
			/*
			 * Not a printable ASCII character; treat this
			 * as if it were binary data and don't print it.
			 */
			goto trunc;
		}
		idx++;
	}
trunc:
	return -1;
print:
	ND_PRINT(": ");
	nd_print_protocol_caps(ndo);
	ND_PRINT(": %.*s", (int)idx, pptr);
	return idx;
}

void
ssh_print(netdissect_options *ndo, const u_char *pptr, u_int len)
{
	ndo->ndo_protocol = "ssh";

	ssh_print_version(ndo, pptr, len);
}
