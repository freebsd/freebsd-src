/*
 * Copyright (c) 1988-1990 The Regents of the University of California.
 * All rights reserved.
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
#ifndef lint
static char rcsid[] =
    "@(#) $Header: /home/ncvs/src/usr.sbin/tcpdump/tcpdump/print-ether.c,v 1.1.1.1 1993/06/12 14:42:09 rgrimes Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_llc.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include "interface.h"
#include "addrtoname.h"
#include "appletalk.h"

u_char *packetp;
u_char *snapend;

static inline void
ether_print(ep, length)
	register struct ether_header *ep;
	int length;
{
	if (qflag)
		(void)printf("%s %s %d: ",
			     etheraddr_string(ESRC(ep)),
			     etheraddr_string(EDST(ep)),
			     length);
	else
		(void)printf("%s %s %s %d: ",
			     etheraddr_string(ESRC(ep)),
			     etheraddr_string(EDST(ep)),
			     etherproto_string(ep->ether_type), 
			     length);
}

static inline void
eight02_print(ep, lp, length)
	register struct ether_header *ep;
	struct llc *lp;
	int length;
{
	if(eflag || length < LLC_UFRAMELEN) {
		(void)printf("%s %s %d: ",
			     etheraddr_string(ESRC(ep)),
			     etheraddr_string(EDST(ep)),
			     length);
	}

	if(length < LLC_UFRAMELEN) {
		printf("[|llc]");
		return;
	}

	if(eflag && lp->llc_dsap != LLC_SNAP_LSAP) {
		switch(lp->llc_control) {
		case LLC_UI:	printf("ui "); break;
		case LLC_UI_P:	printf("ui-p "); break;
		case LLC_DISC:	printf("disc "); break;
		case LLC_DISC_P: printf("disc-p "); break;
		case LLC_UA:	printf("ua "); break;
		case LLC_UA_P:	printf("ua-p "); break;
		case LLC_TEST:	printf("test "); break;
		case LLC_TEST_P: printf("test-p "); break;
		case LLC_FRMR:	printf("frmr "); break;
		case LLC_FRMR_P: printf("frmr-p "); break;
		case LLC_DM:	printf("dm "); break;
		case LLC_DM_P:	printf("dm-p "); break;
		case LLC_XID:	printf("xid "); break;
		case LLC_XID_P:	printf("xid-p "); break;
		case LLC_SABME:	printf("sabme "); break;
		case LLC_SABME_P: printf("sabme-p "); break;
		case LLC_RR:	printf("rr "); break;
		case LLC_RNR:	printf("rnr "); break;
		case LLC_REJ:	printf("rej "); break;
		case LLC_INFO:	printf("info "); break;

		default:
			printf("[control %d] ", lp->llc_control);
			break;
		}
	}
	switch(lp->llc_dsap) {
	case LLC_SNAP_LSAP:
		if(length < 6) {
			printf(" [|snap]");
			return;
		}

#define llc_snap_oui	llc_un.type_snap.org_code
#define llc_snap_type	llc_un.type_snap.ether_type
		if(lp->llc_snap_oui[0] == 0x08
		   && lp->llc_snap_oui[1] == 0x00
		   && lp->llc_snap_oui[2] == 0x07) {
			printf("[ethertalk] ");
			ddp_print((struct atDDP *)((char *)lp + 6),
				  length - 6);
		} else {
			if(!eflag) {
				(void)printf("%s %s %d: ",
					     etheraddr_string(ESRC(ep)),
					     etheraddr_string(EDST(ep)),
					     length);
			}
			printf("snap %02x-%02x-%02x type %04x ",
			       lp->llc_snap_oui[0], lp->llc_snap_oui[1],
			       lp->llc_snap_oui[2], lp->llc_snap_type);
		}
		break;

	default:
		if(!eflag) {
			(void)printf("%s %s %d: ",
				     etheraddr_string(ESRC(ep)),
				     etheraddr_string(EDST(ep)),
				     length);
			switch(lp->llc_control) {
			case LLC_UI:	printf("ui "); break;
			case LLC_UI_P:	printf("ui-p "); break;
			case LLC_DISC:	printf("disc "); break;
			case LLC_DISC_P: printf("disc-p "); break;
			case LLC_UA:	printf("ua "); break;
			case LLC_UA_P:	printf("ua-p "); break;
			case LLC_TEST:	printf("test "); break;
			case LLC_TEST_P: printf("test-p "); break;
			case LLC_FRMR:	printf("frmr "); break;
			case LLC_FRMR_P: printf("frmr-p "); break;
			case LLC_DM:	printf("dm "); break;
			case LLC_DM_P:	printf("dm-p "); break;
			case LLC_XID:	printf("xid "); break;
			case LLC_XID_P:	printf("xid-p "); break;
			case LLC_SABME:	printf("sabme "); break;
			case LLC_SABME_P: printf("sabme-p "); break;
			case LLC_RR:	printf("rr "); break;
			case LLC_RNR:	printf("rnr "); break;
			case LLC_REJ:	printf("rej "); break;
			case LLC_INFO:	printf("info "); break;

			default:
				printf("[control %d] ", lp->llc_control);
				break;
			}
		}

		printf("[dsap %d] [ssap %d] ",
		       lp->llc_dsap, lp->llc_ssap);
		break;
	}

	if (xflag)
		default_print((u_short *)lp, length);
}

/*
 * This is the top level routine of the printer.  'p' is the points
 * to the ether header of the packet, 'tvp' is the timestamp, 
 * 'length' is the length of the packet off the wire, and 'caplen'
 * is the number of bytes actually captured.
 */
void
ether_if_print(p, tvp, length, caplen)
	u_char *p;
	struct timeval *tvp;
	int length;
	int caplen;
{
	struct ether_header *ep;
	register int i;

	ts_print(tvp);

	if (caplen < sizeof(struct ether_header)) {
		printf("[|ether]");
		goto out;
	}

	if (eflag)
		ether_print((struct ether_header *)p, length);

	/*
	 * Some printers want to get back at the ethernet addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = p;
	snapend = p + caplen;
	
	length -= sizeof(struct ether_header);
	ep = (struct ether_header *)p;
	p += sizeof(struct ether_header);
	switch (ntohs(ep->ether_type)) {

	case ETHERTYPE_IP:
		ip_print((struct ip *)p, length);
		break;

	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		arp_print((struct ether_arp *)p, length, caplen - sizeof(*ep));
		break;

	default:
		if (ntohs(ep->ether_type) < 1500) {
			eight02_print(ep, (struct llc *)p,
				      caplen - sizeof(*ep));
			goto out;
		}
		if (!eflag)
			ether_print(ep, length);

		if (!xflag && !qflag)
			default_print((u_short *)p, caplen - sizeof(*ep));
		break;
	}
	if (xflag)
		default_print((u_short *)p, caplen - sizeof(*ep));
 out:
	putchar('\n');
}
