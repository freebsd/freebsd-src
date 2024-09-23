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

/* \summary: Dynamic Trunking Protocol (DTP) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"


#define DTP_HEADER_LEN			1
#define DTP_DOMAIN_TLV			0x0001
#define DTP_STATUS_TLV			0x0002
#define DTP_DTP_TYPE_TLV		0x0003
#define DTP_NEIGHBOR_TLV		0x0004

static const struct tok dtp_tlv_values[] = {
    { DTP_DOMAIN_TLV, "Domain" },
    { DTP_STATUS_TLV, "Status" },
    { DTP_DTP_TYPE_TLV, "DTP type" },
    { DTP_NEIGHBOR_TLV, "Neighbor" },
    { 0, NULL}
};

void
dtp_print(netdissect_options *ndo, const u_char *tptr, u_int length)
{
    ndo->ndo_protocol = "dtp";
    if (length < DTP_HEADER_LEN) {
        ND_PRINT("[zero packet length]");
        goto invalid;
    }

    ND_PRINT("DTPv%u, length %u",
           GET_U_1(tptr),
           length);

    /*
     * In non-verbose mode, just print version.
     */
    if (ndo->ndo_vflag < 1) {
	return;
    }

    tptr += DTP_HEADER_LEN;
    length -= DTP_HEADER_LEN;

    while (length) {
        uint16_t type, len;

        if (length < 4) {
            ND_PRINT("[%u bytes remaining]", length);
            goto invalid;
        }
	type = GET_BE_U_2(tptr);
        len  = GET_BE_U_2(tptr + 2);
       /* XXX: should not be but sometimes it is, see the test captures */
        if (type == 0)
            return;
        ND_PRINT("\n\t%s (0x%04x) TLV, length %u",
               tok2str(dtp_tlv_values, "Unknown", type),
               type, len);

        /* infinite loop check */
        if (len < 4 || len > length) {
            ND_PRINT("[TLV length %u]", len);
            goto invalid;
        }

        switch (type) {
	case DTP_DOMAIN_TLV:
		ND_PRINT(", ");
		nd_printjnp(ndo, tptr+4, len-4);
		break;

	case DTP_STATUS_TLV:
	case DTP_DTP_TYPE_TLV:
                if (len != 5)
                    goto invalid;
                ND_PRINT(", 0x%x", GET_U_1(tptr + 4));
                break;

	case DTP_NEIGHBOR_TLV:
                if (len != 10)
                    goto invalid;
                ND_PRINT(", %s", GET_ETHERADDR_STRING(tptr+4));
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
    ND_TCHECK_LEN(tptr, length);
}
