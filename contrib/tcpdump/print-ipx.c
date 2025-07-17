/*
 * Copyright (c) 1994, 1995, 1996
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
 * Contributed by Brad Parker (brad@fcr.com).
 */

/* \summary: Novell IPX printer */

#include <config.h>

#include "netdissect-stdinc.h"

#include <stdio.h>

#define ND_LONGJMP_FROM_TCHECK
#include "netdissect.h"
#include "addrtoname.h"
#include "extract.h"

/* well-known sockets */
#define	IPX_SKT_NCP		0x0451
#define	IPX_SKT_SAP		0x0452
#define	IPX_SKT_RIP		0x0453
#define	IPX_SKT_NETBIOS		0x0455
#define	IPX_SKT_DIAGNOSTICS	0x0456
#define	IPX_SKT_NWLINK_DGM	0x0553	/* NWLink datagram, may contain SMB */
#define	IPX_SKT_EIGRP		0x85be	/* Cisco EIGRP over IPX */

/* IPX transport header */
struct ipxHdr {
    nd_uint16_t	cksum;		/* Checksum */
    nd_uint16_t	length;		/* Length, in bytes, including header */
    nd_uint8_t	tCtl;		/* Transport Control (i.e. hop count) */
    nd_uint8_t	pType;		/* Packet Type (i.e. level 2 protocol) */
    nd_uint32_t	dstNet;		/* destination net */
    nd_mac_addr	dstNode;	/* destination node */
    nd_uint16_t	dstSkt;		/* destination socket */
    nd_uint32_t	srcNet;		/* source net */
    nd_mac_addr	srcNode;	/* source node */
    nd_uint16_t	srcSkt;		/* source socket */
};

#define ipxSize	30

static const char *ipxaddr_string(netdissect_options *, uint32_t, const u_char *);
static void ipx_decode(netdissect_options *, const struct ipxHdr *, const u_char *, u_int);
static void ipx_sap_print(netdissect_options *, const u_char *, u_int);
static void ipx_rip_print(netdissect_options *, const u_char *, u_int);

/*
 * Print IPX datagram packets.
 */
void
ipx_print(netdissect_options *ndo, const u_char *p, u_int length)
{
	const struct ipxHdr *ipx = (const struct ipxHdr *)p;

	ndo->ndo_protocol = "ipx";
	if (!ndo->ndo_eflag)
		ND_PRINT("IPX ");

	ND_PRINT("%s.%04x > ",
		     ipxaddr_string(ndo, GET_BE_U_4(ipx->srcNet), ipx->srcNode),
		     GET_BE_U_2(ipx->srcSkt));

	ND_PRINT("%s.%04x: ",
		     ipxaddr_string(ndo, GET_BE_U_4(ipx->dstNet), ipx->dstNode),
		     GET_BE_U_2(ipx->dstSkt));

	/* take length from ipx header */
	length = GET_BE_U_2(ipx->length);

	if (length < ipxSize) {
		ND_PRINT("[length %u < %u]", length, ipxSize);
		nd_print_invalid(ndo);
		return;
	}
	ipx_decode(ndo, ipx, p + ipxSize, length - ipxSize);
}

static const char *
ipxaddr_string(netdissect_options *ndo, uint32_t net, const u_char *node)
{
    static char line[256];

    snprintf(line, sizeof(line), "%08x.%02x:%02x:%02x:%02x:%02x:%02x",
	    net, GET_U_1(node), GET_U_1(node + 1),
	    GET_U_1(node + 2), GET_U_1(node + 3),
	    GET_U_1(node + 4), GET_U_1(node + 5));

    return line;
}

static void
ipx_decode(netdissect_options *ndo, const struct ipxHdr *ipx, const u_char *datap, u_int length)
{
    u_short dstSkt;

    dstSkt = GET_BE_U_2(ipx->dstSkt);
    switch (dstSkt) {
      case IPX_SKT_NCP:
	ND_PRINT("ipx-ncp %u", length);
	break;
      case IPX_SKT_SAP:
	ipx_sap_print(ndo, datap, length);
	break;
      case IPX_SKT_RIP:
	ipx_rip_print(ndo, datap, length);
	break;
      case IPX_SKT_NETBIOS:
	ND_PRINT("ipx-netbios %u", length);
#ifdef ENABLE_SMB
	ipx_netbios_print(ndo, datap, length);
#endif
	break;
      case IPX_SKT_DIAGNOSTICS:
	ND_PRINT("ipx-diags %u", length);
	break;
      case IPX_SKT_NWLINK_DGM:
	ND_PRINT("ipx-nwlink-dgm %u", length);
#ifdef ENABLE_SMB
	ipx_netbios_print(ndo, datap, length);
#endif
	break;
      case IPX_SKT_EIGRP:
	eigrp_print(ndo, datap, length);
	break;
      default:
	ND_PRINT("ipx-#%x %u", dstSkt, length);
	break;
    }
}

static void
ipx_sap_print(netdissect_options *ndo, const u_char *ipx, u_int length)
{
    int command, i;

    command = GET_BE_U_2(ipx);
    ND_ICHECK_U(length, <, 2);
    ipx += 2;
    length -= 2;

    switch (command) {
      case 1:
      case 3:
	if (command == 1)
	    ND_PRINT("ipx-sap-req");
	else
	    ND_PRINT("ipx-sap-nearest-req");

	ND_PRINT(" %s", ipxsap_string(ndo, htons(GET_BE_U_2(ipx))));
	break;

      case 2:
      case 4:
	if (command == 2)
	    ND_PRINT("ipx-sap-resp");
	else
	    ND_PRINT("ipx-sap-nearest-resp");

	for (i = 0; i < 8 && length != 0; i++) {
	    ND_TCHECK_2(ipx);
	    if (length < 2)
		goto invalid;
	    ND_PRINT(" %s '", ipxsap_string(ndo, htons(GET_BE_U_2(ipx))));
	    ipx += 2;
	    length -= 2;
	    if (length < 48) {
		ND_PRINT("'");
		goto invalid;
	    }
	    nd_printjnp(ndo, ipx, 48);
	    ND_PRINT("'");
	    ipx += 48;
	    length -= 48;
	    /*
	     * 10 bytes of IPX address.
	     */
	    ND_TCHECK_LEN(ipx, 10);
	    if (length < 10)
		goto invalid;
	    ND_PRINT(" addr %s",
		ipxaddr_string(ndo, GET_BE_U_4(ipx), ipx + 4));
	    ipx += 10;
	    length -= 10;
	    /*
	     * 2 bytes of socket and 2 bytes of number of intermediate
	     * networks.
	     */
	    ND_TCHECK_4(ipx);
	    if (length < 4)
		goto invalid;
	    ipx += 4;
	    length -= 4;
	}
	break;
      default:
	ND_PRINT("ipx-sap-?%x", command);
	break;
    }
    return;

invalid:
    nd_print_invalid(ndo);
}

static void
ipx_rip_print(netdissect_options *ndo, const u_char *ipx, u_int length)
{
    int command, i;

    command = GET_BE_U_2(ipx);
    ND_ICHECK_U(length, <, 2);
    ipx += 2;
    length -= 2;

    switch (command) {
      case 1:
	ND_PRINT("ipx-rip-req");
	if (length != 0) {
	    if (length < 8)
		goto invalid;
	    ND_PRINT(" %08x/%u.%u", GET_BE_U_4(ipx),
			 GET_BE_U_2(ipx + 4), GET_BE_U_2(ipx + 6));
	}
	break;
      case 2:
	ND_PRINT("ipx-rip-resp");
	for (i = 0; i < 50 && length != 0; i++) {
	    if (length < 8)
		goto invalid;
	    ND_PRINT(" %08x/%u.%u", GET_BE_U_4(ipx),
			 GET_BE_U_2(ipx + 4), GET_BE_U_2(ipx + 6));

	    ipx += 8;
	    length -= 8;
	}
	break;
      default:
	ND_PRINT("ipx-rip-?%x", command);
	break;
    }
    return;

invalid:
    nd_print_invalid(ndo);
}
