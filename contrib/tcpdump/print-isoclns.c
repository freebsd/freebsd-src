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
 *
 * Extensively modified by Hannes Gredler (hannes@juniper.net) for more
 * complete IS-IS support.
 *
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-isoclns.c,v 1.36 2002/01/10 09:33:23 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "ether.h"
#include "extract.h"

#define	NLPID_CLNS	129	/* 0x81 */
#define	NLPID_ESIS	130	/* 0x82 */
#define	NLPID_ISIS	131	/* 0x83 */
#define NLPID_IP6       0x8e
#define NLPID_IP        0xcc  
#define	NLPID_NULLNS	0

/*
 * IS-IS is defined in ISO 10589.  Look there for protocol definitions.
 */

#define SYSTEM_ID_LEN	ETHER_ADDR_LEN
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


/*
 * A TLV is a tuple of a type, length and a value and is normally used for
 * encoding information in all sorts of places.  This is an enumeration of
 * the well known types.
 */

#define TLV_AREA_ADDR           1
#define TLV_IS_REACH            2
#define TLV_ES_REACH            3
#define TLV_SUMMARY             5
#define TLV_ISNEIGH     	6
#define TLV_PADDING             8
#define TLV_LSP                 9
#define TLV_AUTH                10
#define TLV_CHECKSUM		12
#define TLV_EXT_IS_REACH	22
#define TLV_IP_REACH		128
#define TLV_PROTOCOLS		129
#define TLV_IP_REACH_EXT	130
#define TLV_IDRP_INFO		131
#define TLV_IPADDR              132
#define TLV_IPAUTH              133
#define TLV_TE_ROUTER_ID	134
#define TLV_EXT_IP_REACH	135
#define	TLV_HOSTNAME		137
#define TLV_RESTART_SIGNALING   211
#define	TLV_MT_IS_REACH		222
#define	TLV_MT_SUPPORTED	229
#define TLV_IP6ADDR             232
#define	TLV_MT_REACH		235
#define	TLV_IP6_REACH		236
#define TLV_PTP_ADJ		240

#define SUBTLV_EXT_IS_REACH_ADMIN_GROUP        3
#define SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR     6
#define SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR 8
#define SUBTLV_EXT_IS_REACH_MAX_LINK_BW        9
#define SUBTLV_EXT_IS_REACH_RESERVABLE_BW      10
#define SUBTLV_EXT_IS_REACH_UNRESERVED_BW      11
#define SUBTLV_EXT_IS_REACH_TE_METRIC          18

#define SUBTLV_AUTH_SIMPLE      1
#define SUBTLV_AUTH_MD5         54

#define ISIS_MASK_LEVEL_BITS(x)            ((x)&0x1) 

#define ISIS_MASK_LSP_OL_BIT(x)            ((x)&0x4)
#define ISIS_MASK_LSP_ISTYPE_BITS(x)       ((x)&0x3)
#define ISIS_MASK_LSP_PARTITION_BIT(x)     ((x)&0x80)
#define ISIS_MASK_LSP_ATT_BITS(x)          ((x)&0x78)
#define ISIS_MASK_LSP_ATT_ERROR_BIT(x)     ((x)&0x40)
#define ISIS_MASK_LSP_ATT_EXPENSE_BIT(x)   ((x)&0x20)
#define ISIS_MASK_LSP_ATT_DELAY_BIT(x)     ((x)&0x10)
#define ISIS_MASK_LSP_ATT_DEFAULT_BIT(x)   ((x)&0x8)

#define ISIS_MASK_TLV_EXT_IP_UPDOWN(x)     ((x)&0x80)
#define ISIS_MASK_TLV_EXT_IP_SUBTLV(x)     ((x)&0x40)

#define ISIS_MASK_TLV_IP6_UPDOWN(x)        ((x)&0x80)
#define ISIS_MASK_TLV_IP6_IE(x)            ((x)&0x40)
#define ISIS_MASK_TLV_IP6_SUBTLV(x)        ((x)&0x20)

#define ISIS_MASK_RESTART_RR(x)            ((x)&0x1)
#define ISIS_MASK_RESTART_RA(x)            ((x)&0x2)

#define ISIS_LSP_TLV_METRIC_SUPPORTED(x)   ((x)&0x80)
#define ISIS_LSP_TLV_METRIC_IE(x)          ((x)&0x40)
#define ISIS_LSP_TLV_METRIC_UPDOWN(x)      ((x)&0x80)
#define ISIS_LSP_TLV_METRIC_VALUE(x)	   ((x)&0x3f)

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

static struct tok isis_nlpid_values[] = {
	{ NLPID_CLNS,	"CLNS"},
	{ NLPID_IP,     "IPv4"},
	{ NLPID_IP6,	"IPv6"},
	{ 0,            "unknown" }
};

/*
 * Katz's point to point adjacency TLV uses codes to tell us the state of
 * the remote adjacency.  Enumerate them.
 */

#define ISIS_PTP_ADJ_UP   0
#define ISIS_PTP_ADJ_INIT 1
#define ISIS_PTP_ADJ_DOWN 2

static int osi_cksum(const u_char *, u_int, u_char *);
static void esis_print(const u_char *, u_int);
static int isis_print(const u_char *, u_int);


static struct tok isis_ptp_adjancey_values[] = {
	{ ISIS_PTP_ADJ_UP,    "Up" },
	{ ISIS_PTP_ADJ_INIT,  "Initializing" },
	{ ISIS_PTP_ADJ_DOWN,  "Down" }
};

struct isis_tlv_ptp_adj {
    u_char adjacency_state;
    u_char ext_local_circuit_id[4];
    u_char neighbor_sysid[SYSTEM_ID_LEN];
    u_char neighbor_ext_local_circuit_id[4]; 
};

struct isis_tlv_ip_reach {
    u_char metric_default;
    u_char metric_delay;
    u_char metric_expense;
    u_char metric_error;
    u_char prefix[4];
    u_char mask[4];
};

struct isis_tlv_is_reach {
    u_char metric_default;
    u_char metric_delay;
    u_char metric_expense;
    u_char metric_error;
    u_char neighbor_nodeid[SYSTEM_ID_LEN+1];
};


struct isis_common_header {
    u_char nlpid;
    u_char fixed_len;
    u_char version;			/* Protocol version? */
    u_char id_length;
    u_char pdu_type;		        /* 3 MSbs are reserved */
    u_char pkt_version;			/* Packet format version? */
    u_char reserved;
    u_char max_area;
};

struct isis_iih_lan_header {
    u_char circuit_type;
    u_char source_id[SYSTEM_ID_LEN];
    u_char holding_time[2];
    u_char pdu_len[2];
    u_char priority;
    u_char lan_id[SYSTEM_ID_LEN+1];
};

struct isis_iih_ptp_header {
    u_char circuit_type;
    u_char source_id[SYSTEM_ID_LEN];
    u_char holding_time[2];
    u_char pdu_len[2];
    u_char circuit_id;
};

struct isis_lsp_header {
    u_char pdu_len[2];
    u_char remaining_lifetime[2];
    u_char lsp_id[SYSTEM_ID_LEN+2];
    u_char sequence_number[4];
    u_char checksum[2];
    u_char typeblock;
};

struct isis_csnp_header {
    u_char pdu_len[2];
    u_char source_id[SYSTEM_ID_LEN+1];
    u_char start_lsp_id[SYSTEM_ID_LEN+2];
    u_char end_lsp_id[SYSTEM_ID_LEN+2];
};

struct isis_psnp_header {
    u_char pdu_len[2];
    u_char source_id[SYSTEM_ID_LEN+1];
};

struct isis_tlv_lsp {
    u_char remaining_lifetime[2];
    u_char lsp_id[SYSTEM_ID_LEN+2];
    u_char sequence_number[4];
    u_char checksum[2];
};
    
#define ISIS_COMMON_HEADER_SIZE (sizeof(struct isis_common_header))
#define ISIS_IIH_LAN_HEADER_SIZE (sizeof(struct isis_iih_lan_header))
#define ISIS_IIH_PTP_HEADER_SIZE (sizeof(struct isis_iih_ptp_header))
#define ISIS_LSP_HEADER_SIZE (sizeof(struct isis_lsp_header))
#define ISIS_CSNP_HEADER_SIZE (sizeof(struct isis_csnp_header))
#define ISIS_PSNP_HEADER_SIZE (sizeof(struct isis_psnp_header))

void isoclns_print(const u_char *p, u_int length, u_int caplen,
	      const u_char *esrc, const u_char *edst)
{
	u_char pdu_type;
	const struct isis_common_header *header;
	
	header = (const struct isis_common_header *)p;
	pdu_type = header->pdu_type & PDU_TYPE_MASK;

	if (caplen < 1) {
		printf("[|iso-clns] ");
		if (!eflag && esrc != NULL && edst != NULL)
			printf("%s > %s",
			       etheraddr_string(esrc),
			       etheraddr_string(edst));
		return;
	}

	switch (*p) {

	case NLPID_CLNS:
		(void)printf("CLNS(%d)", length);
		if (!eflag && esrc != NULL && edst != NULL)
			(void)printf(", %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		break;

	case NLPID_ESIS:
		(void)printf("ESIS");
		if (!eflag && esrc != NULL && edst != NULL)
			(void)printf(", %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		esis_print(p, length);
		return;

	case NLPID_ISIS:
		(void)printf("ISIS(%d)", length);
		if (!eflag && esrc != NULL && edst != NULL)
			(void)printf(", %s > %s",
			     etheraddr_string(esrc),
			     etheraddr_string(edst));
		if (!isis_print(p, length))
			default_print_unaligned(p, caplen);
		break;

	case NLPID_NULLNS:
		(void)printf("ISO NULLNS(%d)", length);
		if (!eflag && esrc != NULL && edst != NULL)
			(void)printf(", %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		break;

	default:
		(void)printf("CLNS %02x(%d)", p[0], length);
		if (!eflag && esrc != NULL && edst != NULL)
			(void)printf(", %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		if (caplen > 1)
			default_print_unaligned(p, caplen);
		break;
	}
}

#define	ESIS_REDIRECT	6
#define	ESIS_ESH	2
#define	ESIS_ISH	4

struct esis_hdr {
	u_char version;
	u_char reserved;
	u_char type;
	u_char tmo[2];
	u_char cksum[2];
};

static void
esis_print(const u_char *p, u_int length)
{
	const u_char *ep;
	u_int li;
	const struct esis_hdr *eh;
	u_char off[2];

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
	switch (eh->type & 0x1f) {

	case ESIS_REDIRECT:
		printf(" redirect");
		break;

	case ESIS_ESH:
		printf(" esh");
		break;

	case ESIS_ISH:
		printf(" ish");
		break;

	default:
		printf(" type %d", eh->type & 0x1f);
		break;
	}
	off[0] = eh->cksum[0];
	off[1] = eh->cksum[1];
	if (vflag && osi_cksum(p, li, off)) {
		printf(" bad cksum (got %02x%02x)",
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
		const u_char *dst, *snpa, *is;

		dst = p; p += *p + 1;
		if (p > snapend)
			return;
		printf("\n\t\t\t %s", isonsap_string(dst));
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
#if 0
	case ESIS_ESH:
		printf(" esh");
		break;
#endif
	case ESIS_ISH: {
		const u_char *is;

		is = p; p += *p + 1;
		if (p > ep) {
			printf(" [bad li]");
			return;
		}
		if (p > snapend)
			return;
		if (!qflag)
			printf("\n\t\t\t %s", isonsap_string(is));
		li = ep - p;
		break;
	}

	default:
		(void)printf(" len=%d", length);
		if (length && p < snapend) {
			length = snapend - p;
			default_print(p, length);
		}
		return;
	}
	if (vflag)
		while (p < ep && li) {
			u_int op, opli;
			const u_char *q;

			if (snapend - p < 2)
				return;
			if (li < 2) {
				printf(" bad opts/li");
				return;
			}
			op = *p++;
			opli = *p++;
			li -= 2;
			if (opli > li) {
				printf(" opt (%d) too long", op);
				return;
			}
			li -= opli;
			q = p;
			p += opli;
			if (snapend < p)
				return;
			if (op == 198 && opli == 2) {
				printf(" tmo=%d", q[0] * 256 + q[1]);
				continue;
			}
			printf (" %d:<", op);
			while (opli-- > 0)
				printf("%02x", *q++);
			printf (">");
		}
}

/*
 * print_nsap
 * Print out an NSAP. 
 */
static int
print_nsap(register const u_char *cp, register int length)
{
	int i;

	for (i = 0; i < length; i++) {
		if (!TTEST2(*cp, 1))
			return (0);
		printf("%02x", *cp++);
		if (((i & 1) == 0) && (i + 1 < length)) {
			printf(".");
		}
	}
	return (1);
}

static int
isis_print_sysid(const u_char *cp)
{
	int i;

	for (i = 1; i <= 6; i++) {
		if (!TTEST2(*cp, 1))
			return (0);
		printf("%02x", *cp++);
		if ((i==2)^(i==4)) {
			printf(".");
		}
	}
	return (1);
}

static int
isis_print_nodeid(const u_char *cp)
{
	int i;

	for (i = 1; i <= 7; i++) {
		if (!TTEST2(*cp, 1))
			return (0);
		printf("%02x", *cp++);
		if ((i & 1) == 0) {
			printf(".");
		}
	}
	return (1);
}

static void
isis_print_lspid(const u_char *cp)
{
	int i;

	for (i = 1; i <= 7; i++) {
		printf("%02x", *cp++);
		if ((i & 1) == 0)
			printf(".");
	}
	printf("-%02x", *cp);
}

static int
isis_print_tlv_ip_reach (const u_char *cp, int length)
{
	int bitmasks[33] = {
		0x00000000,
		0x80000000, 0xc0000000, 0xe0000000, 0xf0000000,
		0xf8000000, 0xfc000000, 0xfe000000, 0xff000000,
		0xff800000, 0xffc00000, 0xffe00000, 0xfff00000,
		0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000,
		0xffff8000, 0xffffc000, 0xffffe000, 0xfffff000,
		0xfffff800, 0xfffffc00, 0xfffffe00, 0xffffff00,
		0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0,
		0xfffffff8, 0xfffffffc, 0xfffffffe, 0xffffffff
	};
	int mask, prefix_len;
	const struct isis_tlv_ip_reach *tlv_ip_reach;

	tlv_ip_reach = (const struct isis_tlv_ip_reach *)cp;

	while (length > 0) {
		if (length < sizeof(*tlv_ip_reach)) {
			printf("short IP reachability (%d vs %lu)", length,
			    (unsigned long)sizeof(*tlv_ip_reach));
			return (0);
		}

		if (!TTEST(*tlv_ip_reach))
		    return (0);

		mask = EXTRACT_32BITS(tlv_ip_reach->mask);
		prefix_len = 0;

		while (prefix_len <= 33) {
			if (bitmasks[prefix_len++] == mask) {
				prefix_len--;
				break;
			}
		}

		/*
		 * 34 indicates no match -> must be a discontiguous netmask
		 * lets dump the mask, otherwise print the prefix_len
		 */
		if (prefix_len == 34) 
			printf("\n\t\t\tIPv4 prefix: %u.%u.%u.%u mask %u.%u.%u.%u",
			    (tlv_ip_reach->prefix)[0],
			    (tlv_ip_reach->prefix)[1],
			    (tlv_ip_reach->prefix)[2],
			    (tlv_ip_reach->prefix)[3],
			    (tlv_ip_reach->mask)[0], (tlv_ip_reach->mask)[1],
			    (tlv_ip_reach->mask)[2], (tlv_ip_reach->mask)[3]);
		else 
			printf("\n\t\t\tIPv4 prefix: %u.%u.%u.%u/%u",
			    (tlv_ip_reach->prefix)[0],
			    (tlv_ip_reach->prefix)[1],
			    (tlv_ip_reach->prefix)[2],
			    (tlv_ip_reach->prefix)[3], prefix_len);

		printf("\n\t\t\t  Default Metric: %02d, %s, Distribution: %s",
		    ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->metric_default),
		    ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->metric_default) ? "External" : "Internal",
		    ISIS_LSP_TLV_METRIC_UPDOWN(tlv_ip_reach->metric_default) ? "down" : "up");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->metric_delay))
			printf("\n\t\t\t  Delay Metric: %02d, %s",
			    ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->metric_delay),
			    ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->metric_delay) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->metric_expense))
			printf("\n\t\t\t  Expense Metric: %02d, %s",
			    ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->metric_expense),
			    ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->metric_expense) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_ip_reach->metric_error))
			printf("\n\t\t\t  Error Metric: %02d, %s",
			    ISIS_LSP_TLV_METRIC_VALUE(tlv_ip_reach->metric_error),
			    ISIS_LSP_TLV_METRIC_IE(tlv_ip_reach->metric_error) ? "External" : "Internal");

		length -= sizeof(struct isis_tlv_ip_reach);
		tlv_ip_reach++;
	}
	return (1);
}

/*
 * isis_print
 * Decode IS-IS packets.  Return 0 on error.
 */

static int isis_print (const u_char *p, u_int length)
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

    u_char pdu_type, max_area, type, len, tmp, alen, subl, subt, tslen, ttslen;
    const u_char *optr, *pptr, *tptr;
    u_short packet_len,pdu_len;
    u_int i,j,bit_length,byte_length,metric;
    u_char prefix[4]; /* copy buffer for ipv4 prefixes */
#ifdef INET6
    u_char prefix6[16]; /* copy buffer for ipv6 prefixes */
#endif
    u_char off[2];
    float bw; /* copy buffer for several subTLVs of the extended IS reachability TLV */

    packet_len=length;
    optr = p; /* initialize the _o_riginal pointer - need it for parsing the checksum TLV */
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
    if (header->nlpid != NLPID_ISIS) {
	printf(", coding error!");
	return (0);
    }

    if (header->version != ISIS_VERSION) {
	printf(", version %d packet not supported", header->version);
	return (0);
    }

    if ((header->id_length != SYSTEM_ID_LEN) && (header->id_length != 0)) {
	printf(", system ID length of %d is not supported",
	       header->id_length);
	return (0);
    }
    
    if (header->pkt_version != ISIS_VERSION) {
	printf(", version %d packet not supported", header->pkt_version);
	return (0);
    }

    max_area = header->max_area;
    switch(max_area) {
    case 0:
	max_area = 3;			/* silly shit */
	break;
    case 255:
	printf(", bad packet -- 255 areas");
	return (0);
    default:
	break;
    }

    printf(", hlen: %u, v: %u, sys-id-len: 6 (0), max-area: %u (%u)",
           header->fixed_len,
           header->pkt_version,
           max_area,
           header->max_area);
           
    pdu_type=header->pdu_type;
     
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

	printf(", L%s Lan IIH (%u)",
	       ISIS_MASK_LEVEL_BITS(pdu_type) ? "1" : "2",
	       pdu_len);
 
	TCHECK(*header_iih_lan);
	printf("\n\t\t  source-id: ");
	isis_print_sysid(header_iih_lan->source_id);
	printf(", holding time: %us",EXTRACT_16BITS(header_iih_lan->holding_time));
	switch(header_iih_lan->circuit_type) {

	case 1:
	    printf(", Level 1 only");
	    break;

	case 2:
	    printf(", Level 2 only");
	    break;

	case 3:
	    printf(", Level 1, Level 2");
	    break; 

	default:
	    printf(", unknown 0x%02x", header_iih_lan->circuit_type);     
	    break;
	}
	printf("\n\t\t  lan-id:    ");
	isis_print_nodeid(header_iih_lan->lan_id);
	printf(", Priority: %u",(header_iih_lan->priority) & PRIORITY_MASK);            

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
	
	printf(", PTP IIH (%u)",pdu_len);
	TCHECK(*header_iih_ptp);
	printf("\n\t\t  source-id: ");
	isis_print_sysid(header_iih_ptp->source_id);
	printf(", holding time: %us",EXTRACT_16BITS(header_iih_ptp->holding_time));
	printf(", circuit-id: 0x%02x", header_iih_ptp->circuit_id);
	switch(header_iih_ptp->circuit_type) {

	case 1:
	    printf(", Level 1 only");
	    break;

	case 2:
	    printf(", Level 2 only");
	    break;

	case 3:
	    printf(", Level 1, Level 2");
	    break; 

	default:
	    printf(", unknown 0x%02x", header_iih_ptp->circuit_type);     
	    break;
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

	if (pdu_type == L1_LSP)	
	    printf(", L1 LSP (%u)",pdu_len);
	else if (pdu_type == L2_LSP)	
	    printf(", L2 LSP (%u)",pdu_len);   

	TCHECK(*header_lsp);
	printf("\n\t\t  lsp-id: ");
	isis_print_lspid(header_lsp->lsp_id);
	printf(", sequence number: 0x%08x",EXTRACT_32BITS(header_lsp->sequence_number));
	printf(", lifetime: %5us",EXTRACT_16BITS(header_lsp->remaining_lifetime));
	printf("\n\t\t  checksum: 0x%04x",EXTRACT_16BITS(header_lsp->checksum));
	      
	printf(", %s", ISIS_MASK_LSP_OL_BIT(header_lsp->typeblock) ? "Overload bit set, " : "");

	if (ISIS_MASK_LSP_ATT_BITS(header_lsp->typeblock)) {
	    printf("%s", ISIS_MASK_LSP_ATT_DEFAULT_BIT(header_lsp->typeblock) ? "default " : "");
	    printf("%s", ISIS_MASK_LSP_ATT_DELAY_BIT(header_lsp->typeblock) ? "delay " : "");
	    printf("%s", ISIS_MASK_LSP_ATT_EXPENSE_BIT(header_lsp->typeblock) ? "expense " : "");
	    printf("%s", ISIS_MASK_LSP_ATT_ERROR_BIT(header_lsp->typeblock) ? "error " : "");
	    printf("ATT bit set, ");
	}
	printf("%s", ISIS_MASK_LSP_PARTITION_BIT(header_lsp->typeblock) ? "P bit set, " : "");
	printf("%s", tok2str(isis_lsp_istype_values,"Unknown(0x%x)",ISIS_MASK_LSP_ISTYPE_BITS(header_lsp->typeblock)));

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

	printf(", L%s CSNP (%u)", ISIS_MASK_LEVEL_BITS(pdu_type) ? "2" : "1", pdu_len);
	TCHECK(*header_csnp);
	printf("\n\t\t  source-id:    ");
	isis_print_nodeid(header_csnp->source_id);		
	printf("\n\t\t  start lsp-id: ");
	isis_print_lspid(header_csnp->start_lsp_id);	
	printf("\n\t\t  end lsp-id:   ");
	isis_print_lspid(header_csnp->end_lsp_id);

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

	printf(", L%s PSNP (%u)", ISIS_MASK_LEVEL_BITS(pdu_type) ? "2" : "1", pdu_len);
	TCHECK(*header_psnp);
	printf("\n\t\t  source-id:    ");
	isis_print_nodeid(header_psnp->source_id); 
 
	packet_len -= (ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE);
	pptr = p + (ISIS_COMMON_HEADER_SIZE+ISIS_PSNP_HEADER_SIZE);
	break;

    default:
	printf(", PDU type (0x%02x) not supported", pdu_type);
	return (1);
    }

    /*
     * Now print the TLV's.
     */
    
    while (packet_len >= 2) {
        if (pptr == snapend) {
	    return (1);
        }

	if (!TTEST2(*pptr, 2)) {
	    printf("\n\t\t\t packet exceeded snapshot (%ld) bytes",
		  (long)(pptr-snapend));
	    return (1);
	}
	type = *pptr++;
	len = *pptr++;
	packet_len -= 2;
	if (len > packet_len) {
	    break;
	}
	printf("\n\t\t    ");

	switch (type) {
	case TLV_AREA_ADDR:
	    printf("Area address(es) (%u)",len);
	    tmp = len;
	    tptr = pptr;
	    if (!TTEST2(*tptr, 1))
		goto trunctlv;
	    alen = *tptr++;
	    while (tmp && alen < tmp) {
		printf("\n\t\t\tArea address (%u): ",alen);
		if (!print_nsap(tptr, alen))
		    return (1);
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
	    printf("IS Neighbor(s) (%u)",len);
	    tmp = len;
	    tptr = pptr;
	    while (tmp >= ETHER_ADDR_LEN) {
		printf("\n\t\t\tIS Neighbor: ");
		if (!isis_print_sysid(tptr))
		    return (1);
		tmp -= ETHER_ADDR_LEN;
		tptr += ETHER_ADDR_LEN;
	    }
	    break;

	case TLV_PADDING:
	    printf("Padding (%u)", len);
	    break;

        case TLV_MT_IS_REACH:
            printf("Multi Topology IS Reachability (%u)",len);
            tptr=pptr;
            tmp=len;
            while (tmp>0) {
                printf("\n\t\t\t");
                if (!TTEST2(*tptr, 2))
		    goto trunctlv;
		switch(EXTRACT_16BITS(tptr)&0x0fff) {

		case 0:
			printf("IPv4 unicast");
			break;

	        case 1:
			printf("In-Band Management");
			break;

	        case 2:
			printf("IPv6 unicast");
			break;

	        case 3:
			printf("Multicast");
			break;

	        case 4095:
			printf("Development, Experimental or Proprietary");
		        break;

	        default:
		        printf("Reserved for IETF Consensus");
		        break;
	        }
		printf(" Topology (0x%03x)",EXTRACT_16BITS(tptr)&0x0fff);
                tptr+=2;
            	printf("\n\t\t\t  IS Neighbor: ");
                if (!isis_print_nodeid(tptr))
                    return (1);
                tptr+=(SYSTEM_ID_LEN+1);
                if (!TTEST2(*tptr, 3))
                    goto trunctlv;
                printf(", Metric: %d",EXTRACT_24BITS(tptr));
                tptr+=3;
                if (!TTEST2(*tptr, 1))
                    goto trunctlv;
                tslen=*(tptr++);
                printf(", %ssub-TLVs present",tslen ? "" : "no ");
   	
                tptr+=tslen;
                tmp-=(13+tslen);
            }
            break;

        case TLV_EXT_IS_REACH:
            printf("Extended IS Reachability (%u)",len);
            tptr=pptr;
            tmp=len;
            while (tmp>0) {
            	printf("\n\t\t\tIS Neighbor: ");
                if (!isis_print_nodeid(tptr))
                    return (1);
                tptr+=(SYSTEM_ID_LEN+1);
                if (!TTEST2(*tptr, 3))
                    goto trunctlv;
                printf(", Metric: %d",EXTRACT_24BITS(tptr));
                tptr+=3;
                if (!TTEST2(*tptr, 1))
                    goto trunctlv;
                tslen=*(tptr++);
                printf(", %ssub-TLVs present",tslen ? "" : "no ");
                if (tslen) {
                    printf(" (%u)",tslen);
                    ttslen=tslen;
                    while (ttslen>0) {
                        if (!TTEST2(*tptr,2))
                            goto trunctlv;
                        printf("\n\t\t\t  ");
                    	subt=*(tptr++);
                    	subl=*(tptr++);
                        switch(subt) {
                        case SUBTLV_EXT_IS_REACH_ADMIN_GROUP:
                            printf("Administrative groups: 0x%08x", EXTRACT_32BITS(tptr));
                            break;
                        case SUBTLV_EXT_IS_REACH_MAX_LINK_BW :
                            if (!TTEST2(*tptr,4))
                                goto trunctlv;
                            j = EXTRACT_32BITS(tptr);
                            memcpy (&bw, &j, 4);            
                            printf("Maximum link bandwidth : %.3f Mbps",
                                   bw*8/1000000 );
                            break;                            
                        case SUBTLV_EXT_IS_REACH_RESERVABLE_BW :
                            if (!TTEST2(*tptr,4))
                                goto trunctlv;
                            j = EXTRACT_32BITS(tptr);
                            memcpy (&bw, &j, 4); 
                            printf("Reservable link bandwidth: %.3f Mbps",
                                   bw*8/1000000  );
                            break;
                        case SUBTLV_EXT_IS_REACH_UNRESERVED_BW :
                            printf("Unreserved bandwidth:");
                            for (i = 0; i < 8; i++) {
                                if (!TTEST2(*tptr,4))
                                    goto trunctlv;
                                j = EXTRACT_32BITS(tptr);
                                memcpy (&bw, &j, 4); 	
                                printf("\n\t\t\t    priority level %d: %.3f Mbps",
                                       i, bw*8/1000000 );
                                tptr+=4;
                            }
                            tptr-=32;
                            break;      
                        case SUBTLV_EXT_IS_REACH_TE_METRIC:
                            if (!TTEST2(*tptr,3))
                                goto trunctlv;
                            printf("Traffic Engineering Metric: %d", EXTRACT_24BITS(tptr));
                            break;                            
                        case SUBTLV_EXT_IS_REACH_IPV4_INTF_ADDR:
                            if (!TTEST2(*tptr,4))
                                goto trunctlv;
                            printf("IPv4 interface address: %s", ipaddr_string(tptr));
                            break;	
                        case SUBTLV_EXT_IS_REACH_IPV4_NEIGHBOR_ADDR:
                            if (!TTEST2(*tptr,4))
                                goto trunctlv;
                            printf("IPv4 neighbor address: %s", ipaddr_string(tptr));
                            break;
                        case 250:
                        case 251:
                        case 252:
                        case 253:
                        case 254:
                             printf("Reserved for cisco specific extensions, type %d, length %d", subt, subl);
                             break;
                        case 255:
                             printf("Reserved for future expansion, type %d, length %d", subt, subl);
                             break;                                 
                        default:
                            printf("unknown subTLV, type %d, length %d", subt, subl);
                        }	
		    tptr+=subl;
	            ttslen-=(subl+2);
                    }
                }	
                tptr+=tslen;
                tmp-=(11+tslen);
            }
            break;
        case TLV_IS_REACH:
            printf("IS Reachability (%u)",len);

	    tptr=pptr;

	    if (!TTEST2(*tptr,1))  /* check if there is one byte left to read out the virtual flag */
		 goto trunctlv;

	    switch (*tptr) {
	    case 0:
	      printf("\n\t\t\tIsNotVirtual");
	      break;
	    case 1:
	      printf("\n\t\t\tIsVirtual");
	      break;
	    default:
	      printf("\n\t\t\tbogus virtual flag 0x%02x",(*tptr));
	      break;     
	    }

	    tptr++;

	    tlv_is_reach = (const struct isis_tlv_is_reach *)tptr;

	    tmp = len;
            while (tmp >= sizeof(struct isis_tlv_is_reach)) {
		if (!TTEST(*tlv_is_reach))
		    goto trunctlv;

		printf("\n\t\t\tIS Neighbor: ");
		isis_print_nodeid(tlv_is_reach->neighbor_nodeid);

		printf(", Default Metric: %d, %s",
			   ISIS_LSP_TLV_METRIC_VALUE(tlv_is_reach->metric_default),
			   ISIS_LSP_TLV_METRIC_IE(tlv_is_reach->metric_default) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_is_reach->metric_delay))
		    printf("\n\t\t\t  Delay Metric: %d, %s",
				   ISIS_LSP_TLV_METRIC_VALUE(tlv_is_reach->metric_delay),
				   ISIS_LSP_TLV_METRIC_IE(tlv_is_reach->metric_delay) ? "External" : "Internal");
               
		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_is_reach->metric_expense))
		    printf("\n\t\t\t  Expense Metric: %d, %s",
				   ISIS_LSP_TLV_METRIC_VALUE(tlv_is_reach->metric_expense),
				   ISIS_LSP_TLV_METRIC_IE(tlv_is_reach->metric_expense) ? "External" : "Internal");

		if (!ISIS_LSP_TLV_METRIC_SUPPORTED(tlv_is_reach->metric_error))
		    printf("\n\t\t\t  Error Metric: %d, %s",
				   ISIS_LSP_TLV_METRIC_VALUE(tlv_is_reach->metric_error),
				   ISIS_LSP_TLV_METRIC_IE(tlv_is_reach->metric_error) ? "External" : "Internal");

		tmp -= sizeof(struct isis_tlv_is_reach);
		tlv_is_reach++;
	    }
            break;

	case TLV_IP_REACH:
	    printf("IP Internal reachability (%u)",len); 
	    if (!isis_print_tlv_ip_reach(pptr, len))
		return (1);
	    break;

	case TLV_IP_REACH_EXT:
	    printf("IP External reachability (%u)",len);
	    if (!isis_print_tlv_ip_reach(pptr, len))
		return (1);
	    break;

	case TLV_EXT_IP_REACH:
	    printf("Extended IP reachability (%u)",len);
	    i=len;
	    tptr=pptr;
		
	    while (i>0) {
		memset (prefix, 0, 4);
		if (!TTEST2(*tptr, 4))
		    return (1);
	        metric = EXTRACT_32BITS(tptr);
		tptr+=4;

		if (!TTEST2(*tptr, 1))
		    return (1);
		j=*(tptr);
		bit_length = (*(tptr)++&0x3f);
		byte_length = (bit_length + 7) / 8;
		if (!TTEST2(*tptr, byte_length))
		    return (1);

		memcpy(prefix,tptr,byte_length);

		printf("\n\t\t\tIPv4 prefix: %u.%u.%u.%u/%d", 
		       prefix[0],
		       prefix[1],
		       prefix[2],
		       prefix[3],
		       bit_length);

		printf("\n\t\t\t  Metric: %u, Distribution: %s",
		       metric,
		       ISIS_MASK_TLV_EXT_IP_UPDOWN(j) ? "down" : "up");

		printf(", %ssub-TLVs present",
		       ISIS_MASK_TLV_EXT_IP_SUBTLV(j) ? "" : "no ");

		if (ISIS_MASK_TLV_EXT_IP_SUBTLV(j)) {
		    if (!TTEST2(*tptr, 1))
		      return (1);		  
		    printf(" (%u)",*tptr);  /* no subTLV decoder supported - just print out subTLV length */
		    i-=*tptr;
		    tptr+=*tptr++;
		}

		i-=(5+byte_length);
		tptr+=byte_length;
	    }
	    break;

#ifdef INET6

	case TLV_IP6_REACH:
	    printf("IP6 reachability (%u)",len);
	    i=len;
	    tptr=pptr;
		
	    while (i>0) {
		if (!TTEST2(*tptr, 4))
		    return (1);
	        metric = EXTRACT_32BITS(tptr);
		tptr+=4;

		if (!TTEST2(*tptr, 2))
		    return (1);
		j=*(tptr++);
		bit_length = (*(tptr)++);
		byte_length = (bit_length + 7) / 8;
		if (!TTEST2(*tptr, byte_length))
		    return (1);

		memset(prefix6, 0, 16);
		memcpy(prefix6,tptr,byte_length);

		printf("\n\t\t\tIPv6 prefix: %s/%u",
		       ip6addr_string(prefix6),
		       bit_length);

		printf("\n\t\t\t  Metric: %u, %s, Distribution: %s, %ssub-TLVs present",
		    metric,
		    ISIS_MASK_TLV_IP6_IE(j) ? "External" : "Internal",
		    ISIS_MASK_TLV_IP6_UPDOWN(j) ? "down" : "up",
		    ISIS_MASK_TLV_IP6_SUBTLV(j) ? "" : "no ");

		if (ISIS_MASK_TLV_IP6_SUBTLV(j)) {
		    if (!TTEST2(*tptr, 1))
		      return (1);		  
		    printf(" (%u)",*tptr); /* no subTLV decoder supported - just print out subTLV length */
		    i-=*tptr;
		    tptr+=*tptr++;
		}

		i-=(6+byte_length);
                tptr+=byte_length;
	    }

	    break;
#endif

#ifdef INET6
	case TLV_IP6ADDR:
	    printf("IPv6 Interface address(es) (%u)",len); 
	    i=len;
	    tptr=pptr;
	    while (i>0) {
		if (!TTEST2(*tptr, 16))
		    goto trunctlv;

                printf("\n\t\t\tIPv6 interface address: %s",
		       ip6addr_string(tptr));

		tptr += 16;
		i -= 16;
	    }
	    break;
#endif
	case TLV_AUTH:
	    if (!TTEST2(*pptr, 1))
		goto trunctlv;
	    printf("Authentication (%u)",len);
	    if (*pptr==SUBTLV_AUTH_SIMPLE) {
		printf("\n\t\t\tsimple text password: ");
		for(i=1;i<len;i++) {
		    if (!TTEST2(*(pptr+i), 1))
			goto trunctlv;
		    printf("%c",*(pptr+i));
		}
	    }
	    if (!TTEST2(*pptr, 1))
		goto trunctlv;
	    if (*pptr==SUBTLV_AUTH_MD5) {
		printf("\n\t\t\tMD5 password: ");
		for(i=1;i<len;i++) {
		    if (!TTEST2(*(pptr+i), 1))
			goto trunctlv;
		    printf("%02x",*(pptr+i));
		}
	    }
	    break;

	case TLV_PTP_ADJ:
	    printf("Point-to-point Adjacency State (%u)",len);
	    tlv_ptp_adj = (const struct isis_tlv_ptp_adj *)pptr;
	    i=len;
	    if(i>=1) {
		if (!TTEST2(*pptr, 1))
		    goto trunctlv;
		printf("\n\t\t\tAdjacency State: %s",
		       tok2str(isis_ptp_adjancey_values, "#0x%x", *pptr));
		i--;
	    }
	    if(i>=4) {
		if (!TTEST2(tlv_ptp_adj->ext_local_circuit_id, 4))
		    goto trunctlv;
		printf("\n\t\t\tExtended Local circuit ID: 0x%08x",
		       EXTRACT_32BITS(tlv_ptp_adj->ext_local_circuit_id));
		i-=4;
	    }
	    if(i>=6) {
		if (!TTEST2(tlv_ptp_adj->neighbor_sysid, 6))
		    goto trunctlv;
		printf("\n\t\t\tNeighbor SystemID: ");
		       isis_print_sysid(tlv_ptp_adj->neighbor_sysid);
		i-=6;
	    }
	    if(i>=4) {
		if (!TTEST2(tlv_ptp_adj->neighbor_ext_local_circuit_id, 4))
		    goto trunctlv;
		printf("\n\t\t\tNeighbor Extended Local circuit ID: 0x%08x",
		       EXTRACT_32BITS(tlv_ptp_adj->neighbor_ext_local_circuit_id));
	    }
	    break;

	case TLV_PROTOCOLS:
	    printf("Protocols supported (%u)", len);
	    printf("\n\t\t\tNLPID(s): ");
	    for (i = 0; i < len; i++) {
		if (!TTEST2(*(pptr+i), 1))
		    goto trunctlv;
		printf("%s (0x%02x)",tok2str(isis_nlpid_values, "Unknown", *(pptr+i)),*(pptr+i));
		if (i<len-1)
		    printf(", ");
	    }
	    break;

	case TLV_TE_ROUTER_ID:
	    printf("Traffic Engineering Router ID (%u)",len); 
	    if (!TTEST2(*pptr, 4))
		goto trunctlv;
	    printf("\n\t\t\tTraffic Engineering Router ID: %s", ipaddr_string(pptr));
	    break;

	case TLV_IPADDR:
	    printf("IPv4 Interface address(es) (%u)",len); 
	    i=len;
	    tptr=pptr;
	    while (i>0) {
		if (!TTEST2(*tptr, 4))
		    goto trunctlv;
		printf("\n\t\t\tIPv4 interface address: %s", ipaddr_string(tptr));
		tptr += 4;
		i -= 4;
	    }
	    break;

	case TLV_HOSTNAME:
	    printf("Hostname (%u)", len);
	    printf("\n\t\t\tHostname: ");
	    for(i = 0; i < len; i++) {
		if (!TTEST2(*(pptr+i), 1))
		    goto trunctlv;
		printf("%c",*(pptr+i));
	    }
	    break;

	case TLV_LSP:    
	    tlv_lsp = (const struct isis_tlv_lsp *)pptr;
	    printf("LSP entries (%u)", len); 
	    i=0;
	    while(i<len) {
		printf("\n\t\t\tlsp-id: ");
		if (!isis_print_nodeid(tlv_lsp->lsp_id))
		    return (1);
		if (!TTEST((tlv_lsp->lsp_id)[SYSTEM_ID_LEN+1]))
		    goto trunctlv;
		printf("-%02x",(tlv_lsp->lsp_id)[SYSTEM_ID_LEN+1]);
		if (!TTEST2(tlv_lsp->sequence_number, 4))
		    goto trunctlv;
		printf("\n\t\t\t  sequence number: 0x%08x",EXTRACT_32BITS(tlv_lsp->sequence_number));
		if (!TTEST2(tlv_lsp->remaining_lifetime, 2))
		    goto trunctlv;
		printf("\n\t\t\t  Remaining lifetime: %5ds",EXTRACT_16BITS(tlv_lsp->remaining_lifetime));
		if (!TTEST2(tlv_lsp->checksum, 2))
		    goto trunctlv;
		printf("\n\t\t\t  checksum: 0x%04x",EXTRACT_16BITS(tlv_lsp->checksum));
		i+=sizeof(struct isis_tlv_lsp);
		tlv_lsp++;
	    }
	    break;

	case TLV_CHECKSUM:
	    if (!TTEST2(*pptr, 2))
		goto trunctlv;
	    printf("Checksum (%u)", len);
	    printf("\n\t\t\tchecksum: 0x%04x", 
		   EXTRACT_16BITS(pptr));

	    if (osi_cksum(optr, length, off))
		printf(" (incorrect)");
	    else
		printf(" (correct)");
	    break;

	case TLV_MT_SUPPORTED:
	    printf("Multi Topology (%u)",len); 
	    i=len;
	    tptr=pptr;
	    while (i>1) {
		/* length can only be a multiple of 2, otherwise there is 
		   something broken -> so decode down until length is 1 */
		if (i!=1) {
		    if (!TTEST2(*tptr, 2))
			goto trunctlv;
		    printf("\n\t\t\t");
		    switch(EXTRACT_16BITS(tptr)&0x0fff) {

		    case 0:
			printf("IPv4 unicast");
			break;

		    case 1:
			printf("In-Band Management");
			break;

		    case 2:
			printf("IPv6 unicast");
			break;

		    case 3:
			printf("Multicast");
			break;

		    case 4095:
			printf("Development, Experimental or Proprietary");
		        break;

		    default:
			printf("Reserved for IETF Consensus");
			break;
		    }
		    printf(" Topology (0x%03x)%s%s",
			   EXTRACT_16BITS(tptr)&0xfff,
			   (EXTRACT_16BITS(tptr)&0x8000) ? "" : ", no sub-TLVs present",
			   (EXTRACT_16BITS(tptr)&0x4000) ? ", ATT bit set" : "" );
		} else {
		    printf("\n\t\t\tmalformed MT-ID");
		    break;
		}
		i-=2;
		tptr+=2;
	    }
	    break;

	case TLV_RESTART_SIGNALING:
	    tptr=pptr;
	    printf("Restart Signaling (%u)",len);
		    if (!TTEST2(*tptr, 3))
			goto trunctlv;
		    
	    printf("\n\t\t\tRestart Request bit %s, Restart Acknowledgement bit %s\n\t\t\tRemaining holding time: %us",
                   ISIS_MASK_RESTART_RR(*tptr) ? "set" : "clear",
		   ISIS_MASK_RESTART_RA(*tptr++) ? "set" : "clear",
		   EXTRACT_16BITS(tptr));

	    break;

	default:
	    printf("unknown TLV, type %d, length %d\n\t\t\t", type, len);
	    tptr=pptr;

	    for(i=0;i<len;i++) {
		if (!TTEST2(*(tptr+i), 1))
		    goto trunctlv;
		printf("%02x",*(tptr+i)); /* formatted hex output of unknown TLV data */
		if (i%2)
		    printf(" ");
		if (i/16!=(i+1)/16) {
		  if (i<(len-1))
		    printf("\n\t\t\t");
		}
	    }
	    break;
	}

	pptr += len;
	packet_len -= len;
    }

    if (packet_len != 0) {
	printf("\n\t\t\t %d straggler bytes", packet_len);
    }
    return (1);

trunc:
    fputs("[|isis]", stdout);
    return (1);

trunctlv:
    printf("\n\t\t\t packet exceeded snapshot");
    return(1);
}

/*
 * Verify the checksum.  See 8473-1, Appendix C, section C.4.
 */

static int
osi_cksum(register const u_char *p, register u_int len, u_char *off)
{
	int32_t c0 = 0, c1 = 0;

	if ((off[0] == 0) && (off[1] == 0))
		return 0;

	off[0] = off[1] = 0;
	while ((int)--len >= 0) {
		c0 += *p++;
		c0 %= 255;
		c1 += c0;
		c1 %= 255;
	}
	return (c0 | c1);
}

