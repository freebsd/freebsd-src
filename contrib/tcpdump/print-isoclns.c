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
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-isoclns.c,v 1.22 2000/10/11 04:04:33 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "ether.h"
#include "extract.h"

#define	NLPID_CLNS	129	/* 0x81 */
#define	NLPID_ESIS	130	/* 0x82 */
#define	NLPID_ISIS	131	/* 0x83 */
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
#define L1_LS_PDU       18
#define L2_LS_PDU       19
#define L1_COMPLETE_SEQ_PDU  24
#define L2_COMPLETE_SEQ_PDU  25

/*
 * A TLV is a tuple of a type, length and a value and is normally used for
 * encoding information in all sorts of places.  This is an enumeration of
 * the well known types.
 */

#define TLV_AREA_ADDR   1
#define TLV_IS_REACH	2
#define TLV_ES_REACH	3
#define TLV_SUMMARY	5
#define TLV_ISNEIGH     6
#define TLV_PADDING     8
#define TLV_LSP		9
#define TLV_AUTHENT     10
#define TLV_IP_REACH	128
#define TLV_PROTOCOLS	129
#define TLV_IP_EXTERN	130
#define TLV_IDRP_INFO	131
#define TLV_IPADDR	132
#define TLV_IPAUTH	133
#define TLV_PTP_ADJ	240

/*
 * Katz's point to point adjacency TLV uses codes to tell us the state of
 * the remote adjacency.  Enumerate them.
 */

#define ISIS_PTP_ADJ_UP   0
#define ISIS_PTP_ADJ_INIT 1
#define ISIS_PTP_ADJ_DOWN 2

static int osi_cksum(const u_char *, int, u_char *);
static void esis_print(const u_char *, u_int);
static int isis_print(const u_char *, u_int);


struct isis_ptp_adjancey_values {
	u_char id;
	char   *name;
};

static struct isis_ptp_adjancey_values isis_ptp_adjancey_values[] = {
	{ ISIS_PTP_ADJ_UP,    "UP" },
	{ ISIS_PTP_ADJ_INIT,  "INIT" },
	{ ISIS_PTP_ADJ_DOWN,  "DOWN" }
};

struct isis_common_header {
    u_char nlpid;
    u_char fixed_len;
    u_char version;			/* Protocol version? */
    u_char id_length;
    u_char enc_pdu_type;		/* 3 MSbs are reserved */
    u_char pkt_version;			/* Packet format version? */
    u_char reserved;
    u_char enc_max_area;
};

struct isis_header {
    u_char nlpid;
    u_char fixed_len;
    u_char version;			/* Protocol version? */
    u_char id_length;
    u_char enc_pdu_type;		/* 3 MSbs are reserved */
    u_char pkt_version;			/* Packet format version? */
    u_char reserved;
    u_char enc_max_area;
    u_char circuit;
    u_char enc_source_id[SYSTEM_ID_LEN];
    u_char enc_holding_time[2];
    u_char enc_packet_len[2];
    u_char enc_priority;
    u_char enc_lan_id[SYSTEM_ID_LEN+1];
};
struct isis_lan_header {
    u_char circuit;
    u_char enc_source_id[SYSTEM_ID_LEN];
    u_char enc_holding_time[2];
    u_char enc_packet_len[2];
    u_char enc_priority;
    u_char enc_lan_id[SYSTEM_ID_LEN+1];
};

struct isis_ptp_header {
    u_char circuit;
    u_char enc_source_id[SYSTEM_ID_LEN];
    u_char enc_holding_time[2];
    u_char enc_packet_len[2];
    u_char loc_circuit_id;
};

#define ISIS_COMMON_HEADER_SIZE (sizeof(struct isis_common_header))
#define ISIS_HEADER_SIZE (15+(SYSTEM_ID_LEN<<1))
#define ISIS_PTP_HEADER_SIZE (14+SYSTEM_ID_LEN)
#define L1_LS_PDU_HEADER_SIZE (21+SYSTEM_ID_LEN)
#define L2_LS_PDU_HEADER_SIZE L1_LS_PDU_HEADER_SIZE
#define L1_COMPLETE_SEQ_PDU_HEADER_SIZE 33
#define L2_COMPLETE_SEQ_PDU_HEADER_SIZE L1_COMPLETE_SEQ_PDU_HEADER_SIZE



void
isoclns_print(const u_char *p, u_int length, u_int caplen,
	      const u_char *esrc, const u_char *edst)
{
	u_char pdu_type;
	struct isis_header *header;
	
	header = (struct isis_header *)p;
	pdu_type = header->enc_pdu_type & PDU_TYPE_MASK;

	if (caplen < 1) {
		printf("[|iso-clns] ");
		if (!eflag)
			printf("%s > %s",
			       etheraddr_string(esrc),
			       etheraddr_string(edst));
		return;
	}

	switch (*p) {

	case NLPID_CLNS:
		printf("iso clns");
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		break;

	case NLPID_ESIS:
		printf("iso esis");
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		esis_print(p, length);
		return;

	case NLPID_ISIS:
		printf("iso isis");
		if (!eflag) {
			if(pdu_type != PTP_IIH)
				(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		}
		(void)printf(" len=%d ", length);
		if (!isis_print(p, length))
		    default_print_unaligned(p, caplen);
		break;

	case NLPID_NULLNS:
		printf("iso nullns");
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		break;

	default:
		printf("iso clns %02x", p[0]);
		if (!eflag)
			(void)printf(" %s > %s",
				     etheraddr_string(esrc),
				     etheraddr_string(edst));
		(void)printf(" len=%d ", length);
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
	int li = p[1];
	const struct esis_hdr *eh = (const struct esis_hdr *) &p[2];
	u_char off[2];

	if (length == 2) {
		if (qflag)
			printf(" bad pkt!");
		else
			printf(" no header at all!");
		return;
	}
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
			int op, opli;
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
			while (--opli >= 0)
				printf("%02x", *q++);
			printf (">");
		}
}

/*
 * print_nsap
 * Print out an NSAP. 
 */

static void
print_nsap (register const u_char *cp, register int length)
{
    int i;
    
    for (i = 0; i < length; i++) {
	printf("%02x", *cp++);
	if (((i & 1) == 0) && (i + 1 < length)) {
	    printf(".");
	}

    }
}

/*
 * isis_print
 * Decode IS-IS packets.  Return 0 on error.
 *
 * So far, this is only smart enough to print IIH's.  Someday...
 */

static int
isis_print (const u_char *p, u_int length)
{
    struct isis_header *header;
    struct isis_ptp_header *header_ptp;
    u_char pdu_type, max_area, priority, type, len, tmp, alen;
    const u_char *pptr, *tptr;
    u_short packet_len, holding_time;
    int i;

    header = (struct isis_header *)p;
    header_ptp = (struct isis_ptp_header *)header;
    printf("\n\t\t\t");

    /*
     * Sanity checking of the header.
     */
    if (header->nlpid != NLPID_ISIS) {
	printf(" coding error!");
	return(0);
    }

    if (header->version != ISIS_VERSION) {
	printf(" version %d packet not supported", header->version);
	return(0);
    }

    if ((header->id_length != SYSTEM_ID_LEN) && (header->id_length != 0)) {
	printf(" system ID length of %d is not supported",
	       header->id_length);
	return(0);
    }

    if ((header->fixed_len != ISIS_HEADER_SIZE) &&
	(header->fixed_len != ISIS_PTP_HEADER_SIZE) &&
	(header->fixed_len != L1_LS_PDU_HEADER_SIZE) &&
	(header-> fixed_len != L1_COMPLETE_SEQ_PDU_HEADER_SIZE) ) {
	    printf(" bogus fixed header length %u",
		   header->fixed_len);
	    return(0);
    }

    pdu_type = header->enc_pdu_type & PDU_TYPE_MASK;
    if ((pdu_type != L1_LAN_IIH) && (pdu_type != L2_LAN_IIH) &&
	(pdu_type != PTP_IIH) && 
	(pdu_type != L1_COMPLETE_SEQ_PDU) &&
	(pdu_type != L2_COMPLETE_SEQ_PDU) ) {
	printf(" PDU type (%d) not supported", pdu_type);
	return(0);
    }
    
    if (header->pkt_version != ISIS_VERSION) {
	printf(" version %d packet not supported", header->pkt_version);
	return(0);
    }

    max_area = header->enc_max_area;
    switch(max_area) {
    case 0:
	max_area = 3;			/* silly shit */
	break;
    case 255:
	printf(" bad packet -- 255 areas");
	return(0);
    default:
	break;
    }

    switch (header->circuit) {
    case 0:
	printf(" PDU with circuit type 0");
	return(0);
    case 1:
	if (pdu_type == L2_LAN_IIH) {
	    printf(" L2 IIH on an L1 only circuit");
	    return(0);
	}
	break;
    case 2:
	if (pdu_type == L1_LAN_IIH) {
	    printf(" L1 IIH on an L2 only circuit");
	    return(0);
	}
	break;
    case 3:
	break;
    default:
	printf(" unknown circuit type");
	return(0);
    }
	
    holding_time = EXTRACT_16BITS(header->enc_holding_time);

    packet_len = EXTRACT_16BITS(header->enc_packet_len);
    if ((packet_len < ISIS_HEADER_SIZE) ||
	(packet_len > length)) {
	printf(" bogus packet length %d, real length %d", packet_len,
	       length);
	return(0);
    }

    if(pdu_type != PTP_IIH)
	    priority = header->enc_priority & PRIORITY_MASK;

    /*
     * Now print the fixed header.
     */
    switch (pdu_type) {
    case L1_LAN_IIH:
	printf(" L1 lan iih, ");
	break;
    case L2_LAN_IIH:
	printf(" L2 lan iih, ");
	break;
    case PTP_IIH:
	printf(" PTP iih, ");
	break;
    }

    printf("circuit ");
    switch (header->circuit) {
    case 1:
	printf("l1 only, ");
	break;
    case 2:
	printf("l2 only, ");
	break;
    case 3:
	printf("l1-l2, ");
	break;
    }

    printf ("holding time %d ", holding_time);
    printf ("\n\t\t\t source %s, length %d",
	    etheraddr_string(header->enc_source_id), packet_len);
    if((pdu_type==L1_LAN_IIH)||(pdu_type==L2_LAN_IIH))
	    printf ("\n\t\t\t lan id %s(%d)", etheraddr_string(header->enc_lan_id),
		    header->enc_lan_id[SYSTEM_ID_LEN]);

    /*
     * Now print the TLV's.
     */
    if(pdu_type==PTP_IIH) {
	    packet_len -= ISIS_PTP_HEADER_SIZE;
	    pptr = p + ISIS_PTP_HEADER_SIZE;
    } else {
	    packet_len -= ISIS_HEADER_SIZE;
	    pptr = p + ISIS_HEADER_SIZE;
    }
    while (packet_len >= 2) {
	if (pptr >= snapend) {
	    printf("\n\t\t\t packet exceeded snapshot");
	    return(1);
	}
	type = *pptr++;
	len = *pptr++;
	packet_len -= 2;
	if (len > packet_len) {
	    break;
	}

	switch (type) {
	case TLV_AREA_ADDR:
	    printf("\n\t\t\t area addresses");
	    tmp = len;
	    tptr = pptr;
	    alen = *tptr++;
	    while (tmp && alen < tmp) {
		printf("\n\t\t\t ");
		print_nsap(tptr, alen);
		printf(" (%d)", alen);
		tptr += alen;
		tmp -= alen + 1;
		alen = *tptr++;
	    }
	    break;
	case TLV_ISNEIGH:
	    printf("\n\t\t\t neighbor addresses");
	    tmp = len;
	    tptr = pptr;
	    while (tmp >= ETHER_ADDR_LEN) {
		printf("\n\t\t\t %s", etheraddr_string(tptr));
		tmp -= ETHER_ADDR_LEN;
		tptr += ETHER_ADDR_LEN;
	    }
	    break;
	case TLV_PADDING:
	    printf("\n\t\t\t padding for %d bytes", len);
	    break;
	case TLV_AUTHENT:
	    printf("\n\t\t\t authentication data");
	    default_print(pptr, len);
	    break;
	case TLV_PTP_ADJ:
	    printf("\n\t\t\t PTP adjacency status %s",
		   isis_ptp_adjancey_values[*pptr].name);
	    break;
	case TLV_PROTOCOLS:
	    printf("\n\t\t\t Supports protocols %s", (len>1)? "are":"is");
	    for(i=0;i<len;i++)
		printf(" %02X", (u_char)*(pptr+i));
	    break;
	case TLV_IPADDR:
	    printf("\n\t\t\t IP address: %s", ipaddr_string(pptr));
	    break;
	default:
	    printf("\n\t\t\t unknown TLV, type %d, length %d", type, len);
	    break;
	}

	pptr += len;
	packet_len -= len;
    }

    if (packet_len != 0) {
	printf("\n\t\t\t %d straggler bytes", packet_len);
    }
    return(1);
}

/*
 * Verify the checksum.  See 8473-1, Appendix C, section C.4.
 */

static int
osi_cksum(register const u_char *p, register int len, u_char *off)
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
