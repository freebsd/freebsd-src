/*
 * Copyright (c) 1990 The Regents of the University of California.
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
static  char rcsid[] =
	"@(#)$Header: print-fddi.c,v 1.4 92/02/03 16:04:02 van Exp $ (LBL)";
#endif

#ifdef FDDI
#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <net/bpf.h>

#include "interface.h"
#include "addrtoname.h"

/*
 * NOTE:  This is a very preliminary hack for FDDI support.
 * There are all sorts of wired in constants & nothing (yet)
 * to print SMT packets as anything other than hex dumps.
 * Most of the necessary changes are waiting on my redoing
 * the "header" that a kernel fddi driver supplies to bpf:  I
 * want it to look like one byte of 'direction' (0 or 1
 * depending on whether the packet was inbound or outbound),
 * two bytes of system/driver dependent data (anything an
 * implementor thinks would be useful to filter on and/or
 * save per-packet, then the real 21-byte FDDI header.
 * Steve McCanne & I have also talked about adding the
 * 'direction' byte to all bpf headers (e.g., in the two
 * bytes of padding on an ethernet header).  It's not clear
 * we could do this in a backwards compatible way & we hate
 * the idea of an incompatible bpf change.  Discussions are
 * proceeding.
 *
 * Also, to really support FDDI (and better support 802.2
 * over ethernet) we really need to re-think the rather simple
 * minded assumptions about fixed length & fixed format link
 * level headers made in gencode.c.  One day...
 *
 *  - vj
 */

/* XXX This goes somewhere else. */
#define FDDI_HDRLEN 21

static u_char fddi_bit_swap[] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

static inline void
fddi_print(p, length)
	u_char *p;
	int length;
{
	u_char fsrc[6], fdst[6];
	register char *srcname, *dstname;
	register int i;

	/*
	 * bit-swap the fddi addresses (isn't the IEEE standards
	 * process wonderful!) then convert them to names. 
	 */
	
	for (i = 0; i < sizeof(fdst); ++i)
		fdst[i] = fddi_bit_swap[p[i+1]];
	for (i = 0; i < sizeof(fsrc); ++i)
		fsrc[i] = fddi_bit_swap[p[i+7]];
	dstname = etheraddr_string(fdst);
	srcname = etheraddr_string(fsrc);

	if (vflag)
		printf("%s %s %02x %02x %02x %02x %02x%02x%02x %s %d: ",
		       dstname, srcname,
		       p[0],
		       p[13], p[14], p[15],
		       p[16], p[17], p[18],
		       etherproto_string((p[19] << 8) | p[20]),
		       length);
	else if (qflag)
		printf("%s %s %d: ", dstname, srcname, length);
	else
		printf("%s %s %02x %s %d: ",
		       dstname, srcname,
		       p[0],
		       etherproto_string((p[19] << 8) | p[20]),
		       length);
}

void
fddi_if_print(p, tvp, length, caplen)
	u_char *p;
	struct timeval *tvp;
	int length;
	int caplen;
{
	struct ip *ip;
	u_short type;

	ts_print(tvp);

	if (caplen < FDDI_HDRLEN) {
		printf("[|fddi]");
		goto out;
	}

	/*
	 * Some printers want to get back at the link level addresses,
	 * and/or check that they're not walking off the end of the packet.
	 * Rather than pass them all the way down, we set these globals.
	 */
	packetp = (u_char *)p;
	snapend = (u_char *)p + caplen;

	/*
	 * If the frame is not an LLC frame or is not an LLC/UI frame
	 * or doesn't have SNAP as a dest NSAP, use the default printer.
	 * (XXX - should interpret SMT packets here.)
	 */
	if ((p[0] & 0xf8) != 0x50)
		/* not LLC frame -- use default printer */
		type = 0;
	else if ((p[15] &~ 0x10) != 0x03)
		/* not UI frame -- use default printer */
		type = 0;
	else if (p[13] != 170)
		/* DSAP not SNAP -- use default printer */
		type = 0;
	else
		type = (p[19] << 8) | p[20];
	if (eflag)
		fddi_print(p, length);

	length -= FDDI_HDRLEN;
	p += FDDI_HDRLEN;

	switch (ntohs(type)) {

	case ETHERTYPE_IP:
		ip_print((struct ip *)p, length);
		break;

	case ETHERTYPE_ARP:
	case ETHERTYPE_REVARP:
		arp_print((struct ether_arp *)p, length, caplen - FDDI_HDRLEN);
		break;

	default:
		if (!eflag)
			fddi_print(p, length);
		if (!xflag && !qflag)
			default_print((u_short *)p, caplen - FDDI_HDRLEN);
		break;
	}
	if (xflag)
		default_print((u_short *)p, caplen - sizeof(FDDI_HDRLEN));
out:
	putchar('\n');
}
#else
#include <stdio.h>
void
fddi_if_print()
{
	void error();

	error("not configured for fddi");
	/* NOTREACHED */
}
#endif
