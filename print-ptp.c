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
 * Original code by Partha S. Ghosh (psglinux dot gmail dot com)
 */

/* \summary: Precision Time Protocol (PTP) printer */

/* specification: https://standards.ieee.org/findstds/standard/1588-2008.html*/

#include <config.h>

#include "netdissect-stdinc.h"
#include "netdissect.h"
#include "extract.h"

/*
 * PTP header
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |  R  | |msgtype|  version      |  Msg Len                      |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |  domain No    | rsvd1         |   flag Field                  |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        Correction NS                          |
 *    +                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                               |      Correction Sub NS        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                           Reserved2                           |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        Clock Identity                         |
 *    |                                                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |         Port Identity         |         Sequence ID           |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |    control    |  log msg int  |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     0                   1                   2                   3
 *
 * Announce Message (msg type=0xB)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |                            Seconds                            |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                         Nano Seconds                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |     Origin Cur UTC Offset     |     Reserved    | GM Prio 1   |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |GM Clock Class | GM Clock Accu |        GM Clock Variance      |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |   GM Prio 2   |                                               |
 *    +-+-+-+-+-+-+-+-+                                               +
 *    |                      GM Clock Identity                        |
 *    +               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |               |         Steps Removed           | Time Source |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     0                   1                   2                   3
 *
 * Sync Message (msg type=0x0)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |                            Seconds                            |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                         Nano Seconds                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  Delay Request Message (msg type=0x1)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |             Origin Time Stamp Seconds                         |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                         Nano Seconds                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  Followup Message (msg type=0x8)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |      Precise Origin Time Stamp Seconds                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                         Nano Seconds                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  Delay Resp Message (msg type=0x9)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |                            Seconds                            |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                         Nano Seconds                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |          Port Identity        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  PDelay Request Message (msg type=0x2)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |                    Origin Time Stamp Seconds                  |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                  Origin Time Stamp Nano Seconds               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |          Port Identity        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  PDelay Response Message (msg type=0x3)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |     Request receipt Time Stamp Seconds                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                         Nano Seconds                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    | Requesting Port Identity      |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  PDelay Resp Follow up Message (msg type=0xA)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    |                               |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *    |      Response Origin Time Stamp Seconds                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                         Nano Seconds                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    | Requesting Port Identity      |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  Signaling Message (msg type=0xC)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    | Requesting Port Identity      |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  Management Message (msg type=0xD)
 *     0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                    | Requesting Port Identity      |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |Start Bndry Hps| Boundary Hops | flags         | Reserved      |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

/* Values from IEEE1588-2008: 13.3.2.2 messageType (Enumeration4) */
#define M_SYNC                  0x0
#define M_DELAY_REQ             0x1
#define M_PDELAY_REQ            0x2
#define M_PDELAY_RESP           0x3
#define M_FOLLOW_UP             0x8
#define M_DELAY_RESP            0x9
#define M_PDELAY_RESP_FOLLOW_UP 0xA
#define M_ANNOUNCE              0xB
#define M_SIGNALING             0xC
#define M_MANAGEMENT            0xD

static const struct tok ptp_msg_type[] = {
    { M_SYNC, "sync msg"},
    { M_DELAY_REQ, "delay req msg"},
    { M_PDELAY_REQ, "peer delay req msg"},
    { M_PDELAY_RESP, "peer delay resp msg"},
    { M_FOLLOW_UP, "follow up msg"},
    { M_DELAY_RESP, "delay resp msg"},
    { M_PDELAY_RESP_FOLLOW_UP, "pdelay resp fup msg"},
    { M_ANNOUNCE, "announce msg"},
    { M_SIGNALING, "signaling msg"},
    { M_MANAGEMENT, "management msg"},
    { 0, NULL}
};

/* Values from IEEE1588-2008: 13.3.2.10 controlField (UInteger8) */
/*
 * The use of this field by the receiver is deprecated.
 * NOTE-This field is provided for compatibility with hardware designed
 * to conform to version 1 of this standard.
 */
#define C_SYNC              0x0
#define C_DELAY_REQ         0x1
#define C_FOLLOW_UP         0x2
#define C_DELAY_RESP        0x3
#define C_MANAGEMENT        0x4
#define C_OTHER             0x5

static const struct tok ptp_control_field[] = {
    { C_SYNC, "Sync"},
    { C_DELAY_REQ, "Delay_Req"},
    { C_FOLLOW_UP, "Follow_Up"},
    { C_DELAY_RESP, "Delay_Resp"},
    { C_MANAGEMENT, "Management"},
    { C_OTHER, "Other"},
    { 0, NULL}
};

#define PTP_TRUE 1
#define PTP_FALSE !PTP_TRUE

#define PTP_HDR_LEN         0x22

/* mask based on the first byte */
#define PTP_MAJOR_VERS_MASK 0x0F
#define PTP_MINOR_VERS_MASK 0xF0
#define PTP_MAJOR_SDO_ID_MASK   0xF0
#define PTP_MSG_TYPE_MASK   0x0F

/*mask based 2byte */
#define PTP_DOMAIN_MASK     0xFF00
#define PTP_RSVD1_MASK      0xFF
#define PTP_CONTROL_MASK    0xFF
#define PTP_LOGMSG_MASK     0xFF

/* mask based on the flags 2 bytes */

#define PTP_L161_MASK               0x1
#define PTP_L1_59_MASK              0x2
#define PTP_UTC_REASONABLE_MASK     0x4
#define PTP_TIMESCALE_MASK          0x8
#define PTP_TIME_TRACABLE_MASK      0x10
#define PTP_FREQUENCY_TRACABLE_MASK 0x20
#define PTP_ALTERNATE_MASTER_MASK   0x100
#define PTP_TWO_STEP_MASK           0x200
#define PTP_UNICAST_MASK            0x400
#define PTP_PROFILE_SPEC_1_MASK     0x1000
#define PTP_PROFILE_SPEC_2_MASK     0x2000
#define PTP_SECURITY_MASK           0x4000
#define PTP_FLAGS_UNKNOWN_MASK      0x18C0

static const struct tok ptp_flag_values[] = {
    { PTP_L161_MASK, "l1 61"},
    { PTP_L1_59_MASK, "l1 59"},
    { PTP_UTC_REASONABLE_MASK, "utc reasonable"},
    { PTP_TIMESCALE_MASK, "timescale"},
    { PTP_TIME_TRACABLE_MASK, "time tracable"},
    { PTP_FREQUENCY_TRACABLE_MASK, "frequency tracable"},
    { PTP_ALTERNATE_MASTER_MASK, "alternate master"},
    { PTP_TWO_STEP_MASK, "two step"},
    { PTP_UNICAST_MASK, "unicast"},
    { PTP_PROFILE_SPEC_1_MASK, "profile specific 1"},
    { PTP_PROFILE_SPEC_2_MASK, "profile specific 2"},
    { PTP_SECURITY_MASK, "security mask"},
    { PTP_FLAGS_UNKNOWN_MASK,  "unknown"},
    {0, NULL}
};

static const char *p_porigin_ts = "preciseOriginTimeStamp";
static const char *p_origin_ts = "originTimeStamp";
static const char *p_recv_ts = "receiveTimeStamp";

#define PTP_VER_1 0x1
#define PTP_VER_2 0x2

#define PTP_UCHAR_LEN  sizeof(uint8_t)
#define PTP_UINT16_LEN sizeof(uint16_t)
#define PTP_UINT32_LEN sizeof(uint32_t)
#define PTP_6BYTES_LEN sizeof(uint32_t)+sizeof(uint16_t)
#define PTP_UINT64_LEN sizeof(uint64_t)

static void ptp_print_1(netdissect_options *ndo);
static void ptp_print_2(netdissect_options *ndo, const u_char *bp, u_int len);

static void ptp_print_timestamp(netdissect_options *ndo, const u_char *bp, u_int *len, const char *stype);
static void ptp_print_timestamp_identity(netdissect_options *ndo, const u_char *bp, u_int *len, const char *ttype);
static void ptp_print_announce_msg(netdissect_options *ndo, const u_char *bp, u_int *len);
static void ptp_print_port_id(netdissect_options *ndo, const u_char *bp, u_int *len);
static void ptp_print_mgmt_msg(netdissect_options *ndo, const u_char *bp, u_int *len);

static void
print_field(netdissect_options *ndo, const char *st, uint32_t flen,
            const u_char *bp, u_int *len, uint8_t hex)
{
    uint8_t u8_val;
    uint16_t u16_val;
    uint32_t u32_val;
    uint64_t u64_val;

    switch(flen) {
        case PTP_UCHAR_LEN:
            u8_val = GET_U_1(bp);
            ND_PRINT(", %s", st);
            if (hex)
                ND_PRINT(" 0x%x", u8_val);
            else
                ND_PRINT(" %u", u8_val);
            *len -= 1; bp += 1;
            break;
        case PTP_UINT16_LEN:
            u16_val = GET_BE_U_2(bp);
            ND_PRINT(", %s", st);
            if (hex)
                ND_PRINT(" 0x%x", u16_val);
            else
                ND_PRINT(" %u", u16_val);
            *len -= 2; bp += 2;
            break;
        case PTP_UINT32_LEN:
            u32_val = GET_BE_U_4(bp);
            ND_PRINT(", %s", st);
            if (hex)
                ND_PRINT(" 0x%x", u32_val);
            else
                ND_PRINT(" %u", u32_val);
            *len -= 4; bp += 4;
            break;
        case PTP_UINT64_LEN:
            u64_val = GET_BE_U_8(bp);
            ND_PRINT(", %s", st);
            if (hex)
                ND_PRINT(" 0x%"PRIx64, u64_val);
            else
                ND_PRINT(" 0x%"PRIu64, u64_val);
            *len -= 8; bp += 8;
            break;
        default:
            break;
    }
}

static void
ptp_print_1(netdissect_options *ndo)
{
    ND_PRINT(" (not implemented)");
}

static void
ptp_print_2(netdissect_options *ndo, const u_char *bp, u_int length)
{
    u_int len = length;
    uint16_t msg_len, flags, port_id, seq_id;
    uint8_t foct, domain_no, msg_type, major_sdo_id, rsvd1, lm_int, control;
    uint64_t ns_corr;
    uint16_t sns_corr;
    uint32_t rsvd2;
    uint64_t clk_id;

    foct = GET_U_1(bp);
    major_sdo_id = (foct & PTP_MAJOR_SDO_ID_MASK) >> 4;
    ND_PRINT(", majorSdoId : 0x%x", major_sdo_id);
    msg_type = foct & PTP_MSG_TYPE_MASK;
    ND_PRINT(", msg type : %s", tok2str(ptp_msg_type, "Reserved", msg_type));

    /* msg length */
    len -= 2; bp += 2; msg_len = GET_BE_U_2(bp); ND_PRINT(", length : %u", msg_len);

    /* domain */
    len -= 2; bp += 2; domain_no = (GET_BE_U_2(bp) & PTP_DOMAIN_MASK) >> 8; ND_PRINT(", domain : %u", domain_no);

    /* rsvd 1*/
    rsvd1 = GET_BE_U_2(bp) & PTP_RSVD1_MASK;
    ND_PRINT(", reserved1 : %u", rsvd1);

    /* flags */
    len -= 2; bp += 2; flags = GET_BE_U_2(bp); ND_PRINT(", Flags [%s]", bittok2str(ptp_flag_values, "none", flags));

    /* correction NS (48 bits) */
    len -= 2; bp += 2; ns_corr = GET_BE_U_6(bp); ND_PRINT(", NS correction : %"PRIu64, ns_corr);

    /* correction sub NS (16 bits) */
    len -= 6; bp += 6; sns_corr = GET_BE_U_2(bp); ND_PRINT(", sub NS correction : %u", sns_corr);

    /* Reserved 2 */
    len -= 2; bp += 2; rsvd2 = GET_BE_U_4(bp); ND_PRINT(", reserved2 : %u", rsvd2);

    /* clock identity */
    len -= 4; bp += 4; clk_id = GET_BE_U_8(bp); ND_PRINT(", clock identity : 0x%"PRIx64, clk_id);

    /* port identity */
    len -= 8; bp += 8; port_id = GET_BE_U_2(bp); ND_PRINT(", port id : %u", port_id);

    /* sequence ID */
    len -= 2; bp += 2; seq_id = GET_BE_U_2(bp); ND_PRINT(", seq id : %u", seq_id);

    /* control */
    len -= 2; bp += 2; control = GET_U_1(bp) ;
    ND_PRINT(", control : %u (%s)", control, tok2str(ptp_control_field, "Reserved", control));

    /* log message interval */
    lm_int = GET_BE_U_2(bp) & PTP_LOGMSG_MASK; ND_PRINT(", log message interval : %u", lm_int); len -= 2; bp += 2;

    switch(msg_type) {
        case M_SYNC:
            ptp_print_timestamp(ndo, bp, &len, p_origin_ts);
            break;
        case M_DELAY_REQ:
            ptp_print_timestamp(ndo, bp, &len, p_origin_ts);
            break;
        case M_PDELAY_REQ:
            ptp_print_timestamp_identity(ndo, bp, &len, p_porigin_ts);
            break;
        case M_PDELAY_RESP:
            ptp_print_timestamp_identity(ndo, bp, &len, p_recv_ts);
            break;
        case M_FOLLOW_UP:
            ptp_print_timestamp(ndo, bp, &len, p_porigin_ts);
            break;
        case M_DELAY_RESP:
            ptp_print_timestamp_identity(ndo, bp, &len, p_recv_ts);
            break;
        case M_PDELAY_RESP_FOLLOW_UP:
            ptp_print_timestamp_identity(ndo, bp, &len, p_porigin_ts);
            break;
        case M_ANNOUNCE:
            ptp_print_announce_msg(ndo, bp, &len);
            break;
        case M_SIGNALING:
            ptp_print_port_id(ndo, bp, &len);
            break;
        case M_MANAGEMENT:
            ptp_print_mgmt_msg(ndo, bp, &len);
            break;
        default:
            break;
    }
}
/*
 * PTP general message
 */
void
ptp_print(netdissect_options *ndo, const u_char *bp, u_int length)
{
    u_int major_vers;
    u_int minor_vers;

    /* In 1588-2019, a minorVersionPTP field has been created in the common PTP
     * message header, from a previously reserved field. Implementations
     * compatible to the 2019 edition shall indicate a versionPTP field value
     * of 2 and minorVersionPTP field value of 1, indicating that this is PTP
     * version 2.1.
     */
    ndo->ndo_protocol = "ptp";
    ND_ICHECK_U(length, <, PTP_HDR_LEN);
    major_vers = GET_BE_U_2(bp) & PTP_MAJOR_VERS_MASK;
    minor_vers = (GET_BE_U_2(bp) & PTP_MINOR_VERS_MASK) >> 4;
    if (minor_vers)
	    ND_PRINT("PTPv%u.%u", major_vers, minor_vers);
    else
	    ND_PRINT("PTPv%u", major_vers);

    switch(major_vers) {
        case PTP_VER_1:
            ptp_print_1(ndo);
            break;
        case PTP_VER_2:
            ptp_print_2(ndo, bp, length);
            break;
        default:
            //ND_PRINT("ERROR: unknown-version\n");
            break;
    }
    return;

invalid:
    nd_print_invalid(ndo);
}

static void
ptp_print_timestamp(netdissect_options *ndo, const u_char *bp, u_int *len, const char *stype)
{
    uint64_t secs;
    uint32_t nsecs;

    ND_PRINT(", %s :", stype);
    /* sec time stamp 6 bytes */
    secs = GET_BE_U_6(bp);
    ND_PRINT(" %"PRIu64" seconds,", secs);
    *len -= 6;
    bp += 6;

    /* NS time stamp 4 bytes */
    nsecs = GET_BE_U_4(bp);
    ND_PRINT(" %u nanoseconds", nsecs);
    *len -= 4;
    bp += 4;
}
static void
ptp_print_timestamp_identity(netdissect_options *ndo,
                            const u_char *bp, u_int *len, const char *ttype)
{
    uint64_t secs;
    uint32_t nsecs;
    uint16_t port_id;
    uint64_t port_identity;

    ND_PRINT(", %s :", ttype);
    /* sec time stamp 6 bytes */
    secs = GET_BE_U_6(bp);
    ND_PRINT(" %"PRIu64" seconds,", secs);
    *len -= 6;
    bp += 6;

    /* NS time stamp 4 bytes */
    nsecs = GET_BE_U_4(bp);
    ND_PRINT(" %u nanoseconds", nsecs);
    *len -= 4;
    bp += 4;

    /* port identity*/
    port_identity = GET_BE_U_8(bp);
    ND_PRINT(", port identity : 0x%"PRIx64, port_identity);
    *len -= 8;
    bp += 8;

    /* port id */
    port_id = GET_BE_U_2(bp);
    ND_PRINT(", port id : %u", port_id);
    *len -= 2;
    bp += 2;
}
static void
ptp_print_announce_msg(netdissect_options *ndo, const u_char *bp, u_int *len)
{
    uint8_t rsvd, gm_prio_1, gm_prio_2, gm_clk_cls, gm_clk_acc, time_src;
    uint16_t origin_cur_utc, gm_clk_var, steps_removed;
    uint64_t gm_clock_id;
    uint64_t secs;
    uint32_t nsecs;

    ND_PRINT(", %s :", p_origin_ts);
    /* sec time stamp 6 bytes */
    secs = GET_BE_U_6(bp);
    ND_PRINT(" %"PRIu64" seconds", secs);
    *len -= 6;
    bp += 6;

    /* NS time stamp 4 bytes */
    nsecs = GET_BE_U_4(bp);
    ND_PRINT(" %u nanoseconds", nsecs);
    *len -= 4;
    bp += 4;

    /* origin cur utc */
    origin_cur_utc = GET_BE_U_2(bp);
    ND_PRINT(", origin cur utc :%u", origin_cur_utc);
    *len -= 2;
    bp += 2;

    /* rsvd */
    rsvd = GET_U_1(bp);
    ND_PRINT(", rsvd : %u", rsvd);
    *len -= 1;
    bp += 1;

    /* gm prio */
    gm_prio_1 = GET_U_1(bp);
    ND_PRINT(", gm priority_1 : %u", gm_prio_1);
    *len -= 1;
    bp += 1;

    /* GM clock class */
    gm_clk_cls = GET_U_1(bp);
    ND_PRINT(", gm clock class : %u", gm_clk_cls);
    *len -= 1;
    bp += 1;
    /* GM clock accuracy */
    gm_clk_acc = GET_U_1(bp);
    ND_PRINT(", gm clock accuracy : %u", gm_clk_acc);
    *len -= 1;
    bp += 1;
    /* GM clock variance */
    gm_clk_var = GET_BE_U_2(bp);
    ND_PRINT(", gm clock variance : %u", gm_clk_var);
    *len -= 2;
    bp += 2;
    /* GM Prio 2 */
    gm_prio_2 = GET_U_1(bp);
    ND_PRINT(", gm priority_2 : %u", gm_prio_2);
    *len -= 1;
    bp += 1;

    /* GM Clock Identity */
    gm_clock_id = GET_BE_U_8(bp);
    ND_PRINT(", gm clock id : 0x%"PRIx64, gm_clock_id);
    *len -= 8;
    bp += 8;
    /* steps removed */
    steps_removed = GET_BE_U_2(bp);
    ND_PRINT(", steps removed : %u", steps_removed);
    *len -= 2;
    bp += 2;
    /* Time source */
    time_src = GET_U_1(bp);
    ND_PRINT(", time source : 0x%x", time_src);
    *len -= 1;
    bp += 1;

}
static void
ptp_print_port_id(netdissect_options *ndo, const u_char *bp, u_int *len)
{
    uint16_t port_id;
    uint64_t port_identity;

    /* port identity*/
    port_identity = GET_BE_U_8(bp);
    ND_PRINT(", port identity : 0x%"PRIx64, port_identity);
    *len -= 8;
    bp += 8;

    /* port id */
    port_id = GET_BE_U_2(bp);
    ND_PRINT(", port id : %u", port_id);
    *len -= 2;
    bp += 2;

}

static void
ptp_print_mgmt_msg(netdissect_options *ndo, const u_char *bp, u_int *len)
{
    ptp_print_port_id(ndo, bp, len);
    print_field(ndo, ", start boundary hops ", PTP_UCHAR_LEN, bp, len, PTP_FALSE);
    print_field(ndo, ", boundary hops ", PTP_UCHAR_LEN, bp, len, PTP_FALSE);
    print_field(ndo, ", flags ", PTP_UCHAR_LEN, bp, len, PTP_TRUE);
    print_field(ndo, ", reserved ", PTP_UCHAR_LEN, bp, len, PTP_TRUE);
}
