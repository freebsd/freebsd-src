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
 * Original code by Hannes Gredler (hannes@gredler.at)
 */

/* \summary: Bidirectional Forwarding Detection (BFD) printer */

/*
 * specification: draft-ietf-bfd-base-01 for version 0,
 * RFC 5880 for version 1, and RFC 5881
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"

#include "udp.h"

/*
 * Control packet, BFDv0, draft-ietf-bfd-base-01
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |Vers |  Diag   |H|D|P|F|C|A|Rsv|  Detect Mult  |    Length     |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                       My Discriminator                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                      Your Discriminator                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                    Desired Min TX Interval                    |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                   Required Min RX Interval                    |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                 Required Min Echo RX Interval                 |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/*
 *  Control packet, BFDv1, RFC 5880
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |Vers |  Diag   |Sta|P|F|C|A|D|M|  Detect Mult  |    Length     |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                       My Discriminator                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                      Your Discriminator                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                    Desired Min TX Interval                    |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                   Required Min RX Interval                    |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                 Required Min Echo RX Interval                 |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct bfd_header_t {
    nd_uint8_t  version_diag;
    nd_uint8_t  flags;
    nd_uint8_t  detect_time_multiplier;
    nd_uint8_t  length;
    nd_uint32_t my_discriminator;
    nd_uint32_t your_discriminator;
    nd_uint32_t desired_min_tx_interval;
    nd_uint32_t required_min_rx_interval;
    nd_uint32_t required_min_echo_interval;
};

/*
 *    An optional Authentication Header may be present
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |   Auth Type   |   Auth Len    |    Authentication Data...     |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct bfd_auth_header_t {
    nd_uint8_t auth_type;
    nd_uint8_t auth_len;
    nd_uint8_t auth_data;
    nd_uint8_t dummy; /* minimum 4 bytes */
};

enum auth_type {
    AUTH_PASSWORD = 1,
    AUTH_MD5      = 2,
    AUTH_MET_MD5  = 3,
    AUTH_SHA1     = 4,
    AUTH_MET_SHA1 = 5
};

static const struct tok bfd_v1_authentication_values[] = {
    { AUTH_PASSWORD, "Simple Password" },
    { AUTH_MD5,      "Keyed MD5" },
    { AUTH_MET_MD5,  "Meticulous Keyed MD5" },
    { AUTH_SHA1,     "Keyed SHA1" },
    { AUTH_MET_SHA1, "Meticulous Keyed SHA1" },
    { 0, NULL }
};

enum auth_length {
    AUTH_PASSWORD_FIELD_MIN_LEN = 4,  /* header + password min: 3 + 1 */
    AUTH_PASSWORD_FIELD_MAX_LEN = 19, /* header + password max: 3 + 16 */
    AUTH_MD5_FIELD_LEN  = 24,
    AUTH_MD5_HASH_LEN   = 16,
    AUTH_SHA1_FIELD_LEN = 28,
    AUTH_SHA1_HASH_LEN  = 20
};

#define BFD_EXTRACT_VERSION(x) (((x)&0xe0)>>5)
#define BFD_EXTRACT_DIAG(x)     ((x)&0x1f)

static const struct tok bfd_diag_values[] = {
    { 0, "No Diagnostic" },
    { 1, "Control Detection Time Expired" },
    { 2, "Echo Function Failed" },
    { 3, "Neighbor Signaled Session Down" },
    { 4, "Forwarding Plane Reset" },
    { 5, "Path Down" },
    { 6, "Concatenated Path Down" },
    { 7, "Administratively Down" },
    { 8, "Reverse Concatenated Path Down" },
    { 0, NULL }
};

static const struct tok bfd_port_values[] = {
    { BFD_CONTROL_PORT,  "Control" },
    { BFD_MULTIHOP_PORT, "Multihop" },
    { BFD_LAG_PORT,      "Lag" },
    { 0, NULL }
};

#define BFD_FLAG_AUTH 0x04

static const struct tok bfd_v0_flag_values[] = {
    { 0x80, "I Hear You" },
    { 0x40, "Demand" },
    { 0x20, "Poll" },
    { 0x10, "Final" },
    { 0x08, "Control Plane Independent" },
    { BFD_FLAG_AUTH, "Authentication Present" },
    { 0x02, "Reserved" },
    { 0x01, "Reserved" },
    { 0, NULL }
};

static const struct tok bfd_v1_flag_values[] = {
    { 0x20, "Poll" },
    { 0x10, "Final" },
    { 0x08, "Control Plane Independent" },
    { BFD_FLAG_AUTH, "Authentication Present" },
    { 0x02, "Demand" },
    { 0x01, "Multipoint" },
    { 0, NULL }
};

static const struct tok bfd_v1_state_values[] = {
    { 0, "AdminDown" },
    { 1, "Down" },
    { 2, "Init" },
    { 3, "Up" },
    { 0, NULL }
};

static void
auth_print(netdissect_options *ndo, const u_char *pptr)
{
        const struct bfd_auth_header_t *bfd_auth_header;
        uint8_t auth_type, auth_len;
        int i;

        pptr += sizeof (struct bfd_header_t);
        bfd_auth_header = (const struct bfd_auth_header_t *)pptr;
        ND_TCHECK_SIZE(bfd_auth_header);
        auth_type = GET_U_1(bfd_auth_header->auth_type);
        auth_len = GET_U_1(bfd_auth_header->auth_len);
        ND_PRINT("\n\tAuthentication: %s (%u), length: %u",
                 tok2str(bfd_v1_authentication_values,"Unknown",auth_type),
                 auth_type, auth_len);
                pptr += 2;
                ND_PRINT("\n\t  Auth Key ID: %u", GET_U_1(pptr));

        switch(auth_type) {
            case AUTH_PASSWORD:
/*
 *    Simple Password Authentication Section Format
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |   Auth Type   |   Auth Len    |  Auth Key ID  |  Password...  |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                              ...                              |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
                if (auth_len < AUTH_PASSWORD_FIELD_MIN_LEN ||
                    auth_len > AUTH_PASSWORD_FIELD_MAX_LEN) {
                    ND_PRINT("[invalid length %u]",
                             auth_len);
                    break;
                }
                pptr++;
                ND_PRINT(", Password: ");
                /* the length is equal to the password length plus three */
                (void)nd_printn(ndo, pptr, auth_len - 3, NULL);
                break;
            case AUTH_MD5:
            case AUTH_MET_MD5:
/*
 *    Keyed MD5 and Meticulous Keyed MD5 Authentication Section Format
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |   Auth Type   |   Auth Len    |  Auth Key ID  |   Reserved    |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        Sequence Number                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                      Auth Key/Digest...                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                              ...                              |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
                if (auth_len != AUTH_MD5_FIELD_LEN) {
                    ND_PRINT("[invalid length %u]",
                             auth_len);
                    break;
                }
                pptr += 2;
                ND_PRINT(", Sequence Number: 0x%08x", GET_BE_U_4(pptr));
                pptr += 4;
                ND_TCHECK_LEN(pptr, AUTH_MD5_HASH_LEN);
                ND_PRINT("\n\t  Digest: ");
                for(i = 0; i < AUTH_MD5_HASH_LEN; i++)
                    ND_PRINT("%02x", GET_U_1(pptr + i));
                break;
            case AUTH_SHA1:
            case AUTH_MET_SHA1:
/*
 *    Keyed SHA1 and Meticulous Keyed SHA1 Authentication Section Format
 *
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |   Auth Type   |   Auth Len    |  Auth Key ID  |   Reserved    |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        Sequence Number                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                       Auth Key/Hash...                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                              ...                              |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
                if (auth_len != AUTH_SHA1_FIELD_LEN) {
                    ND_PRINT("[invalid length %u]",
                             auth_len);
                    break;
                }
                pptr += 2;
                ND_PRINT(", Sequence Number: 0x%08x", GET_BE_U_4(pptr));
                pptr += 4;
                ND_TCHECK_LEN(pptr, AUTH_SHA1_HASH_LEN);
                ND_PRINT("\n\t  Hash: ");
                for(i = 0; i < AUTH_SHA1_HASH_LEN; i++)
                    ND_PRINT("%02x", GET_U_1(pptr + i));
                break;
        }
}

void
bfd_print(netdissect_options *ndo, const u_char *pptr,
          u_int len, u_int port)
{
	ndo->ndo_protocol = "bfd";
        if (port == BFD_CONTROL_PORT ||
            port == BFD_MULTIHOP_PORT ||
            port == BFD_LAG_PORT) {
            /*
             * Control packet.
             */
            const struct bfd_header_t *bfd_header;
            uint8_t version_diag;
            uint8_t version = 0;
            uint8_t flags;

            bfd_header = (const struct bfd_header_t *)pptr;
            ND_TCHECK_SIZE(bfd_header);
            version_diag = GET_U_1(bfd_header->version_diag);
            version = BFD_EXTRACT_VERSION(version_diag);
            flags = GET_U_1(bfd_header->flags);

            switch (version) {

                /* BFDv0 */
            case 0:
                if (ndo->ndo_vflag < 1)
                {
                    ND_PRINT("BFDv0, Control, Flags: [%s], length: %u",
                           bittok2str(bfd_v0_flag_values, "none", flags),
                           len);
                    return;
                }

                ND_PRINT("BFDv0, length: %u\n\tControl, Flags: [%s], Diagnostic: %s (0x%02x)",
                       len,
                       bittok2str(bfd_v0_flag_values, "none", flags),
                       tok2str(bfd_diag_values,"unknown",BFD_EXTRACT_DIAG(version_diag)),
                       BFD_EXTRACT_DIAG(version_diag));

                ND_PRINT("\n\tDetection Timer Multiplier: %u (%u ms Detection time), BFD Length: %u",
                       GET_U_1(bfd_header->detect_time_multiplier),
                       GET_U_1(bfd_header->detect_time_multiplier) * GET_BE_U_4(bfd_header->desired_min_tx_interval)/1000,
                       GET_U_1(bfd_header->length));


                ND_PRINT("\n\tMy Discriminator: 0x%08x",
                         GET_BE_U_4(bfd_header->my_discriminator));
                ND_PRINT(", Your Discriminator: 0x%08x",
                         GET_BE_U_4(bfd_header->your_discriminator));
                ND_PRINT("\n\t  Desired min Tx Interval:    %4u ms",
                         GET_BE_U_4(bfd_header->desired_min_tx_interval)/1000);
                ND_PRINT("\n\t  Required min Rx Interval:   %4u ms",
                         GET_BE_U_4(bfd_header->required_min_rx_interval)/1000);
                ND_PRINT("\n\t  Required min Echo Interval: %4u ms",
                         GET_BE_U_4(bfd_header->required_min_echo_interval)/1000);

                if (flags & BFD_FLAG_AUTH) {
                    auth_print(ndo, pptr);
                }
                break;

                /* BFDv1 */
            case 1:
                if (ndo->ndo_vflag < 1)
                {
                    ND_PRINT("BFDv1, %s, State %s, Flags: [%s], length: %u",
                           tok2str(bfd_port_values, "unknown (%u)", port),
                           tok2str(bfd_v1_state_values, "unknown (%u)", (flags & 0xc0) >> 6),
                           bittok2str(bfd_v1_flag_values, "none", flags & 0x3f),
                           len);
                    return;
                }

                ND_PRINT("BFDv1, length: %u\n\t%s, State %s, Flags: [%s], Diagnostic: %s (0x%02x)",
                       len,
                       tok2str(bfd_port_values, "unknown (%u)", port),
                       tok2str(bfd_v1_state_values, "unknown (%u)", (flags & 0xc0) >> 6),
                       bittok2str(bfd_v1_flag_values, "none", flags & 0x3f),
                       tok2str(bfd_diag_values,"unknown",BFD_EXTRACT_DIAG(version_diag)),
                       BFD_EXTRACT_DIAG(version_diag));

                ND_PRINT("\n\tDetection Timer Multiplier: %u (%u ms Detection time), BFD Length: %u",
                       GET_U_1(bfd_header->detect_time_multiplier),
                       GET_U_1(bfd_header->detect_time_multiplier) * GET_BE_U_4(bfd_header->desired_min_tx_interval)/1000,
                       GET_U_1(bfd_header->length));


                ND_PRINT("\n\tMy Discriminator: 0x%08x",
                         GET_BE_U_4(bfd_header->my_discriminator));
                ND_PRINT(", Your Discriminator: 0x%08x",
                         GET_BE_U_4(bfd_header->your_discriminator));
                ND_PRINT("\n\t  Desired min Tx Interval:    %4u ms",
                         GET_BE_U_4(bfd_header->desired_min_tx_interval)/1000);
                ND_PRINT("\n\t  Required min Rx Interval:   %4u ms",
                         GET_BE_U_4(bfd_header->required_min_rx_interval)/1000);
                ND_PRINT("\n\t  Required min Echo Interval: %4u ms",
                         GET_BE_U_4(bfd_header->required_min_echo_interval)/1000);

                if (flags & BFD_FLAG_AUTH) {
                    auth_print(ndo, pptr);
                }
                break;

            default:
                ND_PRINT("BFDv%u, Control, length: %u",
                       version,
                       len);
                if (ndo->ndo_vflag >= 1) {
                    if(!print_unknown_data(ndo, pptr,"\n\t",len))
                        return;
                }
                break;
            }
        } else if (port == BFD_ECHO_PORT) {
            /*
             * Echo packet.
             */
            ND_PRINT("BFD, Echo, length: %u",
                   len);
            if (ndo->ndo_vflag >= 1) {
                if(!print_unknown_data(ndo, pptr,"\n\t",len))
                    return;
            }
        } else {
            /*
             * Unknown packet type.
             */
            ND_PRINT("BFD, unknown (%u), length: %u",
                   port,
                   len);
            if (ndo->ndo_vflag >= 1) {
                    if(!print_unknown_data(ndo, pptr,"\n\t",len))
                            return;
            }
        }
}
