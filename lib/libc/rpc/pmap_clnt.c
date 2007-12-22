/*	$NetBSD: pmap_clnt.c,v 1.16 2000/07/06 03:10:34 christos Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid2 = "@(#)pmap_clnt.c 1.37 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)pmap_clnt.c	2.2 88/08/01 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * pmap_clnt.c
 * Client interface to pmap rpc service.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <rpc/nettype.h>
#include <netinet/in.h>
#include "un-namespace.h"

#include <stdio.h>
#include <stdlib.h>

#include "rpc_com.h"

bool_t
pmap_set(u_long program, u_long version, int protocol, int port)
{
	bool_t rslt;
	struct netbuf *na;
	struct netconfig *nconf;
	char buf[32];

	if ((protocol != IPPROTO_UDP) && (protocol != IPPROTO_TCP)) {
		return (FALSE);
	}
	nconf = __rpc_getconfip(protocol == IPPROTO_UDP ? "udp" : "tcp");
	if (nconf == NULL) {
		return (FALSE);
	}
	snprintf(buf, sizeof buf, "0.0.0.0.%d.%d", 
	    (((u_int32_t)port) >> 8) & 0xff, port & 0xff);
	na = uaddr2taddr(nconf, buf);
	if (na == NULL) {
		freenetconfigent(nconf);
		return (FALSE);
	}
	rslt = rpcb_set((rpcprog_t)program, (rpcvers_t)version, nconf, na);
	free(na);
	freenetconfigent(nconf);
	return (rslt);
}

/*
 * Remove the mapping between program, version and port.
 * Calls the pmap service remotely to do the un-mapping.
 */
bool_t
pmap_unset(u_long program, u_long version)
{
	struct netconfig *nconf;
	bool_t udp_rslt = FALSE;
	bool_t tcp_rslt = FALSE;

	nconf = __rpc_getconfip("udp");
	if (nconf != NULL) {
		udp_rslt = rpcb_unset((rpcprog_t)program, (rpcvers_t)version,
		    nconf);
		freenetconfigent(nconf);
	}
	nconf = __rpc_getconfip("tcp");
	if (nconf != NULL) {
		tcp_rslt = rpcb_unset((rpcprog_t)program, (rpcvers_t)version,
		    nconf);
		freenetconfigent(nconf);
	}
	/*
	 * XXX: The call may still succeed even if only one of the
	 * calls succeeded.  This was the best that could be
	 * done for backward compatibility.
	 */
	return (tcp_rslt || udp_rslt);
}
