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
 *
 * Original code by Hannes Gredler (hannes@juniper.net)
 */

#define NETDISSECT_REWORKED
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include "interface.h"
#include "extract.h"

void
sip_print(netdissect_options *ndo,
          register const u_char *pptr, register u_int len)
{
    u_int idx;

    ND_PRINT((ndo, "SIP, length: %u%s", len, ndo->ndo_vflag ? "\n\t" : ""));

    /* in non-verbose mode just lets print the protocol and length */
    if (ndo->ndo_vflag < 1)
        return;

    for (idx = 0; idx < len; idx++) {
        ND_TCHECK2(*(pptr+idx), 2);
        if (EXTRACT_16BITS(pptr+idx) != 0x0d0a) { /* linefeed ? */
            safeputchar(ndo, *(pptr + idx));
        } else {
            ND_PRINT((ndo, "\n\t"));
            idx+=1;
        }
    }

    /* do we want to see an additionally hexdump ? */
    if (ndo->ndo_vflag > 1)
        print_unknown_data(ndo, pptr, "\n\t", len);

    return;

trunc:
    ND_PRINT((ndo, "[|sip]"));
}
