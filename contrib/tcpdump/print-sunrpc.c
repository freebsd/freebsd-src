/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
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
    "@(#) $Header: print-sunrpc.c,v 1.24 96/07/23 14:17:27 leres Exp $ (LBL)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <net/ethernet.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <rpc/rpc.h>
#ifdef HAVE_RPC_RPCENT_H
#include <rpc/rpcent.h>
#endif
#include <rpc/pmap_prot.h>

#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"

static struct tok proc2str[] = {
	{ PMAPPROC_NULL,	"null" },
	{ PMAPPROC_SET,		"set" },
	{ PMAPPROC_UNSET,	"unset" },
	{ PMAPPROC_GETPORT,	"getport" },
	{ PMAPPROC_DUMP,	"dump" },
	{ PMAPPROC_CALLIT,	"call" },
	{ 0,			NULL }
};

/* Forwards */
static char *progstr(u_int32_t);

void
sunrpcrequest_print(register const u_char *bp, register u_int length,
		    register const u_char *bp2)
{
	register const struct rpc_msg *rp;
	register const struct ip *ip;
	u_int32_t x;

	rp = (struct rpc_msg *)bp;
	ip = (struct ip *)bp2;

	if (!nflag)
		(void)printf("%s.%x > %s.sunrpc: %d",
			     ipaddr_string(&ip->ip_src),
			     (u_int32_t)ntohl(rp->rm_xid),
			     ipaddr_string(&ip->ip_dst),
			     length);
	else
		(void)printf("%s.%x > %s.%x: %d",
			     ipaddr_string(&ip->ip_src),
			     (u_int32_t)ntohl(rp->rm_xid),
			     ipaddr_string(&ip->ip_dst),
			     PMAPPORT,
			     length);
	printf(" %s", tok2str(proc2str, " proc #%u",
	    (u_int32_t)ntohl(rp->rm_call.cb_proc)));
	x = ntohl(rp->rm_call.cb_rpcvers);
	if (x != 2)
		printf(" [rpcver %u]", x);

	switch (ntohl(rp->rm_call.cb_proc)) {

	case PMAPPROC_SET:
	case PMAPPROC_UNSET:
	case PMAPPROC_GETPORT:
	case PMAPPROC_CALLIT:
		x = ntohl(rp->rm_call.cb_prog);
		if (!nflag)
			printf(" %s", progstr(x));
		else
			printf(" %u", x);
		printf(".%u", (u_int32_t)ntohl(rp->rm_call.cb_vers));
		break;
	}
}

static char *
progstr(prog)
	u_int32_t prog;
{
	register struct rpcent *rp;
	static char buf[32];
	static lastprog = 0;

	if (lastprog != 0 && prog == lastprog)
		return (buf);
	rp = getrpcbynumber(prog);
	if (rp == NULL)
		(void) sprintf(buf, "#%u", prog);
	else
		strcpy(buf, rp->r_name);
	return (buf);
}
