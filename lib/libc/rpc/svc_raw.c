/*	$NetBSD: svc_raw.c,v 1.14 2000/07/06 03:10:35 christos Exp $	*/

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
/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

/* #ident	"@(#)svc_raw.c	1.16	94/04/24 SMI" */

#if 0
#if defined(SCCSIDS) && !defined(lint)
static char sccsid[] = "@(#)svc_raw.c 1.25 89/01/31 Copyr 1984 Sun Micro";
static char *rcsid = "$FreeBSD$";
#endif
#endif

/*
 * svc_raw.c,   This a toy for simple testing and timing.
 * Interface to create an rpc client and server in the same UNIX process.
 * This lets us similate rpc and get rpc (round trip) overhead, without
 * any interference from the kernel.
 *
 */

#include "namespace.h"
#include "reentrant.h"
#include <rpc/rpc.h>
#include <sys/types.h>
#include <rpc/raw.h>
#include <stdlib.h>
#include "un-namespace.h"

#ifndef UDPMSGSIZE
#define	UDPMSGSIZE 8800
#endif

/*
 * This is the "network" that we will be moving data over
 */
static struct svc_raw_private {
	char	*raw_buf;	/* should be shared with the cl handle */
	SVCXPRT	server;
	XDR	xdr_stream;
	char	verf_body[MAX_AUTH_BYTES];
} *svc_raw_private;

extern mutex_t	svcraw_lock;

static enum xprt_stat svc_raw_stat(SVCXPRT *);
static bool_t svc_raw_recv(SVCXPRT *, struct rpc_msg *);
static bool_t svc_raw_reply(SVCXPRT *, struct rpc_msg *);
static bool_t svc_raw_getargs(SVCXPRT *, xdrproc_t, caddr_t);
static bool_t svc_raw_freeargs(SVCXPRT *, xdrproc_t, caddr_t);
static void svc_raw_destroy(SVCXPRT *);
static void svc_raw_ops(SVCXPRT *);
static bool_t svc_raw_control(SVCXPRT *, const u_int, void *);

char *__rpc_rawcombuf = NULL;

SVCXPRT *
svc_raw_create()
{
	struct svc_raw_private *srp;
/* VARIABLES PROTECTED BY svcraw_lock: svc_raw_private, srp */

	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		srp = (struct svc_raw_private *)calloc(1, sizeof (*srp));
		if (srp == NULL) {
			mutex_unlock(&svcraw_lock);
			return (NULL);
		}
		if (__rpc_rawcombuf == NULL)
			__rpc_rawcombuf = calloc(UDPMSGSIZE, sizeof (char));
		srp->raw_buf = __rpc_rawcombuf; /* Share it with the client */
		svc_raw_private = srp;
	}
	srp->server.xp_fd = FD_SETSIZE;
	srp->server.xp_port = 0;
	srp->server.xp_p3 = NULL;
	svc_raw_ops(&srp->server);
	srp->server.xp_verf.oa_base = srp->verf_body;
	xdrmem_create(&srp->xdr_stream, srp->raw_buf, UDPMSGSIZE, XDR_DECODE);
	xprt_register(&srp->server);
	mutex_unlock(&svcraw_lock);
	return (&srp->server);
}

/*ARGSUSED*/
static enum xprt_stat
svc_raw_stat(xprt)
SVCXPRT *xprt; /* args needed to satisfy ANSI-C typechecking */
{
	return (XPRT_IDLE);
}

/*ARGSUSED*/
static bool_t
svc_raw_recv(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct svc_raw_private *srp;
	XDR *xdrs;

	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);

	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_DECODE;
	(void) XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg)) {
		return (FALSE);
	}
	return (TRUE);
}

/*ARGSUSED*/
static bool_t
svc_raw_reply(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct svc_raw_private *srp;
	XDR *xdrs;

	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);

	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_ENCODE;
	(void) XDR_SETPOS(xdrs, 0);
	if (! xdr_replymsg(xdrs, msg)) {
		return (FALSE);
	}
	(void) XDR_GETPOS(xdrs);  /* called just for overhead */
	return (TRUE);
}

/*ARGSUSED*/
static bool_t
svc_raw_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	struct svc_raw_private *srp;

	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);
	return (*xdr_args)(&srp->xdr_stream, args_ptr);
}

/*ARGSUSED*/
static bool_t
svc_raw_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	struct svc_raw_private *srp;
	XDR *xdrs;

	mutex_lock(&svcraw_lock);
	srp = svc_raw_private;
	if (srp == NULL) {
		mutex_unlock(&svcraw_lock);
		return (FALSE);
	}
	mutex_unlock(&svcraw_lock);

	xdrs = &srp->xdr_stream;
	xdrs->x_op = XDR_FREE;
	return (*xdr_args)(xdrs, args_ptr);
}

/*ARGSUSED*/
static void
svc_raw_destroy(xprt)
SVCXPRT *xprt;
{
}

/*ARGSUSED*/
static bool_t
svc_raw_control(xprt, rq, in)
	SVCXPRT *xprt;
	const u_int	rq;
	void		*in;
{
	return (FALSE);
}

static void
svc_raw_ops(xprt)
	SVCXPRT *xprt;
{
	static struct xp_ops ops;
	static struct xp_ops2 ops2;
	extern mutex_t ops_lock;

/* VARIABLES PROTECTED BY ops_lock: ops */

	mutex_lock(&ops_lock);
	if (ops.xp_recv == NULL) {
		ops.xp_recv = svc_raw_recv;
		ops.xp_stat = svc_raw_stat;
		ops.xp_getargs = svc_raw_getargs;
		ops.xp_reply = svc_raw_reply;
		ops.xp_freeargs = svc_raw_freeargs;
		ops.xp_destroy = svc_raw_destroy;
		ops2.xp_control = svc_raw_control;
	}
	xprt->xp_ops = &ops;
	xprt->xp_ops2 = &ops2;
	mutex_unlock(&ops_lock);
}
