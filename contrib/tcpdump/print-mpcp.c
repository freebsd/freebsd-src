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
 * Original code by Hannes Gredler (hannes@gredler.at)
 */

/* \summary: IEEE 802.3ah Multi-Point Control Protocol (MPCP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

struct mpcp_common_header_t {
    nd_uint16_t opcode;
    nd_uint32_t timestamp;
};

#define	MPCP_OPCODE_PAUSE   0x0001
#define	MPCP_OPCODE_GATE    0x0002
#define	MPCP_OPCODE_REPORT  0x0003
#define	MPCP_OPCODE_REG_REQ 0x0004
#define	MPCP_OPCODE_REG     0x0005
#define	MPCP_OPCODE_REG_ACK 0x0006

static const struct tok mpcp_opcode_values[] = {
    { MPCP_OPCODE_PAUSE, "Pause" },
    { MPCP_OPCODE_GATE, "Gate" },
    { MPCP_OPCODE_REPORT, "Report" },
    { MPCP_OPCODE_REG_REQ, "Register Request" },
    { MPCP_OPCODE_REG, "Register" },
    { MPCP_OPCODE_REG_ACK, "Register ACK" },
    { 0, NULL}
};

#define MPCP_GRANT_NUMBER_LEN 1
#define	MPCP_GRANT_NUMBER_MASK 0x7
static const struct tok mpcp_grant_flag_values[] = {
    { 0x08, "Discovery" },
    { 0x10, "Force Grant #1" },
    { 0x20, "Force Grant #2" },
    { 0x40, "Force Grant #3" },
    { 0x80, "Force Grant #4" },
    { 0, NULL}
};

struct mpcp_grant_t {
    nd_uint32_t starttime;
    nd_uint16_t duration;
};

struct mpcp_reg_req_t {
    nd_uint8_t flags;
    nd_uint8_t pending_grants;
};


static const struct tok mpcp_reg_req_flag_values[] = {
    { 1, "Register" },
    { 3, "De-Register" },
    { 0, NULL}
};

struct mpcp_reg_t {
    nd_uint16_t assigned_port;
    nd_uint8_t  flags;
    nd_uint16_t sync_time;
    nd_uint8_t  echoed_pending_grants;
};

static const struct tok mpcp_reg_flag_values[] = {
    { 1, "Re-Register" },
    { 2, "De-Register" },
    { 3, "ACK" },
    { 4, "NACK" },
    { 0, NULL}
};

#define MPCP_REPORT_QUEUESETS_LEN    1
#define MPCP_REPORT_REPORTBITMAP_LEN 1
static const struct tok mpcp_report_bitmap_values[] = {
    { 0x01, "Q0" },
    { 0x02, "Q1" },
    { 0x04, "Q2" },
    { 0x08, "Q3" },
    { 0x10, "Q4" },
    { 0x20, "Q5" },
    { 0x40, "Q6" },
    { 0x80, "Q7" },
    { 0, NULL}
};

struct mpcp_reg_ack_t {
    nd_uint8_t  flags;
    nd_uint16_t echoed_assigned_port;
    nd_uint16_t echoed_sync_time;
};

static const struct tok mpcp_reg_ack_flag_values[] = {
    { 0, "NACK" },
    { 1, "ACK" },
    { 0, NULL}
};

void
mpcp_print(netdissect_options *ndo, const u_char *pptr, u_int length)
{
    const struct mpcp_common_header_t *mpcp_common_header;
    const struct mpcp_reg_req_t *mpcp_reg_req;
    const struct mpcp_reg_t *mpcp_reg;
    const struct mpcp_reg_ack_t *mpcp_reg_ack;


    const u_char *tptr;
    uint16_t opcode;
    uint32_t timestamp;
    uint8_t grant_numbers, grant;
    uint8_t queue_sets, queue_set, report_bitmap, report;

    ndo->ndo_protocol = "mpcp";
    tptr=pptr;
    mpcp_common_header = (const struct mpcp_common_header_t *)pptr;

    opcode = GET_BE_U_2(mpcp_common_header->opcode);
    timestamp = GET_BE_U_4(mpcp_common_header->timestamp);
    ND_PRINT("MPCP, Opcode %s", tok2str(mpcp_opcode_values, "Unknown (%u)", opcode));
    if (opcode != MPCP_OPCODE_PAUSE) {
        ND_PRINT(", Timestamp %u ticks", timestamp);
    }
    ND_PRINT(", length %u", length);

    if (!ndo->ndo_vflag)
        return;

    tptr += sizeof(struct mpcp_common_header_t);

    switch (opcode) {
    case MPCP_OPCODE_PAUSE:
        break;

    case MPCP_OPCODE_GATE:
        grant_numbers = GET_U_1(tptr) & MPCP_GRANT_NUMBER_MASK;
        ND_PRINT("\n\tGrant Numbers %u, Flags [ %s ]",
               grant_numbers,
               bittok2str(mpcp_grant_flag_values,
                          "?",
                          GET_U_1(tptr) & ~MPCP_GRANT_NUMBER_MASK));
        tptr++;

        for (grant = 1; grant <= grant_numbers; grant++) {
            const struct mpcp_grant_t *mpcp_grant = (const struct mpcp_grant_t *)tptr;
            ND_PRINT("\n\tGrant #%u, Start-Time %u ticks, duration %u ticks",
                   grant,
                   GET_BE_U_4(mpcp_grant->starttime),
                   GET_BE_U_2(mpcp_grant->duration));
            tptr += sizeof(struct mpcp_grant_t);
        }

        ND_PRINT("\n\tSync-Time %u ticks", GET_BE_U_2(tptr));
        break;


    case MPCP_OPCODE_REPORT:
        queue_sets = GET_U_1(tptr);
        tptr+=MPCP_REPORT_QUEUESETS_LEN;
        ND_PRINT("\n\tTotal Queue-Sets %u", queue_sets);

        for (queue_set = 1; queue_set < queue_sets; queue_set++) {
            report_bitmap = GET_U_1(tptr);
            ND_PRINT("\n\t  Queue-Set #%u, Report-Bitmap [ %s ]",
                   queue_sets,
                   bittok2str(mpcp_report_bitmap_values, "Unknown", report_bitmap));
            tptr++;

            report=1;
            while (report_bitmap != 0) {
                if (report_bitmap & 1) {
                    ND_PRINT("\n\t    Q%u Report, Duration %u ticks",
                           report,
                           GET_BE_U_2(tptr));
                    tptr += 2;
                }
                report++;
                report_bitmap = report_bitmap >> 1;
            }
        }
        break;

    case MPCP_OPCODE_REG_REQ:
        mpcp_reg_req = (const struct mpcp_reg_req_t *)tptr;
        ND_PRINT("\n\tFlags [ %s ], Pending-Grants %u",
               bittok2str(mpcp_reg_req_flag_values, "Reserved", GET_U_1(mpcp_reg_req->flags)),
               GET_U_1(mpcp_reg_req->pending_grants));
        break;

    case MPCP_OPCODE_REG:
        mpcp_reg = (const struct mpcp_reg_t *)tptr;
        ND_PRINT("\n\tAssigned-Port %u, Flags [ %s ]"
               "\n\tSync-Time %u ticks, Echoed-Pending-Grants %u",
               GET_BE_U_2(mpcp_reg->assigned_port),
               bittok2str(mpcp_reg_flag_values, "Reserved", GET_U_1(mpcp_reg->flags)),
               GET_BE_U_2(mpcp_reg->sync_time),
               GET_U_1(mpcp_reg->echoed_pending_grants));
        break;

    case MPCP_OPCODE_REG_ACK:
        mpcp_reg_ack = (const struct mpcp_reg_ack_t *)tptr;
        ND_PRINT("\n\tEchoed-Assigned-Port %u, Flags [ %s ]"
               "\n\tEchoed-Sync-Time %u ticks",
               GET_BE_U_2(mpcp_reg_ack->echoed_assigned_port),
               bittok2str(mpcp_reg_ack_flag_values, "Reserved", GET_U_1(mpcp_reg_ack->flags)),
               GET_BE_U_2(mpcp_reg_ack->echoed_sync_time));
        break;

    default:
        /* unknown opcode - hexdump for now */
        print_unknown_data(ndo,pptr, "\n\t", length);
        break;
    }
}
