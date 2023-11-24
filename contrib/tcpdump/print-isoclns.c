/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * Original code by Matt Thomas, Digital Equipment Corporation
 *
 * Extensively modified by Hannes Gredler (hannes@gredler.at) for more
 * complete IS-IS & CLNP support.
 */

/* \summary: ISO CLNS, ESIS, and ISIS printer */

/*
 * specification:
 *
 * CLNP: ISO 8473 (respective ITU version is at https://www.itu.int/rec/T-REC-X.233/en/)
 * ES-IS: ISO 9542
 * IS-IS: ISO 10589
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"
#include "nlpid.h"
#include "extract.h"
#include "gmpls.h"
#include "oui.h"
#include "signature.h"


/*
 * IS-IS is defined in ISO 10589.  Look there for protocol definitions.
 */

#define SYSTEM_ID_LEN	MAC_ADDR_LEN
#define NODE_ID_LEN     (SYSTEM_ID_LEN+1)
#define LSP_ID_LEN      (SYSTEM_ID_LEN+2)

#define ISIS_VERSION	1
#define ESIS_VERSION	1
#define CLNP_VERSION	1

#define ISIS_PDU_TYPE_MASK      0x1F
#define ESIS_PDU_TYPE_MASK      0x1F
#define CLNP_PDU_TYPE_MASK      0x1F
#define CLNP_FLAG_MASK          0xE0
#define ISIS_LAN_PRIORITY_MASK  0x7F

#define ISIS_PDU_L1_LAN_IIH	15
#define ISIS_PDU_L2_LAN_IIH	16
#define ISIS_PDU_PTP_IIH	17
#define ISIS_PDU_L1_LSP		18
#define ISIS_PDU_L2_LSP		20
#define ISIS_PDU_L1_CSNP	24
#define ISIS_PDU_L2_CSNP	25
#define ISIS_PDU_L1_PSNP        26
#define ISIS_PDU_L2_PSNP        27

static const struct tok isis_pdu_values[] = {
    { ISIS_PDU_L1_LAN_IIH,       "L1 Lan IIH"},
    { ISIS_PDU_L2_LAN_IIH,       "L2 Lan IIH"},
    { ISIS_PDU_PTP_IIH,          "p2p IIH"},
    { ISIS_PDU_L1_LSP,           "L1 LSP"},
    { ISIS_PDU_L2_LSP,           "L2 LSP"},
    { ISIS_PDU_L1_CSNP,          "L1 CSNP"},
    { ISIS_PDU_L2_CSNP,          "L2 CSNP"},
    { ISIS_PDU_L1_PSNP,          "L1 PSNP"},
    { ISIS_PDU_L2_PSNP,          "L2 PSNP"},
    { 0, NULL}
};

/*
 * A TLV is a tuple of a type, length and a value and is normally used for
 * encoding information in all sorts of places.  This is an enumeration of
 * the well known types.
 *
 * list taken from rfc3359 plus some memory from veterans ;-)
 */

#define ISIS_TLV_AREA_ADDR           1   /* iso10589 */
#define ISIS_TLV_IS_REACH            2   /* iso10589 */
#define ISIS_TLV_ESNEIGH             3   /* iso10589 */
#define ISIS_TLV_PART_DIS            4   /* iso10589 */
#define ISIS_TLV_PREFIX_NEIGH        5   /* iso10589 */
#define ISIS_TLV_ISNEIGH             6   /* iso10589 */
#define ISIS_TLV_INSTANCE_ID         7   /* rfc8202 */
#define ISIS_TLV_PADDING             8   /* iso10589 */
#define ISIS_TLV_LSP                 9   /* iso10589 */
#define ISIS_TLV_AUTH                10  /* iso10589, rfc3567 */
#define ISIS_TLV_CHECKSUM            12  /* rfc3358 */
#define ISIS_TLV_CHECKSUM_MINLEN 2
#define ISIS_TLV_POI                 13  /* rfc6232 */
#define ISIS_TLV_LSP_BUFFERSIZE      14  /* iso10589 rev2 */
#define ISIS_TLV_EXT_IS_REACH        22  /* rfc5305 */
#define ISIS_TLV_IS_ALIAS_ID         24  /* rfc5311 */
#define ISIS_TLV_DECNET_PHASE4       42
#define ISIS_TLV_LUCENT_PRIVATE      66
#define ISIS_TLV_INT_IP_REACH        128 /* rfc1195, rfc2966 */
#define ISIS_TLV_PROTOCOLS           129 /* rfc1195 */
#define ISIS_TLV_EXT_IP_REACH        130 /* rfc1195, rfc2966 */
#define ISIS_TLV_IDRP_INFO           131 /* rfc1195 */
#define ISIS_TLV_IPADDR              132 /* rfc1195 */
#define ISIS_TLV_IPAUTH              133 /* rfc1195 */
#define ISIS_TLV_TE_ROUTER_ID        134 /* rfc5305 */
#define ISIS_TLV_EXTD_IP_REACH       135 /* rfc5305 */
#define ISIS_TLV_HOSTNAME            137 /* rfc2763 */
#define ISIS_TLV_SHARED_RISK_GROUP   138 /* draft-ietf-isis-gmpls-extensions */
#define ISIS_TLV_MT_PORT_CAP         143 /* rfc6165 */
#define ISIS_TLV_MT_CAPABILITY       144 /* rfc6329 */
#define ISIS_TLV_NORTEL_PRIVATE1     176
#define ISIS_TLV_NORTEL_PRIVATE2     177
#define ISIS_TLV_RESTART_SIGNALING   211 /* rfc3847 */
#define ISIS_TLV_RESTART_SIGNALING_FLAGLEN 1
#define ISIS_TLV_RESTART_SIGNALING_HOLDTIMELEN 2
#define ISIS_TLV_MT_IS_REACH         222 /* draft-ietf-isis-wg-multi-topology-05 */
#define ISIS_TLV_MT_SUPPORTED        229 /* draft-ietf-isis-wg-multi-topology-05 */
#define ISIS_TLV_IP6ADDR             232 /* draft-ietf-isis-ipv6-02 */
#define ISIS_TLV_MT_IP_REACH         235 /* draft-ietf-isis-wg-multi-topology-05 */
#define ISIS_TLV_IP6_REACH           236 /* draft-ietf-isis-ipv6-02 */
#define ISIS_TLV_MT_IP6_REACH        237 /* draft-ietf-isis-wg-multi-topology-05 */
#define ISIS_TLV_PTP_ADJ             240 /* rfc3373 */
#define ISIS_TLV_IIH_SEQNR           241 /* draft-shen-isis-iih-sequence-00 */
#define ISIS_TLV_ROUTER_CAPABILITY   242 /* rfc7981 */
#define ISIS_TLV_VENDOR_PRIVATE      250 /* draft-ietf-isis-experimental-tlv-01 */
#define ISIS_TLV_VENDOR_PRIVATE_MINLEN 3

static const struct tok isis_tlv_values[] = {
    { ISIS_TLV_AREA_ADDR,	   "Area address(es)"},
    { ISIS_TLV_IS_REACH,           "IS Reachability"},
    { ISIS_TLV_ESNEIGH,            "ES Neighbor(s)"},
    { ISIS_TLV_PART_DIS,           "Partition DIS"},
    { ISIS_TLV_PREFIX_NEIGH,       "Prefix Neighbors"},
    { ISIS_TLV_ISNEIGH,            "IS Neighbor(s)"},
    { ISIS_TLV_INSTANCE_ID,        "Instance Identifier"},
    { ISIS_TLV_PADDING,            "Padding"},
    { ISIS_TLV_LSP,                "LSP entries"},
    { ISIS_TLV_AUTH,               "Authentication"},
    { ISIS_TLV_CHECKSUM,           "Checksum"},
    { ISIS_TLV_POI,                "Purge Originator Identifier"},
    { ISIS_TLV_LSP_BUFFERSIZE,     "LSP Buffersize"},
    { ISIS_TLV_EXT_IS_REACH,       "Extended IS Reachability"},
    { ISIS_TLV_IS_ALIAS_ID,        "IS Alias ID"},
    { ISIS_TLV_DECNET_PHASE4,      "DECnet Phase IV"},
    { ISIS_TLV_LUCENT_PRIVATE,     "Lucent Proprietary"},
    { ISIS_TLV_INT_IP_REACH,       "IPv4 Internal Reachability"},
    { ISIS_TLV_PROTOCOLS,          "Protocols supported"},
    { ISIS_TLV_EXT_IP_REACH,       "IPv4 External Reachability"},
    { ISIS_TLV_IDRP_INFO,          "Inter-Domain Information Type"},
    { ISIS_TLV_IPADDR,             "IPv4 Interface address(es)"},
    { ISIS_TLV_IPAUTH,             "IPv4 authentication (deprecated)"},
    { ISIS_TLV_TE_ROUTER_ID,       "Traffic Engineering Router ID"},
    { ISIS_TLV_EXTD_IP_REACH,      "Extended IPv4 Reachability"},
    { ISIS_TLV_SHARED_RISK_GROUP,  "Shared Risk Link Group"},
    { ISIS_TLV_MT_PORT_CAP,        "Multi-Topology-Aware Port Capability"},
    { ISIS_TLV_MT_CAPABILITY,      "Multi-Topology Capability"},
    { ISIS_TLV_NORTEL_PRIVATE1,    "Nortel Proprietary"},
    { ISIS_TLV_NORTEL_PRIVATE2,    "Nortel Proprietary"},
    { ISIS_TLV_HOSTNAME,           "Hostname"},
    { ISIS_TLV_RESTART_SIGNALING,  "Restart Signaling"},
    { ISIS_TLV_MT_IS_REACH,        "Multi Topology IS Reachability"},
    { ISIS_TLV_MT_SUPPORTED,       "Multi Topology"},
    { ISIS_TLV_IP6ADDR,            "IPv6 Interface address(es)"},
    { ISIS_TLV_MT_IP_REACH,        "Multi-Topology IPv4 Reachability"},
    { ISIS_TLV_IP6_REACH,          "IPv6 reachability"},
    { ISIS_TLV_MT_IP6_REACH,       "Multi-Topology IP6 Reachability"},
    { ISIS_TLV_PTP_ADJ,            "Point-to-point Adjacency State"},
    { ISIS_TLV_IIH_SEQNR,          "Hello PDU Sequence Number"},
    { ISIS_TLV_ROUTER_CAPABILITY,  "IS-IS Router Capability"},
    { ISIS_TLV_VENDOR_PRIVATE,     "Vendor Private"},
    { 0, NULL }
};

#define ESIS_OPTION_PROTOCOLS        129
#define ESIS_OPTION_QOS_MAINTENANCE  195 /* iso9542 */
#define ESIS_OPTION_SECURITY         197 /* iso9542 */
#define ESIS_OPTION_ES_CONF_TIME     198 /* iso9542 */
#define ESIS_OPTION_PRIORITY         205 /* iso9542 */
#define ESIS_OPTION_ADDRESS_MASK     225 /* iso9542 */
#define ESIS_OPTION_SNPA_MASK        226 /* iso9542 */

static const struct tok esis_option_values[] = {
    { ESIS_OPTION_PROTOCOLS,       "Protocols supported"},
    { ESIS_OPTION_QOS_MAINTENANCE, "QoS Maintenance" },
    { ESIS_OPTION_SECURITY,        "Security" },
    { ESIS_OPTION_ES_CONF_TIME,    "ES Configuration Time" },
    { ESIS_OPTION_PRIORITY,        "Priority" },
    { ESIS_OPTION_ADDRESS_MASK,    "Addressk Mask" },
    { ESIS_OPTION_SNPA_MASK,       "SNPA Mask" },
    { 0, NULL }
};

#define CLNP_OPTION_DISCARD_REASON   193
#define CLNP_OPTION_QOS_MAINTENANCE  195 /* iso8473 */
#define CLNP_OPTION_SECURITY         197 /* iso8473 */
#define CLNP_OPTION_SOURCE_ROUTING   200 /* iso8473 */
#define CLNP_OPTION_ROUTE_RECORDING  203 /* iso8473 */
#define CLNP_OPTION_PADDING          204 /* iso8473 */
#define CLNP_OPTION_PRIORITY         205 /* iso8473 */

static const struct tok clnp_option_values[] = {
    { CLNP_OPTION_DISCARD_REASON,  "Discard Reason"},
    { CLNP_OPTION_PRIORITY,        "Priority"},
    { CLNP_OPTION_QOS_MAINTENANCE, "QoS Maintenance"},
    { CLNP_OPTION_SECURITY, "Security"},
    { CLNP_OPTION_SOURCE_ROUTING, "Source Routing"},
    { CLNP_OPTION_ROUTE_RECORDING, "Route Recording"},
    { CLNP_OPTION_PADDING, "Padding"},
    { 0, NULL }
};

static const struct tok clnp_option_rfd_class_values[] = {
    { 0x0, "General"},
    { 0x8, "Address"},
    { 0x9, "Source Routeing"},
    { 0xa, "Lifetime"},
    { 0xb, "PDU Discarded"},
    { 0xc, "Reassembly"},
    { 0, NULL }
};

static const struct tok clnp_option_rfd_general_values[] = {
    { 0x0, "Reason not specified"},
    { 0x1, "Protocol procedure error"},
    { 0x2, "Incorrect checksum"},
    { 0x3, "PDU discarded due to congestion"},
    { 0x4, "Header syntax error (cannot be parsed)"},
    { 0x5, "Segmentation needed but not permitted"},
    { 0x6, "Incomplete PDU received"},
    { 0x7, "Duplicate option"},
    { 0, NULL }
};

static const struct tok clnp_option_rfd_address_values[] = {
    { 0x0, "Destination address unreachable"},
    { 0x1, "Destination address unknown"},
    { 0, NULL }
};

static const struct tok clnp_option_rfd_source_routeing_values[] = {
    { 0x0, "Unspecified source routeing error"},
    { 0x1, "Syntax error in source routeing field"},
    { 0x2, "Unknown address in source routeing field"},
    { 0x3, "Path not acceptable"},
    { 0, NULL }
};

static const struct tok clnp_option_rfd_lifetime_values[] = {
    { 0x0, "Lifetime expired while data unit in transit"},
    { 0x1, "Lifetime expired during reassembly"},
    { 0, NULL }
};

static const struct tok clnp_option_rfd_pdu_discard_values[] = {
    { 0x0, "Unsupported option not specified"},
    { 0x1, "Unsupported protocol version"},
    { 0x2, "Unsupported security option"},
    { 0x3, "Unsupported source routeing option"},
    { 0x4, "Unsupported recording of route option"},
    { 0, NULL }
};

static const struct tok clnp_option_rfd_reassembly_values[] = {
    { 0x0, "Reassembly interference"},
    { 0, NULL }
};

/* array of 16 error-classes */
static const struct tok *clnp_option_rfd_error_class[] = {
    clnp_option_rfd_general_values,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    clnp_option_rfd_address_values,
    clnp_option_rfd_source_routeing_values,
    clnp_option_rfd_lifetime_values,
    clnp_option_rfd_pdu_discard_values,
    clnp_option_rfd_reassembly_values,
    NULL,
    NULL,
    NULL
};

#define CLNP_OPTION_OPTION_QOS_MASK 0x3f
#define CLNP_OPTION_SCOPE_MASK      0xc0
#define CLNP_OPTION_SCOPE_SA_SPEC   0x40
#define CLNP_OPTION_SCOPE_DA_SPEC   0x80
#define CLNP_OPTION_SCOPE_GLOBAL    0xc0

static const struct tok clnp_option_scope_values[] = {
    { CLNP_OPTION_SCOPE_SA_SPEC, "Source Address Specific"},
    { CLNP_OPTION_SCOPE_DA_SPEC, "Destination Address Specific"},
    { CLNP_OPTION_SCOPE_GLOBAL, "Globally unique"},
    { 0, NULL }
};

static const struct tok clnp_option_sr_rr_values[] = {
    { 0x0, "partial"},
    { 0x1, "complete"},
    { 0, NULL }
};

static const struct tok clnp_option_sr_rr_string_values[] = {
    { CLNP_OPTION_SOURCE_ROUTING, "source routing"},
    { CLNP_OPTION_ROUTE_RECORDING, "recording of route in progress"},
    { 0, NULL }
};

static const struct tok clnp_option_qos_global_values[] = {
    { 0x20, "reserved"},
    { 0x10, "sequencing vs. delay"},
    { 0x08, "congested"},
    { 0x04, "delay vs. cost"},
    { 0x02, "error vs. delay"},
    { 0x01, "error vs. cost"},
    { 0, NULL }
};

static const struct tok isis_tlv_router_capability_flags[] = {
    { 0x01, "S bit"},
    { 0x02, "D bit"},
    { 0, NULL }
};

#define ISIS_SUBTLV_ROUTER_CAP_SR 2 /* rfc 8667 */

static const struct tok isis_router_capability_subtlv_values[] = {
    { ISIS_SUBTLV_ROUTER_CAP_SR, "SR-Capabilities"},
    { 0, NULL }
};

static const struct tok isis_router_capability_sr_flags[] = {
    { 0x80, "ipv4"},
    { 0x40, "ipv6"},
    { 0, NULL }
};

#define ISIS_SUBTLV_EXT_IS_REACH_ADMIN_GROUP           3 /* rfc5305 */
#define ISIS_SUBTLV_EXT_IS_REACH_LINK_LOCAL_REMOTE_ID  4 /* rfc4205 */
#define ISIS_SUBTLV_EXT_IS_REACH_LINK_REMOTE_ID        5 /* rfc5305 */
#define ISIS_SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR        6 /* rfc5305 */
#define ISIS_SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR    8 /* rfc5305 */
#define ISIS_SUBTLV_EXT_IS_REACH_MAX_LINK_BW           9 /* rfc5305 */
#define ISIS_SUBTLV_EXT_IS_REACH_RESERVABLE_BW        10 /* rfc5305 */
#define ISIS_SUBTLV_EXT_IS_REACH_UNRESERVED_BW        11 /* rfc4124 */
#define ISIS_SUBTLV_EXT_IS_REACH_BW_CONSTRAINTS_OLD   12 /* draft-ietf-tewg-diff-te-proto-06 */
#define ISIS_SUBTLV_EXT_IS_REACH_TE_METRIC            18 /* rfc5305 */
#define ISIS_SUBTLV_EXT_IS_REACH_LINK_ATTRIBUTE       19 /* draft-ietf-isis-link-attr-01 */
#define ISIS_SUBTLV_EXT_IS_REACH_LINK_PROTECTION_TYPE 20 /* rfc4205 */
#define ISIS_SUBTLV_EXT_IS_REACH_INTF_SW_CAP_DESCR    21 /* rfc4205 */
#define ISIS_SUBTLV_EXT_IS_REACH_BW_CONSTRAINTS       22 /* rfc4124 */
#define ISIS_SUBTLV_EXT_IS_REACH_LAN_ADJ_SEGMENT_ID   32 /* rfc8667 */

#define ISIS_SUBTLV_SPB_METRIC                        29 /* rfc6329 */

static const struct tok isis_ext_is_reach_subtlv_values[] = {
    { ISIS_SUBTLV_EXT_IS_REACH_ADMIN_GROUP,            "Administrative groups" },
    { ISIS_SUBTLV_EXT_IS_REACH_LINK_LOCAL_REMOTE_ID,   "Link Local/Remote Identifier" },
    { ISIS_SUBTLV_EXT_IS_REACH_LINK_REMOTE_ID,         "Link Remote Identifier" },
    { ISIS_SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR,         "IPv4 interface address" },
    { ISIS_SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR,     "IPv4 neighbor address" },
    { ISIS_SUBTLV_EXT_IS_REACH_MAX_LINK_BW,            "Maximum link bandwidth" },
    { ISIS_SUBTLV_EXT_IS_REACH_RESERVABLE_BW,          "Reservable link bandwidth" },
    { ISIS_SUBTLV_EXT_IS_REACH_UNRESERVED_BW,          "Unreserved bandwidth" },
    { ISIS_SUBTLV_EXT_IS_REACH_TE_METRIC,              "Traffic Engineering Metric" },
    { ISIS_SUBTLV_EXT_IS_REACH_LINK_ATTRIBUTE,         "Link Attribute" },
    { ISIS_SUBTLV_EXT_IS_REACH_LINK_PROTECTION_TYPE,   "Link Protection Type" },
    { ISIS_SUBTLV_EXT_IS_REACH_INTF_SW_CAP_DESCR,      "Interface Switching Capability" },
    { ISIS_SUBTLV_EXT_IS_REACH_BW_CONSTRAINTS_OLD,     "Bandwidth Constraints (old)" },
    { ISIS_SUBTLV_EXT_IS_REACH_BW_CONSTRAINTS,         "Bandwidth Constraints" },
    { ISIS_SUBTLV_EXT_IS_REACH_LAN_ADJ_SEGMENT_ID,     "LAN Adjacency Segment Identifier" },
    { ISIS_SUBTLV_SPB_METRIC,                          "SPB Metric" },
    { 250,                                             "Reserved for cisco specific extensions" },
    { 251,                                             "Reserved for cisco specific extensions" },
    { 252,                                             "Reserved for cisco specific extensions" },
    { 253,                                             "Reserved for cisco specific extensions" },
    { 254,                                             "Reserved for cisco specific extensions" },
    { 255,                                             "Reserved for future expansion" },
    { 0, NULL }
};

#define ISIS_SUBTLV_EXTD_IP_REACH_ADMIN_TAG32          1 /* draft-ietf-isis-admin-tags-01 */
#define ISIS_SUBTLV_EXTD_IP_REACH_ADMIN_TAG64          2 /* draft-ietf-isis-admin-tags-01 */
#define ISIS_SUBTLV_EXTD_IP_REACH_PREFIX_SID           3 /* rfc8667 */
#define ISIS_SUBTLV_EXTD_IP_REACH_MGMT_PREFIX_COLOR  117 /* draft-ietf-isis-wg-multi-topology-05 */

static const struct tok isis_ext_ip_reach_subtlv_values[] = {
    { ISIS_SUBTLV_EXTD_IP_REACH_ADMIN_TAG32,           "32-Bit Administrative tag" },
    { ISIS_SUBTLV_EXTD_IP_REACH_ADMIN_TAG64,           "64-Bit Administrative tag" },
    { ISIS_SUBTLV_EXTD_IP_REACH_PREFIX_SID,            "Prefix SID" },
    { ISIS_SUBTLV_EXTD_IP_REACH_MGMT_PREFIX_COLOR,     "Management Prefix Color" },
    { 0, NULL }
};

#define ISIS_PREFIX_SID_FLAG_R 0x80 /* rfc 8667 */
#define ISIS_PREFIX_SID_FLAG_N 0x40 /* rfc 8667 */
#define ISIS_PREFIX_SID_FLAG_P 0x20 /* rfc 8667 */
#define ISIS_PREFIX_SID_FLAG_E 0x10 /* rfc 8667 */
#define ISIS_PREFIX_SID_FLAG_V 0x08 /* rfc 8667 */
#define ISIS_PREFIX_SID_FLAG_L 0x04 /* rfc 8667 */

static const struct tok prefix_sid_flag_values[] = {
    { ISIS_PREFIX_SID_FLAG_R, "Readvertisement"},
    { ISIS_PREFIX_SID_FLAG_N, "Node"},
    { ISIS_PREFIX_SID_FLAG_P, "No-PHP"},
    { ISIS_PREFIX_SID_FLAG_E, "Explicit NULL"},
    { ISIS_PREFIX_SID_FLAG_V, "Value"},
    { ISIS_PREFIX_SID_FLAG_L, "Local"},
    { 0, NULL}
};


/* rfc 8667 */
static const struct tok prefix_sid_algo_values[] = {
    { 0, "SPF"},
    { 1, "strict-SPF"},
    { 0, NULL}
};

static const struct tok isis_subtlv_link_attribute_values[] = {
    { 0x01, "Local Protection Available" },
    { 0x02, "Link excluded from local protection path" },
    { 0x04, "Local maintenance required"},
    { 0, NULL }
};

static const struct tok isis_lan_adj_sid_flag_values[] = {
    { 0x80, "Address family IPv6" },
    { 0x40, "Backup" },
    { 0x20, "Value" },
    { 0x10, "Local significance" },
    { 0x08, "Set of adjacencies" },
    { 0x04, "Persistent" },
    { 0, NULL }
};

#define ISIS_SUBTLV_AUTH_SIMPLE        1
#define ISIS_SUBTLV_AUTH_GENERIC       3 /* rfc 5310 */
#define ISIS_SUBTLV_AUTH_MD5          54
#define ISIS_SUBTLV_AUTH_MD5_LEN      16
#define ISIS_SUBTLV_AUTH_PRIVATE     255

static const struct tok isis_subtlv_auth_values[] = {
    { ISIS_SUBTLV_AUTH_SIMPLE,	"simple text password"},
    { ISIS_SUBTLV_AUTH_GENERIC, "Generic Crypto key-id"},
    { ISIS_SUBTLV_AUTH_MD5,	"HMAC-MD5 password"},
    { ISIS_SUBTLV_AUTH_PRIVATE,	"Routing Domain private password"},
    { 0, NULL }
};

#define ISIS_SUBTLV_IDRP_RES           0
#define ISIS_SUBTLV_IDRP_LOCAL         1
#define ISIS_SUBTLV_IDRP_ASN           2

static const struct tok isis_subtlv_idrp_values[] = {
    { ISIS_SUBTLV_IDRP_RES,         "Reserved"},
    { ISIS_SUBTLV_IDRP_LOCAL,       "Routing-Domain Specific"},
    { ISIS_SUBTLV_IDRP_ASN,         "AS Number Tag"},
    { 0, NULL}
};

#define ISIS_SUBTLV_SPB_MCID          4
#define ISIS_SUBTLV_SPB_DIGEST        5
#define ISIS_SUBTLV_SPB_BVID          6

#define ISIS_SUBTLV_SPB_INSTANCE      1
#define ISIS_SUBTLV_SPBM_SI           3

#define ISIS_SPB_MCID_LEN                         51
#define ISIS_SUBTLV_SPB_MCID_MIN_LEN              102
#define ISIS_SUBTLV_SPB_DIGEST_MIN_LEN            33
#define ISIS_SUBTLV_SPB_BVID_MIN_LEN              6
#define ISIS_SUBTLV_SPB_INSTANCE_MIN_LEN          19
#define ISIS_SUBTLV_SPB_INSTANCE_VLAN_TUPLE_LEN   8

static const struct tok isis_mt_port_cap_subtlv_values[] = {
    { ISIS_SUBTLV_SPB_MCID,           "SPB MCID" },
    { ISIS_SUBTLV_SPB_DIGEST,         "SPB Digest" },
    { ISIS_SUBTLV_SPB_BVID,           "SPB BVID" },
    { 0, NULL }
};

static const struct tok isis_mt_capability_subtlv_values[] = {
    { ISIS_SUBTLV_SPB_INSTANCE,      "SPB Instance" },
    { ISIS_SUBTLV_SPBM_SI,      "SPBM Service Identifier and Unicast Address" },
    { 0, NULL }
};

struct isis_spb_mcid {
  nd_uint8_t  format_id;
  nd_byte     name[32];
  nd_uint16_t revision_lvl;
  nd_byte     digest[16];
};

struct isis_subtlv_spb_mcid {
  struct isis_spb_mcid mcid;
  struct isis_spb_mcid aux_mcid;
};

struct isis_subtlv_spb_instance {
  nd_byte     cist_root_id[8];
  nd_uint32_t cist_external_root_path_cost;
  nd_uint16_t bridge_priority;
  nd_uint32_t spsourceid;
  nd_uint8_t  no_of_trees;
};

#define CLNP_SEGMENT_PART  0x80
#define CLNP_MORE_SEGMENTS 0x40
#define CLNP_REQUEST_ER    0x20

static const struct tok clnp_flag_values[] = {
    { CLNP_SEGMENT_PART, "Segmentation permitted"},
    { CLNP_MORE_SEGMENTS, "more Segments"},
    { CLNP_REQUEST_ER, "request Error Report"},
    { 0, NULL}
};

#define ISIS_MASK_LSP_OL_BIT(x)            (GET_U_1(x)&0x4)
#define ISIS_MASK_LSP_ISTYPE_BITS(x)       (GET_U_1(x)&0x3)
#define ISIS_MASK_LSP_PARTITION_BIT(x)     (GET_U_1(x)&0x80)
#define ISIS_MASK_LSP_ATT_BITS(x)          (GET_U_1(x)&0x78)
#define ISIS_MASK_LSP_ATT_ERROR_BIT(x)     (GET_U_1(x)&0x40)
#define ISIS_MASK_LSP_ATT_EXPENSE_BIT(x)   (GET_U_1(x)&0x20)
#define ISIS_MASK_LSP_ATT_DELAY_BIT(x)     (GET_U_1(x)&0x10)
#define ISIS_MASK_LSP_ATT_DEFAULT_BIT(x)   (GET_U_1(x)&0x8)

#define ISIS_MASK_MTID(x)                  ((x)&0x0fff)
#define ISIS_MASK_MTFLAGS(x)               ((x)&0xf000)

static const struct tok isis_mt_flag_values[] = {
    { 0x4000,                  "ATT bit set"},
    { 0x8000,                  "Overload bit set"},
    { 0, NULL}
};

#define ISIS_MASK_TLV_EXTD_IP_UPDOWN(x)     ((x)&0x80)
#define ISIS_MASK_TLV_EXTD_IP_SUBTLV(x)     ((x)&0x40)

#define ISIS_MASK_TLV_EXTD_IP6_IE(x)        ((x)&0x40)
#define ISIS_MASK_TLV_EXTD_IP6_SUBTLV(x)    ((x)&0x20)

#define ISIS_LSP_TLV_METRIC_SUPPORTED(x)   (GET_U_1(x)&0x80)
#define ISIS_LSP_TLV_METRIC_IE(x)          (GET_U_1(x)&0x40)
#define ISIS_LSP_TLV_METRIC_UPDOWN(x)      (GET_U_1(x)&0x80)
#define ISIS_LSP_TLV_METRIC_VALUE(x)	   (GET_U_1(x)&0x3f)

#define ISIS_MASK_TLV_SHARED_RISK_GROUP(x) ((x)&0x1)

static const struct tok isis_mt_values[] = {
    { 0,    "IPv4 unicast"},
    { 1,    "In-Band Management"},
    { 2,    "IPv6 unicast"},
    { 3,    "Multicast"},
    { 4095, "Development, Experimental or Proprietary"},
    { 0, NULL }
};

static const struct tok isis_iih_circuit_type_values[] = {
    { 1,    "Level 1 only"},
    { 2,    "Level 2 only"},
    { 3,    "Level 1, Level 2"},
    { 0, NULL}
};

#define ISIS_LSP_TYPE_UNUSED0   0
#define ISIS_LSP_TYPE_LEVEL_1   1
#define ISIS_LSP_TYPE_UNUSED2   2
#define ISIS_LSP_TYPE_LEVEL_2   3

static const struct tok isis_lsp_istype_values[] = {
    { ISIS_LSP_TYPE_UNUSED0,	"Unused 0x0 (invalid)"},
    { ISIS_LSP_TYPE_LEVEL_1,	"L1 IS"},
    { ISIS_LSP_TYPE_UNUSED2,	"Unused 0x2 (invalid)"},
    { ISIS_LSP_TYPE_LEVEL_2,	"L2 IS"},
    { 0, NULL }
};

/*
 * Katz's point to point adjacency TLV uses codes to tell us the state of
 * the remote adjacency.  Enumerate them.
 */

#define ISIS_PTP_ADJ_UP   0
#define ISIS_PTP_ADJ_INIT 1
#define ISIS_PTP_ADJ_DOWN 2

static const struct tok isis_ptp_adjancey_values[] = {
    { ISIS_PTP_ADJ_UP,    "Up" },
    { ISIS_PTP_ADJ_INIT,  "Initializing" },
    { ISIS_PTP_ADJ_DOWN,  "Down" },
    { 0, NULL}
};

struct isis_tlv_ptp_adj {
    nd_uint8_t  adjacency_state;
    nd_uint32_t extd_local_circuit_id;
    nd_byte     neighbor_sysid[SYSTEM_ID_LEN];
    nd_uint32_t neighbor_extd_local_circuit_id;
};

static void osi_print_cksum(netdissect_options *, const uint8_t *pptr,
			    uint16_t checksum, int checksum_offset, u_int length);
static int clnp_print(netdissect_options *, const uint8_t *, u_int);
static void esis_print(netdissect_options *, const uint8_t *, u_int);
static int isis_print(netdissect_options *, const uint8_t *, u_int);

struct isis_metric_block {
    nd_uint8_t metric_default;
    nd_uint8_t metric_delay;
    nd_uint8_t metric_expense;
    nd_uint8_t metric_error;
};

struct isis_tlv_is_reach {
    struct isis_metric_block isis_metric_block;
    nd_byte neighbor_nodeid[NODE_ID_LEN];
};

struct isis_tlv_es_reach {
    struct isis_metric_block isis_metric_block;
    nd_byte neighbor_sysid[SYSTEM_ID_LEN];
};

struct isis_tlv_ip_reach {
    struct isis_metric_block isis_metric_block;
    nd_ipv4 prefix;
    nd_ipv4 mask;
};

static const struct tok isis_is_reach_virtual_values[] = {
    { 0,    "IsNotVirtual"},
    { 1,    "IsVirtual"},
    { 0, NULL }
};

static const struct tok isis_restart_flag_values[] = {
    { 0x1,  "Restart Request"},
    { 0x2,  "Restart Acknowledgement"},
    { 0x4,  "Suppress adjacency advertisement"},
    { 0, NULL }
};

struct isis_common_header {
    nd_uint8_t nlpid;
    nd_uint8_t fixed_len;
    nd_uint8_t version;			/* Protocol version */
    nd_uint8_t id_length;
    nd_uint8_t pdu_type;		/* 3 MSbits are reserved */
    nd_uint8_t pdu_version;		/* Packet format version */
    nd_byte reserved;
    nd_uint8_t max_area;
};

struct isis_iih_lan_header {
    nd_uint8_t  circuit_type;
    nd_byte     source_id[SYSTEM_ID_LEN];
    nd_uint16_t holding_time;
    nd_uint16_t pdu_len;
    nd_uint8_t  priority;
    nd_byte     lan_id[NODE_ID_LEN];
};

struct isis_iih_ptp_header {
    nd_uint8_t  circuit_type;
    nd_byte     source_id[SYSTEM_ID_LEN];
    nd_uint16_t holding_time;
    nd_uint16_t pdu_len;
    nd_uint8_t  circuit_id;
};

struct isis_lsp_header {
    nd_uint16_t pdu_len;
    nd_uint16_t remaining_lifetime;
    nd_byte     lsp_id[LSP_ID_LEN];
    nd_uint32_t sequence_number;
    nd_uint16_t checksum;
    nd_uint8_t  typeblock;
};

struct isis_csnp_header {
    nd_uint16_t pdu_len;
    nd_byte     source_id[NODE_ID_LEN];
    nd_byte     start_lsp_id[LSP_ID_LEN];
    nd_byte     end_lsp_id[LSP_ID_LEN];
};

struct isis_psnp_header {
    nd_uint16_t pdu_len;
    nd_byte     source_id[NODE_ID_LEN];
};

struct isis_tlv_lsp {
    nd_uint16_t remaining_lifetime;
    nd_byte     lsp_id[LSP_ID_LEN];
    nd_uint32_t sequence_number;
    nd_uint16_t checksum;
};

#define ISIS_COMMON_HEADER_SIZE (sizeof(struct isis_common_header))
#define ISIS_IIH_LAN_HEADER_SIZE (sizeof(struct isis_iih_lan_header))
#define ISIS_IIH_PTP_HEADER_SIZE (sizeof(struct isis_iih_ptp_header))
#define ISIS_LSP_HEADER_SIZE (sizeof(struct isis_lsp_header))
#define ISIS_CSNP_HEADER_SIZE (sizeof(struct isis_csnp_header))
#define ISIS_PSNP_HEADER_SIZE (sizeof(struct isis_psnp_header))

void
isoclns_print(netdissect_options *ndo, const u_char *p, u_int length)
{
	ndo->ndo_protocol = "isoclns";

	if (ndo->ndo_eflag)
		ND_PRINT("OSI NLPID %s (0x%02x): ",
			 tok2str(nlpid_values, "Unknown", GET_U_1(p)),
			 GET_U_1(p));

	switch (GET_U_1(p)) {

	case NLPID_CLNP:
		if (!clnp_print(ndo, p, length))
			print_unknown_data(ndo, p, "\n\t", length);
		break;

	case NLPID_ESIS:
		esis_print(ndo, p, length);
		return;

	case NLPID_ISIS:
		if (!isis_print(ndo, p, length))
			print_unknown_data(ndo, p, "\n\t", length);
		break;

	case NLPID_NULLNS:
		ND_PRINT("%slength: %u", ndo->ndo_eflag ? "" : ", ", length);
		break;

	case NLPID_Q933:
		q933_print(ndo, p + 1, length - 1);
		break;

	case NLPID_IP:
		ip_print(ndo, p + 1, length - 1);
		break;

	case NLPID_IP6:
		ip6_print(ndo, p + 1, length - 1);
		break;

	case NLPID_PPP:
		ppp_print(ndo, p + 1, length - 1);
		break;

	default:
		if (!ndo->ndo_eflag)
			ND_PRINT("OSI NLPID 0x%02x unknown", GET_U_1(p));
		ND_PRINT("%slength: %u", ndo->ndo_eflag ? "" : ", ", length);
		if (length > 1)
			print_unknown_data(ndo, p, "\n\t", length);
		break;
	}
}

#define	CLNP_PDU_ER	 1
#define	CLNP_PDU_DT	28
#define	CLNP_PDU_MD	29
#define	CLNP_PDU_ERQ	30
#define	CLNP_PDU_ERP	31

static const struct tok clnp_pdu_values[] = {
    { CLNP_PDU_ER,  "Error Report"},
    { CLNP_PDU_MD,  "MD"},
    { CLNP_PDU_DT,  "Data"},
    { CLNP_PDU_ERQ, "Echo Request"},
    { CLNP_PDU_ERP, "Echo Response"},
    { 0, NULL }
};

struct clnp_header_t {
    nd_uint8_t  nlpid;
    nd_uint8_t  length_indicator;
    nd_uint8_t  version;
    nd_uint8_t  lifetime; /* units of 500ms */
    nd_uint8_t  type;
    nd_uint16_t segment_length;
    nd_uint16_t cksum;
};

struct clnp_segment_header_t {
    nd_uint16_t data_unit_id;
    nd_uint16_t segment_offset;
    nd_uint16_t total_length;
};

/*
 * clnp_print
 * Decode CLNP packets.  Return 0 on error.
 */

static int
clnp_print(netdissect_options *ndo,
           const uint8_t *pptr, u_int length)
{
	const uint8_t *optr,*source_address,*dest_address;
        u_int li,li_remaining,tlen,nsap_offset,source_address_length,dest_address_length, clnp_pdu_type, clnp_flags;
	const struct clnp_header_t *clnp_header;
	const struct clnp_segment_header_t *clnp_segment_header;
        uint8_t rfd_error,rfd_error_major,rfd_error_minor;

	ndo->ndo_protocol = "clnp";
	clnp_header = (const struct clnp_header_t *) pptr;
        ND_TCHECK_SIZE(clnp_header);

        li = GET_U_1(clnp_header->length_indicator);
        li_remaining = li;
        optr = pptr;

        if (!ndo->ndo_eflag)
            nd_print_protocol_caps(ndo);

        /*
         * Sanity checking of the header.
         */

        if (GET_U_1(clnp_header->version) != CLNP_VERSION) {
            ND_PRINT("version %u packet not supported",
                     GET_U_1(clnp_header->version));
            return (0);
        }

	if (li > length) {
            ND_PRINT(" length indicator(%u) > PDU size (%u)!", li, length);
            return (0);
	}

        if (li < sizeof(struct clnp_header_t)) {
            ND_PRINT(" length indicator %u < min PDU size:", li);
            while (pptr < ndo->ndo_snapend) {
                ND_PRINT("%02X", GET_U_1(pptr));
                pptr++;
            }
            return (0);
        }

        /* FIXME further header sanity checking */

        clnp_pdu_type = GET_U_1(clnp_header->type) & CLNP_PDU_TYPE_MASK;
        clnp_flags = GET_U_1(clnp_header->type) & CLNP_FLAG_MASK;

        pptr += sizeof(struct clnp_header_t);
        li_remaining -= sizeof(struct clnp_header_t);

        if (li_remaining < 1) {
            ND_PRINT("li < size of fixed part of CLNP header and addresses");
            return (0);
        }
        dest_address_length = GET_U_1(pptr);
        pptr += 1;
        li_remaining -= 1;
        if (li_remaining < dest_address_length) {
            ND_PRINT("li < size of fixed part of CLNP header and addresses");
            return (0);
        }
        ND_TCHECK_LEN(pptr, dest_address_length);
        dest_address = pptr;
        pptr += dest_address_length;
        li_remaining -= dest_address_length;

        if (li_remaining < 1) {
            ND_PRINT("li < size of fixed part of CLNP header and addresses");
            return (0);
        }
        source_address_length = GET_U_1(pptr);
        pptr += 1;
        li_remaining -= 1;
        if (li_remaining < source_address_length) {
            ND_PRINT("li < size of fixed part of CLNP header and addresses");
            return (0);
        }
        ND_TCHECK_LEN(pptr, source_address_length);
        source_address = pptr;
        pptr += source_address_length;
        li_remaining -= source_address_length;

        if (ndo->ndo_vflag < 1) {
            ND_PRINT("%s%s > %s, %s, length %u",
                   ndo->ndo_eflag ? "" : ", ",
                   GET_ISONSAP_STRING(source_address, source_address_length),
                   GET_ISONSAP_STRING(dest_address, dest_address_length),
                   tok2str(clnp_pdu_values,"unknown (%u)",clnp_pdu_type),
                   length);
            return (1);
        }
        ND_PRINT("%slength %u", ndo->ndo_eflag ? "" : ", ", length);

        ND_PRINT("\n\t%s PDU, hlen: %u, v: %u, lifetime: %u.%us, Segment PDU length: %u, checksum: 0x%04x",
               tok2str(clnp_pdu_values, "unknown (%u)",clnp_pdu_type),
               GET_U_1(clnp_header->length_indicator),
               GET_U_1(clnp_header->version),
               GET_U_1(clnp_header->lifetime)/2,
               (GET_U_1(clnp_header->lifetime)%2)*5,
               GET_BE_U_2(clnp_header->segment_length),
               GET_BE_U_2(clnp_header->cksum));

        osi_print_cksum(ndo, optr, GET_BE_U_2(clnp_header->cksum), 7,
                        GET_U_1(clnp_header->length_indicator));

        ND_PRINT("\n\tFlags [%s]",
               bittok2str(clnp_flag_values, "none", clnp_flags));

        ND_PRINT("\n\tsource address (length %u): %s\n\tdest   address (length %u): %s",
               source_address_length,
               GET_ISONSAP_STRING(source_address, source_address_length),
               dest_address_length,
               GET_ISONSAP_STRING(dest_address, dest_address_length));

        if (clnp_flags & CLNP_SEGMENT_PART) {
                if (li_remaining < sizeof(struct clnp_segment_header_t)) {
                    ND_PRINT("li < size of fixed part of CLNP header, addresses, and segment part");
                    return (0);
                }
		clnp_segment_header = (const struct clnp_segment_header_t *) pptr;
                ND_TCHECK_SIZE(clnp_segment_header);
                ND_PRINT("\n\tData Unit ID: 0x%04x, Segment Offset: %u, Total PDU Length: %u",
                       GET_BE_U_2(clnp_segment_header->data_unit_id),
                       GET_BE_U_2(clnp_segment_header->segment_offset),
                       GET_BE_U_2(clnp_segment_header->total_length));
                pptr+=sizeof(struct clnp_segment_header_t);
                li_remaining-=sizeof(struct clnp_segment_header_t);
        }

        /* now walk the options */
        while (li_remaining != 0) {
            u_int op, opli;
            const uint8_t *tptr;

            if (li_remaining < 2) {
                ND_PRINT(", bad opts/li");
                return (0);
            }
            op = GET_U_1(pptr);
            opli = GET_U_1(pptr + 1);
            pptr += 2;
            li_remaining -= 2;
            if (opli > li_remaining) {
                ND_PRINT(", opt (%u) too long", op);
                return (0);
            }
            ND_TCHECK_LEN(pptr, opli);
            li_remaining -= opli;
            tptr = pptr;
            tlen = opli;

            ND_PRINT("\n\t  %s Option #%u, length %u, value: ",
                   tok2str(clnp_option_values,"Unknown",op),
                   op,
                   opli);

            /*
             * We've already checked that the entire option is present
             * in the captured packet with the ND_TCHECK_LEN() call.
             * Therefore, we don't need to do ND_TCHECK()/ND_TCHECK_LEN()
             * checks.
             * We do, however, need to check tlen, to make sure we
             * don't run past the end of the option.
	     */
            switch (op) {


            case CLNP_OPTION_ROUTE_RECORDING: /* those two options share the format */
            case CLNP_OPTION_SOURCE_ROUTING:
                    if (tlen < 2) {
                            ND_PRINT(", bad opt len");
                            return (0);
                    }
                    ND_PRINT("%s %s",
                           tok2str(clnp_option_sr_rr_values,"Unknown",GET_U_1(tptr)),
                           tok2str(clnp_option_sr_rr_string_values, "Unknown Option %u", op));
                    nsap_offset=GET_U_1(tptr + 1);
                    if (nsap_offset == 0) {
                            ND_PRINT(" Bad NSAP offset (0)");
                            break;
                    }
                    nsap_offset-=1; /* offset to nsap list */
                    if (nsap_offset > tlen) {
                            ND_PRINT(" Bad NSAP offset (past end of option)");
                            break;
                    }
                    tptr+=nsap_offset;
                    tlen-=nsap_offset;
                    while (tlen > 0) {
                            source_address_length=GET_U_1(tptr);
                            if (tlen < source_address_length+1) {
                                    ND_PRINT("\n\t    NSAP address goes past end of option");
                                    break;
                            }
                            if (source_address_length > 0) {
                                    source_address=(tptr+1);
                                    ND_PRINT("\n\t    NSAP address (length %u): %s",
                                           source_address_length,
                                           GET_ISONSAP_STRING(source_address, source_address_length));
                            }
                            tlen-=source_address_length+1;
                    }
                    break;

            case CLNP_OPTION_PRIORITY:
                    if (tlen < 1) {
                            ND_PRINT(", bad opt len");
                            return (0);
                    }
                    ND_PRINT("0x%1x", GET_U_1(tptr)&0x0f);
                    break;

            case CLNP_OPTION_QOS_MAINTENANCE:
                    if (tlen < 1) {
                            ND_PRINT(", bad opt len");
                            return (0);
                    }
                    ND_PRINT("\n\t    Format Code: %s",
                           tok2str(clnp_option_scope_values, "Reserved", GET_U_1(tptr) & CLNP_OPTION_SCOPE_MASK));

                    if ((GET_U_1(tptr)&CLNP_OPTION_SCOPE_MASK) == CLNP_OPTION_SCOPE_GLOBAL)
                            ND_PRINT("\n\t    QoS Flags [%s]",
                                   bittok2str(clnp_option_qos_global_values,
                                              "none",
                                              GET_U_1(tptr)&CLNP_OPTION_OPTION_QOS_MASK));
                    break;

            case CLNP_OPTION_SECURITY:
                    if (tlen < 2) {
                            ND_PRINT(", bad opt len");
                            return (0);
                    }
                    ND_PRINT("\n\t    Format Code: %s, Security-Level %u",
                           tok2str(clnp_option_scope_values,"Reserved",GET_U_1(tptr)&CLNP_OPTION_SCOPE_MASK),
                           GET_U_1(tptr + 1));
                    break;

            case CLNP_OPTION_DISCARD_REASON:
                if (tlen < 1) {
                        ND_PRINT(", bad opt len");
                        return (0);
                }
                rfd_error = GET_U_1(tptr);
                rfd_error_major = (rfd_error&0xf0) >> 4;
                rfd_error_minor = rfd_error&0x0f;
                ND_PRINT("\n\t    Class: %s Error (0x%01x), %s (0x%01x)",
                       tok2str(clnp_option_rfd_class_values,"Unknown",rfd_error_major),
                       rfd_error_major,
                       tok2str(clnp_option_rfd_error_class[rfd_error_major],"Unknown",rfd_error_minor),
                       rfd_error_minor);
                break;

            case CLNP_OPTION_PADDING:
                    ND_PRINT("padding data");
                break;

                /*
                 * FIXME those are the defined Options that lack a decoder
                 * you are welcome to contribute code ;-)
                 */

            default:
                print_unknown_data(ndo, tptr, "\n\t  ", opli);
                break;
            }
            if (ndo->ndo_vflag > 1)
                print_unknown_data(ndo, pptr, "\n\t  ", opli);
            pptr += opli;
        }

        switch (clnp_pdu_type) {

        case    CLNP_PDU_ER: /* fall through */
        case	CLNP_PDU_ERP:
            if (GET_U_1(pptr) == NLPID_CLNP) {
                ND_PRINT("\n\t-----original packet-----\n\t");
                /* FIXME recursion protection */
                clnp_print(ndo, pptr, length - li);
                break;
            }

        /* The cases above break from the switch block if they see and print
         * a CLNP header in the Data part. For an Error Report PDU this is
         * described in Section 7.9.6 of ITU X.233 (1997 E), also known as
         * ISO/IEC 8473-1:1998(E). It is not clear why in this code the same
         * applies to an Echo Response PDU, as the standard does not specify
         * the contents -- could be a proprietary extension or a bug. In either
         * case, if the Data part does not contain a CLNP header, its structure
         * is considered unknown and the decoding falls through to print the
         * contents as-is.
         */
        ND_FALL_THROUGH;

        case	CLNP_PDU_DT:
        case	CLNP_PDU_MD:
        case	CLNP_PDU_ERQ:

        default:
            /* dump the PDU specific data */
            if (length > ND_BYTES_BETWEEN(pptr, optr)) {
                ND_PRINT("\n\t  undecoded non-header data, length %u", length-li);
                print_unknown_data(ndo, pptr, "\n\t  ", length - ND_BYTES_BETWEEN(pptr, optr));
            }
        }

        return (1);

 trunc:
    nd_print_trunc(ndo);
    return (1);

}


#define	ESIS_PDU_REDIRECT	6
#define	ESIS_PDU_ESH	        2
#define	ESIS_PDU_ISH	        4

static const struct tok esis_pdu_values[] = {
    { ESIS_PDU_REDIRECT, "redirect"},
    { ESIS_PDU_ESH,      "ESH"},
    { ESIS_PDU_ISH,      "ISH"},
    { 0, NULL }
};

struct esis_header_t {
	nd_uint8_t  nlpid;
	nd_uint8_t  length_indicator;
	nd_uint8_t  version;
	nd_byte     reserved;
	nd_uint8_t  type;
	nd_uint16_t holdtime;
	nd_uint16_t cksum;
};

static void
esis_print(netdissect_options *ndo,
           const uint8_t *pptr, u_int length)
{
	const uint8_t *optr;
	u_int li, version, esis_pdu_type, source_address_length, source_address_number;
	const struct esis_header_t *esis_header;

	ndo->ndo_protocol = "esis";
	if (!ndo->ndo_eflag)
		ND_PRINT("ES-IS");

	if (length <= 2) {
		ND_PRINT(ndo->ndo_qflag ? "bad pkt!" : "no header at all!");
		return;
	}

	esis_header = (const struct esis_header_t *) pptr;
        ND_TCHECK_SIZE(esis_header);
        li = GET_U_1(esis_header->length_indicator);
        optr = pptr;

        /*
         * Sanity checking of the header.
         */

        if (GET_U_1(esis_header->nlpid) != NLPID_ESIS) {
            ND_PRINT(" nlpid 0x%02x packet not supported",
		     GET_U_1(esis_header->nlpid));
            return;
        }

        version = GET_U_1(esis_header->version);
        if (version != ESIS_VERSION) {
            ND_PRINT(" version %u packet not supported", version);
            return;
        }

	if (li > length) {
            ND_PRINT(" length indicator(%u) > PDU size (%u)!", li, length);
            return;
	}

	if (li < sizeof(struct esis_header_t) + 2) {
            ND_PRINT(" length indicator %u < min PDU size:", li);
            while (pptr < ndo->ndo_snapend) {
                ND_PRINT("%02X", GET_U_1(pptr));
                pptr++;
            }
            return;
	}

        esis_pdu_type = GET_U_1(esis_header->type) & ESIS_PDU_TYPE_MASK;

        if (ndo->ndo_vflag < 1) {
            ND_PRINT("%s%s, length %u",
                   ndo->ndo_eflag ? "" : ", ",
                   tok2str(esis_pdu_values,"unknown type (%u)",esis_pdu_type),
                   length);
            return;
        } else
            ND_PRINT("%slength %u\n\t%s (%u)",
                   ndo->ndo_eflag ? "" : ", ",
                   length,
                   tok2str(esis_pdu_values,"unknown type: %u", esis_pdu_type),
                   esis_pdu_type);

        ND_PRINT(", v: %u%s", version, version == ESIS_VERSION ? "" : "unsupported" );
        ND_PRINT(", checksum: 0x%04x", GET_BE_U_2(esis_header->cksum));

        osi_print_cksum(ndo, pptr, GET_BE_U_2(esis_header->cksum), 7,
                        li);

        ND_PRINT(", holding time: %us, length indicator: %u",
                  GET_BE_U_2(esis_header->holdtime), li);

        if (ndo->ndo_vflag > 1)
            print_unknown_data(ndo, optr, "\n\t", sizeof(struct esis_header_t));

	pptr += sizeof(struct esis_header_t);
	li -= sizeof(struct esis_header_t);

	switch (esis_pdu_type) {
	case ESIS_PDU_REDIRECT: {
		const uint8_t *dst, *snpa, *neta;
		u_int dstl, snpal, netal;

		ND_TCHECK_1(pptr);
		if (li < 1) {
			ND_PRINT(", bad redirect/li");
			return;
		}
		dstl = GET_U_1(pptr);
		pptr++;
		li--;
		ND_TCHECK_LEN(pptr, dstl);
		if (li < dstl) {
			ND_PRINT(", bad redirect/li");
			return;
		}
		dst = pptr;
		pptr += dstl;
                li -= dstl;
		ND_PRINT("\n\t  %s", GET_ISONSAP_STRING(dst, dstl));

		ND_TCHECK_1(pptr);
		if (li < 1) {
			ND_PRINT(", bad redirect/li");
			return;
		}
		snpal = GET_U_1(pptr);
		pptr++;
		li--;
		ND_TCHECK_LEN(pptr, snpal);
		if (li < snpal) {
			ND_PRINT(", bad redirect/li");
			return;
		}
		snpa = pptr;
		pptr += snpal;
                li -= snpal;
		ND_TCHECK_1(pptr);
		if (li < 1) {
			ND_PRINT(", bad redirect/li");
			return;
		}
		netal = GET_U_1(pptr);
		pptr++;
		ND_TCHECK_LEN(pptr, netal);
		if (li < netal) {
			ND_PRINT(", bad redirect/li");
			return;
		}
		neta = pptr;
		pptr += netal;
                li -= netal;

		if (snpal == MAC_ADDR_LEN)
			ND_PRINT("\n\t  SNPA (length: %u): %s",
			       snpal,
			       GET_ETHERADDR_STRING(snpa));
		else
			ND_PRINT("\n\t  SNPA (length: %u): %s",
			       snpal,
			       GET_LINKADDR_STRING(snpa, LINKADDR_OTHER, snpal));
		if (netal != 0)
			ND_PRINT("\n\t  NET (length: %u) %s",
			       netal,
			       GET_ISONSAP_STRING(neta, netal));
		break;
	}

	case ESIS_PDU_ESH:
            ND_TCHECK_1(pptr);
            if (li < 1) {
                ND_PRINT(", bad esh/li");
                return;
            }
            source_address_number = GET_U_1(pptr);
            pptr++;
            li--;

            ND_PRINT("\n\t  Number of Source Addresses: %u", source_address_number);

            while (source_address_number > 0) {
                ND_TCHECK_1(pptr);
		if (li < 1) {
                    ND_PRINT(", bad esh/li");
		    return;
		}
                source_address_length = GET_U_1(pptr);
                pptr++;
		li--;

                ND_TCHECK_LEN(pptr, source_address_length);
		if (li < source_address_length) {
                    ND_PRINT(", bad esh/li");
		    return;
		}
                ND_PRINT("\n\t  NET (length: %u): %s",
                       source_address_length,
                       GET_ISONSAP_STRING(pptr, source_address_length));
                pptr += source_address_length;
                li -= source_address_length;
                source_address_number--;
            }

            break;

	case ESIS_PDU_ISH: {
            ND_TCHECK_1(pptr);
            if (li < 1) {
                ND_PRINT(", bad ish/li");
                return;
            }
            source_address_length = GET_U_1(pptr);
            pptr++;
            li--;
            ND_TCHECK_LEN(pptr, source_address_length);
            if (li < source_address_length) {
                ND_PRINT(", bad ish/li");
                return;
            }
            ND_PRINT("\n\t  NET (length: %u): %s", source_address_length, GET_ISONSAP_STRING(pptr, source_address_length));
            pptr += source_address_length;
            li -= source_address_length;
            break;
	}

	default:
		if (ndo->ndo_vflag <= 1) {
			/*
			 * If there's at least one byte to print, print
			 * it/them.
			 */
			if (ND_TTEST_LEN(pptr, 1))
				print_unknown_data(ndo, pptr, "\n\t  ", ND_BYTES_AVAILABLE_AFTER(pptr));
		}
		return;
	}

        /* now walk the options */
        while (li != 0) {
            u_int op, opli;
            const uint8_t *tptr;

            if (li < 2) {
                ND_PRINT(", bad opts/li");
                return;
            }
            op = GET_U_1(pptr);
            opli = GET_U_1(pptr + 1);
            pptr += 2;
            li -= 2;
            if (opli > li) {
                ND_PRINT(", opt (%u) too long", op);
                return;
            }
            li -= opli;
            tptr = pptr;

            ND_PRINT("\n\t  %s Option #%u, length %u, value: ",
                   tok2str(esis_option_values,"Unknown",op),
                   op,
                   opli);

            switch (op) {

            case ESIS_OPTION_ES_CONF_TIME:
                if (opli == 2) {
                    ND_TCHECK_2(pptr);
                    ND_PRINT("%us", GET_BE_U_2(tptr));
                } else
                    ND_PRINT("(bad length)");
                break;

            case ESIS_OPTION_PROTOCOLS:
                while (opli>0) {
                    ND_PRINT("%s (0x%02x)",
                           tok2str(nlpid_values,
                                   "unknown",
                                   GET_U_1(tptr)),
                           GET_U_1(tptr));
                    if (opli>1) /* further NPLIDs ? - put comma */
                        ND_PRINT(", ");
                    tptr++;
                    opli--;
                }
                break;

                /*
                 * FIXME those are the defined Options that lack a decoder
                 * you are welcome to contribute code ;-)
                 */

            case ESIS_OPTION_QOS_MAINTENANCE:
            case ESIS_OPTION_SECURITY:
            case ESIS_OPTION_PRIORITY:
            case ESIS_OPTION_ADDRESS_MASK:
            case ESIS_OPTION_SNPA_MASK:

            default:
                print_unknown_data(ndo, tptr, "\n\t  ", opli);
                break;
            }
            if (ndo->ndo_vflag > 1)
                print_unknown_data(ndo, pptr, "\n\t  ", opli);
            pptr += opli;
        }
        return;

trunc:
	nd_print_trunc(ndo);
}

static void
isis_print_mcid(netdissect_options *ndo,
                const struct isis_spb_mcid *mcid)
{
  int i;

  ND_TCHECK_SIZE(mcid);
  ND_PRINT("ID: %u, Name: ", GET_U_1(mcid->format_id));

  nd_printjnp(ndo, mcid->name, sizeof(mcid->name));

  ND_PRINT("\n\t              Lvl: %u", GET_BE_U_2(mcid->revision_lvl));

  ND_PRINT(", Digest: ");

  for(i=0;i<16;i++)
    ND_PRINT("%.2x ", mcid->digest[i]);
  return;

trunc:
  nd_print_trunc(ndo);
}

static int
isis_print_mt_port_cap_subtlv(netdissect_options *ndo,
                              const uint8_t *tptr, u_int len)
{
  u_int stlv_type, stlv_len;
  const struct isis_subtlv_spb_mcid *subtlv_spb_mcid;
  int i;

  while (len > 2)
  {
    stlv_type = GET_U_1(tptr);
    stlv_len  = GET_U_1(tptr + 1);

    /* first lets see if we know the subTLVs name*/
    ND_PRINT("\n\t       %s subTLV #%u, length: %u",
               tok2str(isis_mt_port_cap_subtlv_values, "unknown", stlv_type),
               stlv_type,
               stlv_len);

    tptr += 2;
    /*len -= TLV_TYPE_LEN_OFFSET;*/
    len -= 2;

    /* Make sure the subTLV fits within the space left */
    if (len < stlv_len)
      goto subtlv_too_long;
    /* Make sure the entire subTLV is in the captured data */
    ND_TCHECK_LEN(tptr, stlv_len);

    switch (stlv_type)
    {
      case ISIS_SUBTLV_SPB_MCID:
      {
	if (stlv_len < ISIS_SUBTLV_SPB_MCID_MIN_LEN)
          goto subtlv_too_short;

        subtlv_spb_mcid = (const struct isis_subtlv_spb_mcid *)tptr;

        ND_PRINT("\n\t         MCID: ");
        isis_print_mcid(ndo, &(subtlv_spb_mcid->mcid));

          /*tptr += SPB_MCID_MIN_LEN;
            len -= SPB_MCID_MIN_LEN; */

        ND_PRINT("\n\t         AUX-MCID: ");
        isis_print_mcid(ndo, &(subtlv_spb_mcid->aux_mcid));

          /*tptr += SPB_MCID_MIN_LEN;
            len -= SPB_MCID_MIN_LEN; */
        tptr += ISIS_SUBTLV_SPB_MCID_MIN_LEN;
        len -= ISIS_SUBTLV_SPB_MCID_MIN_LEN;
        stlv_len -= ISIS_SUBTLV_SPB_MCID_MIN_LEN;

        break;
      }

      case ISIS_SUBTLV_SPB_DIGEST:
      {
        if (stlv_len < ISIS_SUBTLV_SPB_DIGEST_MIN_LEN)
          goto subtlv_too_short;

        ND_PRINT("\n\t        RES: %u V: %u A: %u D: %u",
                        (GET_U_1(tptr) >> 5),
                        ((GET_U_1(tptr) >> 4) & 0x01),
                        ((GET_U_1(tptr) >> 2) & 0x03),
                        (GET_U_1(tptr) & 0x03));

        tptr++;

        ND_PRINT("\n\t         Digest: ");

        for(i=1;i<=8; i++)
        {
            ND_PRINT("%08x ", GET_BE_U_4(tptr));
            if (i%4 == 0 && i != 8)
              ND_PRINT("\n\t                 ");
            tptr += 4;
        }

        len -= ISIS_SUBTLV_SPB_DIGEST_MIN_LEN;
        stlv_len -= ISIS_SUBTLV_SPB_DIGEST_MIN_LEN;

        break;
      }

      case ISIS_SUBTLV_SPB_BVID:
      {
        while (stlv_len != 0)
        {
          if (stlv_len < 4)
            goto subtlv_too_short;
          ND_PRINT("\n\t           ECT: %08x",
                      GET_BE_U_4(tptr));

          tptr += 4;
          len -= 4;
          stlv_len -= 4;

          if (stlv_len < 2)
            goto subtlv_too_short;
          ND_PRINT(" BVID: %u, U:%01x M:%01x ",
                     (GET_BE_U_2(tptr) >> 4) ,
                     (GET_BE_U_2(tptr) >> 3) & 0x01,
                     (GET_BE_U_2(tptr) >> 2) & 0x01);

          tptr += 2;
          len -= 2;
          stlv_len -= 2;
        }

        break;
      }

      default:
        break;
    }
    tptr += stlv_len;
    len -= stlv_len;
  }
  return (0);

trunc:
  nd_print_trunc(ndo);
  return (1);

subtlv_too_long:
  ND_PRINT(" (> containing TLV length)");
  return (1);

subtlv_too_short:
  ND_PRINT(" (too short)");
  return (1);
}

static int
isis_print_mt_capability_subtlv(netdissect_options *ndo,
                                const uint8_t *tptr, u_int len)
{
  u_int stlv_type, stlv_len, treecount;

  while (len > 2)
  {
    stlv_type = GET_U_1(tptr);
    stlv_len  = GET_U_1(tptr + 1);
    tptr += 2;
    len -= 2;

    /* first lets see if we know the subTLVs name*/
    ND_PRINT("\n\t      %s subTLV #%u, length: %u",
               tok2str(isis_mt_capability_subtlv_values, "unknown", stlv_type),
               stlv_type,
               stlv_len);

    /* Make sure the subTLV fits within the space left */
    if (len < stlv_len)
      goto subtlv_too_long;
    /* Make sure the entire subTLV is in the captured data */
    ND_TCHECK_LEN(tptr, stlv_len);

    switch (stlv_type)
    {
      case ISIS_SUBTLV_SPB_INSTANCE:
          if (stlv_len < ISIS_SUBTLV_SPB_INSTANCE_MIN_LEN)
            goto subtlv_too_short;

          ND_PRINT("\n\t        CIST Root-ID: %08x", GET_BE_U_4(tptr));
          tptr += 4;
          ND_PRINT(" %08x", GET_BE_U_4(tptr));
          tptr += 4;
          ND_PRINT(", Path Cost: %08x", GET_BE_U_4(tptr));
          tptr += 4;
          ND_PRINT(", Prio: %u", GET_BE_U_2(tptr));
          tptr += 2;
          ND_PRINT("\n\t        RES: %u",
                    GET_BE_U_2(tptr) >> 5);
          ND_PRINT(", V: %u",
                    (GET_BE_U_2(tptr) >> 4) & 0x0001);
          ND_PRINT(", SPSource-ID: %u",
                    (GET_BE_U_4(tptr) & 0x000fffff));
          tptr += 4;
          ND_PRINT(", No of Trees: %x", GET_U_1(tptr));

          treecount = GET_U_1(tptr);
          tptr++;

          len -= ISIS_SUBTLV_SPB_INSTANCE_MIN_LEN;
          stlv_len -= ISIS_SUBTLV_SPB_INSTANCE_MIN_LEN;

          while (treecount)
          {
            if (stlv_len < ISIS_SUBTLV_SPB_INSTANCE_VLAN_TUPLE_LEN)
              goto trunc;

            ND_PRINT("\n\t         U:%u, M:%u, A:%u, RES:%u",
                      GET_U_1(tptr) >> 7,
                      (GET_U_1(tptr) >> 6) & 0x01,
                      (GET_U_1(tptr) >> 5) & 0x01,
                      (GET_U_1(tptr) & 0x1f));

            tptr++;

            ND_PRINT(", ECT: %08x", GET_BE_U_4(tptr));

            tptr += 4;

            ND_PRINT(", BVID: %u, SPVID: %u",
                      (GET_BE_U_3(tptr) >> 12) & 0x000fff,
                      GET_BE_U_3(tptr) & 0x000fff);

            tptr += 3;
            len -= ISIS_SUBTLV_SPB_INSTANCE_VLAN_TUPLE_LEN;
            stlv_len -= ISIS_SUBTLV_SPB_INSTANCE_VLAN_TUPLE_LEN;
            treecount--;
          }

          break;

      case ISIS_SUBTLV_SPBM_SI:
          if (stlv_len < 8)
            goto trunc;

          ND_PRINT("\n\t        BMAC: %08x", GET_BE_U_4(tptr));
          tptr += 4;
          ND_PRINT("%04x", GET_BE_U_2(tptr));
          tptr += 2;

          ND_PRINT(", RES: %u, VID: %u", GET_BE_U_2(tptr) >> 12,
                    (GET_BE_U_2(tptr)) & 0x0fff);

          tptr += 2;
          len -= 8;
          stlv_len -= 8;

          while (stlv_len >= 4) {
            ND_PRINT("\n\t        T: %u, R: %u, RES: %u, ISID: %u",
                    (GET_BE_U_4(tptr) >> 31),
                    (GET_BE_U_4(tptr) >> 30) & 0x01,
                    (GET_BE_U_4(tptr) >> 24) & 0x03f,
                    (GET_BE_U_4(tptr)) & 0x0ffffff);

            tptr += 4;
            len -= 4;
            stlv_len -= 4;
          }

        break;

      default:
        break;
    }
    tptr += stlv_len;
    len -= stlv_len;
  }
  return (0);

trunc:
  nd_print_trunc(ndo);
  return (1);

subtlv_too_long:
  ND_PRINT(" (> containing TLV length)");
  return (1);

subtlv_too_short:
  ND_PRINT(" (too short)");
  return (1);
}

/* shared routine for printing system, node and lsp-ids */
static char *
isis_print_id(netdissect_options *ndo, const uint8_t *cp, u_int id_len)
{
    u_int i;
    static char id[sizeof("xxxx.xxxx.xxxx.yy-zz")];
    char *pos = id;
    u_int sysid_len;

    sysid_len = SYSTEM_ID_LEN;
    if (sysid_len > id_len)
        sysid_len = id_len;
    for (i = 1; i <= sysid_len; i++) {
        snprintf(pos, sizeof(id) - (pos - id), "%02x", GET_U_1(cp));
	cp++;
	pos += strlen(pos);
	if (i == 2 || i == 4)
	    *pos++ = '.';
	}
    if (id_len >= NODE_ID_LEN) {
        snprintf(pos, sizeof(id) - (pos - id), ".%02x", GET_U_1(cp));
	cp++;
	pos += strlen(pos);
    }
    if (id_len == LSP_ID_LEN)
        snprintf(pos, sizeof(id) - (pos - id), "-%02x", GET_U_1(cp));
    return (id);
}

/* print the 4-byte metric block which is common found in the old-style TLVs */
static int
isis_print_metric_block(netdissect_options *ndo,
                        const struct isis_metric_block *isis_metric_block)
{
    ND_PRINT(", Default Metric: %u, %s",
           ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_default),
           ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_default) ? "External" : "Internal");
    if (!ISIS_LSP_TLV_METRIC_SUPPORTED(isis_metric_block->metric_delay))
        ND_PRINT("\n\t\t  Delay Metric: %u, %s",
               ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_delay),
               ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_delay) ? "External" : "Internal");
    if (!ISIS_LSP_TLV_METRIC_SUPPORTED(isis_metric_block->metric_expense))
        ND_PRINT("\n\t\t  Expense Metric: %u, %s",
               ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_expense),
               ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_expense) ? "External" : "Internal");
    if (!ISIS_LSP_TLV_METRIC_SUPPORTED(isis_metric_block->metric_error))
        ND_PRINT("\n\t\t  Error Metric: %u, %s",
               ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_error),
               ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_error) ? "External" : "Internal");

    return(1); /* everything is ok */
}

static int
isis_print_tlv_ip_reach(netdissect_options *ndo,
                        const uint8_t *cp, const char *ident, u_int length)
{
	int prefix_len;
	const struct isis_tlv_ip_reach *tlv_ip_reach;

	tlv_ip_reach = (const struct isis_tlv_ip_reach *)cp;

	while (length > 0) {
		if ((size_t)length < sizeof(*tlv_ip_reach)) {
			ND_PRINT("short IPv4 Reachability (%u vs %zu)",
                               length,
                               sizeof(*tlv_ip_reach));
			return (0);
		}

		ND_TCHECK_SIZE(tlv_ip_reach);

		prefix_len = mask2plen(GET_IPV4_TO_HOST_ORDER(tlv_ip_reach->mask));

		if (prefix_len == -1)
			ND_PRINT("%sIPv4 prefix: %s mask %s",
                               ident,
			       GET_IPADDR_STRING(tlv_ip_reach->prefix),
			       GET_IPADDR_STRING(tlv_ip_reach->mask));
		else
			ND_PRINT("%sIPv4 prefix: %15s/%u",
                               ident,
			       GET_IPADDR_STRING(tlv_ip_reach->prefix),
			       prefix_len);

		ND_PRINT(", Distribution: %s, Metric: %u, %s",
                       ISIS_LSP_TLV_METRIC_UPDOWN(tlv_ip_reach->isis_metric_block.metric_default) ? "down" : "up",
                       ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_default),
                       ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_default) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->isis_metric_block.metric_delay))
                    ND_PRINT("%s  Delay Metric: %u, %s",
                           ident,
                           ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_delay),
                           ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_delay) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->isis_metric_block.metric_expense))
                    ND_PRINT("%s  Expense Metric: %u, %s",
                           ident,
                           ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_expense),
                           ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_expense) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->isis_metric_block.metric_error))
                    ND_PRINT("%s  Error Metric: %u, %s",
                           ident,
                           ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_error),
                           ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_error) ? "External" : "Internal");

		length -= sizeof(struct isis_tlv_ip_reach);
		tlv_ip_reach++;
	}
	return (1);
trunc:
	return 0;
}

/*
 * this is the common IP-REACH subTLV decoder it is called
 * from various EXTD-IP REACH TLVs (135,235,236,237)
 */

static int
isis_print_ip_reach_subtlv(netdissect_options *ndo,
                           const uint8_t *tptr, u_int subt, u_int subl,
                           const char *ident)
{
    /* first lets see if we know the subTLVs name*/
    ND_PRINT("%s%s subTLV #%u, length: %u",
              ident, tok2str(isis_ext_ip_reach_subtlv_values, "unknown", subt),
              subt, subl);

    ND_TCHECK_LEN(tptr, subl);

    switch(subt) {
    case ISIS_SUBTLV_EXTD_IP_REACH_MGMT_PREFIX_COLOR: /* fall through */
    case ISIS_SUBTLV_EXTD_IP_REACH_ADMIN_TAG32:
        while (subl >= 4) {
	    ND_PRINT(", 0x%08x (=%u)",
		   GET_BE_U_4(tptr),
		   GET_BE_U_4(tptr));
	    tptr+=4;
	    subl-=4;
	}
	break;
    case ISIS_SUBTLV_EXTD_IP_REACH_ADMIN_TAG64:
        while (subl >= 8) {
	    ND_PRINT(", 0x%08x%08x",
		   GET_BE_U_4(tptr),
		   GET_BE_U_4(tptr + 4));
	    tptr+=8;
	    subl-=8;
	}
	break;
    case ISIS_SUBTLV_EXTD_IP_REACH_PREFIX_SID:
	{
	    uint8_t algo, flags;
	    uint32_t sid;

	    flags = GET_U_1(tptr);
	    algo = GET_U_1(tptr+1);

	    if (flags & ISIS_PREFIX_SID_FLAG_V) {
	        if (subl < 5)
	            goto trunc;
		sid = GET_BE_U_3(tptr+2);
		tptr+=5;
		subl-=5;
	    } else {
	        if (subl < 6)
	            goto trunc;
		sid = GET_BE_U_4(tptr+2);
		tptr+=6;
		subl-=6;
	    }

	    ND_PRINT(", Flags [%s], Algo %s (%u), %s %u",
		     bittok2str(prefix_sid_flag_values, "None", flags),
		     tok2str(prefix_sid_algo_values, "Unknown", algo), algo,
		     flags & ISIS_PREFIX_SID_FLAG_V ? "label" : "index",
		     sid);
	}
	break;
    default:
	if (!print_unknown_data(ndo, tptr, "\n\t\t    ", subl))
	  return(0);
	break;
    }
    return(1);

trunc:
    nd_print_trunc(ndo);
    return(0);
}

/*
 * this is the common IS-REACH decoder it is called
 * from various EXTD-IS REACH style TLVs (22,24,222)
 */

static int
isis_print_ext_is_reach(netdissect_options *ndo,
                        const uint8_t *tptr, const char *ident, u_int tlv_type,
                        u_int tlv_remaining)
{
    char ident_buffer[20];
    u_int subtlv_type,subtlv_len,subtlv_sum_len;
    int proc_bytes = 0; /* how many bytes did we process ? */
    u_int te_class,priority_level,gmpls_switch_cap;
    union { /* int to float conversion buffer for several subTLVs */
        float f;
        uint32_t i;
    } bw;

    ND_TCHECK_LEN(tptr, NODE_ID_LEN);
    if (tlv_remaining < NODE_ID_LEN)
        return(0);

    ND_PRINT("%sIS Neighbor: %s", ident, isis_print_id(ndo, tptr, NODE_ID_LEN));
    tptr+=NODE_ID_LEN;
    tlv_remaining-=NODE_ID_LEN;
    proc_bytes+=NODE_ID_LEN;

    if (tlv_type != ISIS_TLV_IS_ALIAS_ID) { /* the Alias TLV Metric field is implicit 0 */
        ND_TCHECK_3(tptr);
	if (tlv_remaining < 3)
	    return(0);
	ND_PRINT(", Metric: %u", GET_BE_U_3(tptr));
	tptr+=3;
	tlv_remaining-=3;
	proc_bytes+=3;
    }

    ND_TCHECK_1(tptr);
    if (tlv_remaining < 1)
        return(0);
    subtlv_sum_len=GET_U_1(tptr); /* read out subTLV length */
    tptr++;
    tlv_remaining--;
    proc_bytes++;
    ND_PRINT(", %ssub-TLVs present",subtlv_sum_len ? "" : "no ");
    if (subtlv_sum_len) {
        ND_PRINT(" (%u)", subtlv_sum_len);
        /* prepend the indent string */
        snprintf(ident_buffer, sizeof(ident_buffer), "%s  ",ident);
        ident = ident_buffer;
        while (subtlv_sum_len != 0) {
            ND_TCHECK_2(tptr);
            if (tlv_remaining < 2) {
                ND_PRINT("%sRemaining data in TLV shorter than a subTLV header",ident);
                proc_bytes += tlv_remaining;
                break;
            }
            if (subtlv_sum_len < 2) {
                ND_PRINT("%sRemaining data in subTLVs shorter than a subTLV header",ident);
                proc_bytes += subtlv_sum_len;
                break;
            }
            subtlv_type=GET_U_1(tptr);
            subtlv_len=GET_U_1(tptr + 1);
            tptr += 2;
            tlv_remaining -= 2;
            subtlv_sum_len -= 2;
            proc_bytes += 2;
            ND_PRINT("%s%s subTLV #%u, length: %u",
                      ident, tok2str(isis_ext_is_reach_subtlv_values, "unknown", subtlv_type),
                      subtlv_type, subtlv_len);

            if (subtlv_sum_len < subtlv_len) {
                ND_PRINT(" (remaining data in subTLVs shorter than the current subTLV)");
                proc_bytes += subtlv_sum_len;
                break;
            }

            if (tlv_remaining < subtlv_len) {
                ND_PRINT(" (> remaining tlv length)");
                proc_bytes += tlv_remaining;
                break;
            }

            ND_TCHECK_LEN(tptr, subtlv_len);

            switch(subtlv_type) {
            case ISIS_SUBTLV_EXT_IS_REACH_ADMIN_GROUP:
            case ISIS_SUBTLV_EXT_IS_REACH_LINK_LOCAL_REMOTE_ID:
            case ISIS_SUBTLV_EXT_IS_REACH_LINK_REMOTE_ID:
                if (subtlv_len >= 4) {
                    ND_PRINT(", 0x%08x", GET_BE_U_4(tptr));
                    if (subtlv_len == 8) /* rfc4205 */
                        ND_PRINT(", 0x%08x", GET_BE_U_4(tptr + 4));
                }
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR:
            case ISIS_SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR:
                if (subtlv_len >= sizeof(nd_ipv4))
                    ND_PRINT(", %s", GET_IPADDR_STRING(tptr));
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_MAX_LINK_BW :
            case ISIS_SUBTLV_EXT_IS_REACH_RESERVABLE_BW:
                if (subtlv_len >= 4) {
                    bw.i = GET_BE_U_4(tptr);
                    ND_PRINT(", %.3f Mbps", bw.f * 8 / 1000000);
                }
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_UNRESERVED_BW :
                if (subtlv_len >= 32) {
                    for (te_class = 0; te_class < 8; te_class++) {
                        bw.i = GET_BE_U_4(tptr);
                        ND_PRINT("%s  TE-Class %u: %.3f Mbps",
                                  ident,
                                  te_class,
                                  bw.f * 8 / 1000000);
                        tptr += 4;
                        subtlv_len -= 4;
                        subtlv_sum_len -= 4;
                        proc_bytes += 4;
                    }
                }
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_BW_CONSTRAINTS: /* fall through */
            case ISIS_SUBTLV_EXT_IS_REACH_BW_CONSTRAINTS_OLD:
                if (subtlv_len == 0)
                    break;
                ND_PRINT("%sBandwidth Constraints Model ID: %s (%u)",
                          ident,
                          tok2str(diffserv_te_bc_values, "unknown", GET_U_1(tptr)),
                          GET_U_1(tptr));
                tptr++;
                subtlv_len--;
                subtlv_sum_len--;
                proc_bytes++;
                /* decode BCs until the subTLV ends */
                for (te_class = 0; subtlv_len != 0; te_class++) {
                    if (subtlv_len < 4)
                        break;
                    bw.i = GET_BE_U_4(tptr);
                    ND_PRINT("%s  Bandwidth constraint CT%u: %.3f Mbps",
                              ident,
                              te_class,
                              bw.f * 8 / 1000000);
                    tptr += 4;
                    subtlv_len -= 4;
                    subtlv_sum_len -= 4;
                    proc_bytes += 4;
                }
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_TE_METRIC:
                if (subtlv_len >= 3)
                    ND_PRINT(", %u", GET_BE_U_3(tptr));
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_LINK_ATTRIBUTE:
                if (subtlv_len == 2) {
                    ND_PRINT(", [ %s ] (0x%04x)",
                              bittok2str(isis_subtlv_link_attribute_values,
                                         "Unknown",
                                         GET_BE_U_2(tptr)),
                              GET_BE_U_2(tptr));
                }
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_LINK_PROTECTION_TYPE:
                if (subtlv_len >= 2) {
                    ND_PRINT(", %s, Priority %u",
                              bittok2str(gmpls_link_prot_values, "none", GET_U_1(tptr)),
                              GET_U_1(tptr + 1));
                }
                break;
            case ISIS_SUBTLV_SPB_METRIC:
                if (subtlv_len >= 6) {
                    ND_PRINT(", LM: %u", GET_BE_U_3(tptr));
                    tptr += 3;
                    subtlv_len -= 3;
                    subtlv_sum_len -= 3;
                    proc_bytes += 3;
                    ND_PRINT(", P: %u", GET_U_1(tptr));
                    tptr++;
                    subtlv_len--;
                    subtlv_sum_len--;
                    proc_bytes++;
                    ND_PRINT(", P-ID: %u", GET_BE_U_2(tptr));
                }
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_INTF_SW_CAP_DESCR:
                if (subtlv_len >= 36) {
                    gmpls_switch_cap = GET_U_1(tptr);
                    ND_PRINT("%s  Interface Switching Capability:%s",
                              ident,
                              tok2str(gmpls_switch_cap_values, "Unknown", gmpls_switch_cap));
                    ND_PRINT(", LSP Encoding: %s",
                              tok2str(gmpls_encoding_values, "Unknown", GET_U_1((tptr + 1))));
                    tptr += 4;
                    subtlv_len -= 4;
                    subtlv_sum_len -= 4;
                    proc_bytes += 4;
                    ND_PRINT("%s  Max LSP Bandwidth:", ident);
                    for (priority_level = 0; priority_level < 8; priority_level++) {
                        bw.i = GET_BE_U_4(tptr);
                        ND_PRINT("%s    priority level %u: %.3f Mbps",
                                  ident,
                                  priority_level,
                                  bw.f * 8 / 1000000);
                        tptr += 4;
                        subtlv_len -= 4;
                        subtlv_sum_len -= 4;
                        proc_bytes += 4;
                    }
                    switch (gmpls_switch_cap) {
                    case GMPLS_PSC1:
                    case GMPLS_PSC2:
                    case GMPLS_PSC3:
                    case GMPLS_PSC4:
                        if (subtlv_len < 6)
                            break;
                        bw.i = GET_BE_U_4(tptr);
                        ND_PRINT("%s  Min LSP Bandwidth: %.3f Mbps", ident, bw.f * 8 / 1000000);
                        ND_PRINT("%s  Interface MTU: %u", ident,
                                 GET_BE_U_2(tptr + 4));
                        break;
                    case GMPLS_TSC:
                        if (subtlv_len < 8)
                            break;
                        bw.i = GET_BE_U_4(tptr);
                        ND_PRINT("%s  Min LSP Bandwidth: %.3f Mbps", ident, bw.f * 8 / 1000000);
                        ND_PRINT("%s  Indication %s", ident,
                                  tok2str(gmpls_switch_cap_tsc_indication_values, "Unknown (%u)", GET_U_1((tptr + 4))));
                        break;
                    default:
                        /* there is some optional stuff left to decode but this is as of yet
                           not specified so just lets hexdump what is left */
                        if (subtlv_len != 0) {
                            if (!print_unknown_data(ndo, tptr, "\n\t\t    ", subtlv_len))
                                return(0);
                        }
                    }
                }
                break;
            case ISIS_SUBTLV_EXT_IS_REACH_LAN_ADJ_SEGMENT_ID:
                if (subtlv_len >= 8) {
                    ND_PRINT("%s  Flags: [%s]", ident,
                              bittok2str(isis_lan_adj_sid_flag_values,
                                         "none",
                                         GET_U_1(tptr)));
                    int vflag = (GET_U_1(tptr) & 0x20) ? 1:0;
                    int lflag = (GET_U_1(tptr) & 0x10) ? 1:0;
                    tptr++;
                    subtlv_len--;
                    subtlv_sum_len--;
                    proc_bytes++;
                    ND_PRINT("%s  Weight: %u", ident, GET_U_1(tptr));
                    tptr++;
                    subtlv_len--;
                    subtlv_sum_len--;
                    proc_bytes++;
                    if(subtlv_len>=SYSTEM_ID_LEN) {
                        ND_TCHECK_LEN(tptr, SYSTEM_ID_LEN);
                        ND_PRINT("%s  Neighbor System-ID: %s", ident,
                            isis_print_id(ndo, tptr, SYSTEM_ID_LEN));
                    }
                    /* RFC 8667 section 2.2.2 */
                    /* if V-flag is set to 1 and L-flag is set to 1 ==> 3 octet label */
                    /* if V-flag is set to 0 and L-flag is set to 0 ==> 4 octet index */
                    if (vflag && lflag) {
                        ND_PRINT("%s  Label: %u",
                                  ident, GET_BE_U_3(tptr+SYSTEM_ID_LEN));
                    } else if ((!vflag) && (!lflag)) {
                        ND_PRINT("%s  Index: %u",
                                  ident, GET_BE_U_4(tptr+SYSTEM_ID_LEN));
                    } else
                        nd_print_invalid(ndo);
                }
                break;
            default:
                if (!print_unknown_data(ndo, tptr, "\n\t\t    ", subtlv_len))
                    return(0);
                break;
            }

            tptr += subtlv_len;
            tlv_remaining -= subtlv_len;
            subtlv_sum_len -= subtlv_len;
            proc_bytes += subtlv_len;
        }
    }
    return(proc_bytes);

trunc:
    return(0);
}

/*
 * this is the common Multi Topology ID decoder
 * it is called from various MT-TLVs (222,229,235,237)
 */

static uint8_t
isis_print_mtid(netdissect_options *ndo,
                const uint8_t *tptr, const char *ident, u_int tlv_remaining)
{
    if (tlv_remaining < 2)
        goto trunc;

    ND_PRINT("%s%s",
           ident,
           tok2str(isis_mt_values,
                   "Reserved for IETF Consensus",
                   ISIS_MASK_MTID(GET_BE_U_2(tptr))));

    ND_PRINT(" Topology (0x%03x), Flags: [%s]",
           ISIS_MASK_MTID(GET_BE_U_2(tptr)),
           bittok2str(isis_mt_flag_values, "none",ISIS_MASK_MTFLAGS(GET_BE_U_2(tptr))));

    return(2);
trunc:
    return 0;
}

/*
 * this is the common extended IP reach decoder
 * it is called from TLVs (135,235,236,237)
 * we process the TLV and optional subTLVs and return
 * the amount of processed bytes
 */

static u_int
isis_print_extd_ip_reach(netdissect_options *ndo,
                         const uint8_t *tptr, const char *ident, uint16_t afi)
{
    char ident_buffer[20];
    uint8_t prefix[sizeof(nd_ipv6)]; /* shared copy buffer for IPv4 and IPv6 prefixes */
    u_int metric, status_byte, bit_length, byte_length, sublen, processed, subtlvtype, subtlvlen;

    metric = GET_BE_U_4(tptr);
    processed=4;
    tptr+=4;

    if (afi == AF_INET) {
        status_byte=GET_U_1(tptr);
        tptr++;
        bit_length = status_byte&0x3f;
        if (bit_length > 32) {
            ND_PRINT("%sIPv4 prefix: bad bit length %u",
                   ident,
                   bit_length);
            return (0);
        }
        processed++;
    } else if (afi == AF_INET6) {
        status_byte=GET_U_1(tptr);
        bit_length=GET_U_1(tptr + 1);
        if (bit_length > 128) {
            ND_PRINT("%sIPv6 prefix: bad bit length %u",
                   ident,
                   bit_length);
            return (0);
        }
        tptr+=2;
        processed+=2;
    } else
        return (0); /* somebody is fooling us */

    byte_length = (bit_length + 7) / 8; /* prefix has variable length encoding */

    memset(prefix, 0, sizeof(prefix));   /* clear the copy buffer */
    GET_CPY_BYTES(prefix,tptr,byte_length);    /* copy as much as is stored in the TLV */
    tptr+=byte_length;
    processed+=byte_length;

    if (afi == AF_INET)
        ND_PRINT("%sIPv4 prefix: %15s/%u",
               ident,
               ipaddr_string(ndo, prefix), /* local buffer, not packet data; don't use GET_IPADDR_STRING() */
               bit_length);
    else if (afi == AF_INET6)
        ND_PRINT("%sIPv6 prefix: %s/%u",
               ident,
               ip6addr_string(ndo, prefix), /* local buffer, not packet data; don't use GET_IP6ADDR_STRING() */
               bit_length);

    ND_PRINT(", Distribution: %s, Metric: %u",
           ISIS_MASK_TLV_EXTD_IP_UPDOWN(status_byte) ? "down" : "up",
           metric);

    if (afi == AF_INET && ISIS_MASK_TLV_EXTD_IP_SUBTLV(status_byte))
        ND_PRINT(", sub-TLVs present");
    else if (afi == AF_INET6)
        ND_PRINT(", %s%s",
               ISIS_MASK_TLV_EXTD_IP6_IE(status_byte) ? "External" : "Internal",
               ISIS_MASK_TLV_EXTD_IP6_SUBTLV(status_byte) ? ", sub-TLVs present" : "");

    if ((afi == AF_INET  && ISIS_MASK_TLV_EXTD_IP_SUBTLV(status_byte))
     || (afi == AF_INET6 && ISIS_MASK_TLV_EXTD_IP6_SUBTLV(status_byte))
            ) {
        /* assume that one prefix can hold more
           than one subTLV - therefore the first byte must reflect
           the aggregate bytecount of the subTLVs for this prefix
        */
        sublen=GET_U_1(tptr);
        tptr++;
        processed+=sublen+1;
        ND_PRINT(" (%u)", sublen);   /* print out subTLV length */

        while (sublen>0) {
            subtlvtype=GET_U_1(tptr);
            subtlvlen=GET_U_1(tptr + 1);
            tptr+=2;
            /* prepend the indent string */
            snprintf(ident_buffer, sizeof(ident_buffer), "%s  ",ident);
            if (!isis_print_ip_reach_subtlv(ndo, tptr, subtlvtype, subtlvlen, ident_buffer))
                return(0);
            tptr+=subtlvlen;
            sublen-=(subtlvlen+2);
        }
    }
    return (processed);
}

static void
isis_print_router_cap_subtlv(netdissect_options *ndo, const uint8_t *tptr, uint8_t tlen)
{
    uint8_t subt, subl;

    while (tlen >= 2) {
	subt = GET_U_1(tptr);
	subl = GET_U_1(tptr+1);
	tlen -= 2;
	tptr += 2;

	/* first lets see if we know the subTLVs name*/
	ND_PRINT("\n\t\t%s subTLV #%u, length: %u",
              tok2str(isis_router_capability_subtlv_values, "unknown", subt),
              subt, subl);

	/*
	 * Boundary check.
	 */
	if (subl > tlen) {
	    break;
	}
	ND_TCHECK_LEN(tptr, subl);

	switch (subt) {
	case ISIS_SUBTLV_ROUTER_CAP_SR:
	    {
		uint8_t flags, sid_tlen, sid_type, sid_len;
		uint32_t range;
		const uint8_t *sid_ptr;

		flags = GET_U_1(tptr);
		range = GET_BE_U_3(tptr+1);
		ND_PRINT(", Flags [%s], Range %u",
			 bittok2str(isis_router_capability_sr_flags, "None", flags),
			 range);
		sid_ptr = tptr + 4;
		sid_tlen = subl - 4;

		while (sid_tlen >= 5) {
		    sid_type = GET_U_1(sid_ptr);
		    sid_len = GET_U_1(sid_ptr+1);
		    sid_tlen -= 2;
		    sid_ptr += 2;

		    /*
		     * Boundary check.
		     */
		    if (sid_len > sid_tlen) {
			break;
		    }

		    switch (sid_type) {
		    case 1:
			if (sid_len == 3) {
			    ND_PRINT(", SID value %u", GET_BE_U_3(sid_ptr));
			} else if (sid_len == 4) {
			    ND_PRINT(", SID value %u", GET_BE_U_4(sid_ptr));
			} else {
			    ND_PRINT(", Unknown SID length%u", sid_len);
			}
			break;
		    default:
			print_unknown_data(ndo, sid_ptr, "\n\t\t  ", sid_len);
		    }

		    sid_ptr += sid_len;
		    sid_tlen -= sid_len;
		}
	    }
	    break;
	default:
	    print_unknown_data(ndo, tptr, "\n\t\t", subl);
	    break;
	}

	tlen -= subl;
	tptr += subl;
    }
 trunc:
    return;
}

/*
 * Clear checksum and lifetime prior to signature verification.
 */
static void
isis_clear_checksum_lifetime(void *header)
{
    struct isis_lsp_header *header_lsp = (struct isis_lsp_header *) header;

    header_lsp->checksum[0] = 0;
    header_lsp->checksum[1] = 0;
    header_lsp->remaining_lifetime[0] = 0;
    header_lsp->remaining_lifetime[1] = 0;
}

/*
 * isis_print
 * Decode IS-IS packets.  Return 0 on error.
 */

#define INVALID_OR_DECREMENT(length,decr) \
    if ((length) < (decr)) { \
        ND_PRINT(" [packet length %u < %zu]", (length), (decr)); \
        nd_print_invalid(ndo); \
        return 1; \
    } \
    length -= (decr);

static int
isis_print(netdissect_options *ndo,
           const uint8_t *p, u_int length)
{
    const struct isis_common_header *isis_header;

    const struct isis_iih_lan_header *header_iih_lan;
    const struct isis_iih_ptp_header *header_iih_ptp;
    const struct isis_lsp_header *header_lsp;
    const struct isis_csnp_header *header_csnp;
    const struct isis_psnp_header *header_psnp;

    const struct isis_tlv_lsp *tlv_lsp;
    const struct isis_tlv_ptp_adj *tlv_ptp_adj;
    const struct isis_tlv_is_reach *tlv_is_reach;
    const struct isis_tlv_es_reach *tlv_es_reach;

    uint8_t version, pdu_version, fixed_len;
    uint8_t pdu_type, pdu_max_area, max_area, pdu_id_length, id_length, tlv_type, tlv_len, tlen, alen, prefix_len;
    u_int ext_is_len, ext_ip_len;
    uint8_t mt_len;
    uint8_t isis_subtlv_idrp;
    const uint8_t *optr, *pptr, *tptr;
    u_int packet_len;
    u_short pdu_len, key_id;
    u_int i,vendor_id, num_vals;
    uint8_t auth_type;
    uint8_t num_system_ids;
    int sigcheck;

    ndo->ndo_protocol = "isis";
    packet_len=length;
    optr = p; /* initialize the _o_riginal pointer to the packet start -
                 need it for parsing the checksum TLV and authentication
                 TLV verification */
    isis_header = (const struct isis_common_header *)p;
    ND_TCHECK_SIZE(isis_header);
    if (length < ISIS_COMMON_HEADER_SIZE)
        goto trunc;
    pptr = p+(ISIS_COMMON_HEADER_SIZE);
    header_iih_lan = (const struct isis_iih_lan_header *)pptr;
    header_iih_ptp = (const struct isis_iih_ptp_header *)pptr;
    header_lsp = (const struct isis_lsp_header *)pptr;
    header_csnp = (const struct isis_csnp_header *)pptr;
    header_psnp = (const struct isis_psnp_header *)pptr;

    if (!ndo->ndo_eflag)
        ND_PRINT("IS-IS");

    /*
     * Sanity checking of the header.
     */

    version = GET_U_1(isis_header->version);
    if (version != ISIS_VERSION) {
	ND_PRINT("version %u packet not supported", version);
	return (0);
    }

    pdu_id_length = GET_U_1(isis_header->id_length);
    if ((pdu_id_length != SYSTEM_ID_LEN) && (pdu_id_length != 0)) {
	ND_PRINT("system ID length of %u is not supported",
	       pdu_id_length);
	return (0);
    }

    pdu_version = GET_U_1(isis_header->pdu_version);
    if (pdu_version != ISIS_VERSION) {
	ND_PRINT("version %u packet not supported", pdu_version);
	return (0);
    }

    fixed_len = GET_U_1(isis_header->fixed_len);
    if (length < fixed_len) {
	ND_PRINT("fixed header length %u > packet length %u", fixed_len, length);
	return (0);
    }

    if (fixed_len < ISIS_COMMON_HEADER_SIZE) {
	ND_PRINT("fixed header length %u < minimum header size %u", fixed_len, (u_int)ISIS_COMMON_HEADER_SIZE);
	return (0);
    }

    pdu_max_area = GET_U_1(isis_header->max_area);
    switch(pdu_max_area) {
    case 0:
	max_area = 3;	 /* silly shit */
	break;
    case 255:
	ND_PRINT("bad packet -- 255 areas");
	return (0);
    default:
        max_area = pdu_max_area;
	break;
    }

    switch(pdu_id_length) {
    case 0:
        id_length = 6;	 /* silly shit again */
	break;
    case 1:              /* 1-8 are valid sys-ID lengths */
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        id_length = pdu_id_length;
        break;
    case 255:
        id_length = 0;   /* entirely useless */
	break;
    default:
        id_length = pdu_id_length;
        break;
    }

    /* toss any non 6-byte sys-ID len PDUs */
    if (id_length != 6 ) {
	ND_PRINT("bad packet -- illegal sys-ID length (%u)", id_length);
	return (0);
    }

    pdu_type = GET_U_1(isis_header->pdu_type);

    /* in non-verbose mode print the basic PDU Type plus PDU specific brief information*/
    if (ndo->ndo_vflag == 0) {
        ND_PRINT("%s%s",
               ndo->ndo_eflag ? "" : ", ",
               tok2str(isis_pdu_values, "unknown PDU-Type %u", pdu_type));
    } else {
        /* ok they seem to want to know everything - lets fully decode it */
        ND_PRINT("%slength %u", ndo->ndo_eflag ? "" : ", ", length);

        ND_PRINT("\n\t%s, hlen: %u, v: %u, pdu-v: %u, sys-id-len: %u (%u), max-area: %u (%u)",
               tok2str(isis_pdu_values,
                       "unknown, type %u",
                       pdu_type),
               fixed_len,
               version,
               pdu_version,
               id_length,
               pdu_id_length,
               max_area,
               pdu_max_area);

        if (ndo->ndo_vflag > 1) {
            if (!print_unknown_data(ndo, optr, "\n\t", 8)) /* provide the _o_riginal pointer */
                return (0);                         /* for optionally debugging the common header */
        }
    }

    switch (pdu_type) {

    case ISIS_PDU_L1_LAN_IIH:
    case ISIS_PDU_L2_LAN_IIH:
        if (fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE)) {
            ND_PRINT(", bogus fixed header length %u should be %zu",
                     fixed_len, ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE);
            return (0);
        }
        ND_TCHECK_SIZE(header_iih_lan);
        if (length < ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE)
            goto trunc;
        if (ndo->ndo_vflag == 0) {
            ND_PRINT(", src-id %s",
                      isis_print_id(ndo, header_iih_lan->source_id, SYSTEM_ID_LEN));
            ND_PRINT(", lan-id %s, prio %u",
                      isis_print_id(ndo, header_iih_lan->lan_id,NODE_ID_LEN),
                      GET_U_1(header_iih_lan->priority));
            ND_PRINT(", length %u", length);
            return (1);
        }
        pdu_len=GET_BE_U_2(header_iih_lan->pdu_len);
        if (packet_len>pdu_len) {
           packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
           length=pdu_len;
        }

        ND_PRINT("\n\t  source-id: %s,  holding time: %us, Flags: [%s]",
                  isis_print_id(ndo, header_iih_lan->source_id,SYSTEM_ID_LEN),
                  GET_BE_U_2(header_iih_lan->holding_time),
                  tok2str(isis_iih_circuit_type_values,
                          "unknown circuit type 0x%02x",
                          GET_U_1(header_iih_lan->circuit_type)));

        ND_PRINT("\n\t  lan-id:    %s, Priority: %u, PDU length: %u",
                  isis_print_id(ndo, header_iih_lan->lan_id, NODE_ID_LEN),
                  GET_U_1(header_iih_lan->priority) & ISIS_LAN_PRIORITY_MASK,
                  pdu_len);

        if (ndo->ndo_vflag > 1) {
            if (!print_unknown_data(ndo, pptr, "\n\t  ", ISIS_IIH_LAN_HEADER_SIZE))
                return (0);
        }

        INVALID_OR_DECREMENT(packet_len,ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE);
        pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE);
        break;

    case ISIS_PDU_PTP_IIH:
        if (fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE)) {
            ND_PRINT(", bogus fixed header length %u should be %zu",
                      fixed_len, ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE);
            return (0);
        }
        ND_TCHECK_SIZE(header_iih_ptp);
        if (length < ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE)
            goto trunc;
        if (ndo->ndo_vflag == 0) {
            ND_PRINT(", src-id %s", isis_print_id(ndo, header_iih_ptp->source_id, SYSTEM_ID_LEN));
            ND_PRINT(", length %u", length);
            return (1);
        }
        pdu_len=GET_BE_U_2(header_iih_ptp->pdu_len);
        if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
        }

        ND_PRINT("\n\t  source-id: %s, holding time: %us, Flags: [%s]",
                  isis_print_id(ndo, header_iih_ptp->source_id,SYSTEM_ID_LEN),
                  GET_BE_U_2(header_iih_ptp->holding_time),
                  tok2str(isis_iih_circuit_type_values,
                          "unknown circuit type 0x%02x",
                          GET_U_1(header_iih_ptp->circuit_type)));

        ND_PRINT("\n\t  circuit-id: 0x%02x, PDU length: %u",
                  GET_U_1(header_iih_ptp->circuit_id),
                  pdu_len);

        if (ndo->ndo_vflag > 1) {
            if (!print_unknown_data(ndo, pptr, "\n\t  ", ISIS_IIH_PTP_HEADER_SIZE))
                return (0);
        }
        INVALID_OR_DECREMENT(packet_len,ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE);
        pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE);
        break;

    case ISIS_PDU_L1_LSP:
    case ISIS_PDU_L2_LSP:
        if (fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_LSP_HEADER_SIZE)) {
            ND_PRINT(", bogus fixed header length %u should be %zu",
                   fixed_len, ISIS_LSP_HEADER_SIZE);
            return (0);
        }
        ND_TCHECK_SIZE(header_lsp);
        if (length < ISIS_COMMON_HEADER_SIZE+ISIS_LSP_HEADER_SIZE)
            goto trunc;
        if (ndo->ndo_vflag == 0) {
            ND_PRINT(", lsp-id %s, seq 0x%08x, lifetime %5us",
                      isis_print_id(ndo, header_lsp->lsp_id, LSP_ID_LEN),
                      GET_BE_U_4(header_lsp->sequence_number),
                      GET_BE_U_2(header_lsp->remaining_lifetime));
            ND_PRINT(", length %u", length);
            return (1);
        }
        pdu_len=GET_BE_U_2(header_lsp->pdu_len);
        if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
        }

        ND_PRINT("\n\t  lsp-id: %s, seq: 0x%08x, lifetime: %5us\n\t  chksum: 0x%04x",
               isis_print_id(ndo, header_lsp->lsp_id, LSP_ID_LEN),
               GET_BE_U_4(header_lsp->sequence_number),
               GET_BE_U_2(header_lsp->remaining_lifetime),
               GET_BE_U_2(header_lsp->checksum));

        osi_print_cksum(ndo, (const uint8_t *)header_lsp->lsp_id,
                        GET_BE_U_2(header_lsp->checksum),
                        12, length-12);

        ND_PRINT(", PDU length: %u, Flags: [ %s",
               pdu_len,
               ISIS_MASK_LSP_OL_BIT(header_lsp->typeblock) ? "Overload bit set, " : "");

        if (ISIS_MASK_LSP_ATT_BITS(header_lsp->typeblock)) {
            ND_PRINT("%s", ISIS_MASK_LSP_ATT_DEFAULT_BIT(header_lsp->typeblock) ? "default " : "");
            ND_PRINT("%s", ISIS_MASK_LSP_ATT_DELAY_BIT(header_lsp->typeblock) ? "delay " : "");
            ND_PRINT("%s", ISIS_MASK_LSP_ATT_EXPENSE_BIT(header_lsp->typeblock) ? "expense " : "");
            ND_PRINT("%s", ISIS_MASK_LSP_ATT_ERROR_BIT(header_lsp->typeblock) ? "error " : "");
            ND_PRINT("ATT bit set, ");
        }
        ND_PRINT("%s", ISIS_MASK_LSP_PARTITION_BIT(header_lsp->typeblock) ? "P bit set, " : "");
        ND_PRINT("%s ]", tok2str(isis_lsp_istype_values, "Unknown(0x%x)",
                  ISIS_MASK_LSP_ISTYPE_BITS(header_lsp->typeblock)));

        if (ndo->ndo_vflag > 1) {
            if (!print_unknown_data(ndo, pptr, "\n\t  ", ISIS_LSP_HEADER_SIZE))
                return (0);
        }

        INVALID_OR_DECREMENT(packet_len,ISIS_COMMON_HEADER_SIZE+ISIS_LSP_HEADER_SIZE);
        pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_LSP_HEADER_SIZE);
        break;

    case ISIS_PDU_L1_CSNP:
    case ISIS_PDU_L2_CSNP:
        if (fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE)) {
            ND_PRINT(", bogus fixed header length %u should be %zu",
                      fixed_len, ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE);
            return (0);
        }
        ND_TCHECK_SIZE(header_csnp);
        if (length < ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE)
            goto trunc;
        if (ndo->ndo_vflag == 0) {
            ND_PRINT(", src-id %s", isis_print_id(ndo, header_csnp->source_id, NODE_ID_LEN));
            ND_PRINT(", length %u", length);
            return (1);
        }
        pdu_len=GET_BE_U_2(header_csnp->pdu_len);
        if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
        }

        ND_PRINT("\n\t  source-id:    %s, PDU length: %u",
               isis_print_id(ndo, header_csnp->source_id, NODE_ID_LEN),
               pdu_len);
        ND_PRINT("\n\t  start lsp-id: %s",
               isis_print_id(ndo, header_csnp->start_lsp_id, LSP_ID_LEN));
        ND_PRINT("\n\t  end lsp-id:   %s",
               isis_print_id(ndo, header_csnp->end_lsp_id, LSP_ID_LEN));

        if (ndo->ndo_vflag > 1) {
            if (!print_unknown_data(ndo, pptr, "\n\t  ", ISIS_CSNP_HEADER_SIZE))
                return (0);
        }

        INVALID_OR_DECREMENT(packet_len,ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE);
        pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE);
        break;

    case ISIS_PDU_L1_PSNP:
    case ISIS_PDU_L2_PSNP:
        if (fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE)) {
            ND_PRINT("- bogus fixed header length %u should be %zu",
                   fixed_len, ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE);
            return (0);
        }
        ND_TCHECK_SIZE(header_psnp);
        if (length < ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE)
            goto trunc;
        if (ndo->ndo_vflag == 0) {
            ND_PRINT(", src-id %s", isis_print_id(ndo, header_psnp->source_id, NODE_ID_LEN));
            ND_PRINT(", length %u", length);
            return (1);
        }
        pdu_len=GET_BE_U_2(header_psnp->pdu_len);
        if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
        }

        ND_PRINT("\n\t  source-id:    %s, PDU length: %u",
               isis_print_id(ndo, header_psnp->source_id, NODE_ID_LEN),
               pdu_len);

        if (ndo->ndo_vflag > 1) {
            if (!print_unknown_data(ndo, pptr, "\n\t  ", ISIS_PSNP_HEADER_SIZE))
                return (0);
        }

        INVALID_OR_DECREMENT(packet_len,ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE);
        pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE);
        break;

    default:
        if (ndo->ndo_vflag == 0) {
            ND_PRINT(", length %u", length);
            return (1);
        }
	(void)print_unknown_data(ndo, pptr, "\n\t  ", length);
	return (0);
    }

    /*
     * Now print the TLV's.
     */

    while (packet_len > 0) {
	ND_TCHECK_2(pptr);
	if (packet_len < 2)
	    goto trunc;
	tlv_type = GET_U_1(pptr);
	tlv_len = GET_U_1(pptr + 1);
	pptr += 2;
	packet_len -= 2;
        tlen = tlv_len; /* copy temporary len & pointer to packet data */
        tptr = pptr;

        /* first lets see if we know the TLVs name*/
	ND_PRINT("\n\t    %s TLV #%u, length: %u",
               tok2str(isis_tlv_values,
                       "unknown",
                       tlv_type),
               tlv_type,
               tlv_len);

	if (packet_len < tlv_len)
	    goto trunc;

        /* now check if we have a decoder otherwise do a hexdump at the end*/
	switch (tlv_type) {
	case ISIS_TLV_AREA_ADDR:
	    while (tlen != 0) {
		alen = GET_U_1(tptr);
		tptr++;
		tlen--;
		if (tlen < alen)
		    goto tlv_trunc;
		ND_PRINT("\n\t      Area address (length: %u): %s",
                       alen,
                       GET_ISONSAP_STRING(tptr, alen));
		tptr += alen;
		tlen -= alen;
	    }
	    break;
	case ISIS_TLV_ISNEIGH:
	    while (tlen != 0) {
		if (tlen < MAC_ADDR_LEN)
		    goto tlv_trunc;
                ND_TCHECK_LEN(tptr, MAC_ADDR_LEN);
                ND_PRINT("\n\t      SNPA: %s", isis_print_id(ndo, tptr, MAC_ADDR_LEN));
                tlen -= MAC_ADDR_LEN;
                tptr += MAC_ADDR_LEN;
	    }
	    break;

        case ISIS_TLV_INSTANCE_ID:
            if (tlen < 4)
                goto tlv_trunc;
            num_vals = (tlen-2)/2;
            ND_PRINT("\n\t      Instance ID: %u, ITIDs(%u)%s ",
                     GET_BE_U_2(tptr), num_vals,
                     num_vals ? ":" : "");
            tptr += 2;
            tlen -= 2;
            for (i=0; i < num_vals; i++) {
                ND_PRINT("%u", GET_BE_U_2(tptr));
                if (i < (num_vals - 1)) {
                   ND_PRINT(", ");
                }
                tptr += 2;
                tlen -= 2;
            }
            break;

	case ISIS_TLV_PADDING:
	    break;

        case ISIS_TLV_MT_IS_REACH:
            mt_len = isis_print_mtid(ndo, tptr, "\n\t      ", tlen);
            if (mt_len == 0) /* did something go wrong ? */
                goto trunc;
            tptr+=mt_len;
            tlen-=mt_len;
            while (tlen != 0) {
                ext_is_len = isis_print_ext_is_reach(ndo, tptr, "\n\t      ", tlv_type, tlen);
                if (ext_is_len == 0) /* did something go wrong ? */
                    goto trunc;
                if (tlen < ext_is_len) {
                    ND_PRINT(" [remaining tlv length %u < %u]", tlen, ext_is_len);
                    nd_print_invalid(ndo);
                    break;
                }
                tlen-=(uint8_t)ext_is_len;
                tptr+=(uint8_t)ext_is_len;
            }
            break;

        case ISIS_TLV_IS_ALIAS_ID:
	    while (tlen != 0) {
	        ext_is_len = isis_print_ext_is_reach(ndo, tptr, "\n\t      ", tlv_type, tlen);
		if (ext_is_len == 0) /* did something go wrong ? */
	            goto trunc;
                if (tlen < ext_is_len) {
                    ND_PRINT(" [remaining tlv length %u < %u]", tlen, ext_is_len);
                    nd_print_invalid(ndo);
                    break;
                }
		tlen-=(uint8_t)ext_is_len;
		tptr+=(uint8_t)ext_is_len;
	    }
	    break;

        case ISIS_TLV_EXT_IS_REACH:
            while (tlen != 0) {
                ext_is_len = isis_print_ext_is_reach(ndo, tptr, "\n\t      ", tlv_type, tlen);
                if (ext_is_len == 0) /* did something go wrong ? */
                    goto trunc;
                if (tlen < ext_is_len) {
                    ND_PRINT(" [remaining tlv length %u < %u]", tlen, ext_is_len);
                    nd_print_invalid(ndo);
                    break;
                }
                tlen-=(uint8_t)ext_is_len;
                tptr+=(uint8_t)ext_is_len;
            }
            break;
        case ISIS_TLV_IS_REACH:
            if (tlen < 1)
                goto tlv_trunc;
            ND_PRINT("\n\t      %s",
                   tok2str(isis_is_reach_virtual_values,
                           "bogus virtual flag 0x%02x",
                           GET_U_1(tptr)));
	    tptr++;
	    tlen--;
	    tlv_is_reach = (const struct isis_tlv_is_reach *)tptr;
            while (tlen != 0) {
                if (tlen < sizeof(struct isis_tlv_is_reach))
                    goto tlv_trunc;
		ND_TCHECK_SIZE(tlv_is_reach);
		ND_PRINT("\n\t      IS Neighbor: %s",
		       isis_print_id(ndo, tlv_is_reach->neighbor_nodeid, NODE_ID_LEN));
		isis_print_metric_block(ndo, &tlv_is_reach->isis_metric_block);
		tlen -= sizeof(struct isis_tlv_is_reach);
		tlv_is_reach++;
	    }
            break;

        case ISIS_TLV_ESNEIGH:
	    tlv_es_reach = (const struct isis_tlv_es_reach *)tptr;
            while (tlen != 0) {
                if (tlen < sizeof(struct isis_tlv_es_reach))
                    goto tlv_trunc;
		ND_TCHECK_SIZE(tlv_es_reach);
		ND_PRINT("\n\t      ES Neighbor: %s",
                       isis_print_id(ndo, tlv_es_reach->neighbor_sysid, SYSTEM_ID_LEN));
		isis_print_metric_block(ndo, &tlv_es_reach->isis_metric_block);
		tlen -= sizeof(struct isis_tlv_es_reach);
		tlv_es_reach++;
	    }
            break;

            /* those two TLVs share the same format */
	case ISIS_TLV_INT_IP_REACH:
	case ISIS_TLV_EXT_IP_REACH:
		if (!isis_print_tlv_ip_reach(ndo, pptr, "\n\t      ", tlv_len))
			return (1);
		break;

	case ISIS_TLV_EXTD_IP_REACH:
	    while (tlen != 0) {
                ext_ip_len = isis_print_extd_ip_reach(ndo, tptr, "\n\t      ", AF_INET);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunc;
                if (tlen < ext_ip_len) {
                    ND_PRINT(" [remaining tlv length %u < %u]", tlen, ext_ip_len);
                    nd_print_invalid(ndo);
                    break;
                }
                tlen-=(uint8_t)ext_ip_len;
                tptr+=(uint8_t)ext_ip_len;
            }
            break;

        case ISIS_TLV_MT_IP_REACH:
            mt_len = isis_print_mtid(ndo, tptr, "\n\t      ", tlen);
            if (mt_len == 0) { /* did something go wrong ? */
                goto trunc;
            }
            tptr+=mt_len;
            tlen-=mt_len;

            while (tlen != 0) {
                ext_ip_len = isis_print_extd_ip_reach(ndo, tptr, "\n\t      ", AF_INET);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunc;
                if (tlen < ext_ip_len) {
                    ND_PRINT(" [remaining tlv length %u < %u]", tlen, ext_ip_len);
                    nd_print_invalid(ndo);
                    break;
                }
                tlen-=(uint8_t)ext_ip_len;
                tptr+=(uint8_t)ext_ip_len;
            }
            break;

	case ISIS_TLV_IP6_REACH:
            while (tlen != 0) {
                ext_ip_len = isis_print_extd_ip_reach(ndo, tptr, "\n\t      ", AF_INET6);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunc;
                if (tlen < ext_ip_len) {
                    ND_PRINT(" [remaining tlv length %u < %u]", tlen, ext_ip_len);
                    nd_print_invalid(ndo);
                    break;
                }
                tlen-=(uint8_t)ext_ip_len;
                tptr+=(uint8_t)ext_ip_len;
            }
            break;

	case ISIS_TLV_MT_IP6_REACH:
            mt_len = isis_print_mtid(ndo, tptr, "\n\t      ", tlen);
            if (mt_len == 0) { /* did something go wrong ? */
                goto trunc;
            }
            tptr+=mt_len;
            tlen-=mt_len;

            while (tlen != 0) {
                ext_ip_len = isis_print_extd_ip_reach(ndo, tptr, "\n\t      ", AF_INET6);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunc;
                if (tlen < ext_ip_len) {
                    ND_PRINT(" [remaining tlv length %u < %u]", tlen, ext_ip_len);
                    nd_print_invalid(ndo);
                    break;
                }
                tlen-=(uint8_t)ext_ip_len;
                tptr+=(uint8_t)ext_ip_len;
            }
            break;

	case ISIS_TLV_IP6ADDR:
	    while (tlen != 0) {
                if (tlen < sizeof(nd_ipv6))
                    goto tlv_trunc;
                ND_PRINT("\n\t      IPv6 interface address: %s",
		       GET_IP6ADDR_STRING(tptr));

		tptr += sizeof(nd_ipv6);
		tlen -= sizeof(nd_ipv6);
	    }
	    break;
	case ISIS_TLV_AUTH:
	    if (tlen < 1)
	        goto tlv_trunc;
	    auth_type = GET_U_1(tptr);
	    tptr++;
	    tlen--;

            ND_PRINT("\n\t      %s: ",
                   tok2str(isis_subtlv_auth_values,
                           "unknown Authentication type 0x%02x",
                           auth_type));

	    switch (auth_type) {
	    case ISIS_SUBTLV_AUTH_SIMPLE:
		nd_printjnp(ndo, tptr, tlen);
		break;
	    case ISIS_SUBTLV_AUTH_MD5:
		for(i=0;i<tlen;i++) {
		    ND_PRINT("%02x", GET_U_1(tptr + i));
		}
		if (tlen != ISIS_SUBTLV_AUTH_MD5_LEN)
                    ND_PRINT(", (invalid subTLV) ");

                sigcheck = signature_verify(ndo, optr, length, tptr,
                                            isis_clear_checksum_lifetime,
                                            header_lsp);
                ND_PRINT(" (%s)", tok2str(signature_check_values, "Unknown", sigcheck));

		break;
            case ISIS_SUBTLV_AUTH_GENERIC:
                if (tlen < 2)
                    goto tlv_trunc;
                key_id = GET_BE_U_2(tptr);
                ND_PRINT("%u, password: ", key_id);
                tptr += 2;
                tlen -= 2;
                for(i=0;i<tlen;i++) {
                    ND_PRINT("%02x", GET_U_1(tptr + i));
                }
                break;
	    case ISIS_SUBTLV_AUTH_PRIVATE:
	    default:
		if (!print_unknown_data(ndo, tptr, "\n\t\t  ", tlen))
		    return(0);
		break;
	    }
	    break;

	case ISIS_TLV_PTP_ADJ:
	    tlv_ptp_adj = (const struct isis_tlv_ptp_adj *)tptr;
	    if(tlen>=1) {
		ND_PRINT("\n\t      Adjacency State: %s (%u)",
		       tok2str(isis_ptp_adjancey_values, "unknown", GET_U_1(tptr)),
		       GET_U_1(tptr));
		tlen--;
	    }
	    if(tlen>sizeof(tlv_ptp_adj->extd_local_circuit_id)) {
		ND_PRINT("\n\t      Extended Local circuit-ID: 0x%08x",
		       GET_BE_U_4(tlv_ptp_adj->extd_local_circuit_id));
		tlen-=sizeof(tlv_ptp_adj->extd_local_circuit_id);
	    }
	    if(tlen>=SYSTEM_ID_LEN) {
		ND_TCHECK_LEN(tlv_ptp_adj->neighbor_sysid, SYSTEM_ID_LEN);
		ND_PRINT("\n\t      Neighbor System-ID: %s",
		       isis_print_id(ndo, tlv_ptp_adj->neighbor_sysid, SYSTEM_ID_LEN));
		tlen-=SYSTEM_ID_LEN;
	    }
	    if(tlen>=sizeof(tlv_ptp_adj->neighbor_extd_local_circuit_id)) {
		ND_PRINT("\n\t      Neighbor Extended Local circuit-ID: 0x%08x",
		       GET_BE_U_4(tlv_ptp_adj->neighbor_extd_local_circuit_id));
	    }
	    break;

	case ISIS_TLV_PROTOCOLS:
	    ND_PRINT("\n\t      NLPID(s): ");
	    while (tlen != 0) {
		ND_PRINT("%s (0x%02x)",
                       tok2str(nlpid_values,
                               "unknown",
                               GET_U_1(tptr)),
                       GET_U_1(tptr));
		if (tlen>1) /* further NPLIDs ? - put comma */
		    ND_PRINT(", ");
                tptr++;
                tlen--;
	    }
	    break;

        case ISIS_TLV_MT_PORT_CAP:
        {
            if (tlen < 2)
                goto tlv_trunc;

            ND_PRINT("\n\t       RES: %u, MTID(s): %u",
                    (GET_BE_U_2(tptr) >> 12),
                    (GET_BE_U_2(tptr) & 0x0fff));

            tptr += 2;
            tlen -= 2;

            if (tlen)
                isis_print_mt_port_cap_subtlv(ndo, tptr, tlen);

            break;
        }

        case ISIS_TLV_MT_CAPABILITY:
            if (tlen < 2)
                goto tlv_trunc;

            ND_PRINT("\n\t      O: %u, RES: %u, MTID(s): %u",
                      (GET_BE_U_2(tptr) >> 15) & 0x01,
                      (GET_BE_U_2(tptr) >> 12) & 0x07,
                      GET_BE_U_2(tptr) & 0x0fff);

            tptr += 2;
            tlen -= 2;

            if (tlen)
                isis_print_mt_capability_subtlv(ndo, tptr, tlen);

            break;

	case ISIS_TLV_TE_ROUTER_ID:
	    if (tlen < sizeof(nd_ipv4))
	        goto tlv_trunc;
	    ND_PRINT("\n\t      Traffic Engineering Router ID: %s", GET_IPADDR_STRING(pptr));
	    break;

	case ISIS_TLV_IPADDR:
	    while (tlen != 0) {
                if (tlen < sizeof(nd_ipv4))
                    goto tlv_trunc;
		ND_PRINT("\n\t      IPv4 interface address: %s", GET_IPADDR_STRING(tptr));
		tptr += sizeof(nd_ipv4);
		tlen -= sizeof(nd_ipv4);
	    }
	    break;

	case ISIS_TLV_HOSTNAME:
	    ND_PRINT("\n\t      Hostname: ");
	    nd_printjnp(ndo, tptr, tlen);
	    break;

	case ISIS_TLV_SHARED_RISK_GROUP:
	    if (tlen < NODE_ID_LEN)
	        break;
	    ND_TCHECK_LEN(tptr, NODE_ID_LEN);
	    ND_PRINT("\n\t      IS Neighbor: %s", isis_print_id(ndo, tptr, NODE_ID_LEN));
	    tptr+=NODE_ID_LEN;
	    tlen-=NODE_ID_LEN;

	    if (tlen < 1)
	        break;
	    ND_PRINT(", Flags: [%s]",
                     ISIS_MASK_TLV_SHARED_RISK_GROUP(GET_U_1(tptr)) ? "numbered" : "unnumbered");
	    tptr++;
	    tlen--;

	    if (tlen < sizeof(nd_ipv4))
	        break;
	    ND_PRINT("\n\t      IPv4 interface address: %s", GET_IPADDR_STRING(tptr));
	    tptr+=sizeof(nd_ipv4);
	    tlen-=sizeof(nd_ipv4);

	    if (tlen < sizeof(nd_ipv4))
	        break;
	    ND_PRINT("\n\t      IPv4 neighbor address: %s", GET_IPADDR_STRING(tptr));
	    tptr+=sizeof(nd_ipv4);
	    tlen-=sizeof(nd_ipv4);

	    while (tlen != 0) {
		if (tlen < 4)
		    goto tlv_trunc;
                ND_PRINT("\n\t      Link-ID: 0x%08x", GET_BE_U_4(tptr));
                tptr+=4;
                tlen-=4;
	    }
	    break;

	case ISIS_TLV_LSP:
	    tlv_lsp = (const struct isis_tlv_lsp *)tptr;
	    while (tlen != 0) {
		if (tlen < sizeof(struct isis_tlv_lsp))
		    goto tlv_trunc;
		ND_TCHECK_1(tlv_lsp->lsp_id + LSP_ID_LEN - 1);
		ND_PRINT("\n\t      lsp-id: %s",
                       isis_print_id(ndo, tlv_lsp->lsp_id, LSP_ID_LEN));
		ND_PRINT(", seq: 0x%08x",
                         GET_BE_U_4(tlv_lsp->sequence_number));
		ND_PRINT(", lifetime: %5ds",
                         GET_BE_U_2(tlv_lsp->remaining_lifetime));
		ND_PRINT(", chksum: 0x%04x", GET_BE_U_2(tlv_lsp->checksum));
		tlen-=sizeof(struct isis_tlv_lsp);
		tlv_lsp++;
	    }
	    break;

	case ISIS_TLV_CHECKSUM:
	    if (tlen < ISIS_TLV_CHECKSUM_MINLEN)
	        break;
	    ND_TCHECK_LEN(tptr, ISIS_TLV_CHECKSUM_MINLEN);
	    ND_PRINT("\n\t      checksum: 0x%04x ", GET_BE_U_2(tptr));
            /* do not attempt to verify the checksum if it is zero
             * most likely a HMAC-MD5 TLV is also present and
             * to avoid conflicts the checksum TLV is zeroed.
             * see rfc3358 for details
             */
            osi_print_cksum(ndo, optr, GET_BE_U_2(tptr), (int)(tptr-optr),
                            length);
	    break;

	case ISIS_TLV_POI:
	    if (tlen < 1)
	        goto tlv_trunc;
	    num_system_ids = GET_U_1(tptr);
	    tptr++;
	    tlen--;
	    if (num_system_ids == 0) {
		/* Not valid */
		ND_PRINT(" No system IDs supplied");
	    } else {
		if (tlen < SYSTEM_ID_LEN)
		    goto tlv_trunc;
		ND_TCHECK_LEN(tptr, SYSTEM_ID_LEN);
		ND_PRINT("\n\t      Purge Originator System-ID: %s",
		       isis_print_id(ndo, tptr, SYSTEM_ID_LEN));
		tptr += SYSTEM_ID_LEN;
		tlen -= SYSTEM_ID_LEN;

		if (num_system_ids > 1) {
		    if (tlen < SYSTEM_ID_LEN)
			goto tlv_trunc;
		    ND_TCHECK_LEN(tptr, SYSTEM_ID_LEN);
		    ND_TCHECK_LEN(tptr, 2 * SYSTEM_ID_LEN + 1);
		    ND_PRINT("\n\t      Received from System-ID: %s",
			   isis_print_id(ndo, tptr, SYSTEM_ID_LEN));
		}
	    }
	    break;

	case ISIS_TLV_MT_SUPPORTED:
	    while (tlen != 0) {
		/* length can only be a multiple of 2, otherwise there is
		   something broken -> so decode down until length is 1 */
		if (tlen!=1) {
                    mt_len = isis_print_mtid(ndo, tptr, "\n\t      ", tlen);
                    if (mt_len == 0) /* did something go wrong ? */
                        goto trunc;
                    tptr+=mt_len;
                    tlen-=mt_len;
		} else {
		    ND_PRINT("\n\t      invalid MT-ID");
		    break;
		}
	    }
	    break;

	case ISIS_TLV_RESTART_SIGNALING:
            /* first attempt to decode the flags */
            if (tlen < ISIS_TLV_RESTART_SIGNALING_FLAGLEN)
                break;
            ND_TCHECK_LEN(tptr, ISIS_TLV_RESTART_SIGNALING_FLAGLEN);
            ND_PRINT("\n\t      Flags [%s]",
                   bittok2str(isis_restart_flag_values, "none", GET_U_1(tptr)));
            tptr+=ISIS_TLV_RESTART_SIGNALING_FLAGLEN;
            tlen-=ISIS_TLV_RESTART_SIGNALING_FLAGLEN;

            /* is there anything other than the flags field? */
            if (tlen == 0)
                break;

            if (tlen < ISIS_TLV_RESTART_SIGNALING_HOLDTIMELEN)
                break;
            ND_TCHECK_LEN(tptr, ISIS_TLV_RESTART_SIGNALING_HOLDTIMELEN);

            ND_PRINT(", Remaining holding time %us", GET_BE_U_2(tptr));
            tptr+=ISIS_TLV_RESTART_SIGNALING_HOLDTIMELEN;
            tlen-=ISIS_TLV_RESTART_SIGNALING_HOLDTIMELEN;

            /* is there an additional sysid field present ?*/
            if (tlen == SYSTEM_ID_LEN) {
                    ND_TCHECK_LEN(tptr, SYSTEM_ID_LEN);
                    ND_PRINT(", for %s", isis_print_id(ndo, tptr,SYSTEM_ID_LEN));
            }
	    break;

        case ISIS_TLV_IDRP_INFO:
	    if (tlen < 1)
	        break;
            isis_subtlv_idrp = GET_U_1(tptr);
            ND_PRINT("\n\t      Inter-Domain Information Type: %s",
                   tok2str(isis_subtlv_idrp_values,
                           "Unknown (0x%02x)",
                           isis_subtlv_idrp));
            tptr++;
            tlen--;
            switch (isis_subtlv_idrp) {
            case ISIS_SUBTLV_IDRP_ASN:
                if (tlen < 2)
                    goto tlv_trunc;
                ND_PRINT("AS Number: %u", GET_BE_U_2(tptr));
                break;
            case ISIS_SUBTLV_IDRP_LOCAL:
            case ISIS_SUBTLV_IDRP_RES:
            default:
                if (!print_unknown_data(ndo, tptr, "\n\t      ", tlen))
                    return(0);
                break;
            }
            break;

        case ISIS_TLV_LSP_BUFFERSIZE:
	    if (tlen < 2)
	        break;
            ND_PRINT("\n\t      LSP Buffersize: %u", GET_BE_U_2(tptr));
            break;

        case ISIS_TLV_PART_DIS:
            while (tlen != 0) {
                if (tlen < SYSTEM_ID_LEN)
                    goto tlv_trunc;
                ND_TCHECK_LEN(tptr, SYSTEM_ID_LEN);
                ND_PRINT("\n\t      %s", isis_print_id(ndo, tptr, SYSTEM_ID_LEN));
                tptr+=SYSTEM_ID_LEN;
                tlen-=SYSTEM_ID_LEN;
            }
            break;

        case ISIS_TLV_PREFIX_NEIGH:
	    if (tlen < sizeof(struct isis_metric_block))
	        break;
            ND_TCHECK_LEN(tptr, sizeof(struct isis_metric_block));
            ND_PRINT("\n\t      Metric Block");
            isis_print_metric_block(ndo, (const struct isis_metric_block *)tptr);
            tptr+=sizeof(struct isis_metric_block);
            tlen-=sizeof(struct isis_metric_block);

            while (tlen != 0) {
                prefix_len=GET_U_1(tptr); /* read out prefix length in semioctets*/
                tptr++;
                tlen--;
                if (prefix_len < 2) {
                    ND_PRINT("\n\t\tAddress: prefix length %u < 2", prefix_len);
                    break;
                }
                if (tlen < prefix_len/2)
                    break;
                ND_PRINT("\n\t\tAddress: %s/%u",
                       GET_ISONSAP_STRING(tptr, prefix_len / 2), prefix_len * 4);
                tptr+=prefix_len/2;
                tlen-=prefix_len/2;
            }
            break;

        case ISIS_TLV_IIH_SEQNR:
	    if (tlen < 4)
	        break;
            ND_PRINT("\n\t      Sequence number: %u", GET_BE_U_4(tptr));
            break;

        case ISIS_TLV_ROUTER_CAPABILITY:
            if (tlen < 5) {
                ND_PRINT(" [object length %u < 5]", tlen);
                nd_print_invalid(ndo);
                break;
            }
            ND_PRINT("\n\t      Router-ID %s", GET_IPADDR_STRING(tptr));
            ND_PRINT(", Flags [%s]",
		     bittok2str(isis_tlv_router_capability_flags, "none", GET_U_1(tptr+4)));

	    /* Optional set of sub-TLV */
	    if (tlen > 5) {
		isis_print_router_cap_subtlv(ndo, tptr+5, tlen-5);
	    }
            break;

        case ISIS_TLV_VENDOR_PRIVATE:
	    if (tlen < 3)
	        break;
            vendor_id = GET_BE_U_3(tptr);
            ND_PRINT("\n\t      Vendor: %s (%u)",
                   tok2str(oui_values, "Unknown", vendor_id),
                   vendor_id);
            tptr+=3;
            tlen-=3;
            if (tlen != 0) /* hexdump the rest */
                if (!print_unknown_data(ndo, tptr, "\n\t\t", tlen))
                    return(0);
            break;
            /*
             * FIXME those are the defined TLVs that lack a decoder
             * you are welcome to contribute code ;-)
             */

        case ISIS_TLV_DECNET_PHASE4:
        case ISIS_TLV_LUCENT_PRIVATE:
        case ISIS_TLV_IPAUTH:
        case ISIS_TLV_NORTEL_PRIVATE1:
        case ISIS_TLV_NORTEL_PRIVATE2:

	default:
		if (ndo->ndo_vflag <= 1) {
			if (!print_unknown_data(ndo, pptr, "\n\t\t", tlv_len))
				return(0);
		}
		break;
	}
tlv_trunc:
        /* do we want to see an additionally hexdump ? */
	if (ndo->ndo_vflag> 1) {
		if (!print_unknown_data(ndo, pptr, "\n\t      ", tlv_len))
			return(0);
	}

	pptr += tlv_len;
	packet_len -= tlv_len;
    }

    if (packet_len != 0) {
	ND_PRINT("\n\t      %u straggler bytes", packet_len);
    }
    return (1);

trunc:
    nd_print_trunc(ndo);
    return (1);
}

static void
osi_print_cksum(netdissect_options *ndo, const uint8_t *pptr,
	        uint16_t checksum, int checksum_offset, u_int length)
{
        uint16_t calculated_checksum;

        /* do not attempt to verify the checksum if it is zero,
         * if the offset is nonsense,
         * or the base pointer is not sane
         */
        if (!checksum
            || checksum_offset < 0
            || !ND_TTEST_2(pptr + checksum_offset)
            || (u_int)checksum_offset > length
            || !ND_TTEST_LEN(pptr, length)) {
                ND_PRINT(" (unverified)");
        } else {
#if 0
                ND_PRINT("\nosi_print_cksum: %p %d %u\n", pptr, checksum_offset, length);
#endif
                calculated_checksum = create_osi_cksum(pptr, checksum_offset, length);
                if (checksum == calculated_checksum) {
                        ND_PRINT(" (correct)");
                } else {
                        ND_PRINT(" (incorrect should be 0x%04x)", calculated_checksum);
                }
        }
}
