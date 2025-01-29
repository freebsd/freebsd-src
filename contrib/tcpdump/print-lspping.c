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

/* \summary: MPLS LSP PING printer */

/* specification: RFC 4379 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "ntp.h"

#include "l2vpn.h"
#include "oui.h"


/*
 * LSPPING common header
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Version Number        |         Must Be Zero          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Message Type |   Reply mode  |  Return Code  | Return Subcode|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Sender's Handle                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Sequence Number                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    TimeStamp Sent (seconds)                   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                  TimeStamp Sent (microseconds)                |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                  TimeStamp Received (seconds)                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                TimeStamp Received (microseconds)              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                            TLVs ...                           |
 * .                                                               .
 * .                                                               .
 * .                                                               .
 */

struct lspping_common_header {
    nd_uint16_t version;
    nd_uint16_t global_flags;
    nd_uint8_t  msg_type;
    nd_uint8_t  reply_mode;
    nd_uint8_t  return_code;
    nd_uint8_t  return_subcode;
    nd_uint32_t sender_handle;
    nd_uint32_t seq_number;
    struct l_fixedpt ts_sent;
    struct l_fixedpt ts_rcvd;
};

#define LSPPING_VERSION            1

static const struct tok lspping_msg_type_values[] = {
    { 1, "MPLS Echo Request"},
    { 2, "MPLS Echo Reply"},
    { 0, NULL}
};

static const struct tok lspping_reply_mode_values[] = {
    { 1, "Do not reply"},
    { 2, "Reply via an IPv4/IPv6 UDP packet"},
    { 3, "Reply via an IPv4/IPv6 UDP packet with Router Alert"},
    { 4, "Reply via application level control channel"},
    { 0, NULL}
};

static const struct tok lspping_return_code_values[] = {
    {  0, "No return code or return code contained in the Error Code TLV"},
    {  1, "Malformed echo request received"},
    {  2, "One or more of the TLVs was not understood"},
    {  3, "Replying router is an egress for the FEC at stack depth"},
    {  4, "Replying router has no mapping for the FEC at stack depth"},
    {  5, "Reserved"},
    {  6, "Reserved"},
    {  7, "Reserved"},
    {  8, "Label switched at stack-depth"},
    {  9, "Label switched but no MPLS forwarding at stack-depth"},
    { 10, "Mapping for this FEC is not the given label at stack depth"},
    { 11, "No label entry at stack-depth"},
    { 12, "Protocol not associated with interface at FEC stack depth"},
    { 13, "Premature termination of ping due to label stack shrinking to a single label"},
    { 0,  NULL},
};


/*
 * LSPPING TLV header
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |             Type              |            Length             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                             Value                             |
 * .                                                               .
 * .                                                               .
 * .                                                               .
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct lspping_tlv_header {
    nd_uint16_t type;
    nd_uint16_t length;
};

#define	LSPPING_TLV_TARGET_FEC_STACK      1
#define	LSPPING_TLV_DOWNSTREAM_MAPPING    2
#define	LSPPING_TLV_PAD                   3
/* not assigned                           4 */
#define LSPPING_TLV_VENDOR_ENTERPRISE     5
#define LSPPING_TLV_VENDOR_ENTERPRISE_LEN 4
/* not assigned                           6 */
#define LSPPING_TLV_INTERFACE_LABEL_STACK 7
/* not assigned                           8 */
#define	LSPPING_TLV_ERROR_CODE            9
#define LSPPING_TLV_REPLY_TOS_BYTE        10
#define	LSPPING_TLV_BFD_DISCRIMINATOR     15 /* draft-ietf-bfd-mpls-02 */
#define LSPPING_TLV_BFD_DISCRIMINATOR_LEN 4
#define	LSPPING_TLV_VENDOR_PRIVATE        0xfc00

static const struct tok lspping_tlv_values[] = {
    { LSPPING_TLV_TARGET_FEC_STACK, "Target FEC Stack" },
    { LSPPING_TLV_DOWNSTREAM_MAPPING, "Downstream Mapping" },
    { LSPPING_TLV_PAD, "Pad" },
    { LSPPING_TLV_ERROR_CODE, "Error Code" },
    { LSPPING_TLV_VENDOR_ENTERPRISE, "Vendor Enterprise Code" },
    { LSPPING_TLV_INTERFACE_LABEL_STACK, "Interface Label Stack" },
    { LSPPING_TLV_REPLY_TOS_BYTE, "Reply TOS Byte" },
    { LSPPING_TLV_BFD_DISCRIMINATOR, "BFD Discriminator" },
    { LSPPING_TLV_VENDOR_PRIVATE, "Vendor Private Code" },
    { 0, NULL}
};

#define	LSPPING_TLV_TARGETFEC_SUBTLV_LDP_IPV4       1
#define	LSPPING_TLV_TARGETFEC_SUBTLV_LDP_IPV6       2
#define	LSPPING_TLV_TARGETFEC_SUBTLV_RSVP_IPV4      3
#define	LSPPING_TLV_TARGETFEC_SUBTLV_RSVP_IPV6      4
/* not assigned                                     5 */
#define	LSPPING_TLV_TARGETFEC_SUBTLV_L3VPN_IPV4     6
#define	LSPPING_TLV_TARGETFEC_SUBTLV_L3VPN_IPV6     7
#define	LSPPING_TLV_TARGETFEC_SUBTLV_L2VPN_ENDPT    8
#define	LSPPING_TLV_TARGETFEC_SUBTLV_FEC_128_PW_OLD 9
#define	LSPPING_TLV_TARGETFEC_SUBTLV_FEC_128_PW     10
#define	LSPPING_TLV_TARGETFEC_SUBTLV_FEC_129_PW     11
#define	LSPPING_TLV_TARGETFEC_SUBTLV_BGP_IPV4       12
#define	LSPPING_TLV_TARGETFEC_SUBTLV_BGP_IPV6       13
#define	LSPPING_TLV_TARGETFEC_SUBTLV_GENERIC_IPV4   14
#define	LSPPING_TLV_TARGETFEC_SUBTLV_GENERIC_IPV6   15
#define	LSPPING_TLV_TARGETFEC_SUBTLV_NIL_FEC        16

static const struct tok lspping_tlvtargetfec_subtlv_values[] = {
    { LSPPING_TLV_TARGETFEC_SUBTLV_LDP_IPV4, "LDP IPv4 prefix"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_LDP_IPV6, "LDP IPv6 prefix"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_RSVP_IPV4, "RSVP IPv4 Session Query"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_RSVP_IPV6, "RSVP IPv6 Session Query"},
    { 5, "Reserved"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_L3VPN_IPV4, "VPN IPv4 prefix"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_L3VPN_IPV6, "VPN IPv6 prefix"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_L2VPN_ENDPT, "L2 VPN endpoint"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_FEC_128_PW_OLD, "FEC 128 pseudowire (old)"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_FEC_128_PW, "FEC 128 pseudowire"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_BGP_IPV4, "BGP labeled IPv4 prefix"},
    { LSPPING_TLV_TARGETFEC_SUBTLV_BGP_IPV6, "BGP labeled IPv6 prefix"},
    { 0, NULL}
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          IPv4 prefix                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Prefix Length |         Must Be Zero                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_ldp_ipv4_t {
    nd_ipv4    prefix;
    nd_uint8_t prefix_len;
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          IPv6 prefix                          |
 * |                          (16 octets)                          |
 * |                                                               |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Prefix Length |         Must Be Zero                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_ldp_ipv6_t {
    nd_ipv6    prefix;
    nd_uint8_t prefix_len;
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 IPv4 tunnel end point address                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Must Be Zero         |     Tunnel ID                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Extended Tunnel ID                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   IPv4 tunnel sender address                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Must Be Zero         |            LSP ID             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_rsvp_ipv4_t {
    nd_ipv4     tunnel_endpoint;
    nd_byte     res[2];
    nd_uint16_t tunnel_id;
    nd_ipv4     extended_tunnel_id;
    nd_ipv4     tunnel_sender;
    nd_byte     res2[2];
    nd_uint16_t lsp_id;
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 IPv6 tunnel end point address                 |
 * |                                                               |
 * |                                                               |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Must Be Zero         |          Tunnel ID            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Extended Tunnel ID                      |
 * |                                                               |
 * |                                                               |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   IPv6 tunnel sender address                  |
 * |                                                               |
 * |                                                               |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Must Be Zero         |            LSP ID             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_rsvp_ipv6_t {
    nd_ipv6     tunnel_endpoint;
    nd_byte     res[2];
    nd_uint16_t tunnel_id;
    nd_ipv6     extended_tunnel_id;
    nd_ipv6     tunnel_sender;
    nd_byte     res2[2];
    nd_uint16_t lsp_id;
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Route Distinguisher                      |
 * |                          (8 octets)                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         IPv4 prefix                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Prefix Length |                 Must Be Zero                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_l3vpn_ipv4_t {
    nd_byte    rd[8];
    nd_ipv4    prefix;
    nd_uint8_t prefix_len;
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Route Distinguisher                      |
 * |                          (8 octets)                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          IPv6 prefix                          |
 * |                          (16 octets)                          |
 * |                                                               |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Prefix Length |                 Must Be Zero                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_l3vpn_ipv6_t {
    nd_byte    rd[8];
    nd_ipv6    prefix;
    nd_uint8_t prefix_len;
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Route Distinguisher                      |
 * |                          (8 octets)                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Sender's VE ID        |       Receiver's VE ID        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Encapsulation Type       |         Must Be Zero          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  0                   1                   2                   3
 */
struct lspping_tlv_targetfec_subtlv_l2vpn_endpt_t {
    nd_byte     rd[8];
    nd_uint16_t sender_ve_id;
    nd_uint16_t receiver_ve_id;
    nd_uint16_t encapsulation;
};

/*
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Remote PE Address                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                             PW ID                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            PW Type            |          Must Be Zero         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_fec_128_pw_old {
    nd_ipv4     remote_pe_address;
    nd_uint32_t pw_id;
    nd_uint16_t pw_type;
};

/*
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Sender's PE Address                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                      Remote PE Address                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                             PW ID                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            PW Type            |          Must Be Zero         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_fec_128_pw {
    nd_ipv4     sender_pe_address;
    nd_ipv4     remote_pe_address;
    nd_uint32_t pw_id;
    nd_uint16_t pw_type;
};

/*
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         IPv4 prefix                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Prefix Length |                 Must Be Zero                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_bgp_ipv4_t {
    nd_ipv4    prefix;
    nd_uint8_t prefix_len;
};

/*
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          IPv6 prefix                          |
 * |                          (16 octets)                          |
 * |                                                               |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Prefix Length |                 Must Be Zero                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct lspping_tlv_targetfec_subtlv_bgp_ipv6_t {
    nd_ipv6    prefix;
    nd_uint8_t prefix_len;
};

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |               MTU             | Address Type  |  Resvd (SBZ)  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |             Downstream IP Address (4 or 16 octets)            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Downstream Interface Address (4 or 16 octets)         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * | Multipath Type| Depth Limit   |        Multipath Length       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * .                                                               .
 * .                     (Multipath Information)                   .
 * .                                                               .
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |               Downstream Label                |    Protocol   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * .                                                               .
 * .                                                               .
 * .                                                               .
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |               Downstream Label                |    Protocol   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
/* Enough to get the address type */
struct lspping_tlv_downstream_map_t {
    nd_uint16_t mtu;
    nd_uint8_t  address_type;
    nd_uint8_t  ds_flags;
};

struct lspping_tlv_downstream_map_ipv4_t {
    nd_uint16_t mtu;
    nd_uint8_t  address_type;
    nd_uint8_t  ds_flags;
    nd_ipv4     downstream_ip;
    nd_ipv4     downstream_interface;
};

struct lspping_tlv_downstream_map_ipv4_unmb_t {
    nd_uint16_t mtu;
    nd_uint8_t  address_type;
    nd_uint8_t  ds_flags;
    nd_ipv4     downstream_ip;
    nd_uint32_t downstream_interface;
};

struct lspping_tlv_downstream_map_ipv6_t {
    nd_uint16_t mtu;
    nd_uint8_t  address_type;
    nd_uint8_t  ds_flags;
    nd_ipv6     downstream_ip;
    nd_ipv6     downstream_interface;
};

struct lspping_tlv_downstream_map_ipv6_unmb_t {
    nd_uint16_t mtu;
    nd_uint8_t  address_type;
    nd_uint8_t  ds_flags;
    nd_ipv6     downstream_ip;
    nd_uint32_t downstream_interface;
};

struct lspping_tlv_downstream_map_info_t {
    nd_uint8_t  multipath_type;
    nd_uint8_t  depth_limit;
    nd_uint16_t multipath_length;
};

#define LSPPING_AFI_IPV4      1
#define LSPPING_AFI_IPV4_UNMB 2
#define LSPPING_AFI_IPV6      3
#define LSPPING_AFI_IPV6_UNMB 4

static const struct tok lspping_tlv_downstream_addr_values[] = {
    { LSPPING_AFI_IPV4,      "IPv4"},
    { LSPPING_AFI_IPV4_UNMB, "Unnumbered IPv4"},
    { LSPPING_AFI_IPV6,      "IPv6"},
    { LSPPING_AFI_IPV6_UNMB, "IPv6"},
    { 0, NULL}
};

void
lspping_print(netdissect_options *ndo,
              const u_char *pptr, u_int len)
{
    const struct lspping_common_header *lspping_com_header;
    const struct lspping_tlv_header *lspping_tlv_header;
    const struct lspping_tlv_header *lspping_subtlv_header;
    const u_char *tptr,*tlv_tptr,*subtlv_tptr;
    u_int return_code, return_subcode;
    u_int tlen,lspping_tlv_len,lspping_tlv_type,tlv_tlen;
    int tlv_hexdump,subtlv_hexdump;
    u_int lspping_subtlv_len,lspping_subtlv_type;
    uint32_t int_part, fraction;
    u_int address_type;

    union {
        const struct lspping_tlv_downstream_map_t *lspping_tlv_downstream_map;
        const struct lspping_tlv_downstream_map_ipv4_t *lspping_tlv_downstream_map_ipv4;
        const struct lspping_tlv_downstream_map_ipv4_unmb_t *lspping_tlv_downstream_map_ipv4_unmb;
        const struct lspping_tlv_downstream_map_ipv6_t *lspping_tlv_downstream_map_ipv6;
        const struct lspping_tlv_downstream_map_ipv6_unmb_t *lspping_tlv_downstream_map_ipv6_unmb;
        const struct lspping_tlv_downstream_map_info_t  *lspping_tlv_downstream_map_info;
    } tlv_ptr;

    union {
        const struct lspping_tlv_targetfec_subtlv_ldp_ipv4_t *lspping_tlv_targetfec_subtlv_ldp_ipv4;
        const struct lspping_tlv_targetfec_subtlv_ldp_ipv6_t *lspping_tlv_targetfec_subtlv_ldp_ipv6;
        const struct lspping_tlv_targetfec_subtlv_rsvp_ipv4_t *lspping_tlv_targetfec_subtlv_rsvp_ipv4;
        const struct lspping_tlv_targetfec_subtlv_rsvp_ipv6_t *lspping_tlv_targetfec_subtlv_rsvp_ipv6;
        const struct lspping_tlv_targetfec_subtlv_l3vpn_ipv4_t *lspping_tlv_targetfec_subtlv_l3vpn_ipv4;
        const struct lspping_tlv_targetfec_subtlv_l3vpn_ipv6_t *lspping_tlv_targetfec_subtlv_l3vpn_ipv6;
        const struct lspping_tlv_targetfec_subtlv_l2vpn_endpt_t *lspping_tlv_targetfec_subtlv_l2vpn_endpt;
        const struct lspping_tlv_targetfec_subtlv_fec_128_pw_old *lspping_tlv_targetfec_subtlv_l2vpn_vcid_old;
        const struct lspping_tlv_targetfec_subtlv_fec_128_pw *lspping_tlv_targetfec_subtlv_l2vpn_vcid;
        const struct lspping_tlv_targetfec_subtlv_bgp_ipv4_t *lspping_tlv_targetfec_subtlv_bgp_ipv4;
        const struct lspping_tlv_targetfec_subtlv_bgp_ipv6_t *lspping_tlv_targetfec_subtlv_bgp_ipv6;
    } subtlv_ptr;

    ndo->ndo_protocol = "lspping";
    tptr=pptr;
    lspping_com_header = (const struct lspping_common_header *)pptr;
    if (len < sizeof(struct lspping_common_header))
        goto tooshort;
    ND_TCHECK_SIZE(lspping_com_header);

    /*
     * Sanity checking of the header.
     */
    if (GET_BE_U_2(lspping_com_header->version) != LSPPING_VERSION) {
	ND_PRINT("LSP-PING version %u packet not supported",
               GET_BE_U_2(lspping_com_header->version));
	return;
    }

    /* in non-verbose mode just lets print the basic Message Type*/
    if (ndo->ndo_vflag < 1) {
        ND_PRINT("LSP-PINGv%u, %s, seq %u, length: %u",
               GET_BE_U_2(lspping_com_header->version),
               tok2str(lspping_msg_type_values, "unknown (%u)",GET_U_1(lspping_com_header->msg_type)),
               GET_BE_U_4(lspping_com_header->seq_number),
               len);
        return;
    }

    /* ok they seem to want to know everything - lets fully decode it */

    tlen=len;

    ND_PRINT("\n\tLSP-PINGv%u, msg-type: %s (%u), length: %u\n\t  reply-mode: %s (%u)",
           GET_BE_U_2(lspping_com_header->version),
           tok2str(lspping_msg_type_values, "unknown",GET_U_1(lspping_com_header->msg_type)),
           GET_U_1(lspping_com_header->msg_type),
           len,
           tok2str(lspping_reply_mode_values, "unknown",GET_U_1(lspping_com_header->reply_mode)),
           GET_U_1(lspping_com_header->reply_mode));

    /*
     *  the following return codes require that the subcode is attached
     *  at the end of the translated token output
     */
    return_code = GET_U_1(lspping_com_header->return_code);
    return_subcode = GET_U_1(lspping_com_header->return_subcode);
    if (return_code == 3 ||
        return_code == 4 ||
        return_code == 8 ||
        return_code == 10 ||
        return_code == 11 ||
        return_code == 12 )
        ND_PRINT("\n\t  Return Code: %s %u (%u)\n\t  Return Subcode: (%u)",
               tok2str(lspping_return_code_values, "unknown",return_code),
               return_subcode,
               return_code,
               return_subcode);
    else
        ND_PRINT("\n\t  Return Code: %s (%u)\n\t  Return Subcode: (%u)",
               tok2str(lspping_return_code_values, "unknown",return_code),
               return_code,
               return_subcode);

    ND_PRINT("\n\t  Sender Handle: 0x%08x, Sequence: %u",
           GET_BE_U_4(lspping_com_header->sender_handle),
           GET_BE_U_4(lspping_com_header->seq_number));

    ND_PRINT("\n\t  Sender Timestamp: ");
    p_ntp_time(ndo, &lspping_com_header->ts_sent);
    ND_PRINT(" ");

    int_part=GET_BE_U_4(lspping_com_header->ts_rcvd.int_part);
    fraction=GET_BE_U_4(lspping_com_header->ts_rcvd.fraction);
    ND_PRINT("Receiver Timestamp: ");
    if (! (int_part == 0 && fraction == 0))
        p_ntp_time(ndo, &lspping_com_header->ts_rcvd);
    else
        ND_PRINT("no timestamp");

    tptr+=sizeof(struct lspping_common_header);
    tlen-=sizeof(struct lspping_common_header);

    while (tlen != 0) {
        /* Does the TLV go past the end of the packet? */
        if (tlen < sizeof(struct lspping_tlv_header))
            goto tooshort;

        lspping_tlv_header = (const struct lspping_tlv_header *)tptr;
        lspping_tlv_type=GET_BE_U_2(lspping_tlv_header->type);
        lspping_tlv_len=GET_BE_U_2(lspping_tlv_header->length);

        ND_PRINT("\n\t  %s TLV (%u), length: %u",
               tok2str(lspping_tlv_values,
                       "Unknown",
                       lspping_tlv_type),
               lspping_tlv_type,
               lspping_tlv_len);

        /* some little sanity checking */
        if (lspping_tlv_len == 0) {
            tptr+=sizeof(struct lspping_tlv_header);
            tlen-=sizeof(struct lspping_tlv_header);
            continue;    /* no value to dissect */
        }

        tlv_tptr=tptr+sizeof(struct lspping_tlv_header);
        tlv_tlen=lspping_tlv_len; /* header not included -> no adjustment */

        /* Does the TLV go past the end of the packet? */
        if (tlen < lspping_tlv_len+sizeof(struct lspping_tlv_header))
            goto tooshort;
        /* did we capture enough for fully decoding the tlv ? */
        ND_TCHECK_LEN(tlv_tptr, lspping_tlv_len);
        tlv_hexdump=FALSE;

        switch(lspping_tlv_type) {
        case LSPPING_TLV_TARGET_FEC_STACK:
            while (tlv_tlen != 0) {
                /* Does the subTLV header go past the end of the TLV? */
                if (tlv_tlen < sizeof(struct lspping_tlv_header)) {
                    ND_PRINT("\n\t      TLV is too short");
                    tlv_hexdump = TRUE;
                    goto tlv_tooshort;
                }
                subtlv_hexdump=FALSE;

                lspping_subtlv_header = (const struct lspping_tlv_header *)tlv_tptr;
                lspping_subtlv_type=GET_BE_U_2(lspping_subtlv_header->type);
                lspping_subtlv_len=GET_BE_U_2(lspping_subtlv_header->length);
                subtlv_tptr=tlv_tptr+sizeof(struct lspping_tlv_header);

                /* Does the subTLV go past the end of the TLV? */
                if (tlv_tlen < lspping_subtlv_len+sizeof(struct lspping_tlv_header)) {
                    ND_PRINT("\n\t      TLV is too short");
                    tlv_hexdump = TRUE;
                    goto tlv_tooshort;
                }

                /* Did we capture enough for fully decoding the subTLV? */
                ND_TCHECK_LEN(subtlv_tptr, lspping_subtlv_len);

                ND_PRINT("\n\t    %s subTLV (%u), length: %u",
                       tok2str(lspping_tlvtargetfec_subtlv_values,
                               "Unknown",
                               lspping_subtlv_type),
                       lspping_subtlv_type,
                       lspping_subtlv_len);

                switch(lspping_subtlv_type) {

                case LSPPING_TLV_TARGETFEC_SUBTLV_LDP_IPV4:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 5) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 5");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_ldp_ipv4 =
                            (const struct lspping_tlv_targetfec_subtlv_ldp_ipv4_t *)subtlv_tptr;
                        ND_PRINT("\n\t      %s/%u",
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_ldp_ipv4->prefix),
                               GET_U_1(subtlv_ptr.lspping_tlv_targetfec_subtlv_ldp_ipv4->prefix_len));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_LDP_IPV6:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 17) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 17");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_ldp_ipv6 =
                            (const struct lspping_tlv_targetfec_subtlv_ldp_ipv6_t *)subtlv_tptr;
                        ND_PRINT("\n\t      %s/%u",
                               GET_IP6ADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_ldp_ipv6->prefix),
                               GET_U_1(subtlv_ptr.lspping_tlv_targetfec_subtlv_ldp_ipv6->prefix_len));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_BGP_IPV4:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 5) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 5");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_bgp_ipv4 =
                            (const struct lspping_tlv_targetfec_subtlv_bgp_ipv4_t *)subtlv_tptr;
                        ND_PRINT("\n\t      %s/%u",
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_bgp_ipv4->prefix),
                               GET_U_1(subtlv_ptr.lspping_tlv_targetfec_subtlv_bgp_ipv4->prefix_len));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_BGP_IPV6:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 17) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 17");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_bgp_ipv6 =
                            (const struct lspping_tlv_targetfec_subtlv_bgp_ipv6_t *)subtlv_tptr;
                        ND_PRINT("\n\t      %s/%u",
                               GET_IP6ADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_bgp_ipv6->prefix),
                               GET_U_1(subtlv_ptr.lspping_tlv_targetfec_subtlv_bgp_ipv6->prefix_len));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_RSVP_IPV4:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 20) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 20");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv4 =
                            (const struct lspping_tlv_targetfec_subtlv_rsvp_ipv4_t *)subtlv_tptr;
                        ND_PRINT("\n\t      tunnel end-point %s, tunnel sender %s, lsp-id 0x%04x"
                               "\n\t      tunnel-id 0x%04x, extended tunnel-id %s",
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv4->tunnel_endpoint),
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv4->tunnel_sender),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv4->lsp_id),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv4->tunnel_id),
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv4->extended_tunnel_id));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_RSVP_IPV6:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 56) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 56");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv6 =
                            (const struct lspping_tlv_targetfec_subtlv_rsvp_ipv6_t *)subtlv_tptr;
                        ND_PRINT("\n\t      tunnel end-point %s, tunnel sender %s, lsp-id 0x%04x"
                               "\n\t      tunnel-id 0x%04x, extended tunnel-id %s",
                               GET_IP6ADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv6->tunnel_endpoint),
                               GET_IP6ADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv6->tunnel_sender),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv6->lsp_id),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv6->tunnel_id),
                               GET_IP6ADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_rsvp_ipv6->extended_tunnel_id));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_L3VPN_IPV4:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 13) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 13");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv4 =
                            (const struct lspping_tlv_targetfec_subtlv_l3vpn_ipv4_t *)subtlv_tptr;
                        ND_PRINT("\n\t      RD: %s, %s/%u",
                               bgp_vpn_rd_print(ndo, subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv4->rd),
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv4->prefix),
                               GET_U_1(subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv4->prefix_len));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_L3VPN_IPV6:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 25) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 25");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv6 =
                            (const struct lspping_tlv_targetfec_subtlv_l3vpn_ipv6_t *)subtlv_tptr;
                        ND_PRINT("\n\t      RD: %s, %s/%u",
                               bgp_vpn_rd_print(ndo, subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv6->rd),
                               GET_IP6ADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv6->prefix),
                               GET_U_1(subtlv_ptr.lspping_tlv_targetfec_subtlv_l3vpn_ipv6->prefix_len));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_L2VPN_ENDPT:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 14) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 14");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_endpt =
                            (const struct lspping_tlv_targetfec_subtlv_l2vpn_endpt_t *)subtlv_tptr;
                        ND_PRINT("\n\t      RD: %s, Sender VE ID: %u, Receiver VE ID: %u"
                               "\n\t      Encapsulation Type: %s (%u)",
                               bgp_vpn_rd_print(ndo, subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_endpt->rd),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_endpt->sender_ve_id),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_endpt->receiver_ve_id),
                               tok2str(mpls_pw_types_values,
                                       "unknown",
                                       GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_endpt->encapsulation)),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_endpt->encapsulation));
                    }
                    break;

                    /* the old L2VPN VCID subTLV does not have support for the sender field */
                case LSPPING_TLV_TARGETFEC_SUBTLV_FEC_128_PW_OLD:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 10) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 10");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid_old =
                            (const struct lspping_tlv_targetfec_subtlv_fec_128_pw_old *)subtlv_tptr;
                        ND_PRINT("\n\t      Remote PE: %s"
                               "\n\t      PW ID: 0x%08x, PW Type: %s (%u)",
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid_old->remote_pe_address),
                               GET_BE_U_4(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid_old->pw_id),
                               tok2str(mpls_pw_types_values,
                                       "unknown",
                                       GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid_old->pw_type)),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid_old->pw_type));
                    }
                    break;

                case LSPPING_TLV_TARGETFEC_SUBTLV_FEC_128_PW:
                    /* Is the subTLV length correct? */
                    if (lspping_subtlv_len != 14) {
                        ND_PRINT("\n\t      invalid subTLV length, should be 14");
                        subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    } else {
                        subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid =
                            (const struct lspping_tlv_targetfec_subtlv_fec_128_pw *)subtlv_tptr;
                        ND_PRINT("\n\t      Sender PE: %s, Remote PE: %s"
                               "\n\t      PW ID: 0x%08x, PW Type: %s (%u)",
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid->sender_pe_address),
                               GET_IPADDR_STRING(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid->remote_pe_address),
                               GET_BE_U_4(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid->pw_id),
                               tok2str(mpls_pw_types_values,
                                       "unknown",
                                       GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid->pw_type)),
                               GET_BE_U_2(subtlv_ptr.lspping_tlv_targetfec_subtlv_l2vpn_vcid->pw_type));
                    }
                    break;

                default:
                    subtlv_hexdump=TRUE; /* unknown subTLV just hexdump it */
                    break;
                }
                /* do we want to see an additionally subtlv hexdump ? */
                if (ndo->ndo_vflag > 1 || subtlv_hexdump==TRUE)
                    print_unknown_data(ndo, tlv_tptr+sizeof(struct lspping_tlv_header),
                                       "\n\t      ",
                                       lspping_subtlv_len);

                /* All subTLVs are aligned to four octet boundary */
                if (lspping_subtlv_len % 4) {
                    lspping_subtlv_len += 4 - (lspping_subtlv_len % 4);
                    /* Does the subTLV, including padding, go past the end of the TLV? */
                    if (tlv_tlen < lspping_subtlv_len+sizeof(struct lspping_tlv_header)) {
                        ND_PRINT("\n\t\t TLV is too short");
                        return;
                    }
                }
                tlv_tptr+=lspping_subtlv_len;
                tlv_tlen-=lspping_subtlv_len+sizeof(struct lspping_tlv_header);
            }
            break;

        case LSPPING_TLV_DOWNSTREAM_MAPPING:
            /* Does the header go past the end of the TLV? */
            if (tlv_tlen < sizeof(struct lspping_tlv_downstream_map_t)) {
                ND_PRINT("\n\t      TLV is too short");
                tlv_hexdump = TRUE;
                goto tlv_tooshort;
            }
            /* Did we capture enough to get the address family? */
            ND_TCHECK_LEN(tlv_tptr,
                          sizeof(struct lspping_tlv_downstream_map_t));

            tlv_ptr.lspping_tlv_downstream_map=
                (const struct lspping_tlv_downstream_map_t *)tlv_tptr;

            /* that strange thing with the downstream map TLV is that until now
             * we do not know if its IPv4 or IPv6 or is unnumbered; after
             * we find the address-type, we recast the tlv_tptr and move on. */

            address_type = GET_U_1(tlv_ptr.lspping_tlv_downstream_map->address_type);
            ND_PRINT("\n\t    MTU: %u, Address-Type: %s (%u)",
                   GET_BE_U_2(tlv_ptr.lspping_tlv_downstream_map->mtu),
                   tok2str(lspping_tlv_downstream_addr_values,
                           "unknown",
                           address_type),
                   address_type);

            switch(address_type) {

            case LSPPING_AFI_IPV4:
                /* Does the data go past the end of the TLV? */
                if (tlv_tlen < sizeof(struct lspping_tlv_downstream_map_ipv4_t)) {
                    ND_PRINT("\n\t      TLV is too short");
                    tlv_hexdump = TRUE;
                    goto tlv_tooshort;
                }
                /* Did we capture enough for this part of the TLV? */
                ND_TCHECK_LEN(tlv_tptr,
                              sizeof(struct lspping_tlv_downstream_map_ipv4_t));

                tlv_ptr.lspping_tlv_downstream_map_ipv4=
                    (const struct lspping_tlv_downstream_map_ipv4_t *)tlv_tptr;
                ND_PRINT("\n\t    Downstream IP: %s"
                       "\n\t    Downstream Interface IP: %s",
                       GET_IPADDR_STRING(tlv_ptr.lspping_tlv_downstream_map_ipv4->downstream_ip),
                       GET_IPADDR_STRING(tlv_ptr.lspping_tlv_downstream_map_ipv4->downstream_interface));
                tlv_tptr+=sizeof(struct lspping_tlv_downstream_map_ipv4_t);
                tlv_tlen-=sizeof(struct lspping_tlv_downstream_map_ipv4_t);
                break;
            case LSPPING_AFI_IPV4_UNMB:
                /* Does the data go past the end of the TLV? */
                if (tlv_tlen < sizeof(struct lspping_tlv_downstream_map_ipv4_unmb_t)) {
                    ND_PRINT("\n\t      TLV is too short");
                    tlv_hexdump = TRUE;
                    goto tlv_tooshort;
                }
                /* Did we capture enough for this part of the TLV? */
                ND_TCHECK_LEN(tlv_tptr,
                              sizeof(struct lspping_tlv_downstream_map_ipv4_unmb_t));

                tlv_ptr.lspping_tlv_downstream_map_ipv4_unmb=
                    (const struct lspping_tlv_downstream_map_ipv4_unmb_t *)tlv_tptr;
                ND_PRINT("\n\t    Downstream IP: %s"
                       "\n\t    Downstream Interface Index: 0x%08x",
                       GET_IPADDR_STRING(tlv_ptr.lspping_tlv_downstream_map_ipv4_unmb->downstream_ip),
                       GET_BE_U_4(tlv_ptr.lspping_tlv_downstream_map_ipv4_unmb->downstream_interface));
                tlv_tptr+=sizeof(struct lspping_tlv_downstream_map_ipv4_unmb_t);
                tlv_tlen-=sizeof(struct lspping_tlv_downstream_map_ipv4_unmb_t);
                break;
            case LSPPING_AFI_IPV6:
                /* Does the data go past the end of the TLV? */
                if (tlv_tlen < sizeof(struct lspping_tlv_downstream_map_ipv6_t)) {
                    ND_PRINT("\n\t      TLV is too short");
                    tlv_hexdump = TRUE;
                    goto tlv_tooshort;
                }
                /* Did we capture enough for this part of the TLV? */
                ND_TCHECK_LEN(tlv_tptr,
                              sizeof(struct lspping_tlv_downstream_map_ipv6_t));

                tlv_ptr.lspping_tlv_downstream_map_ipv6=
                    (const struct lspping_tlv_downstream_map_ipv6_t *)tlv_tptr;
                ND_PRINT("\n\t    Downstream IP: %s"
                       "\n\t    Downstream Interface IP: %s",
                       GET_IP6ADDR_STRING(tlv_ptr.lspping_tlv_downstream_map_ipv6->downstream_ip),
                       GET_IP6ADDR_STRING(tlv_ptr.lspping_tlv_downstream_map_ipv6->downstream_interface));
                tlv_tptr+=sizeof(struct lspping_tlv_downstream_map_ipv6_t);
                tlv_tlen-=sizeof(struct lspping_tlv_downstream_map_ipv6_t);
                break;
             case LSPPING_AFI_IPV6_UNMB:
                /* Does the data go past the end of the TLV? */
                if (tlv_tlen < sizeof(struct lspping_tlv_downstream_map_ipv6_unmb_t)) {
                    ND_PRINT("\n\t      TLV is too short");
                    tlv_hexdump = TRUE;
                    goto tlv_tooshort;
                }
                /* Did we capture enough for this part of the TLV? */
                ND_TCHECK_LEN(tlv_tptr,
                              sizeof(struct lspping_tlv_downstream_map_ipv6_unmb_t));

                tlv_ptr.lspping_tlv_downstream_map_ipv6_unmb=
                   (const struct lspping_tlv_downstream_map_ipv6_unmb_t *)tlv_tptr;
                ND_PRINT("\n\t    Downstream IP: %s"
                       "\n\t    Downstream Interface Index: 0x%08x",
                       GET_IP6ADDR_STRING(tlv_ptr.lspping_tlv_downstream_map_ipv6_unmb->downstream_ip),
                       GET_BE_U_4(tlv_ptr.lspping_tlv_downstream_map_ipv6_unmb->downstream_interface));
                tlv_tptr+=sizeof(struct lspping_tlv_downstream_map_ipv6_unmb_t);
                tlv_tlen-=sizeof(struct lspping_tlv_downstream_map_ipv6_unmb_t);
                break;

            default:
                /* should not happen ! - no error message - tok2str() has barked already */
                break;
            }

            /* Does the data go past the end of the TLV? */
            if (tlv_tlen < sizeof(struct lspping_tlv_downstream_map_info_t)) {
                ND_PRINT("\n\t      TLV is too short");
                tlv_hexdump = TRUE;
                goto tlv_tooshort;
            }
            /* Did we capture enough for this part of the TLV? */
            ND_TCHECK_LEN(tlv_tptr,
                          sizeof(struct lspping_tlv_downstream_map_info_t));

            tlv_ptr.lspping_tlv_downstream_map_info=
                (const struct lspping_tlv_downstream_map_info_t *)tlv_tptr;

            /* FIXME add hash-key type, depth limit, multipath processing */

            /* FIXME print downstream labels */

            tlv_hexdump=TRUE; /* dump the TLV until code complete */

            break;

        case LSPPING_TLV_BFD_DISCRIMINATOR:
            if (tlv_tlen < LSPPING_TLV_BFD_DISCRIMINATOR_LEN) {
                ND_PRINT("\n\t      TLV is too short");
                tlv_hexdump = TRUE;
                goto tlv_tooshort;
            } else {
                ND_PRINT("\n\t    BFD Discriminator 0x%08x", GET_BE_U_4(tlv_tptr));
            }
            break;

        case  LSPPING_TLV_VENDOR_ENTERPRISE:
        {
            uint32_t vendor_id;

            if (tlv_tlen < LSPPING_TLV_VENDOR_ENTERPRISE_LEN) {
                ND_PRINT("\n\t      TLV is too short");
                tlv_hexdump = TRUE;
                goto tlv_tooshort;
            } else {
                vendor_id = GET_BE_U_4(tlv_tptr);
                ND_PRINT("\n\t    Vendor: %s (0x%04x)",
                       tok2str(smi_values, "Unknown", vendor_id),
                       vendor_id);
            }
        }
            break;

            /*
             *  FIXME those are the defined TLVs that lack a decoder
             *  you are welcome to contribute code ;-)
             */
        case LSPPING_TLV_PAD:
        case LSPPING_TLV_ERROR_CODE:
        case LSPPING_TLV_VENDOR_PRIVATE:

        default:
            if (ndo->ndo_vflag <= 1)
                print_unknown_data(ndo, tlv_tptr, "\n\t    ", tlv_tlen);
            break;
        }
        /* do we want to see an additionally tlv hexdump ? */
    tlv_tooshort:
        if (ndo->ndo_vflag > 1 || tlv_hexdump==TRUE)
            print_unknown_data(ndo, tptr+sizeof(struct lspping_tlv_header), "\n\t    ",
                               lspping_tlv_len);


        /* All TLVs are aligned to four octet boundary */
        if (lspping_tlv_len % 4) {
            lspping_tlv_len += (4 - lspping_tlv_len % 4);
            /* Does the TLV, including padding, go past the end of the packet? */
            if (tlen < lspping_tlv_len+sizeof(struct lspping_tlv_header))
                goto tooshort;
        }

        tptr+=lspping_tlv_len+sizeof(struct lspping_tlv_header);
        tlen-=lspping_tlv_len+sizeof(struct lspping_tlv_header);
    }
    return;
tooshort:
    ND_PRINT("\n\t\t packet is too short");
}
