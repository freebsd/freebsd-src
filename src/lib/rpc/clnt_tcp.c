/* @(#)clnt_tcp.c	2.2 88/08/01 4.0 RPCSRC */
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
static char sccsid[] = "@(#)clnt_tcp.c 1.37 87/10/05 Copyr 1984 Sun Micro";
#endif

/*
 * clnt_tcp.c, Implements a TCP/IP based, client side RPC.
 *
 * TCP based RPC supports 'batched calls'.
 * A sequence of calls may be batched-up in a send buffer.  The rpc call
 * return immediately to the client even though the call was not necessarily
 * sent.  The batching occurs if the results' xdr routine is NULL (0) AND
 * the rpc timeout value is zero (see clnt.h, rpc).
 *
 * Clients should NOT casually batch calls that in fact return results; that is,
 * the server side should be aware that a call is batched and not produce any
 * return message.  Batched calls that produce many result messages can
 * deadlock (netlock) the client and the server....
 *
 * Now go hang yourself.
 */

#include <stdio.h>
#include <unistd.h>
#include <gssrpc/rpc.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <gssrpc/pmap_clnt.h>
/* FD_ZERO may need memset declaration (e.g., Solaris 9) */
#include <string.h>
#include <port-sockets.h>

#define MCALL_MSG_SIZE 24

#ifndef GETSOCKNAME_ARG3_TYPE
#define GETSOCKNAME_ARG3_TYPE int
#endif

static enum clnt_stat	clnttcp_call(CLIENT *, rpcproc_t, xdrproc_t, void *,
				     xdrproc_t, void *, struct timeval);
static void		clnttcp_abort(CLIENT *);
static void		clnttcp_geterr(CLIENT *, struct rpc_err *);
static bool_t		clnttcp_freeres(CLIENT *, xdrproc_t, void *);
static bool_t           clnttcp_control(CLIENT *, int, void *);
static void		clnttcp_destroy(CLIENT *);

static struct clnt_ops tcp_ops = {
	clnttcp_call,
	clnttcp_abort,
	clnttcp_geterr,
	clnttcp_freeres,
	clnttcp_destroy,
	clnttcp_control
};

struct ct_data {
	int		ct_sock;
	bool_t		ct_closeit;
	struct timeval	ct_wait;
	bool_t          ct_waitset;       /* wait set by clnt_control? */
	struct sockaddr_in ct_addr;
	struct rpc_err	ct_error;
	union {
	  char		ct_mcall[MCALL_MSG_SIZE];	/* marshalled callmsg */
	  uint32_t   ct_mcalli;
	} ct_u;
	u_int		ct_mpos;			/* pos after marshal */
	XDR		ct_xdrs;
};

static int	readtcp(char *, caddr_t, int);
static int	writetcp(char *, caddr_t, int);


/*
 * Create a client handle for a tcp/ip connection.
 * If *sockp<0, *sockp is set to a newly created TCP socket and it is
 * connected to raddr.  If *sockp non-negative then
 * raddr is ignored.  The rpc/tcp package does buffering
 * similar to stdio, so the client must pick send and receive buffer sizes,];
 * 0 => use the default.
 * If raddr->sin_port is 0, then a binder on the remote machine is
 * consulted for the right port number.
 * NB: *sockp is copied into a private area.
 * NB: It is the clients responsibility to close *sockp.
 * NB: The rpch->cl_auth is set null authentication.  Caller may wish to set this
 * something more useful.
 */
CLIENT *
clnttcp_create(
	struct sockaddr_in *raddr,
	rpcprog_t prog,
	rpcvers_t vers,
        SOCKET *sockp,
	u_int sendsz,
	u_int recvsz)
{
	CLIENT *h;
	register struct ct_data *ct = 0;
	struct timeval now;
	struct rpc_msg call_msg;

	h  = (CLIENT *)mem_alloc(sizeof(*h));
	if (h == NULL) {
		(void)fprintf(stderr, "clnttcp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}
	ct = (struct ct_data *)mem_alloc(sizeof(*ct));
	if (ct == NULL) {
		(void)fprintf(stderr, "clnttcp_create: out of memory\n");
		rpc_createerr.cf_stat = RPC_SYSTEMERROR;
		rpc_createerr.cf_error.re_errno = errno;
		goto fooy;
	}

	/*
	 * If no port number given ask the pmap for one
	 */
	if (raddr != NULL && raddr->sin_port == 0) {
		u_short port;
		if ((port = pmap_getport(raddr, prog, vers, IPPROTO_TCP)) == 0) {
			mem_free((caddr_t)ct, sizeof(struct ct_data));
			mem_free((caddr_t)h, sizeof(CLIENT));
			return ((CLIENT *)NULL);
		}
		raddr->sin_port = htons(port);
	}

	/*
	 * If no socket given, open one
	 */
	if (*sockp < 0) {
		*sockp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		(void)bindresvport_sa(*sockp, NULL);
		if ((*sockp < 0)
		    || (connect(*sockp, (struct sockaddr *)raddr,
		    sizeof(*raddr)) < 0)) {
			rpc_createerr.cf_stat = RPC_SYSTEMERROR;
			rpc_createerr.cf_error.re_errno = errno;
                        (void)closesocket(*sockp);
			goto fooy;
		}
		ct->ct_closeit = TRUE;
	} else {
		ct->ct_closeit = FALSE;
	}

	/*
	 * Set up private data struct
	 */
	ct->ct_sock = *sockp;
	ct->ct_wait.tv_usec = 0;
	ct->ct_waitset = FALSE;
	if (raddr == NULL) {
	    /* Get the remote address from the socket, if it's IPv4. */
	    struct sockaddr_in sin;
	    socklen_t len = sizeof(sin);
	    int ret = getpeername(ct->ct_sock, (struct sockaddr *)&sin, &len);
	    if (ret == 0 && len == sizeof(sin) && sin.sin_family == AF_INET)
		ct->ct_addr = sin;
	    else
		memset(&ct->ct_addr, 0, sizeof(ct->ct_addr));
	} else
	    ct->ct_addr = *raddr;

	/*
	 * Initialize call message
	 */
	(void)gettimeofday(&now, (struct timezone *)0);
	call_msg.rm_xid = getpid() ^ now.tv_sec ^ now.tv_usec;
	call_msg.rm_direction = CALL;
	call_msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	call_msg.rm_call.cb_prog = prog;
	call_msg.rm_call.cb_vers = vers;

	/*
	 * pre-serialize the staic part of the call msg and stash it away
	 */
	xdrmem_create(&(ct->ct_xdrs), ct->ct_u.ct_mcall, MCALL_MSG_SIZE,
	    XDR_ENCODE);
	if (! xdr_callhdr(&(ct->ct_xdrs), &call_msg)) {
		if (ct->ct_closeit)
                        (void)closesocket(*sockp);
		goto fooy;
	}
	ct->ct_mpos = XDR_GETPOS(&(ct->ct_xdrs));
	XDR_DESTROY(&(ct->ct_xdrs));

	/*
	 * Create a client handle which uses xdrrec for serialization
	 * and authnone for authentication.
	 */
	xdrrec_create(&(ct->ct_xdrs), sendsz, recvsz,
	    (caddr_t)ct, readtcp, writetcp);
	h->cl_ops = &tcp_ops;
	h->cl_private = (caddr_t) ct;
	h->cl_auth = authnone_create();
	return (h);

fooy:
	/*
	 * Something goofed, free stuff and barf
	 */
	mem_free((caddr_t)ct, sizeof(struct ct_data));
	mem_free((caddr_t)h, sizeof(CLIENT));
	return ((CLIENT *)NULL);
}

static enum clnt_stat
clnttcp_call(
	register CLIENT *h,
	rpcproc_t proc,
	xdrproc_t xdr_args,
	void * args_ptr,
	xdrproc_t xdr_results,
	void * results_ptr,
	struct timeval timeout)
{
	register struct ct_data *ct = (struct ct_data *) h->cl_private;
	register XDR *xdrs = &(ct->ct_xdrs);
	struct rpc_msg reply_msg;
	uint32_t x_id;
	uint32_t *msg_x_id = &ct->ct_u.ct_mcalli;	/* yuk */
	register bool_t shipnow;
	int refreshes = 2;
	long procl = proc;

	if (!ct->ct_waitset) {
		ct->ct_wait = timeout;
	}

	shipnow =
	    (xdr_results == (xdrproc_t)0 && timeout.tv_sec == 0
	    && timeout.tv_usec == 0) ? FALSE : TRUE;

call_again:
	xdrs->x_op = XDR_ENCODE;
	ct->ct_error.re_status = RPC_SUCCESS;
	x_id = ntohl(--(*msg_x_id));
	if ((! XDR_PUTBYTES(xdrs, ct->ct_u.ct_mcall, ct->ct_mpos)) ||
	    (! XDR_PUTLONG(xdrs, &procl)) ||
	    (! AUTH_MARSHALL(h->cl_auth, xdrs)) ||
	    (! AUTH_WRAP(h->cl_auth, xdrs, xdr_args, args_ptr))) {
		if (ct->ct_error.re_status == RPC_SUCCESS)
			ct->ct_error.re_status = RPC_CANTENCODEARGS;
		(void)xdrrec_endofrecord(xdrs, TRUE);
		return (ct->ct_error.re_status);
	}
	if (! xdrrec_endofrecord(xdrs, shipnow))
		return (ct->ct_error.re_status = RPC_CANTSEND);
	if (! shipnow)
		return (RPC_SUCCESS);
	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		return(ct->ct_error.re_status = RPC_TIMEDOUT);
	}


	/*
	 * Keep receiving until we get a valid transaction id
	 */
	xdrs->x_op = XDR_DECODE;
	while (TRUE) {
		reply_msg.acpted_rply.ar_verf = gssrpc__null_auth;
		reply_msg.acpted_rply.ar_results.where = NULL;
		reply_msg.acpted_rply.ar_results.proc = xdr_void;
		if (! xdrrec_skiprecord(xdrs))
			return (ct->ct_error.re_status);
		/* now decode and validate the response header */
		if (! xdr_replymsg(xdrs, &reply_msg)) {
			/*
			 * Free some stuff allocated by xdr_replymsg()
			 * to avoid leaks, since it may allocate
			 * memory from partially successful decodes.
			 */
			enum xdr_op op = xdrs->x_op;
			xdrs->x_op = XDR_FREE;
			xdr_replymsg(xdrs, &reply_msg);
			xdrs->x_op = op;
			if (ct->ct_error.re_status == RPC_SUCCESS)
				continue;
			return (ct->ct_error.re_status);
		}
		if (reply_msg.rm_xid == x_id)
			break;
	}

	/*
	 * process header
	 */
	gssrpc__seterr_reply(&reply_msg, &(ct->ct_error));
	if (ct->ct_error.re_status == RPC_SUCCESS) {
		if (! AUTH_VALIDATE(h->cl_auth, &reply_msg.acpted_rply.ar_verf)) {
			ct->ct_error.re_status = RPC_AUTHERROR;
			ct->ct_error.re_why = AUTH_INVALIDRESP;
		} else if (! AUTH_UNWRAP(h->cl_auth, xdrs,
					 xdr_results, results_ptr)) {
			if (ct->ct_error.re_status == RPC_SUCCESS)
				ct->ct_error.re_status = RPC_CANTDECODERES;
		}
	}  /* end successful completion */
	else {
		/* maybe our credentials need to be refreshed ... */
		if (refreshes-- && AUTH_REFRESH(h->cl_auth, &reply_msg))
			goto call_again;
	}  /* end of unsuccessful completion */
	/* free verifier ... */
	if ((reply_msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
	    (reply_msg.acpted_rply.ar_verf.oa_base != NULL)) {
	    xdrs->x_op = XDR_FREE;
	    (void)xdr_opaque_auth(xdrs, &(reply_msg.acpted_rply.ar_verf));
	}
	return (ct->ct_error.re_status);
}

static void
clnttcp_geterr(
	CLIENT *h,
	struct rpc_err *errp)
{
	register struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	*errp = ct->ct_error;
}

static bool_t
clnttcp_freeres(
	CLIENT *cl,
	xdrproc_t xdr_res,
	void * res_ptr)
{
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;
	register XDR *xdrs = &(ct->ct_xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_res)(xdrs, res_ptr));
}

/*ARGSUSED*/
static void
clnttcp_abort(CLIENT *cl)
{
}

static bool_t
clnttcp_control(
	CLIENT *cl,
	int request,
	void *info)
{
	register struct ct_data *ct = (struct ct_data *)cl->cl_private;
	GETSOCKNAME_ARG3_TYPE len;

	switch (request) {
	case CLSET_TIMEOUT:
		ct->ct_wait = *(struct timeval *)info;
		ct->ct_waitset = TRUE;
		break;
	case CLGET_TIMEOUT:
		*(struct timeval *)info = ct->ct_wait;
		break;
	case CLGET_SERVER_ADDR:
		*(struct sockaddr_in *)info = ct->ct_addr;
		break;
	case CLGET_LOCAL_ADDR:
		len = sizeof(struct sockaddr);
		if (getsockname(ct->ct_sock, (struct sockaddr*)info, &len) < 0)
		     return FALSE;
		else
		     return TRUE;
	default:
		return (FALSE);
	}
	return (TRUE);
}


static void
clnttcp_destroy(CLIENT *h)
{
	register struct ct_data *ct =
	    (struct ct_data *) h->cl_private;

	if (ct->ct_closeit)
                (void)closesocket(ct->ct_sock);
	XDR_DESTROY(&(ct->ct_xdrs));
	mem_free((caddr_t)ct, sizeof(struct ct_data));
	mem_free((caddr_t)h, sizeof(CLIENT));
}

/*
 * Interface between xdr serializer and tcp connection.
 * Behaves like the system calls, read & write, but keeps some error state
 * around for the rpc level.
 */
static int
readtcp(
        char *ctptr,
	caddr_t buf,
	register int len)
{
  register struct ct_data *ct = (struct ct_data *)(void *)ctptr;
  struct timeval tout;
#ifdef FD_SETSIZE
	fd_set mask;
	fd_set readfds;

	if (len == 0)
		return (0);
	FD_ZERO(&mask);
	FD_SET(ct->ct_sock, &mask);
#else
	register int mask = 1 << (ct->ct_sock);
	int readfds;

	if (len == 0)
		return (0);

#endif /* def FD_SETSIZE */
	while (TRUE) {
		readfds = mask;
		tout = ct->ct_wait;
		switch (select(gssrpc__rpc_dtablesize(), &readfds, (fd_set*)NULL, (fd_set*)NULL,
			       &tout)) {
		case 0:
			ct->ct_error.re_status = RPC_TIMEDOUT;
			return (-1);

		case -1:
			if (errno == EINTR)
				continue;
			ct->ct_error.re_status = RPC_CANTRECV;
			ct->ct_error.re_errno = errno;
			return (-1);
		}
		break;
	}
	switch (len = read(ct->ct_sock, buf, (size_t) len)) {

	case 0:
		/* premature eof */
		ct->ct_error.re_errno = ECONNRESET;
		ct->ct_error.re_status = RPC_CANTRECV;
		len = -1;  /* it's really an error */
		break;

	case -1:
		ct->ct_error.re_errno = errno;
		ct->ct_error.re_status = RPC_CANTRECV;
		break;
	}
	return (len);
}

static int
writetcp(
        char *ctptr,
	caddr_t buf,
	int len)
{
	struct ct_data *ct = (struct ct_data *)(void *)ctptr;
	register int i, cnt;

	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		if ((i = write(ct->ct_sock, buf, (size_t) cnt)) == -1) {
			ct->ct_error.re_errno = errno;
			ct->ct_error.re_status = RPC_CANTSEND;
			return (-1);
		}
	}
	return (len);
}
