/* @(#)clnt_raw.c	2.2 88/08/01 4.0 RPCSRC */
/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the "Oracle America, Inc." nor the names of
 *       its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_raw.c 1.22 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * clnt_raw.c
 *
 * Memory based rpc for simple testing and timing.
 * Interface to create an rpc client and server in the same process.
 * This lets us similate rpc and get round trip overhead, without
 * any interference from the kernal.
 */

#include <gssrpc/rpc.h>

#define MCALL_MSG_SIZE 24

/*
 * This is the "network" we will be moving stuff over.
 */
static struct clntraw_private {
	CLIENT	client_object;
	XDR	xdr_stream;
	char	_raw_buf[UDPMSGSIZE];
        union {
	  struct rpc_msg    mashl_rpcmsg;
	  char	            mashl_callmsg[MCALL_MSG_SIZE];
	} u;
	u_int	mcnt;
} *clntraw_private;

static enum clnt_stat	clntraw_call(CLIENT *, rpcproc_t, xdrproc_t,
				     void *, xdrproc_t, void *,
				     struct timeval);
static void		clntraw_abort(CLIENT *);
static void		clntraw_geterr(CLIENT *, struct rpc_err *);
static bool_t		clntraw_freeres(CLIENT *, xdrproc_t, void *);
static bool_t		clntraw_control(CLIENT *, int, void *);
static void		clntraw_destroy(CLIENT *);

static struct clnt_ops client_ops = {
	clntraw_call,
	clntraw_abort,
	clntraw_geterr,
	clntraw_freeres,
	clntraw_destroy,
	clntraw_control
};

void	svc_getreq();

/*
 * Create a client handle for memory based rpc.
 */
CLIENT *
clntraw_create(
	rpcprog_t prog,
	rpcvers_t vers)
{
	struct clntraw_private *clp;
	struct rpc_msg call_msg;
	XDR *xdrs;
	CLIENT *client;

	if (clntraw_private == NULL) {
		clntraw_private = calloc(1, sizeof(*clp));
		if (clntraw_private == NULL)
			return (NULL);
	}
	clp = clntraw_private;
	xdrs = &clp->xdr_stream;
	client = &clp->client_object;
	/*
	 * pre-serialize the staic part of the call msg and stash it away
	 */
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = prog;
	call_msg.rm_call.cb_vers = vers;
	xdrmem_create(xdrs, clp->u.mashl_callmsg, MCALL_MSG_SIZE, XDR_ENCODE);
	if (! xdr_callhdr(xdrs, &call_msg)) {
		perror("clnt_raw.c - Fatal header serialization error.");
	}
	clp->mcnt = XDR_GETPOS(xdrs);
	XDR_DESTROY(xdrs);

	/*
	 * Set xdrmem for client/server shared buffer
	 */
	xdrmem_create(xdrs, clp->_raw_buf, UDPMSGSIZE, XDR_FREE);

	/*
	 * create client handle
	 */
	client->cl_ops = &client_ops;
	client->cl_auth = authnone_create();
	return (client);
}

static enum clnt_stat
clntraw_call(
	CLIENT *h,
	rpcproc_t proc,
	xdrproc_t xargs,
	void * argsp,
	xdrproc_t xresults,
	void * resultsp,
	struct timeval timeout)
{
	register struct clntraw_private *clp = clntraw_private;
	register XDR *xdrs = &clp->xdr_stream;
	struct rpc_msg msg;
	enum clnt_stat status;
	struct rpc_err error;
	long procl = proc;

	if (clp == 0)
		return (RPC_FAILED);
call_again:
	/*
	 * send request
	 */
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	clp->u.mashl_rpcmsg.rm_xid ++ ;
	if ((! XDR_PUTBYTES(xdrs, clp->u.mashl_callmsg, clp->mcnt)) ||
	    (! XDR_PUTLONG(xdrs, &procl)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! (*xargs)(xdrs, argsp))) {
		return (RPC_CANTENCODEARGS);
	}
	(void)XDR_GETPOS(xdrs);  /* called just to cause overhead */

	/*
	 * We have to call server input routine here because this is
	 * all going on in one process. Yuk.
	 */
	svc_getreq(1);

	/*
	 * get results
	 */
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	msg.acpted_rply.ar_verf = gssrpc__null_auth;
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
		enum xdr_op op = xdrs->x_op;
		xdrs->x_op = XDR_FREE;
		xdr_replymsg(xdrs, &msg);
		xdrs->x_op = op;
		return (RPC_CANTDECODERES);
	}
	gssrpc__seterr_reply(&msg, &error);
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
clntraw_geterr(
	CLIENT *cl,
	struct rpc_err *err)
{
}


static bool_t
clntraw_freeres(
	CLIENT *cl,
	xdrproc_t xdr_res,
	void *res_ptr)
{
	register struct clntraw_private *clp = clntraw_private;
	register XDR *xdrs = &clp->xdr_stream;
	bool_t rval;

	if (clp == 0)
	{
		rval = (bool_t) RPC_FAILED;
		return (rval);
	}
	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

/*ARGSUSED*/
static void
clntraw_abort(CLIENT *cl)
{
}

/*ARGSUSED*/
static bool_t
clntraw_control(
	CLIENT *cl,
	int request,
	void *info)
{
	return (FALSE);
}

/*ARGSUSED*/
static void
clntraw_destroy(CLIENT *cl)
{
}
