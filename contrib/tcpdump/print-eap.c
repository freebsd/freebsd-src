/*
 * Copyright (c) 2004 - Michael Richardson <mcr@xelerance.com>
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

/* \summary: Extensible Authentication Protocol (EAP) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

#define	EAP_FRAME_TYPE_PACKET		0
#define	EAP_FRAME_TYPE_START		1
#define	EAP_FRAME_TYPE_LOGOFF		2
#define	EAP_FRAME_TYPE_KEY		3
#define	EAP_FRAME_TYPE_ENCAP_ASF_ALERT	4

struct eap_frame_t {
    nd_uint8_t  version;
    nd_uint8_t  type;
    nd_uint16_t length;
};

static const struct tok eap_frame_type_values[] = {
    { EAP_FRAME_TYPE_PACKET,		"EAP packet" },
    { EAP_FRAME_TYPE_START,		"EAPOL start" },
    { EAP_FRAME_TYPE_LOGOFF,		"EAPOL logoff" },
    { EAP_FRAME_TYPE_KEY,		"EAPOL key" },
    { EAP_FRAME_TYPE_ENCAP_ASF_ALERT,	"Encapsulated ASF alert" },
    { 0, NULL}
};

/* RFC 3748 */
struct eap_packet_t {
    nd_uint8_t  code;
    nd_uint8_t  id;
    nd_uint16_t length;
};

#define		EAP_REQUEST	1
#define		EAP_RESPONSE	2
#define		EAP_SUCCESS	3
#define		EAP_FAILURE	4

static const struct tok eap_code_values[] = {
    { EAP_REQUEST,	"Request" },
    { EAP_RESPONSE,	"Response" },
    { EAP_SUCCESS,	"Success" },
    { EAP_FAILURE,	"Failure" },
    { 0, NULL}
};

#define		EAP_TYPE_NO_PROPOSED	0
#define		EAP_TYPE_IDENTITY	1
#define		EAP_TYPE_NOTIFICATION	2
#define		EAP_TYPE_NAK		3
#define		EAP_TYPE_MD5_CHALLENGE	4
#define		EAP_TYPE_OTP		5
#define		EAP_TYPE_GTC		6
#define		EAP_TYPE_TLS		13		/* RFC 5216 */
#define		EAP_TYPE_SIM		18		/* RFC 4186 */
#define		EAP_TYPE_TTLS		21		/* RFC 5281, draft-funk-eap-ttls-v0-01.txt */
#define		EAP_TYPE_AKA		23		/* RFC 4187 */
#define		EAP_TYPE_FAST		43		/* RFC 4851 */
#define		EAP_TYPE_EXPANDED_TYPES	254
#define		EAP_TYPE_EXPERIMENTAL	255

static const struct tok eap_type_values[] = {
    { EAP_TYPE_NO_PROPOSED,	"No proposed" },
    { EAP_TYPE_IDENTITY,	"Identity" },
    { EAP_TYPE_NOTIFICATION,    "Notification" },
    { EAP_TYPE_NAK,		"Nak" },
    { EAP_TYPE_MD5_CHALLENGE,   "MD5-challenge" },
    { EAP_TYPE_OTP,		"OTP" },
    { EAP_TYPE_GTC,		"GTC" },
    { EAP_TYPE_TLS,		"TLS" },
    { EAP_TYPE_SIM,		"SIM" },
    { EAP_TYPE_TTLS,		"TTLS" },
    { EAP_TYPE_AKA,		"AKA" },
    { EAP_TYPE_FAST,		"FAST" },
    { EAP_TYPE_EXPANDED_TYPES,  "Expanded types" },
    { EAP_TYPE_EXPERIMENTAL,    "Experimental" },
    { 0, NULL}
};

#define EAP_TLS_EXTRACT_BIT_L(x)	(((x)&0x80)>>7)

/* RFC 5216 - EAP TLS bits */
#define EAP_TLS_FLAGS_LEN_INCLUDED		(1 << 7)
#define EAP_TLS_FLAGS_MORE_FRAGMENTS		(1 << 6)
#define EAP_TLS_FLAGS_START			(1 << 5)

static const struct tok eap_tls_flags_values[] = {
	{ EAP_TLS_FLAGS_LEN_INCLUDED, "L bit" },
	{ EAP_TLS_FLAGS_MORE_FRAGMENTS, "More fragments bit"},
	{ EAP_TLS_FLAGS_START, "Start bit"},
	{ 0, NULL}
};

#define EAP_TTLS_VERSION(x)		((x)&0x07)

/* EAP-AKA and EAP-SIM - RFC 4187 */
#define EAP_AKA_CHALLENGE		1
#define EAP_AKA_AUTH_REJECT		2
#define EAP_AKA_SYNC_FAILURE		4
#define EAP_AKA_IDENTITY		5
#define EAP_SIM_START			10
#define EAP_SIM_CHALLENGE		11
#define EAP_AKA_NOTIFICATION		12
#define EAP_AKA_REAUTH			13
#define EAP_AKA_CLIENT_ERROR		14

static const struct tok eap_aka_subtype_values[] = {
    { EAP_AKA_CHALLENGE,	"Challenge" },
    { EAP_AKA_AUTH_REJECT,	"Auth reject" },
    { EAP_AKA_SYNC_FAILURE,	"Sync failure" },
    { EAP_AKA_IDENTITY,		"Identity" },
    { EAP_SIM_START,		"Start" },
    { EAP_SIM_CHALLENGE,	"Challenge" },
    { EAP_AKA_NOTIFICATION,	"Notification" },
    { EAP_AKA_REAUTH,		"Reauth" },
    { EAP_AKA_CLIENT_ERROR,	"Client error" },
    { 0, NULL}
};

/*
 * Print EAP requests / responses
 */
void
eap_print(netdissect_options *ndo,
          const u_char *cp,
          u_int length)
{
    u_int type, subtype, len;
    u_int count;
    const char *sep;

    ndo->ndo_protocol = "eap";
    type = GET_U_1(cp);
    len = GET_BE_U_2(cp + 2);
    if (len != length) {
        /*
         * Probably a fragment; in some cases the fragmentation might
         * not put an EAP header on every packet, if reassembly can
         * be done without that (e.g., fragmentation to make a message
         * fit in multiple TLVs in a RADIUS packet).
         */
        ND_PRINT("EAP fragment?");
        return;
    }
    ND_PRINT("%s (%u), id %u, len %u",
            tok2str(eap_code_values, "unknown", type),
            type,
            GET_U_1((cp + 1)),
            len);
    if (len < 4) {
        ND_PRINT(" (too short for EAP header)");
        return;
    }

    ND_TCHECK_LEN(cp, len);

    if (type == EAP_REQUEST || type == EAP_RESPONSE) {
        /* RFC 3748 Section 4.1 */
        if (len < 5) {
            ND_PRINT(" (too short for EAP request/response)");
            return;
        }
        subtype = GET_U_1(cp + 4);
        ND_PRINT("\n\t\t Type %s (%u)",
                tok2str(eap_type_values, "unknown", subtype),
                subtype);

        switch (subtype) {
            case EAP_TYPE_IDENTITY:
                /* According to RFC 3748, the message is optional */
                if (len > 5) {
                    ND_PRINT(", Identity: ");
                    nd_printjnp(ndo, cp + 5, len - 5);
                }
                break;

            case EAP_TYPE_NOTIFICATION:
                /* According to RFC 3748, there must be at least one octet of message */
                if (len < 6) {
                    ND_PRINT(" (too short for EAP Notification request/response)");
                    return;
                }
                ND_PRINT(", Notification: ");
                nd_printjnp(ndo, cp + 5, len - 5);
                break;

            case EAP_TYPE_NAK:
                /*
                 * one or more octets indicating
                 * the desired authentication
                 * type one octet per type
                 */
                if (len < 6) {
                    ND_PRINT(" (too short for EAP Legacy NAK request/response)");
                    return;
                }
                sep = "";
                for (count = 5; count < len; count++) {
                    ND_PRINT("%s %s (%u)", sep,
                           tok2str(eap_type_values, "unknown", GET_U_1((cp + count))),
                           GET_U_1(cp + count));
                    sep = ",";
                }
                break;

            case EAP_TYPE_TTLS:
            case EAP_TYPE_TLS:
                if (len < 6) {
                    ND_PRINT(" (too short for EAP TLS/TTLS request/response)");
                    return;
                }
                if (subtype == EAP_TYPE_TTLS)
                    ND_PRINT(" TTLSv%u",
                           EAP_TTLS_VERSION(GET_U_1((cp + 5))));
                ND_PRINT(" flags [%s] 0x%02x",
                       bittok2str(eap_tls_flags_values, "none", GET_U_1((cp + 5))),
                       GET_U_1(cp + 5));

                if (EAP_TLS_EXTRACT_BIT_L(GET_U_1(cp + 5))) {
                    if (len < 10) {
                        ND_PRINT(" (too short for EAP TLS/TTLS request/response with length)");
                        return;
                    }
                    ND_PRINT(", len %u", GET_BE_U_4(cp + 6));
                }
                break;

            case EAP_TYPE_FAST:
                if (len < 6) {
                    ND_PRINT(" (too short for EAP FAST request/response)");
                    return;
                }
                ND_PRINT(" FASTv%u",
                       EAP_TTLS_VERSION(GET_U_1((cp + 5))));
                ND_PRINT(" flags [%s] 0x%02x",
                       bittok2str(eap_tls_flags_values, "none", GET_U_1((cp + 5))),
                       GET_U_1(cp + 5));

                if (EAP_TLS_EXTRACT_BIT_L(GET_U_1(cp + 5))) {
                    if (len < 10) {
                        ND_PRINT(" (too short for EAP FAST request/response with length)");
                        return;
                    }
                    ND_PRINT(", len %u", GET_BE_U_4(cp + 6));
                }

                /* FIXME - TLV attributes follow */
                break;

            case EAP_TYPE_AKA:
            case EAP_TYPE_SIM:
                if (len < 6) {
                    ND_PRINT(" (too short for EAP SIM/AKA request/response)");
                    return;
                }
                ND_PRINT(" subtype [%s] 0x%02x",
                       tok2str(eap_aka_subtype_values, "unknown", GET_U_1((cp + 5))),
                       GET_U_1(cp + 5));

                /* FIXME - TLV attributes follow */
                break;

            case EAP_TYPE_MD5_CHALLENGE:
            case EAP_TYPE_OTP:
            case EAP_TYPE_GTC:
            case EAP_TYPE_EXPANDED_TYPES:
            case EAP_TYPE_EXPERIMENTAL:
            default:
                break;
        }
    }
    return;
trunc:
    nd_print_trunc(ndo);
}

void
eapol_print(netdissect_options *ndo,
            const u_char *cp)
{
    const struct eap_frame_t *eap;
    u_int eap_type, eap_len;

    ndo->ndo_protocol = "eap";
    eap = (const struct eap_frame_t *)cp;
    ND_TCHECK_SIZE(eap);
    eap_type = GET_U_1(eap->type);

    ND_PRINT("%s (%u) v%u, len %u",
           tok2str(eap_frame_type_values, "unknown", eap_type),
           eap_type,
           GET_U_1(eap->version),
           GET_BE_U_2(eap->length));
    if (ndo->ndo_vflag < 1)
        return;

    cp += sizeof(struct eap_frame_t);
    eap_len = GET_BE_U_2(eap->length);

    switch (eap_type) {
    case EAP_FRAME_TYPE_PACKET:
        if (eap_len == 0)
            goto trunc;
        ND_PRINT(", ");
        eap_print(ndo, cp, eap_len);
        return;
    case EAP_FRAME_TYPE_LOGOFF:
    case EAP_FRAME_TYPE_ENCAP_ASF_ALERT:
    default:
        break;
    }
    return;

 trunc:
    nd_print_trunc(ndo);
}
