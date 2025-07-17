/*
 * Copyright (c) 1998-2006 The TCPDUMP project
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
 * support for the IEEE "slow protocols" LACP, MARKER as per 802.3ad
 *                                       OAM as per 802.3ah
 *
 * Original code by Hannes Gredler (hannes@gredler.at)
 */

/* \summary: IEEE "slow protocols" (802.3ad/802.3ah) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "oui.h"


#define	SLOW_PROTO_LACP                     1
#define	SLOW_PROTO_MARKER                   2
#define SLOW_PROTO_OAM                      3

#define	LACP_VERSION                        1
#define	MARKER_VERSION                      1

static const struct tok slow_proto_values[] = {
    { SLOW_PROTO_LACP, "LACP" },
    { SLOW_PROTO_MARKER, "MARKER" },
    { SLOW_PROTO_OAM, "OAM" },
    { 0, NULL}
};

static const struct tok slow_oam_flag_values[] = {
    { 0x0001, "Link Fault" },
    { 0x0002, "Dying Gasp" },
    { 0x0004, "Critical Event" },
    { 0x0008, "Local Evaluating" },
    { 0x0010, "Local Stable" },
    { 0x0020, "Remote Evaluating" },
    { 0x0040, "Remote Stable" },
    { 0, NULL}
};

#define SLOW_OAM_CODE_INFO          0x00
#define SLOW_OAM_CODE_EVENT_NOTIF   0x01
#define SLOW_OAM_CODE_VAR_REQUEST   0x02
#define SLOW_OAM_CODE_VAR_RESPONSE  0x03
#define SLOW_OAM_CODE_LOOPBACK_CTRL 0x04
#define SLOW_OAM_CODE_PRIVATE       0xfe

static const struct tok slow_oam_code_values[] = {
    { SLOW_OAM_CODE_INFO, "Information" },
    { SLOW_OAM_CODE_EVENT_NOTIF, "Event Notification" },
    { SLOW_OAM_CODE_VAR_REQUEST, "Variable Request" },
    { SLOW_OAM_CODE_VAR_RESPONSE, "Variable Response" },
    { SLOW_OAM_CODE_LOOPBACK_CTRL, "Loopback Control" },
    { SLOW_OAM_CODE_PRIVATE, "Vendor Private" },
    { 0, NULL}
};

struct slow_oam_info_t {
    nd_uint8_t info_type;
    nd_uint8_t info_length;
    nd_uint8_t oam_version;
    nd_uint16_t revision;
    nd_uint8_t state;
    nd_uint8_t oam_config;
    nd_uint16_t oam_pdu_config;
    nd_uint24_t oui;
    nd_uint32_t vendor_private;
};

#define SLOW_OAM_INFO_TYPE_END_OF_TLV 0x00
#define SLOW_OAM_INFO_TYPE_LOCAL 0x01
#define SLOW_OAM_INFO_TYPE_REMOTE 0x02
#define SLOW_OAM_INFO_TYPE_ORG_SPECIFIC 0xfe

static const struct tok slow_oam_info_type_values[] = {
    { SLOW_OAM_INFO_TYPE_END_OF_TLV, "End of TLV marker" },
    { SLOW_OAM_INFO_TYPE_LOCAL, "Local" },
    { SLOW_OAM_INFO_TYPE_REMOTE, "Remote" },
    { SLOW_OAM_INFO_TYPE_ORG_SPECIFIC, "Organization specific" },
    { 0, NULL}
};

#define OAM_INFO_TYPE_PARSER_MASK 0x3
static const struct tok slow_oam_info_type_state_parser_values[] = {
    { 0x00, "forwarding" },
    { 0x01, "looping back" },
    { 0x02, "discarding" },
    { 0x03, "reserved" },
    { 0, NULL}
};

#define OAM_INFO_TYPE_MUX_MASK 0x4
static const struct tok slow_oam_info_type_state_mux_values[] = {
    { 0x00, "forwarding" },
    { 0x04, "discarding" },
    { 0, NULL}
};

static const struct tok slow_oam_info_type_oam_config_values[] = {
    { 0x01, "Active" },
    { 0x02, "Unidirectional" },
    { 0x04, "Remote-Loopback" },
    { 0x08, "Link-Events" },
    { 0x10, "Variable-Retrieval" },
    { 0, NULL}
};

/* 11 Bits */
#define OAM_INFO_TYPE_PDU_SIZE_MASK 0x7ff

#define SLOW_OAM_LINK_EVENT_END_OF_TLV 0x00
#define SLOW_OAM_LINK_EVENT_ERR_SYM_PER 0x01
#define SLOW_OAM_LINK_EVENT_ERR_FRM 0x02
#define SLOW_OAM_LINK_EVENT_ERR_FRM_PER 0x03
#define SLOW_OAM_LINK_EVENT_ERR_FRM_SUMM 0x04
#define SLOW_OAM_LINK_EVENT_ORG_SPECIFIC 0xfe

static const struct tok slow_oam_link_event_values[] = {
    { SLOW_OAM_LINK_EVENT_END_OF_TLV, "End of TLV marker" },
    { SLOW_OAM_LINK_EVENT_ERR_SYM_PER, "Errored Symbol Period Event" },
    { SLOW_OAM_LINK_EVENT_ERR_FRM, "Errored Frame Event" },
    { SLOW_OAM_LINK_EVENT_ERR_FRM_PER, "Errored Frame Period Event" },
    { SLOW_OAM_LINK_EVENT_ERR_FRM_SUMM, "Errored Frame Seconds Summary Event" },
    { SLOW_OAM_LINK_EVENT_ORG_SPECIFIC, "Organization specific" },
    { 0, NULL}
};

struct slow_oam_link_event_t {
    nd_uint8_t event_type;
    nd_uint8_t event_length;
    nd_uint16_t time_stamp;
    nd_uint64_t window;
    nd_uint64_t threshold;
    nd_uint64_t errors;
    nd_uint64_t errors_running_total;
    nd_uint32_t event_running_total;
};

struct slow_oam_variablerequest_t {
    nd_uint8_t branch;
    nd_uint16_t leaf;
};

struct slow_oam_variableresponse_t {
    nd_uint8_t branch;
    nd_uint16_t leaf;
    nd_uint8_t length;
};

struct slow_oam_loopbackctrl_t {
    nd_uint8_t command;
};

static const struct tok slow_oam_loopbackctrl_cmd_values[] = {
    { 0x01, "Enable OAM Remote Loopback" },
    { 0x02, "Disable OAM Remote Loopback" },
    { 0, NULL}
};

struct tlv_header_t {
    nd_uint8_t type;
    nd_uint8_t length;
};

#define LACP_MARKER_TLV_TERMINATOR     0x00  /* same code for LACP and Marker */

#define LACP_TLV_ACTOR_INFO            0x01
#define LACP_TLV_PARTNER_INFO          0x02
#define LACP_TLV_COLLECTOR_INFO        0x03

#define MARKER_TLV_MARKER_INFO         0x01

static const struct tok slow_tlv_values[] = {
    { (SLOW_PROTO_LACP << 8) + LACP_MARKER_TLV_TERMINATOR, "Terminator"},
    { (SLOW_PROTO_LACP << 8) + LACP_TLV_ACTOR_INFO, "Actor Information"},
    { (SLOW_PROTO_LACP << 8) + LACP_TLV_PARTNER_INFO, "Partner Information"},
    { (SLOW_PROTO_LACP << 8) + LACP_TLV_COLLECTOR_INFO, "Collector Information"},

    { (SLOW_PROTO_MARKER << 8) + LACP_MARKER_TLV_TERMINATOR, "Terminator"},
    { (SLOW_PROTO_MARKER << 8) + MARKER_TLV_MARKER_INFO, "Marker Information"},
    { 0, NULL}
};

struct lacp_tlv_actor_partner_info_t {
    nd_uint16_t sys_pri;
    nd_mac_addr sys;
    nd_uint16_t key;
    nd_uint16_t port_pri;
    nd_uint16_t port;
    nd_uint8_t state;
    nd_byte    pad[3];
};

static const struct tok lacp_tlv_actor_partner_info_state_values[] = {
    { 0x01, "Activity"},
    { 0x02, "Timeout"},
    { 0x04, "Aggregation"},
    { 0x08, "Synchronization"},
    { 0x10, "Collecting"},
    { 0x20, "Distributing"},
    { 0x40, "Default"},
    { 0x80, "Expired"},
    { 0, NULL}
};

struct lacp_tlv_collector_info_t {
    nd_uint16_t max_delay;
    nd_byte     pad[12];
};

struct marker_tlv_marker_info_t {
    nd_uint16_t req_port;
    nd_mac_addr req_sys;
    nd_uint32_t req_trans_id;
    nd_byte     pad[2];
};

struct lacp_marker_tlv_terminator_t {
    nd_byte     pad[50];
};

static void slow_marker_lacp_print(netdissect_options *, const u_char *, u_int, u_int);
static void slow_oam_print(netdissect_options *, const u_char *, u_int);

void
slow_print(netdissect_options *ndo,
           const u_char *pptr, u_int len)
{
    int print_version;
    u_int subtype;

    ndo->ndo_protocol = "slow";
    if (len < 1)
        goto tooshort;
    subtype = GET_U_1(pptr);

    /*
     * Sanity checking of the header.
     */
    switch (subtype) {
    case SLOW_PROTO_LACP:
        if (len < 2)
            goto tooshort;
        if (GET_U_1(pptr + 1) != LACP_VERSION) {
            ND_PRINT("LACP version %u packet not supported",
                     GET_U_1(pptr + 1));
            return;
        }
        print_version = 1;
        break;

    case SLOW_PROTO_MARKER:
        if (len < 2)
            goto tooshort;
        if (GET_U_1(pptr + 1) != MARKER_VERSION) {
            ND_PRINT("MARKER version %u packet not supported",
                     GET_U_1(pptr + 1));
            return;
        }
        print_version = 1;
        break;

    case SLOW_PROTO_OAM: /* fall through */
        print_version = 0;
        break;

    default:
        /* print basic information and exit */
        print_version = -1;
        break;
    }

    if (print_version == 1) {
        ND_PRINT("%sv%u, length %u",
               tok2str(slow_proto_values, "unknown (%u)", subtype),
               GET_U_1((pptr + 1)),
               len);
    } else {
        /* some slow protos don't have a version number in the header */
        ND_PRINT("%s, length %u",
               tok2str(slow_proto_values, "unknown (%u)", subtype),
               len);
    }

    /* unrecognized subtype */
    if (print_version == -1) {
        print_unknown_data(ndo, pptr, "\n\t", len);
        return;
    }

    if (!ndo->ndo_vflag)
        return;

    switch (subtype) {
    default: /* should not happen */
        break;

    case SLOW_PROTO_OAM:
        /* skip subtype */
        len -= 1;
        pptr += 1;
        slow_oam_print(ndo, pptr, len);
        break;

    case SLOW_PROTO_LACP:   /* LACP and MARKER share the same semantics */
    case SLOW_PROTO_MARKER:
        /* skip subtype and version */
        len -= 2;
        pptr += 2;
        slow_marker_lacp_print(ndo, pptr, len, subtype);
        break;
    }
    return;

tooshort:
    if (!ndo->ndo_vflag)
        ND_PRINT(" (packet is too short)");
    else
        ND_PRINT("\n\t\t packet is too short");
}

static void
slow_marker_lacp_print(netdissect_options *ndo,
                       const u_char *tptr, u_int tlen,
                       u_int proto_subtype)
{
    const struct tlv_header_t *tlv_header;
    const u_char *tlv_tptr;
    u_int tlv_type, tlv_len, tlv_tlen;

    union {
        const struct lacp_marker_tlv_terminator_t *lacp_marker_tlv_terminator;
        const struct lacp_tlv_actor_partner_info_t *lacp_tlv_actor_partner_info;
        const struct lacp_tlv_collector_info_t *lacp_tlv_collector_info;
        const struct marker_tlv_marker_info_t *marker_tlv_marker_info;
    } tlv_ptr;

    while(tlen>0) {
        /* is the packet big enough to include the tlv header ? */
        if (tlen < sizeof(struct tlv_header_t))
            goto tooshort;
        /* did we capture enough for fully decoding the tlv header ? */
        tlv_header = (const struct tlv_header_t *)tptr;
        tlv_type = GET_U_1(tlv_header->type);
        tlv_len = GET_U_1(tlv_header->length);

        ND_PRINT("\n\t%s TLV (0x%02x), length %u",
               tok2str(slow_tlv_values,
                       "Unknown",
                       (proto_subtype << 8) + tlv_type),
               tlv_type,
               tlv_len);

        if (tlv_type == LACP_MARKER_TLV_TERMINATOR) {
            /*
             * This TLV has a length of zero, and means there are no
             * more TLVs to process.
             */
            return;
        }

        /* length includes the type and length fields */
        if (tlv_len < sizeof(struct tlv_header_t)) {
            ND_PRINT("\n\t    ERROR: illegal length - should be >= %zu",
                     sizeof(struct tlv_header_t));
            return;
        }

        /* is the packet big enough to include the tlv ? */
        if (tlen < tlv_len)
            goto tooshort;
        /* did we capture enough for fully decoding the tlv ? */
        ND_TCHECK_LEN(tptr, tlv_len);

        tlv_tptr=tptr+sizeof(struct tlv_header_t);
        tlv_tlen=tlv_len-sizeof(struct tlv_header_t);

        switch((proto_subtype << 8) + tlv_type) {

            /* those two TLVs have the same structure -> fall through */
        case ((SLOW_PROTO_LACP << 8) + LACP_TLV_ACTOR_INFO):
        case ((SLOW_PROTO_LACP << 8) + LACP_TLV_PARTNER_INFO):
            if (tlv_tlen !=
                sizeof(struct lacp_tlv_actor_partner_info_t)) {
                ND_PRINT("\n\t    ERROR: illegal length - should be %zu",
                         sizeof(struct tlv_header_t) + sizeof(struct lacp_tlv_actor_partner_info_t));
                goto badlength;
            }

            tlv_ptr.lacp_tlv_actor_partner_info = (const struct lacp_tlv_actor_partner_info_t *)tlv_tptr;

            ND_PRINT("\n\t  System %s, System Priority %u, Key %u"
                   ", Port %u, Port Priority %u\n\t  State Flags [%s]",
                   GET_ETHERADDR_STRING(tlv_ptr.lacp_tlv_actor_partner_info->sys),
                   GET_BE_U_2(tlv_ptr.lacp_tlv_actor_partner_info->sys_pri),
                   GET_BE_U_2(tlv_ptr.lacp_tlv_actor_partner_info->key),
                   GET_BE_U_2(tlv_ptr.lacp_tlv_actor_partner_info->port),
                   GET_BE_U_2(tlv_ptr.lacp_tlv_actor_partner_info->port_pri),
                   bittok2str(lacp_tlv_actor_partner_info_state_values,
                              "none",
                              GET_U_1(tlv_ptr.lacp_tlv_actor_partner_info->state)));

            break;

        case ((SLOW_PROTO_LACP << 8) + LACP_TLV_COLLECTOR_INFO):
            if (tlv_tlen !=
                sizeof(struct lacp_tlv_collector_info_t)) {
                ND_PRINT("\n\t    ERROR: illegal length - should be %zu",
                         sizeof(struct tlv_header_t) + sizeof(struct lacp_tlv_collector_info_t));
                goto badlength;
            }

            tlv_ptr.lacp_tlv_collector_info = (const struct lacp_tlv_collector_info_t *)tlv_tptr;

            ND_PRINT("\n\t  Max Delay %u",
                   GET_BE_U_2(tlv_ptr.lacp_tlv_collector_info->max_delay));

            break;

        case ((SLOW_PROTO_MARKER << 8) + MARKER_TLV_MARKER_INFO):
            if (tlv_tlen !=
                sizeof(struct marker_tlv_marker_info_t)) {
                ND_PRINT("\n\t    ERROR: illegal length - should be %zu",
                         sizeof(struct tlv_header_t) + sizeof(struct marker_tlv_marker_info_t));
                goto badlength;
            }

            tlv_ptr.marker_tlv_marker_info = (const struct marker_tlv_marker_info_t *)tlv_tptr;

            ND_PRINT("\n\t  Request System %s, Request Port %u, Request Transaction ID 0x%08x",
                   GET_ETHERADDR_STRING(tlv_ptr.marker_tlv_marker_info->req_sys),
                   GET_BE_U_2(tlv_ptr.marker_tlv_marker_info->req_port),
                   GET_BE_U_4(tlv_ptr.marker_tlv_marker_info->req_trans_id));

            break;

        default:
            if (ndo->ndo_vflag <= 1)
                print_unknown_data(ndo, tlv_tptr, "\n\t  ", tlv_tlen);
            break;
        }

    badlength:
        /* do we want to see an additional hexdump ? */
        if (ndo->ndo_vflag > 1) {
            print_unknown_data(ndo, tptr+sizeof(struct tlv_header_t), "\n\t  ",
                               tlv_len-sizeof(struct tlv_header_t));
        }

        tptr+=tlv_len;
        tlen-=tlv_len;
    }
    return;

tooshort:
    ND_PRINT("\n\t\t packet is too short");
}

static void
slow_oam_print(netdissect_options *ndo,
               const u_char *tptr, u_int tlen)
{
    uint8_t code;
    uint8_t type, length;
    uint8_t state;
    uint8_t command;
    u_int hexdump;

    struct slow_oam_common_header_t {
        nd_uint16_t flags;
        nd_uint8_t code;
    };

    struct slow_oam_tlv_header_t {
        nd_uint8_t type;
        nd_uint8_t length;
    };

    union {
        const struct slow_oam_common_header_t *slow_oam_common_header;
        const struct slow_oam_tlv_header_t *slow_oam_tlv_header;
    } ptr;

    union {
        const struct slow_oam_info_t *slow_oam_info;
        const struct slow_oam_link_event_t *slow_oam_link_event;
        const struct slow_oam_variablerequest_t *slow_oam_variablerequest;
        const struct slow_oam_variableresponse_t *slow_oam_variableresponse;
        const struct slow_oam_loopbackctrl_t *slow_oam_loopbackctrl;
    } tlv;

    ptr.slow_oam_common_header = (const struct slow_oam_common_header_t *)tptr;
    if (tlen < sizeof(*ptr.slow_oam_common_header))
        goto tooshort;
    ND_TCHECK_SIZE(ptr.slow_oam_common_header);
    tptr += sizeof(struct slow_oam_common_header_t);
    tlen -= sizeof(struct slow_oam_common_header_t);

    code = GET_U_1(ptr.slow_oam_common_header->code);
    ND_PRINT("\n\tCode %s OAM PDU, Flags [%s]",
           tok2str(slow_oam_code_values, "Unknown (%u)", code),
           bittok2str(slow_oam_flag_values,
                      "none",
                      GET_BE_U_2(ptr.slow_oam_common_header->flags)));

    switch (code) {
    case SLOW_OAM_CODE_INFO:
        while (tlen > 0) {
            ptr.slow_oam_tlv_header = (const struct slow_oam_tlv_header_t *)tptr;
            if (tlen < sizeof(*ptr.slow_oam_tlv_header))
                goto tooshort;
            ND_TCHECK_SIZE(ptr.slow_oam_tlv_header);
            type = GET_U_1(ptr.slow_oam_tlv_header->type);
            length = GET_U_1(ptr.slow_oam_tlv_header->length);
            ND_PRINT("\n\t  %s Information Type (%u), length %u",
                   tok2str(slow_oam_info_type_values, "Reserved", type),
                   type,
                   length);

            if (type == SLOW_OAM_INFO_TYPE_END_OF_TLV) {
                /*
                 * As IEEE Std 802.3-2015 says for the End of TLV Marker,
                 * "(the length and value of the Type 0x00 TLV can be ignored)".
                 */
                return;
            }

            /* length includes the type and length fields */
            if (length < sizeof(struct slow_oam_tlv_header_t)) {
                ND_PRINT("\n\t    ERROR: illegal length - should be >= %zu",
                         sizeof(struct slow_oam_tlv_header_t));
                return;
            }

            if (tlen < length)
                goto tooshort;
            ND_TCHECK_LEN(tptr, length);

            hexdump = FALSE;
            switch (type) {
            case SLOW_OAM_INFO_TYPE_LOCAL: /* identical format - fall through */
            case SLOW_OAM_INFO_TYPE_REMOTE:
                tlv.slow_oam_info = (const struct slow_oam_info_t *)tptr;

                if (GET_U_1(tlv.slow_oam_info->info_length) !=
                    sizeof(struct slow_oam_info_t)) {
                    ND_PRINT("\n\t    ERROR: illegal length - should be %zu",
                           sizeof(struct slow_oam_info_t));
                    hexdump = TRUE;
                    goto badlength_code_info;
                }

                ND_PRINT("\n\t    OAM-Version %u, Revision %u",
                       GET_U_1(tlv.slow_oam_info->oam_version),
                       GET_BE_U_2(tlv.slow_oam_info->revision));

                state = GET_U_1(tlv.slow_oam_info->state);
                ND_PRINT("\n\t    State-Parser-Action %s, State-MUX-Action %s",
                       tok2str(slow_oam_info_type_state_parser_values, "Reserved",
                               state & OAM_INFO_TYPE_PARSER_MASK),
                       tok2str(slow_oam_info_type_state_mux_values, "Reserved",
                               state & OAM_INFO_TYPE_MUX_MASK));
                ND_PRINT("\n\t    OAM-Config Flags [%s], OAM-PDU-Config max-PDU size %u",
                       bittok2str(slow_oam_info_type_oam_config_values, "none",
                                  GET_U_1(tlv.slow_oam_info->oam_config)),
                       GET_BE_U_2(tlv.slow_oam_info->oam_pdu_config) &
                       OAM_INFO_TYPE_PDU_SIZE_MASK);
                ND_PRINT("\n\t    OUI %s (0x%06x), Vendor-Private 0x%08x",
                       tok2str(oui_values, "Unknown",
                               GET_BE_U_3(tlv.slow_oam_info->oui)),
                       GET_BE_U_3(tlv.slow_oam_info->oui),
                       GET_BE_U_4(tlv.slow_oam_info->vendor_private));
                break;

            case SLOW_OAM_INFO_TYPE_ORG_SPECIFIC:
                hexdump = TRUE;
                break;

            default:
                hexdump = TRUE;
                break;
            }

        badlength_code_info:
            /* do we also want to see a hex dump ? */
            if (ndo->ndo_vflag > 1 || hexdump==TRUE) {
                print_unknown_data(ndo, tptr, "\n\t  ",
                                   length);
            }

            tlen -= length;
            tptr += length;
        }
        break;

    case SLOW_OAM_CODE_EVENT_NOTIF:
        /* Sequence number */
        if (tlen < 2)
            goto tooshort;
        ND_PRINT("\n\t  Sequence Number %u", GET_BE_U_2(tptr));
        tlen -= 2;
        tptr += 2;

        /* TLVs */
        while (tlen > 0) {
            ptr.slow_oam_tlv_header = (const struct slow_oam_tlv_header_t *)tptr;
            if (tlen < sizeof(*ptr.slow_oam_tlv_header))
                goto tooshort;
            type = GET_U_1(ptr.slow_oam_tlv_header->type);
            length = GET_U_1(ptr.slow_oam_tlv_header->length);
            ND_PRINT("\n\t  %s Link Event Type (%u), length %u",
                   tok2str(slow_oam_link_event_values, "Reserved",
                           type),
                   type,
                   length);

            if (type == SLOW_OAM_INFO_TYPE_END_OF_TLV) {
                /*
                 * As IEEE Std 802.3-2015 says for the End of TLV Marker,
                 * "(the length and value of the Type 0x00 TLV can be ignored)".
                 */
                return;
            }

            /* length includes the type and length fields */
            if (length < sizeof(struct slow_oam_tlv_header_t)) {
                ND_PRINT("\n\t    ERROR: illegal length - should be >= %zu",
                         sizeof(struct slow_oam_tlv_header_t));
                return;
            }

            if (tlen < length)
                goto tooshort;
            ND_TCHECK_LEN(tptr, length);

            hexdump = FALSE;
            switch (type) {
            case SLOW_OAM_LINK_EVENT_ERR_SYM_PER: /* identical format - fall through */
            case SLOW_OAM_LINK_EVENT_ERR_FRM:
            case SLOW_OAM_LINK_EVENT_ERR_FRM_PER:
            case SLOW_OAM_LINK_EVENT_ERR_FRM_SUMM:
                tlv.slow_oam_link_event = (const struct slow_oam_link_event_t *)tptr;

                if (GET_U_1(tlv.slow_oam_link_event->event_length) !=
                    sizeof(struct slow_oam_link_event_t)) {
                    ND_PRINT("\n\t    ERROR: illegal length - should be %zu",
                             sizeof(struct slow_oam_link_event_t));
                    hexdump = TRUE;
                    goto badlength_event_notif;
                }

                ND_PRINT("\n\t    Timestamp %u ms, Errored Window %" PRIu64
                       "\n\t    Errored Threshold %" PRIu64
                       "\n\t    Errors %" PRIu64
                       "\n\t    Error Running Total %" PRIu64
                       "\n\t    Event Running Total %u",
                       GET_BE_U_2(tlv.slow_oam_link_event->time_stamp)*100,
                       GET_BE_U_8(tlv.slow_oam_link_event->window),
                       GET_BE_U_8(tlv.slow_oam_link_event->threshold),
                       GET_BE_U_8(tlv.slow_oam_link_event->errors),
                       GET_BE_U_8(tlv.slow_oam_link_event->errors_running_total),
                       GET_BE_U_4(tlv.slow_oam_link_event->event_running_total));
                break;

            case SLOW_OAM_LINK_EVENT_ORG_SPECIFIC:
                hexdump = TRUE;
                break;

            default:
                hexdump = TRUE;
                break;
            }

        badlength_event_notif:
            /* do we also want to see a hex dump ? */
            if (ndo->ndo_vflag > 1 || hexdump==TRUE) {
                print_unknown_data(ndo, tptr, "\n\t  ",
                                   length);
            }

            tlen -= length;
            tptr += length;
        }
        break;

    case SLOW_OAM_CODE_LOOPBACK_CTRL:
        tlv.slow_oam_loopbackctrl = (const struct slow_oam_loopbackctrl_t *)tptr;
        if (tlen < sizeof(*tlv.slow_oam_loopbackctrl))
            goto tooshort;
        command = GET_U_1(tlv.slow_oam_loopbackctrl->command);
        ND_PRINT("\n\t  Command %s (%u)",
               tok2str(slow_oam_loopbackctrl_cmd_values,
                       "Unknown",
                       command),
               command);
        tptr ++;
        tlen --;
        break;

        /*
         * FIXME those are the defined codes that lack a decoder
         * you are welcome to contribute code ;-)
         */
    case SLOW_OAM_CODE_VAR_REQUEST:
    case SLOW_OAM_CODE_VAR_RESPONSE:
    case SLOW_OAM_CODE_PRIVATE:
    default:
        if (ndo->ndo_vflag <= 1) {
            print_unknown_data(ndo, tptr, "\n\t  ", tlen);
        }
        break;
    }
    return;

tooshort:
    ND_PRINT("\n\t\t packet is too short");
}
