/*
 * Copyright (c) 1990, 1991, 1993, 1994, 1995, 1996, 1997
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
 * Extensively modified by Motonori Shindo (mshindo@mshindo.net) for more
 * complete PPP support.
 *
 * $FreeBSD$
 */

/*
 * TODO:
 * o resolve XXX as much as possible
 * o MP support
 * o BAP support
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-ppp.c,v 1.64 2001/09/09 02:04:19 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>

#ifdef __bsdi__
#include <net/slcompress.h>
#include <net/if_ppp.h>
#endif

#include <netinet/in.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <stdio.h>

#include "interface.h"
#include "extract.h"
#include "addrtoname.h"
#include "ppp.h"
#include "chdlc.h"
#include "ethertype.h"

/*
 * The following constatns are defined by IANA. Please refer to
 *    http://www.isi.edu/in-notes/iana/assignments/ppp-numbers
 * for the up-to-date information.
 */

/* Control Protocols (LCP/IPCP/CCP etc.) Codes */

#define CPCODES_VEXT		0	/* Vendor-Specific (RFC2153) */
#define CPCODES_CONF_REQ	1	/* Configure-Request */
#define CPCODES_CONF_ACK	2	/* Configure-Ack */
#define CPCODES_CONF_NAK	3	/* Configure-Nak */
#define CPCODES_CONF_REJ	4	/* Configure-Reject */
#define CPCODES_TERM_REQ	5	/* Terminate-Request */
#define CPCODES_TERM_ACK	6	/* Terminate-Ack */
#define CPCODES_CODE_REJ	7	/* Code-Reject */
#define CPCODES_PROT_REJ	8	/* Protocol-Reject (LCP only) */
#define CPCODES_ECHO_REQ	9	/* Echo-Request (LCP only) */
#define CPCODES_ECHO_RPL	10	/* Echo-Reply (LCP only) */
#define CPCODES_DISC_REQ	11	/* Discard-Request (LCP only) */
#define CPCODES_ID		12	/* Identification (LCP only) */
#define CPCODES_TIME_REM	13	/* Time-Remaining (LCP only) */
#define CPCODES_RESET_REQ	14	/* Reset-Request (CCP only) */
#define CPCODES_RESET_REP	15	/* Reset-Reply (CCP only) */

#define CPCODES_MIN	CPCODES_VEXT
#define CPCODES_MAX	CPCODES_RESET_REP

static const char *cpcodes[] = {
	/*
	 * Control Protocol code values (RFC1661)
	 */
	"Vend-Ext",	/* (0) RFC2153 */
	"Conf-Req",	/* (1) */
	"Conf-Ack",	/* (2) */
	"Conf-Nak",	/* (3) */
	"Conf-Rej",	/* (4) */
	"Term-Req",	/* (5) */
	"Term-Ack",	/* (6) */
 	"Code-Rej",	/* (7) */
	"Prot-Rej",	/* (8) */
	"Echo-Req",	/* (9) */
	"Echo-Rep",	/* (10) */
	"Disc-Req",	/* (11) */
	"Ident",	/* (12) RFC1570 */
	"Time-Rem",	/* (13) RFC1570 */
	"Reset-Req",	/* (14) RFC1962 */
	"Reset-Ack",	/* (15) RFC1962 */
};

/* LCP Config Options */

#define LCPOPT_VEXT	0
#define LCPOPT_MRU	1
#define LCPOPT_ACCM	2
#define LCPOPT_AP	3
#define LCPOPT_QP	4
#define LCPOPT_MN	5
#define LCPOPT_DEP6	6
#define LCPOPT_PFC	7
#define LCPOPT_ACFC	8
#define LCPOPT_FCSALT	9
#define LCPOPT_SDP	10
#define LCPOPT_NUMMODE	11
#define LCPOPT_DEP12	12
#define LCPOPT_CBACK	13
#define LCPOPT_DEP14	14
#define LCPOPT_DEP15	15
#define LCPOPT_DEP16	16
#define LCPOPT_MLMRRU	17
#define LCPOPT_MLSSNHF	18
#define LCPOPT_MLED	19
#define LCPOPT_PROP	20
#define LCPOPT_DCEID	21
#define LCPOPT_MPP	22
#define LCPOPT_LD	23
#define LCPOPT_LCPAOPT	24
#define LCPOPT_COBS	25
#define LCPOPT_PE	26
#define LCPOPT_MLHF	27
#define LCPOPT_I18N	28
#define LCPOPT_SDLOS	29
#define LCPOPT_PPPMUX	30

#define LCPOPT_MIN LCPOPT_VEXT
#define LCPOPT_MAX LCPOPT_PPPMUX

static const char *lcpconfopts[] = {
	"Vend-Ext",		/* (0) */
	"MRU",			/* (1) */
	"ACCM",			/* (2) */
	"Auth-Prot",		/* (3) */
	"Qual-Prot",		/* (4) */
	"Magic-Num",		/* (5) */
	"deprecated(6)",	/* used to be a Quality Protocol */
	"PFC",			/* (7) */
	"ACFC",			/* (8) */
	"FCS-Alt",		/* (9) */
	"SDP",			/* (10) */
	"Num-Mode",		/* (11) */
	"deprecated(12)",	/* used to be a Multi-Link-Procedure*/
	"Call-Back",		/* (13) */
	"deprecated(14)",	/* used to be a Connect-Time */
	"deprecated(15)",	/* used to be a Compund-Frames */
	"deprecated(16)",	/* used to be a Nominal-Data-Encap */
	"MRRU",			/* (17) */
	"SSNHF",		/* (18) */
	"End-Disc",		/* (19) */
	"Proprietary",		/* (20) */
	"DCE-Id",		/* (21) */
	"MP+",			/* (22) */
	"Link-Disc",		/* (23) */
	"LCP-Auth-Opt",		/* (24) */
	"COBS",			/* (25) */
	"Prefix-elision",	/* (26) */
	"Multilink-header-Form",/* (27) */
	"I18N",			/* (28) */
	"SDL-over-SONET/SDH",	/* (29) */
	"PPP-Muxing",		/* (30) */
};

/* IPV6CP - to be supported */
/* ECP - to be supported */

/* CCP Config Options */

#define CCPOPT_OUI	0	/* RFC1962 */
#define CCPOPT_PRED1	1	/* RFC1962 */
#define CCPOPT_PRED2	2	/* RFC1962 */
#define CCPOPT_PJUMP	3	/* RFC1962 */
/* 4-15 unassigned */
#define CCPOPT_HPPPC	16	/* RFC1962 */
#define CCPOPT_STACLZS	17	/* RFC1974 */
#define CCPOPT_MPPC	18	/* RFC2118 */
#define CCPOPT_GFZA	19	/* RFC1962 */
#define CCPOPT_V42BIS	20	/* RFC1962 */
#define CCPOPT_BSDCOMP	21	/* RFC1977 */
/* 22 unassigned */
#define CCPOPT_LZSDCP	23	/* RFC1967 */
#define CCPOPT_MVRCA	24	/* RFC1975 */
#define CCPOPT_DEC	25	/* RFC1976 */
#define CCPOPT_DEFLATE	26	/* RFC1979 */
/* 27-254 unassigned */
#define CCPOPT_RESV	255	/* RFC1962 */

#define CCPOPT_MIN CCPOPT_OUI
#define CCPOPT_MAX CCPOPT_DEFLATE    /* XXX: should be CCPOPT_RESV but... */

static const char *ccpconfopts[] = {
	"OUI",			/* (0) */
	"Pred-1",		/* (1) */
	"Pred-2",		/* (2) */
	"Puddle",		/* (3) */
	"unassigned(4)",	/* (4) */
	"unassigned(5)",	/* (5) */
	"unassigned(6)",	/* (6) */
	"unassigned(7)",	/* (7) */
	"unassigned(8)",	/* (8) */
	"unassigned(9)",	/* (9) */
	"unassigned(10)",	/* (10) */
	"unassigned(11)",	/* (11) */
	"unassigned(12)",	/* (12) */
	"unassigned(13)",	/* (13) */
	"unassigned(14)",	/* (14) */
	"unassigned(15)",	/* (15) */
	"HP-PPC",		/* (16) */
	"Stac-LZS",		/* (17) */
	"MPPC",			/* (18) */
	"Gand-FZA",		/* (19) */
	"V.42bis",		/* (20) */
	"BSD-Comp",		/* (21) */
	"unassigned(22)",	/* (22) */
	"LZS-DCP",		/* (23) */
	"MVRCA",		/* (24) */
	"DEC",			/* (25) */
	"Deflate",		/* (26) */
};

/* BACP Config Options */

#define BACPOPT_FPEER	1	/* RFC2125 */

/* SDCP - to be supported */

/* IPCP Config Options */

#define IPCPOPT_2ADDR	1	/* RFC1172, RFC1332 (deprecated) */
#define IPCPOPT_IPCOMP	2	/* RFC1332 */
#define IPCPOPT_ADDR	3	/* RFC1332 */
#define IPCPOPT_MOBILE4	4	/* RFC2290 */

#define IPCPOPT_PRIDNS	129	/* RFC1877 */
#define IPCPOPT_PRINBNS	130	/* RFC1877 */
#define IPCPOPT_SECDNS	131	/* RFC1877 */
#define IPCPOPT_SECNBNS	132	/* RFC1877 */

/* ATCP - to be supported */
/* OSINLCP - to be supported */
/* BVCP - to be supported */
/* BCP - to be supported */
/* IPXCP - to be supported */

/* Auth Algorithms */

/* 0-4 Reserved (RFC1994) */
#define AUTHALG_CHAPMD5	5	/* RFC1994 */
#define AUTHALG_MSCHAP1	128	/* RFC2433 */
#define AUTHALG_MSCHAP2	129	/* RFC2795 */

/* FCS Alternatives - to be supported */

/* Multilink Endpoint Discriminator (RFC1717) */
#define MEDCLASS_NULL	0	/* Null Class */
#define MEDCLASS_LOCAL	1	/* Locally Assigned */
#define MEDCLASS_IPV4	2	/* Internet Protocol (IPv4) */
#define MEDCLASS_MAC	3	/* IEEE 802.1 global MAC address */
#define MEDCLASS_MNB	4	/* PPP Magic Number Block */
#define MEDCLASS_PSNDN	5	/* Public Switched Network Director Number */

/* PPP LCP Callback */
#define CALLBACK_AUTH	0	/* Location determined by user auth */
#define CALLBACK_DSTR	1	/* Dialing string */
#define CALLBACK_LID	2	/* Location identifier */
#define CALLBACK_E164	3	/* E.164 number */
#define CALLBACK_X500	4	/* X.500 distinguished name */
#define CALLBACK_CBCP	6	/* Location is determined during CBCP nego */

/* CHAP */

#define CHAP_CHAL	1
#define CHAP_RESP	2
#define CHAP_SUCC	3
#define CHAP_FAIL	4

#define CHAP_CODEMIN CHAP_CHAL
#define CHAP_CODEMAX CHAP_FAIL

static const char *chapcode[] = {
	"Chal",		/* (1) */
	"Resp",		/* (2) */
	"Succ",		/* (3) */
	"Fail",		/* (4) */
};

/* PAP */

#define PAP_AREQ	1
#define PAP_AACK	2
#define PAP_ANAK	3

#define PAP_CODEMIN	PAP_AREQ
#define PAP_CODEMAX	PAP_ANAK

static const char *papcode[] = {
	"Auth-Req",	/* (1) */
	"Auth-Ack",	/* (2) */
	"Auth-Nak",	/* (3) */
};

/* BAP */
#define BAP_CALLREQ	1
#define BAP_CALLRES	2
#define BAP_CBREQ	3
#define BAP_CBRES	4
#define BAP_LDQREQ	5
#define BAP_LDQRES	6
#define BAP_CSIND	7
#define BAP_CSRES	8

static const char *ppp_protoname (u_int proto);
static void handle_ctrl_proto (u_int proto,const u_char *p, int length);
static void handle_chap (const u_char *p, int length);
static void handle_pap (const u_char *p, int length);
static void handle_bap (const u_char *p, int length);
static int print_lcp_config_options (const u_char *p, int);
static int print_ipcp_config_options (const u_char *p, int);
static int print_ccp_config_options (const u_char *p, int);
static int print_bacp_config_options (const u_char *p, int);
static void handle_ppp (u_int proto, const u_char *p, int length);

static const char *
ppp_protoname(u_int proto)
{
	static char buf[20];

	switch (proto) {
	case PPP_IP:	return "IP";
	case PPP_IPV6:	return "IPv6";
#ifdef PPP_XNS
	case PPP_XNS:	return "XNS";
#endif
	case PPP_IPX:	return "IPX";
	case PPP_OSI:   return "OSI";
	case PPP_VJC:	return "VJC";
	case PPP_VJNC:	return "VJNC";
	case PPP_COMP:	return "COMP";
	case PPP_IPCP:	return "IPCP";
	case PPP_IPV6CP: return "IPv6CP";
	case PPP_IPXCP:	return "IPXCP";
	case PPP_OSICP: return "OSICP";
	case PPP_CCP:	return "CCP";
	case PPP_LCP:	return "LCP";
	case PPP_PAP:	return "PAP";
#ifdef PPP_LQR
	case PPP_LQR:	return "LQR";
#endif
	case PPP_CHAP:	return "CHAP";
	case PPP_BACP:	return "BACP";
	case PPP_BAP:	return "BAP";
	default:
		snprintf(buf, sizeof(buf), "unknown-0x%04x", proto);
		return buf;
	}
}

/* generic Control Protocol (e.g. LCP, IPCP, CCP, etc.) handler */
static void
handle_ctrl_proto(u_int proto, const u_char *p, int length)
{
	u_int code, len;
	int (*pfunc)(const u_char *, int);
	int x, j;

	if (length < 1) {
		printf("[|%s]", ppp_protoname(proto));
		return;
	} else if (length < 4) {
		printf("[|%s 0x%02x]", ppp_protoname(proto), *p);
		return;
	}

	code = *p;
	if ((code >= CPCODES_MIN) && (code <= CPCODES_MAX))
		printf("%s", cpcodes[code]);
	else {
		printf("0x%02x", code);
		return;
	}
	p++;

	printf("(%u)", *p);		/* ID */
	p++;

	len = EXTRACT_16BITS(p);
	p += 2;

	if (length <= 4)
		return;		/* there may be a NULL confreq etc. */

	switch (code) {
	case CPCODES_VEXT:
		if (length < 11)
			break;
		printf(", Magic-Num=%08x", EXTRACT_32BITS(p));
		p += 4;
		printf(" OUI=%02x%02x%02x", p[0], p[1], p[2]);
		/* XXX: need to decode Kind and Value(s)? */
		break;
	case CPCODES_CONF_REQ:
	case CPCODES_CONF_ACK:
	case CPCODES_CONF_NAK:
	case CPCODES_CONF_REJ:
		x = len - 4;	/* Code(1), Identifier(1) and Length(2) */
		do {
			switch (proto) {
			case PPP_LCP:
				pfunc = print_lcp_config_options;
				break;
			case PPP_IPCP:
				pfunc = print_ipcp_config_options;
				break;
			case PPP_CCP:
				pfunc = print_ccp_config_options;
				break;
			case PPP_BACP:
				pfunc = print_bacp_config_options;
				break;
			default:
				/*
				 * This should never happen, but we set
				 * "pfunc" to squelch uninitialized
				 * variable warnings from compilers.
				 */
				pfunc = NULL;
				break;
			}
			if ((j = (*pfunc)(p, len)) == 0)
				break;
			x -= j;
			p += j;
		} while (x > 0);
		break;

	case CPCODES_TERM_REQ:
	case CPCODES_TERM_ACK:
		/* XXX: need to decode Data? */
		break;
	case CPCODES_CODE_REJ:
		/* XXX: need to decode Rejected-Packet? */
		break;
	case CPCODES_PROT_REJ:
		if (length < 6)
			break;
		printf(", Rejected-Protocol=%04x", EXTRACT_16BITS(p));
		/* XXX: need to decode Rejected-Information? */
		break;
	case CPCODES_ECHO_REQ:
	case CPCODES_ECHO_RPL:
	case CPCODES_DISC_REQ:
	case CPCODES_ID:
		if (length < 8)
			break;
		printf(", Magic-Num=%08x", EXTRACT_32BITS(p));
		/* XXX: need to decode Data? */
		break;
	case CPCODES_TIME_REM:
		if (length < 12)
			break;
		printf(", Magic-Num=%08x", EXTRACT_32BITS(p));
		printf(" Seconds-Remaining=%u", EXTRACT_32BITS(p + 4));
		/* XXX: need to decode Message? */
		break;
	default:
		printf(", unknown-Codes-0x%02x", code);
		break;
	}
}

/* LCP config options */
static int
print_lcp_config_options(const u_char *p, int length)
{
	int len, opt;

	if (length < 2)
		return 0;
	len = p[1];
	opt = p[0];
	if (length < len)
		return 0;
	if ((opt >= LCPOPT_MIN) && (opt <= LCPOPT_MAX))
		printf(", %s", lcpconfopts[opt]);
	else {
		printf(", unknwhown-%d", opt);
		return len;
	}

	switch (opt) {
	case LCPOPT_VEXT:
		if (len >= 6) {
			printf(" OUI=%02x%02x%02x", p[2], p[3], p[4]);
#if 0
			printf(" kind=%02x", p[5]);
			printf(" val=")
			for (i = 0; i < len - 6; i++) {
				printf("%02x", p[6 + i]);
			}
#endif
		}
		break;
	case LCPOPT_MRU:
		if (len == 4)
			printf("=%u", EXTRACT_16BITS(p + 2));
		break;
	case LCPOPT_ACCM:
		if (len == 6)
			printf("=%08x", EXTRACT_32BITS(p + 2));
		break;
	case LCPOPT_AP:
		if (len >= 4) {
			if (p[2] == 0xc0 && p[3] == 0x23)
				printf(" PAP");
			else if (p[2] == 0xc2 && p[3] == 0x23) {
				printf(" CHAP/");
				switch (p[4]) {
				default:
					printf("unknown-algorithm-%u", p[4]);
					break;
				case AUTHALG_CHAPMD5:
					printf("MD5");
					break;
				case AUTHALG_MSCHAP1:
					printf("MSCHAPv1");
					break;
				case AUTHALG_MSCHAP2:
					printf("MSCHAPv2");
					break;
				}
			}
			else if (p[2] == 0xc2 && p[3] == 0x27)
				printf(" EAP");
			else if (p[2] == 0xc0 && p[3] == 0x27)
				printf(" SPAP");
			else if (p[2] == 0xc1 && p[3] == 0x23)
				printf(" Old-SPAP");
			else
				printf("unknown");
		}
		break;
	case LCPOPT_QP:
		if (len >= 4) {
			if (p[2] == 0xc0 && p[3] == 0x25)
				printf(" LQR");
			else
				printf(" unknown");
		}
		break;
	case LCPOPT_MN:
		if (len == 6)
			printf("=%08x", EXTRACT_32BITS(p + 2));
		break;
	case LCPOPT_PFC:
		break;
	case LCPOPT_ACFC:
		break;
	case LCPOPT_LD:
		if (len == 4)
			printf("=%04x", EXTRACT_16BITS(p + 2));
		break;
	case LCPOPT_CBACK:
		if (len < 3)
			break;
		switch (p[2]) {		/* Operation */
		case CALLBACK_AUTH:
			printf(" UserAuth");
			break;
		case CALLBACK_DSTR:
			printf(" DialString");
			break;
		case CALLBACK_LID:
			printf(" LocalID");
			break;
		case CALLBACK_E164:
			printf(" E.164");
			break;
		case CALLBACK_X500:
			printf(" X.500");
			break;
		case CALLBACK_CBCP:
			printf(" CBCP");
			break;
		default:
			printf(" unknown-operation=%u", p[2]);
			break;
		}
		break;
	case LCPOPT_MLMRRU:
		if (len == 4)
			printf("=%u", EXTRACT_16BITS(p + 2));
		break;
	case LCPOPT_MLED:
		if (len < 3)
			break;
		switch (p[2]) {		/* class */
		case MEDCLASS_NULL:
			printf(" Null");
			break;
		case MEDCLASS_LOCAL:
			printf(" Local"); /* XXX */
			break;
		case MEDCLASS_IPV4:
			if (len != 7)
				break;
			printf(" IPv4=%s", ipaddr_string(p + 3));
			break;
		case MEDCLASS_MAC:
			if (len != 9)
				break;
			printf(" MAC=%02x:%02x:%02x:%02x:%02x:%02x",
			       p[3], p[4], p[5], p[6], p[7], p[8]);
			break;
		case MEDCLASS_MNB:
			printf(" Magic-Num-Block"); /* XXX */
			break;
		case MEDCLASS_PSNDN:
			printf(" PSNDN"); /* XXX */
			break;
		}
		break;

/* XXX: to be supported */
#if 0
	case LCPOPT_DEP6:
	case LCPOPT_FCSALT:
	case LCPOPT_SDP:
	case LCPOPT_NUMMODE:
	case LCPOPT_DEP12:
	case LCPOPT_DEP14:
	case LCPOPT_DEP15:
	case LCPOPT_DEP16:
	case LCPOPT_MLSSNHF:
	case LCPOPT_PROP:
	case LCPOPT_DCEID:
	case LCPOPT_MPP:
	case LCPOPT_LCPAOPT:
	case LCPOPT_COBS:
	case LCPOPT_PE:
	case LCPOPT_MLHF:
	case LCPOPT_I18N:
	case LCPOPT_SDLOS:
	case LCPOPT_PPPMUX:
		break;
#endif
	}
	return len;
}

/* CHAP */
static void
handle_chap(const u_char *p, int length)
{
	u_int code, len;
	int val_size, name_size, msg_size;
	const u_char *p0;
	int i;

	p0 = p;
	if (length < 1) {
		printf("[|chap]");
		return;
	} else if (length < 4) {
		printf("[|chap 0x%02x]", *p);
		return;
	}

	code = *p;
	if ((code >= CHAP_CODEMIN) && (code <= CHAP_CODEMAX))
		printf("%s", chapcode[code - 1]);
	else {
		printf("0x%02x", code);
		return;
	}
	p++;

	printf("(%u)", *p);		/* ID */
	p++;

	len = EXTRACT_16BITS(p);
	p += 2;

	/*
	 * Note that this is a generic CHAP decoding routine. Since we
	 * don't know which flavor of CHAP (i.e. CHAP-MD5, MS-CHAPv1,
	 * MS-CHAPv2) is used at this point, we can't decode packet
	 * specifically to each algorithms. Instead, we simply decode
	 * the GCD (Gratest Common Denominator) for all algorithms.
	 */
	switch (code) {
	case CHAP_CHAL:
	case CHAP_RESP:
		if (length - (p - p0) < 1)
			return;
		val_size = *p;		/* value size */
		p++;
		if (length - (p - p0) < val_size)
			return;
		printf(", Value=");
		for (i = 0; i < val_size; i++)
			printf("%02x", *p++);
		name_size = len - (p - p0);
		printf(", Name=");
		for (i = 0; i < name_size; i++)
			safeputchar(*p++);
		break;
	case CHAP_SUCC:
	case CHAP_FAIL:
		msg_size = len - (p - p0);
		printf(", Msg=");
		for (i = 0; i< msg_size; i++)
			safeputchar(*p++);
		break;
	}
}

/* PAP (see RFC 1334) */
static void
handle_pap(const u_char *p, int length)
{
	u_int code, len;
	int peerid_len, passwd_len, msg_len;
	const u_char *p0;
	int i;

	p0 = p;
	if (length < 1) {
		printf("[|pap]");
		return;
	} else if (length < 4) {
		printf("[|pap 0x%02x]", *p);
		return;
	}

	code = *p;
	if ((code >= PAP_CODEMIN) && (code <= PAP_CODEMAX))
		printf("%s", papcode[code - 1]);
	else {
		printf("0x%02x", code);
		return;
	}
	p++;

	printf("(%u)", *p);		/* ID */
	p++;

	len = EXTRACT_16BITS(p);
	p += 2;

	switch (code) {
	case PAP_AREQ:
		if (length - (p - p0) < 1)
			return;
		peerid_len = *p;	/* Peer-ID Length */
		p++;
		if (length - (p - p0) < peerid_len)
			return;
		printf(", Peer=");
		for (i = 0; i < peerid_len; i++)
			safeputchar(*p++);

		if (length - (p - p0) < 1)
			return;
		passwd_len = *p;	/* Password Length */
		p++;
		if (length - (p - p0) < passwd_len)
			return;
		printf(", Name=");
		for (i = 0; i < passwd_len; i++)
			safeputchar(*p++);
		break;
	case PAP_AACK:
	case PAP_ANAK:
		if (length - (p - p0) < 1)
			return;
		msg_len = *p;		/* Msg-Length */
		p++;
		if (length - (p - p0) < msg_len)
			return;
		printf(", Msg=");
		for (i = 0; i< msg_len; i++)
			safeputchar(*p++);
		break;
	}
	return;
}

/* BAP */
static void
handle_bap(const u_char *p, int length)
{
	/* XXX: to be supported!! */
}


/* IPCP config options */
static int
print_ipcp_config_options(const u_char *p, int length)
{
	int len, opt;

	if (length < 2)
		return 0;
	len = p[1];
	opt = p[0];
	if (length < len)
		return 0;
	switch (opt) {
	case IPCPOPT_2ADDR:		/* deprecated */
		if (len != 10)
			goto invlen;
		printf(", IP-Addrs src=%s dst=%s",
		       ipaddr_string(p + 2),
		       ipaddr_string(p + 6));
		break;
	case IPCPOPT_IPCOMP:
		if (len < 4)
			goto invlen;
		printf(", IP-Comp");
		if (EXTRACT_16BITS(p + 2) == PPP_VJC) {
			printf(" VJ-Comp");
			/* XXX: VJ-Comp parameters should be decoded */
		} else
			printf(" unknown-comp-proto=%04x", EXTRACT_16BITS(p + 2));
		break;
	case IPCPOPT_ADDR:
		if (len != 6)
			goto invlen;
		printf(", IP-Addr=%s", ipaddr_string(p + 2));
		break;
	case IPCPOPT_MOBILE4:
		if (len != 6)
			goto invlen;
		printf(", Home-Addr=%s", ipaddr_string(p + 2));
		break;
	case IPCPOPT_PRIDNS:
		if (len != 6)
			goto invlen;
		printf(", Pri-DNS=%s", ipaddr_string(p + 2));
		break;
	case IPCPOPT_PRINBNS:
		if (len != 6)
			goto invlen;
		printf(", Pri-NBNS=%s", ipaddr_string(p + 2));
		break;
	case IPCPOPT_SECDNS:
		if (len != 6)
			goto invlen;
		printf(", Sec-DNS=%s", ipaddr_string(p + 2));
		break;
	case IPCPOPT_SECNBNS:
		if (len != 6)
			goto invlen;
		printf(", Sec-NBNS=%s", ipaddr_string(p + 2));
		break;
	default:
		printf(", unknown-%d", opt);
		break;
	}
	return len;

invlen:
	printf(", invalid-length-%d", opt);
	return 0;
}

/* CCP config options */
static int
print_ccp_config_options(const u_char *p, int length)
{
	int len, opt;

	if (length < 2)
		return 0;
	len = p[1];
	opt = p[0];
	if (length < len)
		return 0;
	if ((opt >= CCPOPT_MIN) && (opt <= CCPOPT_MAX))
		printf(", %s", ccpconfopts[opt]);
#if 0	/* XXX */
	switch (opt) {
	case CCPOPT_OUI:
	case CCPOPT_PRED1:
	case CCPOPT_PRED2:
	case CCPOPT_PJUMP:
	case CCPOPT_HPPPC:
	case CCPOPT_STACLZS:
	case CCPOPT_MPPC:
	case CCPOPT_GFZA:
	case CCPOPT_V42BIS:
	case CCPOPT_BSDCOMP:
	case CCPOPT_LZSDCP:
	case CCPOPT_MVRCA:
	case CCPOPT_DEC:
	case CCPOPT_DEFLATE:
	case CCPOPT_RESV:
		break;

	default:
		printf(", unknown-%d", opt);
		break;
	}
#endif
	return len;
}

/* BACP config options */
static int
print_bacp_config_options(const u_char *p, int length)
{
	int len, opt;

	if (length < 2)
		return 0;
	len = p[1];
	opt = p[0];
	if (length < len)
		return 0;
	if (opt == BACPOPT_FPEER) {
		printf(", Favored-Peer");
		printf(" Magic-Num=%08x", EXTRACT_32BITS(p + 2));
	} else {
		printf(", unknown-option-%d", opt);
	}
	return len;
}


/* PPP */
static void
handle_ppp(u_int proto, const u_char *p, int length)
{
	switch (proto) {
	case PPP_LCP:
	case PPP_IPCP:
	case PPP_CCP:
	case PPP_BACP:
		handle_ctrl_proto(proto, p, length);
		break;
	case PPP_CHAP:
		handle_chap(p, length);
		break;
	case PPP_PAP:
		handle_pap(p, length);
		break;
	case PPP_BAP:		/* XXX: not yet completed */
		handle_bap(p, length);
		break;
	case ETHERTYPE_IP:	/*XXX*/
	case PPP_IP:
		ip_print(p, length);
		break;
#ifdef INET6
	case ETHERTYPE_IPV6:	/*XXX*/
	case PPP_IPV6:
		ip6_print(p, length);
		break;
#endif
	case ETHERTYPE_IPX:	/*XXX*/
	case PPP_IPX:
		ipx_print(p, length);
		break;
	case PPP_OSI:
	        isoclns_print(p, length, length, NULL, NULL);
	        break;
	default:
		break;
	}
}

/* Standard PPP printer */
void
ppp_print(register const u_char *p, u_int length)
{
	u_int proto;
	u_int full_length = length;

	/*
	 * Here, we assume that p points to the Address and Control
	 * field (if they present).
	 */
	if (length < 2)
		goto trunc;
	if (*p == PPP_ADDRESS && *(p + 1) == PPP_CONTROL) {
		p += 2;			/* ACFC not used */
		length -= 2;
	}

	if (length < 2)
		goto trunc;
	if (*p % 2) {
		proto = *p;		/* PFC is used */
		p++;
		length--;
	} else {
		proto = EXTRACT_16BITS(p);
		p += 2;
		length -= 2;
	}

	if (eflag)
		printf("%s %d: ", ppp_protoname(proto), full_length);

	handle_ppp(proto, p, length);
	return;
trunc:
	printf("[|ppp]");
}


/* PPP I/F printer */
void
ppp_if_print(u_char *user, const struct pcap_pkthdr *h,
	     register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;

	++infodelay;
	ts_print(&h->ts);

	if (caplen < PPP_HDRLEN) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.  */

	packetp = p;
	snapend = p + caplen;

#if 0
	/*
	 * XXX: seems to assume that there are 2 octets prepended to an
	 * actual PPP frame. The 1st octet looks like Input/Output flag
	 * while 2nd octet is unknown, at least to me
	 * (mshindo@mshindo.net).
	 *
	 * That was what the original tcpdump code did.
	 *
	 * FreeBSD's "if_ppp.c" *does* set the first octet to 1 for outbound
	 * packets and 0 for inbound packets - but only if the
	 * protocol field has the 0x8000 bit set (i.e., it's a network
	 * control protocol); it does so before running the packet through
	 * "bpf_filter" to see if it should be discarded, and to see
	 * if we should update the time we sent the most recent packet...
	 *
	 * ...but it puts the original address field back after doing
	 * so.
	 *
	 * NetBSD's "if_ppp.c" doesn't set the first octet in that fashion.
	 *
	 * I don't know if any PPP implementation handed up to a BPF
	 * device packets with the first octet being 1 for outbound and
	 * 0 for inbound packets, so I (guy@alum.mit.edu) don't know
	 * whether that ever needs to be checked or not.
	 *
	 * Note that NetBSD has a DLT_PPP_SERIAL, which it uses for PPP,
	 * and its tcpdump appears to assume that the frame always
	 * begins with an address field and a control field, and that
	 * the address field might be 0x0f or 0x8f, for Cisco
	 * point-to-point with HDLC framing as per section 4.3.1 of RFC
	 * 1547, as well as 0xff, for PPP in HDLC-like framing as per
	 * RFC 1662.
	 *
	 * (Is the Cisco framing in question what DLT_C_HDLC, in
	 * BSD/OS, is?)
	 */
	if (eflag)
		printf("%c %4d %02x ", p[0] ? 'O' : 'I', length, p[1]);
#endif

	ppp_print(p, length);

	if (xflag)
		default_print(p, caplen);
out:
	putchar('\n');
	--infodelay;
	if (infoprint)
		info(0);
}

/*
 * PPP I/F printer to use if we know that RFC 1662-style PPP in HDLC-like
 * framing, or Cisco PPP with HDLC framing as per section 4.3.1 of RFC 1547,
 * is being used (i.e., we don't check for PPP_ADDRESS and PPP_CONTROL,
 * discard them *if* those are the first two octets, and parse the remaining
 * packet as a PPP packet, as "ppp_print()" does).
 *
 * This handles, for example, DLT_PPP_SERIAL in NetBSD.
 */
void
ppp_hdlc_if_print(u_char *user, const struct pcap_pkthdr *h,
	     register const u_char *p)
{
	register u_int length = h->len;
	register u_int caplen = h->caplen;
	u_int proto;

	++infodelay;
	ts_print(&h->ts);

	if (caplen < 2) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	switch (p[0]) {

	case PPP_ADDRESS:
		if (caplen < 4) {
			printf("[|ppp]");
			goto out;
		}

		if (eflag)
			printf("%02x %02x %d ", p[0], p[1], length);
		p += 2;
		length -= 2;

		proto = EXTRACT_16BITS(p);
		p += 2;
		length -= 2;
		if (eflag)
			printf("%s: ", ppp_protoname(proto));

		handle_ppp(proto, p, length);
		break;

	case CHDLC_UNICAST:
	case CHDLC_BCAST:
		chdlc_print(p, length, caplen);
		goto out;

	default:
		if (eflag)
			printf("%02x %02x %d ", p[0], p[1], length);
		p += 2;
		length -= 2;

		/*
		 * XXX - NetBSD's "ppp_netbsd_serial_if_print()" treats
		 * the next two octets as an Ethernet type; does that
		 * ever happen?
		 */
		printf("unknown addr %02x; ctrl %02x", p[0], p[1]);
		break;
	}

	if (xflag)
		default_print(p, caplen);
out:
	putchar('\n');
	--infodelay;
	if (infoprint)
		info(0);
}



struct tok ppptype2str[] = {
	{ PPP_IP,	"IP" },
	{ PPP_OSI,	"OSI" },
	{ PPP_NS,	"NS" },
	{ PPP_DECNET,	"DECNET" },
	{ PPP_APPLE,	"APPLE" },
	{ PPP_IPX,	"IPX" },
	{ PPP_VJC,	"VJC" },
	{ PPP_VJNC,	"VJNC" },
	{ PPP_BRPDU,	"BRPDU" },
	{ PPP_STII,	"STII" },
	{ PPP_VINES,	"VINES" },

	{ PPP_HELLO,	"HELLO" },
	{ PPP_LUXCOM,	"LUXCOM" },
	{ PPP_SNS,	"SNS" },
	{ PPP_IPCP,	"IPCP" },
	{ PPP_OSICP,	"OSICP" },
	{ PPP_NSCP,	"NSCP" },
	{ PPP_DECNETCP, "DECNETCP" },
	{ PPP_APPLECP,	"APPLECP" },
	{ PPP_IPXCP,	"IPXCP" },
	{ PPP_STIICP,	"STIICP" },
	{ PPP_VINESCP,	"VINESCP" },

	{ PPP_LCP,	"LCP" },
	{ PPP_PAP,	"PAP" },
	{ PPP_LQM,	"LQM" },
	{ PPP_CHAP,	"CHAP" },
	{ PPP_BACP,	"BACP" },
	{ PPP_BAP,	"BAP" },
	{ PPP_MP,	"MP" },
	{ 0,		NULL }
};

#define PPP_BSDI_HDRLEN 24

/* BSD/OS specific PPP printer */
void
ppp_bsdos_if_print(u_char *user, const struct pcap_pkthdr *h,
	     register const u_char *p)
{
#ifdef __bsdi__
	register u_int length = h->len;
	register u_int caplen = h->caplen;
	register int hdrlength;
	u_int16_t ptype;
	const u_char *q;
	int i;

	++infodelay;
	ts_print(&h->ts);

	if (caplen < PPP_BSDI_HDRLEN) {
		printf("[|ppp]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;
	hdrlength = 0;

#if 0
	if (p[0] == PPP_ADDRESS && p[1] == PPP_CONTROL) {
		if (eflag)
			printf("%02x %02x ", p[0], p[1]);
		p += 2;
		hdrlength = 2;
	}

	if (eflag)
		printf("%d ", length);
	/* Retrieve the protocol type */
	if (*p & 01) {
		/* Compressed protocol field */
		ptype = *p;
		if (eflag)
			printf("%02x ", ptype);
		p++;
		hdrlength += 1;
	} else {
		/* Un-compressed protocol field */
		ptype = ntohs(*(u_int16_t *)p);
		if (eflag)
			printf("%04x ", ptype);
		p += 2;
		hdrlength += 2;
	}
#else
	ptype = 0;	/*XXX*/
	if (eflag)
		printf("%c ", p[SLC_DIR] ? 'O' : 'I');
	if (p[SLC_LLHL]) {
		/* link level header */
		struct ppp_header *ph;

		q = p + SLC_BPFHDRLEN;
		ph = (struct ppp_header *)q;
		if (ph->phdr_addr == PPP_ADDRESS
		 && ph->phdr_ctl == PPP_CONTROL) {
			if (eflag)
				printf("%02x %02x ", q[0], q[1]);
			ptype = ntohs(ph->phdr_type);
			if (eflag && (ptype == PPP_VJC || ptype == PPP_VJNC)) {
				printf("%s ", tok2str(ppptype2str,
						"proto-#%d", ptype));
			}
		} else {
			if (eflag) {
				printf("LLH=[");
				for (i = 0; i < p[SLC_LLHL]; i++)
					printf("%02x", q[i]);
				printf("] ");
			}
		}
	}
	if (eflag)
		printf("%d ", length);
	if (p[SLC_CHL]) {
		q = p + SLC_BPFHDRLEN + p[SLC_LLHL];

		switch (ptype) {
		case PPP_VJC:
			ptype = vjc_print(q, length - (q - p), ptype);
			hdrlength = PPP_BSDI_HDRLEN;
			p += hdrlength;
			switch (ptype) {
			case PPP_IP:
				ip_print(p, length);
				break;
#ifdef INET6
			case PPP_IPV6:
				ip6_print(p, length);
				break;
#endif
			}
			goto printx;
		case PPP_VJNC:
			ptype = vjc_print(q, length - (q - p), ptype);
			hdrlength = PPP_BSDI_HDRLEN;
			p += hdrlength;
			switch (ptype) {
			case PPP_IP:
				ip_print(p, length);
				break;
#ifdef INET6
			case PPP_IPV6:
				ip6_print(p, length);
				break;
#endif
			}
			goto printx;
		default:
			if (eflag) {
				printf("CH=[");
				for (i = 0; i < p[SLC_LLHL]; i++)
					printf("%02x", q[i]);
				printf("] ");
			}
			break;
		}
	}

	hdrlength = PPP_BSDI_HDRLEN;
#endif

	length -= hdrlength;
	p += hdrlength;

	switch (ptype) {
	case PPP_IP:
		ip_print(p, length);
		break;
#ifdef INET6
	case PPP_IPV6:
		ip6_print(p, length);
		break;
#endif
	default:
		printf("%s ", tok2str(ppptype2str, "proto-#%d", ptype));
	}

printx:
	if (xflag)
		default_print((const u_char *)p, caplen - hdrlength);
out:
	putchar('\n');
	--infodelay;
	if (infoprint)
		info(0);
#endif /* __bsdi__ */
}
