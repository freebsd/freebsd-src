/*
 * Copyright (c) 1994
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
 */

/*
 * Format and print Novell IPX packets.
 * Contributed by Brad Parker (brad@fcr.com).
 */
#ifndef lint
static  char rcsid[] =
    "@(#)$Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-ipx.c,v 1.1 1995/03/08 12:52:34 olah Exp $";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#ifdef __STDC__
#include <stdlib.h>
#endif
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "ipx.h"
#include "extract.h"


static const char *ipxaddr_string(u_int32 net, const u_char *node);
void ipx_decode(const struct ipxHdr *ipx, const u_char *datap, int length);
void ipx_sap_print(const u_short *ipx, int length);
void ipx_rip_print(const u_short *ipx, int length);

/*
 * Print IPX datagram packets.
 */
void
ipx_print(const u_char *p, int length)
{
	const struct ipxHdr *ipx = (const struct ipxHdr *)p;

	if (length < ipxSize) {
		(void)printf(" truncated-ipx %d", length);
		return;
	}
	(void)printf("%s.%x > ",
		     ipxaddr_string(EXTRACT_LONG(ipx->srcNet), ipx->srcNode),
		     EXTRACT_SHORT(&ipx->srcSkt));

	(void)printf("%s.%x:",
		     ipxaddr_string(EXTRACT_LONG(ipx->dstNet), ipx->dstNode),
		     EXTRACT_SHORT(&ipx->dstSkt));

	if ((u_char *)(ipx + 1) > snapend) {
		printf(" [|ipx]");
		return;
	}

	/* take length from ipx header */
	length = EXTRACT_SHORT(&ipx->length);

	ipx_decode(ipx, (u_char *)ipx + ipxSize, length - ipxSize);
}

static const char *
ipxaddr_string(u_int32 net, const u_char *node)
{
    static char line[256];

    sprintf(line, "%lu.%02x:%02x:%02x:%02x:%02x:%02x",
	    net, node[0], node[1], node[2], node[3], node[4], node[5]);

    return line;
}

void
ipx_decode(const struct ipxHdr *ipx, const u_char *datap, int length)
{
    switch (EXTRACT_SHORT(&ipx->dstSkt)) {
      case IPX_SKT_NCP:
	(void)printf(" ipx-ncp %d", length);
	break;
      case IPX_SKT_SAP:
	ipx_sap_print((u_short *)datap, length);
	break;
      case IPX_SKT_RIP:
	ipx_rip_print((u_short *)datap, length);
	break;
      case IPX_SKT_NETBIOS:
	(void)printf(" ipx-netbios %d", length);
	break;
      case IPX_SKT_DIAGNOSTICS:
	(void)printf(" ipx-diags %d", length);
	break;
      default:
	(void)printf(" ipx-#%x %d", ipx->dstSkt, length);
	break;
    }
}

void
ipx_sap_print(const u_short *ipx, int length)
{
    int command, i;

    if (length < 2) {
	(void)printf(" truncated-sap %d", length);
	return;
    }

    command = EXTRACT_SHORT(ipx);
    ipx++;
    length -= 2;

    switch (command) {
      case 1:
      case 3:
	if (command == 1)
	    (void)printf("ipx-sap-req");
	else
	    (void)printf("ipx-sap-nearest-req");

	if (length > 0)
	    (void)printf(" %x '%.48s'", EXTRACT_SHORT(&ipx[0]),
			 (char*)&ipx[1]);
	break;

      case 2:
      case 4:
	if (command == 2)
	    (void)printf("ipx-sap-resp");
	else
	    (void)printf("ipx-sap-nearest-resp");

	for (i = 0; i < 8 && length > 0; i++) {
	    (void)printf(" %x '%.48s' addr %s",
			 EXTRACT_SHORT(&ipx[0]), (char *)&ipx[1],
			 ipxaddr_string(EXTRACT_LONG(&ipx[25]),
					(u_char *)&ipx[27]));
	    ipx += 32;
	    length -= 64;
	}
	break;
      default:
	    (void)printf("ipx-sap-?%x", command);
	break;
    }
}

void
ipx_rip_print(const u_short *ipx, int length)
{
    int command, i;

    if (length < 2) {
	(void)printf(" truncated-ipx %d", length);
	return;
    }

    command = EXTRACT_SHORT(ipx);
    ipx++;
    length -= 2;

    switch (command) {
      case 1:
	(void)printf("ipx-rip-req");
	if (length > 0)
	    (void)printf(" %lu/%d.%d", EXTRACT_LONG(&ipx[0]),
			 EXTRACT_SHORT(&ipx[2]), EXTRACT_SHORT(&ipx[3]));
	break;
      case 2:
	(void)printf("ipx-rip-resp");
	for (i = 0; i < 50 && length > 0; i++) {
	    (void)printf(" %lu/%d.%d", EXTRACT_LONG(&ipx[0]),
			 EXTRACT_SHORT(&ipx[2]), EXTRACT_SHORT(&ipx[3]));

	    ipx += 4;
	    length -= 8;
	}
	break;
      default:
	    (void)printf("ipx-rip-?%x", command);
    }
}

