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
 */

/* \summary: Point to Point Protocol (PPP) printer */

/*
 * TODO:
 * o resolve XXX as much as possible
 * o MP support
 * o BAP support
 */

#include <config.h>

#include "netdissect-stdinc.h"

#include <stdlib.h>

#ifdef __bsdi__
#include <net/slcompress.h>
#include <net/if_ppp.h>
#endif

#include <stdlib.h>

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"
#include "ppp.h"
#include "chdlc.h"
#include "ethertype.h"
#include "oui.h"
#include "netdissect-alloc.h"

/*
 * The following constants are defined by IANA. Please refer to
 *    https://www.isi.edu/in-notes/iana/assignments/ppp-numbers
 * for the up-to-date information.
 */

/* Protocol Codes defined in ppp.h */

static const struct tok ppptype2str[] = {
        { PPP_IP,	  "IP" },
        { PPP_OSI,	  "OSI" },
        { PPP_NS,	  "NS" },
        { PPP_DECNET,	  "DECNET" },
        { PPP_APPLE,	  "APPLE" },
	{ PPP_IPX,	  "IPX" },
	{ PPP_VJC,	  "VJC IP" },
	{ PPP_VJNC,	  "VJNC IP" },
	{ PPP_BRPDU,	  "BRPDU" },
	{ PPP_STII,	  "STII" },
	{ PPP_VINES,	  "VINES" },
	{ PPP_MPLS_UCAST, "MPLS" },
	{ PPP_MPLS_MCAST, "MPLS" },
        { PPP_COMP,       "Compressed"},
        { PPP_ML,         "MLPPP"},
        { PPP_IPV6,       "IP6"},

	{ PPP_HELLO,	  "HELLO" },
	{ PPP_LUXCOM,	  "LUXCOM" },
	{ PPP_SNS,	  "SNS" },
	{ PPP_IPCP,	  "IPCP" },
	{ PPP_OSICP,	  "OSICP" },
	{ PPP_NSCP,	  "NSCP" },
	{ PPP_DECNETCP,   "DECNETCP" },
	{ PPP_APPLECP,	  "APPLECP" },
	{ PPP_IPXCP,	  "IPXCP" },
	{ PPP_STIICP,	  "STIICP" },
	{ PPP_VINESCP,	  "VINESCP" },
        { PPP_IPV6CP,     "IP6CP" },
	{ PPP_MPLSCP,	  "MPLSCP" },

	{ PPP_LCP,	  "LCP" },
	{ PPP_PAP,	  "PAP" },
	{ PPP_LQM,	  "LQM" },
	{ PPP_CHAP,	  "CHAP" },
	{ PPP_EAP,	  "EAP" },
	{ PPP_SPAP,	  "SPAP" },
	{ PPP_SPAP_OLD,	  "Old-SPAP" },
	{ PPP_BACP,	  "BACP" },
	{ PPP_BAP,	  "BAP" },
	{ PPP_MPCP,	  "MLPPP-CP" },
	{ PPP_CCP,	  "CCP" },
	{ 0,		  NULL }
};

/* Control Protocols (LCP/IPCP/CCP etc.) Codes defined in RFC 1661 */

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
#define CPCODES_ID		12	/* Identification (LCP only) RFC1570 */
#define CPCODES_TIME_REM	13	/* Time-Remaining (LCP only) RFC1570 */
#define CPCODES_RESET_REQ	14	/* Reset-Request (CCP only) RFC1962 */
#define CPCODES_RESET_REP	15	/* Reset-Reply (CCP only) */

static const struct tok cpcodes[] = {
	{CPCODES_VEXT,      "Vendor-Extension"}, /* RFC2153 */
	{CPCODES_CONF_REQ,  "Conf-Request"},
        {CPCODES_CONF_ACK,  "Conf-Ack"},
	{CPCODES_CONF_NAK,  "Conf-Nack"},
	{CPCODES_CONF_REJ,  "Conf-Reject"},
	{CPCODES_TERM_REQ,  "Term-Request"},
	{CPCODES_TERM_ACK,  "Term-Ack"},
	{CPCODES_CODE_REJ,  "Code-Reject"},
	{CPCODES_PROT_REJ,  "Prot-Reject"},
	{CPCODES_ECHO_REQ,  "Echo-Request"},
	{CPCODES_ECHO_RPL,  "Echo-Reply"},
	{CPCODES_DISC_REQ,  "Disc-Req"},
	{CPCODES_ID,        "Ident"},            /* RFC1570 */
	{CPCODES_TIME_REM,  "Time-Rem"},         /* RFC1570 */
	{CPCODES_RESET_REQ, "Reset-Req"},        /* RFC1962 */
	{CPCODES_RESET_REP, "Reset-Ack"},        /* RFC1962 */
        {0,                 NULL}
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
	"deprecated(15)",	/* used to be a Compound-Frames */
	"deprecated(16)",	/* used to be a Nominal-Data-Encap */
	"MRRU",			/* (17) */
	"12-Bit seq #",		/* (18) */
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

#define NUM_LCPOPTS	(sizeof(lcpconfopts) / sizeof(lcpconfopts[0]))

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

static const struct tok ccpconfopts_values[] = {
        { CCPOPT_OUI, "OUI" },
        { CCPOPT_PRED1, "Pred-1" },
        { CCPOPT_PRED2, "Pred-2" },
        { CCPOPT_PJUMP, "Puddle" },
        { CCPOPT_HPPPC, "HP-PPC" },
        { CCPOPT_STACLZS, "Stac-LZS" },
        { CCPOPT_MPPC, "MPPC" },
        { CCPOPT_GFZA, "Gand-FZA" },
        { CCPOPT_V42BIS, "V.42bis" },
        { CCPOPT_BSDCOMP, "BSD-Comp" },
        { CCPOPT_LZSDCP, "LZS-DCP" },
        { CCPOPT_MVRCA, "MVRCA" },
        { CCPOPT_DEC, "DEC" },
        { CCPOPT_DEFLATE, "Deflate" },
        { CCPOPT_RESV, "Reserved"},
        {0,                 NULL}
};

/* BACP Config Options */

#define BACPOPT_FPEER	1	/* RFC2125 */

static const struct tok bacconfopts_values[] = {
        { BACPOPT_FPEER, "Favored-Peer" },
        {0,                 NULL}
};


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

static const struct tok ipcpopt_values[] = {
        { IPCPOPT_2ADDR, "IP-Addrs" },
        { IPCPOPT_IPCOMP, "IP-Comp" },
        { IPCPOPT_ADDR, "IP-Addr" },
        { IPCPOPT_MOBILE4, "Home-Addr" },
        { IPCPOPT_PRIDNS, "Pri-DNS" },
        { IPCPOPT_PRINBNS, "Pri-NBNS" },
        { IPCPOPT_SECDNS, "Sec-DNS" },
        { IPCPOPT_SECNBNS, "Sec-NBNS" },
	{ 0,		  NULL }
};

#define IPCPOPT_IPCOMP_HDRCOMP 0x61  /* rfc3544 */
#define IPCPOPT_IPCOMP_MINLEN    14

static const struct tok ipcpopt_compproto_values[] = {
        { PPP_VJC, "VJ-Comp" },
        { IPCPOPT_IPCOMP_HDRCOMP, "IP Header Compression" },
	{ 0,		  NULL }
};

static const struct tok ipcpopt_compproto_subopt_values[] = {
        { 1, "RTP-Compression" },
        { 2, "Enhanced RTP-Compression" },
	{ 0,		  NULL }
};

/* IP6CP Config Options */
#define IP6CP_IFID      1

static const struct tok ip6cpopt_values[] = {
        { IP6CP_IFID, "Interface-ID" },
	{ 0,		  NULL }
};

/* ATCP - to be supported */
/* OSINLCP - to be supported */
/* BVCP - to be supported */
/* BCP - to be supported */
/* IPXCP - to be supported */
/* MPLSCP - to be supported */

/* Auth Algorithms */

/* 0-4 Reserved (RFC1994) */
#define AUTHALG_CHAPMD5	5	/* RFC1994 */
#define AUTHALG_MSCHAP1	128	/* RFC2433 */
#define AUTHALG_MSCHAP2	129	/* RFC2795 */

static const struct tok authalg_values[] = {
        { AUTHALG_CHAPMD5, "MD5" },
        { AUTHALG_MSCHAP1, "MS-CHAPv1" },
        { AUTHALG_MSCHAP2, "MS-CHAPv2" },
	{ 0,		  NULL }
};

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

static const struct tok ppp_callback_values[] = {
        { CALLBACK_AUTH, "UserAuth" },
        { CALLBACK_DSTR, "DialString" },
        { CALLBACK_LID, "LocalID" },
        { CALLBACK_E164, "E.164" },
        { CALLBACK_X500, "X.500" },
        { CALLBACK_CBCP, "CBCP" },
	{ 0,		  NULL }
};

/* CHAP */

#define CHAP_CHAL	1
#define CHAP_RESP	2
#define CHAP_SUCC	3
#define CHAP_FAIL	4

static const struct tok chapcode_values[] = {
	{ CHAP_CHAL, "Challenge" },
	{ CHAP_RESP, "Response" },
	{ CHAP_SUCC, "Success" },
	{ CHAP_FAIL, "Fail" },
        { 0, NULL}
};

/* PAP */

#define PAP_AREQ	1
#define PAP_AACK	2
#define PAP_ANAK	3

static const struct tok papcode_values[] = {
        { PAP_AREQ, "Auth-Req" },
        { PAP_AACK, "Auth-ACK" },
        { PAP_ANAK, "Auth-NACK" },
        { 0, NULL }
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

static u_int print_lcp_config_options(netdissect_options *, const u_char *p, u_int);
static u_int print_ipcp_config_options(netdissect_options *, const u_char *p, u_int);
static u_int print_ip6cp_config_options(netdissect_options *, const u_char *p, u_int);
static u_int print_ccp_config_options(netdissect_options *, const u_char *p, u_int);
static u_int print_bacp_config_options(netdissect_options *, const u_char *p, u_int);
static void handle_ppp(netdissect_options *, u_int proto, const u_char *p, u_int length);

/* generic Control Protocol (e.g. LCP, IPCP, CCP, etc.) handler */
static void
handle_ctrl_proto(netdissect_options *ndo,
                  u_int proto, const u_char *pptr, u_int length)
{
	const char *typestr;
	u_int code, len;
	u_int (*pfunc)(netdissect_options *, const u_char *, u_int);
	u_int tlen, advance;
        const u_char *tptr;

        tptr=pptr;

        typestr = tok2str(ppptype2str, "unknown ctrl-proto (0x%04x)", proto);
	ND_PRINT("%s, ", typestr);

	if (length < 4) /* FIXME weak boundary checking */
		goto trunc;
	ND_TCHECK_2(tptr);

	code = GET_U_1(tptr);
	tptr++;

	ND_PRINT("%s (0x%02x), id %u, length %u",
	          tok2str(cpcodes, "Unknown Opcode",code),
	          code,
	          GET_U_1(tptr), /* ID */
	          length + 2);
	tptr++;

	if (!ndo->ndo_vflag)
		return;

	len = GET_BE_U_2(tptr);
	tptr += 2;

	if (len < 4) {
		ND_PRINT("\n\tencoded length %u (< 4))", len);
		return;
	}

	if (len > length) {
		ND_PRINT("\n\tencoded length %u (> packet length %u))", len, length);
		return;
	}
	length = len;

	ND_PRINT("\n\tencoded length %u (=Option(s) length %u)", len, len - 4);

	if (length == 4)
		return;    /* there may be a NULL confreq etc. */

	if (ndo->ndo_vflag > 1)
		print_unknown_data(ndo, pptr - 2, "\n\t", 6);


	switch (code) {
	case CPCODES_VEXT:
		if (length < 11)
			break;
		ND_PRINT("\n\t  Magic-Num 0x%08x", GET_BE_U_4(tptr));
		tptr += 4;
		ND_PRINT(" Vendor: %s (%u)",
                       tok2str(oui_values,"Unknown",GET_BE_U_3(tptr)),
                       GET_BE_U_3(tptr));
		/* XXX: need to decode Kind and Value(s)? */
		break;
	case CPCODES_CONF_REQ:
	case CPCODES_CONF_ACK:
	case CPCODES_CONF_NAK:
	case CPCODES_CONF_REJ:
		tlen = len - 4;	/* Code(1), Identifier(1) and Length(2) */
		do {
			switch (proto) {
			case PPP_LCP:
				pfunc = print_lcp_config_options;
				break;
			case PPP_IPCP:
				pfunc = print_ipcp_config_options;
				break;
			case PPP_IPV6CP:
				pfunc = print_ip6cp_config_options;
				break;
			case PPP_CCP:
				pfunc = print_ccp_config_options;
				break;
			case PPP_BACP:
				pfunc = print_bacp_config_options;
				break;
			default:
				/*
				 * No print routine for the options for
				 * this protocol.
				 */
				pfunc = NULL;
				break;
			}

			if (pfunc == NULL) /* catch the above null pointer if unknown CP */
				break;

			if ((advance = (*pfunc)(ndo, tptr, len)) == 0)
				break;
			if (tlen < advance) {
				ND_PRINT(" [remaining options length %u < %u]",
					 tlen, advance);
				nd_print_invalid(ndo);
				break;
			}
			tlen -= advance;
			tptr += advance;
		} while (tlen != 0);
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
		ND_PRINT("\n\t  Rejected %s Protocol (0x%04x)",
		       tok2str(ppptype2str,"unknown", GET_BE_U_2(tptr)),
		       GET_BE_U_2(tptr));
		/* XXX: need to decode Rejected-Information? - hexdump for now */
		if (len > 6) {
			ND_PRINT("\n\t  Rejected Packet");
			print_unknown_data(ndo, tptr + 2, "\n\t    ", len - 2);
		}
		break;
	case CPCODES_ECHO_REQ:
	case CPCODES_ECHO_RPL:
	case CPCODES_DISC_REQ:
		if (length < 8)
			break;
		ND_PRINT("\n\t  Magic-Num 0x%08x", GET_BE_U_4(tptr));
		/* XXX: need to decode Data? - hexdump for now */
		if (len > 8) {
			ND_PRINT("\n\t  -----trailing data-----");
			ND_TCHECK_LEN(tptr + 4, len - 8);
			print_unknown_data(ndo, tptr + 4, "\n\t  ", len - 8);
		}
		break;
	case CPCODES_ID:
		if (length < 8)
			break;
		ND_PRINT("\n\t  Magic-Num 0x%08x", GET_BE_U_4(tptr));
		/* RFC 1661 says this is intended to be human readable */
		if (len > 8) {
			ND_PRINT("\n\t  Message\n\t    ");
			if (nd_printn(ndo, tptr + 4, len - 4, ndo->ndo_snapend))
				goto trunc;
		}
		break;
	case CPCODES_TIME_REM:
		if (length < 12)
			break;
		ND_PRINT("\n\t  Magic-Num 0x%08x", GET_BE_U_4(tptr));
		ND_PRINT(", Seconds-Remaining %us", GET_BE_U_4(tptr + 4));
		/* XXX: need to decode Message? */
		break;
	default:
		/* XXX this is dirty but we do not get the
		 * original pointer passed to the begin
		 * the PPP packet */
		if (ndo->ndo_vflag <= 1)
			print_unknown_data(ndo, pptr - 2, "\n\t  ", length + 2);
		break;
	}
	return;

trunc:
	ND_PRINT("[|%s]", typestr);
}

/* LCP config options */
static u_int
print_lcp_config_options(netdissect_options *ndo,
                         const u_char *p, u_int length)
{
	u_int opt, len;

	if (length < 2)
		return 0;
	ND_TCHECK_2(p);
	opt = GET_U_1(p);
	len = GET_U_1(p + 1);
	if (length < len)
		return 0;
	if (len < 2) {
		if (opt < NUM_LCPOPTS)
			ND_PRINT("\n\t  %s Option (0x%02x), length %u (length bogus, should be >= 2)",
			          lcpconfopts[opt], opt, len);
		else
			ND_PRINT("\n\tunknown LCP option 0x%02x", opt);
		return 0;
	}
	if (opt < NUM_LCPOPTS)
		ND_PRINT("\n\t  %s Option (0x%02x), length %u", lcpconfopts[opt], opt, len);
	else {
		ND_PRINT("\n\tunknown LCP option 0x%02x", opt);
		return len;
	}

	switch (opt) {
	case LCPOPT_VEXT:
		if (len < 6) {
			ND_PRINT(" (length bogus, should be >= 6)");
			return len;
		}
		ND_PRINT(": Vendor: %s (%u)",
			tok2str(oui_values,"Unknown",GET_BE_U_3(p + 2)),
			GET_BE_U_3(p + 2));
#if 0
		ND_PRINT(", kind: 0x%02x", GET_U_1(p + 5));
		ND_PRINT(", Value: 0x");
		for (i = 0; i < len - 6; i++) {
			ND_PRINT("%02x", GET_U_1(p + 6 + i));
		}
#endif
		break;
	case LCPOPT_MRU:
		if (len != 4) {
			ND_PRINT(" (length bogus, should be = 4)");
			return len;
		}
		ND_PRINT(": %u", GET_BE_U_2(p + 2));
		break;
	case LCPOPT_ACCM:
		if (len != 6) {
			ND_PRINT(" (length bogus, should be = 6)");
			return len;
		}
		ND_PRINT(": 0x%08x", GET_BE_U_4(p + 2));
		break;
	case LCPOPT_AP:
		if (len < 4) {
			ND_PRINT(" (length bogus, should be >= 4)");
			return len;
		}
		ND_PRINT(": %s",
			 tok2str(ppptype2str, "Unknown Auth Proto (0x04x)", GET_BE_U_2(p + 2)));

		switch (GET_BE_U_2(p + 2)) {
		case PPP_CHAP:
			ND_PRINT(", %s",
				 tok2str(authalg_values, "Unknown Auth Alg %u", GET_U_1(p + 4)));
			break;
		case PPP_PAP: /* fall through */
		case PPP_EAP:
		case PPP_SPAP:
		case PPP_SPAP_OLD:
                        break;
		default:
			print_unknown_data(ndo, p, "\n\t", len);
		}
		break;
	case LCPOPT_QP:
		if (len < 4) {
			ND_PRINT(" (length bogus, should be >= 4)");
			return 0;
		}
		if (GET_BE_U_2(p + 2) == PPP_LQM)
			ND_PRINT(": LQR");
		else
			ND_PRINT(": unknown");
		break;
	case LCPOPT_MN:
		if (len != 6) {
			ND_PRINT(" (length bogus, should be = 6)");
			return 0;
		}
		ND_PRINT(": 0x%08x", GET_BE_U_4(p + 2));
		break;
	case LCPOPT_PFC:
		break;
	case LCPOPT_ACFC:
		break;
	case LCPOPT_LD:
		if (len != 4) {
			ND_PRINT(" (length bogus, should be = 4)");
			return 0;
		}
		ND_PRINT(": 0x%04x", GET_BE_U_2(p + 2));
		break;
	case LCPOPT_CBACK:
		if (len < 3) {
			ND_PRINT(" (length bogus, should be >= 3)");
			return 0;
		}
		ND_PRINT(": Callback Operation %s (%u)",
                       tok2str(ppp_callback_values, "Unknown", GET_U_1(p + 2)),
                       GET_U_1(p + 2));
		break;
	case LCPOPT_MLMRRU:
		if (len != 4) {
			ND_PRINT(" (length bogus, should be = 4)");
			return 0;
		}
		ND_PRINT(": %u", GET_BE_U_2(p + 2));
		break;
	case LCPOPT_MLED:
		if (len < 3) {
			ND_PRINT(" (length bogus, should be >= 3)");
			return 0;
		}
		switch (GET_U_1(p + 2)) {		/* class */
		case MEDCLASS_NULL:
			ND_PRINT(": Null");
			break;
		case MEDCLASS_LOCAL:
			ND_PRINT(": Local"); /* XXX */
			break;
		case MEDCLASS_IPV4:
			if (len != 7) {
				ND_PRINT(" (length bogus, should be = 7)");
				return 0;
			}
			ND_PRINT(": IPv4 %s", GET_IPADDR_STRING(p + 3));
			break;
		case MEDCLASS_MAC:
			if (len != 9) {
				ND_PRINT(" (length bogus, should be = 9)");
				return 0;
			}
			ND_PRINT(": MAC %s", GET_ETHERADDR_STRING(p + 3));
			break;
		case MEDCLASS_MNB:
			ND_PRINT(": Magic-Num-Block"); /* XXX */
			break;
		case MEDCLASS_PSNDN:
			ND_PRINT(": PSNDN"); /* XXX */
			break;
		default:
			ND_PRINT(": Unknown class %u", GET_U_1(p + 2));
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
	default:
		/*
		 * Unknown option; dump it as raw bytes now if we're
		 * not going to do so below.
		 */
		if (ndo->ndo_vflag < 2)
			print_unknown_data(ndo, p + 2, "\n\t    ", len - 2);
		break;
	}

	if (ndo->ndo_vflag > 1)
		print_unknown_data(ndo, p + 2, "\n\t    ", len - 2); /* exclude TLV header */

	return len;

trunc:
	ND_PRINT("[|lcp]");
	return 0;
}

/* ML-PPP*/
static const struct tok ppp_ml_flag_values[] = {
    { 0x80, "begin" },
    { 0x40, "end" },
    { 0, NULL }
};

static void
handle_mlppp(netdissect_options *ndo,
             const u_char *p, u_int length)
{
    if (!ndo->ndo_eflag)
        ND_PRINT("MLPPP, ");

    if (length < 2) {
        ND_PRINT("[|mlppp]");
        return;
    }
    if (!ND_TTEST_2(p)) {
        ND_PRINT("[|mlppp]");
        return;
    }

    ND_PRINT("seq 0x%03x, Flags [%s], length %u",
           (GET_BE_U_2(p))&0x0fff,
           /* only support 12-Bit sequence space for now */
           bittok2str(ppp_ml_flag_values, "none", GET_U_1(p) & 0xc0),
           length);
}

/* CHAP */
static void
handle_chap(netdissect_options *ndo,
            const u_char *p, u_int length)
{
	u_int code, len;
	u_int val_size, name_size, msg_size;
	const u_char *p0;
	u_int i;

	p0 = p;
	if (length < 1) {
		ND_PRINT("[|chap]");
		return;
	} else if (length < 4) {
		ND_PRINT("[|chap 0x%02x]", GET_U_1(p));
		return;
	}

	code = GET_U_1(p);
	ND_PRINT("CHAP, %s (0x%02x)",
               tok2str(chapcode_values,"unknown",code),
               code);
	p++;

	ND_PRINT(", id %u", GET_U_1(p));	/* ID */
	p++;

	len = GET_BE_U_2(p);
	p += 2;

	/*
	 * Note that this is a generic CHAP decoding routine. Since we
	 * don't know which flavor of CHAP (i.e. CHAP-MD5, MS-CHAPv1,
	 * MS-CHAPv2) is used at this point, we can't decode packet
	 * specifically to each algorithms. Instead, we simply decode
	 * the GCD (Greatest Common Denominator) for all algorithms.
	 */
	switch (code) {
	case CHAP_CHAL:
	case CHAP_RESP:
		if (length - (p - p0) < 1)
			return;
		val_size = GET_U_1(p);	/* value size */
		p++;
		if (length - (p - p0) < val_size)
			return;
		ND_PRINT(", Value ");
		for (i = 0; i < val_size; i++) {
			ND_PRINT("%02x", GET_U_1(p));
			p++;
		}
		name_size = len - (u_int)(p - p0);
		ND_PRINT(", Name ");
		for (i = 0; i < name_size; i++) {
			fn_print_char(ndo, GET_U_1(p));
			p++;
		}
		break;
	case CHAP_SUCC:
	case CHAP_FAIL:
		msg_size = len - (u_int)(p - p0);
		ND_PRINT(", Msg ");
		for (i = 0; i< msg_size; i++) {
			fn_print_char(ndo, GET_U_1(p));
			p++;
		}
		break;
	}
}

/* PAP (see RFC 1334) */
static void
handle_pap(netdissect_options *ndo,
           const u_char *p, u_int length)
{
	u_int code, len;
	u_int peerid_len, passwd_len, msg_len;
	const u_char *p0;
	u_int i;

	p0 = p;
	if (length < 1) {
		ND_PRINT("[|pap]");
		return;
	} else if (length < 4) {
		ND_PRINT("[|pap 0x%02x]", GET_U_1(p));
		return;
	}

	code = GET_U_1(p);
	ND_PRINT("PAP, %s (0x%02x)",
	          tok2str(papcode_values, "unknown", code),
	          code);
	p++;

	ND_PRINT(", id %u", GET_U_1(p));	/* ID */
	p++;

	len = GET_BE_U_2(p);
	p += 2;

	if (len > length) {
		ND_PRINT(", length %u > packet size", len);
		return;
	}
	length = len;
	if (length < (size_t)(p - p0)) {
		ND_PRINT(", length %u < PAP header length", length);
		return;
	}

	switch (code) {
	case PAP_AREQ:
		/* A valid Authenticate-Request is 6 or more octets long. */
		if (len < 6)
			goto trunc;
		if (length - (p - p0) < 1)
			return;
		peerid_len = GET_U_1(p);	/* Peer-ID Length */
		p++;
		if (length - (p - p0) < peerid_len)
			return;
		ND_PRINT(", Peer ");
		for (i = 0; i < peerid_len; i++) {
			fn_print_char(ndo, GET_U_1(p));
			p++;
		}

		if (length - (p - p0) < 1)
			return;
		passwd_len = GET_U_1(p);	/* Password Length */
		p++;
		if (length - (p - p0) < passwd_len)
			return;
		ND_PRINT(", Name ");
		for (i = 0; i < passwd_len; i++) {
			fn_print_char(ndo, GET_U_1(p));
			p++;
		}
		break;
	case PAP_AACK:
	case PAP_ANAK:
		/* Although some implementations ignore truncation at
		 * this point and at least one generates a truncated
		 * packet, RFC 1334 section 2.2.2 clearly states that
		 * both AACK and ANAK are at least 5 bytes long.
		 */
		if (len < 5)
			goto trunc;
		if (length - (p - p0) < 1)
			return;
		msg_len = GET_U_1(p);	/* Msg-Length */
		p++;
		if (length - (p - p0) < msg_len)
			return;
		ND_PRINT(", Msg ");
		for (i = 0; i< msg_len; i++) {
			fn_print_char(ndo, GET_U_1(p));
			p++;
		}
		break;
	}
	return;

trunc:
	ND_PRINT("[|pap]");
}

/* BAP */
static void
handle_bap(netdissect_options *ndo _U_,
           const u_char *p _U_, u_int length _U_)
{
	/* XXX: to be supported!! */
}


/* IPCP config options */
static u_int
print_ipcp_config_options(netdissect_options *ndo,
                          const u_char *p, u_int length)
{
	u_int opt, len;
        u_int compproto, ipcomp_subopttotallen, ipcomp_subopt, ipcomp_suboptlen;

	if (length < 2)
		return 0;
	ND_TCHECK_2(p);
	opt = GET_U_1(p);
	len = GET_U_1(p + 1);
	if (length < len)
		return 0;
	if (len < 2) {
		ND_PRINT("\n\t  %s Option (0x%02x), length %u (length bogus, should be >= 2)",
		       tok2str(ipcpopt_values,"unknown",opt),
		       opt,
		       len);
		return 0;
	}

	ND_PRINT("\n\t  %s Option (0x%02x), length %u",
	       tok2str(ipcpopt_values,"unknown",opt),
	       opt,
	       len);

	switch (opt) {
	case IPCPOPT_2ADDR:		/* deprecated */
		if (len != 10) {
			ND_PRINT(" (length bogus, should be = 10)");
			return len;
		}
		ND_PRINT(": src %s, dst %s",
		       GET_IPADDR_STRING(p + 2),
		       GET_IPADDR_STRING(p + 6));
		break;
	case IPCPOPT_IPCOMP:
		if (len < 4) {
			ND_PRINT(" (length bogus, should be >= 4)");
			return 0;
		}
		compproto = GET_BE_U_2(p + 2);

		ND_PRINT(": %s (0x%02x):",
		          tok2str(ipcpopt_compproto_values, "Unknown", compproto),
		          compproto);

		switch (compproto) {
                case PPP_VJC:
			/* XXX: VJ-Comp parameters should be decoded */
                        break;
                case IPCPOPT_IPCOMP_HDRCOMP:
                        if (len < IPCPOPT_IPCOMP_MINLEN) {
                                ND_PRINT(" (length bogus, should be >= %u)",
                                         IPCPOPT_IPCOMP_MINLEN);
                                return 0;
                        }

                        ND_TCHECK_LEN(p + 2, IPCPOPT_IPCOMP_MINLEN);
                        ND_PRINT("\n\t    TCP Space %u, non-TCP Space %u"
                               ", maxPeriod %u, maxTime %u, maxHdr %u",
                               GET_BE_U_2(p + 4),
                               GET_BE_U_2(p + 6),
                               GET_BE_U_2(p + 8),
                               GET_BE_U_2(p + 10),
                               GET_BE_U_2(p + 12));

                        /* suboptions present ? */
                        if (len > IPCPOPT_IPCOMP_MINLEN) {
                                ipcomp_subopttotallen = len - IPCPOPT_IPCOMP_MINLEN;
                                p += IPCPOPT_IPCOMP_MINLEN;

                                ND_PRINT("\n\t      Suboptions, length %u", ipcomp_subopttotallen);

                                while (ipcomp_subopttotallen >= 2) {
                                        ND_TCHECK_2(p);
                                        ipcomp_subopt = GET_U_1(p);
                                        ipcomp_suboptlen = GET_U_1(p + 1);

                                        /* sanity check */
                                        if (ipcomp_subopt == 0 ||
                                            ipcomp_suboptlen == 0 )
                                                break;

                                        /* XXX: just display the suboptions for now */
                                        ND_PRINT("\n\t\t%s Suboption #%u, length %u",
                                               tok2str(ipcpopt_compproto_subopt_values,
                                                       "Unknown",
                                                       ipcomp_subopt),
                                               ipcomp_subopt,
                                               ipcomp_suboptlen);
                                        if (ipcomp_subopttotallen < ipcomp_suboptlen) {
                                                ND_PRINT(" [remaining suboptions length %u < %u]",
                                                         ipcomp_subopttotallen, ipcomp_suboptlen);
                                                nd_print_invalid(ndo);
                                                break;
                                        }
                                        ipcomp_subopttotallen -= ipcomp_suboptlen;
                                        p += ipcomp_suboptlen;
                                }
                        }
                        break;
                default:
                        break;
		}
		break;

	case IPCPOPT_ADDR:     /* those options share the same format - fall through */
	case IPCPOPT_MOBILE4:
	case IPCPOPT_PRIDNS:
	case IPCPOPT_PRINBNS:
	case IPCPOPT_SECDNS:
	case IPCPOPT_SECNBNS:
		if (len != 6) {
			ND_PRINT(" (length bogus, should be = 6)");
			return 0;
		}
		ND_PRINT(": %s", GET_IPADDR_STRING(p + 2));
		break;
	default:
		/*
		 * Unknown option; dump it as raw bytes now if we're
		 * not going to do so below.
		 */
		if (ndo->ndo_vflag < 2)
			print_unknown_data(ndo, p + 2, "\n\t    ", len - 2);
		break;
	}
	if (ndo->ndo_vflag > 1 && ND_TTEST_LEN(p + 2, len - 2))
		print_unknown_data(ndo, p + 2, "\n\t    ", len - 2); /* exclude TLV header */
	return len;

trunc:
	ND_PRINT("[|ipcp]");
	return 0;
}

/* IP6CP config options */
static u_int
print_ip6cp_config_options(netdissect_options *ndo,
                           const u_char *p, u_int length)
{
	u_int opt, len;

	if (length < 2)
		return 0;
	ND_TCHECK_2(p);
	opt = GET_U_1(p);
	len = GET_U_1(p + 1);
	if (length < len)
		return 0;
	if (len < 2) {
		ND_PRINT("\n\t  %s Option (0x%02x), length %u (length bogus, should be >= 2)",
		       tok2str(ip6cpopt_values,"unknown",opt),
		       opt,
		       len);
		return 0;
	}

	ND_PRINT("\n\t  %s Option (0x%02x), length %u",
	       tok2str(ip6cpopt_values,"unknown",opt),
	       opt,
	       len);

	switch (opt) {
	case IP6CP_IFID:
		if (len != 10) {
			ND_PRINT(" (length bogus, should be = 10)");
			return len;
		}
		ND_TCHECK_8(p + 2);
		ND_PRINT(": %04x:%04x:%04x:%04x",
		       GET_BE_U_2(p + 2),
		       GET_BE_U_2(p + 4),
		       GET_BE_U_2(p + 6),
		       GET_BE_U_2(p + 8));
		break;
	default:
		/*
		 * Unknown option; dump it as raw bytes now if we're
		 * not going to do so below.
		 */
		if (ndo->ndo_vflag < 2)
			print_unknown_data(ndo, p + 2, "\n\t    ", len - 2);
		break;
	}
	if (ndo->ndo_vflag > 1)
		print_unknown_data(ndo, p + 2, "\n\t    ", len - 2); /* exclude TLV header */

	return len;

trunc:
	ND_PRINT("[|ip6cp]");
	return 0;
}


/* CCP config options */
static u_int
print_ccp_config_options(netdissect_options *ndo,
                         const u_char *p, u_int length)
{
	u_int opt, len;

	if (length < 2)
		return 0;
	ND_TCHECK_2(p);
	opt = GET_U_1(p);
	len = GET_U_1(p + 1);
	if (length < len)
		return 0;
	if (len < 2) {
		ND_PRINT("\n\t  %s Option (0x%02x), length %u (length bogus, should be >= 2)",
		          tok2str(ccpconfopts_values, "Unknown", opt),
		          opt,
		          len);
		return 0;
	}

	ND_PRINT("\n\t  %s Option (0x%02x), length %u",
	          tok2str(ccpconfopts_values, "Unknown", opt),
	          opt,
	          len);

	switch (opt) {
	case CCPOPT_BSDCOMP:
		if (len < 3) {
			ND_PRINT(" (length bogus, should be >= 3)");
			return len;
		}
		ND_PRINT(": Version: %u, Dictionary Bits: %u",
			GET_U_1(p + 2) >> 5,
			GET_U_1(p + 2) & 0x1f);
		break;
	case CCPOPT_MVRCA:
		if (len < 4) {
			ND_PRINT(" (length bogus, should be >= 4)");
			return len;
		}
		ND_PRINT(": Features: %u, PxP: %s, History: %u, #CTX-ID: %u",
				(GET_U_1(p + 2) & 0xc0) >> 6,
				(GET_U_1(p + 2) & 0x20) ? "Enabled" : "Disabled",
				GET_U_1(p + 2) & 0x1f,
				GET_U_1(p + 3));
		break;
	case CCPOPT_DEFLATE:
		if (len < 4) {
			ND_PRINT(" (length bogus, should be >= 4)");
			return len;
		}
		ND_PRINT(": Window: %uK, Method: %s (0x%x), MBZ: %u, CHK: %u",
			(GET_U_1(p + 2) & 0xf0) >> 4,
			((GET_U_1(p + 2) & 0x0f) == 8) ? "zlib" : "unknown",
			GET_U_1(p + 2) & 0x0f,
			(GET_U_1(p + 3) & 0xfc) >> 2,
			GET_U_1(p + 3) & 0x03);
		break;

/* XXX: to be supported */
#if 0
	case CCPOPT_OUI:
	case CCPOPT_PRED1:
	case CCPOPT_PRED2:
	case CCPOPT_PJUMP:
	case CCPOPT_HPPPC:
	case CCPOPT_STACLZS:
	case CCPOPT_MPPC:
	case CCPOPT_GFZA:
	case CCPOPT_V42BIS:
	case CCPOPT_LZSDCP:
	case CCPOPT_DEC:
	case CCPOPT_RESV:
		break;
#endif
	default:
		/*
		 * Unknown option; dump it as raw bytes now if we're
		 * not going to do so below.
		 */
		if (ndo->ndo_vflag < 2)
			print_unknown_data(ndo, p + 2, "\n\t    ", len - 2);
		break;
	}
	if (ndo->ndo_vflag > 1)
		print_unknown_data(ndo, p + 2, "\n\t    ", len - 2); /* exclude TLV header */

	return len;

trunc:
	ND_PRINT("[|ccp]");
	return 0;
}

/* BACP config options */
static u_int
print_bacp_config_options(netdissect_options *ndo,
                          const u_char *p, u_int length)
{
	u_int opt, len;

	if (length < 2)
		return 0;
	ND_TCHECK_2(p);
	opt = GET_U_1(p);
	len = GET_U_1(p + 1);
	if (length < len)
		return 0;
	if (len < 2) {
		ND_PRINT("\n\t  %s Option (0x%02x), length %u (length bogus, should be >= 2)",
		          tok2str(bacconfopts_values, "Unknown", opt),
		          opt,
		          len);
		return 0;
	}

	ND_PRINT("\n\t  %s Option (0x%02x), length %u",
	          tok2str(bacconfopts_values, "Unknown", opt),
	          opt,
	          len);

	switch (opt) {
	case BACPOPT_FPEER:
		if (len != 6) {
			ND_PRINT(" (length bogus, should be = 6)");
			return len;
		}
		ND_PRINT(": Magic-Num 0x%08x", GET_BE_U_4(p + 2));
		break;
	default:
		/*
		 * Unknown option; dump it as raw bytes now if we're
		 * not going to do so below.
		 */
		if (ndo->ndo_vflag < 2)
			print_unknown_data(ndo, p + 2, "\n\t    ", len - 2);
		break;
	}
	if (ndo->ndo_vflag > 1)
		print_unknown_data(ndo, p + 2, "\n\t    ", len - 2); /* exclude TLV header */

	return len;

trunc:
	ND_PRINT("[|bacp]");
	return 0;
}

/*
 * Un-escape RFC 1662 PPP in HDLC-like framing, with octet escapes.
 * The length argument is the on-the-wire length, not the captured
 * length; we can only un-escape the captured part.
 */
static void
ppp_hdlc(netdissect_options *ndo,
         const u_char *p, u_int length)
{
	u_int caplen = ND_BYTES_AVAILABLE_AFTER(p);
	u_char *b, *t, c;
	const u_char *s;
	u_int i, proto;

	if (caplen == 0)
		return;

        if (length == 0)
                return;

	b = (u_char *)malloc(caplen);
	if (b == NULL) {
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
			"%s: malloc", __func__);
	}

	/*
	 * Unescape all the data into a temporary, private, buffer.
	 * Do this so that we don't overwrite the original packet
	 * contents.
	 */
	for (s = p, t = b, i = caplen; i != 0; i--) {
		c = GET_U_1(s);
		s++;
		if (c == 0x7d) {
			if (i <= 1)
				break;
			i--;
			c = GET_U_1(s) ^ 0x20;
			s++;
		}
		*t++ = c;
	}

	/*
	 * Switch to the output buffer for dissection, and save it
	 * on the buffer stack so it can be freed; our caller must
	 * pop it when done.
	 */
	if (!nd_push_buffer(ndo, b, b, (u_int)(t - b))) {
		free(b);
		(*ndo->ndo_error)(ndo, S_ERR_ND_MEM_ALLOC,
			"%s: can't push buffer on buffer stack", __func__);
	}
	length = ND_BYTES_AVAILABLE_AFTER(b);

        /* now lets guess about the payload codepoint format */
        if (length < 1)
                goto trunc;
        proto = GET_U_1(b); /* start with a one-octet codepoint guess */

        switch (proto) {
        case PPP_IP:
		ip_print(ndo, b + 1, length - 1);
		goto cleanup;
        case PPP_IPV6:
		ip6_print(ndo, b + 1, length - 1);
		goto cleanup;
        default: /* no luck - try next guess */
		break;
        }

        if (length < 2)
                goto trunc;
        proto = GET_BE_U_2(b); /* next guess - load two octets */

        switch (proto) {
        case (PPP_ADDRESS << 8 | PPP_CONTROL): /* looks like a PPP frame */
            if (length < 4)
                goto trunc;
            proto = GET_BE_U_2(b + 2); /* load the PPP proto-id */
            if ((proto & 0xff00) == 0x7e00)
                ND_PRINT("(protocol 0x%04x invalid)", proto);
            else
                handle_ppp(ndo, proto, b + 4, length - 4);
            break;
        default: /* last guess - proto must be a PPP proto-id */
            if ((proto & 0xff00) == 0x7e00)
                ND_PRINT("(protocol 0x%04x invalid)", proto);
            else
                handle_ppp(ndo, proto, b + 2, length - 2);
            break;
        }

cleanup:
	nd_pop_packet_info(ndo);
        return;

trunc:
	nd_pop_packet_info(ndo);
	nd_print_trunc(ndo);
}


/* PPP */
static void
handle_ppp(netdissect_options *ndo,
           u_int proto, const u_char *p, u_int length)
{
	if ((proto & 0xff00) == 0x7e00) { /* is this an escape code ? */
		ppp_hdlc(ndo, p - 1, length);
		return;
	}

	switch (proto) {
	case PPP_LCP: /* fall through */
	case PPP_IPCP:
	case PPP_OSICP:
	case PPP_MPLSCP:
	case PPP_IPV6CP:
	case PPP_CCP:
	case PPP_BACP:
		handle_ctrl_proto(ndo, proto, p, length);
		break;
	case PPP_ML:
		handle_mlppp(ndo, p, length);
		break;
	case PPP_CHAP:
		handle_chap(ndo, p, length);
		break;
	case PPP_PAP:
		handle_pap(ndo, p, length);
		break;
	case PPP_BAP:		/* XXX: not yet completed */
		handle_bap(ndo, p, length);
		break;
	case ETHERTYPE_IP:	/*XXX*/
        case PPP_VJNC:
	case PPP_IP:
		ip_print(ndo, p, length);
		break;
	case ETHERTYPE_IPV6:	/*XXX*/
	case PPP_IPV6:
		ip6_print(ndo, p, length);
		break;
	case ETHERTYPE_IPX:	/*XXX*/
	case PPP_IPX:
		ipx_print(ndo, p, length);
		break;
	case PPP_OSI:
		isoclns_print(ndo, p, length);
		break;
	case PPP_MPLS_UCAST:
	case PPP_MPLS_MCAST:
		mpls_print(ndo, p, length);
		break;
	case PPP_COMP:
		ND_PRINT("compressed PPP data");
		break;
	default:
		ND_PRINT("%s ", tok2str(ppptype2str, "unknown PPP protocol (0x%04x)", proto));
		print_unknown_data(ndo, p, "\n\t", length);
		break;
	}
}

/* Standard PPP printer */
u_int
ppp_print(netdissect_options *ndo,
          const u_char *p, u_int length)
{
	u_int proto,ppp_header;
        u_int olen = length; /* _o_riginal length */
	u_int hdr_len = 0;

	ndo->ndo_protocol = "ppp";
	/*
	 * Here, we assume that p points to the Address and Control
	 * field (if they present).
	 */
	if (length < 2)
		goto trunc;
        ppp_header = GET_BE_U_2(p);

        switch(ppp_header) {
        case (PPP_PPPD_IN  << 8 | PPP_CONTROL):
            if (ndo->ndo_eflag) ND_PRINT("In  ");
            p += 2;
            length -= 2;
            hdr_len += 2;
            break;
        case (PPP_PPPD_OUT << 8 | PPP_CONTROL):
            if (ndo->ndo_eflag) ND_PRINT("Out ");
            p += 2;
            length -= 2;
            hdr_len += 2;
            break;
        case (PPP_ADDRESS << 8 | PPP_CONTROL):
            p += 2;			/* ACFC not used */
            length -= 2;
            hdr_len += 2;
            break;

        default:
            break;
        }

	if (length < 2)
		goto trunc;
	if (GET_U_1(p) % 2) {
		proto = GET_U_1(p);	/* PFC is used */
		p++;
		length--;
		hdr_len++;
	} else {
		proto = GET_BE_U_2(p);
		p += 2;
		length -= 2;
		hdr_len += 2;
	}

	if (ndo->ndo_eflag) {
		const char *typestr;
		typestr = tok2str(ppptype2str, "unknown", proto);
		ND_PRINT("%s (0x%04x), length %u",
		          typestr,
		          proto,
		          olen);
		if (*typestr == 'u')	/* "unknown" */
			return hdr_len;

		ND_PRINT(": ");
	}

	handle_ppp(ndo, proto, p, length);
	return (hdr_len);
trunc:
	nd_print_trunc(ndo);
	return (0);
}


/* PPP I/F printer */
void
ppp_if_print(netdissect_options *ndo,
             const struct pcap_pkthdr *h, const u_char *p)
{
	u_int length = h->len;
	u_int caplen = h->caplen;

	ndo->ndo_protocol = "ppp";
	if (caplen < PPP_HDRLEN) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}
	ndo->ndo_ll_hdr_len += PPP_HDRLEN;

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
	if (ndo->ndo_eflag)
		ND_PRINT("%c %4d %02x ", GET_U_1(p) ? 'O' : 'I',
			 length, GET_U_1(p + 1));
#endif

	ppp_print(ndo, p, length);
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
ppp_hdlc_if_print(netdissect_options *ndo,
                  const struct pcap_pkthdr *h, const u_char *p)
{
	u_int length = h->len;
	u_int caplen = h->caplen;
	u_int proto;
	u_int hdrlen = 0;

	ndo->ndo_protocol = "ppp_hdlc";
	if (caplen < 2) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}

	switch (GET_U_1(p)) {

	case PPP_ADDRESS:
		if (caplen < 4) {
			nd_print_trunc(ndo);
			ndo->ndo_ll_hdr_len += caplen;
			return;
		}

		if (ndo->ndo_eflag)
			ND_PRINT("%02x %02x %u ", GET_U_1(p),
				 GET_U_1(p + 1), length);
		p += 2;
		length -= 2;
		hdrlen += 2;

		proto = GET_BE_U_2(p);
		p += 2;
		length -= 2;
		hdrlen += 2;
		ND_PRINT("%s: ", tok2str(ppptype2str, "unknown PPP protocol (0x%04x)", proto));

		handle_ppp(ndo, proto, p, length);
		break;

	case CHDLC_UNICAST:
	case CHDLC_BCAST:
		chdlc_if_print(ndo, h, p);
		return;

	default:
		if (caplen < 4) {
			nd_print_trunc(ndo);
			ndo->ndo_ll_hdr_len += caplen;
			return;
		}

		if (ndo->ndo_eflag)
			ND_PRINT("%02x %02x %u ", GET_U_1(p),
				 GET_U_1(p + 1), length);
		p += 2;
		hdrlen += 2;

		/*
		 * XXX - NetBSD's "ppp_netbsd_serial_if_print()" treats
		 * the next two octets as an Ethernet type; does that
		 * ever happen?
		 */
		ND_PRINT("unknown addr %02x; ctrl %02x", GET_U_1(p),
			 GET_U_1(p + 1));
		break;
	}

	ndo->ndo_ll_hdr_len += hdrlen;
}

#define PPP_BSDI_HDRLEN 24

/* BSD/OS specific PPP printer */
void
ppp_bsdos_if_print(netdissect_options *ndo,
                   const struct pcap_pkthdr *h _U_, const u_char *p _U_)
{
	u_int hdrlength;
#ifdef __bsdi__
	u_int length = h->len;
	u_int caplen = h->caplen;
	uint16_t ptype;
	uint8_t llhl;
	const u_char *q;
	u_int i;

	ndo->ndo_protocol = "ppp_bsdos";
	if (caplen < PPP_BSDI_HDRLEN) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}

	hdrlength = 0;

#if 0
	if (GET_U_1(p) == PPP_ADDRESS &&
	    GET_U_1(p + 1) == PPP_CONTROL) {
		if (ndo->ndo_eflag)
			ND_PRINT("%02x %02x ", GET_U_1(p),
				 GET_U_1(p + 1));
		p += 2;
		hdrlength = 2;
	}

	if (ndo->ndo_eflag)
		ND_PRINT("%u ", length);
	/* Retrieve the protocol type */
	if (GET_U_1(p) & 01) {
		/* Compressed protocol field */
		ptype = GET_U_1(p);
		if (ndo->ndo_eflag)
			ND_PRINT("%02x ", ptype);
		p++;
		hdrlength += 1;
	} else {
		/* Un-compressed protocol field */
		ptype = GET_BE_U_2(p);
		if (ndo->ndo_eflag)
			ND_PRINT("%04x ", ptype);
		p += 2;
		hdrlength += 2;
	}
#else
	ptype = 0;	/*XXX*/
	if (ndo->ndo_eflag)
		ND_PRINT("%c ", GET_U_1(p + SLC_DIR) ? 'O' : 'I');
	llhl = GET_U_1(p + SLC_LLHL);
	if (llhl) {
		/* link level header */
		struct ppp_header *ph;

		q = p + SLC_BPFHDRLEN;
		ph = (struct ppp_header *)q;
		if (ph->phdr_addr == PPP_ADDRESS
		 && ph->phdr_ctl == PPP_CONTROL) {
			if (ndo->ndo_eflag)
				ND_PRINT("%02x %02x ", GET_U_1(q),
					 GET_U_1(q + 1));
			ptype = GET_BE_U_2(&ph->phdr_type);
			if (ndo->ndo_eflag && (ptype == PPP_VJC || ptype == PPP_VJNC)) {
				ND_PRINT("%s ", tok2str(ppptype2str,
						"proto-#%u", ptype));
			}
		} else {
			if (ndo->ndo_eflag) {
				ND_PRINT("LLH=[");
				for (i = 0; i < llhl; i++)
					ND_PRINT("%02x", GET_U_1(q + i));
				ND_PRINT("] ");
			}
		}
	}
	if (ndo->ndo_eflag)
		ND_PRINT("%u ", length);
	if (GET_U_1(p + SLC_CHL)) {
		q = p + SLC_BPFHDRLEN + llhl;

		switch (ptype) {
		case PPP_VJC:
			ptype = vjc_print(ndo, q, ptype);
			hdrlength = PPP_BSDI_HDRLEN;
			p += hdrlength;
			switch (ptype) {
			case PPP_IP:
				ip_print(ndo, p, length);
				break;
			case PPP_IPV6:
				ip6_print(ndo, p, length);
				break;
			case PPP_MPLS_UCAST:
			case PPP_MPLS_MCAST:
				mpls_print(ndo, p, length);
				break;
			}
			goto printx;
		case PPP_VJNC:
			ptype = vjc_print(ndo, q, ptype);
			hdrlength = PPP_BSDI_HDRLEN;
			p += hdrlength;
			switch (ptype) {
			case PPP_IP:
				ip_print(ndo, p, length);
				break;
			case PPP_IPV6:
				ip6_print(ndo, p, length);
				break;
			case PPP_MPLS_UCAST:
			case PPP_MPLS_MCAST:
				mpls_print(ndo, p, length);
				break;
			}
			goto printx;
		default:
			if (ndo->ndo_eflag) {
				ND_PRINT("CH=[");
				for (i = 0; i < llhl; i++)
					ND_PRINT("%02x",
					    GET_U_1(q + i));
				ND_PRINT("] ");
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
	case PPP_IPV6:
		ip6_print(ndo, p, length);
		break;
	case PPP_MPLS_UCAST:
	case PPP_MPLS_MCAST:
		mpls_print(ndo, p, length);
		break;
	default:
		ND_PRINT("%s ", tok2str(ppptype2str, "unknown PPP protocol (0x%04x)", ptype));
	}

printx:
#else /* __bsdi */
	hdrlength = 0;
#endif /* __bsdi__ */
	ndo->ndo_ll_hdr_len += hdrlength;
}
