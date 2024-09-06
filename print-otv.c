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
 * Original code by Francesco Fondelli (francesco dot fondelli, gmail dot com)
 */

/* \summary: Overlay Transport Virtualization (OTV) printer */

/* specification: draft-hasmit-otv-04 */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"

#define OTV_HDR_LEN 8

/*
 * OTV header, draft-hasmit-otv-04
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |R|R|R|R|I|R|R|R|           Overlay ID                          |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |          Instance ID                          | Reserved      |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

void
otv_print(netdissect_options *ndo, const u_char *bp, u_int len)
{
    uint8_t flags;

    ndo->ndo_protocol = "otv";
    ND_PRINT("OTV, ");
    if (len < OTV_HDR_LEN) {
        ND_PRINT("[length %u < %u]", len, OTV_HDR_LEN);
        goto invalid;
    }

    flags = GET_U_1(bp);
    ND_PRINT("flags [%s] (0x%02x), ", flags & 0x08 ? "I" : ".", flags);
    bp += 1;

    ND_PRINT("overlay %u, ", GET_BE_U_3(bp));
    bp += 3;

    ND_PRINT("instance %u\n", GET_BE_U_3(bp));
    bp += 3;

    /* Reserved */
    ND_TCHECK_1(bp);
    bp += 1;

    ether_print(ndo, bp, len - OTV_HDR_LEN, ND_BYTES_AVAILABLE_AFTER(bp), NULL, NULL);
    return;

invalid:
    nd_print_invalid(ndo);
    ND_TCHECK_LEN(bp, len);
}
