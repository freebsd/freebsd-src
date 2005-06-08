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
 * Original code by Hannes Gredler (hannes@juniper.net)
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-ldp.c,v 1.4.2.2 2003/11/16 08:51:31 guy Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"

/*
 * ldp common header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Version                      |         PDU Length            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         LDP Identifier                        |
 * +                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */

struct ldp_common_header {
    u_int8_t version[2];
    u_int8_t pdu_length[2];
    u_int8_t lsr_id[4];
    u_int8_t label_space[2];
};

#define LDP_VERSION 1

/*
 * ldp message header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |U|   Message Type              |      Message Length           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Message ID                                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                     Mandatory Parameters                      |
 * +                                                               +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                     Optional Parameters                       |
 * +                                                               +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct ldp_msg_header {
    u_int8_t type[2];
    u_int8_t length[2];
    u_int8_t id[4];
};

#define	LDP_MASK_MSG_TYPE(x)  ((x)&0x7fff) 
#define	LDP_MASK_U_BIT(x)     ((x)&0x8000) 

#define	LDP_MSG_NOTIF                0x0001
#define	LDP_MSG_HELLO                0x0100
#define	LDP_MSG_INIT                 0x0200
#define	LDP_MSG_KEEPALIVE            0x0201
#define	LDP_MSG_ADDRESS              0x0300
#define	LDP_MSG_ADDRESS_WITHDRAW     0x0301
#define	LDP_MSG_LABEL_MAPPING        0x0400
#define	LDP_MSG_LABEL_REQUEST        0x0401
#define	LDP_MSG_LABEL_WITHDRAW       0x0402
#define	LDP_MSG_LABEL_RELEASE        0x0403
#define	LDP_MSG_LABEL_ABORT_REQUEST  0x0404

#define	LDP_VENDOR_PRIVATE_MIN       0x3e00
#define	LDP_VENDOR_PRIVATE_MAX       0x3eff
#define	LDP_EXPERIMENTAL_MIN         0x3f00
#define	LDP_EXPERIMENTAL_MAX         0x3fff

static const struct tok ldp_msg_values[] = {
    { LDP_MSG_NOTIF,	             "Notification" },
    { LDP_MSG_HELLO,	             "Hello" },
    { LDP_MSG_INIT,	             "Initialization" },
    { LDP_MSG_KEEPALIVE,             "Keepalive" },
    { LDP_MSG_ADDRESS,	             "Address" },
    { LDP_MSG_ADDRESS_WITHDRAW,	     "Address Widthdraw" },
    { LDP_MSG_LABEL_MAPPING,	     "Label Mapping" },
    { LDP_MSG_LABEL_REQUEST,	     "Label Request" },
    { LDP_MSG_LABEL_WITHDRAW,	     "Label Withdraw" },
    { LDP_MSG_LABEL_RELEASE,	     "Label Release" },
    { LDP_MSG_LABEL_ABORT_REQUEST,   "Label Abort Request" },
    { 0, NULL}
};

#define	LDP_MASK_TLV_TYPE(x)  ((x)&0x3fff) 
#define	LDP_MASK_F_BIT(x) ((x)&0x4000) 

#define	LDP_TLV_FEC                  0x0100
#define	LDP_TLV_ADDRESS_LIST         0x0101
#define	LDP_TLV_HOP_COUNT            0x0103
#define	LDP_TLV_PATH_VECTOR          0x0104
#define	LDP_TLV_GENERIC_LABEL        0x0200
#define	LDP_TLV_ATM_LABEL            0x0201
#define	LDP_TLV_FR_LABEL             0x0202
#define	LDP_TLV_STATUS               0x0300
#define	LDP_TLV_EXTD_STATUS          0x0301
#define	LDP_TLV_RETURNED_PDU         0x0302
#define	LDP_TLV_RETURNED_MSG         0x0303
#define	LDP_TLV_COMMON_HELLO         0x0400
#define	LDP_TLV_IPV4_TRANSPORT_ADDR  0x0401
#define	LDP_TLV_CONFIG_SEQ_NUMBER    0x0402
#define	LDP_TLV_IPV6_TRANSPORT_ADDR  0x0403
#define	LDP_TLV_COMMON_SESSION       0x0500
#define	LDP_TLV_ATM_SESSION_PARM     0x0501
#define	LDP_TLV_FR_SESSION_PARM      0x0502
#define	LDP_TLV_LABEL_REQUEST_MSG_ID 0x0600

static const struct tok ldp_tlv_values[] = {
    { LDP_TLV_FEC,	             "FEC" },
    { LDP_TLV_ADDRESS_LIST,          "Address List" },
    { LDP_TLV_HOP_COUNT,             "Hop Count" },
    { LDP_TLV_PATH_VECTOR,           "Path Vector" },
    { LDP_TLV_GENERIC_LABEL,         "Generic Label" },
    { LDP_TLV_ATM_LABEL,             "ATM Label" },
    { LDP_TLV_FR_LABEL,              "Frame-Relay Label" },
    { LDP_TLV_STATUS,                "Status" },
    { LDP_TLV_EXTD_STATUS,           "Extended Status" },
    { LDP_TLV_RETURNED_PDU,          "Returned PDU" },
    { LDP_TLV_RETURNED_MSG,          "Returned Message" },
    { LDP_TLV_COMMON_HELLO,          "Common Hello Parameters" },
    { LDP_TLV_IPV4_TRANSPORT_ADDR,   "IPv4 Transport Address" },
    { LDP_TLV_CONFIG_SEQ_NUMBER,     "Configuration Sequence Number" },
    { LDP_TLV_IPV6_TRANSPORT_ADDR,   "IPv6 Transport Address" },
    { LDP_TLV_COMMON_SESSION,        "Common Session Parameters" },
    { LDP_TLV_ATM_SESSION_PARM,      "ATM Session Parameters" },
    { LDP_TLV_FR_SESSION_PARM,       "Frame-Relay Session Parameters" },
    { LDP_TLV_LABEL_REQUEST_MSG_ID,  "Label Request Message ID" },
    { 0, NULL}
};

#define FALSE 0
#define TRUE  1

int ldp_tlv_print(register const u_char *);
   
/* 
 * ldp tlv header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |U|F|        Type               |            Length             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * |                             Value                             |
 * ~                                                               ~
 * |                                                               |
 * |                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

int
ldp_tlv_print(register const u_char *tptr) {

    struct ldp_tlv_header {
        u_int8_t type[2];
        u_int8_t length[2];
    };

    const struct ldp_tlv_header *ldp_tlv_header;
    u_short tlv_type,tlv_len,tlv_tlen;

    ldp_tlv_header = (const struct ldp_tlv_header *)tptr;    
    tlv_len=EXTRACT_16BITS(ldp_tlv_header->length);
    tlv_tlen=tlv_len;
    tlv_type=LDP_MASK_TLV_TYPE(EXTRACT_16BITS(ldp_tlv_header->type));

    /* FIXME vendor private / experimental check */
    printf("\n\t    %s TLV (0x%04x), length: %u, Flags: [%s and %s forward if unknown]",
           tok2str(ldp_tlv_values,
                   "Unknown",
                   tlv_type),
           tlv_type,
           tlv_len,
           LDP_MASK_U_BIT(EXTRACT_16BITS(&ldp_tlv_header->type)) ? "continue processing" : "ignore",
           LDP_MASK_F_BIT(EXTRACT_16BITS(&ldp_tlv_header->type)) ? "do" : "don't");

    tptr+=sizeof(struct ldp_tlv_header);

    switch(tlv_type) {

    case LDP_TLV_COMMON_HELLO:
        printf("\n\t      Hold Time: %us, Flags: [%s Hello%s]",
               EXTRACT_16BITS(tptr),
               (EXTRACT_16BITS(tptr+2)&0x8000) ? "Targeted" : "Link",
               (EXTRACT_16BITS(tptr+2)&0x4000) ? ", Request for targeted Hellos" : "");
        break;

    case LDP_TLV_IPV4_TRANSPORT_ADDR:
        printf("\n\t      IPv4 Transport Address: %s", ipaddr_string(tptr));
        break;
#ifdef INET6
    case LDP_TLV_IPV6_TRANSPORT_ADDR:
        printf("\n\t      IPv6 Transport Address: %s", ip6addr_string(tptr));
        break;
#endif
    case LDP_TLV_CONFIG_SEQ_NUMBER:
        printf("\n\t      Sequence Number: %u", EXTRACT_32BITS(tptr));
        break;

    /*
     *  FIXME those are the defined TLVs that lack a decoder
     *  you are welcome to contribute code ;-)
     */

    case LDP_TLV_FEC:
    case LDP_TLV_ADDRESS_LIST:
    case LDP_TLV_HOP_COUNT:
    case LDP_TLV_PATH_VECTOR:
    case LDP_TLV_GENERIC_LABEL:
    case LDP_TLV_ATM_LABEL:
    case LDP_TLV_FR_LABEL:
    case LDP_TLV_STATUS:
    case LDP_TLV_EXTD_STATUS:
    case LDP_TLV_RETURNED_PDU:
    case LDP_TLV_RETURNED_MSG:
    case LDP_TLV_COMMON_SESSION:
    case LDP_TLV_ATM_SESSION_PARM:
    case LDP_TLV_FR_SESSION_PARM:
    case LDP_TLV_LABEL_REQUEST_MSG_ID:

    default:
        if (vflag <= 1)
            print_unknown_data(tptr,"\n\t      ",tlv_tlen);
        break;
    }
    return(tlv_len+4); /* Type & Length fields not included */
}

void
ldp_print(register const u_char *pptr, register u_int len) {

    const struct ldp_common_header *ldp_com_header;
    const struct ldp_msg_header *ldp_msg_header;
    const u_char *tptr,*msg_tptr;
    u_short tlen;
    u_short msg_len,msg_type,msg_tlen;
    int hexdump,processed;

    tptr=pptr;
    ldp_com_header = (const struct ldp_common_header *)pptr;
    TCHECK(*ldp_com_header);

    /*
     * Sanity checking of the header.
     */
    if (EXTRACT_16BITS(&ldp_com_header->version) != LDP_VERSION) {
	printf("LDP version %u packet not supported",
               EXTRACT_16BITS(&ldp_com_header->version));
	return;
    }

    /* print the LSR-ID, label-space & length */
    printf("%sLDP, Label-Space-ID: %s:%u, length: %u",
           (vflag < 1) ? "" : "\n\t",
           ipaddr_string(&ldp_com_header->lsr_id),
           EXTRACT_16BITS(&ldp_com_header->label_space),
           len);

    /* bail out if non-verbose */ 
    if (vflag < 1)
        return;

    /* ok they seem to want to know everything - lets fully decode it */
    tlen=EXTRACT_16BITS(ldp_com_header->pdu_length);

    tptr+=sizeof(const struct ldp_common_header);
    tlen-=sizeof(const struct ldp_common_header);

    while(tlen>0) {
        /* did we capture enough for fully decoding the msg header ? */
        if (!TTEST2(*tptr, sizeof(struct ldp_msg_header)))
            goto trunc;

        ldp_msg_header = (const struct ldp_msg_header *)tptr;
        msg_len=EXTRACT_16BITS(ldp_msg_header->length);
        msg_type=LDP_MASK_MSG_TYPE(EXTRACT_16BITS(ldp_msg_header->type));

        /* FIXME vendor private / experimental check */
        printf("\n\t  %s Message (0x%04x), length: %u, Message ID: 0x%08x, Flags: [%s if unknown]",
               tok2str(ldp_msg_values,
                       "Unknown",
                       msg_type),
               msg_type,
               msg_len,
               EXTRACT_32BITS(&ldp_msg_header->id),
               LDP_MASK_U_BIT(EXTRACT_16BITS(&ldp_msg_header->type)) ? "continue processing" : "ignore");

        if (msg_len == 0) /* infinite loop protection */
            break;

        msg_tptr=tptr+sizeof(struct ldp_msg_header);
        msg_tlen=msg_len-sizeof(struct ldp_msg_header)+4; /* Type & Length fields not included */

        /* did we capture enough for fully decoding the message ? */
        if (!TTEST2(*tptr, msg_len))
            goto trunc;
        hexdump=FALSE;

        switch(msg_type) {
 
        case LDP_MSG_HELLO:
            while(msg_tlen >= 4) {
                processed = ldp_tlv_print(msg_tptr);
                if (processed == 0)
                    break;
                msg_tlen-=processed;
                msg_tptr+=processed;
            }
            break;

        /*
         *  FIXME those are the defined messages that lack a decoder
         *  you are welcome to contribute code ;-)
         */

        case LDP_MSG_NOTIF:
        case LDP_MSG_INIT:
        case LDP_MSG_KEEPALIVE:
        case LDP_MSG_ADDRESS:
        case LDP_MSG_ADDRESS_WITHDRAW:
        case LDP_MSG_LABEL_MAPPING:
        case LDP_MSG_LABEL_REQUEST:
        case LDP_MSG_LABEL_WITHDRAW:
        case LDP_MSG_LABEL_RELEASE:
        case LDP_MSG_LABEL_ABORT_REQUEST:

        default:
            if (vflag <= 1)
                print_unknown_data(msg_tptr,"\n\t  ",msg_tlen);
            break;
        }
        /* do we want to see an additionally hexdump ? */
        if (vflag > 1 || hexdump==TRUE)
            print_unknown_data(tptr+sizeof(sizeof(struct ldp_msg_header)),"\n\t  ",
                               msg_len);

        tptr+=msg_len;
        tlen-=msg_len;
    }
    return;
trunc:
    printf("\n\t\t packet exceeded snapshot");
}

