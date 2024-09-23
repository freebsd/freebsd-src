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

/* \summary: Light Weight Access Point Protocol (LWAPP) printer */

/* specification: RFC 5412 */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"


/*
 * LWAPP transport (common) header
 *      0                   1                   2                   3
 *     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |VER| RID |C|F|L|    Frag ID    |            Length             |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |          Status/WLANs         |   Payload...  |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

struct lwapp_transport_header {
    nd_uint8_t  version;
    nd_uint8_t  frag_id;
    nd_uint16_t length;
    nd_uint16_t status;
};

/*
 * LWAPP control header
 *      0                   1                   2                   3
 *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |  Message Type |    Seq Num    |      Msg Element Length       |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |                           Session ID                          |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     |      Msg Element [0..N]       |
 *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct lwapp_control_header {
    nd_uint8_t  msg_type;
    nd_uint8_t  seq_num;
    nd_uint16_t len;
    nd_uint32_t session_id;
};

#define LWAPP_VERSION 0
#define	LWAPP_EXTRACT_VERSION(x) (((x)&0xC0)>>6)
#define	LWAPP_EXTRACT_RID(x) (((x)&0x38)>>3)
#define LWAPP_EXTRACT_CONTROL_BIT(x) (((x)&0x04)>>2)

static const struct tok lwapp_header_bits_values[] = {
    { 0x01, "Last Fragment Bit"},
    { 0x02, "Fragment Bit"},
    { 0x04, "Control Bit"},
    { 0, NULL}
};

#define	LWAPP_MSGTYPE_DISCOVERY_REQUEST			1
#define	LWAPP_MSGTYPE_DISCOVERY_RESPONSE		2
#define	LWAPP_MSGTYPE_JOIN_REQUEST			3
#define LWAPP_MSGTYPE_JOIN_RESPONSE			4
#define LWAPP_MSGTYPE_JOIN_ACK				5
#define LWAPP_MSGTYPE_JOIN_CONFIRM			6
#define LWAPP_MSGTYPE_CONFIGURE_REQUEST			10
#define LWAPP_MSGTYPE_CONFIGURE_RESPONSE		11
#define LWAPP_MSGTYPE_CONF_UPDATE_REQUEST		12
#define LWAPP_MSGTYPE_CONF_UPDATE_RESPONSE		13
#define LWAPP_MSGTYPE_WTP_EVENT_REQUEST			14
#define LWAPP_MSGTYPE_WTP_EVENT_RESPONSE		15
#define LWAPP_MSGTYPE_CHANGE_STATE_EVENT_REQUEST	16
#define LWAPP_MSGTYPE_CHANGE_STATE_EVENT_RESPONSE	17
#define LWAPP_MSGTYPE_ECHO_REQUEST			22
#define LWAPP_MSGTYPE_ECHO_RESPONSE			23
#define LWAPP_MSGTYPE_IMAGE_DATA_REQUEST		24
#define LWAPP_MSGTYPE_IMAGE_DATA_RESPONSE		25
#define LWAPP_MSGTYPE_RESET_REQUEST			26
#define LWAPP_MSGTYPE_RESET_RESPONSE			27
#define LWAPP_MSGTYPE_KEY_UPDATE_REQUEST		30
#define LWAPP_MSGTYPE_KEY_UPDATE_RESPONSE		31
#define LWAPP_MSGTYPE_PRIMARY_DISCOVERY_REQUEST		32
#define LWAPP_MSGTYPE_PRIMARY_DISCOVERY_RESPONSE	33
#define LWAPP_MSGTYPE_DATA_TRANSFER_REQUEST		34
#define LWAPP_MSGTYPE_DATA_TRANSFER_RESPONSE		35
#define LWAPP_MSGTYPE_CLEAR_CONFIG_INDICATION		36
#define LWAPP_MSGTYPE_WLAN_CONFIG_REQUEST		37
#define LWAPP_MSGTYPE_WLAN_CONFIG_RESPONSE		38
#define LWAPP_MSGTYPE_MOBILE_CONFIG_REQUEST		39
#define LWAPP_MSGTYPE_MOBILE_CONFIG_RESPONSE		40

static const struct tok lwapp_msg_type_values[] = {
    { LWAPP_MSGTYPE_DISCOVERY_REQUEST, "Discovery req"},
    { LWAPP_MSGTYPE_DISCOVERY_RESPONSE, "Discovery resp"},
    { LWAPP_MSGTYPE_JOIN_REQUEST, "Join req"},
    { LWAPP_MSGTYPE_JOIN_RESPONSE, "Join resp"},
    { LWAPP_MSGTYPE_JOIN_ACK, "Join ack"},
    { LWAPP_MSGTYPE_JOIN_CONFIRM, "Join confirm"},
    { LWAPP_MSGTYPE_CONFIGURE_REQUEST, "Configure req"},
    { LWAPP_MSGTYPE_CONFIGURE_RESPONSE, "Configure resp"},
    { LWAPP_MSGTYPE_CONF_UPDATE_REQUEST, "Update req"},
    { LWAPP_MSGTYPE_CONF_UPDATE_RESPONSE, "Update resp"},
    { LWAPP_MSGTYPE_WTP_EVENT_REQUEST, "WTP event req"},
    { LWAPP_MSGTYPE_WTP_EVENT_RESPONSE, "WTP event resp"},
    { LWAPP_MSGTYPE_CHANGE_STATE_EVENT_REQUEST, "Change state event req"},
    { LWAPP_MSGTYPE_CHANGE_STATE_EVENT_RESPONSE, "Change state event resp"},
    { LWAPP_MSGTYPE_ECHO_REQUEST, "Echo req"},
    { LWAPP_MSGTYPE_ECHO_RESPONSE, "Echo resp"},
    { LWAPP_MSGTYPE_IMAGE_DATA_REQUEST, "Image data req"},
    { LWAPP_MSGTYPE_IMAGE_DATA_RESPONSE, "Image data resp"},
    { LWAPP_MSGTYPE_RESET_REQUEST, "Channel status req"},
    { LWAPP_MSGTYPE_RESET_RESPONSE, "Channel status resp"},
    { LWAPP_MSGTYPE_KEY_UPDATE_REQUEST, "Key update req"},
    { LWAPP_MSGTYPE_KEY_UPDATE_RESPONSE, "Key update resp"},
    { LWAPP_MSGTYPE_PRIMARY_DISCOVERY_REQUEST, "Primary discovery req"},
    { LWAPP_MSGTYPE_PRIMARY_DISCOVERY_RESPONSE, "Primary discovery resp"},
    { LWAPP_MSGTYPE_DATA_TRANSFER_REQUEST, "Data transfer req"},
    { LWAPP_MSGTYPE_DATA_TRANSFER_RESPONSE, "Data transfer resp"},
    { LWAPP_MSGTYPE_CLEAR_CONFIG_INDICATION, "Clear config ind"},
    { LWAPP_MSGTYPE_WLAN_CONFIG_REQUEST, "Wlan config req"},
    { LWAPP_MSGTYPE_WLAN_CONFIG_RESPONSE, "Wlan config resp"},
    { LWAPP_MSGTYPE_MOBILE_CONFIG_REQUEST, "Mobile config req"},
    { LWAPP_MSGTYPE_MOBILE_CONFIG_RESPONSE, "Mobile config resp"},
    { 0, NULL}
};

/*
 * LWAPP message elements
 *
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Type     |             Length            |   Value ...   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lwapp_message_header {
    nd_uint8_t  type;
    nd_uint16_t length;
};

void
lwapp_control_print(netdissect_options *ndo,
                    const u_char *pptr, u_int len, int has_ap_ident)
{
    const struct lwapp_transport_header *lwapp_trans_header;
    const struct lwapp_control_header *lwapp_control_header;
    const u_char *tptr;
    uint8_t version;
    u_int tlen;
    u_int msg_type, msg_tlen;

    ndo->ndo_protocol = "lwapp_control";
    tptr=pptr;

    if (has_ap_ident) {
        /* check if enough bytes for AP identity */
        ND_TCHECK_6(tptr);
        lwapp_trans_header = (const struct lwapp_transport_header *)(pptr+6);
    } else {
        lwapp_trans_header = (const struct lwapp_transport_header *)pptr;
    }
    ND_TCHECK_SIZE(lwapp_trans_header);
    version = GET_U_1(lwapp_trans_header->version);

    /*
     * Sanity checking of the header.
     */
    if (LWAPP_EXTRACT_VERSION(version) != LWAPP_VERSION) {
	ND_PRINT("LWAPP version %u packet not supported",
               LWAPP_EXTRACT_VERSION(version));
	return;
    }

    /* non-verbose */
    if (ndo->ndo_vflag < 1) {
        ND_PRINT("LWAPPv%u, %s frame, Flags [%s], length %u",
               LWAPP_EXTRACT_VERSION(version),
               LWAPP_EXTRACT_CONTROL_BIT(version) ? "Control" : "Data",
               bittok2str(lwapp_header_bits_values,"none",version&0x07),
               len);
        return;
    }

    /* ok they seem to want to know everything - lets fully decode it */
    tlen=GET_BE_U_2(lwapp_trans_header->length);

    ND_PRINT("LWAPPv%u, %s frame, Radio-id %u, Flags [%s], Frag-id %u, length %u",
           LWAPP_EXTRACT_VERSION(version),
           LWAPP_EXTRACT_CONTROL_BIT(version) ? "Control" : "Data",
           LWAPP_EXTRACT_RID(version),
           bittok2str(lwapp_header_bits_values,"none",version&0x07),
	   GET_U_1(lwapp_trans_header->frag_id),
	   tlen);

    if (has_ap_ident) {
        ND_PRINT("\n\tAP identity: %s", GET_ETHERADDR_STRING(tptr));
        tptr+=sizeof(struct lwapp_transport_header)+6;
    } else {
        tptr+=sizeof(struct lwapp_transport_header);
    }

    while(tlen!=0) {

        /* did we capture enough for fully decoding the object header ? */
        ND_TCHECK_LEN(tptr, sizeof(struct lwapp_control_header));
        if (tlen < sizeof(struct lwapp_control_header)) {
            ND_PRINT("\n\t  Msg goes past end of PDU");
            break;
        }

        lwapp_control_header = (const struct lwapp_control_header *)tptr;
	msg_tlen = GET_BE_U_2(lwapp_control_header->len);
        if (tlen < sizeof(struct lwapp_control_header) + msg_tlen) {
            ND_PRINT("\n\t  Msg goes past end of PDU");
            break;
        }

	/* print message header */
	msg_type = GET_U_1(lwapp_control_header->msg_type);
        ND_PRINT("\n\t  Msg type: %s (%u), Seqnum: %u, Msg len: %u, Session: 0x%08x",
               tok2str(lwapp_msg_type_values,"Unknown",msg_type),
               msg_type,
               GET_U_1(lwapp_control_header->seq_num),
               msg_tlen,
               GET_BE_U_4(lwapp_control_header->session_id));

        /* did we capture enough for fully decoding the message */
        ND_TCHECK_LEN(tptr, msg_tlen);

	/* XXX - Decode sub messages for each message */
        switch(msg_type) {
        case LWAPP_MSGTYPE_DISCOVERY_REQUEST:
        case LWAPP_MSGTYPE_DISCOVERY_RESPONSE:
        case LWAPP_MSGTYPE_JOIN_REQUEST:
        case LWAPP_MSGTYPE_JOIN_RESPONSE:
        case LWAPP_MSGTYPE_JOIN_ACK:
        case LWAPP_MSGTYPE_JOIN_CONFIRM:
        case LWAPP_MSGTYPE_CONFIGURE_REQUEST:
        case LWAPP_MSGTYPE_CONFIGURE_RESPONSE:
        case LWAPP_MSGTYPE_CONF_UPDATE_REQUEST:
        case LWAPP_MSGTYPE_CONF_UPDATE_RESPONSE:
        case LWAPP_MSGTYPE_WTP_EVENT_REQUEST:
        case LWAPP_MSGTYPE_WTP_EVENT_RESPONSE:
        case LWAPP_MSGTYPE_CHANGE_STATE_EVENT_REQUEST:
        case LWAPP_MSGTYPE_CHANGE_STATE_EVENT_RESPONSE:
        case LWAPP_MSGTYPE_ECHO_REQUEST:
        case LWAPP_MSGTYPE_ECHO_RESPONSE:
        case LWAPP_MSGTYPE_IMAGE_DATA_REQUEST:
        case LWAPP_MSGTYPE_IMAGE_DATA_RESPONSE:
        case LWAPP_MSGTYPE_RESET_REQUEST:
        case LWAPP_MSGTYPE_RESET_RESPONSE:
        case LWAPP_MSGTYPE_KEY_UPDATE_REQUEST:
        case LWAPP_MSGTYPE_KEY_UPDATE_RESPONSE:
        case LWAPP_MSGTYPE_PRIMARY_DISCOVERY_REQUEST:
        case LWAPP_MSGTYPE_PRIMARY_DISCOVERY_RESPONSE:
        case LWAPP_MSGTYPE_DATA_TRANSFER_REQUEST:
        case LWAPP_MSGTYPE_DATA_TRANSFER_RESPONSE:
        case LWAPP_MSGTYPE_CLEAR_CONFIG_INDICATION:
        case LWAPP_MSGTYPE_WLAN_CONFIG_REQUEST:
        case LWAPP_MSGTYPE_WLAN_CONFIG_RESPONSE:
        case LWAPP_MSGTYPE_MOBILE_CONFIG_REQUEST:
        case LWAPP_MSGTYPE_MOBILE_CONFIG_RESPONSE:
        default:
            break;
        }

        tptr += sizeof(struct lwapp_control_header) + msg_tlen;
        tlen -= sizeof(struct lwapp_control_header) + msg_tlen;
    }
    return;

trunc:
    nd_print_trunc(ndo);
}

void
lwapp_data_print(netdissect_options *ndo,
                 const u_char *pptr, u_int len)
{
    const struct lwapp_transport_header *lwapp_trans_header;
    const u_char *tptr;
    u_int tlen;
    u_int version;

    ndo->ndo_protocol = "lwapp_data";
    tptr=pptr;

    /* check if enough bytes for AP identity */
    ND_TCHECK_6(tptr);
    lwapp_trans_header = (const struct lwapp_transport_header *)pptr;
    ND_TCHECK_SIZE(lwapp_trans_header);
    version = GET_U_1(lwapp_trans_header->version);

    /*
     * Sanity checking of the header.
     */
    if (LWAPP_EXTRACT_VERSION(version) != LWAPP_VERSION) {
        ND_PRINT("LWAPP version %u packet not supported",
               LWAPP_EXTRACT_VERSION(version));
        return;
    }

    /* non-verbose */
    if (ndo->ndo_vflag < 1) {
        ND_PRINT("LWAPPv%u, %s frame, Flags [%s], length %u",
               LWAPP_EXTRACT_VERSION(version),
               LWAPP_EXTRACT_CONTROL_BIT(version) ? "Control" : "Data",
               bittok2str(lwapp_header_bits_values,"none",version&0x07),
               len);
        return;
    }

    /* ok they seem to want to know everything - lets fully decode it */
    tlen=GET_BE_U_2(lwapp_trans_header->length);
    if (tlen < sizeof(struct lwapp_transport_header)) {
        ND_PRINT("LWAPPv%u, %s frame, Radio-id  %u, Flags [%s], length %u < transport header length",
               LWAPP_EXTRACT_VERSION(version),
               LWAPP_EXTRACT_CONTROL_BIT(version) ? "Control" : "Data",
               LWAPP_EXTRACT_RID(version),
               bittok2str(lwapp_header_bits_values,"none",version&0x07),
               tlen);
        return;
    }

    ND_PRINT("LWAPPv%u, %s frame, Radio-id  %u, Flags [%s], Frag-id  %u, length %u",
           LWAPP_EXTRACT_VERSION(version),
           LWAPP_EXTRACT_CONTROL_BIT(version) ? "Control" : "Data",
           LWAPP_EXTRACT_RID(version),
           bittok2str(lwapp_header_bits_values,"none",version&0x07),
           GET_U_1(lwapp_trans_header->frag_id),
           tlen);

    tptr+=sizeof(struct lwapp_transport_header);
    tlen-=sizeof(struct lwapp_transport_header);

    /* FIX - An IEEE 802.11 frame follows - hexdump for now */
    print_unknown_data(ndo, tptr, "\n\t", tlen);

    return;

trunc:
    nd_print_trunc(ndo);
}
