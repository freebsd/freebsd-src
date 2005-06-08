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
 * Extensively modified by Hannes Gredler (hannes@juniper.net) for more
 * complete IS-IS support.
 *
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] _U_ =
    "@(#) $Header: /tcpdump/master/tcpdump/print-isoclns.c,v 1.106.2.5 2004/03/24 01:45:26 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <tcpdump-stdinc.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "ether.h"
#include "extract.h"
#include "gmpls.h"

#define	NLPID_CLNS	129	/* 0x81 */
#define	NLPID_ESIS	130	/* 0x82 */
#define	NLPID_ISIS	131	/* 0x83 */
#define NLPID_IP6       0x8e
#define NLPID_IP        0xcc
#define	NLPID_NULLNS	0

#define IPV4            1       /* AFI value */
#define IPV6            2       /* AFI value */

/*
 * IS-IS is defined in ISO 10589.  Look there for protocol definitions.
 */

#define SYSTEM_ID_LEN	ETHER_ADDR_LEN
#define NODE_ID_LEN     SYSTEM_ID_LEN+1
#define LSP_ID_LEN      SYSTEM_ID_LEN+2

#define ISIS_VERSION	1
#define PDU_TYPE_MASK	0x1F
#define PRIORITY_MASK	0x7F

#define L1_LAN_IIH	15
#define L2_LAN_IIH	16
#define PTP_IIH		17
#define L1_LSP       	18
#define L2_LSP       	20
#define L1_CSNP  	24
#define L2_CSNP  	25
#define L1_PSNP		26
#define L2_PSNP		27

static struct tok isis_pdu_values[] = {
    { L1_LAN_IIH,       "L1 Lan IIH"},
    { L2_LAN_IIH,       "L2 Lan IIH"},
    { PTP_IIH,          "p2p IIH"},
    { L1_LSP,           "L1 LSP"},
    { L2_LSP,           "L2 LSP"},
    { L1_CSNP,          "L1 CSNP"},
    { L2_CSNP,          "L2 CSNP"},
    { L1_PSNP,          "L1 PSNP"},
    { L2_PSNP,          "L2 PSNP"},
    { 0, NULL}
};

/*
 * A TLV is a tuple of a type, length and a value and is normally used for
 * encoding information in all sorts of places.  This is an enumeration of
 * the well known types.
 *
 * list taken from rfc3359 plus some memory from veterans ;-)
 */

#define TLV_AREA_ADDR           1   /* iso10589 */
#define TLV_IS_REACH            2   /* iso10589 */
#define TLV_ESNEIGH             3   /* iso10589 */
#define TLV_PART_DIS            4   /* iso10589 */
#define TLV_PREFIX_NEIGH        5   /* iso10589 */
#define TLV_ISNEIGH             6   /* iso10589 */
#define TLV_ISNEIGH_VARLEN      7   /* iso10589 */
#define TLV_PADDING             8   /* iso10589 */
#define TLV_LSP                 9   /* iso10589 */
#define TLV_AUTH                10  /* iso10589, rfc3567 */
#define TLV_CHECKSUM            12  /* rfc3358 */
#define TLV_LSP_BUFFERSIZE      14  /* iso10589 rev2 */
#define TLV_EXT_IS_REACH        22  /* draft-ietf-isis-traffic-05 */
#define TLV_IS_ALIAS_ID         24  /* draft-ietf-isis-ext-lsp-frags-02 */
#define TLV_DECNET_PHASE4       42
#define TLV_LUCENT_PRIVATE      66
#define TLV_INT_IP_REACH        128 /* rfc1195, rfc2966 */
#define TLV_PROTOCOLS           129 /* rfc1195 */
#define TLV_EXT_IP_REACH        130 /* rfc1195, rfc2966 */
#define TLV_IDRP_INFO           131 /* rfc1195 */
#define TLV_IPADDR              132 /* rfc1195 */
#define TLV_IPAUTH              133 /* rfc1195 */
#define TLV_TE_ROUTER_ID        134 /* draft-ietf-isis-traffic-05 */
#define TLV_EXTD_IP_REACH       135 /* draft-ietf-isis-traffic-05 */
#define TLV_HOSTNAME            137 /* rfc2763 */
#define TLV_SHARED_RISK_GROUP   138 /* draft-ietf-isis-gmpls-extensions */
#define TLV_NORTEL_PRIVATE1     176
#define TLV_NORTEL_PRIVATE2     177
#define TLV_HOLDTIME            198 /* ES-IS */
#define TLV_RESTART_SIGNALING   211 /* draft-ietf-isis-restart-01 */
#define TLV_MT_IS_REACH         222 /* draft-ietf-isis-wg-multi-topology-05 */
#define TLV_MT_SUPPORTED        229 /* draft-ietf-isis-wg-multi-topology-05 */
#define TLV_IP6ADDR             232 /* draft-ietf-isis-ipv6-02 */
#define TLV_MT_IP_REACH         235 /* draft-ietf-isis-wg-multi-topology-05 */
#define TLV_IP6_REACH           236 /* draft-ietf-isis-ipv6-02 */
#define TLV_MT_IP6_REACH        237 /* draft-ietf-isis-wg-multi-topology-05 */
#define TLV_PTP_ADJ             240 /* rfc3373 */
#define TLV_IIH_SEQNR           241 /* draft-shen-isis-iih-sequence-00 */
#define TLV_VENDOR_PRIVATE      250 /* draft-ietf-isis-proprietary-tlv-00 */

static struct tok isis_tlv_values[] = {
    { TLV_AREA_ADDR,	     "Area address(es)"},
    { TLV_IS_REACH,          "IS Reachability"},
    { TLV_ESNEIGH,           "ES Neighbor(s)"},
    { TLV_PART_DIS,          "Partition DIS"},
    { TLV_PREFIX_NEIGH,      "Prefix Neighbors"},
    { TLV_ISNEIGH,           "IS Neighbor(s)"},
    { TLV_ISNEIGH_VARLEN,    "IS Neighbor(s) (variable length)"},
    { TLV_PADDING,           "Padding"},
    { TLV_LSP,               "LSP entries"},
    { TLV_AUTH,              "Authentication"},
    { TLV_CHECKSUM,          "Checksum"},
    { TLV_LSP_BUFFERSIZE,    "LSP Buffersize"},
    { TLV_EXT_IS_REACH,      "Extended IS Reachability"},
    { TLV_IS_ALIAS_ID,       "IS Alias ID"},
    { TLV_DECNET_PHASE4,     "DECnet Phase IV"},
    { TLV_LUCENT_PRIVATE,    "Lucent Proprietary"},
    { TLV_INT_IP_REACH,      "IPv4 Internal Reachability"},
    { TLV_PROTOCOLS,         "Protocols supported"},
    { TLV_EXT_IP_REACH,      "IPv4 External Reachability"},
    { TLV_IDRP_INFO,         "Inter-Domain Information Type"},
    { TLV_IPADDR,            "IPv4 Interface address(es)"},
    { TLV_IPAUTH,            "IPv4 authentication (deprecated)"},
    { TLV_TE_ROUTER_ID,      "Traffic Engineering Router ID"},
    { TLV_EXTD_IP_REACH,      "Extended IPv4 Reachability"},
    { TLV_HOSTNAME,          "Hostname"},
    { TLV_SHARED_RISK_GROUP, "Shared Risk Link Group"},
    { TLV_NORTEL_PRIVATE1,   "Nortel Proprietary"},
    { TLV_NORTEL_PRIVATE2,   "Nortel Proprietary"},
    { TLV_HOLDTIME,          "Holdtime"},
    { TLV_RESTART_SIGNALING, "Restart Signaling"},
    { TLV_MT_IS_REACH,       "Multi Topology IS Reachability"},
    { TLV_MT_SUPPORTED,      "Multi Topology"},
    { TLV_IP6ADDR,           "IPv6 Interface address(es)"},
    { TLV_MT_IP_REACH,       "Multi-Topology IPv4 Reachability"},
    { TLV_IP6_REACH,         "IPv6 reachability"},
    { TLV_MT_IP6_REACH,      "Multi-Topology IP6 Reachability"},
    { TLV_PTP_ADJ,           "Point-to-point Adjacency State"},
    { TLV_IIH_SEQNR,         "Hello PDU Sequence Number"},
    { TLV_VENDOR_PRIVATE,    "Vendor Private"},
    { 0, NULL }
};

#define SUBTLV_EXT_IS_REACH_ADMIN_GROUP           3 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_LINK_LOCAL_REMOTE_ID  4 /* draft-ietf-isis-gmpls-extensions */
#define SUBTLV_EXT_IS_REACH_LINK_REMOTE_ID        5 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR        6 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR    8 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_MAX_LINK_BW           9 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_RESERVABLE_BW        10 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_UNRESERVED_BW        11 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_TE_METRIC            18 /* draft-ietf-isis-traffic-05 */
#define SUBTLV_EXT_IS_REACH_LINK_PROTECTION_TYPE 20 /* draft-ietf-isis-gmpls-extensions */
#define SUBTLV_EXT_IS_REACH_INTF_SW_CAP_DESCR    21 /* draft-ietf-isis-gmpls-extensions */

static struct tok isis_ext_is_reach_subtlv_values[] = {
    { SUBTLV_EXT_IS_REACH_ADMIN_GROUP,            "Administrative groups" },
    { SUBTLV_EXT_IS_REACH_LINK_LOCAL_REMOTE_ID,   "Link Local/Remote Identifier" },
    { SUBTLV_EXT_IS_REACH_LINK_REMOTE_ID,         "Link Remote Identifier" },
    { SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR,         "IPv4 interface address" },
    { SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR,     "IPv4 neighbor address" },
    { SUBTLV_EXT_IS_REACH_MAX_LINK_BW,            "Maximum link bandwidth" },
    { SUBTLV_EXT_IS_REACH_RESERVABLE_BW,          "Reservable link bandwidth" },
    { SUBTLV_EXT_IS_REACH_UNRESERVED_BW,          "Unreserved bandwidth" },
    { SUBTLV_EXT_IS_REACH_TE_METRIC,              "Traffic Engineering Metric" },
    { SUBTLV_EXT_IS_REACH_LINK_PROTECTION_TYPE,   "Link Protection Type" },
    { SUBTLV_EXT_IS_REACH_INTF_SW_CAP_DESCR,      "Interface Switching Capability" },
    { 250,                                        "Reserved for cisco specific extensions" },
    { 251,                                        "Reserved for cisco specific extensions" },
    { 252,                                        "Reserved for cisco specific extensions" },
    { 253,                                        "Reserved for cisco specific extensions" },
    { 254,                                        "Reserved for cisco specific extensions" },
    { 255,                                        "Reserved for future expansion" },
    { 0, NULL }
};

#define SUBTLV_EXTD_IP_REACH_ADMIN_TAG32          1
#define SUBTLV_EXTD_IP_REACH_ADMIN_TAG64          2

static struct tok isis_ext_ip_reach_subtlv_values[] = {
    { SUBTLV_EXTD_IP_REACH_ADMIN_TAG32,           "32-Bit Administrative tag" },
    { SUBTLV_EXTD_IP_REACH_ADMIN_TAG64,           "64-Bit Administrative tag" },
    { 0, NULL }
};

#define SUBTLV_AUTH_SIMPLE        1
#define SUBTLV_AUTH_MD5          54
#define SUBTLV_AUTH_MD5_LEN      16
#define SUBTLV_AUTH_PRIVATE     255

static struct tok isis_subtlv_auth_values[] = {
    { SUBTLV_AUTH_SIMPLE,	"simple text password"},
    { SUBTLV_AUTH_MD5,	        "HMAC-MD5 password"},
    { SUBTLV_AUTH_PRIVATE,	"Routing Domain private password"},
    { 0, NULL }
};

#define SUBTLV_IDRP_RES           0
#define SUBTLV_IDRP_LOCAL         1
#define SUBTLV_IDRP_ASN           2

static struct tok isis_subtlv_idrp_values[] = {
    { SUBTLV_IDRP_RES,         "Reserved"},
    { SUBTLV_IDRP_LOCAL,       "Routing-Domain Specific"},
    { SUBTLV_IDRP_ASN,         "AS Number Tag"},
    { 0, NULL}
};

#define ISIS_8BIT_MASK(x)                  ((x)&0xff)

#define ISIS_MASK_LSP_OL_BIT(x)            ((x)&0x4)
#define ISIS_MASK_LSP_ISTYPE_BITS(x)       ((x)&0x3)
#define ISIS_MASK_LSP_PARTITION_BIT(x)     ((x)&0x80)
#define ISIS_MASK_LSP_ATT_BITS(x)          ((x)&0x78)
#define ISIS_MASK_LSP_ATT_ERROR_BIT(x)     ((x)&0x40)
#define ISIS_MASK_LSP_ATT_EXPENSE_BIT(x)   ((x)&0x20)
#define ISIS_MASK_LSP_ATT_DELAY_BIT(x)     ((x)&0x10)
#define ISIS_MASK_LSP_ATT_DEFAULT_BIT(x)   ((x)&0x8)

#define ISIS_MASK_MTID(x)                  ((x)&0x0fff)
#define ISIS_MASK_MTFLAGS(x)               ((x)&0xf000)

static struct tok isis_mt_flag_values[] = {
    { 0x4000,                  "sub-TLVs present"},
    { 0x8000,                  "ATT bit set"},
    { 0, NULL}
};

#define ISIS_MASK_TLV_EXTD_IP_UPDOWN(x)     ((x)&0x80)
#define ISIS_MASK_TLV_EXTD_IP_SUBTLV(x)     ((x)&0x40)

#define ISIS_MASK_TLV_EXTD_IP6_IE(x)        ((x)&0x40)
#define ISIS_MASK_TLV_EXTD_IP6_SUBTLV(x)    ((x)&0x20)

#define ISIS_LSP_TLV_METRIC_SUPPORTED(x)   ((x)&0x80)
#define ISIS_LSP_TLV_METRIC_IE(x)          ((x)&0x40)
#define ISIS_LSP_TLV_METRIC_UPDOWN(x)      ((x)&0x80)
#define ISIS_LSP_TLV_METRIC_VALUE(x)	   ((x)&0x3f)

#define ISIS_MASK_TLV_SHARED_RISK_GROUP(x) ((x)&0x1)

static struct tok isis_mt_values[] = {
    { 0,    "IPv4 unicast"},
    { 1,    "In-Band Management"},
    { 2,    "IPv6 unicast"},
    { 3,    "Multicast"},
    { 4095, "Development, Experimental or Proprietary"},
    { 0, NULL }
};

static struct tok isis_iih_circuit_type_values[] = {
    { 1,    "Level 1 only"},
    { 2,    "Level 2 only"},
    { 3,    "Level 1, Level 2"},
    { 0, NULL}
};

#define ISIS_LSP_TYPE_UNUSED0   0
#define ISIS_LSP_TYPE_LEVEL_1   1
#define ISIS_LSP_TYPE_UNUSED2   2
#define ISIS_LSP_TYPE_LEVEL_2   3

static struct tok isis_lsp_istype_values[] = {
    { ISIS_LSP_TYPE_UNUSED0,	"Unused 0x0 (invalid)"},
    { ISIS_LSP_TYPE_LEVEL_1,	"L1 IS"},
    { ISIS_LSP_TYPE_UNUSED2,	"Unused 0x2 (invalid)"},
    { ISIS_LSP_TYPE_LEVEL_2,	"L1L2 IS"},
    { 0, NULL }
};

static struct tok osi_nlpid_values[] = {
    { NLPID_CLNS,   "CLNS"},
    { NLPID_IP,     "IPv4"},
    { NLPID_IP6,    "IPv6"},
    { 0, NULL }
};

/*
 * Katz's point to point adjacency TLV uses codes to tell us the state of
 * the remote adjacency.  Enumerate them.
 */

#define ISIS_PTP_ADJ_UP   0
#define ISIS_PTP_ADJ_INIT 1
#define ISIS_PTP_ADJ_DOWN 2


static struct tok isis_ptp_adjancey_values[] = {
    { ISIS_PTP_ADJ_UP,    "Up" },
    { ISIS_PTP_ADJ_INIT,  "Initializing" },
    { ISIS_PTP_ADJ_DOWN,  "Down" },
    { 0, NULL}
};

struct isis_tlv_ptp_adj {
    u_int8_t adjacency_state;
    u_int8_t extd_local_circuit_id[4];
    u_int8_t neighbor_sysid[SYSTEM_ID_LEN];
    u_int8_t neighbor_extd_local_circuit_id[4];
};

static int osi_cksum(const u_int8_t *, u_int);
static void esis_print(const u_int8_t *, u_int);
static int isis_print(const u_int8_t *, u_int);

struct isis_metric_block {
    u_int8_t metric_default;
    u_int8_t metric_delay;
    u_int8_t metric_expense;
    u_int8_t metric_error;
};

struct isis_tlv_is_reach {
    struct isis_metric_block isis_metric_block;
    u_int8_t neighbor_nodeid[NODE_ID_LEN];
};

struct isis_tlv_es_reach {
    struct isis_metric_block isis_metric_block;
    u_int8_t neighbor_sysid[SYSTEM_ID_LEN];
};

struct isis_tlv_ip_reach {
    struct isis_metric_block isis_metric_block;
    u_int8_t prefix[4];
    u_int8_t mask[4];
};

static struct tok isis_is_reach_virtual_values[] = {
    { 0,    "IsNotVirtual"},
    { 1,    "IsVirtual"},
    { 0, NULL }
};

static struct tok isis_restart_flag_values[] = {
    { 0x1,  "Restart Request"},
    { 0x2,  "Restart Acknowledgement"},
    { 0, NULL }
};

struct isis_common_header {
    u_int8_t nlpid;
    u_int8_t fixed_len;
    u_int8_t version;			/* Protocol version */
    u_int8_t id_length;
    u_int8_t pdu_type;		        /* 3 MSbits are reserved */
    u_int8_t pdu_version;		/* Packet format version */
    u_int8_t reserved;
    u_int8_t max_area;
};

struct isis_iih_lan_header {
    u_int8_t circuit_type;
    u_int8_t source_id[SYSTEM_ID_LEN];
    u_int8_t holding_time[2];
    u_int8_t pdu_len[2];
    u_int8_t priority;
    u_int8_t lan_id[NODE_ID_LEN];
};

struct isis_iih_ptp_header {
    u_int8_t circuit_type;
    u_int8_t source_id[SYSTEM_ID_LEN];
    u_int8_t holding_time[2];
    u_int8_t pdu_len[2];
    u_int8_t circuit_id;
};

struct isis_lsp_header {
    u_int8_t pdu_len[2];
    u_int8_t remaining_lifetime[2];
    u_int8_t lsp_id[LSP_ID_LEN];
    u_int8_t sequence_number[4];
    u_int8_t checksum[2];
    u_int8_t typeblock;
};

struct isis_csnp_header {
    u_int8_t pdu_len[2];
    u_int8_t source_id[NODE_ID_LEN];
    u_int8_t start_lsp_id[LSP_ID_LEN];
    u_int8_t end_lsp_id[LSP_ID_LEN];
};

struct isis_psnp_header {
    u_int8_t pdu_len[2];
    u_int8_t source_id[NODE_ID_LEN];
};

struct isis_tlv_lsp {
    u_int8_t remaining_lifetime[2];
    u_int8_t lsp_id[LSP_ID_LEN];
    u_int8_t sequence_number[4];
    u_int8_t checksum[2];
};

static char *
print_nsap(register const u_int8_t *pptr, register int nsap_length)
{
	int nsap_idx;
	static char nsap_ascii_output[sizeof("xx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xxxx.xx")];
        char *junk_buf = nsap_ascii_output;

        if (nsap_length < 1 || nsap_length > 20) {
                snprintf(nsap_ascii_output, sizeof(nsap_ascii_output),
                    "illegal length");
                return (nsap_ascii_output);
        }

	for (nsap_idx = 0; nsap_idx < nsap_length; nsap_idx++) {
		if (!TTEST2(*pptr, 1))
			return (0);
		snprintf(junk_buf,
		    sizeof(nsap_ascii_output) - (junk_buf - nsap_ascii_output),
		    "%02x", *pptr++);
		junk_buf += strlen(junk_buf);
		if (((nsap_idx & 1) == 0) &&
                     (nsap_idx + 1 < nsap_length)) {
                     	*junk_buf++ = '.';
		}
	}
        *(junk_buf) = '\0';
	return (nsap_ascii_output);
}

#define ISIS_COMMON_HEADER_SIZE (sizeof(struct isis_common_header))
#define ISIS_IIH_LAN_HEADER_SIZE (sizeof(struct isis_iih_lan_header))
#define ISIS_IIH_PTP_HEADER_SIZE (sizeof(struct isis_iih_ptp_header))
#define ISIS_LSP_HEADER_SIZE (sizeof(struct isis_lsp_header))
#define ISIS_CSNP_HEADER_SIZE (sizeof(struct isis_csnp_header))
#define ISIS_PSNP_HEADER_SIZE (sizeof(struct isis_psnp_header))

void isoclns_print(const u_int8_t *p, u_int length, u_int caplen)
{
	const struct isis_common_header *header;

	header = (const struct isis_common_header *)p;

        printf("%sOSI", caplen < 1 ? "|" : "");

        if (caplen < 1) /* enough bytes on the wire ? */
                return;

	switch (*p) {

	case NLPID_CLNS:
		(void)printf(", CLNS, length %u", length);
		break;

	case NLPID_ESIS:
		esis_print(p, length);
		return;

	case NLPID_ISIS:
		if (!isis_print(p, length))
                        print_unknown_data(p,"\n\t",caplen);
		break;

	case NLPID_NULLNS:
		(void)printf(", ISO NULLNS, length: %u", length);
		break;

	default:
		(void)printf(", Unknown NLPID 0x%02x, length: %u", p[0], length);
		if (caplen > 1)
                        print_unknown_data(p,"\n\t",caplen);
		break;
	}
}

#define	ESIS_REDIRECT	6
#define	ESIS_ESH	2
#define	ESIS_ISH	4

static struct tok esis_values[] = {
    { ESIS_REDIRECT, "redirect"},
    { ESIS_ESH,      "ESH"},
    { ESIS_ISH,      "ISH"},
    { 0, NULL }
};

struct esis_hdr {
	u_int8_t version;
	u_int8_t reserved;
	u_int8_t type;
	u_int8_t tmo[2];
	u_int8_t cksum[2];
};

static void
esis_print(const u_int8_t *p, u_int length)
{
	const u_int8_t *ep;
	u_int li;
	const struct esis_hdr *eh;

	if (length <= 2) {
		if (qflag)
			printf(" bad pkt!");
		else
			printf(" no header at all!");
		return;
	}
	li = p[1];
	eh = (const struct esis_hdr *) &p[2];
	ep = p + li;
	if (li > length) {
		if (qflag)
			printf(" bad pkt!");
		else
			printf(" LI(%d) > PDU size (%d)!", li, length);
		return;
	}
	if (li < sizeof(struct esis_hdr) + 2) {
		if (qflag)
			printf(" bad pkt!");
		else {
			printf(" too short for esis header %d:", li);
			while (--length != 0)
				printf("%02X", *p++);
		}
		return;
	}

        printf(", ES-IS, %s, length %u",
               tok2str(esis_values,"unknown type: %u",eh->type & 0x1f),
               length);

        if(vflag < 1)
               return;

	if (vflag && osi_cksum(p, li)) {
		printf(" bad cksum (got 0x%02x%02x)",
		       eh->cksum[1], eh->cksum[0]);
		default_print(p, length);
		return;
	}
	if (eh->version != 1) {
		printf(" unsupported version %d", eh->version);
		return;
	}
	p += sizeof(*eh) + 2;
	li -= sizeof(*eh) + 2;	/* protoid * li */

	switch (eh->type & 0x1f) {
	case ESIS_REDIRECT: {
		const u_int8_t *dst, *snpa, *is;

		dst = p; p += *p + 1;
		if (p > snapend)
			return;
		printf("\n\t\t %s", isonsap_string(dst));
		snpa = p; p += *p + 1;
		is = p;   p += *p + 1;
		if (p > snapend)
			return;
		if (p > ep) {
			printf(" [bad li]");
			return;
		}
		if (is[0] == 0)
			printf(" > %s", etheraddr_string(&snpa[1]));
		else
			printf(" > %s", isonsap_string(is));
		li = ep - p;
		break;
	}

	case ESIS_ESH:
		break;

	case ESIS_ISH: {
		const u_int8_t *is;

		is = p; p += *p + 1;
		if (p > ep) {
			printf(" [bad li]");
			return;
		}
		if (p > snapend)
			return;
		if (!qflag)
			printf("\n\tNET: %s", print_nsap(is+1,*is));
		li = ep - p;
		break;
	}

	default:
            if (vflag <= 1) {
		    if (p < snapend) 
                            print_unknown_data(p,"\n\t  ",snapend-p);
            }
            return;
	}

        /* hexdump - FIXME ? */
        if (vflag > 1) {
                    if (p < snapend)
                            print_unknown_data(p,"\n\t  ",snapend-p);
        }
	if (vflag)
		while (p < ep && li) {
			u_int op, opli;
			const u_int8_t *q;

			if (snapend - p < 2)
				return;
			if (li < 2) {
				printf(", bad opts/li");
				return;
			}
			op = *p++;
			opli = *p++;
			li -= 2;
			if (opli > li) {
				printf(", opt (%d) too long", op);
				return;
			}
			li -= opli;
			q = p;
			p += opli;

			if (snapend < p)
				return;

			if (op == TLV_HOLDTIME && opli == 2) {
				printf("\n\tholdtime: %us", EXTRACT_16BITS(q));
				continue;
			}

			if (op == TLV_PROTOCOLS && opli >= 1) {
				printf("\n\t%s (length: %u): %s",
                                       tok2str(isis_tlv_values, "unknown", op),
                                       opli,
                                       tok2str(osi_nlpid_values,"Unknown 0x%02x",*q));
				continue;
			}

                        print_unknown_data(q,"\n\t  ",opli);
		}
}

/* shared routine for printing system, node and lsp-ids */
static char *
isis_print_id(const u_int8_t *cp, int id_len)
{
    int i;
    static char id[sizeof("xxxx.xxxx.xxxx.yy-zz")];
    char *pos = id;

    for (i = 1; i <= SYSTEM_ID_LEN; i++) {
        snprintf(pos, sizeof(id) - (pos - id), "%02x", *cp++);
	pos += strlen(pos);
	if (i == 2 || i == 4)
	    *pos++ = '.';
	}
    if (id_len >= NODE_ID_LEN) {
        snprintf(pos, sizeof(id) - (pos - id), ".%02x", *cp++);
	pos += strlen(pos);
    }
    if (id_len == LSP_ID_LEN)
        snprintf(pos, sizeof(id) - (pos - id), "-%02x", *cp);
    return (id);
}

/* print the 4-byte metric block which is common found in the old-style TLVs */
static int
isis_print_metric_block (const struct isis_metric_block *isis_metric_block)
{
    printf(", Default Metric: %d, %s",
           ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_default),
           ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_default) ? "External" : "Internal");
    if (!ISIS_LSP_TLV_METRIC_SUPPORTED(isis_metric_block->metric_delay))
        printf("\n\t\t  Delay Metric: %d, %s",
               ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_delay),
               ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_delay) ? "External" : "Internal");
    if (!ISIS_LSP_TLV_METRIC_SUPPORTED(isis_metric_block->metric_expense))
        printf("\n\t\t  Expense Metric: %d, %s",
               ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_expense),
               ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_expense) ? "External" : "Internal");
    if (!ISIS_LSP_TLV_METRIC_SUPPORTED(isis_metric_block->metric_error))
        printf("\n\t\t  Error Metric: %d, %s",
               ISIS_LSP_TLV_METRIC_VALUE(isis_metric_block->metric_error),
               ISIS_LSP_TLV_METRIC_IE(isis_metric_block->metric_error) ? "External" : "Internal");

    return(1); /* everything is ok */
}

static int
isis_print_tlv_ip_reach (const u_int8_t *cp, const char *ident, int length)
{
	int prefix_len;
	const struct isis_tlv_ip_reach *tlv_ip_reach;

	tlv_ip_reach = (const struct isis_tlv_ip_reach *)cp;

	while (length > 0) {
		if ((size_t)length < sizeof(*tlv_ip_reach)) {
			printf("short IPv4 Reachability (%d vs %lu)",
                               length,
                               (unsigned long)sizeof(*tlv_ip_reach));
			return (0);
		}

		if (!TTEST(*tlv_ip_reach))
		    return (0);

		prefix_len = mask2plen(EXTRACT_32BITS(tlv_ip_reach->mask));

		if (prefix_len == -1)
			printf("%sIPv4 prefix: %s mask %s",
                               ident,
			       ipaddr_string((tlv_ip_reach->prefix)),
			       ipaddr_string((tlv_ip_reach->mask)));
		else
			printf("%sIPv4 prefix: %15s/%u",
                               ident,
			       ipaddr_string((tlv_ip_reach->prefix)),
			       prefix_len);

		printf(", Distribution: %s, Metric: %u, %s",
                       ISIS_LSP_TLV_METRIC_UPDOWN(tlv_ip_reach->isis_metric_block.metric_default) ? "down" : "up",
                       ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_default),
                       ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_default) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->isis_metric_block.metric_delay))
                    printf("%s  Delay Metric: %u, %s",
                           ident,
                           ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_delay),
                           ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_delay) ? "External" : "Internal");
                
		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->isis_metric_block.metric_expense))
                    printf("%s  Expense Metric: %u, %s",
                           ident,
                           ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_expense),
                           ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_expense) ? "External" : "Internal");
                
		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->isis_metric_block.metric_error))
                    printf("%s  Error Metric: %u, %s",
                           ident,
                           ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->isis_metric_block.metric_error),
                           ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->isis_metric_block.metric_error) ? "External" : "Internal");

		length -= sizeof(struct isis_tlv_ip_reach);
		tlv_ip_reach++;
	}
	return (1);
}

/*
 * this is the common IP-REACH subTLV decoder it is called
 * from various EXTD-IP REACH TLVs (135,235,236,237)
 */

static int
isis_print_ip_reach_subtlv (const u_int8_t *tptr,int subt,int subl,const char *ident) {

        /* first lets see if we know the subTLVs name*/
	printf("%s%s subTLV #%u, length: %u",
	       ident,
               tok2str(isis_ext_ip_reach_subtlv_values,
                       "unknown",
                       subt),
               subt,
               subl);

	if (!TTEST2(*tptr,subl))
	    goto trunctlv;

    switch(subt) {
    case SUBTLV_EXTD_IP_REACH_ADMIN_TAG32:
        while (subl >= 4) {
	    printf(", 0x%08x (=%u)",
		   EXTRACT_32BITS(tptr),
		   EXTRACT_32BITS(tptr));
	    tptr+=4;
	    subl-=4;
	}
	break;
    case SUBTLV_EXTD_IP_REACH_ADMIN_TAG64:
        while (subl >= 8) {
	    printf(", 0x%08x%08x",
		   EXTRACT_32BITS(tptr),
		   EXTRACT_32BITS(tptr+4));
	    tptr+=8;
	    subl-=8;
	}
	break;
    default:
	if(!print_unknown_data(tptr,"\n\t\t    ",
			       subl))
	  return(0);
	break;
    }
    return(1);
	
trunctlv:
    printf("%spacket exceeded snapshot",ident);
    return(0);
}

/*
 * this is the common IS-REACH subTLV decoder it is called
 * from isis_print_ext_is_reach()
 */

static int
isis_print_is_reach_subtlv (const u_int8_t *tptr,int subt,int subl,const char *ident) {

        int priority_level;
        union { /* int to float conversion buffer for several subTLVs */
            float f; 
            u_int32_t i;
        } bw;

        /* first lets see if we know the subTLVs name*/
	printf("%s%s subTLV #%u, length: %u",
	       ident,
               tok2str(isis_ext_is_reach_subtlv_values,
                       "unknown",
                       subt),
               subt,
               subl);

	if (!TTEST2(*tptr,subl))
	    goto trunctlv;

        switch(subt) {
        case SUBTLV_EXT_IS_REACH_ADMIN_GROUP:      
        case SUBTLV_EXT_IS_REACH_LINK_LOCAL_REMOTE_ID:
        case SUBTLV_EXT_IS_REACH_LINK_REMOTE_ID:
	    if (subl >= 4) {
	      printf(", 0x%08x", EXTRACT_32BITS(tptr));
	      if (subl == 8) /* draft-ietf-isis-gmpls-extensions */
	        printf(", 0x%08x", EXTRACT_32BITS(tptr+4));
	    }
	    break;
        case SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR:
        case SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR:
            if (subl >= 4)
              printf(", %s", ipaddr_string(tptr));
            break;
        case SUBTLV_EXT_IS_REACH_MAX_LINK_BW :
	case SUBTLV_EXT_IS_REACH_RESERVABLE_BW:  
            if (subl >= 4) {
              bw.i = EXTRACT_32BITS(tptr);
              printf(", %.3f Mbps", bw.f*8/1000000 );
            }
            break;
        case SUBTLV_EXT_IS_REACH_UNRESERVED_BW :
            if (subl >= 32) {
              for (priority_level = 0; priority_level < 8; priority_level++) {
                bw.i = EXTRACT_32BITS(tptr);
                printf("%s  priority level %d: %.3f Mbps",
                       ident,
                       priority_level,
                       bw.f*8/1000000 );
		tptr+=4;
	      }
            }
            break;
        case SUBTLV_EXT_IS_REACH_TE_METRIC:
            if (subl >= 3)
              printf(", %u", EXTRACT_24BITS(tptr));
            break;
        case SUBTLV_EXT_IS_REACH_LINK_PROTECTION_TYPE:
            if (subl >= 2) {
              printf(", %s, Priority %u",
		   bittok2str(gmpls_link_prot_values, "none", *tptr),
                   *(tptr+1));
            }
            break;
        case SUBTLV_EXT_IS_REACH_INTF_SW_CAP_DESCR:
            if (subl >= 36) {
              printf("%s  Interface Switching Capability:%s",
                   ident,
                   tok2str(gmpls_switch_cap_values, "Unknown", *(tptr)));
              printf(", LSP Encoding: %s",
                   tok2str(gmpls_encoding_values, "Unknown", *(tptr+1)));
	      tptr+=4;
              printf("%s  Max LSP Bandwidth:",ident);
              for (priority_level = 0; priority_level < 8; priority_level++) {
                bw.i = EXTRACT_32BITS(tptr);
                printf("%s    priority level %d: %.3f Mbps",
                       ident,
                       priority_level,
                       bw.f*8/1000000 );
		tptr+=4;
              }
              subl-=36;
              /* there is some optional stuff left to decode but this is as of yet
                 not specified so just lets hexdump what is left */
              if(subl>0){
                if(!print_unknown_data(tptr,"\n\t\t    ",
				       subl-36))
                    return(0);
              }
            }
            break;
        default:
            if(!print_unknown_data(tptr,"\n\t\t    ",
				   subl))
                return(0);
            break;
        }
        return(1);

trunctlv:
    printf("%spacket exceeded snapshot",ident);
    return(0);
}


/*
 * this is the common IS-REACH decoder it is called
 * from various EXTD-IS REACH style TLVs (22,24,222)
 */

static int
isis_print_ext_is_reach (const u_int8_t *tptr,const char *ident, int tlv_type) {

    char ident_buffer[20];
    int subtlv_type,subtlv_len,subtlv_sum_len;
    int proc_bytes = 0; /* how many bytes did we process ? */
    
    if (!TTEST2(*tptr, NODE_ID_LEN))
        return(0);

    printf("%sIS Neighbor: %s", ident, isis_print_id(tptr, NODE_ID_LEN));
    tptr+=(NODE_ID_LEN);

    if (tlv_type != TLV_IS_ALIAS_ID) { /* the Alias TLV Metric field is implicit 0 */
        if (!TTEST2(*tptr, 3))    /* and is therefore skipped */
	    return(0);
	printf(", Metric: %d",EXTRACT_24BITS(tptr));
	tptr+=3;
    }
        
    if (!TTEST2(*tptr, 1))
        return(0);
    subtlv_sum_len=*(tptr++); /* read out subTLV length */
    proc_bytes=NODE_ID_LEN+3+1;
    printf(", %ssub-TLVs present",subtlv_sum_len ? "" : "no ");
    if (subtlv_sum_len) {
        printf(" (%u)",subtlv_sum_len);
        while (subtlv_sum_len>0) {
            if (!TTEST2(*tptr,2))
                return(0);
            subtlv_type=*(tptr++);
            subtlv_len=*(tptr++);
            /* prepend the ident string */
            snprintf(ident_buffer, sizeof(ident_buffer), "%s  ",ident);
            if(!isis_print_is_reach_subtlv(tptr,subtlv_type,subtlv_len,ident_buffer))
                return(0);
            tptr+=subtlv_len;
            subtlv_sum_len-=(subtlv_len+2);
            proc_bytes+=(subtlv_len+2);
        }
    }
    return(proc_bytes);
}

/*
 * this is the common Multi Topology ID decoder
 * it is called from various MT-TLVs (222,229,235,237)
 */

static int
isis_print_mtid (const u_int8_t *tptr,const char *ident) {
    
    if (!TTEST2(*tptr, 2))
        return(0);

    printf("%s%s",
           ident,
           tok2str(isis_mt_values,
                   "Reserved for IETF Consensus",
                   ISIS_MASK_MTID(EXTRACT_16BITS(tptr))));

    printf(" Topology (0x%03x), Flags: [%s]",
           ISIS_MASK_MTID(EXTRACT_16BITS(tptr)),
           bittok2str(isis_mt_flag_values, "none",ISIS_MASK_MTFLAGS(EXTRACT_16BITS(tptr))));

    return(2);
}

/*
 * this is the common extended IP reach decoder
 * it is called from TLVs (135,235,236,237)
 * we process the TLV and optional subTLVs and return
 * the amount of processed bytes
 */

static int
isis_print_extd_ip_reach (const u_int8_t *tptr, const char *ident, u_int16_t afi) {

    char ident_buffer[20];
    u_int8_t prefix[16]; /* shared copy buffer for IPv4 and IPv6 prefixes */
    u_int metric, status_byte, bit_length, byte_length, sublen, processed, subtlvtype, subtlvlen;

    if (!TTEST2(*tptr, 4))
        return (0);
    metric = EXTRACT_32BITS(tptr);
    processed=4;
    tptr+=4;
    
    if (afi == IPV4) {
        if (!TTEST2(*tptr, 1)) /* fetch status byte */
            return (0);
        status_byte=*(tptr++);
        bit_length = status_byte&0x3f;
        processed++;
#ifdef INET6
    } else if (afi == IPV6) {
        if (!TTEST2(*tptr, 1)) /* fetch status & prefix_len byte */
            return (0);
        status_byte=*(tptr++);
        bit_length=*(tptr++);
        processed+=2;
#endif
    } else
        return (0); /* somebody is fooling us */

    byte_length = (bit_length + 7) / 8; /* prefix has variable length encoding */
   
    if (!TTEST2(*tptr, byte_length))
        return (0);
    memset(prefix, 0, 16);              /* clear the copy buffer */
    memcpy(prefix,tptr,byte_length);    /* copy as much as is stored in the TLV */
    tptr+=byte_length;
    processed+=byte_length;

    if (afi == IPV4)
        printf("%sIPv4 prefix: %15s/%u",
               ident,
               ipaddr_string(prefix),
               bit_length);
#ifdef INET6
    if (afi == IPV6)
        printf("%sIPv6 prefix: %s/%u",
               ident,
               ip6addr_string(prefix),
               bit_length);
#endif 
   
    printf(", Distribution: %s, Metric: %u",
           ISIS_MASK_TLV_EXTD_IP_UPDOWN(status_byte) ? "down" : "up",
           metric);

    if (afi == IPV4 && ISIS_MASK_TLV_EXTD_IP_SUBTLV(status_byte))
        printf(", sub-TLVs present");
#ifdef INET6
    if (afi == IPV6)
        printf(", %s%s",
               ISIS_MASK_TLV_EXTD_IP6_IE(status_byte) ? "External" : "Internal",
               ISIS_MASK_TLV_EXTD_IP6_SUBTLV(status_byte) ? ", sub-TLVs present" : "");
#endif
    
    if ((ISIS_MASK_TLV_EXTD_IP_SUBTLV(status_byte)  && afi == IPV4) ||
        (ISIS_MASK_TLV_EXTD_IP6_SUBTLV(status_byte) && afi == IPV6)) {
        /* assume that one prefix can hold more
           than one subTLV - therefore the first byte must reflect
           the aggregate bytecount of the subTLVs for this prefix
        */
        if (!TTEST2(*tptr, 1))
            return (0);
        sublen=*(tptr++);
        processed+=sublen+1;
        printf(" (%u)",sublen);   /* print out subTLV length */
        
        while (sublen>0) {
            if (!TTEST2(*tptr,2))
                return (0);
            subtlvtype=*(tptr++);
            subtlvlen=*(tptr++);
            /* prepend the ident string */
            snprintf(ident_buffer, sizeof(ident_buffer), "%s  ",ident);
            if(!isis_print_ip_reach_subtlv(tptr,subtlvtype,subtlvlen,ident_buffer))
                return(0);
            tptr+=subtlvlen;
            sublen-=(subtlvlen+2);
        }
    }
    return (processed);
}

/*
 * isis_print
 * Decode IS-IS packets.  Return 0 on error.
 */

static int isis_print (const u_int8_t *p, u_int length)
{
    const struct isis_common_header *header;

    const struct isis_iih_lan_header *header_iih_lan;
    const struct isis_iih_ptp_header *header_iih_ptp;
    const struct isis_lsp_header *header_lsp;
    const struct isis_csnp_header *header_csnp;
    const struct isis_psnp_header *header_psnp;

    const struct isis_tlv_lsp *tlv_lsp;
    const struct isis_tlv_ptp_adj *tlv_ptp_adj;
    const struct isis_tlv_is_reach *tlv_is_reach;
    const struct isis_tlv_es_reach *tlv_es_reach;

    u_int8_t pdu_type, max_area, id_length, tlv_type, tlv_len, tmp, alen, lan_alen, prefix_len;
    u_int8_t ext_is_len, ext_ip_len, mt_len;
    const u_int8_t *optr, *pptr, *tptr;
    u_short packet_len,pdu_len;
    u_int i;

    packet_len=length;
    optr = p; /* initialize the _o_riginal pointer to the packet start -
                 need it for parsing the checksum TLV */
    header = (const struct isis_common_header *)p;
    TCHECK(*header);
    pptr = p+(ISIS_COMMON_HEADER_SIZE);
    header_iih_lan = (const struct isis_iih_lan_header *)pptr;
    header_iih_ptp = (const struct isis_iih_ptp_header *)pptr;
    header_lsp = (const struct isis_lsp_header *)pptr;
    header_csnp = (const struct isis_csnp_header *)pptr;
    header_psnp = (const struct isis_psnp_header *)pptr;

    /*
     * Sanity checking of the header.
     */

    if (header->version != ISIS_VERSION) {
	printf(", version %d packet not supported", header->version);
	return (0);
    }

    if ((header->id_length != SYSTEM_ID_LEN) && (header->id_length != 0)) {
	printf(", system ID length of %d is not supported",
	       header->id_length);
	return (0);
    }

    if (header->pdu_version != ISIS_VERSION) {
	printf(", version %d packet not supported", header->pdu_version);
	return (0);
    }

    max_area = header->max_area;
    switch(max_area) {
    case 0:
	max_area = 3;	 /* silly shit */
	break;
    case 255:
	printf(", bad packet -- 255 areas");
	return (0);
    default:
	break;
    }

    id_length = header->id_length;
    switch(id_length) {
    case 0:
        id_length = 6;	 /* silly shit again */
	break;
    case 1:              /* 1-8 are valid sys-ID lenghts */
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        break;
    case 255:
        id_length = 0;   /* entirely useless */
	break;
    default:
        break;
    }

    /* toss any non 6-byte sys-ID len PDUs */
    if (id_length != 6 ) { 
	printf(", bad packet -- illegal sys-ID length (%u)", id_length);
	return (0);
    }

    pdu_type=header->pdu_type;

    /* in non-verbose mode print the basic PDU Type plus PDU specific brief information*/
    if (vflag < 1) {
        printf(", IS-IS, %s",
               tok2str(isis_pdu_values,"unknown PDU-Type %u",pdu_type));

	switch (pdu_type) {

	case L1_LAN_IIH:
	case L2_LAN_IIH:
	    printf(", src-id %s",
                   isis_print_id(header_iih_lan->source_id,SYSTEM_ID_LEN));
	    printf(", lan-id %s, prio %u",
                   isis_print_id(header_iih_lan->lan_id,NODE_ID_LEN),
                   header_iih_lan->priority);
	    break;
	case PTP_IIH:
	    printf(", src-id %s", isis_print_id(header_iih_ptp->source_id,SYSTEM_ID_LEN));
	    break;
	case L1_LSP:
	case L2_LSP:
	    printf(", lsp-id %s, seq 0x%08x, lifetime %5us",
		   isis_print_id(header_lsp->lsp_id, LSP_ID_LEN),
		   EXTRACT_32BITS(header_lsp->sequence_number),
		   EXTRACT_16BITS(header_lsp->remaining_lifetime));
	    break;
	case L1_CSNP:
	case L2_CSNP:
	    printf(", src-id %s", isis_print_id(header_csnp->source_id,SYSTEM_ID_LEN));
	    break;
	case L1_PSNP:
	case L2_PSNP:
	    printf(", src-id %s", isis_print_id(header_psnp->source_id,SYSTEM_ID_LEN));
	    break;

	}
	printf(", length %u", length);

        return(1);
    }

    /* ok they seem to want to know everything - lets fully decode it */
    printf(", IS-IS, length: %u",length);

    printf("\n\t%s, hlen: %u, v: %u, pdu-v: %u, sys-id-len: %u (%u), max-area: %u (%u)",
           tok2str(isis_pdu_values,
                   "unknown, type %u",
                   pdu_type),
           header->fixed_len,
           header->version,
           header->pdu_version,
	   id_length,
	   header->id_length,
           max_area,
           header->max_area);

    if (vflag > 1) {
        if(!print_unknown_data(optr,"\n\t",8)) /* provide the _o_riginal pointer */
            return(0);                         /* for optionally debugging the common header */
    }

    switch (pdu_type) {

    case L1_LAN_IIH:
    case L2_LAN_IIH:
	if (header->fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE)) {
	    printf(", bogus fixed header length %u should be %lu",
		   header->fixed_len, (unsigned long)ISIS_IIH_LAN_HEADER_SIZE);
	    return (0);
	}

	pdu_len=EXTRACT_16BITS(header_iih_lan->pdu_len);
	if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
	}

	TCHECK(*header_iih_lan);
	printf("\n\t  source-id: %s,  holding time: %us, Flags: [%s]",
               isis_print_id(header_iih_lan->source_id,SYSTEM_ID_LEN),
               EXTRACT_16BITS(header_iih_lan->holding_time),
               tok2str(isis_iih_circuit_type_values,
                       "unknown circuit type 0x%02x",
                       header_iih_lan->circuit_type));

	printf("\n\t  lan-id:    %s, Priority: %u, PDU length: %u",
               isis_print_id(header_iih_lan->lan_id, NODE_ID_LEN),
               (header_iih_lan->priority) & PRIORITY_MASK,
               pdu_len);

        if (vflag > 1) {
            if(!print_unknown_data(pptr,"\n\t  ",ISIS_IIH_LAN_HEADER_SIZE))
                return(0);
        }

	packet_len -= (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE);
	pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_LAN_HEADER_SIZE);
	break;

    case PTP_IIH:
	if (header->fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE)) {
	    printf(", bogus fixed header length %u should be %lu",
		   header->fixed_len, (unsigned long)ISIS_IIH_PTP_HEADER_SIZE);
	    return (0);
	}

	pdu_len=EXTRACT_16BITS(header_iih_ptp->pdu_len);
	if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
	}

	TCHECK(*header_iih_ptp);
	printf("\n\t  source-id: %s, holding time: %us, Flags: [%s]",
               isis_print_id(header_iih_ptp->source_id,SYSTEM_ID_LEN),
               EXTRACT_16BITS(header_iih_ptp->holding_time),
               tok2str(isis_iih_circuit_type_values,
                       "unknown circuit type 0x%02x",
                       header_iih_ptp->circuit_type));

	printf("\n\t  circuit-id: 0x%02x, PDU length: %u",
               header_iih_ptp->circuit_id,
               pdu_len);

        if (vflag > 1) {
            if(!print_unknown_data(pptr,"\n\t  ",ISIS_IIH_PTP_HEADER_SIZE))
                return(0);
        }

	packet_len -= (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE);
	pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_IIH_PTP_HEADER_SIZE);
	break;

    case L1_LSP:
    case L2_LSP:
	if (header->fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_LSP_HEADER_SIZE)) {
	    printf(", bogus fixed header length %u should be %lu",
		   header->fixed_len, (unsigned long)ISIS_LSP_HEADER_SIZE);
	    return (0);
	}

	pdu_len=EXTRACT_16BITS(header_lsp->pdu_len);
	if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
	}

	TCHECK(*header_lsp);
	printf("\n\t  lsp-id: %s, seq: 0x%08x, lifetime: %5us\n\t  chksum: 0x%04x",
               isis_print_id(header_lsp->lsp_id, LSP_ID_LEN),
               EXTRACT_32BITS(header_lsp->sequence_number),
               EXTRACT_16BITS(header_lsp->remaining_lifetime),
               EXTRACT_16BITS(header_lsp->checksum));

        /* if this is a purge do not attempt to verify the checksum */
        if ( EXTRACT_16BITS(header_lsp->remaining_lifetime) == 0 &&
             EXTRACT_16BITS(header_lsp->checksum) == 0)
            printf(" (purged)");
        else
            /* verify the checksum -
             * checking starts at the lsp-id field at byte position [12]
             * hence the length needs to be reduced by 12 bytes */
            printf(" (%s)", (osi_cksum((u_int8_t *)header_lsp->lsp_id, length-12)) ? "incorrect" : "correct");

	printf(", PDU length: %u, Flags: [ %s",
               pdu_len,
               ISIS_MASK_LSP_OL_BIT(header_lsp->typeblock) ? "Overload bit set, " : "");

	if (ISIS_MASK_LSP_ATT_BITS(header_lsp->typeblock)) {
	    printf("%s", ISIS_MASK_LSP_ATT_DEFAULT_BIT(header_lsp->typeblock) ? "default " : "");
	    printf("%s", ISIS_MASK_LSP_ATT_DELAY_BIT(header_lsp->typeblock) ? "delay " : "");
	    printf("%s", ISIS_MASK_LSP_ATT_EXPENSE_BIT(header_lsp->typeblock) ? "expense " : "");
	    printf("%s", ISIS_MASK_LSP_ATT_ERROR_BIT(header_lsp->typeblock) ? "error " : "");
	    printf("ATT bit set, ");
	}
	printf("%s", ISIS_MASK_LSP_PARTITION_BIT(header_lsp->typeblock) ? "P bit set, " : "");
	printf("%s ]", tok2str(isis_lsp_istype_values,"Unknown(0x%x)",ISIS_MASK_LSP_ISTYPE_BITS(header_lsp->typeblock)));

        if (vflag > 1) {
            if(!print_unknown_data(pptr,"\n\t  ",ISIS_LSP_HEADER_SIZE))
                return(0);
        }

	packet_len -= (ISIS_COMMON_HEADER_SIZE+ISIS_LSP_HEADER_SIZE);
	pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_LSP_HEADER_SIZE);
	break;

    case L1_CSNP:
    case L2_CSNP:
	if (header->fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE)) {
	    printf(", bogus fixed header length %u should be %lu",
		   header->fixed_len, (unsigned long)ISIS_CSNP_HEADER_SIZE);
	    return (0);
	}

	pdu_len=EXTRACT_16BITS(header_csnp->pdu_len);
	if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
	}

	TCHECK(*header_csnp);
	printf("\n\t  source-id:    %s, PDU length: %u",
               isis_print_id(header_csnp->source_id, NODE_ID_LEN),
               pdu_len);
	printf("\n\t  start lsp-id: %s",
               isis_print_id(header_csnp->start_lsp_id, LSP_ID_LEN));
	printf("\n\t  end lsp-id:   %s",
               isis_print_id(header_csnp->end_lsp_id, LSP_ID_LEN));

        if (vflag > 1) {
            if(!print_unknown_data(pptr,"\n\t  ",ISIS_CSNP_HEADER_SIZE))
                return(0);
        }

	packet_len -= (ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE);
	pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_CSNP_HEADER_SIZE);
        break;

    case L1_PSNP:
    case L2_PSNP:
	if (header->fixed_len != (ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE)) {
	    printf("- bogus fixed header length %u should be %lu",
		   header->fixed_len, (unsigned long)ISIS_PSNP_HEADER_SIZE);
	    return (0);
	}

	pdu_len=EXTRACT_16BITS(header_psnp->pdu_len);
	if (packet_len>pdu_len) {
            packet_len=pdu_len; /* do TLV decoding as long as it makes sense */
            length=pdu_len;
	}

	TCHECK(*header_psnp);
	printf("\n\t  source-id:    %s, PDU length: %u",
               isis_print_id(header_psnp->source_id, NODE_ID_LEN),
               pdu_len);

        if (vflag > 1) {
            if(!print_unknown_data(pptr,"\n\t  ",ISIS_PSNP_HEADER_SIZE))
                return(0);
        }

	packet_len -= (ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE);
	pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE);
	break;

    default:
	if(!print_unknown_data(pptr,"\n\t  ",length))
	    return(0);
	return (0);
    }

    /*
     * Now print the TLV's.
     */

    while (packet_len >= 2) {
        if (pptr == snapend) {
	    return (1);
        }

	if (!TTEST2(*pptr, 2)) {
	    printf("\n\t\t packet exceeded snapshot (%ld) bytes",
                   (long)(pptr-snapend));
	    return (1);
	}
	tlv_type = *pptr++;
	tlv_len = *pptr++;
        tmp =tlv_len; /* copy temporary len & pointer to packet data */
        tptr = pptr;
	packet_len -= 2;
	if (tlv_len > packet_len) {
	    break;
	}

        /* first lets see if we know the TLVs name*/
	printf("\n\t    %s TLV #%u, length: %u",
               tok2str(isis_tlv_values,
                       "unknown",
                       tlv_type),
               tlv_type,
               tlv_len);

        if (tlv_len == 0) /* something is malformed */
            break;

        /* now check if we have a decoder otherwise do a hexdump at the end*/
	switch (tlv_type) {
	case TLV_AREA_ADDR:
	    if (!TTEST2(*tptr, 1))
		goto trunctlv;
	    alen = *tptr++;
	    while (tmp && alen < tmp) {
		printf("\n\t      Area address (length: %u): %s",
                       alen,
                       print_nsap(tptr, alen));
		tptr += alen;
		tmp -= alen + 1;
		if (tmp==0) /* if this is the last area address do not attemt a boundary check */
                    break;
		if (!TTEST2(*tptr, 1))
		    goto trunctlv;
		alen = *tptr++;
	    }
	    break;
	case TLV_ISNEIGH:
	    while (tmp >= ETHER_ADDR_LEN) {
                if (!TTEST2(*tptr, ETHER_ADDR_LEN))
                    goto trunctlv;
                printf("\n\t      SNPA: %s",isis_print_id(tptr,ETHER_ADDR_LEN));
                tmp -= ETHER_ADDR_LEN;
                tptr += ETHER_ADDR_LEN;
	    }
	    break;

        case TLV_ISNEIGH_VARLEN:
            if (!TTEST2(*tptr, 1) || tmp < 3) /* min. TLV length */
		goto trunctlv;
	    lan_alen = *tptr++; /* LAN adress length */
            tmp --;
            printf("\n\t      LAN address length %u bytes ",lan_alen);
	    while (tmp >= lan_alen) {
                if (!TTEST2(*tptr, lan_alen))
                    goto trunctlv;
                printf("\n\t\tIS Neighbor: %s",isis_print_id(tptr,lan_alen));
                tmp -= lan_alen;
                tptr +=lan_alen;
            }
            break;

	case TLV_PADDING:
	    break;

        case TLV_MT_IS_REACH:
            while (tmp >= 2+NODE_ID_LEN+3+1) {
                mt_len = isis_print_mtid(tptr, "\n\t      ");
                if (mt_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                tptr+=mt_len;
                tmp-=mt_len;

                ext_is_len = isis_print_ext_is_reach(tptr,"\n\t      ",tlv_type);
                if (ext_is_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                   
                tmp-=ext_is_len;
                tptr+=ext_is_len;
            }
            break;

        case TLV_IS_ALIAS_ID:
	    while (tmp >= NODE_ID_LEN+1) { /* is it worth attempting a decode ? */
	        ext_is_len = isis_print_ext_is_reach(tptr,"\n\t      ",tlv_type);
		if (ext_is_len == 0) /* did something go wrong ? */
	            goto trunctlv;
		tmp-=ext_is_len;
		tptr+=ext_is_len;
	    }
	    break;

        case TLV_EXT_IS_REACH:
            while (tmp >= NODE_ID_LEN+3+1) { /* is it worth attempting a decode ? */
                ext_is_len = isis_print_ext_is_reach(tptr,"\n\t      ",tlv_type);
                if (ext_is_len == 0) /* did something go wrong ? */
                    goto trunctlv;                   
                tmp-=ext_is_len;
                tptr+=ext_is_len;
            }
            break;
        case TLV_IS_REACH:
	    if (!TTEST2(*tptr,1))  /* check if there is one byte left to read out the virtual flag */
                goto trunctlv;
            printf("\n\t      %s",
                   tok2str(isis_is_reach_virtual_values,
                           "bogus virtual flag 0x%02x",
                           *tptr++));
	    tlv_is_reach = (const struct isis_tlv_is_reach *)tptr;
            while (tmp >= sizeof(struct isis_tlv_is_reach)) {
		if (!TTEST(*tlv_is_reach))
		    goto trunctlv;
		printf("\n\t      IS Neighbor: %s",
		       isis_print_id(tlv_is_reach->neighbor_nodeid, NODE_ID_LEN));
                isis_print_metric_block(&tlv_is_reach->isis_metric_block);
		tmp -= sizeof(struct isis_tlv_is_reach);
		tlv_is_reach++;
	    }
            break;

        case TLV_ESNEIGH:
	    tlv_es_reach = (const struct isis_tlv_es_reach *)tptr;
            while (tmp >= sizeof(struct isis_tlv_es_reach)) {
		if (!TTEST(*tlv_es_reach))
		    goto trunctlv;
		printf("\n\t      ES Neighbor: %s",
                       isis_print_id(tlv_es_reach->neighbor_sysid,SYSTEM_ID_LEN));
                isis_print_metric_block(&tlv_es_reach->isis_metric_block);
		tmp -= sizeof(struct isis_tlv_es_reach);
		tlv_es_reach++;
	    }
            break;

            /* those two TLVs share the same format */
	case TLV_INT_IP_REACH:
	case TLV_EXT_IP_REACH:
	    if (!isis_print_tlv_ip_reach(pptr, "\n\t      ", tlv_len))
		return (1);
	    break;

	case TLV_EXTD_IP_REACH:
	    while (tmp>0) {
                ext_ip_len = isis_print_extd_ip_reach(tptr, "\n\t      ", IPV4);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                tptr+=ext_ip_len;
		tmp-=ext_ip_len;
	    }
	    break;

        case TLV_MT_IP_REACH:
	    while (tmp>0) {
                mt_len = isis_print_mtid(tptr, "\n\t      ");
                if (mt_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                tptr+=mt_len;
                tmp-=mt_len;

                ext_ip_len = isis_print_extd_ip_reach(tptr, "\n\t      ", IPV4);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                tptr+=ext_ip_len;
		tmp-=ext_ip_len;
	    }
	    break;

#ifdef INET6
	case TLV_IP6_REACH:
	    while (tmp>0) {
                ext_ip_len = isis_print_extd_ip_reach(tptr, "\n\t      ", IPV6);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                tptr+=ext_ip_len;
		tmp-=ext_ip_len;
	    }
	    break;

	case TLV_MT_IP6_REACH:
	    while (tmp>0) {
                mt_len = isis_print_mtid(tptr, "\n\t      ");
                if (mt_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                tptr+=mt_len;
                tmp-=mt_len;

                ext_ip_len = isis_print_extd_ip_reach(tptr, "\n\t      ", IPV6);
                if (ext_ip_len == 0) /* did something go wrong ? */
                    goto trunctlv;
                tptr+=ext_ip_len;
		tmp-=ext_ip_len;
	    }
	    break;

	case TLV_IP6ADDR:
	    while (tmp>0) {
		if (!TTEST2(*tptr, 16))
		    goto trunctlv;

                printf("\n\t      IPv6 interface address: %s",
		       ip6addr_string(tptr));

		tptr += 16;
		tmp -= 16;
	    }
	    break;
#endif
	case TLV_AUTH:
	    if (!TTEST2(*tptr, 1))
		goto trunctlv;

            printf("\n\t      %s: ",
                   tok2str(isis_subtlv_auth_values,
                           "unknown Authentication type 0x%02x",
                           *tptr));

	    switch (*tptr) {
	    case SUBTLV_AUTH_SIMPLE:
		for(i=1;i<tlv_len;i++) {
		    if (!TTEST2(*(tptr+i), 1))
			goto trunctlv;
		    printf("%c",*(tptr+i));
		}
		break;
	    case SUBTLV_AUTH_MD5:
		for(i=1;i<tlv_len;i++) {
		    if (!TTEST2(*(tptr+i), 1))
			goto trunctlv;
		    printf("%02x",*(tptr+i));
		}
		if (tlv_len != SUBTLV_AUTH_MD5_LEN+1)
                    printf(", (malformed subTLV) ");
		break;
	    case SUBTLV_AUTH_PRIVATE:
	    default:
		if(!print_unknown_data(tptr+1,"\n\t\t  ",tlv_len-1))
		    return(0);
		break;
	    }
	    break;

	case TLV_PTP_ADJ:
	    tlv_ptp_adj = (const struct isis_tlv_ptp_adj *)tptr;
	    if(tmp>=1) {
		if (!TTEST2(*tptr, 1))
		    goto trunctlv;
		printf("\n\t      Adjacency State: %s (%u)",
		       tok2str(isis_ptp_adjancey_values, "unknown", *tptr),
                        *tptr);
		tmp--;
	    }
	    if(tmp>sizeof(tlv_ptp_adj->extd_local_circuit_id)) {
		if (!TTEST2(tlv_ptp_adj->extd_local_circuit_id,
                            sizeof(tlv_ptp_adj->extd_local_circuit_id)))
		    goto trunctlv;
		printf("\n\t      Extended Local circuit-ID: 0x%08x",
		       EXTRACT_32BITS(tlv_ptp_adj->extd_local_circuit_id));
		tmp-=sizeof(tlv_ptp_adj->extd_local_circuit_id);
	    }
	    if(tmp>=SYSTEM_ID_LEN) {
		if (!TTEST2(tlv_ptp_adj->neighbor_sysid, SYSTEM_ID_LEN))
		    goto trunctlv;
		printf("\n\t      Neighbor System-ID: %s",
		       isis_print_id(tlv_ptp_adj->neighbor_sysid,SYSTEM_ID_LEN));
		tmp-=SYSTEM_ID_LEN;
	    }
	    if(tmp>=sizeof(tlv_ptp_adj->neighbor_extd_local_circuit_id)) {
		if (!TTEST2(tlv_ptp_adj->neighbor_extd_local_circuit_id,
                            sizeof(tlv_ptp_adj->neighbor_extd_local_circuit_id)))
		    goto trunctlv;
		printf("\n\t      Neighbor Extended Local circuit-ID: 0x%08x",
		       EXTRACT_32BITS(tlv_ptp_adj->neighbor_extd_local_circuit_id));
	    }
	    break;

	case TLV_PROTOCOLS:
	    printf("\n\t      NLPID(s): ");
	    while (tmp>0) {
		if (!TTEST2(*(tptr), 1))
		    goto trunctlv;
		printf("%s (0x%02x)",
                       tok2str(osi_nlpid_values,
                               "unknown",
                               *tptr),
                       *tptr);
		if (tmp>1) /* further NPLIDs ? - put comma */
		    printf(", ");
                tptr++;
                tmp--;
	    }
	    break;

	case TLV_TE_ROUTER_ID:
	    if (!TTEST2(*pptr, 4))
		goto trunctlv;
	    printf("\n\t      Traffic Engineering Router ID: %s", ipaddr_string(pptr));
	    break;

	case TLV_IPADDR:
	    while (tmp>0) {
		if (!TTEST2(*tptr, 4))
		    goto trunctlv;
		printf("\n\t      IPv4 interface address: %s", ipaddr_string(tptr));
		tptr += 4;
		tmp -= 4;
	    }
	    break;

	case TLV_HOSTNAME:
	    printf("\n\t      Hostname: ");
	    while (tmp>0) {
		if (!TTEST2(*tptr, 1))
		    goto trunctlv;
		printf("%c",*tptr++);
                tmp--;
	    }
	    break;

	case TLV_SHARED_RISK_GROUP:
	    if (!TTEST2(*tptr, NODE_ID_LEN))
                goto trunctlv;
	    printf("\n\t      IS Neighbor: %s", isis_print_id(tptr, NODE_ID_LEN));
	    tptr+=(NODE_ID_LEN);
	    tmp-=(NODE_ID_LEN);

	    if (!TTEST2(*tptr, 1))
                goto trunctlv;
	    printf(", Flags: [%s]", ISIS_MASK_TLV_SHARED_RISK_GROUP(*tptr++) ? "numbered" : "unnumbered");
	    tmp--;

	    if (!TTEST2(*tptr,4))
                goto trunctlv;
	    printf("\n\t      IPv4 interface address: %s", ipaddr_string(tptr));
	    tptr+=4;
	    tmp-=4;

	    if (!TTEST2(*tptr,4))
                goto trunctlv;
	    printf("\n\t      IPv4 neighbor address: %s", ipaddr_string(tptr));
	    tptr+=4;
	    tmp-=4;

	    while (tmp>0) {
                if (!TTEST2(*tptr, 4))
                    goto trunctlv;
                printf("\n\t      Link-ID: 0x%08x", EXTRACT_32BITS(tptr));
                tptr+=4;
                tmp-=4;
	    }
	    break;

	case TLV_LSP:
	    tlv_lsp = (const struct isis_tlv_lsp *)tptr;
	    while(tmp>0) {
		if (!TTEST((tlv_lsp->lsp_id)[LSP_ID_LEN-1]))
		    goto trunctlv;
		printf("\n\t      lsp-id: %s",
                       isis_print_id(tlv_lsp->lsp_id, LSP_ID_LEN));
		if (!TTEST2(tlv_lsp->sequence_number, 4))
		    goto trunctlv;
		printf(", seq: 0x%08x",EXTRACT_32BITS(tlv_lsp->sequence_number));
		if (!TTEST2(tlv_lsp->remaining_lifetime, 2))
		    goto trunctlv;
		printf(", lifetime: %5ds",EXTRACT_16BITS(tlv_lsp->remaining_lifetime));
		if (!TTEST2(tlv_lsp->checksum, 2))
		    goto trunctlv;
		printf(", chksum: 0x%04x",EXTRACT_16BITS(tlv_lsp->checksum));
		tmp-=sizeof(struct isis_tlv_lsp);
		tlv_lsp++;
	    }
	    break;

	case TLV_CHECKSUM:
	    if (!TTEST2(*tptr, 2))
		goto trunctlv;
	    printf("\n\t      checksum: 0x%04x ", EXTRACT_16BITS(tptr));
            /* do not attempt to verify the checksum if it is zero
             * most likely a HMAC-MD5 TLV is also present and
             * to avoid conflicts the checksum TLV is zeroed.
             * see rfc3358 for details
             */
            if (EXTRACT_16BITS(tptr) == 0)
                printf("(unverified)");
            else printf("(%s)", osi_cksum(optr, length) ? "incorrect" : "correct");
	    break;

	case TLV_MT_SUPPORTED:
	    while (tmp>1) {
		/* length can only be a multiple of 2, otherwise there is
		   something broken -> so decode down until length is 1 */
		if (tmp!=1) {
                    mt_len = isis_print_mtid(tptr, "\n\t      ");
                    if (mt_len == 0) /* did something go wrong ? */
                        goto trunctlv;
                    tptr+=mt_len;
                    tmp-=mt_len;
		} else {
		    printf("\n\t      malformed MT-ID");
		    break;
		}
	    }
	    break;

	case TLV_RESTART_SIGNALING:
            if (!TTEST2(*tptr, 3))
                goto trunctlv;
            printf("\n\t      Flags [%s], Remaining holding time %us",
                   bittok2str(isis_restart_flag_values, "none", *tptr),
                   EXTRACT_16BITS(tptr+1));
	    tptr+=3;
	    break;

        case TLV_IDRP_INFO:
            if (!TTEST2(*tptr, 1))
                goto trunctlv;
            printf("\n\t      Inter-Domain Information Type: %s",
                   tok2str(isis_subtlv_idrp_values,
                           "Unknown (0x%02x)",
                           *tptr));
            switch (*tptr++) {
            case SUBTLV_IDRP_ASN:
                if (!TTEST2(*tptr, 2)) /* fetch AS number */
                    goto trunctlv;
                printf("AS Number: %u",EXTRACT_16BITS(tptr));
                break;
            case SUBTLV_IDRP_LOCAL:
            case SUBTLV_IDRP_RES:
            default:
                if(!print_unknown_data(tptr,"\n\t      ",tlv_len-1))
                    return(0);
                break;
            }
            break;

        case TLV_LSP_BUFFERSIZE:
            if (!TTEST2(*tptr, 2))
                goto trunctlv;
            printf("\n\t      LSP Buffersize: %u",EXTRACT_16BITS(tptr));
            break;

        case TLV_PART_DIS:
            while (tmp >= SYSTEM_ID_LEN) {
                if (!TTEST2(*tptr, SYSTEM_ID_LEN))
                    goto trunctlv;
                printf("\n\t      %s",isis_print_id(tptr,SYSTEM_ID_LEN));
                tptr+=SYSTEM_ID_LEN;
                tmp-=SYSTEM_ID_LEN;
            }
            break;

        case TLV_PREFIX_NEIGH:
            if (!TTEST2(*tptr, sizeof(struct isis_metric_block)))
                goto trunctlv;
            printf("\n\t      Metric Block");
            isis_print_metric_block((const struct isis_metric_block *)tptr);
            tptr+=sizeof(struct isis_metric_block);
            tmp-=sizeof(struct isis_metric_block);

            while(tmp>0) {
                if (!TTEST2(*tptr, 1))
                    goto trunctlv;
                prefix_len=*tptr++; /* read out prefix length in semioctets*/
                tmp--;
                if (!TTEST2(*tptr, prefix_len/2))
                    goto trunctlv;
                printf("\n\t\tAddress: %s/%u",
                       print_nsap(tptr,prefix_len/2),
                       prefix_len*4);
                tptr+=prefix_len/2;
                tmp-=prefix_len/2;
            }
            break;

        case TLV_IIH_SEQNR:
            if (!TTEST2(*tptr, 4)) /* check if four bytes are on the wire */
                goto trunctlv;
            printf("\n\t      Sequence number: %u", EXTRACT_32BITS(tptr) );
            break;

        case TLV_VENDOR_PRIVATE:
            if (!TTEST2(*tptr, 3)) /* check if enough byte for a full oui */
                goto trunctlv;
            printf("\n\t      Vendor OUI Code: 0x%06x", EXTRACT_24BITS(tptr) );
            tptr+=3;
            tmp-=3;
            if (tmp > 0) /* hexdump the rest */
                if(!print_unknown_data(tptr,"\n\t\t",tmp))
                    return(0);
            break;
            /*
             * FIXME those are the defined TLVs that lack a decoder
             * you are welcome to contribute code ;-)
             */

        case TLV_DECNET_PHASE4:
        case TLV_LUCENT_PRIVATE:
        case TLV_IPAUTH:
        case TLV_NORTEL_PRIVATE1:
        case TLV_NORTEL_PRIVATE2:

	default:
            if (vflag <= 1) {
                if(!print_unknown_data(pptr,"\n\t\t",tlv_len))
                    return(0);
            }
	    break;
	}
        /* do we want to see an additionally hexdump ? */
        if (vflag> 1) {
	    if(!print_unknown_data(pptr,"\n\t      ",tlv_len))
	        return(0);
        }

	pptr += tlv_len;
	packet_len -= tlv_len;
    }

    if (packet_len != 0) {
	printf("\n\t      %u straggler bytes", packet_len);
    }
    return (1);

 trunc:
    fputs("[|isis]", stdout);
    return (1);

 trunctlv:
    printf("\n\t\t packet exceeded snapshot");
    return(1);
}

/*
 * Verify the checksum.  See 8473-1, Appendix C, section C.4.
 */

static int
osi_cksum(const u_int8_t *tptr, u_int len)
{
	int32_t c0 = 0, c1 = 0;

	while ((int)--len >= 0) {
		c0 += *tptr++;
		c0 %= 255;
		c1 += c0;
		c1 %= 255;
	}
	return (c0 | c1);
}
