/*
 * Copyright (c) 1998-2007 The TCPDUMP project
 *
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
 * Original code by Carles Kishimoto <carles.kishimoto@gmail.com>
 */

/* \summary: Cisco UniDirectional Link Detection (UDLD) protocol printer */

/* specification: RFC 5171 */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"


#define UDLD_HEADER_LEN			4
#define UDLD_TLV_HEADER_LEN		4
#define UDLD_DEVICE_ID_TLV		0x0001
#define UDLD_PORT_ID_TLV		0x0002
#define UDLD_ECHO_TLV			0x0003
#define UDLD_MESSAGE_INTERVAL_TLV	0x0004
#define UDLD_TIMEOUT_INTERVAL_TLV	0x0005
#define UDLD_DEVICE_NAME_TLV		0x0006
#define UDLD_SEQ_NUMBER_TLV		0x0007

static const struct tok udld_tlv_values[] = {
    { UDLD_DEVICE_ID_TLV, "Device-ID TLV"},
    { UDLD_PORT_ID_TLV, "Port-ID TLV"},
    { UDLD_ECHO_TLV, "Echo TLV"},
    { UDLD_MESSAGE_INTERVAL_TLV, "Message Interval TLV"},
    { UDLD_TIMEOUT_INTERVAL_TLV, "Timeout Interval TLV"},
    { UDLD_DEVICE_NAME_TLV, "Device Name TLV"},
    { UDLD_SEQ_NUMBER_TLV,"Sequence Number TLV"},
    { 0, NULL}
};

static const struct tok udld_code_values[] = {
    { 0x00, "Reserved"},
    { 0x01, "Probe message"},
    { 0x02, "Echo message"},
    { 0x03, "Flush message"},
    { 0, NULL}
};

static const struct tok udld_flags_bitmap_str[] = {
    { 1U << 0, "RT"    },
    { 1U << 1, "RSY"   },
    { 1U << 2, "MBZ-2" },
    { 1U << 3, "MBZ-3" },
    { 1U << 4, "MBZ-4" },
    { 1U << 5, "MBZ-5" },
    { 1U << 6, "MBZ-6" },
    { 1U << 7, "MBZ-7" },
    { 0, NULL}
};

/*
 * UDLD's Protocol Data Unit format:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Ver | Opcode  |     Flags     |           Checksum            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |               List of TLVs (variable length list)             |
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * TLV format:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |             TYPE              |            LENGTH             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                             VALUE                             |
 * |                              ...                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * LENGTH: Length in bytes of the Type, Length, and Value fields.
 */

#define	UDLD_EXTRACT_VERSION(x) (((x)&0xe0)>>5)
#define	UDLD_EXTRACT_OPCODE(x) ((x)&0x1f)

void
udld_print(netdissect_options *ndo,
           const u_char *tptr, u_int length)
{
    uint8_t ver, code, flags;

    ndo->ndo_protocol = "udld";
    if (length < UDLD_HEADER_LEN)
        goto invalid;

    ver = UDLD_EXTRACT_VERSION(GET_U_1(tptr));
    code = UDLD_EXTRACT_OPCODE(GET_U_1(tptr));
    tptr += 1;
    length -= 1;

    flags = GET_U_1(tptr);
    tptr += 1;
    length -= 1;

    ND_PRINT("UDLDv%u, Code %s (%x), Flags [%s] (0x%02x), length %u",
           ver,
           tok2str(udld_code_values, "Reserved", code),
           code,
           bittok2str(udld_flags_bitmap_str, "none", flags),
           flags,
           length + 2);

    /*
     * In non-verbose mode, just print version and opcode type
     */
    if (ndo->ndo_vflag < 1) {
        goto tcheck_remainder;
    }

    ND_PRINT("\n\tChecksum 0x%04x (unverified)", GET_BE_U_2(tptr));
    tptr += 2;
    length -= 2;

    while (length) {
        uint16_t type, len;

        if (length < UDLD_TLV_HEADER_LEN)
            goto invalid;

	type = GET_BE_U_2(tptr);
        tptr += 2;
        length -= 2;

        len  = GET_BE_U_2(tptr);
        tptr += 2;
        length -= 2;

        ND_PRINT("\n\t%s (0x%04x) TLV, length %u",
               tok2str(udld_tlv_values, "Unknown", type),
               type, len);

        /* infinite loop check */
        if (len <= UDLD_TLV_HEADER_LEN)
            goto invalid;

        len -= UDLD_TLV_HEADER_LEN;
        if (length < len)
            goto invalid;

        switch (type) {
        case UDLD_DEVICE_ID_TLV:
        case UDLD_PORT_ID_TLV:
        case UDLD_DEVICE_NAME_TLV:
            ND_PRINT(", ");
            nd_printjnp(ndo, tptr, len);
            break;

        case UDLD_ECHO_TLV:
            ND_PRINT(", ");
            (void)nd_printn(ndo, tptr, len, NULL);
            break;

        case UDLD_MESSAGE_INTERVAL_TLV:
        case UDLD_TIMEOUT_INTERVAL_TLV:
            if (len != 1)
                goto invalid;
            ND_PRINT(", %us", (GET_U_1(tptr)));
            break;

        case UDLD_SEQ_NUMBER_TLV:
            if (len != 4)
                goto invalid;
            ND_PRINT(", %u", GET_BE_U_4(tptr));
            break;

        default:
            ND_TCHECK_LEN(tptr, len);
            break;
        }
        tptr += len;
        length -= len;
    }

    return;

invalid:
    nd_print_invalid(ndo);
tcheck_remainder:
    ND_TCHECK_LEN(tptr, length);
}
