/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996
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
 * Hacked version of print-ether.c  Larry Lile <lile@stdio.com>
 *
 * $FreeBSD: src/contrib/tcpdump/print-token.c,v 1.3 2000/01/30 01:00:54 fenner Exp $
 */
#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/ncvs/src/contrib/tcpdump/print-token.c,v 1.1 1999/02/20 11:17:55 julian Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include "token.h"

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <stdio.h>
#include <pcap.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "llc.h"

const u_char *packetp;
const u_char *snapend;

static inline void
token_print(register const u_char *bp, u_int length)
{
	register const struct token_header *tp;
        register const struct llc *lp;
        u_short ether_type;

        tp = (const struct token_header *)bp;
        lp = (struct llc *)(bp + TOKEN_HDR_LEN);

        if (IS_SOURCE_ROUTED) {
            tp->ether_shost[0] = tp->ether_shost[0] & 0x7f;
                lp = (struct llc *)(bp + TOKEN_HDR_LEN + RIF_LENGTH);
        }

        /* 
         * Ethertype on ethernet is a short, but ethertype in an llc-snap has
         * been defined as 2 u_chars.  This is a stupid little hack to fix
         * this for now but something better should be done using ntohs()
         * XXX
         */
         ether_type = ((u_short)lp->ethertype[1] << 16) | lp->ethertype[0];

	if (qflag)
		(void)printf("%s %s %d: ",
			     etheraddr_string(ESRC(tp)),
			     etheraddr_string(EDST(tp)),
			     length);
	else
		(void)printf("%s %s %s %d: ",
			     etheraddr_string(ESRC(tp)),
			     etheraddr_string(EDST(tp)),
			     etherproto_string(ether_type),
			     length);
}

/*
 * This is the top level routine of the printer.  'p' is the points
 * to the ether header of the packet, 'tvp' is the timestamp,
 * 'length' is the length of the packet off the wire, and 'caplen'
 * is the number of bytes actually captured.
 */
void
token_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	struct token_header *tp;
	u_short ether_type;
	u_short extracted_ethertype;
        u_int route_len = 0, seg;
        struct llc *lp;

        tp = (struct token_header *)p;

	ts_print(&h->ts);

	if (caplen < TOKEN_HDR_LEN) {
		printf("[|token-ring]");
		goto out;
	}

	/*
	 * Some printers want to get back at the ethernet addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	tp = (struct token_header *)p;

        /* Adjust for source routing information in the MAC header */
        if (IS_SOURCE_ROUTED) {

            if (eflag)
                token_print(p, length);

                route_len =  RIF_LENGTH;
            if (vflag) {
                if (vflag > 1) 
                    printf("ac %x fc %x ", tp->ac, tp->fc);
                ether_type = ntohs((int)lp->ethertype);
    
                printf("%s ", broadcast_indicator[BROADCAST]);
                printf("%s", direction[DIRECTION]);
     
                for (seg = 0; seg < SEGMENT_COUNT; seg++)
                    printf(" [%d:%d]", RING_NUMBER(seg), BRIDGE_NUMBER(seg));
            } else {
                printf("rt = %x", ntohs(tp->rcf));
 
                for (seg = 0; seg < SEGMENT_COUNT; seg++)
                    printf(":%x", ntohs(tp->rseg[seg]));
            } 
            printf(" (%s) ", largest_frame[LARGEST_FRAME]);
        } else {
            if (eflag)
                token_print(p, length);
        }

        /* Set pointer to llc header, adjusted for routing information */
        lp = (struct llc *)(p + TOKEN_HDR_LEN + route_len);

        packetp = p;
	snapend = p + caplen;

        /* Skip over token ring MAC header */
	length -= TOKEN_HDR_LEN + route_len;
	caplen -= TOKEN_HDR_LEN + route_len;
	p += TOKEN_HDR_LEN + route_len;

	extracted_ethertype = 0;
	/* Try to print the LLC-layer header & higher layers */
	if (llc_print(p, length, caplen, ESRC(tp), EDST(tp)) == 0) {
		/* ether_type not known, print raw packet */
		if (!eflag)
			token_print((u_char *)tp, length);
		if (extracted_ethertype) {
			printf("(LLC %s) ",
		       etherproto_string(htons(extracted_ethertype)));
		}
		if (!xflag && !qflag)
			default_print(p, caplen);
	}
	if (xflag)
		default_print(p, caplen);
 out:
	putchar('\n');
}
