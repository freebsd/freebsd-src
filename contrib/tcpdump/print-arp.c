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
 * $FreeBSD$
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-arp.c,v 1.49 2000/10/10 05:05:07 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "ether.h"
#include "ethertype.h"
#include "extract.h"			/* must come after interface.h */

/*
 * Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  ARP packets are variable
 * in size; the arphdr structure defines the fixed-length portion.
 * Protocol type values are the same as those for 10 Mb/s Ethernet.
 * It is followed by the variable-sized fields ar_sha, arp_spa,
 * arp_tha and arp_tpa in that order, according to the lengths
 * specified.  Field names used correspond to RFC 826.
 */
struct	arphdr {
	u_short	ar_hrd;		/* format of hardware address */
#define ARPHRD_ETHER 	1	/* ethernet hardware format */
#define ARPHRD_IEEE802	6	/* token-ring hardware format */
#define ARPHRD_FRELAY 	15	/* frame relay hardware format */
	u_short	ar_pro;		/* format of protocol address */
	u_char	ar_hln;		/* length of hardware address */
	u_char	ar_pln;		/* length of protocol address */
	u_short	ar_op;		/* one of: */
#define	ARPOP_REQUEST	1	/* request to resolve address */
#define	ARPOP_REPLY	2	/* response to previous request */
#define	ARPOP_REVREQUEST 3	/* request protocol address given hardware */
#define	ARPOP_REVREPLY	4	/* response giving protocol address */
#define ARPOP_INVREQUEST 8 	/* request to identify peer */
#define ARPOP_INVREPLY	9	/* response identifying peer */
/*
 * The remaining fields are variable in size,
 * according to the sizes above.
 */
#ifdef COMMENT_ONLY
	u_char	ar_sha[];	/* sender hardware address */
	u_char	ar_spa[];	/* sender protocol address */
	u_char	ar_tha[];	/* target hardware address */
	u_char	ar_tpa[];	/* target protocol address */
#endif
};

#define ARP_HDRLEN	8

/*
 * Ethernet Address Resolution Protocol.
 *
 * See RFC 826 for protocol description.  Structure below is adapted
 * to resolving internet addresses.  Field names used correspond to 
 * RFC 826.
 */
struct	ether_arp {
	struct	arphdr ea_hdr;	/* fixed-size header */
	u_char	arp_sha[6];	/* sender hardware address */
	u_char	arp_spa[4];	/* sender protocol address */
	u_char	arp_tha[6];	/* target hardware address */
	u_char	arp_tpa[4];	/* target protocol address */
};
#define	arp_hrd	ea_hdr.ar_hrd
#define	arp_pro	ea_hdr.ar_pro
#define	arp_hln	ea_hdr.ar_hln
#define	arp_pln	ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op

#define ETHER_ARP_HDRLEN	(ARP_HDRLEN + 6 + 4 + 6 + 4)	

#define SHA(ap) ((ap)->arp_sha)
#define THA(ap) ((ap)->arp_tha)
#define SPA(ap) ((ap)->arp_spa)
#define TPA(ap) ((ap)->arp_tpa)

/* Compatibility */
#ifndef REVARP_REQUEST
#define REVARP_REQUEST		3
#endif
#ifndef REVARP_REPLY
#define REVARP_REPLY		4
#endif

static u_char ezero[6];

void
arp_print(register const u_char *bp, u_int length, u_int caplen)
{
	register const struct ether_arp *ap;
	register const struct ether_header *eh;
	register u_short pro, hrd, op;

	ap = (struct ether_arp *)bp;
	if ((u_char *)(ap + 1) > snapend) {
		printf("[|arp]");
		return;
	}
	if (length < ETHER_ARP_HDRLEN) {
		(void)printf("truncated-arp");
		default_print((u_char *)ap, length);
		return;
	}

	pro = EXTRACT_16BITS(&ap->arp_pro);
	hrd = EXTRACT_16BITS(&ap->arp_hrd);
	op = EXTRACT_16BITS(&ap->arp_op);

	if ((pro != ETHERTYPE_IP && pro != ETHERTYPE_TRAIL)
	    || ap->arp_hln != sizeof(SHA(ap))
	    || ap->arp_pln != sizeof(SPA(ap))) {
		(void)printf("arp-#%d for proto #%d (%d) hardware #%d (%d)",
				op, pro, ap->arp_pln,
				hrd, ap->arp_hln);
		return;
	}
	if (pro == ETHERTYPE_TRAIL)
		(void)printf("trailer-");
	eh = (struct ether_header *)packetp;
	switch (op) {

	case ARPOP_REQUEST:
		(void)printf("arp who-has %s", ipaddr_string(TPA(ap)));
		if (memcmp((char *)ezero, (char *)THA(ap), 6) != 0)
			(void)printf(" (%s)", etheraddr_string(THA(ap)));
		(void)printf(" tell %s", ipaddr_string(SPA(ap)));
		if (memcmp((char *)ESRC(eh), (char *)SHA(ap), 6) != 0)
			(void)printf(" (%s)", etheraddr_string(SHA(ap)));
		break;

	case ARPOP_REPLY:
		(void)printf("arp reply %s", ipaddr_string(SPA(ap)));
		if (memcmp((char *)ESRC(eh), (char *)SHA(ap), 6) != 0)
			(void)printf(" (%s)", etheraddr_string(SHA(ap)));
		(void)printf(" is-at %s", etheraddr_string(SHA(ap)));
		if (memcmp((char *)EDST(eh), (char *)THA(ap), 6) != 0)
			(void)printf(" (%s)", etheraddr_string(THA(ap)));
		break;

	case REVARP_REQUEST:
		(void)printf("rarp who-is %s tell %s",
			etheraddr_string(THA(ap)),
			etheraddr_string(SHA(ap)));
		break;

	case REVARP_REPLY:
		(void)printf("rarp reply %s at %s",
			etheraddr_string(THA(ap)),
			ipaddr_string(TPA(ap)));
		break;

	default:
		(void)printf("arp-#%d", op);
		default_print((u_char *)ap, caplen);
		return;
	}
	if (hrd != ARPHRD_ETHER)
		printf(" hardware #%d", hrd);
}
