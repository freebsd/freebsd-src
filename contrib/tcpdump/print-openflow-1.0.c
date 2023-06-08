/*
 * This module implements decoding of OpenFlow protocol version 1.0 (wire
 * protocol 0x01). The decoder implements terse (default), detailed (-v) and
 * full (-vv) output formats and, as much as each format implies, detects and
 * tries to work around sizing anomalies inside the messages. The decoder marks
 * up bogus values of selected message fields and decodes partially captured
 * messages up to the snapshot end. It is based on the specification below:
 *
 * [OF10] https://www.opennetworking.org/wp-content/uploads/2013/04/openflow-spec-v1.0.0.pdf
 *
 * Most functions in this file take the following arguments:
 * * cp -- the pointer to the first octet to decode
 * * len -- the declared length of the structure to decode
 * The convention is that a printer function returns iff the given structure is
 * completely within the packet buffer; otherwise it processes the part that is
 * within the buffer, sooner of later takes the "truncated packet" shortcut via
 * longjmp() and never returns. With that in mind, the function may return
 * without printing the structure completely if it is invalid or the ndo_vflag
 * value is not high enough. This way the calling function can try to decode
 * the next data item.
 *
 * Decoding of Ethernet frames nested in OFPT_PACKET_IN and OFPT_PACKET_OUT
 * messages is done only when the verbosity level set by command-line argument
 * is "-vvv" or higher. In that case the verbosity level is temporarily
 * decremented by 3 during the nested frame decoding. For example, running
 * tcpdump with "-vvvv" will do full decoding of OpenFlow and "-v" decoding of
 * the nested frames.
 *
 * Partial decoding of Big Switch Networks vendor extensions is done after the
 * oftest (OpenFlow Testing Framework) and Loxigen (library generator) source
 * code.
 *
 *
 * Copyright (c) 2013 The TCPDUMP project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* \summary: OpenFlow protocol version 1.0 printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "ipproto.h"
#include "oui.h"
#include "openflow.h"


#define OFPT_HELLO                    0x00
#define OFPT_ERROR                    0x01
#define OFPT_ECHO_REQUEST             0x02
#define OFPT_ECHO_REPLY               0x03
#define OFPT_VENDOR                   0x04
#define OFPT_FEATURES_REQUEST         0x05
#define OFPT_FEATURES_REPLY           0x06
#define OFPT_GET_CONFIG_REQUEST       0x07
#define OFPT_GET_CONFIG_REPLY         0x08
#define OFPT_SET_CONFIG               0x09
#define OFPT_PACKET_IN                0x0a
#define OFPT_FLOW_REMOVED             0x0b
#define OFPT_PORT_STATUS              0x0c
#define OFPT_PACKET_OUT               0x0d
#define OFPT_FLOW_MOD                 0x0e
#define OFPT_PORT_MOD                 0x0f
#define OFPT_STATS_REQUEST            0x10
#define OFPT_STATS_REPLY              0x11
#define OFPT_BARRIER_REQUEST          0x12
#define OFPT_BARRIER_REPLY            0x13
#define OFPT_QUEUE_GET_CONFIG_REQUEST 0x14
#define OFPT_QUEUE_GET_CONFIG_REPLY   0x15
#define OFPT_MAX                      OFPT_QUEUE_GET_CONFIG_REPLY

#define OFPPC_PORT_DOWN    (1U <<0)
#define OFPPC_NO_STP       (1U <<1)
#define OFPPC_NO_RECV      (1U <<2)
#define OFPPC_NO_RECV_STP  (1U <<3)
#define OFPPC_NO_FLOOD     (1U <<4)
#define OFPPC_NO_FWD       (1U <<5)
#define OFPPC_NO_PACKET_IN (1U <<6)
static const struct tok ofppc_bm[] = {
	{ OFPPC_PORT_DOWN,    "PORT_DOWN"    },
	{ OFPPC_NO_STP,       "NO_STP"       },
	{ OFPPC_NO_RECV,      "NO_RECV"      },
	{ OFPPC_NO_RECV_STP,  "NO_RECV_STP"  },
	{ OFPPC_NO_FLOOD,     "NO_FLOOD"     },
	{ OFPPC_NO_FWD,       "NO_FWD"       },
	{ OFPPC_NO_PACKET_IN, "NO_PACKET_IN" },
	{ 0, NULL }
};
#define OFPPC_U (~(OFPPC_PORT_DOWN | OFPPC_NO_STP | OFPPC_NO_RECV | \
                   OFPPC_NO_RECV_STP | OFPPC_NO_FLOOD | OFPPC_NO_FWD | \
                   OFPPC_NO_PACKET_IN))

/*
 * [OF10] lists all FPPS_ constants in one enum, but they mean a 1-bit bitmap
 * in the least significant octet and a 2-bit code point in the next octet.
 * Remember to mix or to separate these two parts as the context requires.
 */
#define OFPPS_LINK_DOWN   (1U << 0) /* bitmap             */
#define OFPPS_STP_LISTEN  (0U << 8) /* code point         */
#define OFPPS_STP_LEARN   (1U << 8) /* code point         */
#define OFPPS_STP_FORWARD (2U << 8) /* code point         */
#define OFPPS_STP_BLOCK   (3U << 8) /* code point         */
#define OFPPS_STP_MASK    (3U << 8) /* code point bitmask */
static const struct tok ofpps_stp_str[] = {
	{ OFPPS_STP_LISTEN,  "STP_LISTEN"  },
	{ OFPPS_STP_LEARN,   "STP_LEARN"   },
	{ OFPPS_STP_FORWARD, "STP_FORWARD" },
	{ OFPPS_STP_BLOCK,   "STP_BLOCK"   },
	{ 0, NULL }
};
#define OFPPS_U (~(OFPPS_LINK_DOWN | OFPPS_STP_LISTEN | OFPPS_STP_LEARN | \
                   OFPPS_STP_FORWARD | OFPPS_STP_BLOCK))

#define OFPP_MAX        0xff00U
#define OFPP_IN_PORT    0xfff8U
#define OFPP_TABLE      0xfff9U
#define OFPP_NORMAL     0xfffaU
#define OFPP_FLOOD      0xfffbU
#define OFPP_ALL        0xfffcU
#define OFPP_CONTROLLER 0xfffdU
#define OFPP_LOCAL      0xfffeU
#define OFPP_NONE       0xffffU
static const struct tok ofpp_str[] = {
	{ OFPP_MAX,        "MAX"        },
	{ OFPP_IN_PORT,    "IN_PORT"    },
	{ OFPP_TABLE,      "TABLE"      },
	{ OFPP_NORMAL,     "NORMAL"     },
	{ OFPP_FLOOD,      "FLOOD"      },
	{ OFPP_ALL,        "ALL"        },
	{ OFPP_CONTROLLER, "CONTROLLER" },
	{ OFPP_LOCAL,      "LOCAL"      },
	{ OFPP_NONE,       "NONE"       },
	{ 0, NULL }
};

#define OFPPF_10MB_HD    (1U << 0)
#define OFPPF_10MB_FD    (1U << 1)
#define OFPPF_100MB_HD   (1U << 2)
#define OFPPF_100MB_FD   (1U << 3)
#define OFPPF_1GB_HD     (1U << 4)
#define OFPPF_1GB_FD     (1U << 5)
#define OFPPF_10GB_FD    (1U << 6)
#define OFPPF_COPPER     (1U << 7)
#define OFPPF_FIBER      (1U << 8)
#define OFPPF_AUTONEG    (1U << 9)
#define OFPPF_PAUSE      (1U <<10)
#define OFPPF_PAUSE_ASYM (1U <<11)
static const struct tok ofppf_bm[] = {
	{ OFPPF_10MB_HD,    "10MB_HD"    },
	{ OFPPF_10MB_FD,    "10MB_FD"    },
	{ OFPPF_100MB_HD,   "100MB_HD"   },
	{ OFPPF_100MB_FD,   "100MB_FD"   },
	{ OFPPF_1GB_HD,     "1GB_HD"     },
	{ OFPPF_1GB_FD,     "1GB_FD"     },
	{ OFPPF_10GB_FD,    "10GB_FD"    },
	{ OFPPF_COPPER,     "COPPER"     },
	{ OFPPF_FIBER,      "FIBER"      },
	{ OFPPF_AUTONEG,    "AUTONEG"    },
	{ OFPPF_PAUSE,      "PAUSE"      },
	{ OFPPF_PAUSE_ASYM, "PAUSE_ASYM" },
	{ 0, NULL }
};
#define OFPPF_U (~(OFPPF_10MB_HD | OFPPF_10MB_FD | OFPPF_100MB_HD | \
                   OFPPF_100MB_FD | OFPPF_1GB_HD | OFPPF_1GB_FD | \
                   OFPPF_10GB_FD | OFPPF_COPPER | OFPPF_FIBER | \
                   OFPPF_AUTONEG | OFPPF_PAUSE | OFPPF_PAUSE_ASYM))

#define OFPQT_NONE     0x0000
#define OFPQT_MIN_RATE 0x0001
static const struct tok ofpqt_str[] = {
	{ OFPQT_NONE,     "NONE"     },
	{ OFPQT_MIN_RATE, "MIN_RATE" },
	{ 0, NULL }
};

#define OFPFW_IN_PORT      (1U <<0)
#define OFPFW_DL_VLAN      (1U <<1)
#define OFPFW_DL_SRC       (1U <<2)
#define OFPFW_DL_DST       (1U <<3)
#define OFPFW_DL_TYPE      (1U <<4)
#define OFPFW_NW_PROTO     (1U <<5)
#define OFPFW_TP_SRC       (1U <<6)
#define OFPFW_TP_DST       (1U <<7)
#define OFPFW_NW_SRC_SHIFT 8
#define OFPFW_NW_SRC_BITS  6
#define OFPFW_NW_SRC_MASK  (((1U <<OFPFW_NW_SRC_BITS) - 1) << OFPFW_NW_SRC_SHIFT)
#define OFPFW_NW_DST_SHIFT 14
#define OFPFW_NW_DST_BITS  6
#define OFPFW_NW_DST_MASK  (((1U <<OFPFW_NW_DST_BITS) - 1) << OFPFW_NW_DST_SHIFT)
#define OFPFW_DL_VLAN_PCP  (1U <<20)
#define OFPFW_NW_TOS       (1U <<21)
#define OFPFW_ALL          ((1U <<22) - 1)
static const struct tok ofpfw_bm[] = {
	{ OFPFW_IN_PORT,     "IN_PORT"     },
	{ OFPFW_DL_VLAN,     "DL_VLAN"     },
	{ OFPFW_DL_SRC,      "DL_SRC"      },
	{ OFPFW_DL_DST,      "DL_DST"      },
	{ OFPFW_DL_TYPE,     "DL_TYPE"     },
	{ OFPFW_NW_PROTO,    "NW_PROTO"    },
	{ OFPFW_TP_SRC,      "TP_SRC"      },
	{ OFPFW_TP_DST,      "TP_DST"      },
	{ OFPFW_DL_VLAN_PCP, "DL_VLAN_PCP" },
	{ OFPFW_NW_TOS,      "NW_TOS"      },
	{ 0, NULL }
};
/* The above array does not include bits 8~13 (OFPFW_NW_SRC_*) and 14~19
 * (OFPFW_NW_DST_*), which are not a part of the bitmap and require decoding
 * other than that of tok2str(). The macro below includes these bits such that
 * they are not reported as bogus in the decoding. */
#define OFPFW_U (~(OFPFW_ALL))

#define OFPAT_OUTPUT       0x0000U
#define OFPAT_SET_VLAN_VID 0x0001U
#define OFPAT_SET_VLAN_PCP 0x0002U
#define OFPAT_STRIP_VLAN   0x0003U
#define OFPAT_SET_DL_SRC   0x0004U
#define OFPAT_SET_DL_DST   0x0005U
#define OFPAT_SET_NW_SRC   0x0006U
#define OFPAT_SET_NW_DST   0x0007U
#define OFPAT_SET_NW_TOS   0x0008U
#define OFPAT_SET_TP_SRC   0x0009U
#define OFPAT_SET_TP_DST   0x000aU
#define OFPAT_ENQUEUE      0x000bU
#define OFPAT_VENDOR       0xffffU
static const struct tok ofpat_str[] = {
	{ OFPAT_OUTPUT,       "OUTPUT"       },
	{ OFPAT_SET_VLAN_VID, "SET_VLAN_VID" },
	{ OFPAT_SET_VLAN_PCP, "SET_VLAN_PCP" },
	{ OFPAT_STRIP_VLAN,   "STRIP_VLAN"   },
	{ OFPAT_SET_DL_SRC,   "SET_DL_SRC"   },
	{ OFPAT_SET_DL_DST,   "SET_DL_DST"   },
	{ OFPAT_SET_NW_SRC,   "SET_NW_SRC"   },
	{ OFPAT_SET_NW_DST,   "SET_NW_DST"   },
	{ OFPAT_SET_NW_TOS,   "SET_NW_TOS"   },
	{ OFPAT_SET_TP_SRC,   "SET_TP_SRC"   },
	{ OFPAT_SET_TP_DST,   "SET_TP_DST"   },
	{ OFPAT_ENQUEUE,      "ENQUEUE"      },
	{ OFPAT_VENDOR,       "VENDOR"       },
	{ 0, NULL }
};

/* bit-shifted, w/o vendor action */
static const struct tok ofpat_bm[] = {
	{ 1U <<OFPAT_OUTPUT,       "OUTPUT"       },
	{ 1U <<OFPAT_SET_VLAN_VID, "SET_VLAN_VID" },
	{ 1U <<OFPAT_SET_VLAN_PCP, "SET_VLAN_PCP" },
	{ 1U <<OFPAT_STRIP_VLAN,   "STRIP_VLAN"   },
	{ 1U <<OFPAT_SET_DL_SRC,   "SET_DL_SRC"   },
	{ 1U <<OFPAT_SET_DL_DST,   "SET_DL_DST"   },
	{ 1U <<OFPAT_SET_NW_SRC,   "SET_NW_SRC"   },
	{ 1U <<OFPAT_SET_NW_DST,   "SET_NW_DST"   },
	{ 1U <<OFPAT_SET_NW_TOS,   "SET_NW_TOS"   },
	{ 1U <<OFPAT_SET_TP_SRC,   "SET_TP_SRC"   },
	{ 1U <<OFPAT_SET_TP_DST,   "SET_TP_DST"   },
	{ 1U <<OFPAT_ENQUEUE,      "ENQUEUE"      },
	{ 0, NULL }
};
#define OFPAT_U (~(1U <<OFPAT_OUTPUT | 1U <<OFPAT_SET_VLAN_VID | \
                   1U <<OFPAT_SET_VLAN_PCP | 1U <<OFPAT_STRIP_VLAN | \
                   1U <<OFPAT_SET_DL_SRC | 1U <<OFPAT_SET_DL_DST | \
                   1U <<OFPAT_SET_NW_SRC | 1U <<OFPAT_SET_NW_DST | \
                   1U <<OFPAT_SET_NW_TOS | 1U <<OFPAT_SET_TP_SRC | \
                   1U <<OFPAT_SET_TP_DST | 1U <<OFPAT_ENQUEUE))

#define OFPC_FLOW_STATS   (1U <<0)
#define OFPC_TABLE_STATS  (1U <<1)
#define OFPC_PORT_STATS   (1U <<2)
#define OFPC_STP          (1U <<3)
#define OFPC_RESERVED     (1U <<4)
#define OFPC_IP_REASM     (1U <<5)
#define OFPC_QUEUE_STATS  (1U <<6)
#define OFPC_ARP_MATCH_IP (1U <<7)
static const struct tok ofp_capabilities_bm[] = {
	{ OFPC_FLOW_STATS,   "FLOW_STATS"   },
	{ OFPC_TABLE_STATS,  "TABLE_STATS"  },
	{ OFPC_PORT_STATS,   "PORT_STATS"   },
	{ OFPC_STP,          "STP"          },
	{ OFPC_RESERVED,     "RESERVED"     }, /* not in the mask below */
	{ OFPC_IP_REASM,     "IP_REASM"     },
	{ OFPC_QUEUE_STATS,  "QUEUE_STATS"  },
	{ OFPC_ARP_MATCH_IP, "ARP_MATCH_IP" },
	{ 0, NULL }
};
#define OFPCAP_U (~(OFPC_FLOW_STATS | OFPC_TABLE_STATS | OFPC_PORT_STATS | \
                    OFPC_STP | OFPC_IP_REASM | OFPC_QUEUE_STATS | \
                    OFPC_ARP_MATCH_IP))

#define OFPC_FRAG_NORMAL 0x0000U
#define OFPC_FRAG_DROP   0x0001U
#define OFPC_FRAG_REASM  0x0002U
#define OFPC_FRAG_MASK   0x0003U
static const struct tok ofp_config_str[] = {
	{ OFPC_FRAG_NORMAL, "FRAG_NORMAL" },
	{ OFPC_FRAG_DROP,   "FRAG_DROP"   },
	{ OFPC_FRAG_REASM,  "FRAG_REASM"  },
	{ 0, NULL }
};

#define OFPFC_ADD           0x0000U
#define OFPFC_MODIFY        0x0001U
#define OFPFC_MODIFY_STRICT 0x0002U
#define OFPFC_DELETE        0x0003U
#define OFPFC_DELETE_STRICT 0x0004U
static const struct tok ofpfc_str[] = {
	{ OFPFC_ADD,           "ADD"           },
	{ OFPFC_MODIFY,        "MODIFY"        },
	{ OFPFC_MODIFY_STRICT, "MODIFY_STRICT" },
	{ OFPFC_DELETE,        "DELETE"        },
	{ OFPFC_DELETE_STRICT, "DELETE_STRICT" },
	{ 0, NULL }
};

static const struct tok bufferid_str[] = {
	{ 0xffffffff, "NONE" },
	{ 0, NULL }
};

#define OFPFF_SEND_FLOW_REM (1U <<0)
#define OFPFF_CHECK_OVERLAP (1U <<1)
#define OFPFF_EMERG         (1U <<2)
static const struct tok ofpff_bm[] = {
	{ OFPFF_SEND_FLOW_REM, "SEND_FLOW_REM" },
	{ OFPFF_CHECK_OVERLAP, "CHECK_OVERLAP" },
	{ OFPFF_EMERG,         "EMERG"         },
	{ 0, NULL }
};
#define OFPFF_U (~(OFPFF_SEND_FLOW_REM | OFPFF_CHECK_OVERLAP | OFPFF_EMERG))

#define OFPST_DESC      0x0000U
#define OFPST_FLOW      0x0001U
#define OFPST_AGGREGATE 0x0002U
#define OFPST_TABLE     0x0003U
#define OFPST_PORT      0x0004U
#define OFPST_QUEUE     0x0005U
#define OFPST_VENDOR    0xffffU
static const struct tok ofpst_str[] = {
	{ OFPST_DESC,      "DESC"      },
	{ OFPST_FLOW,      "FLOW"      },
	{ OFPST_AGGREGATE, "AGGREGATE" },
	{ OFPST_TABLE,     "TABLE"     },
	{ OFPST_PORT,      "PORT"      },
	{ OFPST_QUEUE,     "QUEUE"     },
	{ OFPST_VENDOR,    "VENDOR"    },
	{ 0, NULL }
};

static const struct tok tableid_str[] = {
	{ 0xfeU, "EMERG" },
	{ 0xffU, "ALL"   },
	{ 0, NULL }
};

#define OFPQ_ALL      0xffffffffU
static const struct tok ofpq_str[] = {
	{ OFPQ_ALL, "ALL" },
	{ 0, NULL }
};

#define OFPSF_REPLY_MORE 0x0001U
static const struct tok ofpsf_reply_bm[] = {
	{ OFPSF_REPLY_MORE, "MORE" },
	{ 0, NULL }
};
#define OFPSF_REPLY_U (~(OFPSF_REPLY_MORE))

#define OFPR_NO_MATCH 0x00U
#define OFPR_ACTION   0x01U
static const struct tok ofpr_str[] = {
	{ OFPR_NO_MATCH, "NO_MATCH" },
	{ OFPR_ACTION,   "ACTION"   },
	{ 0, NULL }
};

#define OFPRR_IDLE_TIMEOUT 0x00U
#define OFPRR_HARD_TIMEOUT 0x01U
#define OFPRR_DELETE       0x02U
static const struct tok ofprr_str[] = {
	{ OFPRR_IDLE_TIMEOUT, "IDLE_TIMEOUT" },
	{ OFPRR_HARD_TIMEOUT, "HARD_TIMEOUT" },
	{ OFPRR_DELETE,       "DELETE"       },
	{ 0, NULL }
};

#define OFPPR_ADD    0x00U
#define OFPPR_DELETE 0x01U
#define OFPPR_MODIFY 0x02U
static const struct tok ofppr_str[] = {
	{ OFPPR_ADD,    "ADD"    },
	{ OFPPR_DELETE, "DELETE" },
	{ OFPPR_MODIFY, "MODIFY" },
	{ 0, NULL }
};

#define OFPET_HELLO_FAILED    0x0000U
#define OFPET_BAD_REQUEST     0x0001U
#define OFPET_BAD_ACTION      0x0002U
#define OFPET_FLOW_MOD_FAILED 0x0003U
#define OFPET_PORT_MOD_FAILED 0x0004U
#define OFPET_QUEUE_OP_FAILED 0x0005U
static const struct tok ofpet_str[] = {
	{ OFPET_HELLO_FAILED,    "HELLO_FAILED"    },
	{ OFPET_BAD_REQUEST,     "BAD_REQUEST"     },
	{ OFPET_BAD_ACTION,      "BAD_ACTION"      },
	{ OFPET_FLOW_MOD_FAILED, "FLOW_MOD_FAILED" },
	{ OFPET_PORT_MOD_FAILED, "PORT_MOD_FAILED" },
	{ OFPET_QUEUE_OP_FAILED, "QUEUE_OP_FAILED" },
	{ 0, NULL }
};

#define OFPHFC_INCOMPATIBLE 0x0000U
#define OFPHFC_EPERM        0x0001U
static const struct tok ofphfc_str[] = {
	{ OFPHFC_INCOMPATIBLE, "INCOMPATIBLE" },
	{ OFPHFC_EPERM,        "EPERM"        },
	{ 0, NULL }
};

#define OFPBRC_BAD_VERSION    0x0000U
#define OFPBRC_BAD_TYPE       0x0001U
#define OFPBRC_BAD_STAT       0x0002U
#define OFPBRC_BAD_VENDOR     0x0003U
#define OFPBRC_BAD_SUBTYPE    0x0004U
#define OFPBRC_EPERM          0x0005U
#define OFPBRC_BAD_LEN        0x0006U
#define OFPBRC_BUFFER_EMPTY   0x0007U
#define OFPBRC_BUFFER_UNKNOWN 0x0008U
static const struct tok ofpbrc_str[] = {
	{ OFPBRC_BAD_VERSION,    "BAD_VERSION"    },
	{ OFPBRC_BAD_TYPE,       "BAD_TYPE"       },
	{ OFPBRC_BAD_STAT,       "BAD_STAT"       },
	{ OFPBRC_BAD_VENDOR,     "BAD_VENDOR"     },
	{ OFPBRC_BAD_SUBTYPE,    "BAD_SUBTYPE"    },
	{ OFPBRC_EPERM,          "EPERM"          },
	{ OFPBRC_BAD_LEN,        "BAD_LEN"        },
	{ OFPBRC_BUFFER_EMPTY,   "BUFFER_EMPTY"   },
	{ OFPBRC_BUFFER_UNKNOWN, "BUFFER_UNKNOWN" },
	{ 0, NULL }
};

#define OFPBAC_BAD_TYPE        0x0000U
#define OFPBAC_BAD_LEN         0x0001U
#define OFPBAC_BAD_VENDOR      0x0002U
#define OFPBAC_BAD_VENDOR_TYPE 0x0003U
#define OFPBAC_BAD_OUT_PORT    0x0004U
#define OFPBAC_BAD_ARGUMENT    0x0005U
#define OFPBAC_EPERM           0x0006U
#define OFPBAC_TOO_MANY        0x0007U
#define OFPBAC_BAD_QUEUE       0x0008U
static const struct tok ofpbac_str[] = {
	{ OFPBAC_BAD_TYPE,        "BAD_TYPE"        },
	{ OFPBAC_BAD_LEN,         "BAD_LEN"         },
	{ OFPBAC_BAD_VENDOR,      "BAD_VENDOR"      },
	{ OFPBAC_BAD_VENDOR_TYPE, "BAD_VENDOR_TYPE" },
	{ OFPBAC_BAD_OUT_PORT,    "BAD_OUT_PORT"    },
	{ OFPBAC_BAD_ARGUMENT,    "BAD_ARGUMENT"    },
	{ OFPBAC_EPERM,           "EPERM"           },
	{ OFPBAC_TOO_MANY,        "TOO_MANY"        },
	{ OFPBAC_BAD_QUEUE,       "BAD_QUEUE"       },
	{ 0, NULL }
};

#define OFPFMFC_ALL_TABLES_FULL   0x0000U
#define OFPFMFC_OVERLAP           0x0001U
#define OFPFMFC_EPERM             0x0002U
#define OFPFMFC_BAD_EMERG_TIMEOUT 0x0003U
#define OFPFMFC_BAD_COMMAND       0x0004U
#define OFPFMFC_UNSUPPORTED       0x0005U
static const struct tok ofpfmfc_str[] = {
	{ OFPFMFC_ALL_TABLES_FULL,   "ALL_TABLES_FULL"   },
	{ OFPFMFC_OVERLAP,           "OVERLAP"           },
	{ OFPFMFC_EPERM,             "EPERM"             },
	{ OFPFMFC_BAD_EMERG_TIMEOUT, "BAD_EMERG_TIMEOUT" },
	{ OFPFMFC_BAD_COMMAND,       "BAD_COMMAND"       },
	{ OFPFMFC_UNSUPPORTED,       "UNSUPPORTED"       },
	{ 0, NULL }
};

#define OFPPMFC_BAD_PORT    0x0000U
#define OFPPMFC_BAD_HW_ADDR 0x0001U
static const struct tok ofppmfc_str[] = {
	{ OFPPMFC_BAD_PORT,    "BAD_PORT"    },
	{ OFPPMFC_BAD_HW_ADDR, "BAD_HW_ADDR" },
	{ 0, NULL }
};

#define OFPQOFC_BAD_PORT  0x0000U
#define OFPQOFC_BAD_QUEUE 0x0001U
#define OFPQOFC_EPERM     0x0002U
static const struct tok ofpqofc_str[] = {
	{ OFPQOFC_BAD_PORT,  "BAD_PORT"  },
	{ OFPQOFC_BAD_QUEUE, "BAD_QUEUE" },
	{ OFPQOFC_EPERM,     "EPERM"     },
	{ 0, NULL }
};

static const struct uint_tokary of10_ofpet2tokary[] = {
	{ OFPET_HELLO_FAILED,    ofphfc_str  },
	{ OFPET_BAD_REQUEST,     ofpbrc_str  },
	{ OFPET_BAD_ACTION,      ofpbac_str  },
	{ OFPET_FLOW_MOD_FAILED, ofpfmfc_str },
	{ OFPET_PORT_MOD_FAILED, ofppmfc_str },
	{ OFPET_QUEUE_OP_FAILED, ofpqofc_str },
	/* uint2tokary() does not use array termination. */
};

/* lengths (fixed or minimal) of particular message types, where not 0 */
#define OF_SWITCH_CONFIG_FIXLEN            (12U - OF_HEADER_FIXLEN)
#define OF_FEATURES_REPLY_MINLEN           (32U - OF_HEADER_FIXLEN)
#define OF_PORT_STATUS_FIXLEN              (64U - OF_HEADER_FIXLEN)
#define OF_PORT_MOD_FIXLEN                 (32U - OF_HEADER_FIXLEN)
#define OF_PACKET_IN_MINLEN                (20U - OF_HEADER_FIXLEN) /* with 2 mock octets */
#define OF_PACKET_OUT_MINLEN               (16U - OF_HEADER_FIXLEN)
#define OF_FLOW_MOD_MINLEN                 (72U - OF_HEADER_FIXLEN)
#define OF_FLOW_REMOVED_FIXLEN             (88U - OF_HEADER_FIXLEN)
#define OF_ERROR_MSG_MINLEN                (12U - OF_HEADER_FIXLEN)
#define OF_STATS_REQUEST_MINLEN            (12U - OF_HEADER_FIXLEN)
#define OF_STATS_REPLY_MINLEN              (12U - OF_HEADER_FIXLEN)
#define OF_VENDOR_MINLEN                   (12U - OF_HEADER_FIXLEN)
#define OF_QUEUE_GET_CONFIG_REQUEST_FIXLEN (12U - OF_HEADER_FIXLEN)
#define OF_QUEUE_GET_CONFIG_REPLY_MINLEN   (16U - OF_HEADER_FIXLEN)

/* lengths (fixed or minimal) of particular protocol structures */
#define OF_PHY_PORT_FIXLEN                    48
#define OF_ACTION_MINLEN                       8
#define OF_MATCH_FIXLEN                       40
#define OF_DESC_STATS_REPLY_FIXLEN          1056
#define OF_FLOW_STATS_REQUEST_FIXLEN          44
#define OF_FLOW_STATS_REPLY_MINLEN            88
#define OF_AGGREGATE_STATS_REPLY_FIXLEN       24
#define OF_TABLE_STATS_REPLY_FIXLEN           64
#define OF_PORT_STATS_REQUEST_FIXLEN           8
#define OF_PORT_STATS_REPLY_FIXLEN           104
#define OF_QUEUE_PROP_MINLEN                   8
#define OF_QUEUE_PROP_MIN_RATE_FIXLEN         16
#define OF_PACKET_QUEUE_MINLEN                 8
#define OF_QUEUE_STATS_REQUEST_FIXLEN          8
#define OF_QUEUE_STATS_REPLY_FIXLEN           32

/* miscellaneous constants from [OF10] */
#define OFP_MAX_TABLE_NAME_LEN     32
#define OFP_MAX_PORT_NAME_LEN      16
#define DESC_STR_LEN              256
#define SERIAL_NUM_LEN             32
#define OFP_VLAN_NONE          0xffffU

/* vendor extensions */
#define BSN_SET_IP_MASK                    0
#define BSN_GET_IP_MASK_REQUEST            1
#define BSN_GET_IP_MASK_REPLY              2
#define BSN_SET_MIRRORING                  3
#define BSN_GET_MIRRORING_REQUEST          4
#define BSN_GET_MIRRORING_REPLY            5
#define BSN_SHELL_COMMAND                  6
#define BSN_SHELL_OUTPUT                   7
#define BSN_SHELL_STATUS                   8
#define BSN_GET_INTERFACES_REQUEST         9
#define BSN_GET_INTERFACES_REPLY          10
#define BSN_SET_PKTIN_SUPPRESSION_REQUEST 11
#define BSN_SET_L2_TABLE_REQUEST          12
#define BSN_GET_L2_TABLE_REQUEST          13
#define BSN_GET_L2_TABLE_REPLY            14
#define BSN_VIRTUAL_PORT_CREATE_REQUEST   15
#define BSN_VIRTUAL_PORT_CREATE_REPLY     16
#define BSN_VIRTUAL_PORT_REMOVE_REQUEST   17
#define BSN_BW_ENABLE_SET_REQUEST         18
#define BSN_BW_ENABLE_GET_REQUEST         19
#define BSN_BW_ENABLE_GET_REPLY           20
#define BSN_BW_CLEAR_DATA_REQUEST         21
#define BSN_BW_CLEAR_DATA_REPLY           22
#define BSN_BW_ENABLE_SET_REPLY           23
#define BSN_SET_L2_TABLE_REPLY            24
#define BSN_SET_PKTIN_SUPPRESSION_REPLY   25
#define BSN_VIRTUAL_PORT_REMOVE_REPLY     26
#define BSN_HYBRID_GET_REQUEST            27
#define BSN_HYBRID_GET_REPLY              28
                                       /* 29 */
                                       /* 30 */
#define BSN_PDU_TX_REQUEST                31
#define BSN_PDU_TX_REPLY                  32
#define BSN_PDU_RX_REQUEST                33
#define BSN_PDU_RX_REPLY                  34
#define BSN_PDU_RX_TIMEOUT                35

static const struct tok bsn_subtype_str[] = {
	{ BSN_SET_IP_MASK,                   "SET_IP_MASK"                   },
	{ BSN_GET_IP_MASK_REQUEST,           "GET_IP_MASK_REQUEST"           },
	{ BSN_GET_IP_MASK_REPLY,             "GET_IP_MASK_REPLY"             },
	{ BSN_SET_MIRRORING,                 "SET_MIRRORING"                 },
	{ BSN_GET_MIRRORING_REQUEST,         "GET_MIRRORING_REQUEST"         },
	{ BSN_GET_MIRRORING_REPLY,           "GET_MIRRORING_REPLY"           },
	{ BSN_SHELL_COMMAND,                 "SHELL_COMMAND"                 },
	{ BSN_SHELL_OUTPUT,                  "SHELL_OUTPUT"                  },
	{ BSN_SHELL_STATUS,                  "SHELL_STATUS"                  },
	{ BSN_GET_INTERFACES_REQUEST,        "GET_INTERFACES_REQUEST"        },
	{ BSN_GET_INTERFACES_REPLY,          "GET_INTERFACES_REPLY"          },
	{ BSN_SET_PKTIN_SUPPRESSION_REQUEST, "SET_PKTIN_SUPPRESSION_REQUEST" },
	{ BSN_SET_L2_TABLE_REQUEST,          "SET_L2_TABLE_REQUEST"          },
	{ BSN_GET_L2_TABLE_REQUEST,          "GET_L2_TABLE_REQUEST"          },
	{ BSN_GET_L2_TABLE_REPLY,            "GET_L2_TABLE_REPLY"            },
	{ BSN_VIRTUAL_PORT_CREATE_REQUEST,   "VIRTUAL_PORT_CREATE_REQUEST"   },
	{ BSN_VIRTUAL_PORT_CREATE_REPLY,     "VIRTUAL_PORT_CREATE_REPLY"     },
	{ BSN_VIRTUAL_PORT_REMOVE_REQUEST,   "VIRTUAL_PORT_REMOVE_REQUEST"   },
	{ BSN_BW_ENABLE_SET_REQUEST,         "BW_ENABLE_SET_REQUEST"         },
	{ BSN_BW_ENABLE_GET_REQUEST,         "BW_ENABLE_GET_REQUEST"         },
	{ BSN_BW_ENABLE_GET_REPLY,           "BW_ENABLE_GET_REPLY"           },
	{ BSN_BW_CLEAR_DATA_REQUEST,         "BW_CLEAR_DATA_REQUEST"         },
	{ BSN_BW_CLEAR_DATA_REPLY,           "BW_CLEAR_DATA_REPLY"           },
	{ BSN_BW_ENABLE_SET_REPLY,           "BW_ENABLE_SET_REPLY"           },
	{ BSN_SET_L2_TABLE_REPLY,            "SET_L2_TABLE_REPLY"            },
	{ BSN_SET_PKTIN_SUPPRESSION_REPLY,   "SET_PKTIN_SUPPRESSION_REPLY"   },
	{ BSN_VIRTUAL_PORT_REMOVE_REPLY,     "VIRTUAL_PORT_REMOVE_REPLY"     },
	{ BSN_HYBRID_GET_REQUEST,            "HYBRID_GET_REQUEST"            },
	{ BSN_HYBRID_GET_REPLY,              "HYBRID_GET_REPLY"              },
	{ BSN_PDU_TX_REQUEST,                "PDU_TX_REQUEST"                },
	{ BSN_PDU_TX_REPLY,                  "PDU_TX_REPLY"                  },
	{ BSN_PDU_RX_REQUEST,                "PDU_RX_REQUEST"                },
	{ BSN_PDU_RX_REPLY,                  "PDU_RX_REPLY"                  },
	{ BSN_PDU_RX_TIMEOUT,                "PDU_RX_TIMEOUT"                },
	{ 0, NULL }
};

#define BSN_ACTION_MIRROR                  1
#define BSN_ACTION_SET_TUNNEL_DST          2
                                        /* 3 */
#define BSN_ACTION_CHECKSUM                4

static const struct tok bsn_action_subtype_str[] = {
	{ BSN_ACTION_MIRROR,                 "MIRROR"                        },
	{ BSN_ACTION_SET_TUNNEL_DST,         "SET_TUNNEL_DST"                },
	{ BSN_ACTION_CHECKSUM,               "CHECKSUM"                      },
	{ 0, NULL }
};

static const struct tok bsn_mirror_copy_stage_str[] = {
	{ 0, "INGRESS" },
	{ 1, "EGRESS"  },
	{ 0, NULL },
};

static const struct tok bsn_onoff_str[] = {
	{ 0, "OFF" },
	{ 1, "ON"  },
	{ 0, NULL },
};

static const char *
vlan_str(const uint16_t vid)
{
	static char buf[sizeof("65535 (bogus)")];

	if (vid == OFP_VLAN_NONE)
		return "NONE";
	snprintf(buf, sizeof(buf), "%u%s", vid,
	         (vid > 0 && vid < 0x0fff) ? "" : " (bogus)");
	return buf;
}

static const char *
pcp_str(const uint8_t pcp)
{
	static char buf[sizeof("255 (bogus)")];
	snprintf(buf, sizeof(buf), "%u%s", pcp,
	         pcp <= 7 ? "" : " (bogus)");
	return buf;
}

static void
of10_bsn_message_print(netdissect_options *ndo,
                       const u_char *cp, u_int len)
{
	uint32_t subtype;

	if (len < 4)
		goto invalid;
	/* subtype */
	subtype = GET_BE_U_4(cp);
	OF_FWD(4);
	ND_PRINT("\n\t subtype %s", tok2str(bsn_subtype_str, "unknown (0x%08x)", subtype));
	switch (subtype) {
	case BSN_GET_IP_MASK_REQUEST:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * |     index     |                      pad                      |
		 * +---------------+---------------+---------------+---------------+
		 * |                              pad                              |
		 * +---------------+---------------+---------------+---------------+
		 *
		 */
		if (len != 8)
			goto invalid;
		/* index */
		ND_PRINT(", index %u", GET_U_1(cp));
		OF_FWD(1);
		/* pad */
		/* Always the last field, check bounds. */
		ND_TCHECK_7(cp);
		break;
	case BSN_SET_IP_MASK:
	case BSN_GET_IP_MASK_REPLY:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * |     index     |                      pad                      |
		 * +---------------+---------------+---------------+---------------+
		 * |                              mask                             |
		 * +---------------+---------------+---------------+---------------+
		 *
		 */
		if (len != 8)
			goto invalid;
		/* index */
		ND_PRINT(", index %u", GET_U_1(cp));
		OF_FWD(1);
		/* pad */
		OF_FWD(3);
		/* mask */
		ND_PRINT(", mask %s", GET_IPADDR_STRING(cp));
		break;
	case BSN_SET_MIRRORING:
	case BSN_GET_MIRRORING_REQUEST:
	case BSN_GET_MIRRORING_REPLY:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * | report m. p.  |                      pad                      |
		 * +---------------+---------------+---------------+---------------+
		 *
		 */
		if (len != 4)
			goto invalid;
		/* report_mirror_ports */
		ND_PRINT(", report_mirror_ports %s",
			 tok2str(bsn_onoff_str, "bogus (%u)", GET_U_1(cp)));
		OF_FWD(1);
		/* pad */
		/* Always the last field, check bounds. */
		ND_TCHECK_3(cp);
		break;
	case BSN_GET_INTERFACES_REQUEST:
	case BSN_GET_L2_TABLE_REQUEST:
	case BSN_BW_ENABLE_GET_REQUEST:
	case BSN_BW_CLEAR_DATA_REQUEST:
	case BSN_HYBRID_GET_REQUEST:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 *
		 */
		if (len)
			goto invalid;
		break;
	case BSN_VIRTUAL_PORT_REMOVE_REQUEST:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * |                           vport_no                            |
		 * +---------------+---------------+---------------+---------------+
		 *
		 */
		if (len != 4)
			goto invalid;
		/* vport_no */
		ND_PRINT(", vport_no %u", GET_BE_U_4(cp));
		break;
	case BSN_SHELL_COMMAND:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * |                            service                            |
		 * +---------------+---------------+---------------+---------------+
		 * |                             data ...
		 * +---------------+---------------+--------
		 *
		 */
		if (len < 4)
			goto invalid;
		/* service */
		ND_PRINT(", service %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* data */
		ND_PRINT(", data '");
		(void)nd_printn(ndo, cp, len, NULL);
		ND_PRINT("'");
		break;
	case BSN_SHELL_OUTPUT:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * |                             data ...
		 * +---------------+---------------+--------
		 *
		 */
		/* already checked that len >= 4 */
		/* data */
		ND_PRINT(", data '");
		(void)nd_printn(ndo, cp, len, NULL);
		ND_PRINT("'");
		break;
	case BSN_SHELL_STATUS:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * |                            status                             |
		 * +---------------+---------------+---------------+---------------+
		 *
		 */
		if (len != 4)
			goto invalid;
		/* status */
		ND_PRINT(", status 0x%08x", GET_BE_U_4(cp));
		break;
	default:
		ND_TCHECK_LEN(cp, len);
	}
	return;

invalid: /* skip the undersized data */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

static void
of10_bsn_actions_print(netdissect_options *ndo,
                       const u_char *cp, u_int len)
{
	uint32_t subtype, vlan_tag;

	if (len < 4)
		goto invalid;
	/* subtype */
	subtype = GET_BE_U_4(cp);
	OF_FWD(4);
	ND_PRINT("\n\t  subtype %s", tok2str(bsn_action_subtype_str, "unknown (0x%08x)", subtype));
	switch (subtype) {
	case BSN_ACTION_MIRROR:
		/*
		 *  0                   1                   2                   3
		 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		 * +---------------+---------------+---------------+---------------+
		 * |                            subtype                            |
		 * +---------------+---------------+---------------+---------------+
		 * |                           dest_port                           |
		 * +---------------+---------------+---------------+---------------+
		 * |                           vlan_tag                            |
		 * +---------------+---------------+---------------+---------------+
		 * |  copy_stage   |                      pad                      |
		 * +---------------+---------------+---------------+---------------+
		 *
		 */
		if (len != 12)
			goto invalid;
		/* dest_port */
		ND_PRINT(", dest_port %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* vlan_tag */
		vlan_tag = GET_BE_U_4(cp);
		OF_FWD(4);
		switch (vlan_tag >> 16) {
		case 0:
			ND_PRINT(", vlan_tag none");
			break;
		case ETHERTYPE_8021Q:
			ND_PRINT(", vlan_tag 802.1Q (%s)", ieee8021q_tci_string(vlan_tag & 0xffff));
			break;
		default:
			ND_PRINT(", vlan_tag unknown (0x%04x)", vlan_tag >> 16);
		}
		/* copy_stage */
		ND_PRINT(", copy_stage %s",
			 tok2str(bsn_mirror_copy_stage_str, "unknown (%u)", GET_U_1(cp)));
		OF_FWD(1);
		/* pad */
		/* Always the last field, check bounds. */
		ND_TCHECK_3(cp);
		break;
	default:
		ND_TCHECK_LEN(cp, len);
	}
	return;

invalid:
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

static void
of10_vendor_action_print(netdissect_options *ndo,
                         const u_char *cp, u_int len)
{
	uint32_t vendor;
	void (*decoder)(netdissect_options *, const u_char *, u_int);

	if (len < 4)
		goto invalid;
	/* vendor */
	vendor = GET_BE_U_4(cp);
	OF_FWD(4);
	ND_PRINT(", vendor 0x%08x (%s)", vendor, of_vendor_name(vendor));
	/* data */
	decoder =
		vendor == OUI_BSN         ? of10_bsn_actions_print         :
		of_data_print;
	decoder(ndo, cp, len);
	return;

invalid: /* skip the undersized data */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* [OF10] Section 5.5.4 */
static void
of10_vendor_message_print(netdissect_options *ndo,
                          const u_char *cp, u_int len)
{
	uint32_t vendor;
	void (*decoder)(netdissect_options *, const u_char *, u_int);

	/* vendor */
	vendor = GET_BE_U_4(cp);
	OF_FWD(4);
	ND_PRINT(", vendor 0x%08x (%s)", vendor, of_vendor_name(vendor));
	/* data */
	decoder =
		vendor == OUI_BSN         ? of10_bsn_message_print         :
		of_data_print;
	decoder(ndo, cp, len);
}

/* Vendor ID is mandatory, data is optional. */
static void
of10_vendor_data_print(netdissect_options *ndo,
                       const u_char *cp, u_int len)
{
	uint32_t vendor;

	if (len < 4)
		goto invalid;
	/* vendor */
	vendor = GET_BE_U_4(cp);
	OF_FWD(4);
	ND_PRINT(", vendor 0x%08x (%s)", vendor, of_vendor_name(vendor));
	/* data */
	of_data_print(ndo, cp, len);
	return;

invalid: /* skip the undersized data */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

static void
of10_packet_data_print(netdissect_options *ndo,
                       const u_char *cp, const u_int len)
{
	if (len == 0)
		return;
	/* data */
	ND_PRINT("\n\t data (%u octets)", len);
	if (ndo->ndo_vflag < 3) {
		ND_TCHECK_LEN(cp, len);
		return;
	}
	ndo->ndo_vflag -= 3;
	ND_PRINT(", frame decoding below\n");
	/*
	 * The encapsulated Ethernet frame is not necessarily the last
	 * data of this packet (i.e. there may be more OpenFlow messages
	 * after the current OFPT_PACKET_IN/OFPT_PACKET_OUT message, in
	 * which case the current (outer) packet's snapshot end is not
	 * what ether_print() needs to decode an Ethernet frame nested in
	 * the middle of a TCP payload.
	 */
	const u_char *snapend_save = ndo->ndo_snapend;
	ndo->ndo_snapend = ND_MIN(cp + len, ndo->ndo_snapend);
	ether_print(ndo, cp, len, ND_BYTES_AVAILABLE_AFTER(cp), NULL, NULL);
	ndo->ndo_snapend = snapend_save;
	ndo->ndo_vflag += 3;
}

/* [OF10] Section 5.2.1 */
static void
of10_phy_port_print(netdissect_options *ndo,
                    const u_char *cp)
{
	uint32_t state;

	/* port_no */
	ND_PRINT("\n\t  port_no %s",
		 tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	cp += 2;
	/* hw_addr */
	ND_PRINT(", hw_addr %s", GET_ETHERADDR_STRING(cp));
	cp += MAC_ADDR_LEN;
	/* name */
	ND_PRINT(", name '");
	nd_printjnp(ndo, cp, OFP_MAX_PORT_NAME_LEN);
	ND_PRINT("'");
	cp += OFP_MAX_PORT_NAME_LEN;

	if (ndo->ndo_vflag < 2) {
		ND_TCHECK_LEN(cp, 24);
		return;
	}
	/* config */
	ND_PRINT("\n\t   config 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppc_bm, GET_BE_U_4(cp), OFPPC_U);
	cp += 4;
	/* state */
	state = GET_BE_U_4(cp);
	/*
	 * Decode the code point and the single bit separately, but
	 * format the result as a single sequence of comma-separated
	 * strings (see the comments at the OFPPS_ props).
	 */
	ND_PRINT("\n\t   state 0x%08x (%s%s)%s", state,
	         tok2str(ofpps_stp_str, "", state & OFPPS_STP_MASK),
	         state & OFPPS_LINK_DOWN ? ", LINK_DOWN" : "",
	         state & OFPPS_U ? " (bogus)" : "");
	cp += 4;
	/* curr */
	ND_PRINT("\n\t   curr 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppf_bm, GET_BE_U_4(cp), OFPPF_U);
	cp += 4;
	/* advertised */
	ND_PRINT("\n\t   advertised 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppf_bm, GET_BE_U_4(cp), OFPPF_U);
	cp += 4;
	/* supported */
	ND_PRINT("\n\t   supported 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppf_bm, GET_BE_U_4(cp), OFPPF_U);
	cp += 4;
	/* peer */
	ND_PRINT("\n\t   peer 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppf_bm, GET_BE_U_4(cp), OFPPF_U);
}

/* [OF10] Section 5.2.2 */
static void
of10_queue_props_print(netdissect_options *ndo,
                       const u_char *cp, u_int len)
{
	while (len) {
		uint16_t property, plen;
		u_char plen_bogus = 0, skip = 0;

		if (len < OF_QUEUE_PROP_MINLEN)
			goto invalid;
		/* property */
		property = GET_BE_U_2(cp);
		OF_FWD(2);
		ND_PRINT("\n\t   property %s", tok2str(ofpqt_str, "invalid (0x%04x)", property));
		/* len */
		plen = GET_BE_U_2(cp);
		OF_FWD(2);
		ND_PRINT(", len %u", plen);
		if (plen < OF_QUEUE_PROP_MINLEN || plen > len + 4)
			goto invalid;
		/* pad */
		/* Sometimes the last field, check bounds. */
		OF_CHK_FWD(4);
		/* property-specific constraints and decoding */
		switch (property) {
		case OFPQT_NONE:
			plen_bogus = plen != OF_QUEUE_PROP_MINLEN;
			break;
		case OFPQT_MIN_RATE:
			plen_bogus = plen != OF_QUEUE_PROP_MIN_RATE_FIXLEN;
			break;
		default:
			skip = 1;
		}
		if (plen_bogus) {
			ND_PRINT(" (bogus)");
			skip = 1;
		}
		if (skip) {
			/*
			 * plen >= OF_QUEUE_PROP_MINLEN
			 * cp is OF_QUEUE_PROP_MINLEN bytes in
			 */
			OF_CHK_FWD(plen - OF_QUEUE_PROP_MINLEN);
			continue;
		}
		if (property == OFPQT_MIN_RATE) { /* the only case of property decoding */
			/* rate */
			uint16_t rate = GET_BE_U_2(cp);
			OF_FWD(2);
			if (rate > 1000)
				ND_PRINT(", rate disabled");
			else
				ND_PRINT(", rate %u.%u%%", rate / 10, rate % 10);
			/* pad */
			/* Sometimes the last field, check bounds. */
			OF_CHK_FWD(6);
		}
	} /* while */
	return;

invalid: /* skip the rest of queue properties */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_queues_print(netdissect_options *ndo,
                  const u_char *cp, u_int len)
{
	while (len) {
		uint16_t desclen;

		if (len < OF_PACKET_QUEUE_MINLEN)
			goto invalid;
		/* queue_id */
		ND_PRINT("\n\t  queue_id %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* len */
		desclen = GET_BE_U_2(cp);
		OF_FWD(2);
		ND_PRINT(", len %u", desclen);
		if (desclen < OF_PACKET_QUEUE_MINLEN || desclen > len + 6)
			goto invalid;
		/* pad */
		/* Sometimes the last field, check bounds. */
		OF_CHK_FWD(2);
		/* properties */
		if (ndo->ndo_vflag >= 2)
			of10_queue_props_print(ndo, cp, desclen - OF_PACKET_QUEUE_MINLEN);
		else
			ND_TCHECK_LEN(cp, desclen - OF_PACKET_QUEUE_MINLEN);
		OF_FWD(desclen - OF_PACKET_QUEUE_MINLEN);
	} /* while */
	return;

invalid: /* skip the rest of queues */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* [OF10] Section 5.2.3 */
static void
of10_match_print(netdissect_options *ndo,
                 const char *pfx, const u_char *cp)
{
	uint32_t wildcards;
	uint16_t dl_type;
	uint8_t nw_proto;
	u_int nw_bits;
	const char *field_name;

	/* wildcards */
	wildcards = GET_BE_U_4(cp);
	if (wildcards & OFPFW_U)
		ND_PRINT("%swildcards 0x%08x (bogus)", pfx, wildcards);
	cp += 4;
	/* in_port */
	if (! (wildcards & OFPFW_IN_PORT))
		ND_PRINT("%smatch in_port %s", pfx,
		         tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	cp += 2;
	/* dl_src */
	if (! (wildcards & OFPFW_DL_SRC))
		ND_PRINT("%smatch dl_src %s", pfx, GET_ETHERADDR_STRING(cp));
	cp += MAC_ADDR_LEN;
	/* dl_dst */
	if (! (wildcards & OFPFW_DL_DST))
		ND_PRINT("%smatch dl_dst %s", pfx, GET_ETHERADDR_STRING(cp));
	cp += MAC_ADDR_LEN;
	/* dl_vlan */
	if (! (wildcards & OFPFW_DL_VLAN))
		ND_PRINT("%smatch dl_vlan %s", pfx, vlan_str(GET_BE_U_2(cp)));
	cp += 2;
	/* dl_vlan_pcp */
	if (! (wildcards & OFPFW_DL_VLAN_PCP))
		ND_PRINT("%smatch dl_vlan_pcp %s", pfx, pcp_str(GET_U_1(cp)));
	cp += 1;
	/* pad1 */
	cp += 1;
	/* dl_type */
	dl_type = GET_BE_U_2(cp);
	cp += 2;
	if (! (wildcards & OFPFW_DL_TYPE))
		ND_PRINT("%smatch dl_type 0x%04x", pfx, dl_type);
	/* nw_tos */
	if (! (wildcards & OFPFW_NW_TOS))
		ND_PRINT("%smatch nw_tos 0x%02x", pfx, GET_U_1(cp));
	cp += 1;
	/* nw_proto */
	nw_proto = GET_U_1(cp);
	cp += 1;
	if (! (wildcards & OFPFW_NW_PROTO)) {
		field_name = ! (wildcards & OFPFW_DL_TYPE) && dl_type == ETHERTYPE_ARP
		  ? "arp_opcode" : "nw_proto";
		ND_PRINT("%smatch %s %u", pfx, field_name, nw_proto);
	}
	/* pad2 */
	cp += 2;
	/* nw_src */
	nw_bits = (wildcards & OFPFW_NW_SRC_MASK) >> OFPFW_NW_SRC_SHIFT;
	if (nw_bits < 32)
		ND_PRINT("%smatch nw_src %s/%u", pfx, GET_IPADDR_STRING(cp), 32 - nw_bits);
	cp += 4;
	/* nw_dst */
	nw_bits = (wildcards & OFPFW_NW_DST_MASK) >> OFPFW_NW_DST_SHIFT;
	if (nw_bits < 32)
		ND_PRINT("%smatch nw_dst %s/%u", pfx, GET_IPADDR_STRING(cp), 32 - nw_bits);
	cp += 4;
	/* tp_src */
	if (! (wildcards & OFPFW_TP_SRC)) {
		field_name = ! (wildcards & OFPFW_DL_TYPE) && dl_type == ETHERTYPE_IP
		  && ! (wildcards & OFPFW_NW_PROTO) && nw_proto == IPPROTO_ICMP
		  ? "icmp_type" : "tp_src";
		ND_PRINT("%smatch %s %u", pfx, field_name, GET_BE_U_2(cp));
	}
	cp += 2;
	/* tp_dst */
	/* The last unconditional check was at nw_proto, so have an "else" here. */
	if (! (wildcards & OFPFW_TP_DST)) {
		field_name = ! (wildcards & OFPFW_DL_TYPE) && dl_type == ETHERTYPE_IP
		  && ! (wildcards & OFPFW_NW_PROTO) && nw_proto == IPPROTO_ICMP
		  ? "icmp_code" : "tp_dst";
		ND_PRINT("%smatch %s %u", pfx, field_name, GET_BE_U_2(cp));
	}
	else
		ND_TCHECK_2(cp);
}

/* [OF10] Section 5.2.4 */
static void
of10_actions_print(netdissect_options *ndo,
                   const char *pfx, const u_char *cp, u_int len)
{
	while (len) {
		uint16_t type, alen, output_port;
		u_char alen_bogus = 0, skip = 0;

		if (len < OF_ACTION_MINLEN)
			goto invalid;
		/* type */
		type = GET_BE_U_2(cp);
		OF_FWD(2);
		ND_PRINT("%saction type %s", pfx, tok2str(ofpat_str, "invalid (0x%04x)", type));
		/* length */
		alen = GET_BE_U_2(cp);
		OF_FWD(2);
		ND_PRINT(", len %u", alen);
		/*
		 * The 4-byte "pad" in the specification is not a field of the
		 * action header, but a placeholder to illustrate the 64-bit
		 * alignment requirement. Action type specific case blocks
		 * below fetch these 4 bytes.
		 */

		/* On action size underrun/overrun skip the rest of the action list. */
		if (alen < OF_ACTION_MINLEN || alen > len + 4)
			goto invalid;
		/*
		 * After validating the basic length constraint it will be safe
		 * to skip the current action if the action size is not valid
		 * for the type or the type is invalid.
		 */
		switch (type) {
		case OFPAT_OUTPUT:
		case OFPAT_SET_VLAN_VID:
		case OFPAT_SET_VLAN_PCP:
		case OFPAT_STRIP_VLAN:
		case OFPAT_SET_NW_SRC:
		case OFPAT_SET_NW_DST:
		case OFPAT_SET_NW_TOS:
		case OFPAT_SET_TP_SRC:
		case OFPAT_SET_TP_DST:
			alen_bogus = alen != 8;
			break;
		case OFPAT_SET_DL_SRC:
		case OFPAT_SET_DL_DST:
		case OFPAT_ENQUEUE:
			alen_bogus = alen != 16;
			break;
		case OFPAT_VENDOR:
			alen_bogus = alen % 8 != 0; /* already >= 8 so far */
			break;
		default:
			skip = 1;
		}
		if (alen_bogus) {
			ND_PRINT(" (bogus)");
			skip = 1;
		}
		if (skip) {
			/*
			 * alen >= OF_ACTION_MINLEN
			 * cp is 4 bytes in
			 */
			OF_CHK_FWD(alen - 4);
			continue;
		}
		/* OK to decode the rest of the action structure */
		switch (type) {
		case OFPAT_OUTPUT:
			/* port */
			output_port = GET_BE_U_2(cp);
			OF_FWD(2);
			ND_PRINT(", port %s", tok2str(ofpp_str, "%u", output_port));
			/* max_len */
			if (output_port == OFPP_CONTROLLER)
				ND_PRINT(", max_len %u", GET_BE_U_2(cp));
			else
				ND_TCHECK_2(cp);
			OF_FWD(2);
			break;
		case OFPAT_SET_VLAN_VID:
			/* vlan_vid */
			ND_PRINT(", vlan_vid %s", vlan_str(GET_BE_U_2(cp)));
			OF_FWD(2);
			/* pad */
			/* Sometimes the last field, check bounds. */
			OF_CHK_FWD(2);
			break;
		case OFPAT_SET_VLAN_PCP:
			/* vlan_pcp */
			ND_PRINT(", vlan_pcp %s", pcp_str(GET_U_1(cp)));
			OF_FWD(1);
			/* pad */
			/* Sometimes the last field, check bounds. */
			OF_CHK_FWD(3);
			break;
		case OFPAT_SET_DL_SRC:
		case OFPAT_SET_DL_DST:
			/* dl_addr */
			ND_PRINT(", dl_addr %s", GET_ETHERADDR_STRING(cp));
			OF_FWD(MAC_ADDR_LEN);
			/* pad */
			/* Sometimes the last field, check bounds. */
			OF_CHK_FWD(6);
			break;
		case OFPAT_SET_NW_SRC:
		case OFPAT_SET_NW_DST:
			/* nw_addr */
			ND_PRINT(", nw_addr %s", GET_IPADDR_STRING(cp));
			OF_FWD(4);
			break;
		case OFPAT_SET_NW_TOS:
			/* nw_tos */
			ND_PRINT(", nw_tos 0x%02x", GET_U_1(cp));
			OF_FWD(1);
			/* pad */
			/* Sometimes the last field, check bounds. */
			OF_CHK_FWD(3);
			break;
		case OFPAT_SET_TP_SRC:
		case OFPAT_SET_TP_DST:
			/* nw_tos */
			ND_PRINT(", tp_port %u", GET_BE_U_2(cp));
			OF_FWD(2);
			/* pad */
			/* Sometimes the last field, check bounds. */
			OF_CHK_FWD(2);
			break;
		case OFPAT_ENQUEUE:
			/* port */
			ND_PRINT(", port %s",
				 tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
			OF_FWD(2);
			/* pad */
			OF_FWD(6);
			/* queue_id */
			ND_PRINT(", queue_id %s",
				 tok2str(ofpq_str, "%u", GET_BE_U_4(cp)));
			OF_FWD(4);
			break;
		case OFPAT_VENDOR:
			of10_vendor_action_print(ndo, cp, alen - 4);
			OF_FWD(alen - 4);
			break;
		case OFPAT_STRIP_VLAN:
			/* pad */
			/* Sometimes the last field, check bounds. */
			OF_CHK_FWD(4);
			break;
		} /* switch */
	} /* while */
	return;

invalid: /* skip the rest of actions */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* [OF10] Section 5.3.1 */
static void
of10_features_reply_print(netdissect_options *ndo,
                          const u_char *cp, u_int len)
{
	/* datapath_id */
	ND_PRINT("\n\t dpid 0x%016" PRIx64, GET_BE_U_8(cp));
	OF_FWD(8);
	/* n_buffers */
	ND_PRINT(", n_buffers %u", GET_BE_U_4(cp));
	OF_FWD(4);
	/* n_tables */
	ND_PRINT(", n_tables %u", GET_U_1(cp));
	OF_FWD(1);
	/* pad */
	OF_FWD(3);
	/* capabilities */
	ND_PRINT("\n\t capabilities 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofp_capabilities_bm, GET_BE_U_4(cp), OFPCAP_U);
	OF_FWD(4);
	/* actions */
	ND_PRINT("\n\t actions 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofpat_bm, GET_BE_U_4(cp), OFPAT_U);
	OF_FWD(4);
	/* ports */
	while (len) {
		if (len < OF_PHY_PORT_FIXLEN)
			goto invalid;
		of10_phy_port_print(ndo, cp);
		OF_FWD(OF_PHY_PORT_FIXLEN);
	}
	return;

invalid: /* skip the undersized trailing data */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* [OF10] Section 5.3.2 */
static void
of10_switch_config_msg_print(netdissect_options *ndo,
                             const u_char *cp, u_int len _U_)
{
	/* flags */
	ND_PRINT("\n\t flags %s",
	         tok2str(ofp_config_str, "invalid (0x%04x)", GET_BE_U_2(cp)));
	cp += 2;
	/* miss_send_len */
	ND_PRINT(", miss_send_len %u", GET_BE_U_2(cp));
}

/* [OF10] Section 5.3.3 */
static void
of10_flow_mod_print(netdissect_options *ndo,
                    const u_char *cp, u_int len)
{
	uint16_t command;

	/* match */
	of10_match_print(ndo, "\n\t ", cp);
	OF_FWD(OF_MATCH_FIXLEN);
	/* cookie */
	ND_PRINT("\n\t cookie 0x%016" PRIx64, GET_BE_U_8(cp));
	OF_FWD(8);
	/* command */
	command = GET_BE_U_2(cp);
	ND_PRINT(", command %s", tok2str(ofpfc_str, "invalid (0x%04x)", command));
	OF_FWD(2);
	/* idle_timeout */
	if (GET_BE_U_2(cp))
		ND_PRINT(", idle_timeout %u", GET_BE_U_2(cp));
	OF_FWD(2);
	/* hard_timeout */
	if (GET_BE_U_2(cp))
		ND_PRINT(", hard_timeout %u", GET_BE_U_2(cp));
	OF_FWD(2);
	/* priority */
	if (GET_BE_U_2(cp))
		ND_PRINT(", priority %u", GET_BE_U_2(cp));
	OF_FWD(2);
	/* buffer_id */
	if (command == OFPFC_ADD || command == OFPFC_MODIFY ||
	    command == OFPFC_MODIFY_STRICT)
		ND_PRINT(", buffer_id %s",
		         tok2str(bufferid_str, "0x%08x", GET_BE_U_4(cp)));
	OF_FWD(4);
	/* out_port */
	if (command == OFPFC_DELETE || command == OFPFC_DELETE_STRICT)
		ND_PRINT(", out_port %s",
		         tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	OF_FWD(2);
	/* flags */
	ND_PRINT(", flags 0x%04x", GET_BE_U_2(cp));
	of_bitmap_print(ndo, ofpff_bm, GET_BE_U_2(cp), OFPFF_U);
	OF_FWD(2);
	/* actions */
	of10_actions_print(ndo, "\n\t ", cp, len);
}

/* ibid */
static void
of10_port_mod_print(netdissect_options *ndo,
                    const u_char *cp, u_int len _U_)
{
	/* port_no */
	ND_PRINT("\n\t port_no %s", tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	cp += 2;
	/* hw_addr */
	ND_PRINT(", hw_addr %s", GET_ETHERADDR_STRING(cp));
	cp += MAC_ADDR_LEN;
	/* config */
	ND_PRINT("\n\t config 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppc_bm, GET_BE_U_4(cp), OFPPC_U);
	cp += 4;
	/* mask */
	ND_PRINT("\n\t mask 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppc_bm, GET_BE_U_4(cp), OFPPC_U);
	cp += 4;
	/* advertise */
	ND_PRINT("\n\t advertise 0x%08x", GET_BE_U_4(cp));
	of_bitmap_print(ndo, ofppf_bm, GET_BE_U_4(cp), OFPPF_U);
	cp += 4;
	/* pad */
	/* Always the last field, check bounds. */
	ND_TCHECK_4(cp);
}

/* [OF10] Section 5.3.4 */
static void
of10_queue_get_config_request_print(netdissect_options *ndo,
                                    const u_char *cp, u_int len _U_)
{
	/* port */
	ND_PRINT("\n\t port %s", tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	cp += 2;
	/* pad */
	/* Always the last field, check bounds. */
	ND_TCHECK_2(cp);
}

/* ibid */
static void
of10_queue_get_config_reply_print(netdissect_options *ndo,
                                  const u_char *cp, u_int len)
{
	/* port */
	ND_PRINT("\n\t port %s", tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	OF_FWD(2);
	/* pad */
	/* Sometimes the last field, check bounds. */
	OF_CHK_FWD(6);
	/* queues */
	of10_queues_print(ndo, cp, len);
}

/* [OF10] Section 5.3.5 */
static void
of10_stats_request_print(netdissect_options *ndo,
                         const u_char *cp, u_int len)
{
	uint16_t type;

	/* type */
	type = GET_BE_U_2(cp);
	OF_FWD(2);
	ND_PRINT("\n\t type %s", tok2str(ofpst_str, "invalid (0x%04x)", type));
	/* flags */
	ND_PRINT(", flags 0x%04x", GET_BE_U_2(cp));
	if (GET_BE_U_2(cp))
		ND_PRINT(" (bogus)");
	OF_FWD(2);
	/* type-specific body of one of fixed lengths */
	switch(type) {
	case OFPST_DESC:
	case OFPST_TABLE:
		if (len)
			goto invalid;
		return;
	case OFPST_FLOW:
	case OFPST_AGGREGATE:
		if (len != OF_FLOW_STATS_REQUEST_FIXLEN)
			goto invalid;
		/* match */
		of10_match_print(ndo, "\n\t ", cp);
		OF_FWD(OF_MATCH_FIXLEN);
		/* table_id */
		ND_PRINT("\n\t table_id %s",
			 tok2str(tableid_str, "%u", GET_U_1(cp)));
		OF_FWD(1);
		/* pad */
		OF_FWD(1);
		/* out_port */
		ND_PRINT(", out_port %s",
			 tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
		return;
	case OFPST_PORT:
		if (len != OF_PORT_STATS_REQUEST_FIXLEN)
			goto invalid;
		/* port_no */
		ND_PRINT("\n\t port_no %s",
			 tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
		OF_FWD(2);
		/* pad */
		/* Always the last field, check bounds. */
		OF_CHK_FWD(6);
		return;
	case OFPST_QUEUE:
		if (len != OF_QUEUE_STATS_REQUEST_FIXLEN)
			goto invalid;
		/* port_no */
		ND_PRINT("\n\t port_no %s",
			 tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
		OF_FWD(2);
		/* pad */
		OF_FWD(2);
		/* queue_id */
		ND_PRINT(", queue_id %s",
			 tok2str(ofpq_str, "%u", GET_BE_U_4(cp)));
		return;
	case OFPST_VENDOR:
		of10_vendor_data_print(ndo, cp, len);
		return;
	}
	return;

invalid: /* skip the message body */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_desc_stats_reply_print(netdissect_options *ndo,
                            const u_char *cp, u_int len)
{
	if (len != OF_DESC_STATS_REPLY_FIXLEN)
		goto invalid;
	/* mfr_desc */
	ND_PRINT("\n\t  mfr_desc '");
	nd_printjnp(ndo, cp, DESC_STR_LEN);
	ND_PRINT("'");
	OF_FWD(DESC_STR_LEN);
	/* hw_desc */
	ND_PRINT("\n\t  hw_desc '");
	nd_printjnp(ndo, cp, DESC_STR_LEN);
	ND_PRINT("'");
	OF_FWD(DESC_STR_LEN);
	/* sw_desc */
	ND_PRINT("\n\t  sw_desc '");
	nd_printjnp(ndo, cp, DESC_STR_LEN);
	ND_PRINT("'");
	OF_FWD(DESC_STR_LEN);
	/* serial_num */
	ND_PRINT("\n\t  serial_num '");
	nd_printjnp(ndo, cp, SERIAL_NUM_LEN);
	ND_PRINT("'");
	OF_FWD(SERIAL_NUM_LEN);
	/* dp_desc */
	ND_PRINT("\n\t  dp_desc '");
	nd_printjnp(ndo, cp, DESC_STR_LEN);
	ND_PRINT("'");
	return;

invalid: /* skip the message body */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_flow_stats_reply_print(netdissect_options *ndo,
                            const u_char *cp, u_int len)
{
	while (len) {
		uint16_t entry_len;

		if (len < OF_FLOW_STATS_REPLY_MINLEN)
			goto invalid;
		/* length */
		entry_len = GET_BE_U_2(cp);
		ND_PRINT("\n\t length %u", entry_len);
		if (entry_len < OF_FLOW_STATS_REPLY_MINLEN || entry_len > len)
			goto invalid;
		OF_FWD(2);
		/* table_id */
		ND_PRINT(", table_id %s",
			 tok2str(tableid_str, "%u", GET_U_1(cp)));
		OF_FWD(1);
		/* pad */
		OF_FWD(1);
		/* match */
		of10_match_print(ndo, "\n\t  ", cp);
		OF_FWD(OF_MATCH_FIXLEN);
		/* duration_sec */
		ND_PRINT("\n\t  duration_sec %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* duration_nsec */
		ND_PRINT(", duration_nsec %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* priority */
		ND_PRINT(", priority %u", GET_BE_U_2(cp));
		OF_FWD(2);
		/* idle_timeout */
		ND_PRINT(", idle_timeout %u", GET_BE_U_2(cp));
		OF_FWD(2);
		/* hard_timeout */
		ND_PRINT(", hard_timeout %u", GET_BE_U_2(cp));
		OF_FWD(2);
		/* pad2 */
		OF_FWD(6);
		/* cookie */
		ND_PRINT(", cookie 0x%016" PRIx64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* packet_count */
		ND_PRINT(", packet_count %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* byte_count */
		ND_PRINT(", byte_count %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* actions */
		of10_actions_print(ndo, "\n\t  ", cp, entry_len - OF_FLOW_STATS_REPLY_MINLEN);
		OF_FWD(entry_len - OF_FLOW_STATS_REPLY_MINLEN);
	} /* while */
	return;

invalid: /* skip the rest of flow statistics entries */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_aggregate_stats_reply_print(netdissect_options *ndo,
                                 const u_char *cp, u_int len)
{
	if (len != OF_AGGREGATE_STATS_REPLY_FIXLEN)
		goto invalid;
	/* packet_count */
	ND_PRINT("\n\t packet_count %" PRIu64, GET_BE_U_8(cp));
	OF_FWD(8);
	/* byte_count */
	ND_PRINT(", byte_count %" PRIu64, GET_BE_U_8(cp));
	OF_FWD(8);
	/* flow_count */
	ND_PRINT(", flow_count %u", GET_BE_U_4(cp));
	OF_FWD(4);
	/* pad */
	/* Always the last field, check bounds. */
	ND_TCHECK_4(cp);
	return;

invalid: /* skip the message body */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_table_stats_reply_print(netdissect_options *ndo,
                             const u_char *cp, u_int len)
{
	while (len) {
		if (len < OF_TABLE_STATS_REPLY_FIXLEN)
			goto invalid;
		/* table_id */
		ND_PRINT("\n\t table_id %s",
		         tok2str(tableid_str, "%u", GET_U_1(cp)));
		OF_FWD(1);
		/* pad */
		OF_FWD(3);
		/* name */
		ND_PRINT(", name '");
		nd_printjnp(ndo, cp, OFP_MAX_TABLE_NAME_LEN);
		ND_PRINT("'");
		OF_FWD(OFP_MAX_TABLE_NAME_LEN);
		/* wildcards */
		ND_PRINT("\n\t  wildcards 0x%08x", GET_BE_U_4(cp));
		of_bitmap_print(ndo, ofpfw_bm, GET_BE_U_4(cp), OFPFW_U);
		OF_FWD(4);
		/* max_entries */
		ND_PRINT("\n\t  max_entries %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* active_count */
		ND_PRINT(", active_count %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* lookup_count */
		ND_PRINT(", lookup_count %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* matched_count */
		ND_PRINT(", matched_count %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
	} /* while */
	return;

invalid: /* skip the undersized trailing data */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_port_stats_reply_print(netdissect_options *ndo,
                            const u_char *cp, u_int len)
{
	while (len) {
		if (len < OF_PORT_STATS_REPLY_FIXLEN)
			goto invalid;
		/* port_no */
		ND_PRINT("\n\t  port_no %s",
			 tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
		OF_FWD(2);
		if (ndo->ndo_vflag < 2) {
			OF_CHK_FWD(OF_PORT_STATS_REPLY_FIXLEN - 2);
			continue;
		}
		/* pad */
		OF_FWD(6);
		/* rx_packets */
		ND_PRINT(", rx_packets %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* tx_packets */
		ND_PRINT(", tx_packets %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* rx_bytes */
		ND_PRINT(", rx_bytes %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* tx_bytes */
		ND_PRINT(", tx_bytes %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* rx_dropped */
		ND_PRINT(", rx_dropped %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* tx_dropped */
		ND_PRINT(", tx_dropped %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* rx_errors */
		ND_PRINT(", rx_errors %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* tx_errors */
		ND_PRINT(", tx_errors %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* rx_frame_err */
		ND_PRINT(", rx_frame_err %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* rx_over_err */
		ND_PRINT(", rx_over_err %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* rx_crc_err */
		ND_PRINT(", rx_crc_err %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* collisions */
		ND_PRINT(", collisions %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
	} /* while */
	return;

invalid: /* skip the undersized trailing data */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_queue_stats_reply_print(netdissect_options *ndo,
                             const u_char *cp, u_int len)
{
	while (len) {
		if (len < OF_QUEUE_STATS_REPLY_FIXLEN)
			goto invalid;
		/* port_no */
		ND_PRINT("\n\t  port_no %s",
		         tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
		OF_FWD(2);
		/* pad */
		OF_FWD(2);
		/* queue_id */
		ND_PRINT(", queue_id %u", GET_BE_U_4(cp));
		OF_FWD(4);
		/* tx_bytes */
		ND_PRINT(", tx_bytes %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* tx_packets */
		ND_PRINT(", tx_packets %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
		/* tx_errors */
		ND_PRINT(", tx_errors %" PRIu64, GET_BE_U_8(cp));
		OF_FWD(8);
	} /* while */
	return;

invalid: /* skip the undersized trailing data */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* ibid */
static void
of10_stats_reply_print(netdissect_options *ndo,
                       const u_char *cp, u_int len)
{
	uint16_t type;

	/* type */
	type = GET_BE_U_2(cp);
	ND_PRINT("\n\t type %s", tok2str(ofpst_str, "invalid (0x%04x)", type));
	OF_FWD(2);
	/* flags */
	ND_PRINT(", flags 0x%04x", GET_BE_U_2(cp));
	of_bitmap_print(ndo, ofpsf_reply_bm, GET_BE_U_2(cp), OFPSF_REPLY_U);
	OF_FWD(2);

	if (ndo->ndo_vflag > 0) {
		void (*decoder)(netdissect_options *, const u_char *, u_int) =
			type == OFPST_DESC      ? of10_desc_stats_reply_print      :
			type == OFPST_FLOW      ? of10_flow_stats_reply_print      :
			type == OFPST_AGGREGATE ? of10_aggregate_stats_reply_print :
			type == OFPST_TABLE     ? of10_table_stats_reply_print     :
			type == OFPST_PORT      ? of10_port_stats_reply_print      :
			type == OFPST_QUEUE     ? of10_queue_stats_reply_print     :
			type == OFPST_VENDOR    ? of10_vendor_data_print           :
			NULL;
		if (decoder != NULL) {
			decoder(ndo, cp, len);
			return;
		}
	}
	ND_TCHECK_LEN(cp, len);
}

/* [OF10] Section 5.3.6 */
static void
of10_packet_out_print(netdissect_options *ndo,
                      const u_char *cp, u_int len)
{
	uint16_t actions_len;

	/* buffer_id */
	ND_PRINT("\n\t buffer_id 0x%08x", GET_BE_U_4(cp));
	OF_FWD(4);
	/* in_port */
	ND_PRINT(", in_port %s", tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	OF_FWD(2);
	/* actions_len */
	actions_len = GET_BE_U_2(cp);
	OF_FWD(2);
	if (actions_len > len)
		goto invalid;
	/* actions */
	of10_actions_print(ndo, "\n\t ", cp, actions_len);
	OF_FWD(actions_len);
	/* data */
	of10_packet_data_print(ndo, cp, len);
	return;

invalid: /* skip the rest of the message body */
	nd_print_invalid(ndo);
	ND_TCHECK_LEN(cp, len);
}

/* [OF10] Section 5.4.1 */
static void
of10_packet_in_print(netdissect_options *ndo,
                     const u_char *cp, u_int len)
{
	/* buffer_id */
	ND_PRINT("\n\t buffer_id %s",
	         tok2str(bufferid_str, "0x%08x", GET_BE_U_4(cp)));
	OF_FWD(4);
	/* total_len */
	ND_PRINT(", total_len %u", GET_BE_U_2(cp));
	OF_FWD(2);
	/* in_port */
	ND_PRINT(", in_port %s", tok2str(ofpp_str, "%u", GET_BE_U_2(cp)));
	OF_FWD(2);
	/* reason */
	ND_PRINT(", reason %s",
		 tok2str(ofpr_str, "invalid (0x%02x)", GET_U_1(cp)));
	OF_FWD(1);
	/* pad */
	/* Sometimes the last field, check bounds. */
	OF_CHK_FWD(1);
	/* data */
	of10_packet_data_print(ndo, cp, len);
}

/* [OF10] Section 5.4.2 */
static void
of10_flow_removed_print(netdissect_options *ndo,
                        const u_char *cp, u_int len _U_)
{
	/* match */
	of10_match_print(ndo, "\n\t ", cp);
	cp += OF_MATCH_FIXLEN;
	/* cookie */
	ND_PRINT("\n\t cookie 0x%016" PRIx64, GET_BE_U_8(cp));
	cp += 8;
	/* priority */
	if (GET_BE_U_2(cp))
		ND_PRINT(", priority %u", GET_BE_U_2(cp));
	cp += 2;
	/* reason */
	ND_PRINT(", reason %s",
	         tok2str(ofprr_str, "unknown (0x%02x)", GET_U_1(cp)));
	cp += 1;
	/* pad */
	cp += 1;
	/* duration_sec */
	ND_PRINT(", duration_sec %u", GET_BE_U_4(cp));
	cp += 4;
	/* duration_nsec */
	ND_PRINT(", duration_nsec %u", GET_BE_U_4(cp));
	cp += 4;
	/* idle_timeout */
	if (GET_BE_U_2(cp))
		ND_PRINT(", idle_timeout %u", GET_BE_U_2(cp));
	cp += 2;
	/* pad2 */
	cp += 2;
	/* packet_count */
	ND_PRINT(", packet_count %" PRIu64, GET_BE_U_8(cp));
	cp += 8;
	/* byte_count */
	ND_PRINT(", byte_count %" PRIu64, GET_BE_U_8(cp));
}

/* [OF10] Section 5.4.3 */
static void
of10_port_status_print(netdissect_options *ndo,
                       const u_char *cp, u_int len _U_)
{
	/* reason */
	ND_PRINT("\n\t reason %s",
	         tok2str(ofppr_str, "invalid (0x%02x)", GET_U_1(cp)));
	cp += 1;
	/* pad */
	/* No need to check bounds, more data follows. */
	cp += 7;
	/* desc */
	of10_phy_port_print(ndo, cp);
}

/* [OF10] Section 5.4.4 */
static void
of10_error_print(netdissect_options *ndo,
                 const u_char *cp, u_int len)
{
	uint16_t type, code;
	const struct tok *code_str;

	/* type */
	type = GET_BE_U_2(cp);
	OF_FWD(2);
	ND_PRINT("\n\t type %s", tok2str(ofpet_str, "invalid (0x%04x)", type));
	/* code */
	code = GET_BE_U_2(cp);
	OF_FWD(2);
	code_str = uint2tokary(of10_ofpet2tokary, type);
	if (code_str != NULL)
		ND_PRINT(", code %s",
		         tok2str(code_str, "invalid (0x%04x)", code));
	else
		ND_PRINT(", code invalid (0x%04x)", code);
	/* data */
	of_data_print(ndo, cp, len);
}

static const struct of_msgtypeinfo of10_msgtypeinfo[OFPT_MAX + 1] = {
	/*
	 * [OF10] Section 5.5.1
	 * Variable-size data.
	 */
	{
		"HELLO",                    of_data_print,
		REQ_MINLEN,                 0
	},
	/*
	 * [OF10] Section 5.4.4
	 * A fixed-size message body and variable-size data.
	 */
	{
		"ERROR",                    of10_error_print,
		REQ_MINLEN,                 OF_ERROR_MSG_MINLEN
	},
	/*
	 * [OF10] Section 5.5.2
	 * Variable-size data.
	 */
	{
		"ECHO_REQUEST",             of_data_print,
		REQ_MINLEN,                 0
	},
	/*
	 * [OF10] Section 5.5.3
	 * Variable-size data.
	 */
	{
		"ECHO_REPLY",               of_data_print,
		REQ_MINLEN,                 0
	},
	/*
	 * [OF10] Section 5.5.4
	 * A fixed-size message body and variable-size data.
	 */
	{
		"VENDOR",                   of10_vendor_message_print,
		REQ_MINLEN,                 OF_VENDOR_MINLEN
	},
	/*
	 * [OF10] Section 5.3.1
	 * No message body.
	 */
	{
		"FEATURES_REQUEST",         NULL,
		REQ_FIXLEN,                 0
	},
	/*
	 * [OF10] Section 5.3.1
	 * A fixed-size message body and n * fixed-size data units.
	 */
	{
		"FEATURES_REPLY",           of10_features_reply_print,
		REQ_MINLEN,                 OF_FEATURES_REPLY_MINLEN
	},
	/*
	 * [OF10] Section 5.3.2
	 * No message body.
	 */
	{
		"GET_CONFIG_REQUEST",       NULL,
		REQ_FIXLEN,                 0
	},
	/*
	 * [OF10] Section 5.3.2
	 * A fixed-size message body.
	 */
	{
		"GET_CONFIG_REPLY",         of10_switch_config_msg_print,
		REQ_FIXLEN,                 OF_SWITCH_CONFIG_FIXLEN
	},
	/*
	 * [OF10] Section 5.3.2
	 * A fixed-size message body.
	 */
	{
		"SET_CONFIG",               of10_switch_config_msg_print,
		REQ_FIXLEN,                 OF_SWITCH_CONFIG_FIXLEN
	},
	/*
	 * [OF10] Section 5.4.1
	 * A fixed-size message body and variable-size data.
	 * (The 2 mock octets count in OF_PACKET_IN_MINLEN only.)
	 */
	{
		"PACKET_IN",                of10_packet_in_print,
		REQ_MINLEN,                 OF_PACKET_IN_MINLEN - 2
	},
	/*
	 * [OF10] Section 5.4.2
	 * A fixed-size message body.
	 */
	{
		"FLOW_REMOVED",             of10_flow_removed_print,
		REQ_FIXLEN,                 OF_FLOW_REMOVED_FIXLEN
	},
	/*
	 * [OF10] Section 5.4.3
	 * A fixed-size message body.
	 */
	{
		"PORT_STATUS",              of10_port_status_print,
		REQ_FIXLEN,                 OF_PORT_STATUS_FIXLEN
	},
	/*
	 * [OF10] Section 5.3.6
	 * A fixed-size message body, n * variable-size data units and
	 * variable-size data.
	 */
	{
		"PACKET_OUT",               of10_packet_out_print,
		REQ_MINLEN,                 OF_PACKET_OUT_MINLEN
	},
	/*
	 * [OF10] Section 5.3.3
	 * A fixed-size message body and n * variable-size data units.
	 */
	{
		"FLOW_MOD",                 of10_flow_mod_print,
		REQ_MINLEN,                 OF_FLOW_MOD_MINLEN
	},
	/*
	 * [OF10] Section 5.3.3
	 * A fixed-size message body.
	 */
	{
		"PORT_MOD",                 of10_port_mod_print,
		REQ_FIXLEN,                 OF_PORT_MOD_FIXLEN
	},
	/*
	 * [OF10] Section 5.3.5
	 * A fixed-size message body and possibly more data of varying size
	 * and structure.
	 */
	{
		"STATS_REQUEST",            of10_stats_request_print,
		REQ_MINLEN,                 OF_STATS_REQUEST_MINLEN
	},
	/*
	 * [OF10] Section 5.3.5
	 * A fixed-size message body and possibly more data of varying size
	 * and structure.
	 */
	{
		"STATS_REPLY",              of10_stats_reply_print,
		REQ_MINLEN,                 OF_STATS_REPLY_MINLEN
	},
	/*
	 * [OF10] Section 5.3.7
	 * No message body.
	 */
	{
		"BARRIER_REQUEST",          NULL,
		REQ_FIXLEN,                 0
	},
	/*
	 * [OF10] Section 5.3.7
	 * No message body.
	 */
	{
		"BARRIER_REPLY",            NULL,
		REQ_FIXLEN,                 0
	},
	/*
	 * [OF10] Section 5.3.4
	 * A fixed-size message body.
	 */
	{
		"QUEUE_GET_CONFIG_REQUEST", of10_queue_get_config_request_print,
		REQ_FIXLEN,                 OF_QUEUE_GET_CONFIG_REQUEST_FIXLEN
	},
	/*
	 * [OF10] Section 5.3.4
	 * A fixed-size message body and n * variable-size data units.
	 */
	{
		"QUEUE_GET_CONFIG_REPLY",   of10_queue_get_config_reply_print,
		REQ_MINLEN,                 OF_QUEUE_GET_CONFIG_REPLY_MINLEN
	},
};

const struct of_msgtypeinfo *
of10_identify_msgtype(const uint8_t type)
{
	return type <= OFPT_MAX ? &of10_msgtypeinfo[type] : NULL;
}
