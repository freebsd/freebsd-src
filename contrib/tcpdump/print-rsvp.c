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
 * Original code by Hannes Gredler (hannes@gredler.at)
 */

/* \summary: Resource ReSerVation Protocol (RSVP) printer */

/* specification: RFC 2205 */

#include <config.h>

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "gmpls.h"
#include "af.h"
#include "signature.h"


/*
 * RFC 2205 common header
 *
 *               0             1              2             3
 *        +-------------+-------------+-------------+-------------+
 *        | Vers | Flags|  Msg Type   |       RSVP Checksum       |
 *        +-------------+-------------+-------------+-------------+
 *        |  Send_TTL   | (Reserved)  |        RSVP Length        |
 *        +-------------+-------------+-------------+-------------+
 *
 */

struct rsvp_common_header {
    nd_uint8_t  version_flags;
    nd_uint8_t  msg_type;
    nd_uint16_t checksum;
    nd_uint8_t  ttl;
    nd_byte     reserved[1];
    nd_uint16_t length;
};

/*
 * RFC2205 object header
 *
 *
 *               0             1              2             3
 *        +-------------+-------------+-------------+-------------+
 *        |       Length (bytes)      |  Class-Num  |   C-Type    |
 *        +-------------+-------------+-------------+-------------+
 *        |                                                       |
 *        //                  (Object contents)                   //
 *        |                                                       |
 *        +-------------+-------------+-------------+-------------+
 */

struct rsvp_object_header {
    nd_uint16_t length;
    nd_uint8_t  class_num;
    nd_uint8_t  ctype;
};

#define RSVP_VERSION            1
#define	RSVP_EXTRACT_VERSION(x) (((x)&0xf0)>>4)
#define	RSVP_EXTRACT_FLAGS(x)   ((x)&0x0f)

#define	RSVP_MSGTYPE_PATH       1
#define	RSVP_MSGTYPE_RESV       2
#define	RSVP_MSGTYPE_PATHERR    3
#define	RSVP_MSGTYPE_RESVERR    4
#define	RSVP_MSGTYPE_PATHTEAR   5
#define	RSVP_MSGTYPE_RESVTEAR   6
#define	RSVP_MSGTYPE_RESVCONF   7
#define RSVP_MSGTYPE_BUNDLE     12
#define RSVP_MSGTYPE_ACK        13
#define RSVP_MSGTYPE_HELLO_OLD  14      /* ancient Hellos */
#define RSVP_MSGTYPE_SREFRESH   15
#define	RSVP_MSGTYPE_HELLO      20

static const struct tok rsvp_msg_type_values[] = {
    { RSVP_MSGTYPE_PATH,	"Path" },
    { RSVP_MSGTYPE_RESV,	"Resv" },
    { RSVP_MSGTYPE_PATHERR,	"PathErr" },
    { RSVP_MSGTYPE_RESVERR,	"ResvErr" },
    { RSVP_MSGTYPE_PATHTEAR,	"PathTear" },
    { RSVP_MSGTYPE_RESVTEAR,	"ResvTear" },
    { RSVP_MSGTYPE_RESVCONF,	"ResvConf" },
    { RSVP_MSGTYPE_BUNDLE,	"Bundle" },
    { RSVP_MSGTYPE_ACK,	        "Acknowledgement" },
    { RSVP_MSGTYPE_HELLO_OLD,	"Hello (Old)" },
    { RSVP_MSGTYPE_SREFRESH,	"Refresh" },
    { RSVP_MSGTYPE_HELLO,	"Hello" },
    { 0, NULL}
};

static const struct tok rsvp_header_flag_values[] = {
    { 0x01,	              "Refresh reduction capable" }, /* rfc2961 */
    { 0, NULL}
};

static const struct tok rsvp_obj_capability_flag_values[] = {
    { 0x0004,                "RecoveryPath Transmit Enabled" },
    { 0x0002,                "RecoveryPath Desired" },
    { 0x0001,                "RecoveryPath Srefresh Capable" },
    { 0, NULL}
};

#define	RSVP_OBJ_SESSION            1   /* rfc2205 */
#define	RSVP_OBJ_RSVP_HOP           3   /* rfc2205, rfc3473 */
#define	RSVP_OBJ_INTEGRITY          4   /* rfc2747 */
#define	RSVP_OBJ_TIME_VALUES        5   /* rfc2205 */
#define	RSVP_OBJ_ERROR_SPEC         6
#define	RSVP_OBJ_SCOPE              7
#define	RSVP_OBJ_STYLE              8   /* rfc2205 */
#define	RSVP_OBJ_FLOWSPEC           9   /* rfc2215 */
#define	RSVP_OBJ_FILTERSPEC         10  /* rfc2215 */
#define	RSVP_OBJ_SENDER_TEMPLATE    11
#define	RSVP_OBJ_SENDER_TSPEC       12  /* rfc2215 */
#define	RSVP_OBJ_ADSPEC             13  /* rfc2215 */
#define	RSVP_OBJ_POLICY_DATA        14
#define	RSVP_OBJ_CONFIRM            15  /* rfc2205 */
#define	RSVP_OBJ_LABEL              16  /* rfc3209 */
#define	RSVP_OBJ_LABEL_REQ          19  /* rfc3209 */
#define	RSVP_OBJ_ERO                20  /* rfc3209 */
#define	RSVP_OBJ_RRO                21  /* rfc3209 */
#define	RSVP_OBJ_HELLO              22  /* rfc3209 */
#define	RSVP_OBJ_MESSAGE_ID         23  /* rfc2961 */
#define	RSVP_OBJ_MESSAGE_ID_ACK     24  /* rfc2961 */
#define	RSVP_OBJ_MESSAGE_ID_LIST    25  /* rfc2961 */
#define	RSVP_OBJ_RECOVERY_LABEL     34  /* rfc3473 */
#define	RSVP_OBJ_UPSTREAM_LABEL     35  /* rfc3473 */
#define	RSVP_OBJ_LABEL_SET          36  /* rfc3473 */
#define	RSVP_OBJ_PROTECTION         37  /* rfc3473 */
#define RSVP_OBJ_S2L                50  /* rfc4875 */
#define	RSVP_OBJ_DETOUR             63  /* rfc4090 */
#define	RSVP_OBJ_CLASSTYPE          66  /* rfc4124 */
#define RSVP_OBJ_CLASSTYPE_OLD      125 /* draft-ietf-tewg-diff-te-proto-07 */
#define	RSVP_OBJ_SUGGESTED_LABEL    129 /* rfc3473 */
#define	RSVP_OBJ_ACCEPT_LABEL_SET   130 /* rfc3473 */
#define	RSVP_OBJ_RESTART_CAPABILITY 131 /* rfc3473 */
#define RSVP_OBJ_CAPABILITY         134 /* rfc5063 */
#define	RSVP_OBJ_NOTIFY_REQ         195 /* rfc3473 */
#define	RSVP_OBJ_ADMIN_STATUS       196 /* rfc3473 */
#define	RSVP_OBJ_PROPERTIES         204 /* juniper proprietary */
#define	RSVP_OBJ_FASTREROUTE        205 /* rfc4090 */
#define	RSVP_OBJ_SESSION_ATTRIBUTE  207 /* rfc3209 */
#define RSVP_OBJ_GENERALIZED_UNI    229 /* OIF RSVP extensions UNI 1.0 Signaling, Rel. 2 */
#define RSVP_OBJ_CALL_ID            230 /* rfc3474 */
#define RSVP_OBJ_CALL_OPS           236 /* rfc3474 */

static const struct tok rsvp_obj_values[] = {
    { RSVP_OBJ_SESSION,            "Session" },
    { RSVP_OBJ_RSVP_HOP,           "RSVP Hop" },
    { RSVP_OBJ_INTEGRITY,          "Integrity" },
    { RSVP_OBJ_TIME_VALUES,        "Time Values" },
    { RSVP_OBJ_ERROR_SPEC,         "Error Spec" },
    { RSVP_OBJ_SCOPE,              "Scope" },
    { RSVP_OBJ_STYLE,              "Style" },
    { RSVP_OBJ_FLOWSPEC,           "Flowspec" },
    { RSVP_OBJ_FILTERSPEC,         "FilterSpec" },
    { RSVP_OBJ_SENDER_TEMPLATE,    "Sender Template" },
    { RSVP_OBJ_SENDER_TSPEC,       "Sender TSpec" },
    { RSVP_OBJ_ADSPEC,             "Adspec" },
    { RSVP_OBJ_POLICY_DATA,        "Policy Data" },
    { RSVP_OBJ_CONFIRM,            "Confirm" },
    { RSVP_OBJ_LABEL,              "Label" },
    { RSVP_OBJ_LABEL_REQ,          "Label Request" },
    { RSVP_OBJ_ERO,                "ERO" },
    { RSVP_OBJ_RRO,                "RRO" },
    { RSVP_OBJ_HELLO,              "Hello" },
    { RSVP_OBJ_MESSAGE_ID,         "Message ID" },
    { RSVP_OBJ_MESSAGE_ID_ACK,     "Message ID Ack" },
    { RSVP_OBJ_MESSAGE_ID_LIST,    "Message ID List" },
    { RSVP_OBJ_RECOVERY_LABEL,     "Recovery Label" },
    { RSVP_OBJ_UPSTREAM_LABEL,     "Upstream Label" },
    { RSVP_OBJ_LABEL_SET,          "Label Set" },
    { RSVP_OBJ_ACCEPT_LABEL_SET,   "Acceptable Label Set" },
    { RSVP_OBJ_DETOUR,             "Detour" },
    { RSVP_OBJ_CLASSTYPE,          "Class Type" },
    { RSVP_OBJ_CLASSTYPE_OLD,      "Class Type (old)" },
    { RSVP_OBJ_SUGGESTED_LABEL,    "Suggested Label" },
    { RSVP_OBJ_PROPERTIES,         "Properties" },
    { RSVP_OBJ_FASTREROUTE,        "Fast Re-Route" },
    { RSVP_OBJ_SESSION_ATTRIBUTE,  "Session Attribute" },
    { RSVP_OBJ_GENERALIZED_UNI,    "Generalized UNI" },
    { RSVP_OBJ_CALL_ID,            "Call-ID" },
    { RSVP_OBJ_CALL_OPS,           "Call Capability" },
    { RSVP_OBJ_RESTART_CAPABILITY, "Restart Capability" },
    { RSVP_OBJ_CAPABILITY,         "Capability" },
    { RSVP_OBJ_NOTIFY_REQ,         "Notify Request" },
    { RSVP_OBJ_PROTECTION,         "Protection" },
    { RSVP_OBJ_ADMIN_STATUS,       "Administrative Status" },
    { RSVP_OBJ_S2L,                "Sub-LSP to LSP" },
    { 0, NULL}
};

#define	RSVP_CTYPE_IPV4        1
#define	RSVP_CTYPE_IPV6        2
#define	RSVP_CTYPE_TUNNEL_IPV4 7
#define	RSVP_CTYPE_TUNNEL_IPV6 8
#define	RSVP_CTYPE_UNI_IPV4    11 /* OIF RSVP extensions UNI 1.0 Signaling Rel. 2 */
#define RSVP_CTYPE_1           1
#define RSVP_CTYPE_2           2
#define RSVP_CTYPE_3           3
#define RSVP_CTYPE_4           4
#define RSVP_CTYPE_12         12
#define RSVP_CTYPE_13         13
#define RSVP_CTYPE_14         14

/*
 * the ctypes are not globally unique so for
 * translating it to strings we build a table based
 * on objects offsetted by the ctype
 */

static const struct tok rsvp_ctype_values[] = {
    { 256*RSVP_OBJ_RSVP_HOP+RSVP_CTYPE_IPV4,	             "IPv4" },
    { 256*RSVP_OBJ_RSVP_HOP+RSVP_CTYPE_IPV6,	             "IPv6" },
    { 256*RSVP_OBJ_RSVP_HOP+RSVP_CTYPE_3,	             "IPv4 plus opt. TLVs" },
    { 256*RSVP_OBJ_RSVP_HOP+RSVP_CTYPE_4,	             "IPv6 plus opt. TLVs" },
    { 256*RSVP_OBJ_NOTIFY_REQ+RSVP_CTYPE_IPV4,	             "IPv4" },
    { 256*RSVP_OBJ_NOTIFY_REQ+RSVP_CTYPE_IPV6,	             "IPv6" },
    { 256*RSVP_OBJ_CONFIRM+RSVP_CTYPE_IPV4,	             "IPv4" },
    { 256*RSVP_OBJ_CONFIRM+RSVP_CTYPE_IPV6,	             "IPv6" },
    { 256*RSVP_OBJ_TIME_VALUES+RSVP_CTYPE_1,	             "1" },
    { 256*RSVP_OBJ_FLOWSPEC+RSVP_CTYPE_1,	             "obsolete" },
    { 256*RSVP_OBJ_FLOWSPEC+RSVP_CTYPE_2,	             "IntServ" },
    { 256*RSVP_OBJ_SENDER_TSPEC+RSVP_CTYPE_2,	             "IntServ" },
    { 256*RSVP_OBJ_ADSPEC+RSVP_CTYPE_2,	                     "IntServ" },
    { 256*RSVP_OBJ_FILTERSPEC+RSVP_CTYPE_IPV4,	             "IPv4" },
    { 256*RSVP_OBJ_FILTERSPEC+RSVP_CTYPE_IPV6,	             "IPv6" },
    { 256*RSVP_OBJ_FILTERSPEC+RSVP_CTYPE_3,	             "IPv6 Flow-label" },
    { 256*RSVP_OBJ_FILTERSPEC+RSVP_CTYPE_TUNNEL_IPV4,	     "Tunnel IPv4" },
    { 256*RSVP_OBJ_FILTERSPEC+RSVP_CTYPE_12,                 "IPv4 P2MP LSP Tunnel" },
    { 256*RSVP_OBJ_FILTERSPEC+RSVP_CTYPE_13,                 "IPv6 P2MP LSP Tunnel" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_IPV4,	             "IPv4" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_IPV6,	             "IPv6" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_TUNNEL_IPV4,           "Tunnel IPv4" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_UNI_IPV4,              "UNI IPv4" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_13,                    "IPv4 P2MP LSP Tunnel" },
    { 256*RSVP_OBJ_SESSION+RSVP_CTYPE_14,                    "IPv6 P2MP LSP Tunnel" },
    { 256*RSVP_OBJ_SENDER_TEMPLATE+RSVP_CTYPE_IPV4,          "IPv4" },
    { 256*RSVP_OBJ_SENDER_TEMPLATE+RSVP_CTYPE_IPV6,          "IPv6" },
    { 256*RSVP_OBJ_SENDER_TEMPLATE+RSVP_CTYPE_TUNNEL_IPV4,   "Tunnel IPv4" },
    { 256*RSVP_OBJ_SENDER_TEMPLATE+RSVP_CTYPE_12,            "IPv4 P2MP LSP Tunnel" },
    { 256*RSVP_OBJ_SENDER_TEMPLATE+RSVP_CTYPE_13,            "IPv6 P2MP LSP Tunnel" },
    { 256*RSVP_OBJ_MESSAGE_ID+RSVP_CTYPE_1,                  "1" },
    { 256*RSVP_OBJ_MESSAGE_ID_ACK+RSVP_CTYPE_1,              "Message id ack" },
    { 256*RSVP_OBJ_MESSAGE_ID_ACK+RSVP_CTYPE_2,              "Message id nack" },
    { 256*RSVP_OBJ_MESSAGE_ID_LIST+RSVP_CTYPE_1,             "1" },
    { 256*RSVP_OBJ_STYLE+RSVP_CTYPE_1,                       "1" },
    { 256*RSVP_OBJ_HELLO+RSVP_CTYPE_1,                       "Hello Request" },
    { 256*RSVP_OBJ_HELLO+RSVP_CTYPE_2,                       "Hello Ack" },
    { 256*RSVP_OBJ_LABEL_REQ+RSVP_CTYPE_1,	             "without label range" },
    { 256*RSVP_OBJ_LABEL_REQ+RSVP_CTYPE_2,	             "with ATM label range" },
    { 256*RSVP_OBJ_LABEL_REQ+RSVP_CTYPE_3,                   "with FR label range" },
    { 256*RSVP_OBJ_LABEL_REQ+RSVP_CTYPE_4,                   "Generalized Label" },
    { 256*RSVP_OBJ_LABEL+RSVP_CTYPE_1,                       "Label" },
    { 256*RSVP_OBJ_LABEL+RSVP_CTYPE_2,                       "Generalized Label" },
    { 256*RSVP_OBJ_LABEL+RSVP_CTYPE_3,                       "Waveband Switching" },
    { 256*RSVP_OBJ_SUGGESTED_LABEL+RSVP_CTYPE_1,             "Label" },
    { 256*RSVP_OBJ_SUGGESTED_LABEL+RSVP_CTYPE_2,             "Generalized Label" },
    { 256*RSVP_OBJ_SUGGESTED_LABEL+RSVP_CTYPE_3,             "Waveband Switching" },
    { 256*RSVP_OBJ_UPSTREAM_LABEL+RSVP_CTYPE_1,              "Label" },
    { 256*RSVP_OBJ_UPSTREAM_LABEL+RSVP_CTYPE_2,              "Generalized Label" },
    { 256*RSVP_OBJ_UPSTREAM_LABEL+RSVP_CTYPE_3,              "Waveband Switching" },
    { 256*RSVP_OBJ_RECOVERY_LABEL+RSVP_CTYPE_1,              "Label" },
    { 256*RSVP_OBJ_RECOVERY_LABEL+RSVP_CTYPE_2,              "Generalized Label" },
    { 256*RSVP_OBJ_RECOVERY_LABEL+RSVP_CTYPE_3,              "Waveband Switching" },
    { 256*RSVP_OBJ_ERO+RSVP_CTYPE_IPV4,                      "IPv4" },
    { 256*RSVP_OBJ_RRO+RSVP_CTYPE_IPV4,                      "IPv4" },
    { 256*RSVP_OBJ_ERROR_SPEC+RSVP_CTYPE_IPV4,               "IPv4" },
    { 256*RSVP_OBJ_ERROR_SPEC+RSVP_CTYPE_IPV6,               "IPv6" },
    { 256*RSVP_OBJ_ERROR_SPEC+RSVP_CTYPE_3,                  "IPv4 plus opt. TLVs" },
    { 256*RSVP_OBJ_ERROR_SPEC+RSVP_CTYPE_4,                  "IPv6 plus opt. TLVs" },
    { 256*RSVP_OBJ_RESTART_CAPABILITY+RSVP_CTYPE_1,          "IPv4" },
    { 256*RSVP_OBJ_CAPABILITY+RSVP_CTYPE_1,                  "1" },
    { 256*RSVP_OBJ_SESSION_ATTRIBUTE+RSVP_CTYPE_TUNNEL_IPV4, "Tunnel IPv4" },
    { 256*RSVP_OBJ_FASTREROUTE+RSVP_CTYPE_TUNNEL_IPV4,       "Tunnel IPv4" }, /* old style*/
    { 256*RSVP_OBJ_FASTREROUTE+RSVP_CTYPE_1,                 "1" }, /* new style */
    { 256*RSVP_OBJ_DETOUR+RSVP_CTYPE_TUNNEL_IPV4,            "Tunnel IPv4" },
    { 256*RSVP_OBJ_PROPERTIES+RSVP_CTYPE_1,                  "1" },
    { 256*RSVP_OBJ_ADMIN_STATUS+RSVP_CTYPE_1,                "1" },
    { 256*RSVP_OBJ_CLASSTYPE+RSVP_CTYPE_1,                   "1" },
    { 256*RSVP_OBJ_CLASSTYPE_OLD+RSVP_CTYPE_1,               "1" },
    { 256*RSVP_OBJ_LABEL_SET+RSVP_CTYPE_1,                   "1" },
    { 256*RSVP_OBJ_GENERALIZED_UNI+RSVP_CTYPE_1,             "1" },
    { 256*RSVP_OBJ_S2L+RSVP_CTYPE_IPV4,                      "IPv4 sub-LSP" },
    { 256*RSVP_OBJ_S2L+RSVP_CTYPE_IPV6,                      "IPv6 sub-LSP" },
    { 0, NULL}
};

/*
 * XXX - this assumes a 16-byte digest, which is true for HMAC-MD5, but
 * isn't necessarily the case for other hash algorithms.
 *
 * Unless I've missed something, there's nothing in RFC 2747 to indicate
 * the hash algorithm being used, so it's presumably something set up
 * out-of-band, or negotiated by other RSVP objects.
 */
struct rsvp_obj_integrity_t {
    uint8_t flags;
    uint8_t res;
    uint8_t key_id[6];
    uint8_t sequence[8];
    uint8_t digest[16];
};

static const struct tok rsvp_obj_integrity_flag_values[] = {
    { 0x80, "Handshake" },
    { 0, NULL}
};

struct rsvp_obj_frr_t {
    uint8_t setup_prio;
    uint8_t hold_prio;
    uint8_t hop_limit;
    uint8_t flags;
    uint8_t bandwidth[4];
    uint8_t include_any[4];
    uint8_t exclude_any[4];
    uint8_t include_all[4];
};


#define RSVP_OBJ_XRO_MASK_SUBOBJ(x)   ((x)&0x7f)
#define RSVP_OBJ_XRO_MASK_LOOSE(x)    ((x)&0x80)

#define RSVP_OBJ_CAPABILITY_FLAGS_MASK  0x7U

#define	RSVP_OBJ_XRO_RES       0
#define	RSVP_OBJ_XRO_IPV4      1
#define	RSVP_OBJ_XRO_IPV6      2
#define	RSVP_OBJ_XRO_LABEL     3
#define	RSVP_OBJ_XRO_ASN       32
#define	RSVP_OBJ_XRO_MPLS      64

static const struct tok rsvp_obj_xro_values[] = {
    { RSVP_OBJ_XRO_RES,	      "Reserved" },
    { RSVP_OBJ_XRO_IPV4,      "IPv4 prefix" },
    { RSVP_OBJ_XRO_IPV6,      "IPv6 prefix" },
    { RSVP_OBJ_XRO_LABEL,     "Label" },
    { RSVP_OBJ_XRO_ASN,       "Autonomous system number" },
    { RSVP_OBJ_XRO_MPLS,      "MPLS label switched path termination" },
    { 0, NULL}
};

/* RFC4090 */
static const struct tok rsvp_obj_rro_flag_values[] = {
    { 0x01,	              "Local protection available" },
    { 0x02,                   "Local protection in use" },
    { 0x04,                   "Bandwidth protection" },
    { 0x08,                   "Node protection" },
    { 0, NULL}
};

/* RFC3209 */
static const struct tok rsvp_obj_rro_label_flag_values[] = {
    { 0x01,                   "Global" },
    { 0, NULL}
};

static const struct tok rsvp_resstyle_values[] = {
    { 17,	              "Wildcard Filter" },
    { 10,                     "Fixed Filter" },
    { 18,                     "Shared Explicit" },
    { 0, NULL}
};

#define RSVP_OBJ_INTSERV_GUARANTEED_SERV 2
#define RSVP_OBJ_INTSERV_CONTROLLED_LOAD 5

static const struct tok rsvp_intserv_service_type_values[] = {
    { 1,                                "Default/Global Information" },
    { RSVP_OBJ_INTSERV_GUARANTEED_SERV, "Guaranteed Service" },
    { RSVP_OBJ_INTSERV_CONTROLLED_LOAD,	"Controlled Load" },
    { 0, NULL}
};

static const struct tok rsvp_intserv_parameter_id_values[] = {
    { 4,                     "IS hop cnt" },
    { 6,                     "Path b/w estimate" },
    { 8,                     "Minimum path latency" },
    { 10,                    "Composed MTU" },
    { 127,                   "Token Bucket TSpec" },
    { 130,                   "Guaranteed Service RSpec" },
    { 133,                   "End-to-end composed value for C" },
    { 134,                   "End-to-end composed value for D" },
    { 135,                   "Since-last-reshaping point composed C" },
    { 136,                   "Since-last-reshaping point composed D" },
    { 0, NULL}
};

static const struct tok rsvp_session_attribute_flag_values[] = {
    { 0x01,	              "Local Protection" },
    { 0x02,                   "Label Recording" },
    { 0x04,                   "SE Style" },
    { 0x08,                   "Bandwidth protection" }, /* RFC4090 */
    { 0x10,                   "Node protection" },      /* RFC4090 */
    { 0, NULL}
};

static const struct tok rsvp_obj_prop_tlv_values[] = {
    { 0x01,                   "Cos" },
    { 0x02,                   "Metric 1" },
    { 0x04,                   "Metric 2" },
    { 0x08,                   "CCC Status" },
    { 0x10,                   "Path Type" },
    { 0, NULL}
};

#define RSVP_OBJ_ERROR_SPEC_CODE_ROUTING 24
#define RSVP_OBJ_ERROR_SPEC_CODE_NOTIFY  25
#define RSVP_OBJ_ERROR_SPEC_CODE_DIFFSERV_TE 28
#define RSVP_OBJ_ERROR_SPEC_CODE_DIFFSERV_TE_OLD 125

static const struct tok rsvp_obj_error_code_values[] = {
    { RSVP_OBJ_ERROR_SPEC_CODE_ROUTING, "Routing Problem" },
    { RSVP_OBJ_ERROR_SPEC_CODE_NOTIFY,  "Notify Error" },
    { RSVP_OBJ_ERROR_SPEC_CODE_DIFFSERV_TE, "Diffserv TE Error" },
    { RSVP_OBJ_ERROR_SPEC_CODE_DIFFSERV_TE_OLD, "Diffserv TE Error (Old)" },
    { 0, NULL}
};

static const struct tok rsvp_obj_error_code_routing_values[] = {
    { 1,                      "Bad EXPLICIT_ROUTE object" },
    { 2,                      "Bad strict node" },
    { 3,                      "Bad loose node" },
    { 4,                      "Bad initial subobject" },
    { 5,                      "No route available toward destination" },
    { 6,                      "Unacceptable label value" },
    { 7,                      "RRO indicated routing loops" },
    { 8,                      "non-RSVP-capable router in the path" },
    { 9,                      "MPLS label allocation failure" },
    { 10,                     "Unsupported L3PID" },
    { 0, NULL}
};

static const struct tok rsvp_obj_error_code_diffserv_te_values[] = {
    { 1,                      "Unexpected CT object" },
    { 2,                      "Unsupported CT" },
    { 3,                      "Invalid CT value" },
    { 4,                      "CT/setup priority do not form a configured TE-Class" },
    { 5,                      "CT/holding priority do not form a configured TE-Class" },
    { 6,                      "CT/setup priority and CT/holding priority do not form a configured TE-Class" },
    { 7,                      "Inconsistency between signaled PSC and signaled CT" },
    { 8,                      "Inconsistency between signaled PHBs and signaled CT" },
   { 0, NULL}
};

/* rfc3473 / rfc 3471 */
static const struct tok rsvp_obj_admin_status_flag_values[] = {
    { 0x80000000, "Reflect" },
    { 0x00000004, "Testing" },
    { 0x00000002, "Admin-down" },
    { 0x00000001, "Delete-in-progress" },
    { 0, NULL}
};

/* label set actions - rfc3471 */
#define LABEL_SET_INCLUSIVE_LIST  0
#define LABEL_SET_EXCLUSIVE_LIST  1
#define LABEL_SET_INCLUSIVE_RANGE 2
#define LABEL_SET_EXCLUSIVE_RANGE 3

static const struct tok rsvp_obj_label_set_action_values[] = {
    { LABEL_SET_INCLUSIVE_LIST, "Inclusive list" },
    { LABEL_SET_EXCLUSIVE_LIST, "Exclusive list" },
    { LABEL_SET_INCLUSIVE_RANGE, "Inclusive range" },
    { LABEL_SET_EXCLUSIVE_RANGE, "Exclusive range" },
    { 0, NULL}
};

/* OIF RSVP extensions UNI 1.0 Signaling, release 2 */
#define RSVP_GEN_UNI_SUBOBJ_SOURCE_TNA_ADDRESS	    1
#define RSVP_GEN_UNI_SUBOBJ_DESTINATION_TNA_ADDRESS 2
#define RSVP_GEN_UNI_SUBOBJ_DIVERSITY		    3
#define RSVP_GEN_UNI_SUBOBJ_EGRESS_LABEL            4
#define RSVP_GEN_UNI_SUBOBJ_SERVICE_LEVEL           5

static const struct tok rsvp_obj_generalized_uni_values[] = {
    { RSVP_GEN_UNI_SUBOBJ_SOURCE_TNA_ADDRESS, "Source TNA address" },
    { RSVP_GEN_UNI_SUBOBJ_DESTINATION_TNA_ADDRESS, "Destination TNA address" },
    { RSVP_GEN_UNI_SUBOBJ_DIVERSITY, "Diversity" },
    { RSVP_GEN_UNI_SUBOBJ_EGRESS_LABEL, "Egress label" },
    { RSVP_GEN_UNI_SUBOBJ_SERVICE_LEVEL, "Service level" },
    { 0, NULL}
};

/*
 * this is a dissector for all the intserv defined
 * specs as defined per rfc2215
 * it is called from various rsvp objects;
 * returns the amount of bytes being processed
 */
static u_int
rsvp_intserv_print(netdissect_options *ndo,
                   const u_char *tptr, u_int obj_tlen)
{
    u_int parameter_id,parameter_length;
    union {
	float f;
	uint32_t i;
    } bw;

    if (obj_tlen < 4)
        return 0;
    parameter_id = GET_U_1(tptr);
    parameter_length = GET_BE_U_2(tptr + 2)<<2; /* convert wordcount to bytecount */

    ND_PRINT("\n\t      Parameter ID: %s (%u), length: %u, Flags: [0x%02x]",
           tok2str(rsvp_intserv_parameter_id_values,"unknown",parameter_id),
           parameter_id,
           parameter_length,
           GET_U_1(tptr + 1));

    if (obj_tlen < parameter_length+4)
        return 0;
    switch(parameter_id) { /* parameter_id */

    case 4:
       /*
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |    4 (e)      |    (f)        |           1 (g)               |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |        IS hop cnt (32-bit unsigned integer)                   |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        if (parameter_length == 4) {
            ND_PRINT("\n\t\tIS hop count: %u", GET_BE_U_4(tptr + 4));
        }
        break;

    case 6:
       /*
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |    6 (h)      |    (i)        |           1 (j)               |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Path b/w estimate  (32-bit IEEE floating point number)       |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        if (parameter_length == 4) {
            bw.i = GET_BE_U_4(tptr + 4);
            ND_PRINT("\n\t\tPath b/w estimate: %.10g Mbps", bw.f / 125000);
        }
        break;

    case 8:
       /*
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |     8 (k)     |    (l)        |           1 (m)               |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |        Minimum path latency (32-bit integer)                  |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        if (parameter_length == 4) {
            ND_PRINT("\n\t\tMinimum path latency: ");
            if (GET_BE_U_4(tptr + 4) == 0xffffffff)
                ND_PRINT("don't care");
            else
                ND_PRINT("%u", GET_BE_U_4(tptr + 4));
        }
        break;

    case 10:

       /*
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |     10 (n)    |      (o)      |           1 (p)               |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |      Composed MTU (32-bit unsigned integer)                   |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */
        if (parameter_length == 4) {
            ND_PRINT("\n\t\tComposed MTU: %u bytes", GET_BE_U_4(tptr + 4));
        }
        break;
    case 127:
       /*
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |   127 (e)     |    0 (f)      |             5 (g)             |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Token Bucket Rate [r] (32-bit IEEE floating point number)    |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Token Bucket Size [b] (32-bit IEEE floating point number)    |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Peak Data Rate [p] (32-bit IEEE floating point number)       |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Minimum Policed Unit [m] (32-bit integer)                    |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Maximum Packet Size [M]  (32-bit integer)                    |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */

        if (parameter_length == 20) {
	    ND_TCHECK_LEN(tptr + 4, 20);
            bw.i = GET_BE_U_4(tptr + 4);
            ND_PRINT("\n\t\tToken Bucket Rate: %.10g Mbps", bw.f / 125000);
            bw.i = GET_BE_U_4(tptr + 8);
            ND_PRINT("\n\t\tToken Bucket Size: %.10g bytes", bw.f);
            bw.i = GET_BE_U_4(tptr + 12);
            ND_PRINT("\n\t\tPeak Data Rate: %.10g Mbps", bw.f / 125000);
            ND_PRINT("\n\t\tMinimum Policed Unit: %u bytes",
                     GET_BE_U_4(tptr + 16));
            ND_PRINT("\n\t\tMaximum Packet Size: %u bytes",
                     GET_BE_U_4(tptr + 20));
        }
        break;

    case 130:
       /*
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |     130 (h)   |    0 (i)      |            2 (j)              |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Rate [R]  (32-bit IEEE floating point number)                |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        * |  Slack Term [S]  (32-bit integer)                             |
        * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        */

        if (parameter_length == 8) {
	    ND_TCHECK_8(tptr + 4);
            bw.i = GET_BE_U_4(tptr + 4);
            ND_PRINT("\n\t\tRate: %.10g Mbps", bw.f / 125000);
            ND_PRINT("\n\t\tSlack Term: %u", GET_BE_U_4(tptr + 8));
        }
        break;

    case 133:
    case 134:
    case 135:
    case 136:
        if (parameter_length == 4) {
            ND_PRINT("\n\t\tValue: %u", GET_BE_U_4(tptr + 4));
        }
        break;

    default:
        if (ndo->ndo_vflag <= 1)
            print_unknown_data(ndo, tptr + 4, "\n\t\t", parameter_length);
    }
    return (parameter_length+4); /* header length 4 bytes */

trunc:
    nd_print_trunc(ndo);
    return 0;
}

/*
 * Clear checksum prior to signature verification.
 */
static void
rsvp_clear_checksum(void *header)
{
    struct rsvp_common_header *rsvp_com_header = (struct rsvp_common_header *) header;

    rsvp_com_header->checksum[0] = 0;
    rsvp_com_header->checksum[1] = 0;
}

static int
rsvp_obj_print(netdissect_options *ndo,
               const u_char *pptr, u_int plen, const u_char *tptr,
               const char *indent, u_int tlen,
               const struct rsvp_common_header *rsvp_com_header)
{
    const struct rsvp_object_header *rsvp_obj_header;
    const u_char *obj_tptr;
    union {
        const struct rsvp_obj_integrity_t *rsvp_obj_integrity;
        const struct rsvp_obj_frr_t *rsvp_obj_frr;
    } obj_ptr;

    u_short rsvp_obj_len,rsvp_obj_ctype,rsvp_obj_class_num;
    u_int obj_tlen,intserv_serv_tlen;
    int hexdump;
    u_int processed,padbytes,error_code,error_value,i,sigcheck;
    union {
	float f;
	uint32_t i;
    } bw;
    u_int namelen;

    u_int action, subchannel;

    while(tlen>=sizeof(struct rsvp_object_header)) {
        /* did we capture enough for fully decoding the object header ? */
        ND_TCHECK_LEN(tptr, sizeof(struct rsvp_object_header));

        rsvp_obj_header = (const struct rsvp_object_header *)tptr;
        rsvp_obj_len=GET_BE_U_2(rsvp_obj_header->length);
        rsvp_obj_ctype=GET_U_1(rsvp_obj_header->ctype);

        if(rsvp_obj_len % 4) {
            ND_PRINT("%sERROR: object header size %u not a multiple of 4", indent, rsvp_obj_len);
            return -1;
        }
        if(rsvp_obj_len < sizeof(struct rsvp_object_header)) {
            ND_PRINT("%sERROR: object header too short %u < %zu", indent, rsvp_obj_len,
                   sizeof(struct rsvp_object_header));
            return -1;
        }

        rsvp_obj_class_num = GET_U_1(rsvp_obj_header->class_num);
        ND_PRINT("%s%s Object (%u) Flags: [%s",
               indent,
               tok2str(rsvp_obj_values,
                       "Unknown",
                       rsvp_obj_class_num),
               rsvp_obj_class_num,
               (rsvp_obj_class_num & 0x80) ?
                   ((rsvp_obj_class_num & 0x40) ? "ignore and forward" :
                                         "ignore silently") :
                   "reject");

        ND_PRINT(" if unknown], Class-Type: %s (%u), length: %u",
               tok2str(rsvp_ctype_values,
                       "Unknown",
                       (rsvp_obj_class_num<<8)+rsvp_obj_ctype),
               rsvp_obj_ctype,
               rsvp_obj_len);

        if(tlen < rsvp_obj_len) {
            ND_PRINT("%sERROR: object goes past end of objects TLV", indent);
            return -1;
        }

        obj_tptr=tptr+sizeof(struct rsvp_object_header);
        obj_tlen=rsvp_obj_len-sizeof(struct rsvp_object_header);

        /* did we capture enough for fully decoding the object ? */
        ND_TCHECK_LEN(tptr, rsvp_obj_len);
        hexdump=FALSE;

        switch(rsvp_obj_class_num) {
        case RSVP_OBJ_SESSION:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 DestAddress: %s, Protocol ID: 0x%02x",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_U_1(obj_tptr + sizeof(nd_ipv4)));
                ND_PRINT("%s  Flags: [0x%02x], DestPort %u",
                       indent,
                       GET_U_1((obj_tptr + 5)),
                       GET_BE_U_2(obj_tptr + 6));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < 20)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv6 DestAddress: %s, Protocol ID: 0x%02x",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_U_1(obj_tptr + sizeof(nd_ipv6)));
                ND_PRINT("%s  Flags: [0x%02x], DestPort %u",
                       indent,
                       GET_U_1((obj_tptr + sizeof(nd_ipv6) + 1)),
                       GET_BE_U_2(obj_tptr + sizeof(nd_ipv6) + 2));
                obj_tlen-=20;
                obj_tptr+=20;
                break;

            case RSVP_CTYPE_TUNNEL_IPV6:
                if (obj_tlen < 36)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv6 Tunnel EndPoint: %s, Tunnel ID: 0x%04x, Extended Tunnel ID: %s",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 18),
                       GET_IP6ADDR_STRING(obj_tptr + 20));
                obj_tlen-=36;
                obj_tptr+=36;
                break;

            case RSVP_CTYPE_14: /* IPv6 p2mp LSP Tunnel */
                if (obj_tlen < 26)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv6 P2MP LSP ID: 0x%08x, Tunnel ID: 0x%04x, Extended Tunnel ID: %s",
                       indent,
                       GET_BE_U_4(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6),
                       GET_IP6ADDR_STRING(obj_tptr + 8));
                obj_tlen-=26;
                obj_tptr+=26;
                break;
            case RSVP_CTYPE_13: /* IPv4 p2mp LSP Tunnel */
                if (obj_tlen < 12)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 P2MP LSP ID: %s, Tunnel ID: 0x%04x, Extended Tunnel ID: %s",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6),
                       GET_IPADDR_STRING(obj_tptr + 8));
                obj_tlen-=12;
                obj_tptr+=12;
                break;
            case RSVP_CTYPE_TUNNEL_IPV4:
            case RSVP_CTYPE_UNI_IPV4:
                if (obj_tlen < 12)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 Tunnel EndPoint: %s, Tunnel ID: 0x%04x, Extended Tunnel ID: %s",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6),
                       GET_IPADDR_STRING(obj_tptr + 8));
                obj_tlen-=12;
                obj_tptr+=12;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_CONFIRM:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < sizeof(nd_ipv4))
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 Receiver Address: %s",
                       indent,
                       GET_IPADDR_STRING(obj_tptr));
                obj_tlen-=sizeof(nd_ipv4);
                obj_tptr+=sizeof(nd_ipv4);
                break;
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < sizeof(nd_ipv6))
                    goto obj_tooshort;
                ND_PRINT("%s  IPv6 Receiver Address: %s",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr));
                obj_tlen-=sizeof(nd_ipv6);
                obj_tptr+=sizeof(nd_ipv6);
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_NOTIFY_REQ:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < sizeof(nd_ipv4))
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 Notify Node Address: %s",
                       indent,
                       GET_IPADDR_STRING(obj_tptr));
                obj_tlen-=sizeof(nd_ipv4);
                obj_tptr+=sizeof(nd_ipv4);
                break;
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < sizeof(nd_ipv6))
                    goto obj_tooshort;
                ND_PRINT("%s  IPv6 Notify Node Address: %s",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr));
                obj_tlen-=sizeof(nd_ipv6);
                obj_tptr+=sizeof(nd_ipv6);
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_SUGGESTED_LABEL: /* fall through */
        case RSVP_OBJ_UPSTREAM_LABEL:  /* fall through */
        case RSVP_OBJ_RECOVERY_LABEL:  /* fall through */
        case RSVP_OBJ_LABEL:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                while(obj_tlen >= 4 ) {
                    ND_PRINT("%s  Label: %u", indent, GET_BE_U_4(obj_tptr));
                    obj_tlen-=4;
                    obj_tptr+=4;
                }
                break;
            case RSVP_CTYPE_2:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  Generalized Label: %u",
                       indent,
                       GET_BE_U_4(obj_tptr));
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            case RSVP_CTYPE_3:
                if (obj_tlen < 12)
                    goto obj_tooshort;
                ND_PRINT("%s  Waveband ID: %u%s  Start Label: %u, Stop Label: %u",
                       indent,
                       GET_BE_U_4(obj_tptr),
                       indent,
                       GET_BE_U_4(obj_tptr + 4),
                       GET_BE_U_4(obj_tptr + 8));
                obj_tlen-=12;
                obj_tptr+=12;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_STYLE:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  Reservation Style: %s, Flags: [0x%02x]",
                       indent,
                       tok2str(rsvp_resstyle_values,
                               "Unknown",
                               GET_BE_U_3(obj_tptr + 1)),
                       GET_U_1(obj_tptr));
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_SENDER_TEMPLATE:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Address: %s, Source Port: %u",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < 20)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Address: %s, Source Port: %u",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 18));
                obj_tlen-=20;
                obj_tptr+=20;
                break;
            case RSVP_CTYPE_13: /* IPv6 p2mp LSP tunnel */
                if (obj_tlen < 40)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv6 Tunnel Sender Address: %s, LSP ID: 0x%04x"
                       "%s  Sub-Group Originator ID: %s, Sub-Group ID: 0x%04x",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 18),
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr+20),
                       GET_BE_U_2(obj_tptr + 38));
                obj_tlen-=40;
                obj_tptr+=40;
                break;
            case RSVP_CTYPE_TUNNEL_IPV4:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 Tunnel Sender Address: %s, LSP-ID: 0x%04x",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            case RSVP_CTYPE_12: /* IPv4 p2mp LSP tunnel */
                if (obj_tlen < 16)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 Tunnel Sender Address: %s, LSP ID: 0x%04x"
                       "%s  Sub-Group Originator ID: %s, Sub-Group ID: 0x%04x",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6),
                       indent,
                       GET_IPADDR_STRING(obj_tptr+8),
                       GET_BE_U_2(obj_tptr + 12));
                obj_tlen-=16;
                obj_tptr+=16;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_LABEL_REQ:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                while(obj_tlen >= 4 ) {
                    ND_PRINT("%s  L3 Protocol ID: %s",
                           indent,
                           tok2str(ethertype_values,
                                   "Unknown Protocol (0x%04x)",
                                   GET_BE_U_2(obj_tptr + 2)));
                    obj_tlen-=4;
                    obj_tptr+=4;
                }
                break;
            case RSVP_CTYPE_2:
                if (obj_tlen < 12)
                    goto obj_tooshort;
                ND_PRINT("%s  L3 Protocol ID: %s",
                       indent,
                       tok2str(ethertype_values,
                               "Unknown Protocol (0x%04x)",
                               GET_BE_U_2(obj_tptr + 2)));
                ND_PRINT(",%s merge capability",
                         ((GET_U_1(obj_tptr + 4)) & 0x80) ? "no" : "" );
                ND_PRINT("%s  Minimum VPI/VCI: %u/%u",
                       indent,
                       (GET_BE_U_2(obj_tptr + 4))&0xfff,
                       (GET_BE_U_2(obj_tptr + 6)) & 0xfff);
                ND_PRINT("%s  Maximum VPI/VCI: %u/%u",
                       indent,
                       (GET_BE_U_2(obj_tptr + 8))&0xfff,
                       (GET_BE_U_2(obj_tptr + 10)) & 0xfff);
                obj_tlen-=12;
                obj_tptr+=12;
                break;
            case RSVP_CTYPE_3:
                if (obj_tlen < 12)
                    goto obj_tooshort;
                ND_PRINT("%s  L3 Protocol ID: %s",
                       indent,
                       tok2str(ethertype_values,
                               "Unknown Protocol (0x%04x)",
                               GET_BE_U_2(obj_tptr + 2)));
                ND_PRINT("%s  Minimum/Maximum DLCI: %u/%u, %s%s bit DLCI",
                       indent,
                       (GET_BE_U_4(obj_tptr + 4))&0x7fffff,
                       (GET_BE_U_4(obj_tptr + 8))&0x7fffff,
                       (((GET_BE_U_2(obj_tptr + 4)>>7)&3) == 0 ) ? "10" : "",
                       (((GET_BE_U_2(obj_tptr + 4) >> 7) & 3) == 2 ) ? "23" : "");
                obj_tlen-=12;
                obj_tptr+=12;
                break;
            case RSVP_CTYPE_4:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  LSP Encoding Type: %s (%u)",
                       indent,
                       tok2str(gmpls_encoding_values,
                               "Unknown",
                               GET_U_1(obj_tptr)),
                       GET_U_1(obj_tptr));
                ND_PRINT("%s  Switching Type: %s (%u), Payload ID: %s (0x%04x)",
                       indent,
                       tok2str(gmpls_switch_cap_values,
                               "Unknown",
                               GET_U_1((obj_tptr + 1))),
                       GET_U_1(obj_tptr + 1),
                       tok2str(gmpls_payload_values,
                               "Unknown",
                               GET_BE_U_2(obj_tptr + 2)),
                       GET_BE_U_2(obj_tptr + 2));
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_RRO:
        case RSVP_OBJ_ERO:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                while(obj_tlen >= 4 ) {
		    u_char length;

		    ND_TCHECK_4(obj_tptr);
		    length = GET_U_1(obj_tptr + 1);
                    ND_PRINT("%s  Subobject Type: %s, length %u",
                           indent,
                           tok2str(rsvp_obj_xro_values,
                                   "Unknown %u",
                                   RSVP_OBJ_XRO_MASK_SUBOBJ(GET_U_1(obj_tptr))),
                           length);
                    if (obj_tlen < length) {
                        ND_PRINT("%s  ERROR: ERO subobject length > object length", indent);
                        break;
                    }

                    if (length == 0) { /* prevent infinite loops */
                        ND_PRINT("%s  ERROR: zero length ERO subtype", indent);
                        break;
                    }

                    switch(RSVP_OBJ_XRO_MASK_SUBOBJ(GET_U_1(obj_tptr))) {
		    u_char prefix_length;

                    case RSVP_OBJ_XRO_IPV4:
			if (length != 8) {
				ND_PRINT(" ERROR: length != 8");
				goto invalid;
			}
			ND_TCHECK_8(obj_tptr);
			prefix_length = GET_U_1(obj_tptr + 6);
			if (prefix_length != 32) {
				ND_PRINT(" ERROR: Prefix length %u != 32",
					  prefix_length);
				goto invalid;
			}
                        ND_PRINT(", %s, %s/%u, Flags: [%s]",
                               RSVP_OBJ_XRO_MASK_LOOSE(GET_U_1(obj_tptr)) ? "Loose" : "Strict",
                               GET_IPADDR_STRING(obj_tptr+2),
                               GET_U_1((obj_tptr + 6)),
                               bittok2str(rsvp_obj_rro_flag_values,
                                   "none",
                                   GET_U_1((obj_tptr + 7)))); /* rfc3209 says that this field is rsvd. */
                    break;
                    case RSVP_OBJ_XRO_LABEL:
			if (length != 8) {
				ND_PRINT(" ERROR: length != 8");
				goto invalid;
			}
			ND_TCHECK_8(obj_tptr);
                        ND_PRINT(", Flags: [%s] (%#x), Class-Type: %s (%u), %u",
                               bittok2str(rsvp_obj_rro_label_flag_values,
                                   "none",
                                   GET_U_1((obj_tptr + 2))),
                               GET_U_1(obj_tptr + 2),
                               tok2str(rsvp_ctype_values,
                                       "Unknown",
                                       GET_U_1((obj_tptr + 3)) + (256 * RSVP_OBJ_RRO)),
                               GET_U_1((obj_tptr + 3)),
                               GET_BE_U_4(obj_tptr + 4));
                    }
                    obj_tlen-=length;
                    obj_tptr+=length;
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_HELLO:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
            case RSVP_CTYPE_2:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Instance: 0x%08x, Destination Instance: 0x%08x",
                       indent,
                       GET_BE_U_4(obj_tptr),
                       GET_BE_U_4(obj_tptr + 4));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_RESTART_CAPABILITY:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  Restart  Time: %ums, Recovery Time: %ums",
                       indent,
                       GET_BE_U_4(obj_tptr),
                       GET_BE_U_4(obj_tptr + 4));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_CAPABILITY:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                uint32_t unused_and_flags = GET_BE_U_4(obj_tptr);
                if (unused_and_flags & ~RSVP_OBJ_CAPABILITY_FLAGS_MASK)
                    ND_PRINT("%s  [reserved=0x%08x must be zero]", indent,
                        unused_and_flags & ~RSVP_OBJ_CAPABILITY_FLAGS_MASK);
                ND_PRINT("%s  Flags: [%s]",
                       indent,
                       bittok2str(rsvp_obj_capability_flag_values,
                                  "none",
                                  (unused_and_flags & RSVP_OBJ_CAPABILITY_FLAGS_MASK)));
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_SESSION_ATTRIBUTE:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_TUNNEL_IPV4:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                namelen = GET_U_1(obj_tptr + 3);
                if (obj_tlen < 4+namelen)
                    goto obj_tooshort;
                ND_PRINT("%s  Session Name: ", indent);
                for (i = 0; i < namelen; i++)
                    fn_print_char(ndo, GET_U_1(obj_tptr + 4 + i));
                ND_PRINT("%s  Setup Priority: %u, Holding Priority: %u, Flags: [%s] (%#x)",
                       indent,
                       GET_U_1(obj_tptr),
                       GET_U_1(obj_tptr + 1),
                       bittok2str(rsvp_session_attribute_flag_values,
                                  "none",
                                  GET_U_1((obj_tptr + 2))),
                       GET_U_1(obj_tptr + 2));
                obj_tlen-=4+namelen;
                obj_tptr+=4+namelen;
                break;
            default:
                hexdump=TRUE;
            }
            break;

	case RSVP_OBJ_GENERALIZED_UNI:
            switch(rsvp_obj_ctype) {
		u_int subobj_type,af,subobj_len,total_subobj_len;

            case RSVP_CTYPE_1:

                if (obj_tlen < 4)
                    goto obj_tooshort;

		/* read variable length subobjects */
		total_subobj_len = obj_tlen;
                while(total_subobj_len > 0) {
                    /* If RFC 3476 Section 3.1 defined that a sub-object of the
                     * GENERALIZED_UNI RSVP object must have the Length field as
                     * a multiple of 4, instead of the check below it would be
                     * better to test total_subobj_len only once before the loop.
                     * So long as it does not define it and this while loop does
                     * not implement such a requirement, let's accept that within
                     * each iteration subobj_len may happen to be a multiple of 1
                     * and test it and total_subobj_len respectively.
                     */
                    if (total_subobj_len < 4)
                        goto invalid;
                    subobj_len  = GET_BE_U_2(obj_tptr);
                    subobj_type = (GET_BE_U_2(obj_tptr + 2))>>8;
                    af = (GET_BE_U_2(obj_tptr + 2))&0x00FF;

                    ND_PRINT("%s  Subobject Type: %s (%u), AF: %s (%u), length: %u",
                           indent,
                           tok2str(rsvp_obj_generalized_uni_values, "Unknown", subobj_type),
                           subobj_type,
                           tok2str(af_values, "Unknown", af), af,
                           subobj_len);

                    /* In addition to what is explained above, the same spec does not
                     * explicitly say that the same Length field includes the 4-octet
                     * sub-object header, but as long as this while loop implements it
                     * as it does include, let's keep the check below consistent with
                     * the rest of the code.
                     *
                     * XXX - RFC 3476 Section 3.1 says "The contents of these
                     * sub-objects are described in [8]", where [8] is
                     * UNI 1.0 Signaling Specification, The Optical
                     * Internetworking Forum.  The URL they give for that
                     * document is
                     *
                     *    http://www.oiforum.com/public/UNI_1.0_ia.html
                     *
                     * but that doesn't work; the new URL appears to be
                     *
                     *    https://web.archive.org/web/20160401194747/http://www.oiforum.com/public/documents/OIF-UNI-01.0.pdf
                     *
                     * and *that* document, in section 12.5.2.3
                     * "GENERALIZED_UNI Object (Class-Num=11bbbbbb (TBA))",
                     * says nothing about the length field in general, but
                     * some of the examples it gives in subsections have
                     * length field values that clearly includes the length
                     * of the sub-object header as well as the length of the
                     * value.
                     */
                    if(subobj_len < 4 || subobj_len > total_subobj_len ||
                       obj_tlen < subobj_len)
                        goto invalid;

                    switch(subobj_type) {
                    case RSVP_GEN_UNI_SUBOBJ_SOURCE_TNA_ADDRESS:
                    case RSVP_GEN_UNI_SUBOBJ_DESTINATION_TNA_ADDRESS:

                        switch(af) {
                        case AFNUM_INET:
                            if (subobj_len < 8)
                                goto subobj_tooshort;
                            ND_PRINT("%s    UNI IPv4 TNA address: %s",
                                   indent, GET_IPADDR_STRING(obj_tptr + 4));
                            break;
                        case AFNUM_INET6:
                            if (subobj_len < 20)
                                goto subobj_tooshort;
                            ND_PRINT("%s    UNI IPv6 TNA address: %s",
                                   indent, GET_IP6ADDR_STRING(obj_tptr + 4));
                            break;
                        case AFNUM_NSAP:
                            if (subobj_len) {
                                /* unless we have a TLV parser lets just hexdump */
                                hexdump=TRUE;
                            }
                            break;
                        }
                        break;

                    case RSVP_GEN_UNI_SUBOBJ_DIVERSITY:
                        if (subobj_len > 4) {
                            /* unless we have a TLV parser lets just hexdump */
                            hexdump=TRUE;
                        }
                        break;

                    case RSVP_GEN_UNI_SUBOBJ_EGRESS_LABEL:
                        if (subobj_len < 16) {
                            goto subobj_tooshort;
                        }

                        ND_PRINT("%s    U-bit: %x, Label type: %u, Logical port id: %u, Label: %u",
                               indent,
                               ((GET_BE_U_4(obj_tptr + 4))>>31),
                               ((GET_BE_U_4(obj_tptr + 4))&0xFF),
                               GET_BE_U_4(obj_tptr + 8),
                               GET_BE_U_4(obj_tptr + 12));
                        break;

                    case RSVP_GEN_UNI_SUBOBJ_SERVICE_LEVEL:
                        if (subobj_len < 8) {
                            goto subobj_tooshort;
                        }

                        ND_PRINT("%s    Service level: %u",
                               indent, (GET_BE_U_4(obj_tptr + 4)) >> 24);
                        break;

                    default:
                        hexdump=TRUE;
                        break;
                    }
                    total_subobj_len-=subobj_len;
                    obj_tptr+=subobj_len;
                    obj_tlen+=subobj_len;
		}
                break;

            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_RSVP_HOP:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_3: /* fall through - FIXME add TLV parser */
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  Previous/Next Interface: %s, Logical Interface Handle: 0x%08x",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_4(obj_tptr + 4));
                obj_tlen-=8;
                obj_tptr+=8;
                if (obj_tlen)
                    hexdump=TRUE; /* unless we have a TLV parser lets just hexdump */
                break;
            case RSVP_CTYPE_4: /* fall through - FIXME add TLV parser */
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < 20)
                    goto obj_tooshort;
                ND_PRINT("%s  Previous/Next Interface: %s, Logical Interface Handle: 0x%08x",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_BE_U_4(obj_tptr + 16));
                obj_tlen-=20;
                obj_tptr+=20;
                hexdump=TRUE; /* unless we have a TLV parser lets just hexdump */
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_TIME_VALUES:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  Refresh Period: %ums",
                       indent,
                       GET_BE_U_4(obj_tptr));
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        /* those three objects do share the same semantics */
        case RSVP_OBJ_SENDER_TSPEC:
        case RSVP_OBJ_ADSPEC:
        case RSVP_OBJ_FLOWSPEC:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_2:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  Msg-Version: %u, length: %u",
                       indent,
                       (GET_U_1(obj_tptr) & 0xf0) >> 4,
                       GET_BE_U_2(obj_tptr + 2) << 2);
                obj_tptr+=4; /* get to the start of the service header */
                obj_tlen-=4;

                while (obj_tlen >= 4) {
                    intserv_serv_tlen=GET_BE_U_2(obj_tptr + 2)<<2;
                    ND_PRINT("%s  Service Type: %s (%u), break bit %sset, Service length: %u",
                           indent,
                           tok2str(rsvp_intserv_service_type_values,"unknown",GET_U_1((obj_tptr))),
                           GET_U_1(obj_tptr),
                           (GET_U_1(obj_tptr + 1)&0x80) ? "" : "not ",
                           intserv_serv_tlen);

                    obj_tptr+=4; /* get to the start of the parameter list */
                    obj_tlen-=4;

                    while (intserv_serv_tlen>=4) {
                        processed = rsvp_intserv_print(ndo, obj_tptr, obj_tlen);
                        if (processed == 0)
                            break;
                        obj_tlen-=processed;
                        intserv_serv_tlen-=processed;
                        obj_tptr+=processed;
                    }
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_FILTERSPEC:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Address: %s, Source Port: %u",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < 20)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Address: %s, Source Port: %u",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 18));
                obj_tlen-=20;
                obj_tptr+=20;
                break;
            case RSVP_CTYPE_3:
                if (obj_tlen < 20)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Address: %s, Flow Label: %u",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_BE_U_3(obj_tptr + 17));
                obj_tlen-=20;
                obj_tptr+=20;
                break;
            case RSVP_CTYPE_TUNNEL_IPV6:
                if (obj_tlen < 20)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Address: %s, LSP-ID: 0x%04x",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 18));
                obj_tlen-=20;
                obj_tptr+=20;
                break;
            case RSVP_CTYPE_13: /* IPv6 p2mp LSP tunnel */
                if (obj_tlen < 40)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv6 Tunnel Sender Address: %s, LSP ID: 0x%04x"
                       "%s  Sub-Group Originator ID: %s, Sub-Group ID: 0x%04x",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 18),
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr+20),
                       GET_BE_U_2(obj_tptr + 38));
                obj_tlen-=40;
                obj_tptr+=40;
                break;
            case RSVP_CTYPE_TUNNEL_IPV4:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                ND_PRINT("%s  Source Address: %s, LSP-ID: 0x%04x",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6));
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            case RSVP_CTYPE_12: /* IPv4 p2mp LSP tunnel */
                if (obj_tlen < 16)
                    goto obj_tooshort;
                ND_PRINT("%s  IPv4 Tunnel Sender Address: %s, LSP ID: 0x%04x"
                       "%s  Sub-Group Originator ID: %s, Sub-Group ID: 0x%04x",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_BE_U_2(obj_tptr + 6),
                       indent,
                       GET_IPADDR_STRING(obj_tptr+8),
                       GET_BE_U_2(obj_tptr + 12));
                obj_tlen-=16;
                obj_tptr+=16;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_FASTREROUTE:
            /* the differences between c-type 1 and 7 are minor */
            obj_ptr.rsvp_obj_frr = (const struct rsvp_obj_frr_t *)obj_tptr;

            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1: /* new style */
                if (obj_tlen < sizeof(struct rsvp_obj_frr_t))
                    goto obj_tooshort;
                bw.i = GET_BE_U_4(obj_ptr.rsvp_obj_frr->bandwidth);
                ND_PRINT("%s  Setup Priority: %u, Holding Priority: %u, Hop-limit: %u, Bandwidth: %.10g Mbps",
                       indent,
                       obj_ptr.rsvp_obj_frr->setup_prio,
                       obj_ptr.rsvp_obj_frr->hold_prio,
                       obj_ptr.rsvp_obj_frr->hop_limit,
                       bw.f * 8 / 1000000);
                ND_PRINT("%s  Include-any: 0x%08x, Exclude-any: 0x%08x, Include-all: 0x%08x",
                       indent,
                       GET_BE_U_4(obj_ptr.rsvp_obj_frr->include_any),
                       GET_BE_U_4(obj_ptr.rsvp_obj_frr->exclude_any),
                       GET_BE_U_4(obj_ptr.rsvp_obj_frr->include_all));
                obj_tlen-=sizeof(struct rsvp_obj_frr_t);
                obj_tptr+=sizeof(struct rsvp_obj_frr_t);
                break;

            case RSVP_CTYPE_TUNNEL_IPV4: /* old style */
                if (obj_tlen < 16)
                    goto obj_tooshort;
                bw.i = GET_BE_U_4(obj_ptr.rsvp_obj_frr->bandwidth);
                ND_PRINT("%s  Setup Priority: %u, Holding Priority: %u, Hop-limit: %u, Bandwidth: %.10g Mbps",
                       indent,
                       obj_ptr.rsvp_obj_frr->setup_prio,
                       obj_ptr.rsvp_obj_frr->hold_prio,
                       obj_ptr.rsvp_obj_frr->hop_limit,
                       bw.f * 8 / 1000000);
                ND_PRINT("%s  Include Colors: 0x%08x, Exclude Colors: 0x%08x",
                       indent,
                       GET_BE_U_4(obj_ptr.rsvp_obj_frr->include_any),
                       GET_BE_U_4(obj_ptr.rsvp_obj_frr->exclude_any));
                obj_tlen-=16;
                obj_tptr+=16;
                break;

            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_DETOUR:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_TUNNEL_IPV4:
                while(obj_tlen >= 8) {
                    ND_PRINT("%s  PLR-ID: %s, Avoid-Node-ID: %s",
                           indent,
                           GET_IPADDR_STRING(obj_tptr),
                           GET_IPADDR_STRING(obj_tptr + 4));
                    obj_tlen-=8;
                    obj_tptr+=8;
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_CLASSTYPE:
        case RSVP_OBJ_CLASSTYPE_OLD: /* fall through */
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  CT: %u",
                       indent,
                       GET_BE_U_4(obj_tptr) & 0x7);
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_ERROR_SPEC:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_3: /* fall through - FIXME add TLV parser */
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < 8)
                    goto obj_tooshort;
                error_code=GET_U_1(obj_tptr + 5);
                error_value=GET_BE_U_2(obj_tptr + 6);
                ND_PRINT("%s  Error Node Address: %s, Flags: [0x%02x]%s  Error Code: %s (%u)",
                       indent,
                       GET_IPADDR_STRING(obj_tptr),
                       GET_U_1(obj_tptr + 4),
                       indent,
                       tok2str(rsvp_obj_error_code_values,"unknown",error_code),
                       error_code);
                switch (error_code) {
                case RSVP_OBJ_ERROR_SPEC_CODE_ROUTING:
                    ND_PRINT(", Error Value: %s (%u)",
                           tok2str(rsvp_obj_error_code_routing_values,"unknown",error_value),
                           error_value);
                    break;
                case RSVP_OBJ_ERROR_SPEC_CODE_DIFFSERV_TE: /* fall through */
                case RSVP_OBJ_ERROR_SPEC_CODE_DIFFSERV_TE_OLD:
                    ND_PRINT(", Error Value: %s (%u)",
                           tok2str(rsvp_obj_error_code_diffserv_te_values,"unknown",error_value),
                           error_value);
                    break;
                default:
                    ND_PRINT(", Unknown Error Value (%u)", error_value);
                    break;
                }
                obj_tlen-=8;
                obj_tptr+=8;
                break;
            case RSVP_CTYPE_4: /* fall through - FIXME add TLV parser */
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < 20)
                    goto obj_tooshort;
                error_code=GET_U_1(obj_tptr + 17);
                error_value=GET_BE_U_2(obj_tptr + 18);
                ND_PRINT("%s  Error Node Address: %s, Flags: [0x%02x]%s  Error Code: %s (%u)",
                       indent,
                       GET_IP6ADDR_STRING(obj_tptr),
                       GET_U_1(obj_tptr + 16),
                       indent,
                       tok2str(rsvp_obj_error_code_values,"unknown",error_code),
                       error_code);

                switch (error_code) {
                case RSVP_OBJ_ERROR_SPEC_CODE_ROUTING:
                    ND_PRINT(", Error Value: %s (%u)",
                           tok2str(rsvp_obj_error_code_routing_values,"unknown",error_value),
			   error_value);
                    break;
                default:
                    break;
                }
                obj_tlen-=20;
                obj_tptr+=20;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_PROPERTIES:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                padbytes = GET_BE_U_2(obj_tptr + 2);
                ND_PRINT("%s  TLV count: %u, padding bytes: %u",
                       indent,
                       GET_BE_U_2(obj_tptr),
                       padbytes);
                obj_tlen-=4;
                obj_tptr+=4;
                /* loop through as long there is anything longer than the TLV header (2) */
                while(obj_tlen >= 2 + padbytes) {
                    ND_PRINT("%s    %s TLV (0x%02x), length: %u", /* length includes header */
                           indent,
                           tok2str(rsvp_obj_prop_tlv_values,"unknown",GET_U_1(obj_tptr)),
                           GET_U_1(obj_tptr),
                           GET_U_1(obj_tptr + 1));
                    if (obj_tlen < GET_U_1(obj_tptr + 1))
                        goto obj_tooshort;
                    if (GET_U_1(obj_tptr + 1) < 2) {
                        ND_PRINT("%sERROR: property TLV is too short", indent);
                        return -1;
                    }
                    print_unknown_data(ndo, obj_tptr + 2, "\n\t\t",
                                       GET_U_1(obj_tptr + 1) - 2);
                    obj_tlen-=GET_U_1(obj_tptr + 1);
                    obj_tptr+=GET_U_1(obj_tptr + 1);
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_MESSAGE_ID:     /* fall through */
        case RSVP_OBJ_MESSAGE_ID_ACK: /* fall through */
        case RSVP_OBJ_MESSAGE_ID_LIST:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
            case RSVP_CTYPE_2:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  Flags [0x%02x], epoch: %u",
                       indent,
                       GET_U_1(obj_tptr),
                       GET_BE_U_3(obj_tptr + 1));
                obj_tlen-=4;
                obj_tptr+=4;
                /* loop through as long there are no messages left */
                while(obj_tlen >= 4) {
                    ND_PRINT("%s    Message-ID 0x%08x (%u)",
                           indent,
                           GET_BE_U_4(obj_tptr),
                           GET_BE_U_4(obj_tptr));
                    obj_tlen-=4;
                    obj_tptr+=4;
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_INTEGRITY:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < sizeof(struct rsvp_obj_integrity_t))
                    goto obj_tooshort;
                obj_ptr.rsvp_obj_integrity = (const struct rsvp_obj_integrity_t *)obj_tptr;
                ND_PRINT("%s  Key-ID 0x%04x%08x, Sequence 0x%08x%08x, Flags [%s]",
                       indent,
                       GET_BE_U_2(obj_ptr.rsvp_obj_integrity->key_id),
                       GET_BE_U_4(obj_ptr.rsvp_obj_integrity->key_id + 2),
                       GET_BE_U_4(obj_ptr.rsvp_obj_integrity->sequence),
                       GET_BE_U_4(obj_ptr.rsvp_obj_integrity->sequence + 4),
                       bittok2str(rsvp_obj_integrity_flag_values,
                                  "none",
                                  obj_ptr.rsvp_obj_integrity->flags));
                ND_PRINT("%s  MD5-sum 0x%08x%08x%08x%08x ",
                       indent,
                       GET_BE_U_4(obj_ptr.rsvp_obj_integrity->digest),
                       GET_BE_U_4(obj_ptr.rsvp_obj_integrity->digest + 4),
                       GET_BE_U_4(obj_ptr.rsvp_obj_integrity->digest + 8),
                       GET_BE_U_4(obj_ptr.rsvp_obj_integrity->digest + 12));

                sigcheck = signature_verify(ndo, pptr, plen,
                                            obj_ptr.rsvp_obj_integrity->digest,
                                            rsvp_clear_checksum,
                                            rsvp_com_header);
                ND_PRINT(" (%s)", tok2str(signature_check_values, "Unknown", sigcheck));

                obj_tlen+=sizeof(struct rsvp_obj_integrity_t);
                obj_tptr+=sizeof(struct rsvp_obj_integrity_t);
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_ADMIN_STATUS:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  Flags [%s]", indent,
                       bittok2str(rsvp_obj_admin_status_flag_values, "none",
                                  GET_BE_U_4(obj_tptr)));
                obj_tlen-=4;
                obj_tptr+=4;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_LABEL_SET:
            switch(rsvp_obj_ctype) {
            case RSVP_CTYPE_1:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                action = (GET_BE_U_2(obj_tptr)>>8);

                ND_PRINT("%s  Action: %s (%u), Label type: %u", indent,
                       tok2str(rsvp_obj_label_set_action_values, "Unknown", action),
                       action, (GET_BE_U_4(obj_tptr) & 0x7F));

                switch (action) {
                case LABEL_SET_INCLUSIVE_RANGE:
                case LABEL_SET_EXCLUSIVE_RANGE: /* fall through */

		    /* only a couple of subchannels are expected */
		    if (obj_tlen < 12)
			goto obj_tooshort;
		    ND_PRINT("%s  Start range: %u, End range: %u", indent,
                           GET_BE_U_4(obj_tptr + 4),
                           GET_BE_U_4(obj_tptr + 8));
		    obj_tlen-=12;
		    obj_tptr+=12;
                    break;

                default:
                    obj_tlen-=4;
                    obj_tptr+=4;
                    subchannel = 1;
                    while(obj_tlen >= 4 ) {
                        ND_PRINT("%s  Subchannel #%u: %u", indent, subchannel,
                               GET_BE_U_4(obj_tptr));
                        obj_tptr+=4;
                        obj_tlen-=4;
                        subchannel++;
                    }
                    break;
                }
                break;
            default:
                hexdump=TRUE;
            }
            break;

        case RSVP_OBJ_S2L:
            switch (rsvp_obj_ctype) {
            case RSVP_CTYPE_IPV4:
                if (obj_tlen < 4)
                    goto obj_tooshort;
                ND_PRINT("%s  Sub-LSP destination address: %s",
                       indent, GET_IPADDR_STRING(obj_tptr));

                obj_tlen-=4;
                obj_tptr+=4;
                break;
            case RSVP_CTYPE_IPV6:
                if (obj_tlen < 16)
                    goto obj_tooshort;
                ND_PRINT("%s  Sub-LSP destination address: %s",
                       indent, GET_IP6ADDR_STRING(obj_tptr));

                obj_tlen-=16;
                obj_tptr+=16;
                break;
            default:
                hexdump=TRUE;
            }
            break;

        /*
         *  FIXME those are the defined objects that lack a decoder
         *  you are welcome to contribute code ;-)
         */

        case RSVP_OBJ_SCOPE:
        case RSVP_OBJ_POLICY_DATA:
        case RSVP_OBJ_ACCEPT_LABEL_SET:
        case RSVP_OBJ_PROTECTION:
        default:
            if (ndo->ndo_vflag <= 1)
                print_unknown_data(ndo, obj_tptr, "\n\t    ", obj_tlen); /* FIXME indentation */
            break;
        }
        /* do we also want to see a hex dump ? */
        if (ndo->ndo_vflag > 1 || hexdump == TRUE)
            print_unknown_data(ndo, tptr + sizeof(struct rsvp_object_header), "\n\t    ", /* FIXME indentation */
                               rsvp_obj_len - sizeof(struct rsvp_object_header));

        tptr+=rsvp_obj_len;
        tlen-=rsvp_obj_len;
    }
    return 0;
subobj_tooshort:
    ND_PRINT("%sERROR: sub-object is too short", indent);
    return -1;
obj_tooshort:
    ND_PRINT("%sERROR: object is too short", indent);
    return -1;
invalid:
    nd_print_invalid(ndo);
    return -1;
trunc:
    nd_print_trunc(ndo);
    return -1;
}

void
rsvp_print(netdissect_options *ndo,
           const u_char *pptr, u_int len)
{
    const struct rsvp_common_header *rsvp_com_header;
    uint8_t version_flags, msg_type;
    const u_char *tptr;
    u_short plen, tlen;

    ndo->ndo_protocol = "rsvp";
    tptr=pptr;

    rsvp_com_header = (const struct rsvp_common_header *)pptr;
    ND_TCHECK_SIZE(rsvp_com_header);
    version_flags = GET_U_1(rsvp_com_header->version_flags);

    /*
     * Sanity checking of the header.
     */
    if (RSVP_EXTRACT_VERSION(version_flags) != RSVP_VERSION) {
	ND_PRINT("ERROR: RSVP version %u packet not supported",
               RSVP_EXTRACT_VERSION(version_flags));
	return;
    }

    msg_type = GET_U_1(rsvp_com_header->msg_type);

    /* in non-verbose mode just lets print the basic Message Type*/
    if (ndo->ndo_vflag < 1) {
        ND_PRINT("RSVPv%u %s Message, length: %u",
               RSVP_EXTRACT_VERSION(version_flags),
               tok2str(rsvp_msg_type_values, "unknown (%u)",msg_type),
               len);
        return;
    }

    /* ok they seem to want to know everything - lets fully decode it */

    plen = tlen = GET_BE_U_2(rsvp_com_header->length);

    ND_PRINT("\n\tRSVPv%u %s Message (%u), Flags: [%s], length: %u, ttl: %u, checksum: 0x%04x",
           RSVP_EXTRACT_VERSION(version_flags),
           tok2str(rsvp_msg_type_values, "unknown, type: %u",msg_type),
           msg_type,
           bittok2str(rsvp_header_flag_values,"none",RSVP_EXTRACT_FLAGS(version_flags)),
           tlen,
           GET_U_1(rsvp_com_header->ttl),
           GET_BE_U_2(rsvp_com_header->checksum));

    if (tlen < sizeof(struct rsvp_common_header)) {
        ND_PRINT("ERROR: common header too short %u < %zu", tlen,
               sizeof(struct rsvp_common_header));
        return;
    }

    tptr+=sizeof(struct rsvp_common_header);
    tlen-=sizeof(struct rsvp_common_header);

    switch(msg_type) {

    case RSVP_MSGTYPE_BUNDLE:
        /*
         * Process each submessage in the bundle message.
         * Bundle messages may not contain bundle submessages, so we don't
         * need to handle bundle submessages specially.
         */
        while(tlen > 0) {
            const u_char *subpptr=tptr, *subtptr;
            u_short subplen, subtlen;

            subtptr=subpptr;

            rsvp_com_header = (const struct rsvp_common_header *)subpptr;
            ND_TCHECK_SIZE(rsvp_com_header);
            version_flags = GET_U_1(rsvp_com_header->version_flags);

            /*
             * Sanity checking of the header.
             */
            if (RSVP_EXTRACT_VERSION(version_flags) != RSVP_VERSION) {
                ND_PRINT("ERROR: RSVP version %u packet not supported",
                       RSVP_EXTRACT_VERSION(version_flags));
                return;
            }

            subplen = subtlen = GET_BE_U_2(rsvp_com_header->length);

            msg_type = GET_U_1(rsvp_com_header->msg_type);
            ND_PRINT("\n\t  RSVPv%u %s Message (%u), Flags: [%s], length: %u, ttl: %u, checksum: 0x%04x",
                   RSVP_EXTRACT_VERSION(version_flags),
                   tok2str(rsvp_msg_type_values, "unknown, type: %u",msg_type),
                   msg_type,
                   bittok2str(rsvp_header_flag_values,"none",RSVP_EXTRACT_FLAGS(version_flags)),
                   subtlen,
                   GET_U_1(rsvp_com_header->ttl),
                   GET_BE_U_2(rsvp_com_header->checksum));

            if (subtlen < sizeof(struct rsvp_common_header)) {
                ND_PRINT("ERROR: common header too short %u < %zu", subtlen,
                       sizeof(struct rsvp_common_header));
                return;
            }

            if (tlen < subtlen) {
                ND_PRINT("ERROR: common header too large %u > %u", subtlen,
                       tlen);
                return;
            }

            subtptr+=sizeof(struct rsvp_common_header);
            subtlen-=sizeof(struct rsvp_common_header);

            /*
             * Print all objects in the submessage.
             */
            if (rsvp_obj_print(ndo, subpptr, subplen, subtptr, "\n\t    ", subtlen, rsvp_com_header) == -1)
                return;

            tptr+=subtlen+sizeof(struct rsvp_common_header);
            tlen-=subtlen+sizeof(struct rsvp_common_header);
        }

        break;

    case RSVP_MSGTYPE_PATH:
    case RSVP_MSGTYPE_RESV:
    case RSVP_MSGTYPE_PATHERR:
    case RSVP_MSGTYPE_RESVERR:
    case RSVP_MSGTYPE_PATHTEAR:
    case RSVP_MSGTYPE_RESVTEAR:
    case RSVP_MSGTYPE_RESVCONF:
    case RSVP_MSGTYPE_HELLO_OLD:
    case RSVP_MSGTYPE_HELLO:
    case RSVP_MSGTYPE_ACK:
    case RSVP_MSGTYPE_SREFRESH:
        /*
         * Print all objects in the message.
         */
        if (rsvp_obj_print(ndo, pptr, plen, tptr, "\n\t  ", tlen, rsvp_com_header) == -1)
            return;
        break;

    default:
        print_unknown_data(ndo, tptr, "\n\t    ", tlen);
        break;
    }

    return;
trunc:
    nd_print_trunc(ndo);
}
