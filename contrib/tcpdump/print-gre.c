/*
 * Copyright (c) 1996
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Lawrence Berkeley Laboratory,
 * Berkeley, CA.  The name of the University may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Initial contribution from John Hawkinson <jhawk@bbnplanet.com>
 *
 * This module implements support for decoding GRE (Generic Routing
 * Encapsulation) tunnels; they're documented in RFC1701 and RFC1702.
 * This code only supports the IP encapsulation thereof.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: print-gre.c,v 1.4 96/12/10 23:28:23 leres Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <netdb.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"		/* must come after interface.h */

#define GRE_SIZE (20)

struct gre {
	u_short flags;
	u_short proto;
	union {
		struct gre_ckof {
			u_short cksum;
			u_short offset;
		}        gre_ckof;
		u_int32_t key;
		u_int32_t seq;
	}     gre_void1;
	union {
		u_int32_t key;
		u_int32_t seq;
		u_int32_t routing;
	}     gre_void2;
	union {
		u_int32_t seq;
		u_int32_t routing;
	}     gre_void3;
	union {
		u_int32_t routing;
	}     gre_void4;
};

#define GRE_CP		0x8000	/* Checksum Present */
#define GRE_RP		0x4000	/* Routing Present */
#define GRE_KP		0x2000	/* Key Present */
#define GRE_SP		0x1000	/* Sequence Present */


#define GREPROTO_IP	0x0800


/*
 * Deencapsulate and print a GRE-tunneled IP datagram
 */
void
gre_print(const u_char *bp, u_int length)
{
	const u_char *cp = bp + 4;
	const struct gre *gre;
	u_short flags, proto;

	gre = (const struct gre *)bp;

	if (length < GRE_SIZE) {
		goto trunc;
	}
	flags = EXTRACT_16BITS(&gre->flags);
	proto = EXTRACT_16BITS(&gre->proto);

	if (vflag) {
		/* Decode the flags */
		putchar('[');
		if (flags & GRE_CP)
			putchar('C');
		if (flags & GRE_RP)
			putchar('R');
		if (flags & GRE_KP)
			putchar('K');
		if (flags & GRE_SP)
			putchar('S');
		fputs("] ", stdout);
	}
	/* Checksum & Offset are present */
	if ((flags & GRE_CP) | (flags & GRE_RP))
		cp += 4;

	/* We don't support routing fields (variable length) now. Punt. */
	if (flags & GRE_RP)
		return;

	if (flags & GRE_KP)
		cp += 4;
	if (flags & GRE_SP)
		cp += 4;

	switch (proto) {

	case GREPROTO_IP:
		ip_print(cp, length - ((cp - bp) / sizeof(u_char)));
		break;

	default:
		printf("gre-proto-0x%04X", proto);
		break;
	}
	return;

trunc:
	fputs("[|gre]", stdout);

}
