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
    "@(#) $Header: /tcpdump/master/tcpdump/print-lane.c,v 1.12 2001/07/05 18:54:15 guy Exp $ (LBL)";
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
#include "ether.h"
#include "lane.h"

static inline void
lane_print(register const u_char *bp, int length)
{
	register const struct lecdatahdr_8023 *ep;

	ep = (const struct lecdatahdr_8023 *)bp;
	if (qflag)
		(void)printf("lecid:%d %s %s %d: ",
			     ntohs(ep->le_header),
			     etheraddr_string(ep->h_source),
			     etheraddr_string(ep->h_dest),
			     length);
	else
		(void)printf("lecid:%d %s %s %s %d: ",
			     ntohs(ep->le_header),
			     etheraddr_string(ep->h_source),
			     etheraddr_string(ep->h_dest),
			     etherproto_string(ep->h_type),
			     length);
}

/*
 * This is the top level routine of the printer.  'p' is the points
 * to the ether header of the packet, 'h->tv' is the timestamp,
 * 'h->length' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
lane_if_print(u_char *user, const struct pcap_pkthdr *h, const u_char *p)
{
	int caplen = h->caplen;
	int length = h->len;
	struct lecdatahdr_8023 *ep;
	u_short ether_type;
	u_short extracted_ethertype;

	++infodelay;
	ts_print(&h->ts);

	if (caplen < sizeof(struct lecdatahdr_8023)) {
		printf("[|lane]");
		goto out;
	}

	if (eflag)
		lane_print(p, length);

	/*
	 * Some printers want to get back at the ethernet addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;

	length -= sizeof(struct lecdatahdr_8023);
	caplen -= sizeof(struct lecdatahdr_8023);
	ep = (struct lecdatahdr_8023 *)p;
	p += sizeof(struct lecdatahdr_8023);

	ether_type = ntohs(ep->h_type);

	/*
	 * Is it (gag) an 802.3 encapsulation?
	 */
	extracted_ethertype = 0;
	if (ether_type < ETHERMTU) {
		/* Try to print the LLC-layer header & higher layers */
		if (llc_print(p, length, caplen, ep->h_source, ep->h_dest,
		    &extracted_ethertype) == 0) {
			/* ether_type not known, print raw packet */
			if (!eflag)
				lane_print((u_char *)ep, length + sizeof(*ep));
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
			lane_print((u_char *)ep, length + sizeof(*ep));
		if (!xflag && !qflag)
			default_print(p, caplen);
	}
	if (xflag)
		default_print(p, caplen);
 out:
	putchar('\n');
	--infodelay;
	if (infoprint)
		info(0);
}
