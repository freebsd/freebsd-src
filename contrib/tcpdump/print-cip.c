/*
 * Marko Kiiskila carnil@cs.tut.fi 
 * 
 * Tampere University of Technology - Telecommunications Laboratory
 *
 * Permission to use, copy, modify and distribute this
 * software and its documentation is hereby granted,
 * provided that both the copyright notice and this
 * permission notice appear in all copies of the software,
 * derivative works or modified versions, and any portions
 * thereof, that both notices appear in supporting
 * documentation, and that the use of this software is
 * acknowledged in any publications resulting from using
 * the software.
 * 
 * TUT ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION AND DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS
 * SOFTWARE.
 * 
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-cip.c,v 1.11 2000/12/22 22:45:10 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>


#include <netinet/in.h>

#include <stdio.h>
#include <pcap.h>

#include "interface.h"
#include "addrtoname.h"
#include "ethertype.h"
#include "ether.h"

const u_char *packetp;
const u_char *snapend;

#define RFC1483LLC_LEN	8 

static unsigned char rfcllc[] = {
	0xaa,	/* DSAP: non-ISO */
	0xaa,	/* SSAP: non-ISO */
	0x03,	/* Ctrl: Unnumbered Information Command PDU */
	0x00,	/* OUI: EtherType */
	0x00,
	0x00 };

static inline void
cip_print(register const u_char *bp, int length)
{
	int i;

	if (memcmp(rfcllc, bp, sizeof(rfcllc))) {
		if (qflag) {
			for (i = 0;i < RFC1483LLC_LEN; i++)
			(void)printf("%2.2x ",bp[i]);
		} else {
			for (i = 0;i < RFC1483LLC_LEN - 2; i++)
				(void)printf("%2.2x ",bp[i]);
			etherproto_string(((u_short*)bp)[3]);
		} 
	} else {
		if (qflag)
			(void)printf("(null encapsulation)");
		else {
			(void)printf("(null encap)");
			etherproto_string(ETHERTYPE_IP);
		}
	}
}

/*
 * This is the top level routine of the printer.  'p' is the points
 * to the raw header of the packet, 'tvp' is the timestamp,
 * 'length' is the length of the packet off the wire, and 'caplen'
 * is the number of bytes actually captured.
 */
void
cip_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	int caplen = h->caplen;
	int length = h->len;
	u_short ether_type;
	u_short extracted_ethertype;
	u_short *bp;

	ts_print(&h->ts);

	if (memcmp(rfcllc, p, sizeof(rfcllc))==0 && caplen < RFC1483LLC_LEN) {
		printf("[|cip]");
		goto out;
	}

	if (eflag)
		cip_print(p, length);

	/*
	 * Some printers want to get back at the ethernet addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	if (memcmp(rfcllc, p, sizeof(rfcllc))==0) {
		length -= RFC1483LLC_LEN;
		caplen -= RFC1483LLC_LEN;
		bp = (u_short *)p;
		p += RFC1483LLC_LEN;
		ether_type = ntohs(bp[3]);
	} else {
		ether_type = ETHERTYPE_IP;
		bp = (u_short *)p;
	}

	/*
	 * Is it (gag) an 802.3 encapsulation?
	 */
	extracted_ethertype = 0;
	if (ether_type < ETHERMTU) {
		/* Try to print the LLC-layer header & higher layers */
		if (llc_print(p, length, caplen, NULL, NULL,
		    &extracted_ethertype)==0) {
			/* ether_type not known, print raw packet */
			if (!eflag)
				cip_print((u_char *)bp, length + RFC1483LLC_LEN);
			if (extracted_ethertype) {
				printf("(LLC %s) ",
			       etherproto_string(htons(extracted_ethertype)));
			}
			if (!xflag && !qflag)
				default_print(p, caplen);
		}
	} else if (ether_encap_print(ether_type, p, length, caplen,
	    &extracted_ethertype) == 0) {
		/* ether_type not known, print raw packet */
		if (!eflag)
			cip_print((u_char *)bp, length + RFC1483LLC_LEN);
		if (!xflag && !qflag)
			default_print(p, caplen);
	}
	if (xflag)
		default_print(p, caplen);
 out:
	putchar('\n');
}
