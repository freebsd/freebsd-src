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
/*static char *sccsid = "from: @(#)svc_unix.c 1.21 87/08/11 Copyr 1984 Sun Micro";*/
/*static char *sccsid = "from: @(#)svc_unix.c	2.2 88/08/01 4.0 RPCSRC";*/
static char *rcsid = "$FreeBSD$";
#endif

/*
 * svc_unix.c, Server side for TCP/IP based RPC.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * Actually implements two flavors of transporter -
 * a unix rendezvouser (a listner and connection establisher)
 * and a record/unix stream.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <errno.h>

/*
 * Ops vector for AF_UNIX based rpc service handle
 */
static bool_t		svcunix_recv();
static enum xprt_stat	svcunix_stat();
static bool_t		svcunix_getargs();
static bool_t		svcunix_reply();
static bool_t		svcunix_freeargs();
static void		svcunix_destroy();

static struct xp_ops svcunix_op = {
	svcunix_recv,
	svcunix_stat,
	svcunix_getargs,
	svcunix_reply,
	svcunix_freeargs,
	svcunix_destroy
};

/*
 * Ops vector for TCP/IP rendezvous handler
 */
static bool_t		rendezvous_request();
static enum xprt_stat	rendezvous_stat();

static struct xp_ops svcunix_rendezvous_op = {
	rendezvous_request,
	rendezvous_stat,
	(bool_t (*)())abort,
	(bool_t (*)())abort,
	(bool_t (*)())abort,
	svcunix_destroy
};

static int readunix(), writeunix();
static SVCXPRT *makefd_xprt();

struct unix_rendezvous { /* kept in xprt->xp_p1 */
	u_int sendsize;
	u_int recvsize;
};

struct unix_conn {  /* kept in xprt->xp_p1 */
	enum xprt_stat strm_stat;
	u_long x_id;
	XDR xdrs;
	char verf_body[MAX_AUTH_BYTES];
};


struct cmessage {
	struct cmsghdr cmsg;
	struct cmsgcred cmcred;
};

static struct cmessage cm;

static int __msgread(sock, buf, cnt)
	int sock;
	void *buf;
	size_t cnt;
{
	struct iovec iov[1];
	struct msghdr msg;

	bzero((char *)&cm, sizeof(cm));
	iov[0].iov_base = buf;
	iov[0].iov_len = cnt;

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = (caddr_t)&cm;
	msg.msg_controllen = sizeof(struct cmessage);
	msg.msg_flags = 0;

	return(recvmsg(sock, &msg, 0));
}

static int __msgwrite(sock, buf, cnt)
	int sock;
	void *buf;
	size_t cnt;
{
	struct iovec iov[1];
	struct msghdr msg;

	bzero((char *)&cm, sizeof(cm));
	iov[0].iov_base = buf;
	iov[0].iov_len = cnt;

	cm.cmsg.cmsg_type = SCM_CREDS;
	cm.cmsg.cmsg_level = SOL_SOCKET;
	cm.cmsg.cmsg_len = sizeof(struct cmessage);

	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = (caddr_t)&cm;
	msg.msg_controllen = sizeof(struct cmessage);
	msg.msg_flags = 0;

	return(sendmsg(sock, &msg, 0));
}

/*
 * Usage:
 *	xprt = svcunix_create(sock, send_buf_size, recv_buf_size);
 *
 * Creates, registers, and returns a (rpc) unix based transporter.
 * Once *xprt is initialized, it is registered as a transporter
 * see (svc.h, xprt_register).  This routine returns
 * a NULL if a problem occurred.
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svcunix_create
 * binds it to an arbitrary port.  The routine then starts a unix
 * listener on the socket's associated port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 *
 * Since unix streams do buffered io similar to stdio, the caller can specify
 * how big the send and receive buffers are via the second and third parms;
 * 0 => use the system default.
 */
SVCXPRT *
svcunix_create(sock, sendsize, recvsize, path)
	register int sock;
	u_int sendsize;
	u_int recvsize;
	char *path;
{
	bool_t madesock = FALSE;
	register SVCXPRT *xprt;
	register struct unix_rendezvous *r;
	struct sockaddr_un addr;
	int len = sizeof(struct sockaddr_un);

	if (sock == RPC_ANYSOCK) {
		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
			perror("svc_unix.c - AF_UNIX socket creation problem");
			return ((SVCXPRT *)NULL);
		}
		madesock = TRUE;
	}
	memset(&addr, 0, sizeof (addr));
	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, path);
	len = strlen(addr.sun_path) + sizeof(addr.sun_family) +
		sizeof(addr.sun_len) + 1;
	addr.sun_len = len;

	bind(sock, (struct sockaddr *)&addr, len);

	if ((getsockname(sock, (struct sockaddr *)&addr, &len) != 0)  ||
	    (listen(sock, 2) != 0)) {
		perror("svc_unix.c - cannot getsockname or listen");
		if (madesock)
		       (void)_close(sock);
		return ((SVCXPRT *)NULL);
	}
	r = (struct unix_rendezvous *)mem_alloc(sizeof(*r));
	if (r == NULL) {
		(void) fprintf(stderr, "svcunix_create: out of memory\n");
		return (NULL);
	}
	r->sendsize = sendsize;
	r->recvsize = recvsize;
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		(void) fprintf(stderr, "svcunix_create: out of memory\n");
		return (NULL);
	}
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)r;
	xprt->xp_verf = _null_auth;
	xprt->xp_ops = &svcunix_rendezvous_op;
	xprt->xp_port = -1 /*ntohs(addr.sin_port)*/;
	xprt->xp_sock = sock;
	xprt_register(xprt);
	return (xprt);
}

/*
 * Like svunix_create(), except the routine takes any *open* UNIX file
 * descriptor as its first input.
 */
SVCXPRT *
svcunixfd_create(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{

	return (makefd_xprt(fd, sendsize, recvsize));
}

static SVCXPRT *
makefd_xprt(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{
	register SVCXPRT *xprt;
	register struct unix_conn *cd;

	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == (SVCXPRT *)NULL) {
		(void) fprintf(stderr, "svc_unix: makefd_xprt: out of memory\n");
		goto done;
	}
	cd = (struct unix_conn *)mem_alloc(sizeof(struct unix_conn));
	if (cd == (struct unix_conn *)NULL) {
		(void) fprintf(stderr, "svc_unix: makefd_xprt: out of memory\n");
		mem_free((char *) xprt, sizeof(SVCXPRT));
		xprt = (SVCXPRT *)NULL;
		goto done;
	}
	cd->strm_stat = XPRT_IDLE;
	xdrrec_create(&(cd->xdrs), sendsize, recvsize,
	    (caddr_t)xprt, readunix, writeunix);
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)cd;
	xprt->xp_verf.oa_base = cd->verf_body;
	xprt->xp_addrlen = 0;
	xprt->xp_ops = &svcunix_op;  /* truely deals with calls */
	xprt->xp_port = 0;  /* this is a connection, not a rendezvouser */
	xprt->xp_sock = fd;
	xprt_register(xprt);
    done:
	return (xprt);
}

static bool_t
rendezvous_request(xprt)
	register SVCXPRT *xprt;
{
	int sock;
	struct unix_rendezvous *r;
	struct sockaddr_un addr;
	struct sockaddr_in in_addr;
	int len;

	r = (struct unix_rendezvous *)xprt->xp_p1;
    again:
	len = sizeof(struct sockaddr_in);
	if ((sock = accept(xprt->xp_sock, (struct sockaddr *)&addr,
	    &len)) < 0) {
		if (errno == EINTR)
			goto again;
	       return (FALSE);
	}

	/*
	 * make a new transporter (re-uses xprt)
	 */
	bzero((char *)&in_addr, sizeof(in_addr));
	in_addr.sin_family = AF_UNIX;
	xprt = makefd_xprt(sock, r->sendsize, r->recvsize);
	xprt->xp_raddr = in_addr;
	xprt->xp_addrlen = len;
	return (FALSE); /* there is never an rpc msg to be processed */
}

static enum xprt_stat
rendezvous_stat()
{

	return (XPRT_IDLE);
}

static void
svcunix_destroy(xprt)
	register SVCXPRT *xprt;
{
	register struct unix_conn *cd = (struct unix_conn *)xprt->xp_p1;

	xprt_unregister(xprt);
	(void)_close(xprt->xp_sock);
	if (xprt->xp_port != 0) {
		/* a rendezvouser socket */
		xprt->xp_port = 0;
	} else {
		/* an actual connection socket */
		XDR_DESTROY(&(cd->xdrs));
	}
	mem_free((caddr_t)cd, sizeof(struct unix_conn));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}

/*
 * All read operations timeout after 35 seconds.
 * A timeout is fatal for the connection.
 */
static struct timeval wait_per_try = { 35, 0 };

/*
 * reads data from the unix conection.
 * any error is fatal and the connection is closed.
 * (And a read of zero bytes is a half closed stream => error.)
 *
 * Note: we have to be careful here not to allow ourselves to become
 * blocked too long in this routine. While we're waiting for data from one
 * client, another client may be trying to connect. To avoid this situation,
 * some code from svc_run() is transplanted here: the select() loop checks
 * all RPC descriptors including the one we want and calls svc_getreqset2()
 * to handle new requests if any are detected.
 */
static int
readunix(xprt, buf, len)
	register SVCXPRT *xprt;
	caddr_t buf;
	register int len;
{
	register int sock = xprt->xp_sock;
	struct timeval start, delta, tv;
	struct timeval tmp1, tmp2;
	fd_set *fds;
	extern fd_set		*__svc_fdset;
	extern int		__svc_fdsetsize;

	delta = wait_per_try;
	fds = NULL;
	gettimeofday(&start, NULL);
	do {
		int bytes = howmany(__svc_fdsetsize, NFDBITS) *
				sizeof(fd_mask);
		if (fds != NULL)
			free(fds);
		fds = (fd_set *)malloc(bytes);
		if (fds == NULL)
			goto fatal_err;
		memcpy(fds, __svc_fdset, bytes);

		/* XXX we know the other bits are still clear */
		FD_SET(sock, fds);
		tv = delta;	/* in case select() implements writeback */
		switch (select(svc_maxfd + 1, fds, NULL, NULL, &tv)) {
		case -1:
			memset(fds, 0, bytes);
			if (errno != EINTR)
				goto fatal_err;
			gettimeofday(&tmp1, NULL);
			timersub(&tmp1, &start, &tmp2);
			timersub(&wait_per_try, &tmp2, &tmp1);
			if (tmp1.tv_sec < 0 || !timerisset(&tmp1))
				goto fatal_err;
			delta = tmp1;
			continue;
		case 0:
			goto fatal_err;
		default:
			if (!FD_ISSET(sock, fds)) {
				svc_getreqset2(fds, svc_maxfd + 1);
				gettimeofday(&tmp1, NULL);
				timersub(&tmp1, &start, &tmp2);
				timersub(&wait_per_try, &tmp2, &tmp1);
				if (tmp1.tv_sec < 0 || !timerisset(&tmp1))
					goto fatal_err;
				delta = tmp1;
				continue;
			}
		}
	} while (!FD_ISSET(sock, fds));
	if ((len = __msgread(sock, buf, len)) > 0) {
		if (fds != NULL)
			free(fds);
		return (len);
	}
fatal_err:
	((struct unix_conn *)(xprt->xp_p1))->strm_stat = XPRT_DIED;
	if (fds != NULL)
		free(fds);
	return (-1);
}

/*
 * writes data to the unix connection.
 * Any error is fatal and the connection is closed.
 */
static int
writeunix(xprt, buf, len)
	register SVCXPRT *xprt;
	caddr_t buf;
	int len;
{
	register int i, cnt;

	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		if ((i = __msgwrite(xprt->xp_sock, buf, cnt)) < 0) {
			((struct unix_conn *)(xprt->xp_p1))->strm_stat =
			    XPRT_DIED;
			return (-1);
		}
	}
	return (len);
}

static enum xprt_stat
svcunix_stat(xprt)
	SVCXPRT *xprt;
{
	register struct unix_conn *cd =
	    (struct unix_conn *)(xprt->xp_p1);

	if (cd->strm_stat == XPRT_DIED)
		return (XPRT_DIED);
	if (! xdrrec_eof(&(cd->xdrs)))
		return (XPRT_MOREREQS);
	return (XPRT_IDLE);
}

static bool_t
svcunix_recv(xprt, msg)
	SVCXPRT *xprt;
	register struct rpc_msg *msg;
{
	register struct unix_conn *cd =
	    (struct unix_conn *)(xprt->xp_p1);
	register XDR *xdrs = &(cd->xdrs);

	xdrs->x_op = XDR_DECODE;
	(void)xdrrec_skiprecord(xdrs);
	if (xdr_callmsg(xdrs, msg)) {
		cd->x_id = msg->rm_xid;
		/* set up verifiers */
		msg->rm_call.cb_verf.oa_flavor = AUTH_UNIX;
		msg->rm_call.cb_verf.oa_base = (caddr_t)&cm;
		msg->rm_call.cb_verf.oa_length = sizeof(cm);
		return (TRUE);
	}
	cd->strm_stat = XPRT_DIED;	/* XXXX */
	return (FALSE);
}

static bool_t
svcunix_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{

	return ((*xdr_args)(&(((struct unix_conn *)(xprt->xp_p1))->xdrs), args_ptr));
}

static bool_t
svcunix_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	register XDR *xdrs =
	    &(((struct unix_conn *)(xprt->xp_p1))->xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static bool_t
svcunix_reply(xprt, msg)
	SVCXPRT *xprt;
	register struct rpc_msg *msg;
{
	register struct unix_conn *cd =
	    (struct unix_conn *)(xprt->xp_p1);
	register XDR *xdrs = &(cd->xdrs);
	register bool_t stat;

	xdrs->x_op = XDR_ENCODE;
	msg->rm_xid = cd->x_id;
	stat = xdr_replymsg(xdrs, msg);
	(void)xdrrec_endofrecord(xdrs, TRUE);
	return (stat);
}
