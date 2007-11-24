/*	$NetBSD: clnt_raw.c,v 1.20 2000/12/10 04:12:03 christos Exp $	*/

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
static char *sccsid2 = "@(#)clnt_raw.c 1.22 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)clnt_raw.c	2.2 88/08/01 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * clnt_raw.c
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * Memory based rpc for simple testing and timing.
 * Interface to create an rpc client and server in the same process.
 * This lets us similate rpc and get round trip overhead, without
 * any interference from the kernel.
 */

#include "namespace.h"
#include "reentrant.h"
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include <rpc/rpc.h>
#include <rpc/raw.h>
#include "un-namespace.h"

extern mutex_t clntraw_lock;

#define MCALL_MSG_SIZE 24

/*
 * This is the "network" we will be moving stuff over.
 */
static struct clntraw_private {
	CLIENT	client_object;
	XDR	xdr_stream;
	char	*_raw_buf;
	union {
	    struct rpc_msg	mashl_rpcmsg;
	    char 		mashl_callmsg[MCALL_MSG_SIZE];
	} u;
	u_int	mcnt;
} *clntraw_private;

static enum clnt_stat clnt_raw_call(CLIENT *, rpcproc_t, xdrproc_t, void *,
	xdrproc_t, void *, struct timeval);
static void clnt_raw_geterr(CLIENT *, struct rpc_err *);
static bool_t clnt_raw_freeres(CLIENT *, xdrproc_t, void *);
static void clnt_raw_abort(CLIENT *);
static bool_t clnt_raw_control(CLIENT *, u_int, void *);
static void clnt_raw_destroy(CLIENT *);
static struct clnt_ops *clnt_raw_ops(void);

/*
 * Create a client handle for memory based rpc.
 */
CLIENT *
clnt_raw_create(prog, vers)
	rpcprog_t prog;
	rpcvers_t vers;
{
	struct clntraw_private *clp = clntraw_private;
	struct rpc_msg call_msg;
	XDR *xdrs = &clp->xdr_stream;
	CLIENT	*client = &clp->client_object;

	mutex_lock(&clntraw_lock);
	if (clp == NULL) {
		clp = (struct clntraw_private *)calloc(1, sizeof (*clp));
		if (clp == NULL) {
			mutex_unlock(&clntraw_lock);
			return NULL;
		}
		if (__rpc_rawcombuf == NULL)
			__rpc_rawcombuf =
			    (char *)calloc(UDPMSGSIZE, sizeof (char));
		clp->_raw_buf = __rpc_rawcombuf;
		clntraw_private = clp;
	}
	/*
	 * pre-serialize the static part of the call msg and stash it away
	 */
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	/* XXX: prog and vers have been long historically :-( */
	call_msg.rm_call.cb_prog = (u_int32_t)prog;
	call_msg.rm_call.cb_vers = (u_int32_t)vers;
	xdrmem_create(xdrs, clp->u.mashl_callmsg, MCALL_MSG_SIZE, XDR_ENCODE); 
	if (! xdr_callhdr(xdrs, &call_msg))
		warnx("clntraw_create - Fatal header serialization error.");
	clp->mcnt = XDR_GETPOS(xdrs);
	XDR_DESTROY(xdrs);

	/*
	 * Set xdrmem for client/server shared buffer
	 */
	xdrmem_create(xdrs, clp->_raw_buf, UDPMSGSIZE, XDR_FREE);

	/*
	 * create client handle
	 */
	client->cl_ops = clnt_raw_ops();
	client->cl_auth = authnone_create();
	mutex_unlock(&clntraw_lock);
	return (client);
}

/* ARGSUSED */
static enum clnt_stat 
clnt_raw_call(h, proc, xargs, argsp, xresults, resultsp, timeout)
	CLIENT *h;
	rpcproc_t proc;
	xdrproc_t xargs;
	void *argsp;
	xdrproc_t xresults;
	void *resultsp;
	struct timeval timeout;
{
	struct clntraw_private *clp = clntraw_private;
	XDR *xdrs = &clp->xdr_stream;
	struct rpc_msg msg;
	enum clnt_stat status;
	struct rpc_err error;

	assert(h != NULL);

	mutex_lock(&clntraw_lock);
	if (clp == NULL) {
		mutex_unlock(&clntraw_lock);
		return (RPC_FAILED);
	}
	mutex_unlock(&clntraw_lock);

call_again:
	/*
	 * send request
	 */
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	clp->u.mashl_rpcmsg.rm_xid ++ ;
	if ((! XDR_PUTBYTES(xdrs, clp->u.mashl_callmsg, clp->mcnt)) ||
	    (! XDR_PUTINT32(xdrs, &proc)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*xargs)(xdrs, argsp))) {
		return (RPC_CANTENCODEARGS);
	}
	(void)XDR_GETPOS(xdrs);  /* called just to cause overhead */

	/*
	 * We have to call server input routine here because this is
	 * all going on in one process. Yuk.
	 */
	svc_getreq_common(FD_SETSIZE);

	/*
	 * get results
	 */
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	msg.acpted_rply.ar_verf = _null_auth;
	msg.acpted_rply.ar_results.where = resultsp;
	msg.acpted_rply.ar_results.proc = xresults;
	if (! xdr_replymsg(xdrs, &msg)) {
		/*
		 * It's possible for xdr_replymsg() to fail partway
		 * through its attempt to decode the result from the
		 * server. If this happens, it will leave the reply
		 * structure partially populated with dynamically
		 * allocated memory. (This can happen if someone uses
		 * clntudp_bufcreate() to create a CLIENT handle and
		 * specifies a receive buffer size that is too small.)
		 * This memory must be free()ed to avoid a leak.
		 */
		int op = xdrs->x_op;
		xdrs->x_op = XDR_FREE;
		xdr_replymsg(xdrs, &msg);
		xdrs->x_op = op;
		return (RPC_CANTDECODERES);
	}
	_seterr_reply(&msg, &error);
	status = error.re_status;

	if (status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, &msg.acpted_rply.ar_verf)) {
			status = RPC_AUTHERROR;
		}
	}  /* end successful completion */
	else {
		if (AUTH_REFRESH(h->cl_auth, &msg))
			goto call_again;
	}  /* end of unsuccessful completion */

	if (status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, &msg.acpted_rply.ar_verf)) {
			status = RPC_AUTHERROR;
		}
		if (msg.acpted_rply.ar_verf.oa_base != NULL) {
			xdrs->x_op = XDR_FREE;
			(void)xdr_opaque_auth(xdrs, &(msg.acpted_rply.ar_verf));
		}
	}

	return (status);
}

/*ARGSUSED*/
static void
clnt_raw_geterr(cl, err)
	CLIENT *cl;
	struct rpc_err *err;
{
}


/* ARGSUSED */
static bool_t
clnt_raw_freeres(cl, xdr_res, res_ptr)
	CLIENT *cl;
	xdrproc_t xdr_res;
	void *res_ptr;
{
	struct clntraw_private *clp = clntraw_private;
	XDR *xdrs = &clp->xdr_stream;
	bool_t rval;

	mutex_lock(&clntraw_lock);
	if (clp == NULL) {
		rval = (bool_t) RPC_FAILED;
		mutex_unlock(&clntraw_lock);
		return (rval);
	}
	mutex_unlock(&clntraw_lock);
	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

/*ARGSUSED*/
static void
clnt_raw_abort(cl)
	CLIENT *cl;
{
}

/*ARGSUSED*/
static bool_t
clnt_raw_control(cl, ui, str)
	CLIENT *cl;
	u_int ui;
	void *str;
{
	return (FALSE);
}

/*ARGSUSED*/
static void
clnt_raw_destroy(cl)
	CLIENT *cl;
{
}

static struct clnt_ops *
clnt_raw_ops()
{
	static struct clnt_ops ops;
	extern mutex_t  ops_lock;

	/* VARIABLES PROTECTED BY ops_lock: ops */

	mutex_lock(&ops_lock);
	if (ops.cl_call == NULL) {
		ops.cl_call = clnt_raw_call;
		ops.cl_abort = clnt_raw_abort;
		ops.cl_geterr = clnt_raw_geterr;
		ops.cl_freeres = clnt_raw_freeres;
		ops.cl_destroy = clnt_raw_destroy;
		ops.cl_control = clnt_raw_control;
	}
	mutex_unlock(&ops_lock);
	return (&ops);
}
