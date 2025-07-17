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
 * Reference documentation:
 *  https://www.cisco.com/c/en/us/support/docs/lan-switching/vtp/10558-21.html
 *  https://docstore.mik.ua/univercd/cc/td/doc/product/lan/trsrb/frames.htm
 *
 * Original code ode by Carles Kishimoto <carles.kishimoto@gmail.com>
 */

/* \summary: Cisco VLAN Trunking Protocol (VTP) printer */

#include <config.h>

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

#define VTP_HEADER_LEN			36
#define	VTP_DOMAIN_NAME_LEN		32
#define	VTP_MD5_DIGEST_LEN		16
#define VTP_UPDATE_TIMESTAMP_LEN	12
#define VTP_VLAN_INFO_FIXED_PART_LEN	12	/* length of VLAN info before VLAN name */

#define VTP_SUMMARY_ADV			0x01
#define VTP_SUBSET_ADV			0x02
#define VTP_ADV_REQUEST			0x03
#define VTP_JOIN_MESSAGE		0x04

struct vtp_vlan_ {
    nd_uint8_t  len;
    nd_uint8_t  status;
    nd_uint8_t  type;
    nd_uint8_t  name_len;
    nd_uint16_t vlanid;
    nd_uint16_t mtu;
    nd_uint32_t index;
};

static const struct tok vtp_message_type_values[] = {
    { VTP_SUMMARY_ADV, "Summary advertisement"},
    { VTP_SUBSET_ADV, "Subset advertisement"},
    { VTP_ADV_REQUEST, "Advertisement request"},
    { VTP_JOIN_MESSAGE, "Join message"},
    { 0, NULL }
};

static const struct tok vtp_header_values[] = {
    { 0x01, "Followers"}, /* On Summary advertisement, 3rd byte is Followers */
    { 0x02, "Seq number"}, /* On Subset  advertisement, 3rd byte is Sequence number */
    { 0x03, "Rsvd"}, /* On Adver. requests 3rd byte is Rsvd */
    { 0x04, "Rsvd"}, /* On Adver. requests 3rd byte is Rsvd */
    { 0, NULL }
};

static const struct tok vtp_vlan_type_values[] = {
    { 0x01, "Ethernet"},
    { 0x02, "FDDI"},
    { 0x03, "TrCRF"},
    { 0x04, "FDDI-net"},
    { 0x05, "TrBRF"},
    { 0, NULL }
};

static const struct tok vtp_vlan_status[] = {
    { 0x00, "Operational"},
    { 0x01, "Suspended"},
    { 0, NULL }
};

#define VTP_VLAN_SOURCE_ROUTING_RING_NUMBER      0x01
#define VTP_VLAN_SOURCE_ROUTING_BRIDGE_NUMBER    0x02
#define VTP_VLAN_STP_TYPE                        0x03
#define VTP_VLAN_PARENT_VLAN                     0x04
#define VTP_VLAN_TRANS_BRIDGED_VLAN              0x05
#define VTP_VLAN_PRUNING                         0x06
#define VTP_VLAN_BRIDGE_TYPE                     0x07
#define VTP_VLAN_ARP_HOP_COUNT                   0x08
#define VTP_VLAN_STE_HOP_COUNT                   0x09
#define VTP_VLAN_BACKUP_CRF_MODE                 0x0A

static const struct tok vtp_vlan_tlv_values[] = {
    { VTP_VLAN_SOURCE_ROUTING_RING_NUMBER, "Source-Routing Ring Number TLV"},
    { VTP_VLAN_SOURCE_ROUTING_BRIDGE_NUMBER, "Source-Routing Bridge Number TLV"},
    { VTP_VLAN_STP_TYPE, "STP type TLV"},
    { VTP_VLAN_PARENT_VLAN, "Parent VLAN TLV"},
    { VTP_VLAN_TRANS_BRIDGED_VLAN, "Translationally bridged VLANs TLV"},
    { VTP_VLAN_PRUNING, "Pruning TLV"},
    { VTP_VLAN_BRIDGE_TYPE, "Bridge Type TLV"},
    { VTP_VLAN_ARP_HOP_COUNT, "Max ARP Hop Count TLV"},
    { VTP_VLAN_STE_HOP_COUNT, "Max STE Hop Count TLV"},
    { VTP_VLAN_BACKUP_CRF_MODE, "Backup CRF Mode TLV"},
    { 0,                                  NULL }
};

static const struct tok vtp_stp_type_values[] = {
    { 1, "SRT"},
    { 2, "SRB"},
    { 3, "Auto"},
    { 0, NULL }
};

void
vtp_print(netdissect_options *ndo,
          const u_char *pptr, const u_int length)
{
    u_int type, len, name_len, tlv_len, tlv_value, mgmtd_len;
    const u_char *tptr;
    const struct vtp_vlan_ *vtp_vlan;

    ndo->ndo_protocol = "vtp";
    if (length < VTP_HEADER_LEN)
        goto invalid;

    tptr = pptr;

    ND_TCHECK_LEN(tptr, VTP_HEADER_LEN);

    type = GET_U_1(tptr + 1);
    ND_PRINT("VTPv%u, Message %s (0x%02x), length %u",
	   GET_U_1(tptr),
	   tok2str(vtp_message_type_values,"Unknown message type", type),
	   type,
	   length);

    /* In non-verbose mode, just print version and message type */
    if (ndo->ndo_vflag < 1) {
        goto tcheck_full_packet;
    }

    /* verbose mode print all fields */
    ND_PRINT("\n\tDomain name: ");
    mgmtd_len = GET_U_1(tptr + 3);
    if (mgmtd_len < 1 ||  mgmtd_len > VTP_DOMAIN_NAME_LEN) {
	ND_PRINT(" [MgmtD Len %u]", mgmtd_len);
	goto invalid;
    }
    nd_printjnp(ndo, tptr + 4, mgmtd_len);
    ND_PRINT(", %s: %u",
	   tok2str(vtp_header_values, "Unknown", type),
	   GET_U_1(tptr + 2));

    tptr += VTP_HEADER_LEN;

    switch (type) {

    case VTP_SUMMARY_ADV:

	/*
	 *  SUMMARY ADVERTISEMENT
	 *
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |     Version   |     Code      |    Followers  |    MgmtD Len  |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |       Management Domain Name  (zero-padded to 32 bytes)       |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                    Configuration revision number              |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                  Updater Identity IP address                  |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                    Update Timestamp (12 bytes)                |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                        MD5 digest (16 bytes)                  |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 */

	ND_PRINT("\n\t  Config Rev %x, Updater %s",
	       GET_BE_U_4(tptr),
	       GET_IPADDR_STRING(tptr+4));
	tptr += 8;
	ND_PRINT(", Timestamp 0x%08x 0x%08x 0x%08x",
	       GET_BE_U_4(tptr),
	       GET_BE_U_4(tptr + 4),
	       GET_BE_U_4(tptr + 8));
	tptr += VTP_UPDATE_TIMESTAMP_LEN;
	ND_PRINT(", MD5 digest: %08x%08x%08x%08x",
	       GET_BE_U_4(tptr),
	       GET_BE_U_4(tptr + 4),
	       GET_BE_U_4(tptr + 8),
	       GET_BE_U_4(tptr + 12));
	tptr += VTP_MD5_DIGEST_LEN;
	break;

    case VTP_SUBSET_ADV:

	/*
	 *  SUBSET ADVERTISEMENT
	 *
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |     Version   |     Code      |   Seq number  |    MgmtD Len  |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |       Management Domain Name  (zero-padded to 32 bytes)       |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                    Configuration revision number              |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                         VLAN info field 1                     |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                         ................                      |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                         VLAN info field N                     |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 */

	ND_PRINT(", Config Rev %x", GET_BE_U_4(tptr));

	/*
	 *  VLAN INFORMATION
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  | V info len    |    Status     |  VLAN type    | VLAN name len |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |       ISL vlan id             |            MTU size           |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                     802.10 index (SAID)                       |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                         VLAN name                             |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 */

	tptr += 4;
	while ((unsigned)(tptr - pptr) < length) {

	    len = GET_U_1(tptr);
	    if (len == 0)
		break;

	    ND_TCHECK_LEN(tptr, len);

	    vtp_vlan = (const struct vtp_vlan_*)tptr;
	    if (len < VTP_VLAN_INFO_FIXED_PART_LEN)
		goto invalid;
	    ND_PRINT("\n\tVLAN info status %s, type %s, VLAN-id %u, MTU %u, SAID 0x%08x, Name ",
		   tok2str(vtp_vlan_status,"Unknown",GET_U_1(vtp_vlan->status)),
		   tok2str(vtp_vlan_type_values,"Unknown",GET_U_1(vtp_vlan->type)),
		   GET_BE_U_2(vtp_vlan->vlanid),
		   GET_BE_U_2(vtp_vlan->mtu),
		   GET_BE_U_4(vtp_vlan->index));
	    len  -= VTP_VLAN_INFO_FIXED_PART_LEN;
	    tptr += VTP_VLAN_INFO_FIXED_PART_LEN;
	    name_len = GET_U_1(vtp_vlan->name_len);
	    if (len < 4*((name_len + 3)/4))
		goto invalid;
	    nd_printjnp(ndo, tptr, name_len);

	    /*
	     * Vlan names are aligned to 32-bit boundaries.
	     */
	    len  -= 4*((name_len + 3)/4);
	    tptr += 4*((name_len + 3)/4);

            /* TLV information follows */

            while (len > 0) {

                /*
                 * Cisco specs say 2 bytes for type + 2 bytes for length;
                 * see https://docstore.mik.ua/univercd/cc/td/doc/product/lan/trsrb/frames.htm
                 * However, actual packets on the wire appear to use 1
                 * byte for the type and 1 byte for the length, so that's
                 * what we do.
                 */
                if (len < 2)
                    goto invalid;
                type = GET_U_1(tptr);
                tlv_len = GET_U_1(tptr + 1);

                ND_PRINT("\n\t\t%s (0x%04x) TLV",
                       tok2str(vtp_vlan_tlv_values, "Unknown", type),
                       type);

                if (len < tlv_len * 2 + 2) {
                    ND_PRINT(" (TLV goes past the end of the packet)");
                    goto invalid;
                }
                ND_TCHECK_LEN(tptr, tlv_len * 2 + 2);

                /*
                 * We assume the value is a 2-byte integer; the length is
                 * in units of 16-bit words.
                 */
                if (tlv_len != 1) {
                    ND_PRINT(" [TLV length %u != 1]", tlv_len);
                    goto invalid;
                } else {
                    tlv_value = GET_BE_U_2(tptr + 2);

                    switch (type) {
                    case VTP_VLAN_STE_HOP_COUNT:
                        ND_PRINT(", %u", tlv_value);
                        break;

                    case VTP_VLAN_PRUNING:
                        ND_PRINT(", %s (%u)",
                               tlv_value == 1 ? "Enabled" : "Disabled",
                               tlv_value);
                        break;

                    case VTP_VLAN_STP_TYPE:
                        ND_PRINT(", %s (%u)",
                               tok2str(vtp_stp_type_values, "Unknown", tlv_value),
                               tlv_value);
                        break;

                    case VTP_VLAN_BRIDGE_TYPE:
                        ND_PRINT(", %s (%u)",
                               tlv_value == 1 ? "SRB" : "SRT",
                               tlv_value);
                        break;

                    case VTP_VLAN_BACKUP_CRF_MODE:
                        ND_PRINT(", %s (%u)",
                               tlv_value == 1 ? "Backup" : "Not backup",
                               tlv_value);
                        break;

                        /*
                         * FIXME those are the defined TLVs that lack a decoder
                         * you are welcome to contribute code ;-)
                         */

                    case VTP_VLAN_SOURCE_ROUTING_RING_NUMBER:
                    case VTP_VLAN_SOURCE_ROUTING_BRIDGE_NUMBER:
                    case VTP_VLAN_PARENT_VLAN:
                    case VTP_VLAN_TRANS_BRIDGED_VLAN:
                    case VTP_VLAN_ARP_HOP_COUNT:
                    default:
                        print_unknown_data(ndo, tptr, "\n\t\t  ", 2 + tlv_len*2);
                        break;
                    }
                }
                len -= 2 + tlv_len*2;
                tptr += 2 + tlv_len*2;
            }
	}
	break;

    case VTP_ADV_REQUEST:

	/*
	 *  ADVERTISEMENT REQUEST
	 *
	 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |     Version   |     Code      |   Reserved    |    MgmtD Len  |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |       Management Domain Name  (zero-padded to 32 bytes)       |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *  |                          Start value                          |
	 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *
	 */

	ND_PRINT("\n\tStart value: %u", GET_BE_U_4(tptr));
	break;

    case VTP_JOIN_MESSAGE:

	/* FIXME - Could not find message format */
	break;

    default:
	break;
    }

    return;

invalid:
    nd_print_invalid(ndo);
tcheck_full_packet:
    ND_TCHECK_LEN(pptr, length);
}
