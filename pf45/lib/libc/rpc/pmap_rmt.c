/*	$NetBSD: pmap_rmt.c,v 1.29 2000/07/06 03:10:34 christos Exp $	*/

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
static char *sccsid2 = "@(#)pmap_rmt.c 1.21 87/08/27 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)pmap_rmt.c	2.2 88/08/01 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * pmap_rmt.c
 * Client interface to pmap rpc service.
 * remote call and broadcast service
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include "namespace.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_rmt.h>
#include "un-namespace.h"

static const struct timeval timeout = { 3, 0 };

/*
 * pmapper remote-call-service interface.
 * This routine is used to call the pmapper remote call service
 * which will look up a service program in the port maps, and then
 * remotely call that routine with the given parameters.  This allows
 * programs to do a lookup and call in one step.
*/
enum clnt_stat
pmap_rmtcall(addr, prog, vers, proc, xdrargs, argsp, xdrres, resp, tout,
    port_ptr)
	struct sockaddr_in *addr;
	u_long prog, vers, proc;
	xdrproc_t xdrargs, xdrres;
	caddr_t argsp, resp;
	struct timeval tout;
	u_long *port_ptr;
{
	int sock = -1;
	CLIENT *client;
	struct rmtcallargs a;
	struct rmtcallres r;
	enum clnt_stat stat;

	assert(addr != NULL);
	assert(port_ptr != NULL);

	addr->sin_port = htons(PMAPPORT);
	client = clntudp_create(addr, PMAPPROG, PMAPVERS, timeout, &sock);
	if (client != NULL) {
		a.prog = prog;
		a.vers = vers;
		a.proc = proc;
		a.args_ptr = argsp;
		a.xdr_args = xdrargs;
		r.port_ptr = port_ptr;
		r.results_ptr = resp;
		r.xdr_results = xdrres;
		stat = CLNT_CALL(client, (rpcproc_t)PMAPPROC_CALLIT,
		    (xdrproc_t)xdr_rmtcall_args, &a, (xdrproc_t)xdr_rmtcallres,
		    &r, tout);
		CLNT_DESTROY(client);
	} else {
		stat = RPC_FAILED;
	}
	addr->sin_port = 0;
	return (stat);
}


/*
 * XDR remote call arguments
 * written for XDR_ENCODE direction only
 */
bool_t
xdr_rmtcall_args(xdrs, cap)
	XDR *xdrs;
	struct rmtcallargs *cap;
{
	u_int lenposition, argposition, position;

	assert(xdrs != NULL);
	assert(cap != NULL);

	if (xdr_u_long(xdrs, &(cap->prog)) &&
	    xdr_u_long(xdrs, &(cap->vers)) &&
	    xdr_u_long(xdrs, &(cap->proc))) {
		lenposition = XDR_GETPOS(xdrs);
		if (! xdr_u_long(xdrs, &(cap->arglen)))
		    return (FALSE);
		argposition = XDR_GETPOS(xdrs);
		if (! (*(cap->xdr_args))(xdrs, cap->args_ptr))
		    return (FALSE);
		position = XDR_GETPOS(xdrs);
		cap->arglen = (u_long)position - (u_long)argposition;
		XDR_SETPOS(xdrs, lenposition);
		if (! xdr_u_long(xdrs, &(cap->arglen)))
		    return (FALSE);
		XDR_SETPOS(xdrs, position);
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR remote call results
 * written for XDR_DECODE direction only
 */
bool_t
xdr_rmtcallres(xdrs, crp)
	XDR *xdrs;
	struct rmtcallres *crp;
{
	caddr_t port_ptr;

	assert(xdrs != NULL);
	assert(crp != NULL);

	port_ptr = (caddr_t)(void *)crp->port_ptr;
	if (xdr_reference(xdrs, &port_ptr, sizeof (u_long),
	    (xdrproc_t)xdr_u_long) && xdr_u_long(xdrs, &crp->resultslen)) {
		crp->port_ptr = (u_long *)(void *)port_ptr;
		return ((*(crp->xdr_results))(xdrs, crp->results_ptr));
	}
	return (FALSE);
}
