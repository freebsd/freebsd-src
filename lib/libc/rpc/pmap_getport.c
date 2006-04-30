/*	$NetBSD: pmap_getport.c,v 1.16 2000/07/06 03:10:34 christos Exp $	*/

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
static char *sccsid2 = "from: @(#)pmap_getport.c 1.9 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "from: @(#)pmap_getport.c	2.2 88/08/01 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * pmap_getport.c
 * Client interface to pmap rpc service.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <net/if.h>

#include <assert.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include "un-namespace.h"

static const struct timeval timeout = { 5, 0 };
static const struct timeval tottimeout = { 60, 0 };

/*
 * Find the mapped port for program,version.
 * Calls the pmap service remotely to do the lookup.
 * Returns 0 if no map exists.
 */
u_short
pmap_getport(address, program, version, protocol)
	struct sockaddr_in *address;
	u_long program;
	u_long version;
	u_int protocol;
{
	u_short port = 0;
	int sock = -1;
	CLIENT *client;
	struct pmap parms;

	assert(address != NULL);

	address->sin_port = htons(PMAPPORT);
	client = clntudp_bufcreate(address, PMAPPROG,
	    PMAPVERS, timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client != NULL) {
		parms.pm_prog = program;
		parms.pm_vers = version;
		parms.pm_prot = protocol;
		parms.pm_port = 0;  /* not needed or used */
		if (CLNT_CALL(client, (rpcproc_t)PMAPPROC_GETPORT,
		    (xdrproc_t)xdr_pmap,
		    &parms, (xdrproc_t)xdr_u_short, &port, tottimeout) !=
		    RPC_SUCCESS){
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
		} else if (port == 0) {
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		}
		CLNT_DESTROY(client);
	}
	address->sin_port = 0;
	return (port);
}
