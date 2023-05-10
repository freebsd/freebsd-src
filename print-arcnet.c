/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
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
 * From: NetBSD: print-arcnet.c,v 1.2 2000/04/24 13:02:28 itojun Exp
 */

/* \summary: Attached Resource Computer NETwork (ARCNET) printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"

/*
 * from: NetBSD: if_arc.h,v 1.13 1999/11/19 20:41:19 thorpej Exp
 */

/*
 * Structure of a 2.5MB/s Arcnet header on the BSDs,
 * as given to interface code.
 */
struct	arc_header {
	nd_uint8_t  arc_shost;
	nd_uint8_t  arc_dhost;
	nd_uint8_t  arc_type;
	/*
	 * only present for newstyle encoding with LL fragmentation.
	 * Don't use sizeof(anything), use ARC_HDR{,NEW}LEN instead.
	 */
	nd_uint8_t  arc_flag;
	nd_uint16_t arc_seqid;

	/*
	 * only present in exception packets (arc_flag == 0xff)
	 */
	nd_uint8_t  arc_type2;	/* same as arc_type */
	nd_uint8_t  arc_flag2;	/* real flag value */
	nd_uint16_t arc_seqid2;	/* real seqid value */
};

#define	ARC_HDRLEN		3
#define	ARC_HDRNEWLEN		6
#define	ARC_HDRNEWLEN_EXC	10

/* RFC 1051 */
#define	ARCTYPE_IP_OLD		240	/* IP protocol */
#define	ARCTYPE_ARP_OLD		241	/* address resolution protocol */

/* RFC 1201 */
#define	ARCTYPE_IP		212	/* IP protocol */
#define	ARCTYPE_ARP		213	/* address resolution protocol */
#define	ARCTYPE_REVARP		214	/* reverse addr resolution protocol */

#define	ARCTYPE_ATALK		221	/* Appletalk */
#define	ARCTYPE_BANIAN		247	/* Banyan Vines */
#define	ARCTYPE_IPX		250	/* Novell IPX */

#define ARCTYPE_INET6		0xc4	/* IPng */
#define ARCTYPE_DIAGNOSE	0x80	/* as per ANSI/ATA 878.1 */

/*
 * Structure of a 2.5MB/s Arcnet header on Linux.  Linux has
 * an extra "offset" field when given to interface code, and
 * never presents packets that look like exception frames.
 */
struct	arc_linux_header {
	nd_uint8_t  arc_shost;
	nd_uint8_t  arc_dhost;
	nd_uint16_t arc_offset;
	nd_uint8_t  arc_type;
	/*
	 * only present for newstyle encoding with LL fragmentation.
	 * Don't use sizeof(anything), use ARC_LINUX_HDR{,NEW}LEN
	 * instead.
	 */
	nd_uint8_t  arc_flag;
	nd_uint16_t arc_seqid;
};

#define	ARC_LINUX_HDRLEN	5
#define	ARC_LINUX_HDRNEWLEN	8

static int arcnet_encap_print(netdissect_options *, u_char arctype, const u_char *p,
    u_int length, u_int caplen);

static const struct tok arctypemap[] = {
	{ ARCTYPE_IP_OLD,	"oldip" },
	{ ARCTYPE_ARP_OLD,	"oldarp" },
	{ ARCTYPE_IP,		"ip" },
	{ ARCTYPE_ARP,		"arp" },
	{ ARCTYPE_REVARP,	"rarp" },
	{ ARCTYPE_ATALK,	"atalk" },
	{ ARCTYPE_BANIAN,	"banyan" },
	{ ARCTYPE_IPX,		"ipx" },
	{ ARCTYPE_INET6,	"ipv6" },
	{ ARCTYPE_DIAGNOSE,	"diag" },
	{ 0, NULL }
};

static void
arcnet_print(netdissect_options *ndo, const u_char *bp, u_int length, int phds,
             u_int flag, u_int seqid)
{
	const struct arc_header *ap;
	const char *arctypename;

	ndo->ndo_protocol = "arcnet";
	ap = (const struct arc_header *)bp;

	if (ndo->ndo_qflag) {
		ND_PRINT("%02x %02x %u: ",
			     GET_U_1(ap->arc_shost),
			     GET_U_1(ap->arc_dhost),
			     length);
		return;
	}

	arctypename = tok2str(arctypemap, "%02x", GET_U_1(ap->arc_type));

	if (!phds) {
		ND_PRINT("%02x %02x %s %u: ",
			     GET_U_1(ap->arc_shost),
			     GET_U_1(ap->arc_dhost),
			     arctypename,
			     length);
		return;
	}

	if (flag == 0) {
		ND_PRINT("%02x %02x %s seqid %04x %u: ",
			GET_U_1(ap->arc_shost),
			GET_U_1(ap->arc_dhost),
			arctypename, seqid,
			length);
		return;
	}

	if (flag & 1)
		ND_PRINT("%02x %02x %s seqid %04x "
			"(first of %u fragments) %u: ",
			GET_U_1(ap->arc_shost),
			GET_U_1(ap->arc_dhost),
			arctypename, seqid,
			(flag + 3) / 2, length);
	else
		ND_PRINT("%02x %02x %s seqid %04x "
			"(fragment %u) %u: ",
			GET_U_1(ap->arc_shost),
			GET_U_1(ap->arc_dhost),
			arctypename, seqid,
			flag/2 + 1, length);
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ARCNET header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
arcnet_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	const struct arc_header *ap;

	int phds;
	u_int flag = 0, archdrlen = 0;
	u_int seqid = 0;
	u_char arc_type;

	ndo->ndo_protocol = "arcnet";
	if (caplen < ARC_HDRLEN) {
		ndo->ndo_ll_hdr_len += caplen;
		nd_trunc_longjmp(ndo);
	}

	ap = (const struct arc_header *)p;
	arc_type = GET_U_1(ap->arc_type);

	switch (arc_type) {
	default:
		phds = 1;
		break;
	case ARCTYPE_IP_OLD:
	case ARCTYPE_ARP_OLD:
	case ARCTYPE_DIAGNOSE:
		phds = 0;
		archdrlen = ARC_HDRLEN;
		break;
	}

	if (phds) {
		if (caplen < ARC_HDRNEWLEN) {
			arcnet_print(ndo, p, length, 0, 0, 0);
			ND_PRINT(" phds");
			ndo->ndo_ll_hdr_len += caplen;
			nd_trunc_longjmp(ndo);
		}

		flag = GET_U_1(ap->arc_flag);
		if (flag == 0xff) {
			if (caplen < ARC_HDRNEWLEN_EXC) {
				arcnet_print(ndo, p, length, 0, 0, 0);
				ND_PRINT(" phds extended");
				ndo->ndo_ll_hdr_len += caplen;
				nd_trunc_longjmp(ndo);
			}
			flag = GET_U_1(ap->arc_flag2);
			seqid = GET_BE_U_2(ap->arc_seqid2);
			archdrlen = ARC_HDRNEWLEN_EXC;
		} else {
			seqid = GET_BE_U_2(ap->arc_seqid);
			archdrlen = ARC_HDRNEWLEN;
		}
	}


	if (ndo->ndo_eflag)
		arcnet_print(ndo, p, length, phds, flag, seqid);

	/*
	 * Go past the ARCNET header.
	 */
	length -= archdrlen;
	caplen -= archdrlen;
	p += archdrlen;

	if (phds && flag && (flag & 1) == 0) {
		/*
		 * This is a middle fragment.
		 */
		ndo->ndo_ll_hdr_len += archdrlen;
		return;
	}

	if (!arcnet_encap_print(ndo, arc_type, p, length, caplen))
		ND_DEFAULTPRINT(p, caplen);

	ndo->ndo_ll_hdr_len += archdrlen;
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the ARCNET header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.  It is quite similar
 * to the non-Linux style printer except that Linux doesn't ever
 * supply packets that look like exception frames, it always supplies
 * reassembled packets rather than raw frames, and headers have an
 * extra "offset" field between the src/dest and packet type.
 */
void
arcnet_linux_if_print(netdissect_options *ndo, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	const struct arc_linux_header *ap;

	int archdrlen = 0;
	u_char arc_type;

	ndo->ndo_protocol = "arcnet_linux";
	if (caplen < ARC_LINUX_HDRLEN) {
		ndo->ndo_ll_hdr_len += caplen;
		nd_trunc_longjmp(ndo);
	}

	ap = (const struct arc_linux_header *)p;
	arc_type = GET_U_1(ap->arc_type);

	switch (arc_type) {
	default:
		archdrlen = ARC_LINUX_HDRNEWLEN;
		if (caplen < ARC_LINUX_HDRNEWLEN) {
			ndo->ndo_ll_hdr_len += caplen;
			nd_trunc_longjmp(ndo);
		}
		break;
	case ARCTYPE_IP_OLD:
	case ARCTYPE_ARP_OLD:
	case ARCTYPE_DIAGNOSE:
		archdrlen = ARC_LINUX_HDRLEN;
		break;
	}

	if (ndo->ndo_eflag)
		arcnet_print(ndo, p, length, 0, 0, 0);

	/*
	 * Go past the ARCNET header.
	 */
	length -= archdrlen;
	caplen -= archdrlen;
	p += archdrlen;

	if (!arcnet_encap_print(ndo, arc_type, p, length, caplen))
		ND_DEFAULTPRINT(p, caplen);

	ndo->ndo_ll_hdr_len += archdrlen;
}

/*
 * Prints the packet encapsulated in an ARCnet data field,
 * given the ARCnet system code.
 *
 * Returns non-zero if it can do so, zero if the system code is unknown.
 */


static int
arcnet_encap_print(netdissect_options *ndo, u_char arctype, const u_char *p,
    u_int length, u_int caplen)
{
	switch (arctype) {

	case ARCTYPE_IP_OLD:
	case ARCTYPE_IP:
	        ip_print(ndo, p, length);
		return (1);

	case ARCTYPE_INET6:
		ip6_print(ndo, p, length);
		return (1);

	case ARCTYPE_ARP_OLD:
	case ARCTYPE_ARP:
	case ARCTYPE_REVARP:
		arp_print(ndo, p, length, caplen);
		return (1);

	case ARCTYPE_ATALK:	/* XXX was this ever used? */
		if (ndo->ndo_vflag)
			ND_PRINT("et1 ");
		atalk_print(ndo, p, length);
		return (1);

	case ARCTYPE_IPX:
		ipx_print(ndo, p, length);
		return (1);

	default:
		return (0);
	}
}
