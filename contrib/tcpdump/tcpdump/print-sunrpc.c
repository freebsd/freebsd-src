/*
 * Copyright (c) 1992 The Regents of the University of California.
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
    "@(#) $Header: /a/cvs/386BSD/src/contrib/tcpdump/tcpdump/print-sunrpc.c,v 1.2 1993/09/15 20:27:24 jtc Exp $ (LBL)";
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <sys/time.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>

#include <ctype.h>

#include "interface.h"

#include "addrtoname.h"
#include "extract.h"

#if BYTE_ORDER == LITTLE_ENDIAN
/*
 * Byte swap an array of n words.
 * Assume input is word-aligned.
 * Check that buffer is bounded by "snapend".
 */
static void
bswap(bp, n)
	register u_long *bp;
	register u_int n;
{
	register int nwords = ((char *)snapend - (char *)bp) / sizeof(*bp);

	if (nwords > n)
		nwords = n;
	for (; --nwords >= 0; ++bp)
		*bp = ntohl(*bp);
}
#endif

void
sunrpcrequest_print(rp, length, ip)
	register struct rpc_msg *rp;
	int length;
	register struct ip *ip;
{
	register u_long *dp;
	register u_char *ep = snapend;
#define TCHECK(p, l) if ((u_char *)(p) > ep - l) break

#if BYTE_ORDER == LITTLE_ENDIAN
	bswap((u_long *)rp, sizeof(*rp) / sizeof(u_long));
#endif

	if (!nflag)
		(void)printf("%s.%x > %s.sunrpc: %d",
			     ipaddr_string(&ip->ip_src),
			     rp->rm_xid,
			     ipaddr_string(&ip->ip_dst),
			     length);
	else
		(void)printf("%s.%x > %s.%x: %d",
			     ipaddr_string(&ip->ip_src),
			     rp->rm_xid,
			     ipaddr_string(&ip->ip_dst),
			     PMAPPORT,
			     length);

	switch (rp->rm_call.cb_proc) {

	case PMAPPROC_NULL:
		printf(" null");
		break;

	case PMAPPROC_SET:
		printf(" set");
		break;

	case PMAPPROC_UNSET:
		printf(" unset");
		break;

	case PMAPPROC_GETPORT:
		printf(" getport");
		break;

	case PMAPPROC_DUMP:
		printf(" dump");
		break;

	case PMAPPROC_CALLIT:
		printf(" callit");
		break;

	default:
		printf(" proc #%d", rp->rm_call.cb_proc);
	}
	printf(" prog #%d", rp->rm_call.cb_prog);
	putchar('\n');
}

