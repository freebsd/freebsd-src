/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1993, 1994
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

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/tcpdump/print-frag6.c,v 1.13 2001/09/17 21:58:02 fenner Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef INET6

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>


#include <netinet/in.h>

#include <stdio.h>

#include "ip6.h"

#include "interface.h"
#include "addrtoname.h"

int
frag6_print(register const u_char *bp, register const u_char *bp2)
{
	register const struct ip6_frag *dp;
	register const struct ip6_hdr *ip6;
	register const u_char *ep;

	dp = (const struct ip6_frag *)bp;
	ip6 = (const struct ip6_hdr *)bp2;

	/* 'ep' points to the end of available data. */
	ep = snapend;

	TCHECK(dp->ip6f_offlg);

	if (vflag) {
		printf("frag (0x%08x:%d|%ld)",
		       (u_int32_t)ntohl(dp->ip6f_ident),
		       ntohs(dp->ip6f_offlg & IP6F_OFF_MASK),
		       sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) -
			       (long)(bp - bp2) - sizeof(struct ip6_frag));
	} else {
		printf("frag (%d|%ld)",
		       ntohs(dp->ip6f_offlg & IP6F_OFF_MASK),
		       sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) -
			       (long)(bp - bp2) - sizeof(struct ip6_frag));
	}

#if 1
	/* it is meaningless to decode non-first fragment */
	if (ntohs(dp->ip6f_offlg & IP6F_OFF_MASK) != 0)
		return 65535;
	else
#endif
	{
		fputs(" ", stdout);
		return sizeof(struct ip6_frag);
	}
trunc:
	fputs("[|frag]", stdout);
	return 65535;
#undef TCHECK
}
#endif /* INET6 */
