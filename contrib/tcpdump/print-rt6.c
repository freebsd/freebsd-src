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
    "@(#) $Header: /tcpdump/master/tcpdump/print-rt6.c,v 1.3.2.1 2000/01/11 06:58:26 fenner Exp $";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef INET6

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>

#include <stdio.h>

#include <netinet/ip6.h>

#include "interface.h"
#include "addrtoname.h"

int
rt6_print(register const u_char *bp, register const u_char *bp2)
{
	register const struct ip6_rthdr *dp;
	register const struct ip6_rthdr0 *dp0;
	register const struct ip6_hdr *ip;
	register const u_char *ep;
	int i, len;

	dp = (struct ip6_rthdr *)bp;
	ip = (struct ip6_hdr *)bp2;
	len = dp->ip6r_len;

	/* 'ep' points to the end of avaible data. */
	ep = snapend;

        printf("%s > %s: ",
	       ip6addr_string(&ip->ip6_src),
	       ip6addr_string(&ip->ip6_dst));

	TCHECK(dp->ip6r_segleft);

	printf("srcrt (len=%d, ", dp->ip6r_len);
	printf("type=%d, ", dp->ip6r_type);
	printf("segleft=%d, ", dp->ip6r_segleft);

	switch(dp->ip6r_type) {
	case IPV6_RTHDR_TYPE_0:
		dp0 = (struct ip6_rthdr0 *)dp;

		TCHECK(dp0->ip6r0_reserved);
		if (dp0->ip6r0_reserved || vflag) {
			printf("rsv=0x%0x, ",
				(u_int32_t)ntohl(dp0->ip6r0_reserved));
		}

		if (len % 2 == 1)
			goto trunc;
		len >>= 1;
		for (i = 0; i < len; i++) {
			struct in6_addr *addr;

			addr = ((struct in6_addr *)(dp0 + 1)) + i;
			if ((u_char *)addr > ep - sizeof(*addr))
				goto trunc;

			printf("[%d]%s", i, ip6addr_string((u_char *)addr));
			if (i != len - 1)
				printf(", ");
		   
		}
		printf(")");
		return((dp0->ip6r0_len + 1) << 3);
		break;
	default:
		goto trunc;
		break;
	}

 trunc:
	fputs("[|srcrt]", stdout);
	return 65535;		/* XXX */
}
#endif /* INET6 */
