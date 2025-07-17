/* @(#)svc_tcp.c	2.2 88/08/01 4.0 RPCSRC */
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
static char sccsid[] = "@(#)svc_tcp.c 1.21 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * svc_tcp.c, Server side for TCP/IP based RPC.
 *
 * Actually implements two flavors of transporter -
 * a tcp rendezvouser (a listener and connection establisher)
 * and a record/tcp stream.
 */

#include "k5-platform.h"
#include <unistd.h>
#include <gssrpc/rpc.h>
#include <sys/socket.h>
#include <port-sockets.h>
#include <socket-utils.h>
/*extern bool_t abort();
extern errno;
*/

#ifndef FD_SETSIZE
#ifdef NBBY
#define NOFILE (sizeof(int) * NBBY)
#else
#define NOFILE (sizeof(int) * 8)
#endif
#endif

/*
 * Ops vector for TCP/IP based rpc service handle
 */
static bool_t		svctcp_recv(SVCXPRT *, struct rpc_msg *);
static enum xprt_stat	svctcp_stat(SVCXPRT *);
static bool_t		svctcp_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t		svctcp_reply(SVCXPRT *, struct rpc_msg *);
static bool_t		svctcp_freeargs(SVCXPRT *, xdrproc_t, void *);
static void		svctcp_destroy(SVCXPRT *);

static struct xp_ops svctcp_op = {
	svctcp_recv,
	svctcp_stat,
	svctcp_getargs,
	svctcp_reply,
	svctcp_freeargs,
	svctcp_destroy
};

/*
 * Ops vector for TCP/IP rendezvous handler
 */
static bool_t		rendezvous_request(SVCXPRT *, struct rpc_msg *);
static bool_t		abortx(void);
static bool_t		abortx_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t		abortx_reply(SVCXPRT *, struct rpc_msg *);
static bool_t		abortx_freeargs(SVCXPRT *, xdrproc_t, void *);
static enum xprt_stat	rendezvous_stat(SVCXPRT *);

static struct xp_ops svctcp_rendezvous_op = {
	rendezvous_request,
	rendezvous_stat,
	abortx_getargs,
	abortx_reply,
	abortx_freeargs,
	svctcp_destroy
};

static int readtcp(char *, caddr_t, int), writetcp(char *, caddr_t, int);
static SVCXPRT *makefd_xprt(int, u_int, u_int);

struct tcp_rendezvous { /* kept in xprt->xp_p1 */
	u_int sendsize;
	u_int recvsize;
};

struct tcp_conn {  /* kept in xprt->xp_p1 */
	enum xprt_stat strm_stat;
	uint32_t x_id;
	XDR xdrs;
	char verf_body[MAX_AUTH_BYTES];
};

/*
 * Usage:
 *	xprt = svctcp_create(sock, send_buf_size, recv_buf_size);
 *
 * Creates, registers, and returns a (rpc) tcp based transporter.
 * Once *xprt is initialized, it is registered as a transporter
 * see (svc.h, xprt_register).  This routine returns
 * a NULL if a problem occurred.
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svctcp_create
 * binds it to an arbitrary port.  The routine then starts a tcp
 * listener on the socket's associated port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 *
 * Since tcp streams do buffered io similar to stdio, the caller can specify
 * how big the send and receive buffers are via the second and third parms;
 * 0 => use the system default.
 */
SVCXPRT *
svctcp_create(
        SOCKET sock,
	u_int sendsize,
	u_int recvsize)
{
	bool_t madesock = FALSE;
	SVCXPRT *xprt;
	struct tcp_rendezvous *r;
	struct sockaddr_storage ss;
	struct sockaddr *sa = (struct sockaddr *)&ss;
	socklen_t len;

	if (sock == RPC_ANYSOCK) {
		if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
			perror("svctcp_.c - udp socket creation problem");
			return ((SVCXPRT *)NULL);
		}
		set_cloexec_fd(sock);
		madesock = TRUE;
		memset(&ss, 0, sizeof(ss));
		sa->sa_family = AF_INET;
	} else {
		len = sizeof(struct sockaddr_storage);
		if (getsockname(sock, sa, &len) != 0) {
			perror("svc_tcp.c - cannot getsockname");
			return ((SVCXPRT *)NULL);
		}
	}

	if (bindresvport_sa(sock, sa)) {
		sa_setport(sa, 0);
		(void)bind(sock, sa, sa_socklen(sa));
	}
	len = sizeof(struct sockaddr_storage);
	if (getsockname(sock, sa, &len) != 0) {
		perror("svc_tcp.c - cannot getsockname");
                if (madesock)
                        (void)closesocket(sock);
		return ((SVCXPRT *)NULL);
	}
	if (listen(sock, 2) != 0) {
		perror("svctcp_.c - cannot listen");
                if (madesock)
                        (void)closesocket(sock);
		return ((SVCXPRT *)NULL);
	}
	r = (struct tcp_rendezvous *)mem_alloc(sizeof(*r));
	if (r == NULL) {
		(void) fprintf(stderr, "svctcp_create: out of memory\n");
		return (NULL);
	}
	r->sendsize = sendsize;
	r->recvsize = recvsize;
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		(void) fprintf(stderr, "svctcp_create: out of memory\n");
		return (NULL);
	}
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)r;
	xprt->xp_auth = NULL;
	xprt->xp_verf = gssrpc__null_auth;
	xprt->xp_ops = &svctcp_rendezvous_op;
	xprt->xp_port = sa_getport(sa);
	xprt->xp_sock = sock;
	xprt->xp_laddrlen = 0;
	xprt_register(xprt);
	return (xprt);
}

/*
 * Like svtcp_create(), except the routine takes any *open* UNIX file
 * descriptor as its first input.
 */
SVCXPRT *
svcfd_create(
	int fd,
	u_int sendsize,
	u_int recvsize)
{

	return (makefd_xprt(fd, sendsize, recvsize));
}

static SVCXPRT *
makefd_xprt(
	int fd,
	u_int sendsize,
	u_int recvsize)
{
	SVCXPRT *xprt;
	struct tcp_conn *cd;

#ifdef FD_SETSIZE
	if (fd >= FD_SETSIZE) {
		(void) fprintf(stderr, "svc_tcp: makefd_xprt: fd too high\n");
		xprt = NULL;
		goto done;
	}
#else
	if (fd >= NOFILE) {
		(void) fprintf(stderr, "svc_tcp: makefd_xprt: fd too high\n");
		xprt = NULL;
		goto done;
	}
#endif
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == (SVCXPRT *)NULL) {
		(void) fprintf(stderr, "svc_tcp: makefd_xprt: out of memory\n");
		goto done;
	}
	cd = (struct tcp_conn *)mem_alloc(sizeof(struct tcp_conn));
	if (cd == (struct tcp_conn *)NULL) {
		(void) fprintf(stderr, "svc_tcp: makefd_xprt: out of memory\n");
		mem_free((char *) xprt, sizeof(SVCXPRT));
		xprt = (SVCXPRT *)NULL;
		goto done;
	}
	cd->strm_stat = XPRT_IDLE;
	xdrrec_create(&(cd->xdrs), sendsize, recvsize,
	    (caddr_t)xprt, readtcp, writetcp);
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)cd;
	xprt->xp_auth = NULL;
	xprt->xp_verf.oa_base = cd->verf_body;
	xprt->xp_addrlen = 0;
	xprt->xp_laddrlen = 0;
	xprt->xp_ops = &svctcp_op;  /* truly deals with calls */
	xprt->xp_port = 0;  /* this is a connection, not a rendezvouser */
	xprt->xp_sock = fd;
	xprt_register(xprt);
    done:
	return (xprt);
}

static bool_t
rendezvous_request(
	SVCXPRT *xprt,
	struct rpc_msg *msg)
{
        SOCKET sock;
	struct tcp_rendezvous *r;
	struct sockaddr_in addr, laddr;
	socklen_t len, llen;

	r = (struct tcp_rendezvous *)xprt->xp_p1;
    again:
	len = llen = sizeof(struct sockaddr_in);
	if ((sock = accept(xprt->xp_sock, (struct sockaddr *)&addr,
	    &len)) < 0) {
		if (errno == EINTR)
			goto again;
	       return (FALSE);
	}
	set_cloexec_fd(sock);
	if (getsockname(sock, (struct sockaddr *) &laddr, &llen) < 0)
	     return (FALSE);

	/*
	 * make a new transporter (re-uses xprt)
	 */
	xprt = makefd_xprt(sock, r->sendsize, r->recvsize);
	if (xprt == NULL) {
                (void)closesocket(sock);
		return (FALSE);
	}
	xprt->xp_raddr = addr;
	xprt->xp_addrlen = len;
	xprt->xp_laddr = laddr;
	xprt->xp_laddrlen = llen;
	return (FALSE); /* there is never an rpc msg to be processed */
}

static enum xprt_stat
rendezvous_stat(SVCXPRT *xprt)
{

	return (XPRT_IDLE);
}

static void
svctcp_destroy(SVCXPRT *xprt)
{
	struct tcp_conn *cd = xprt->xp_p1;

	xprt_unregister(xprt);
        (void)closesocket(xprt->xp_sock);
	if (xprt->xp_port != 0) {
		/* a rendezvouser socket */
		xprt->xp_port = 0;
	} else {
		/* an actual connection socket */
		XDR_DESTROY(&(cd->xdrs));
	}
	if (xprt->xp_auth != NULL) {
		SVCAUTH_DESTROY(xprt->xp_auth);
		xprt->xp_auth = NULL;
	}
	mem_free((caddr_t)cd, sizeof(struct tcp_conn));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}

/*
 * All read operations timeout after 35 seconds.
 * A timeout is fatal for the connection.
 */
static struct timeval wait_per_try = { 35, 0 };

/*
 * reads data from the tcp connection.
 * any error is fatal and the connection is closed.
 * (And a read of zero bytes is a half closed stream => error.)
 */
static int
readtcp(
        char *xprtptr,
	caddr_t buf,
	int len)
{
	SVCXPRT *xprt = (void *)xprtptr;
	int sock = xprt->xp_sock;
	struct timeval tout;
#ifdef FD_SETSIZE
	fd_set mask;
	fd_set readfds;

	FD_ZERO(&mask);
	FD_SET(sock, &mask);
#else
	int mask = 1 << sock;
	int readfds;
#endif /* def FD_SETSIZE */
#ifdef FD_SETSIZE
#define loopcond (!FD_ISSET(sock, &readfds))
#else
#define loopcond (readfds != mask)
#endif
	do {
		readfds = mask;
		tout = wait_per_try;
		if (select(sock + 1, &readfds, (fd_set*)NULL,
			   (fd_set*)NULL, &tout) <= 0) {
			if (errno == EINTR) {
				continue;
			}
			goto fatal_err;
		}
	} while (loopcond);
	if ((len = read(sock, buf, (size_t) len)) > 0) {
		return (len);
	}
fatal_err:
	((struct tcp_conn *)(xprt->xp_p1))->strm_stat = XPRT_DIED;
	return (-1);
}

/*
 * writes data to the tcp connection.
 * Any error is fatal and the connection is closed.
 */
static int
writetcp(
	char *xprtptr,
	caddr_t buf,
	int len)
{
	SVCXPRT *xprt = (void *)xprtptr;
	int i, cnt;

	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		if ((i = write(xprt->xp_sock, buf, (size_t) cnt)) < 0) {
			((struct tcp_conn *)(xprt->xp_p1))->strm_stat =
			    XPRT_DIED;
			return (-1);
		}
	}
	return (len);
}

static enum xprt_stat
svctcp_stat(SVCXPRT *xprt)
{
	struct tcp_conn *cd = xprt->xp_p1;

	if (cd->strm_stat == XPRT_DIED)
		return (XPRT_DIED);
	if (! xdrrec_eof(&(cd->xdrs)))
		return (XPRT_MOREREQS);
	return (XPRT_IDLE);
}

static bool_t
svctcp_recv(
	SVCXPRT *xprt,
	struct rpc_msg *msg)
{
	struct tcp_conn *cd = xprt->xp_p1;
	XDR *xdrs = &cd->xdrs;

	xdrs->x_op = XDR_DECODE;
	(void)xdrrec_skiprecord(xdrs);
	if (xdr_callmsg(xdrs, msg)) {
		cd->x_id = msg->rm_xid;
		return (TRUE);
	}
	return (FALSE);
}

static bool_t
svctcp_getargs(
	SVCXPRT *xprt,
	xdrproc_t xdr_args,
	void *args_ptr)
{
	if (! SVCAUTH_UNWRAP(xprt->xp_auth,
			     &(((struct tcp_conn *)(xprt->xp_p1))->xdrs),
			     xdr_args, args_ptr)) {
		(void)svctcp_freeargs(xprt, xdr_args, args_ptr);
		return FALSE;
	}
	return TRUE;
}

static bool_t
svctcp_freeargs(
	SVCXPRT *xprt,
	xdrproc_t xdr_args,
	void * args_ptr)
{
	XDR *xdrs = &((struct tcp_conn *)(xprt->xp_p1))->xdrs;

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static bool_t svctcp_reply(
	SVCXPRT *xprt,
	struct rpc_msg *msg)
{
	struct tcp_conn *cd = xprt->xp_p1;
	XDR *xdrs = &cd->xdrs;
	bool_t stat;

	xdrproc_t xdr_results = NULL;
	caddr_t xdr_location = 0;
	bool_t has_args;

	if (msg->rm_reply.rp_stat == MSG_ACCEPTED &&
	    msg->rm_reply.rp_acpt.ar_stat == SUCCESS) {
		has_args = TRUE;
		xdr_results = msg->acpted_rply.ar_results.proc;
		xdr_location = msg->acpted_rply.ar_results.where;

		msg->acpted_rply.ar_results.proc = xdr_void;
		msg->acpted_rply.ar_results.where = NULL;
	} else
		has_args = FALSE;

	xdrs->x_op = XDR_ENCODE;
	msg->rm_xid = cd->x_id;
	stat = FALSE;
	if (xdr_replymsg(xdrs, msg) &&
	    (!has_args ||
	     (SVCAUTH_WRAP(xprt->xp_auth, xdrs, xdr_results, xdr_location)))) {
		stat = TRUE;
	}
	(void)xdrrec_endofrecord(xdrs, TRUE);
	return (stat);
}

static bool_t abortx(void)
{
	abort();
	return 1;
}

static bool_t abortx_getargs(
	SVCXPRT *xprt,
	xdrproc_t proc,
	void *info)
{
	return abortx();
}

static bool_t abortx_reply(SVCXPRT *xprt, struct rpc_msg *msg)
{
	return abortx();
}

static bool_t abortx_freeargs(
	SVCXPRT *xprt, xdrproc_t proc,
	void * info)
{
	return abortx();
}
