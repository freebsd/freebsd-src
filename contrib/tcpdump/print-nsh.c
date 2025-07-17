/* Copyright (c) 2015, bugyo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* \summary: Network Service Header (NSH) printer */

/* specification: RFC 8300 */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"

static const struct tok nsh_flags [] = {
    { 0x2, "O" },
    { 0, NULL }
};

/*
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |Ver|O|U|    TTL    |   Length  |U|U|U|U|MD Type| Next Protocol |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
#define NSH_BASE_HDR_LEN 4
#define NSH_VER(x)       (((x) & 0xc0000000) >> 30)
#define NSH_FLAGS(x)     (((x) & 0x30000000) >> 28)
#define NSH_TTL(x)       (((x) & 0x0fc00000) >> 22)
#define NSH_LENGTH(x)    (((x) & 0x003f0000) >> 16)
#define NSH_MD_TYPE(x)   (((x) & 0x00000f00) >>  8)
#define NSH_NEXT_PROT(x) (((x) & 0x000000ff) >>  0)

#define NSH_SERVICE_PATH_HDR_LEN 4
#define NSH_HDR_WORD_SIZE 4U

#define MD_RSV   0x00
#define MD_TYPE1 0x01
#define MD_TYPE2 0x02
#define MD_EXP   0x0F
static const struct tok md_str[] = {
    { MD_RSV,   "reserved"     },
    { MD_TYPE1, "1"            },
    { MD_TYPE2, "2"            },
    { MD_EXP,   "experimental" },
    { 0, NULL }
};

#define NP_IPV4 0x01
#define NP_IPV6 0x02
#define NP_ETH  0x03
#define NP_NSH  0x04
#define NP_MPLS 0x05
#define NP_EXP1 0xFE
#define NP_EXP2 0xFF
static const struct tok np_str[] = {
    { NP_IPV4, "IPv4"         },
    { NP_IPV6, "IPv6"         },
    { NP_ETH,  "Ethernet"     },
    { NP_NSH,  "NSH"          },
    { NP_MPLS, "MPLS"         },
    { NP_EXP1, "Experiment 1" },
    { NP_EXP2, "Experiment 2" },
    { 0, NULL }
};

void
nsh_print(netdissect_options *ndo, const u_char *bp, u_int len)
{
    uint32_t basehdr;
    u_int ver, length, md_type;
    uint8_t next_protocol;
    u_char past_headers = 0;
    u_int next_len;

    ndo->ndo_protocol = "nsh";
    /*
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                Base Header                                    |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                Service Path Header                            |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |                                                               |
     *   ~                Context Header(s)                              ~
     *   |                                                               |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */

    /* print Base Header and Service Path Header */
    if (len < NSH_BASE_HDR_LEN + NSH_SERVICE_PATH_HDR_LEN) {
        ND_PRINT(" (packet length %u < %u)",
                 len, NSH_BASE_HDR_LEN + NSH_SERVICE_PATH_HDR_LEN);
        goto invalid;
    }

    basehdr = GET_BE_U_4(bp);
    bp += 4;
    ver = NSH_VER(basehdr);
    length = NSH_LENGTH(basehdr);
    md_type = NSH_MD_TYPE(basehdr);
    next_protocol = NSH_NEXT_PROT(basehdr);

    ND_PRINT("NSH, ");
    if (ndo->ndo_vflag > 1) {
        ND_PRINT("ver %u, ", ver);
    }
    if (ver != 0)
        return;
    ND_PRINT("flags [%s], ",
             bittok2str_nosep(nsh_flags, "none", NSH_FLAGS(basehdr)));
    if (ndo->ndo_vflag > 2) {
        ND_PRINT("TTL %u, ", NSH_TTL(basehdr));
        ND_PRINT("length %u, ", length);
        ND_PRINT("md type %s, ", tok2str(md_str, "unknown (0x%02x)", md_type));
    }
    if (ndo->ndo_vflag > 1) {
        ND_PRINT("next-protocol %s, ",
                 tok2str(np_str, "unknown (0x%02x)", next_protocol));
    }

    /* Make sure we have all the headers */
    if (len < length * NSH_HDR_WORD_SIZE) {
        ND_PRINT(" (too many headers for packet length %u)", len);
        goto invalid;
    }

    /*
     *    0                   1                   2                   3
     *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *   |          Service Path Identifier (SPI)        | Service Index |
     *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *
     */
    ND_PRINT("service-path-id 0x%06x, ", GET_BE_U_3(bp));
    bp += 3;
    ND_PRINT("service-index 0x%x", GET_U_1(bp));
    bp += 1;

    /*
     * length includes the lengths of the Base and Service Path headers.
     * That means it must be at least 2.
     */
    if (length < 2) {
        ND_PRINT(" (less than two headers)");
        goto invalid;
    }

    /*
     * Print, or skip, the Context Headers.
     * (length - 2) is the length of those headers.
     */
    if (ndo->ndo_vflag > 2) {
        u_int n;

        if (md_type == MD_TYPE1) {
            if (length != 6) {
                ND_PRINT(" (length for the MD type)");
                goto invalid;
            }
            for (n = 0; n < length - 2; n++) {
                ND_PRINT("\n        Context[%02u]: 0x%08x", n, GET_BE_U_4(bp));
                bp += NSH_HDR_WORD_SIZE;
            }
            past_headers = 1;
        } else if (md_type == MD_TYPE2) {
            n = 0;
            while (n < length - 2) {
                uint16_t tlv_class;
                uint8_t tlv_type, tlv_len, tlv_len_padded;

                tlv_class = GET_BE_U_2(bp);
                bp += 2;
                tlv_type  = GET_U_1(bp);
                bp += 1;
                tlv_len   = GET_U_1(bp) & 0x7f;
                bp += 1;
                tlv_len_padded = roundup2(tlv_len, NSH_HDR_WORD_SIZE);

                ND_PRINT("\n        TLV Class %u, Type %u, Len %u",
                          tlv_class, tlv_type, tlv_len);

                n += 1;

                if (length - 2 < n + tlv_len_padded / NSH_HDR_WORD_SIZE) {
                    ND_PRINT(" (length too big)");
                    goto invalid;
                }

                if (tlv_len) {
                    const char *sep = "0x";
                    u_int vn;

                    ND_PRINT("\n            Value: ");
                    for (vn = 0; vn < tlv_len; vn++) {
                        ND_PRINT("%s%02x", sep, GET_U_1(bp));
                        bp += 1;
                        sep = ":";
                    }
                    /* Cover any TLV padding. */
                    ND_TCHECK_LEN(bp, tlv_len_padded - tlv_len);
                    bp += tlv_len_padded - tlv_len;
                    n += tlv_len_padded / NSH_HDR_WORD_SIZE;
                }
            }
            past_headers = 1;
        }
    }
    if (! past_headers) {
        ND_TCHECK_LEN(bp, (length - 2) * NSH_HDR_WORD_SIZE);
        bp += (length - 2) * NSH_HDR_WORD_SIZE;
    }
    ND_PRINT(ndo->ndo_vflag ? "\n    " : ": ");

    /* print Next Protocol */
    next_len = len - length * NSH_HDR_WORD_SIZE;
    switch (next_protocol) {
    case NP_IPV4:
        ip_print(ndo, bp, next_len);
        break;
    case NP_IPV6:
        ip6_print(ndo, bp, next_len);
        break;
    case NP_ETH:
        ether_print(ndo, bp, next_len, ND_BYTES_AVAILABLE_AFTER(bp), NULL, NULL);
        break;
    default:
        ND_PRINT("ERROR: unknown-next-protocol");
        return;
    }

    return;

invalid:
    nd_print_invalid(ndo);
}

