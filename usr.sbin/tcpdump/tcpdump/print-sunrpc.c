/*
 * Copyright (c) 1992, 1993, 1994
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
    "@(#) $Header: /pub/FreeBSD/FreeBSD-CVS/src/usr.sbin/tcpdump/tcpdump/print-sunrpc.c,v 1.3 1995/03/08 12:52:43 olah Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#ifdef SOLARIS
#include <tiuser.h>
#endif
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/svc.h>
#include <rpc/rpc_msg.h>
#include <rpc/pmap_prot.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

#if BYTE_ORDER == LITTLE_ENDIAN
static void bswap(u_int32 *, u_int);
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
/*
 * Byte swap an array of n 32-bit words.
 * Assume input is word-aligned.
 * Check that buffer is bounded by "snapend".
 */
static void
bswap(register u_int32 *bp, register u_int n)
{
	register int nwords = ((char *)snapend - (char *)bp) / sizeof(*bp);

	if (nwords > n)
		nwords = n;
	for (; --nwords >= 0; ++bp)
		*bp = ntohl(*bp);
}
#endif

static struct token proc2str[] = {
	{ PMAPPROC_NULL,	"null" },
	{ PMAPPROC_SET,		"set" },
	{ PMAPPROC_UNSET,	"unset" },
	{ PMAPPROC_GETPORT,	"getport" },
	{ PMAPPROC_DUMP,	"dump" },
	{ PMAPPROC_CALLIT,	"call" },
	{ 0,			NULL }
};

void
sunrpcrequest_print(register const u_char *bp, register int length,
		    register const u_char *bp2)
{
	register const struct rpc_msg *rp;
	register const struct ip *ip;

	rp = (struct rpc_msg *)bp;
	ip = (struct ip *)bp2;

	if (!nflag)
		(void)printf("%s.%x > %s.sunrpc: %d",
			     ipaddr_string(&ip->ip_src),
			     ntohl(rp->rm_xid),
			     ipaddr_string(&ip->ip_dst),
			     length);
	else
		(void)printf("%s.%x > %s.%x: %d",
			     ipaddr_string(&ip->ip_src),
			     ntohl(rp->rm_xid),
			     ipaddr_string(&ip->ip_dst),
			     PMAPPORT,
			     length);
	printf(" %s", tok2str(proc2str, " proc #%d",
			      ntohl(rp->rm_call.cb_proc)));
}

