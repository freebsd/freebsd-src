/*
 * Copyright (c) 1989, 1990, 1991, 1993, 1994, 1996
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
static char rcsid[] =
    "@(#) $Header: print-rip.c,v 1.34 96/07/23 14:17:26 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

struct rip {
	u_char rip_cmd;			/* request/response */
	u_char rip_vers;		/* protocol version # */
	u_short rip_zero2;		/* unused */
};
#define	RIPCMD_REQUEST		1	/* want info */
#define	RIPCMD_RESPONSE		2	/* responding to request */
#define	RIPCMD_TRACEON		3	/* turn tracing on */
#define	RIPCMD_TRACEOFF		4	/* turn it off */
#define	RIPCMD_POLL		5	/* want info from everybody */
#define	RIPCMD_POLLENTRY	6	/* poll for entry */

struct rip_netinfo {
	u_short rip_family;
	u_short rip_tag;
	u_int32_t rip_dest;
	u_int32_t rip_dest_mask;
	u_int32_t rip_router;
	u_int32_t rip_metric;		/* cost of route */
};

static void
rip_entry_print(register int vers, register const struct rip_netinfo *ni)
{
	register u_char *cp, *ep;

	if (EXTRACT_16BITS(&ni->rip_family) != AF_INET) {

		printf(" [family %d:", EXTRACT_16BITS(&ni->rip_family));
		cp = (u_char *)&ni->rip_tag;
		ep = (u_char *)&ni->rip_metric + sizeof(ni->rip_metric);
		for (; cp < ep; cp += 2)
			printf(" %04x", EXTRACT_16BITS(cp));
		printf("]");
	} else if (vers < 2) {
		/* RFC 1058 */
		printf(" %s", ipaddr_string(&ni->rip_dest));
	} else {
		/* RFC 1723 */
		printf(" {%s", ipaddr_string(&ni->rip_dest));
		if (ni->rip_dest_mask)
			printf("/%s", ipaddr_string(&ni->rip_dest_mask));
		if (ni->rip_router)
			printf("->%s", ipaddr_string(&ni->rip_router));
		if (ni->rip_tag)
			printf(" tag %04x", EXTRACT_16BITS(&ni->rip_tag));
		printf("}");
	}
	printf("(%d)", EXTRACT_32BITS(&ni->rip_metric));
}

void
rip_print(const u_char *dat, u_int length)
{
	register const struct rip *rp;
	register const struct rip_netinfo *ni;
	register int i, j, trunc;

	i = min(length, snapend - dat) - (sizeof(*rp) - sizeof(*ni));
	if (i < 0)
		return;

	rp = (struct rip *)dat;
	switch (rp->rip_cmd) {

	case RIPCMD_REQUEST:
		printf(" rip-req %d", length);
		break;

	case RIPCMD_RESPONSE:
		j = length / sizeof(*ni);
		if (j * sizeof(*ni) != length - 4)
			printf(" rip-resp %d[%d]:", j, length);
		else
			printf(" rip-resp %d:", j);
		trunc = ((i / sizeof(*ni)) * sizeof(*ni) != i);
		ni = (struct rip_netinfo *)(rp + 1);
		for (; (i -= sizeof(*ni)) >= 0; ++ni)
			rip_entry_print(rp->rip_vers, ni);
		if (trunc)
			printf("[|rip]");
		break;

	case RIPCMD_TRACEON:
		printf(" rip-traceon %d: \"", length);
		(void)fn_print((const u_char *)(rp + 1), snapend);
		fputs("\"\n", stdout);
		break;

	case RIPCMD_TRACEOFF:
		printf(" rip-traceoff %d", length);
		break;

	case RIPCMD_POLL:
		printf(" rip-poll %d", length);
		break;

	case RIPCMD_POLLENTRY:
		printf(" rip-pollentry %d", length);
		break;

	default:
		printf(" rip-#%d %d", rp->rip_cmd, length);
		break;
	}
	switch (rp->rip_vers) {

	case 1:
	case 2:
		break;

	default:
		printf(" [vers %d]", rp->rip_vers);
		break;
        }
}
