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
 * Original code by Carles Kishimoto <Carles.Kishimoto@bsc.es>
 */

/* \summary: Cisco VLAN Query Protocol (VQP) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"

#define VQP_VERSION 1

/*
 * VQP common header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Constant    | Packet type   |  Error Code   |    nitems     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                Packet Sequence Number (4 bytes)               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct vqp_common_header_t {
    nd_uint8_t  version;
    nd_uint8_t  msg_type;
    nd_uint8_t  error_code;
    nd_uint8_t  nitems;
    nd_uint32_t sequence;
};

struct vqp_obj_tlv_t {
    nd_uint32_t obj_type;
    nd_uint16_t obj_length;
};

#define VQP_OBJ_REQ_JOIN_PORT  0x01
#define VQP_OBJ_RESP_VLAN      0x02
#define VQP_OBJ_REQ_RECONFIRM  0x03
#define VQP_OBJ_RESP_RECONFIRM 0x04

static const struct tok vqp_msg_type_values[] = {
    { VQP_OBJ_REQ_JOIN_PORT, "Request, Join Port"},
    { VQP_OBJ_RESP_VLAN, "Response, VLAN"},
    { VQP_OBJ_REQ_RECONFIRM, "Request, Reconfirm"},
    { VQP_OBJ_RESP_RECONFIRM, "Response, Reconfirm"},
    { 0, NULL}
};

static const struct tok vqp_error_code_values[] = {
    { 0x00, "No error"},
    { 0x03, "Access denied"},
    { 0x04, "Shutdown port"},
    { 0x05, "Wrong VTP domain"},
    { 0, NULL}
};

/* FIXME the heading 0x0c looks ugly - those must be flags etc. */
#define VQP_OBJ_IP_ADDRESS    0x0c01
#define VQP_OBJ_PORT_NAME     0x0c02
#define VQP_OBJ_VLAN_NAME     0x0c03
#define VQP_OBJ_VTP_DOMAIN    0x0c04
#define VQP_OBJ_ETHERNET_PKT  0x0c05
#define VQP_OBJ_MAC_NULL      0x0c06
#define VQP_OBJ_MAC_ADDRESS   0x0c08

static const struct tok vqp_obj_values[] = {
    { VQP_OBJ_IP_ADDRESS, "Client IP Address" },
    { VQP_OBJ_PORT_NAME, "Port Name" },
    { VQP_OBJ_VLAN_NAME, "VLAN Name" },
    { VQP_OBJ_VTP_DOMAIN, "VTP Domain" },
    { VQP_OBJ_ETHERNET_PKT, "Ethernet Packet" },
    { VQP_OBJ_MAC_NULL, "MAC Null" },
    { VQP_OBJ_MAC_ADDRESS, "MAC Address" },
    { 0, NULL}
};

void
vqp_print(netdissect_options *ndo, const u_char *pptr, u_int len)
{
    const struct vqp_common_header_t *vqp_common_header;
    const struct vqp_obj_tlv_t *vqp_obj_tlv;

    const u_char *tptr;
    uint8_t version;
    uint16_t vqp_obj_len;
    uint32_t vqp_obj_type;
    u_int tlen;
    uint8_t nitems;

    ndo->ndo_protocol = "vqp";
    tptr=pptr;
    tlen = len;
    vqp_common_header = (const struct vqp_common_header_t *)pptr;
    ND_TCHECK_SIZE(vqp_common_header);
    if (sizeof(struct vqp_common_header_t) > tlen)
        goto invalid;
    version = GET_U_1(vqp_common_header->version);

    /*
     * Sanity checking of the header.
     */
    if (version != VQP_VERSION) {
	ND_PRINT("VQP version %u packet not supported",
               version);
	return;
    }

    /* in non-verbose mode just lets print the basic Message Type */
    if (ndo->ndo_vflag < 1) {
        ND_PRINT("VQPv%u %s Message, error-code %s (%u), length %u",
               version,
               tok2str(vqp_msg_type_values, "unknown (%u)",GET_U_1(vqp_common_header->msg_type)),
               tok2str(vqp_error_code_values, "unknown", GET_U_1(vqp_common_header->error_code)),
               GET_U_1(vqp_common_header->error_code),
               len);
        return;
    }

    /* ok they seem to want to know everything - lets fully decode it */
    nitems = GET_U_1(vqp_common_header->nitems);
    ND_PRINT("\n\tVQPv%u, %s Message, error-code %s (%u), seq 0x%08x, items %u, length %u",
           version,
	   tok2str(vqp_msg_type_values, "unknown (%u)",GET_U_1(vqp_common_header->msg_type)),
	   tok2str(vqp_error_code_values, "unknown", GET_U_1(vqp_common_header->error_code)),
	   GET_U_1(vqp_common_header->error_code),
	   GET_BE_U_4(vqp_common_header->sequence),
	   nitems,
           len);

    /* skip VQP Common header */
    tptr+=sizeof(struct vqp_common_header_t);
    tlen-=sizeof(struct vqp_common_header_t);

    while (nitems != 0 && tlen != 0) {

        vqp_obj_tlv = (const struct vqp_obj_tlv_t *)tptr;
        ND_TCHECK_SIZE(vqp_obj_tlv);
        if (sizeof(struct vqp_obj_tlv_t) > tlen)
            goto invalid;
        vqp_obj_type = GET_BE_U_4(vqp_obj_tlv->obj_type);
        vqp_obj_len = GET_BE_U_2(vqp_obj_tlv->obj_length);
        tptr+=sizeof(struct vqp_obj_tlv_t);
        tlen-=sizeof(struct vqp_obj_tlv_t);

        ND_PRINT("\n\t  %s Object (0x%08x), length %u, value: ",
               tok2str(vqp_obj_values, "Unknown", vqp_obj_type),
               vqp_obj_type, vqp_obj_len);

        /* basic sanity check */
        if (vqp_obj_type == 0 || vqp_obj_len ==0) {
            return;
        }

        /* did we capture enough for fully decoding the object ? */
        ND_TCHECK_LEN(tptr, vqp_obj_len);
        if (vqp_obj_len > tlen)
            goto invalid;

        switch(vqp_obj_type) {
	case VQP_OBJ_IP_ADDRESS:
            if (vqp_obj_len != 4)
                goto invalid;
            ND_PRINT("%s (0x%08x)", GET_IPADDR_STRING(tptr),
                     GET_BE_U_4(tptr));
            break;
            /* those objects have similar semantics - fall through */
        case VQP_OBJ_PORT_NAME:
	case VQP_OBJ_VLAN_NAME:
	case VQP_OBJ_VTP_DOMAIN:
	case VQP_OBJ_ETHERNET_PKT:
            nd_printjnp(ndo, tptr, vqp_obj_len);
            break;
            /* those objects have similar semantics - fall through */
	case VQP_OBJ_MAC_ADDRESS:
	case VQP_OBJ_MAC_NULL:
            if (vqp_obj_len != MAC_ADDR_LEN)
                goto invalid;
	      ND_PRINT("%s", GET_ETHERADDR_STRING(tptr));
              break;
        default:
            if (ndo->ndo_vflag <= 1)
                print_unknown_data(ndo,tptr, "\n\t    ", vqp_obj_len);
            break;
        }
	tptr += vqp_obj_len;
	tlen -= vqp_obj_len;
	nitems--;
    }
    return;
invalid:
    nd_print_invalid(ndo);
}
