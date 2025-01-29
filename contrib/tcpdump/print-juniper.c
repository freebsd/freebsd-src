/*     NetBSD: print-juniper.c,v 1.2 2007/07/24 11:53:45 drochner Exp        */

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

/* \summary: DLT_JUNIPER_* printers */

#ifndef lint
#else
__RCSID("NetBSD: print-juniper.c,v 1.3 2007/07/25 06:31:32 dogcow Exp ");
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"
#include "ppp.h"
#include "llc.h"
#include "nlpid.h"
#include "ethertype.h"
#include "atm.h"

/*
 * If none of the Juniper DLT_s are defined, there's nothing to do.
 */
#if defined(DLT_JUNIPER_GGSN) || defined(DLT_JUNIPER_ES) || \
    defined(DLT_JUNIPER_MONITOR) || defined(DLT_JUNIPER_SERVICES) || \
    defined(DLT_JUNIPER_PPPOE) || defined(DLT_JUNIPER_ETHER) || \
    defined(DLT_JUNIPER_PPP) || defined(DLT_JUNIPER_FRELAY) || \
    defined(DLT_JUNIPER_CHDLC) || defined(DLT_JUNIPER_PPPOE_ATM) || \
    defined(DLT_JUNIPER_MLPPP) || defined(DLT_JUNIPER_MFR) || \
    defined(DLT_JUNIPER_MLFR) || defined(DLT_JUNIPER_ATM1) || \
    defined(DLT_JUNIPER_ATM2)
#define JUNIPER_BPF_OUT           0       /* Outgoing packet */
#define JUNIPER_BPF_IN            1       /* Incoming packet */
#define JUNIPER_BPF_PKT_IN        0x1     /* Incoming packet */
#define JUNIPER_BPF_NO_L2         0x2     /* L2 header stripped */
#define JUNIPER_BPF_IIF           0x4     /* IIF is valid */
#define JUNIPER_BPF_FILTER        0x40    /* BPF filtering is supported */
#define JUNIPER_BPF_EXT           0x80    /* extensions present */
#define JUNIPER_MGC_NUMBER        0x4d4743 /* = "MGC" */

#define JUNIPER_LSQ_COOKIE_RE         (1 << 3)
#define JUNIPER_LSQ_COOKIE_DIR        (1 << 2)
#define JUNIPER_LSQ_L3_PROTO_SHIFT     4
#define JUNIPER_LSQ_L3_PROTO_MASK     (0x17 << JUNIPER_LSQ_L3_PROTO_SHIFT)
#define JUNIPER_LSQ_L3_PROTO_IPV4     (0 << JUNIPER_LSQ_L3_PROTO_SHIFT)
#define JUNIPER_LSQ_L3_PROTO_IPV6     (1 << JUNIPER_LSQ_L3_PROTO_SHIFT)
#define JUNIPER_LSQ_L3_PROTO_MPLS     (2 << JUNIPER_LSQ_L3_PROTO_SHIFT)
#define JUNIPER_LSQ_L3_PROTO_ISO      (3 << JUNIPER_LSQ_L3_PROTO_SHIFT)
#define AS_PIC_COOKIE_LEN 8

#define JUNIPER_IPSEC_O_ESP_ENCRYPT_ESP_AUTHEN_TYPE 1
#define JUNIPER_IPSEC_O_ESP_ENCRYPT_AH_AUTHEN_TYPE 2
#define JUNIPER_IPSEC_O_ESP_AUTHENTICATION_TYPE 3
#define JUNIPER_IPSEC_O_AH_AUTHENTICATION_TYPE 4
#define JUNIPER_IPSEC_O_ESP_ENCRYPTION_TYPE 5

#ifdef DLT_JUNIPER_ES
static const struct tok juniper_ipsec_type_values[] = {
    { JUNIPER_IPSEC_O_ESP_ENCRYPT_ESP_AUTHEN_TYPE, "ESP ENCR-AUTH" },
    { JUNIPER_IPSEC_O_ESP_ENCRYPT_AH_AUTHEN_TYPE, "ESP ENCR-AH AUTH" },
    { JUNIPER_IPSEC_O_ESP_AUTHENTICATION_TYPE, "ESP AUTH" },
    { JUNIPER_IPSEC_O_AH_AUTHENTICATION_TYPE, "AH AUTH" },
    { JUNIPER_IPSEC_O_ESP_ENCRYPTION_TYPE, "ESP ENCR" },
    { 0, NULL}
};
#endif

static const struct tok juniper_direction_values[] = {
    { JUNIPER_BPF_IN,  "In"},
    { JUNIPER_BPF_OUT, "Out"},
    { 0, NULL}
};

/* codepoints for encoding extensions to a .pcap file */
enum {
    JUNIPER_EXT_TLV_IFD_IDX = 1,
    JUNIPER_EXT_TLV_IFD_NAME = 2,
    JUNIPER_EXT_TLV_IFD_MEDIATYPE = 3,
    JUNIPER_EXT_TLV_IFL_IDX = 4,
    JUNIPER_EXT_TLV_IFL_UNIT = 5,
    JUNIPER_EXT_TLV_IFL_ENCAPS = 6,
    JUNIPER_EXT_TLV_TTP_IFD_MEDIATYPE = 7,
    JUNIPER_EXT_TLV_TTP_IFL_ENCAPS = 8
};

/* 1 byte type and 1-byte length */
#define JUNIPER_EXT_TLV_OVERHEAD 2U

static const struct tok jnx_ext_tlv_values[] = {
    { JUNIPER_EXT_TLV_IFD_IDX, "Device Interface Index" },
    { JUNIPER_EXT_TLV_IFD_NAME,"Device Interface Name" },
    { JUNIPER_EXT_TLV_IFD_MEDIATYPE, "Device Media Type" },
    { JUNIPER_EXT_TLV_IFL_IDX, "Logical Interface Index" },
    { JUNIPER_EXT_TLV_IFL_UNIT,"Logical Unit Number" },
    { JUNIPER_EXT_TLV_IFL_ENCAPS, "Logical Interface Encapsulation" },
    { JUNIPER_EXT_TLV_TTP_IFD_MEDIATYPE, "TTP derived Device Media Type" },
    { JUNIPER_EXT_TLV_TTP_IFL_ENCAPS, "TTP derived Logical Interface Encapsulation" },
    { 0, NULL }
};

static const struct tok jnx_flag_values[] = {
    { JUNIPER_BPF_EXT, "Ext" },
    { JUNIPER_BPF_FILTER, "Filter" },
    { JUNIPER_BPF_IIF, "IIF" },
    { JUNIPER_BPF_NO_L2, "no-L2" },
    { JUNIPER_BPF_PKT_IN, "In" },
    { 0, NULL }
};

#define JUNIPER_IFML_ETHER              1
#define JUNIPER_IFML_FDDI               2
#define JUNIPER_IFML_TOKENRING          3
#define JUNIPER_IFML_PPP                4
#define JUNIPER_IFML_FRAMERELAY         5
#define JUNIPER_IFML_CISCOHDLC          6
#define JUNIPER_IFML_SMDSDXI            7
#define JUNIPER_IFML_ATMPVC             8
#define JUNIPER_IFML_PPP_CCC            9
#define JUNIPER_IFML_FRAMERELAY_CCC     10
#define JUNIPER_IFML_IPIP               11
#define JUNIPER_IFML_GRE                12
#define JUNIPER_IFML_PIM                13
#define JUNIPER_IFML_PIMD               14
#define JUNIPER_IFML_CISCOHDLC_CCC      15
#define JUNIPER_IFML_VLAN_CCC           16
#define JUNIPER_IFML_MLPPP              17
#define JUNIPER_IFML_MLFR               18
#define JUNIPER_IFML_ML                 19
#define JUNIPER_IFML_LSI                20
#define JUNIPER_IFML_DFE                21
#define JUNIPER_IFML_ATM_CELLRELAY_CCC  22
#define JUNIPER_IFML_CRYPTO             23
#define JUNIPER_IFML_GGSN               24
#define JUNIPER_IFML_LSI_PPP            25
#define JUNIPER_IFML_LSI_CISCOHDLC      26
#define JUNIPER_IFML_PPP_TCC            27
#define JUNIPER_IFML_FRAMERELAY_TCC     28
#define JUNIPER_IFML_CISCOHDLC_TCC      29
#define JUNIPER_IFML_ETHERNET_CCC       30
#define JUNIPER_IFML_VT                 31
#define JUNIPER_IFML_EXTENDED_VLAN_CCC  32
#define JUNIPER_IFML_ETHER_OVER_ATM     33
#define JUNIPER_IFML_MONITOR            34
#define JUNIPER_IFML_ETHERNET_TCC       35
#define JUNIPER_IFML_VLAN_TCC           36
#define JUNIPER_IFML_EXTENDED_VLAN_TCC  37
#define JUNIPER_IFML_CONTROLLER         38
#define JUNIPER_IFML_MFR                39
#define JUNIPER_IFML_LS                 40
#define JUNIPER_IFML_ETHERNET_VPLS      41
#define JUNIPER_IFML_ETHERNET_VLAN_VPLS 42
#define JUNIPER_IFML_ETHERNET_EXTENDED_VLAN_VPLS 43
#define JUNIPER_IFML_LT                 44
#define JUNIPER_IFML_SERVICES           45
#define JUNIPER_IFML_ETHER_VPLS_OVER_ATM 46
#define JUNIPER_IFML_FR_PORT_CCC        47
#define JUNIPER_IFML_FRAMERELAY_EXT_CCC 48
#define JUNIPER_IFML_FRAMERELAY_EXT_TCC 49
#define JUNIPER_IFML_FRAMERELAY_FLEX    50
#define JUNIPER_IFML_GGSNI              51
#define JUNIPER_IFML_ETHERNET_FLEX      52
#define JUNIPER_IFML_COLLECTOR          53
#define JUNIPER_IFML_AGGREGATOR         54
#define JUNIPER_IFML_LAPD               55
#define JUNIPER_IFML_PPPOE              56
#define JUNIPER_IFML_PPP_SUBORDINATE    57
#define JUNIPER_IFML_CISCOHDLC_SUBORDINATE  58
#define JUNIPER_IFML_DFC                59
#define JUNIPER_IFML_PICPEER            60

static const struct tok juniper_ifmt_values[] = {
    { JUNIPER_IFML_ETHER, "Ethernet" },
    { JUNIPER_IFML_FDDI, "FDDI" },
    { JUNIPER_IFML_TOKENRING, "Token-Ring" },
    { JUNIPER_IFML_PPP, "PPP" },
    { JUNIPER_IFML_PPP_SUBORDINATE, "PPP-Subordinate" },
    { JUNIPER_IFML_FRAMERELAY, "Frame-Relay" },
    { JUNIPER_IFML_CISCOHDLC, "Cisco-HDLC" },
    { JUNIPER_IFML_SMDSDXI, "SMDS-DXI" },
    { JUNIPER_IFML_ATMPVC, "ATM-PVC" },
    { JUNIPER_IFML_PPP_CCC, "PPP-CCC" },
    { JUNIPER_IFML_FRAMERELAY_CCC, "Frame-Relay-CCC" },
    { JUNIPER_IFML_FRAMERELAY_EXT_CCC, "Extended FR-CCC" },
    { JUNIPER_IFML_IPIP, "IP-over-IP" },
    { JUNIPER_IFML_GRE, "GRE" },
    { JUNIPER_IFML_PIM, "PIM-Encapsulator" },
    { JUNIPER_IFML_PIMD, "PIM-Decapsulator" },
    { JUNIPER_IFML_CISCOHDLC_CCC, "Cisco-HDLC-CCC" },
    { JUNIPER_IFML_VLAN_CCC, "VLAN-CCC" },
    { JUNIPER_IFML_EXTENDED_VLAN_CCC, "Extended-VLAN-CCC" },
    { JUNIPER_IFML_MLPPP, "Multilink-PPP" },
    { JUNIPER_IFML_MLFR, "Multilink-FR" },
    { JUNIPER_IFML_MFR, "Multilink-FR-UNI-NNI" },
    { JUNIPER_IFML_ML, "Multilink" },
    { JUNIPER_IFML_LS, "LinkService" },
    { JUNIPER_IFML_LSI, "LSI" },
    { JUNIPER_IFML_ATM_CELLRELAY_CCC, "ATM-CCC-Cell-Relay" },
    { JUNIPER_IFML_CRYPTO, "IPSEC-over-IP" },
    { JUNIPER_IFML_GGSN, "GGSN" },
    { JUNIPER_IFML_PPP_TCC, "PPP-TCC" },
    { JUNIPER_IFML_FRAMERELAY_TCC, "Frame-Relay-TCC" },
    { JUNIPER_IFML_FRAMERELAY_EXT_TCC, "Extended FR-TCC" },
    { JUNIPER_IFML_CISCOHDLC_TCC, "Cisco-HDLC-TCC" },
    { JUNIPER_IFML_ETHERNET_CCC, "Ethernet-CCC" },
    { JUNIPER_IFML_VT, "VPN-Loopback-tunnel" },
    { JUNIPER_IFML_ETHER_OVER_ATM, "Ethernet-over-ATM" },
    { JUNIPER_IFML_ETHER_VPLS_OVER_ATM, "Ethernet-VPLS-over-ATM" },
    { JUNIPER_IFML_MONITOR, "Monitor" },
    { JUNIPER_IFML_ETHERNET_TCC, "Ethernet-TCC" },
    { JUNIPER_IFML_VLAN_TCC, "VLAN-TCC" },
    { JUNIPER_IFML_EXTENDED_VLAN_TCC, "Extended-VLAN-TCC" },
    { JUNIPER_IFML_CONTROLLER, "Controller" },
    { JUNIPER_IFML_ETHERNET_VPLS, "VPLS" },
    { JUNIPER_IFML_ETHERNET_VLAN_VPLS, "VLAN-VPLS" },
    { JUNIPER_IFML_ETHERNET_EXTENDED_VLAN_VPLS, "Extended-VLAN-VPLS" },
    { JUNIPER_IFML_LT, "Logical-tunnel" },
    { JUNIPER_IFML_SERVICES, "General-Services" },
    { JUNIPER_IFML_PPPOE, "PPPoE" },
    { JUNIPER_IFML_ETHERNET_FLEX, "Flexible-Ethernet-Services" },
    { JUNIPER_IFML_FRAMERELAY_FLEX, "Flexible-FrameRelay" },
    { JUNIPER_IFML_COLLECTOR, "Flow-collection" },
    { JUNIPER_IFML_PICPEER, "PIC Peer" },
    { JUNIPER_IFML_DFC, "Dynamic-Flow-Capture" },
    {0,                    NULL}
};

#define JUNIPER_IFLE_ATM_SNAP           2
#define JUNIPER_IFLE_ATM_NLPID          3
#define JUNIPER_IFLE_ATM_VCMUX          4
#define JUNIPER_IFLE_ATM_LLC            5
#define JUNIPER_IFLE_ATM_PPP_VCMUX      6
#define JUNIPER_IFLE_ATM_PPP_LLC        7
#define JUNIPER_IFLE_ATM_PPP_FUNI       8
#define JUNIPER_IFLE_ATM_CCC            9
#define JUNIPER_IFLE_FR_NLPID           10
#define JUNIPER_IFLE_FR_SNAP            11
#define JUNIPER_IFLE_FR_PPP             12
#define JUNIPER_IFLE_FR_CCC             13
#define JUNIPER_IFLE_ENET2              14
#define JUNIPER_IFLE_IEEE8023_SNAP      15
#define JUNIPER_IFLE_IEEE8023_LLC       16
#define JUNIPER_IFLE_PPP                17
#define JUNIPER_IFLE_CISCOHDLC          18
#define JUNIPER_IFLE_PPP_CCC            19
#define JUNIPER_IFLE_IPIP_NULL          20
#define JUNIPER_IFLE_PIM_NULL           21
#define JUNIPER_IFLE_GRE_NULL           22
#define JUNIPER_IFLE_GRE_PPP            23
#define JUNIPER_IFLE_PIMD_DECAPS        24
#define JUNIPER_IFLE_CISCOHDLC_CCC      25
#define JUNIPER_IFLE_ATM_CISCO_NLPID    26
#define JUNIPER_IFLE_VLAN_CCC           27
#define JUNIPER_IFLE_MLPPP              28
#define JUNIPER_IFLE_MLFR               29
#define JUNIPER_IFLE_LSI_NULL           30
#define JUNIPER_IFLE_AGGREGATE_UNUSED   31
#define JUNIPER_IFLE_ATM_CELLRELAY_CCC  32
#define JUNIPER_IFLE_CRYPTO             33
#define JUNIPER_IFLE_GGSN               34
#define JUNIPER_IFLE_ATM_TCC            35
#define JUNIPER_IFLE_FR_TCC             36
#define JUNIPER_IFLE_PPP_TCC            37
#define JUNIPER_IFLE_CISCOHDLC_TCC      38
#define JUNIPER_IFLE_ETHERNET_CCC       39
#define JUNIPER_IFLE_VT                 40
#define JUNIPER_IFLE_ATM_EOA_LLC        41
#define JUNIPER_IFLE_EXTENDED_VLAN_CCC          42
#define JUNIPER_IFLE_ATM_SNAP_TCC       43
#define JUNIPER_IFLE_MONITOR            44
#define JUNIPER_IFLE_ETHERNET_TCC       45
#define JUNIPER_IFLE_VLAN_TCC           46
#define JUNIPER_IFLE_EXTENDED_VLAN_TCC  47
#define JUNIPER_IFLE_MFR                48
#define JUNIPER_IFLE_ETHERNET_VPLS      49
#define JUNIPER_IFLE_ETHERNET_VLAN_VPLS 50
#define JUNIPER_IFLE_ETHERNET_EXTENDED_VLAN_VPLS 51
#define JUNIPER_IFLE_SERVICES           52
#define JUNIPER_IFLE_ATM_ETHER_VPLS_ATM_LLC                53
#define JUNIPER_IFLE_FR_PORT_CCC        54
#define JUNIPER_IFLE_ATM_MLPPP_LLC      55
#define JUNIPER_IFLE_ATM_EOA_CCC        56
#define JUNIPER_IFLE_LT_VLAN            57
#define JUNIPER_IFLE_COLLECTOR          58
#define JUNIPER_IFLE_AGGREGATOR         59
#define JUNIPER_IFLE_LAPD               60
#define JUNIPER_IFLE_ATM_PPPOE_LLC          61
#define JUNIPER_IFLE_ETHERNET_PPPOE         62
#define JUNIPER_IFLE_PPPOE                  63
#define JUNIPER_IFLE_PPP_SUBORDINATE        64
#define JUNIPER_IFLE_CISCOHDLC_SUBORDINATE  65
#define JUNIPER_IFLE_DFC                    66
#define JUNIPER_IFLE_PICPEER                67

static const struct tok juniper_ifle_values[] = {
    { JUNIPER_IFLE_AGGREGATOR, "Aggregator" },
    { JUNIPER_IFLE_ATM_CCC, "CCC over ATM" },
    { JUNIPER_IFLE_ATM_CELLRELAY_CCC, "ATM CCC Cell Relay" },
    { JUNIPER_IFLE_ATM_CISCO_NLPID, "CISCO compatible NLPID" },
    { JUNIPER_IFLE_ATM_EOA_CCC, "Ethernet over ATM CCC" },
    { JUNIPER_IFLE_ATM_EOA_LLC, "Ethernet over ATM LLC" },
    { JUNIPER_IFLE_ATM_ETHER_VPLS_ATM_LLC, "Ethernet VPLS over ATM LLC" },
    { JUNIPER_IFLE_ATM_LLC, "ATM LLC" },
    { JUNIPER_IFLE_ATM_MLPPP_LLC, "MLPPP over ATM LLC" },
    { JUNIPER_IFLE_ATM_NLPID, "ATM NLPID" },
    { JUNIPER_IFLE_ATM_PPPOE_LLC, "PPPoE over ATM LLC" },
    { JUNIPER_IFLE_ATM_PPP_FUNI, "PPP over FUNI" },
    { JUNIPER_IFLE_ATM_PPP_LLC, "PPP over ATM LLC" },
    { JUNIPER_IFLE_ATM_PPP_VCMUX, "PPP over ATM VCMUX" },
    { JUNIPER_IFLE_ATM_SNAP, "ATM SNAP" },
    { JUNIPER_IFLE_ATM_SNAP_TCC, "ATM SNAP TCC" },
    { JUNIPER_IFLE_ATM_TCC, "ATM VCMUX TCC" },
    { JUNIPER_IFLE_ATM_VCMUX, "ATM VCMUX" },
    { JUNIPER_IFLE_CISCOHDLC, "C-HDLC" },
    { JUNIPER_IFLE_CISCOHDLC_CCC, "C-HDLC CCC" },
    { JUNIPER_IFLE_CISCOHDLC_SUBORDINATE, "C-HDLC via dialer" },
    { JUNIPER_IFLE_CISCOHDLC_TCC, "C-HDLC TCC" },
    { JUNIPER_IFLE_COLLECTOR, "Collector" },
    { JUNIPER_IFLE_CRYPTO, "Crypto" },
    { JUNIPER_IFLE_ENET2, "Ethernet" },
    { JUNIPER_IFLE_ETHERNET_CCC, "Ethernet CCC" },
    { JUNIPER_IFLE_ETHERNET_EXTENDED_VLAN_VPLS, "Extended VLAN VPLS" },
    { JUNIPER_IFLE_ETHERNET_PPPOE, "PPPoE over Ethernet" },
    { JUNIPER_IFLE_ETHERNET_TCC, "Ethernet TCC" },
    { JUNIPER_IFLE_ETHERNET_VLAN_VPLS, "VLAN VPLS" },
    { JUNIPER_IFLE_ETHERNET_VPLS, "VPLS" },
    { JUNIPER_IFLE_EXTENDED_VLAN_CCC, "Extended VLAN CCC" },
    { JUNIPER_IFLE_EXTENDED_VLAN_TCC, "Extended VLAN TCC" },
    { JUNIPER_IFLE_FR_CCC, "FR CCC" },
    { JUNIPER_IFLE_FR_NLPID, "FR NLPID" },
    { JUNIPER_IFLE_FR_PORT_CCC, "FR CCC" },
    { JUNIPER_IFLE_FR_PPP, "FR PPP" },
    { JUNIPER_IFLE_FR_SNAP, "FR SNAP" },
    { JUNIPER_IFLE_FR_TCC, "FR TCC" },
    { JUNIPER_IFLE_GGSN, "GGSN" },
    { JUNIPER_IFLE_GRE_NULL, "GRE NULL" },
    { JUNIPER_IFLE_GRE_PPP, "PPP over GRE" },
    { JUNIPER_IFLE_IPIP_NULL, "IPIP" },
    { JUNIPER_IFLE_LAPD, "LAPD" },
    { JUNIPER_IFLE_LSI_NULL, "LSI Null" },
    { JUNIPER_IFLE_LT_VLAN, "LT VLAN" },
    { JUNIPER_IFLE_MFR, "MFR" },
    { JUNIPER_IFLE_MLFR, "MLFR" },
    { JUNIPER_IFLE_MLPPP, "MLPPP" },
    { JUNIPER_IFLE_MONITOR, "Monitor" },
    { JUNIPER_IFLE_PIMD_DECAPS, "PIMd" },
    { JUNIPER_IFLE_PIM_NULL, "PIM Null" },
    { JUNIPER_IFLE_PPP, "PPP" },
    { JUNIPER_IFLE_PPPOE, "PPPoE" },
    { JUNIPER_IFLE_PPP_CCC, "PPP CCC" },
    { JUNIPER_IFLE_PPP_SUBORDINATE, "" },
    { JUNIPER_IFLE_PPP_TCC, "PPP TCC" },
    { JUNIPER_IFLE_SERVICES, "General Services" },
    { JUNIPER_IFLE_VLAN_CCC, "VLAN CCC" },
    { JUNIPER_IFLE_VLAN_TCC, "VLAN TCC" },
    { JUNIPER_IFLE_VT, "VT" },
    {0,                    NULL}
};

struct juniper_cookie_table_t {
    uint32_t pictype;		/* pic type */
    uint8_t  cookie_len;	/* cookie len */
    const char *s;		/* pic name */
};

static const struct juniper_cookie_table_t juniper_cookie_table[] = {
#ifdef DLT_JUNIPER_ATM1
    { DLT_JUNIPER_ATM1,  4, "ATM1"},
#endif
#ifdef DLT_JUNIPER_ATM2
    { DLT_JUNIPER_ATM2,  8, "ATM2"},
#endif
#ifdef DLT_JUNIPER_MLPPP
    { DLT_JUNIPER_MLPPP, 2, "MLPPP"},
#endif
#ifdef DLT_JUNIPER_MLFR
    { DLT_JUNIPER_MLFR,  2, "MLFR"},
#endif
#ifdef DLT_JUNIPER_MFR
    { DLT_JUNIPER_MFR,   4, "MFR"},
#endif
#ifdef DLT_JUNIPER_PPPOE
    { DLT_JUNIPER_PPPOE, 0, "PPPoE"},
#endif
#ifdef DLT_JUNIPER_PPPOE_ATM
    { DLT_JUNIPER_PPPOE_ATM, 0, "PPPoE ATM"},
#endif
#ifdef DLT_JUNIPER_GGSN
    { DLT_JUNIPER_GGSN, 8, "GGSN"},
#endif
#ifdef DLT_JUNIPER_MONITOR
    { DLT_JUNIPER_MONITOR, 8, "MONITOR"},
#endif
#ifdef DLT_JUNIPER_SERVICES
    { DLT_JUNIPER_SERVICES, 8, "AS"},
#endif
#ifdef DLT_JUNIPER_ES
    { DLT_JUNIPER_ES, 0, "ES"},
#endif
    { 0, 0, NULL }
};

struct juniper_l2info_t {
    uint32_t length;
    uint32_t caplen;
    uint32_t pictype;
    uint8_t direction;
    u_int header_len;
    uint8_t cookie_len;
    uint8_t cookie_type;
    uint8_t cookie[8];
    u_int bundle;
    uint16_t proto;
    uint8_t flags;
};

#define LS_COOKIE_ID            0x54
#define AS_COOKIE_ID            0x47
#define LS_MLFR_COOKIE_LEN	4
#define ML_MLFR_COOKIE_LEN	2
#define LS_MFR_COOKIE_LEN	6
#define ATM1_COOKIE_LEN         4
#define ATM2_COOKIE_LEN         8

#define ATM2_PKT_TYPE_MASK  0x70
#define ATM2_GAP_COUNT_MASK 0x3F

#define JUNIPER_PROTO_NULL          1
#define JUNIPER_PROTO_IPV4          2
#define JUNIPER_PROTO_IPV6          6

#define MFR_BE_MASK 0xc0

#ifdef DLT_JUNIPER_GGSN
static const struct tok juniper_protocol_values[] = {
    { JUNIPER_PROTO_NULL, "Null" },
    { JUNIPER_PROTO_IPV4, "IPv4" },
    { JUNIPER_PROTO_IPV6, "IPv6" },
    { 0, NULL}
};
#endif

static int ip_heuristic_guess(netdissect_options *, const u_char *, u_int);
#ifdef DLT_JUNIPER_ATM2
static int juniper_ppp_heuristic_guess(netdissect_options *, const u_char *, u_int);
#endif
static int juniper_parse_header(netdissect_options *, const u_char *, const struct pcap_pkthdr *, struct juniper_l2info_t *);

#ifdef DLT_JUNIPER_GGSN
void
juniper_ggsn_if_print(netdissect_options *ndo,
                      const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;
        struct juniper_ggsn_header {
            nd_uint8_t  svc_id;
            nd_uint8_t  flags_len;
            nd_uint8_t  proto;
            nd_uint8_t  flags;
            nd_uint16_t vlan_id;
            nd_byte     res[2];
        };
        const struct juniper_ggsn_header *gh;
        uint8_t proto;

        ndo->ndo_protocol = "juniper_ggsn";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_GGSN;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        gh = (struct juniper_ggsn_header *)&l2info.cookie;

        /* use EXTRACT_, not GET_ (not packet buffer pointer) */
        proto = EXTRACT_U_1(gh->proto);
        if (ndo->ndo_eflag) {
            ND_PRINT("proto %s (%u), vlan %u: ",
                   tok2str(juniper_protocol_values,"Unknown",proto),
                   proto,
                   EXTRACT_BE_U_2(gh->vlan_id));
        }

        switch (proto) {
        case JUNIPER_PROTO_IPV4:
            ip_print(ndo, p, l2info.length);
            break;
        case JUNIPER_PROTO_IPV6:
            ip6_print(ndo, p, l2info.length);
            break;
        default:
            if (!ndo->ndo_eflag)
                ND_PRINT("unknown GGSN proto (%u)", proto);
        }

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_ES
void
juniper_es_if_print(netdissect_options *ndo,
                    const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;
        struct juniper_ipsec_header {
            nd_uint16_t sa_index;
            nd_uint8_t  ttl;
            nd_uint8_t  type;
            nd_uint32_t spi;
            nd_ipv4     src_ip;
            nd_ipv4     dst_ip;
        };
        u_int rewrite_len,es_type_bundle;
        const struct juniper_ipsec_header *ih;

        ndo->ndo_protocol = "juniper_es";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_ES;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        ih = (const struct juniper_ipsec_header *)p;

        ND_TCHECK_SIZE(ih);
        switch (GET_U_1(ih->type)) {
        case JUNIPER_IPSEC_O_ESP_ENCRYPT_ESP_AUTHEN_TYPE:
        case JUNIPER_IPSEC_O_ESP_ENCRYPT_AH_AUTHEN_TYPE:
            rewrite_len = 0;
            es_type_bundle = 1;
            break;
        case JUNIPER_IPSEC_O_ESP_AUTHENTICATION_TYPE:
        case JUNIPER_IPSEC_O_AH_AUTHENTICATION_TYPE:
        case JUNIPER_IPSEC_O_ESP_ENCRYPTION_TYPE:
            rewrite_len = 16;
            es_type_bundle = 0;
            break;
        default:
            ND_PRINT("ES Invalid type %u, length %u",
                   GET_U_1(ih->type),
                   l2info.length);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        l2info.length-=rewrite_len;
        p+=rewrite_len;

        if (ndo->ndo_eflag) {
            if (!es_type_bundle) {
                ND_PRINT("ES SA, index %u, ttl %u type %s (%u), spi %u, Tunnel %s > %s, length %u\n",
                       GET_BE_U_2(ih->sa_index),
                       GET_U_1(ih->ttl),
                       tok2str(juniper_ipsec_type_values,"Unknown",GET_U_1(ih->type)),
                       GET_U_1(ih->type),
                       GET_BE_U_4(ih->spi),
                       GET_IPADDR_STRING(ih->src_ip),
                       GET_IPADDR_STRING(ih->dst_ip),
                       l2info.length);
            } else {
                ND_PRINT("ES SA, index %u, ttl %u type %s (%u), length %u\n",
                       GET_BE_U_2(ih->sa_index),
                       GET_U_1(ih->ttl),
                       tok2str(juniper_ipsec_type_values,"Unknown",GET_U_1(ih->type)),
                       GET_U_1(ih->type),
                       l2info.length);
            }
        }

        ip_print(ndo, p, l2info.length);
        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_MONITOR
void
juniper_monitor_if_print(netdissect_options *ndo,
                         const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;
        struct juniper_monitor_header {
            nd_uint8_t  pkt_type;
            nd_byte     padding;
            nd_uint16_t iif;
            nd_uint32_t service_id;
        };
        const struct juniper_monitor_header *mh;

        ndo->ndo_protocol = "juniper_monitor";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_MONITOR;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        mh = (const struct juniper_monitor_header *)p;

        ND_TCHECK_SIZE(mh);
        if (ndo->ndo_eflag)
            ND_PRINT("service-id %u, iif %u, pkt-type %u: ",
                   GET_BE_U_4(mh->service_id),
                   GET_BE_U_2(mh->iif),
                   GET_U_1(mh->pkt_type));

        /* no proto field - lets guess by first byte of IP header*/
        ip_heuristic_guess (ndo, p, l2info.length);

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_SERVICES
void
juniper_services_if_print(netdissect_options *ndo,
                          const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;
        struct juniper_services_header {
            nd_uint8_t  svc_id;
            nd_uint8_t  flags_len;
            nd_uint16_t svc_set_id;
            nd_byte     pad;
            nd_uint24_t dir_iif;
        };
        const struct juniper_services_header *sh;

        ndo->ndo_protocol = "juniper_services";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_SERVICES;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        sh = (const struct juniper_services_header *)p;

        ND_TCHECK_SIZE(sh);
        if (ndo->ndo_eflag)
            ND_PRINT("service-id %u flags 0x%02x service-set-id 0x%04x iif %u: ",
                   GET_U_1(sh->svc_id),
                   GET_U_1(sh->flags_len),
                   GET_BE_U_2(sh->svc_set_id),
                   GET_BE_U_3(sh->dir_iif));

        /* no proto field - lets guess by first byte of IP header*/
        ip_heuristic_guess (ndo, p, l2info.length);

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_PPPOE
void
juniper_pppoe_if_print(netdissect_options *ndo,
                       const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_pppoe";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_PPPOE;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        /* this DLT contains nothing but raw ethernet frames */
        ether_print(ndo, p, l2info.length, l2info.caplen, NULL, NULL);
        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_ETHER
void
juniper_ether_if_print(netdissect_options *ndo,
                       const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_ether";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_ETHER;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        /* this DLT contains nothing but raw Ethernet frames */
        ndo->ndo_ll_hdr_len +=
		l2info.header_len +
		ether_print(ndo, p, l2info.length, l2info.caplen, NULL, NULL);
}
#endif

#ifdef DLT_JUNIPER_PPP
void
juniper_ppp_if_print(netdissect_options *ndo,
                     const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_ppp";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_PPP;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        /* this DLT contains nothing but raw ppp frames */
        ppp_print(ndo, p, l2info.length);
        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_FRELAY
void
juniper_frelay_if_print(netdissect_options *ndo,
                        const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_frelay";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_FRELAY;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        /* this DLT contains nothing but raw frame-relay frames */
        fr_print(ndo, p, l2info.length);
        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_CHDLC
void
juniper_chdlc_if_print(netdissect_options *ndo,
                       const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_chdlc";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_CHDLC;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;
        /* this DLT contains nothing but raw c-hdlc frames */
        chdlc_print(ndo, p, l2info.length);
        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_PPPOE_ATM
void
juniper_pppoe_atm_if_print(netdissect_options *ndo,
                           const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;
        uint16_t extracted_ethertype;

        ndo->ndo_protocol = "juniper_pppoe_atm";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_PPPOE_ATM;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;

        extracted_ethertype = GET_BE_U_2(p);
        /* this DLT contains nothing but raw PPPoE frames,
         * prepended with a type field*/
        if (ethertype_print(ndo, extracted_ethertype,
                              p+ETHERTYPE_LEN,
                              l2info.length-ETHERTYPE_LEN,
                              l2info.caplen-ETHERTYPE_LEN,
                              NULL, NULL) == 0)
            /* ether_type not known, probably it wasn't one */
            ND_PRINT("unknown ethertype 0x%04x", extracted_ethertype);

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_MLPPP
void
juniper_mlppp_if_print(netdissect_options *ndo,
                       const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_mlppp";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_MLPPP;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        /* suppress Bundle-ID if frame was captured on a child-link
         * best indicator if the cookie looks like a proto */
        if (ndo->ndo_eflag &&
            /* use EXTRACT_, not GET_ (not packet buffer pointer) */
            EXTRACT_BE_U_2(&l2info.cookie) != PPP_OSI &&
            /* use EXTRACT_, not GET_ (not packet buffer pointer) */
            EXTRACT_BE_U_2(&l2info.cookie) !=  (PPP_ADDRESS << 8 | PPP_CONTROL))
            ND_PRINT("Bundle-ID %u: ", l2info.bundle);

        p+=l2info.header_len;

        /* first try the LSQ protos */
        switch(l2info.proto) {
        case JUNIPER_LSQ_L3_PROTO_IPV4:
            /* IP traffic going to the RE would not have a cookie
             * -> this must be incoming IS-IS over PPP
             */
            if (l2info.cookie[4] == (JUNIPER_LSQ_COOKIE_RE|JUNIPER_LSQ_COOKIE_DIR))
                ppp_print(ndo, p, l2info.length);
            else
                ip_print(ndo, p, l2info.length);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        case JUNIPER_LSQ_L3_PROTO_IPV6:
            ip6_print(ndo, p,l2info.length);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        case JUNIPER_LSQ_L3_PROTO_MPLS:
            mpls_print(ndo, p, l2info.length);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        case JUNIPER_LSQ_L3_PROTO_ISO:
            isoclns_print(ndo, p, l2info.length);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        default:
            break;
        }

        /* zero length cookie ? */
        /* use EXTRACT_, not GET_ (not packet buffer pointer) */
        switch (EXTRACT_BE_U_2(&l2info.cookie)) {
        case PPP_OSI:
            ppp_print(ndo, p - 2, l2info.length + 2);
            break;
        case (PPP_ADDRESS << 8 | PPP_CONTROL): /* fall through */
        default:
            ppp_print(ndo, p, l2info.length);
            break;
        }

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif


#ifdef DLT_JUNIPER_MFR
void
juniper_mfr_if_print(netdissect_options *ndo,
                     const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_mfr";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_MFR;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;

        /* child-link ? */
        if (l2info.cookie_len == 0) {
            mfr_print(ndo, p, l2info.length);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        /* first try the LSQ protos */
        if (l2info.cookie_len == AS_PIC_COOKIE_LEN) {
            switch(l2info.proto) {
            case JUNIPER_LSQ_L3_PROTO_IPV4:
                ip_print(ndo, p, l2info.length);
                ndo->ndo_ll_hdr_len += l2info.header_len;
                return;
            case JUNIPER_LSQ_L3_PROTO_IPV6:
                ip6_print(ndo, p,l2info.length);
                ndo->ndo_ll_hdr_len += l2info.header_len;
                return;
            case JUNIPER_LSQ_L3_PROTO_MPLS:
                mpls_print(ndo, p, l2info.length);
                ndo->ndo_ll_hdr_len += l2info.header_len;
                return;
            case JUNIPER_LSQ_L3_PROTO_ISO:
                isoclns_print(ndo, p, l2info.length);
                ndo->ndo_ll_hdr_len += l2info.header_len;
                return;
            default:
                break;
            }
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        /* suppress Bundle-ID if frame was captured on a child-link */
        /* use EXTRACT_, not GET_ (not packet buffer pointer) */
        if (ndo->ndo_eflag && EXTRACT_BE_U_4(l2info.cookie) != 1)
            ND_PRINT("Bundle-ID %u, ", l2info.bundle);
        switch (l2info.proto) {
        case (LLCSAP_ISONS<<8 | LLCSAP_ISONS):
            /* At least one byte is required */
            ND_TCHECK_LEN(p, 1);
            isoclns_print(ndo, p + 1, l2info.length - 1);
            break;
        case (LLC_UI<<8 | NLPID_Q933):
        case (LLC_UI<<8 | NLPID_IP):
        case (LLC_UI<<8 | NLPID_IP6):
            /* pass IP{4,6} to the OSI layer for proper link-layer printing */
            isoclns_print(ndo, p - 1, l2info.length + 1);
            break;
        default:
            ND_PRINT("unknown protocol 0x%04x, length %u", l2info.proto, l2info.length);
        }

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

#ifdef DLT_JUNIPER_MLFR
void
juniper_mlfr_if_print(netdissect_options *ndo,
                      const struct pcap_pkthdr *h, const u_char *p)
{
        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_mlfr";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_MLFR;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;

        /* suppress Bundle-ID if frame was captured on a child-link */
        /* use EXTRACT_, not GET_ (not packet buffer pointer) */
        if (ndo->ndo_eflag && EXTRACT_BE_U_4(l2info.cookie) != 1)
            ND_PRINT("Bundle-ID %u, ", l2info.bundle);
        switch (l2info.proto) {
        case (LLC_UI):
        case (LLC_UI<<8):
            isoclns_print(ndo, p, l2info.length);
            break;
        case (LLC_UI<<8 | NLPID_Q933):
        case (LLC_UI<<8 | NLPID_IP):
        case (LLC_UI<<8 | NLPID_IP6):
            /* pass IP{4,6} to the OSI layer for proper link-layer printing */
            isoclns_print(ndo, p - 1, l2info.length + 1);
            break;
        default:
            ND_PRINT("unknown protocol 0x%04x, length %u", l2info.proto, l2info.length);
        }

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

/*
 *     ATM1 PIC cookie format
 *
 *     +-----+-------------------------+-------------------------------+
 *     |fmtid|     vc index            |  channel  ID                  |
 *     +-----+-------------------------+-------------------------------+
 */

#ifdef DLT_JUNIPER_ATM1
void
juniper_atm1_if_print(netdissect_options *ndo,
                      const struct pcap_pkthdr *h, const u_char *p)
{
        int llc_hdrlen;

        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_atm1";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_ATM1;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;

        if (l2info.cookie[0] == 0x80) { /* OAM cell ? */
            oam_print(ndo, p, l2info.length, ATM_OAM_NOHEC);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        if (GET_BE_U_3(p) == 0xfefe03 || /* NLPID encaps ? */
            GET_BE_U_3(p) == 0xaaaa03) { /* SNAP encaps ? */

            llc_hdrlen = llc_print(ndo, p, l2info.length, l2info.caplen, NULL, NULL);
            if (llc_hdrlen > 0) {
                ndo->ndo_ll_hdr_len += l2info.header_len;
                return;
            }
        }

        if (GET_U_1(p) == 0x03) { /* Cisco style NLPID encaps ? */
            /* At least one byte is required */
            ND_TCHECK_LEN(p, 1);
            isoclns_print(ndo, p + 1, l2info.length - 1);
            /* FIXME check if frame was recognized */
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        if (ip_heuristic_guess(ndo, p, l2info.length) != 0) { /* last try - vcmux encaps ? */
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        ndo->ndo_ll_hdr_len += l2info.header_len;
}
#endif

/*
 *     ATM2 PIC cookie format
 *
 *     +-------------------------------+---------+---+-----+-----------+
 *     |     channel ID                |  reserv |AAL| CCRQ| gap cnt   |
 *     +-------------------------------+---------+---+-----+-----------+
 */

#ifdef DLT_JUNIPER_ATM2
void
juniper_atm2_if_print(netdissect_options *ndo,
                      const struct pcap_pkthdr *h, const u_char *p)
{
        int llc_hdrlen;

        struct juniper_l2info_t l2info;

        ndo->ndo_protocol = "juniper_atm2";
        memset(&l2info, 0, sizeof(l2info));
        l2info.pictype = DLT_JUNIPER_ATM2;
        if (juniper_parse_header(ndo, p, h, &l2info) == 0) {
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        p+=l2info.header_len;

        if (l2info.cookie[7] & ATM2_PKT_TYPE_MASK) { /* OAM cell ? */
            oam_print(ndo, p, l2info.length, ATM_OAM_NOHEC);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        if (GET_BE_U_3(p) == 0xfefe03 || /* NLPID encaps ? */
            GET_BE_U_3(p) == 0xaaaa03) { /* SNAP encaps ? */

            llc_hdrlen = llc_print(ndo, p, l2info.length, l2info.caplen, NULL, NULL);
            if (llc_hdrlen > 0) {
                ndo->ndo_ll_hdr_len += l2info.header_len;
                return;
            }
        }

        if (l2info.direction != JUNIPER_BPF_PKT_IN && /* ether-over-1483 encaps ? */
            /* use EXTRACT_, not GET_ (not packet buffer pointer) */
            (EXTRACT_BE_U_4(l2info.cookie) & ATM2_GAP_COUNT_MASK)) {
            ether_print(ndo, p, l2info.length, l2info.caplen, NULL, NULL);
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        if (GET_U_1(p) == 0x03) { /* Cisco style NLPID encaps ? */
            /* At least one byte is required */
            ND_TCHECK_LEN(p, 1);
            isoclns_print(ndo, p + 1, l2info.length - 1);
            /* FIXME check if frame was recognized */
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        if(juniper_ppp_heuristic_guess(ndo, p, l2info.length) != 0) { /* PPPoA vcmux encaps ? */
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        if (ip_heuristic_guess(ndo, p, l2info.length) != 0) { /* last try - vcmux encaps ? */
            ndo->ndo_ll_hdr_len += l2info.header_len;
            return;
        }

        ndo->ndo_ll_hdr_len += l2info.header_len;
}

/* try to guess, based on all PPP protos that are supported in
 * a juniper router if the payload data is encapsulated using PPP */
static int
juniper_ppp_heuristic_guess(netdissect_options *ndo,
                            const u_char *p, u_int length)
{
    switch(GET_BE_U_2(p)) {
    case PPP_IP :
    case PPP_OSI :
    case PPP_MPLS_UCAST :
    case PPP_MPLS_MCAST :
    case PPP_IPCP :
    case PPP_OSICP :
    case PPP_MPLSCP :
    case PPP_LCP :
    case PPP_PAP :
    case PPP_CHAP :
    case PPP_ML :
    case PPP_IPV6 :
    case PPP_IPV6CP :
        ppp_print(ndo, p, length);
        break;

    default:
        return 0; /* did not find a ppp header */
        break;
    }
    return 1; /* we printed a ppp packet */
}
#endif

static int
ip_heuristic_guess(netdissect_options *ndo,
                   const u_char *p, u_int length)
{
    switch(GET_U_1(p)) {
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4a:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4e:
    case 0x4f:
        ip_print(ndo, p, length);
        break;
    case 0x60:
    case 0x61:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x65:
    case 0x66:
    case 0x67:
    case 0x68:
    case 0x69:
    case 0x6a:
    case 0x6b:
    case 0x6c:
    case 0x6d:
    case 0x6e:
    case 0x6f:
        ip6_print(ndo, p, length);
        break;
    default:
        return 0; /* did not find a ip header */
        break;
    }
    return 1; /* we printed an v4/v6 packet */
}

static int
juniper_read_tlv_value(netdissect_options *ndo,
		       const u_char *p, u_int tlv_type, u_int tlv_len)
{
   int tlv_value;

   /* TLVs < 128 are little endian encoded */
   if (tlv_type < 128) {
       switch (tlv_len) {
       case 1:
           tlv_value = GET_U_1(p);
           break;
       case 2:
           tlv_value = GET_LE_U_2(p);
           break;
       case 3:
           tlv_value = GET_LE_U_3(p);
           break;
       case 4:
           tlv_value = GET_LE_U_4(p);
           break;
       default:
           tlv_value = -1;
           break;
       }
   } else {
       /* TLVs >= 128 are big endian encoded */
       switch (tlv_len) {
       case 1:
           tlv_value = GET_U_1(p);
           break;
       case 2:
           tlv_value = GET_BE_U_2(p);
           break;
       case 3:
           tlv_value = GET_BE_U_3(p);
           break;
       case 4:
           tlv_value = GET_BE_U_4(p);
           break;
       default:
           tlv_value = -1;
           break;
       }
   }
   return tlv_value;
}

static int
juniper_parse_header(netdissect_options *ndo,
                     const u_char *p, const struct pcap_pkthdr *h, struct juniper_l2info_t *l2info)
{
    const struct juniper_cookie_table_t *lp;
    u_int idx, extension_length, jnx_header_len = 0;
    uint8_t tlv_type,tlv_len;
#ifdef DLT_JUNIPER_ATM2
    uint32_t control_word;
#endif
    int tlv_value;
    const u_char *tptr;


    l2info->header_len = 0;
    l2info->cookie_len = 0;
    l2info->proto = 0;


    l2info->length = h->len;
    l2info->caplen = h->caplen;
    l2info->flags = GET_U_1(p + 3);
    l2info->direction = GET_U_1(p + 3) & JUNIPER_BPF_PKT_IN;

    if (GET_BE_U_3(p) != JUNIPER_MGC_NUMBER) { /* magic number found ? */
        ND_PRINT("no magic-number found!");
        return 0;
    }

    if (ndo->ndo_eflag) /* print direction */
        ND_PRINT("%3s ", tok2str(juniper_direction_values, "---", l2info->direction));

    /* magic number + flags */
    jnx_header_len = 4;

    if (ndo->ndo_vflag > 1)
        ND_PRINT("\n\tJuniper PCAP Flags [%s]",
               bittok2str(jnx_flag_values, "none", l2info->flags));

    /* extensions present ?  - calculate how much bytes to skip */
    if ((l2info->flags & JUNIPER_BPF_EXT ) == JUNIPER_BPF_EXT ) {

        tptr = p+jnx_header_len;

        /* ok to read extension length ? */
        extension_length = GET_BE_U_2(tptr);
        jnx_header_len += 2;
        tptr +=2;

        /* nail up the total length -
         * just in case something goes wrong
         * with TLV parsing */
        jnx_header_len += extension_length;

        if (ndo->ndo_vflag > 1)
            ND_PRINT(", PCAP Extension(s) total length %u", extension_length);

        ND_TCHECK_LEN(tptr, extension_length);
        while (extension_length > JUNIPER_EXT_TLV_OVERHEAD) {
            tlv_type = GET_U_1(tptr);
            tptr++;
            tlv_len = GET_U_1(tptr);
            tptr++;
            tlv_value = 0;

            /* sanity checks */
            if (tlv_type == 0 || tlv_len == 0)
                break;
            ND_LCHECK_U(extension_length, tlv_len + JUNIPER_EXT_TLV_OVERHEAD);

            if (ndo->ndo_vflag > 1)
                ND_PRINT("\n\t  %s Extension TLV #%u, length %u, value ",
                       tok2str(jnx_ext_tlv_values,"Unknown",tlv_type),
                       tlv_type,
                       tlv_len);

            tlv_value = juniper_read_tlv_value(ndo, tptr, tlv_type, tlv_len);
            switch (tlv_type) {
            case JUNIPER_EXT_TLV_IFD_NAME:
                /* FIXME */
                break;
            case JUNIPER_EXT_TLV_IFD_MEDIATYPE:
            case JUNIPER_EXT_TLV_TTP_IFD_MEDIATYPE:
                if (tlv_value != -1) {
                    if (ndo->ndo_vflag > 1)
                        ND_PRINT("%s (%u)",
                               tok2str(juniper_ifmt_values, "Unknown", tlv_value),
                               tlv_value);
                }
                break;
            case JUNIPER_EXT_TLV_IFL_ENCAPS:
            case JUNIPER_EXT_TLV_TTP_IFL_ENCAPS:
                if (tlv_value != -1) {
                    if (ndo->ndo_vflag > 1)
                        ND_PRINT("%s (%u)",
                               tok2str(juniper_ifle_values, "Unknown", tlv_value),
                               tlv_value);
                }
                break;
            case JUNIPER_EXT_TLV_IFL_IDX: /* fall through */
            case JUNIPER_EXT_TLV_IFL_UNIT:
            case JUNIPER_EXT_TLV_IFD_IDX:
            default:
                if (tlv_value != -1) {
                    if (ndo->ndo_vflag > 1)
                        ND_PRINT("%u", tlv_value);
                }
                break;
            }

            tptr+=tlv_len;
            extension_length -= tlv_len+JUNIPER_EXT_TLV_OVERHEAD;
        }

        if (ndo->ndo_vflag > 1)
            ND_PRINT("\n\t-----original packet-----\n\t");
    }

    if ((l2info->flags & JUNIPER_BPF_NO_L2 ) == JUNIPER_BPF_NO_L2 ) {
        if (ndo->ndo_eflag)
            ND_PRINT("no-L2-hdr, ");

        /* there is no link-layer present -
         * perform the v4/v6 heuristics
         * to figure out what it is
         */
        ND_TCHECK_1(p + (jnx_header_len + 4));
        if (ip_heuristic_guess(ndo, p + jnx_header_len + 4,
                               l2info->length - (jnx_header_len + 4)) == 0)
            ND_PRINT("no IP-hdr found!");

        l2info->header_len=jnx_header_len+4;
        return 0; /* stop parsing the output further */

    }
    l2info->header_len = jnx_header_len;
    p+=l2info->header_len;
    l2info->length -= l2info->header_len;
    l2info->caplen -= l2info->header_len;

    /* search through the cookie table for one matching our PIC type */
    lp = NULL;
    for (const struct juniper_cookie_table_t *table_lp = juniper_cookie_table;
         table_lp->s != NULL; table_lp++) {
        if (table_lp->pictype == l2info->pictype) {
            lp = table_lp;
            break;
        }
    }

    /* If we found one matching our PIC type, copy its values */
    if (lp != NULL) {
        l2info->cookie_len += lp->cookie_len;

        switch (GET_U_1(p)) {
        case LS_COOKIE_ID:
            l2info->cookie_type = LS_COOKIE_ID;
            l2info->cookie_len += 2;
            break;
        case AS_COOKIE_ID:
            l2info->cookie_type = AS_COOKIE_ID;
            l2info->cookie_len = 8;
            break;

        default:
            l2info->bundle = l2info->cookie[0];
            break;
        }


#ifdef DLT_JUNIPER_MFR
        /* MFR child links don't carry cookies */
        if (l2info->pictype == DLT_JUNIPER_MFR &&
            (GET_U_1(p) & MFR_BE_MASK) == MFR_BE_MASK) {
            l2info->cookie_len = 0;
        }
#endif

        l2info->header_len += l2info->cookie_len;
        l2info->length -= l2info->cookie_len;
        l2info->caplen -= l2info->cookie_len;

        if (ndo->ndo_eflag)
            ND_PRINT("%s-PIC, cookie-len %u",
                   lp->s,
                   l2info->cookie_len);

        if (l2info->cookie_len > 8) {
            nd_print_invalid(ndo);
            return 0;
        }

        if (l2info->cookie_len > 0) {
            ND_TCHECK_LEN(p, l2info->cookie_len);
            if (ndo->ndo_eflag)
                ND_PRINT(", cookie 0x");
            for (idx = 0; idx < l2info->cookie_len; idx++) {
                l2info->cookie[idx] = GET_U_1(p + idx); /* copy cookie data */
                if (ndo->ndo_eflag) ND_PRINT("%02x", GET_U_1(p + idx));
            }
        }

        if (ndo->ndo_eflag) ND_PRINT(": "); /* print demarc b/w L2/L3*/


        l2info->proto = GET_BE_U_2(p + l2info->cookie_len);
    }
    p+=l2info->cookie_len;

    /* DLT_ specific parsing */
    switch(l2info->pictype) {
#ifdef DLT_JUNIPER_MLPPP
    case DLT_JUNIPER_MLPPP:
        switch (l2info->cookie_type) {
        case LS_COOKIE_ID:
            l2info->bundle = l2info->cookie[1];
            break;
        case AS_COOKIE_ID:
            /* use EXTRACT_, not GET_ (not packet buffer pointer) */
            l2info->bundle = (EXTRACT_BE_U_2(&l2info->cookie[6])>>3)&0xfff;
            l2info->proto = (l2info->cookie[5])&JUNIPER_LSQ_L3_PROTO_MASK;
            break;
        default:
            l2info->bundle = l2info->cookie[0];
            break;
        }
        break;
#endif
#ifdef DLT_JUNIPER_MLFR
    case DLT_JUNIPER_MLFR:
        switch (l2info->cookie_type) {
        case LS_COOKIE_ID:
            l2info->bundle = l2info->cookie[1];
            l2info->proto = GET_BE_U_2(p);
            l2info->header_len += 2;
            l2info->length -= 2;
            l2info->caplen -= 2;
            break;
        case AS_COOKIE_ID:
            /* use EXTRACT_, not GET_ (not packet buffer pointer) */
            l2info->bundle = (EXTRACT_BE_U_2(&l2info->cookie[6])>>3)&0xfff;
            l2info->proto = (l2info->cookie[5])&JUNIPER_LSQ_L3_PROTO_MASK;
            break;
        default:
            l2info->bundle = l2info->cookie[0];
            l2info->header_len += 2;
            l2info->length -= 2;
            l2info->caplen -= 2;
            break;
        }
        break;
#endif
#ifdef DLT_JUNIPER_MFR
    case DLT_JUNIPER_MFR:
        switch (l2info->cookie_type) {
        case LS_COOKIE_ID:
            l2info->bundle = l2info->cookie[1];
            l2info->proto = GET_BE_U_2(p);
            l2info->header_len += 2;
            l2info->length -= 2;
            l2info->caplen -= 2;
            break;
        case AS_COOKIE_ID:
            /* use EXTRACT_, not GET_ (not packet buffer pointer) */
            l2info->bundle = (EXTRACT_BE_U_2(&l2info->cookie[6])>>3)&0xfff;
            l2info->proto = (l2info->cookie[5])&JUNIPER_LSQ_L3_PROTO_MASK;
            break;
        default:
            l2info->bundle = l2info->cookie[0];
            break;
        }
        break;
#endif
#ifdef DLT_JUNIPER_ATM2
    case DLT_JUNIPER_ATM2:
        ND_TCHECK_4(p);
        /* ATM cell relay control word present ? */
        if (l2info->cookie[7] & ATM2_PKT_TYPE_MASK) {
            control_word = GET_BE_U_4(p);
            /* some control word heuristics */
            switch(control_word) {
            case 0: /* zero control word */
            case 0x08000000: /* < JUNOS 7.4 control-word */
            case 0x08380000: /* cntl word plus cell length (56) >= JUNOS 7.4*/
                l2info->header_len += 4;
                break;
            default:
                break;
            }

            if (ndo->ndo_eflag)
                ND_PRINT("control-word 0x%08x ", control_word);
        }
        break;
#endif
#ifdef DLT_JUNIPER_ES
    case DLT_JUNIPER_ES:
        break;
#endif
#ifdef DLT_JUNIPER_GGSN
    case DLT_JUNIPER_GGSN:
        break;
#endif
#ifdef DLT_JUNIPER_SERVICES
    case DLT_JUNIPER_SERVICES:
        break;
#endif
#ifdef DLT_JUNIPER_ATM1
    case DLT_JUNIPER_ATM1:
        break;
#endif
#ifdef DLT_JUNIPER_PPP
    case DLT_JUNIPER_PPP:
        break;
#endif
#ifdef DLT_JUNIPER_CHDLC
    case DLT_JUNIPER_CHDLC:
        break;
#endif
#ifdef DLT_JUNIPER_ETHER
    case DLT_JUNIPER_ETHER:
        break;
#endif
#ifdef DLT_JUNIPER_FRELAY
    case DLT_JUNIPER_FRELAY:
        break;
#endif
#ifdef DLT_JUNIPER_MONITOR
    case DLT_JUNIPER_MONITOR:
        break;
#endif
#ifdef DLT_JUNIPER_PPPOE
    case DLT_JUNIPER_PPPOE:
        break;
#endif
#ifdef DLT_JUNIPER_PPPOE_ATM
    case DLT_JUNIPER_PPPOE_ATM:
        break;
#endif

    default:
        ND_PRINT("Unknown Juniper DLT_ type %u: ", l2info->pictype);
        break;
    }

    if (ndo->ndo_eflag)
        ND_PRINT("hlen %u, proto 0x%04x, ", l2info->header_len, l2info->proto);

    return 1; /* everything went ok so far. continue parsing */
invalid:
    nd_print_invalid(ndo);
    return 0;
}
#endif /* defined(DLT_JUNIPER_GGSN) || defined(DLT_JUNIPER_ES) || \
          defined(DLT_JUNIPER_MONITOR) || defined(DLT_JUNIPER_SERVICES) || \
          defined(DLT_JUNIPER_PPPOE) || defined(DLT_JUNIPER_ETHER) || \
          defined(DLT_JUNIPER_PPP) || defined(DLT_JUNIPER_FRELAY) || \
          defined(DLT_JUNIPER_CHDLC) || defined(DLT_JUNIPER_PPPOE_ATM) || \
          defined(DLT_JUNIPER_MLPPP) || defined(DLT_JUNIPER_MFR) || \
          defined(DLT_JUNIPER_MLFR) || defined(DLT_JUNIPER_ATM1) || \
          defined(DLT_JUNIPER_ATM2) */
