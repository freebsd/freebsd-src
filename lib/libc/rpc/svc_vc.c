/*	$NetBSD: svc_vc.c,v 1.7 2000/08/03 00:01:53 fvdl Exp $	*/
/*	$FreeBSD$ */

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

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid = "@(#)svc_tcp.c 1.21 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc_tcp.c	2.2 88/08/01 4.0 RPCSRC";
#endif

/*
 * svc_vc.c, Server side for Connection Oriented based RPC. 
 *
 * Actually implements two flavors of transporter -
 * a tcp rendezvouser (a listner and connection establisher)
 * and a record/tcp stream.
 */

#include "namespace.h"
#include "reentrant.h"
#include <sys/types.h>
#include <sys/param.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rpc/rpc.h>

#include "rpc_com.h"
#include "un-namespace.h"

struct cmessage {
        struct cmsghdr cmsg;
        struct cmsgcred cmcred;
};

static SVCXPRT *makefd_xprt __P((int, u_int, u_int));
static bool_t rendezvous_request __P((SVCXPRT *, struct rpc_msg *));
static enum xprt_stat rendezvous_stat __P((SVCXPRT *));
static void svc_vc_destroy __P((SVCXPRT *));
static int read_vc __P((caddr_t, caddr_t, int));
static int write_vc __P((caddr_t, caddr_t, int));
static enum xprt_stat svc_vc_stat __P((SVCXPRT *));
static bool_t svc_vc_recv __P((SVCXPRT *, struct rpc_msg *));
static bool_t svc_vc_getargs __P((SVCXPRT *, xdrproc_t, caddr_t));
static bool_t svc_vc_freeargs __P((SVCXPRT *, xdrproc_t, caddr_t));
static bool_t svc_vc_reply __P((SVCXPRT *, struct rpc_msg *));
static void svc_vc_rendezvous_ops __P((SVCXPRT *));
static void svc_vc_ops __P((SVCXPRT *));
static bool_t svc_vc_control __P((SVCXPRT *xprt, const u_int rq, void *in));
static int __msgread_withcred(int, void *, size_t, struct cmessage *);
static int __msgwrite(int, void *, size_t);

struct cf_rendezvous { /* kept in xprt->xp_p1 for rendezvouser */
	u_int sendsize;
	u_int recvsize;
};

struct cf_conn {  /* kept in xprt->xp_p1 for actual connection */
	enum xprt_stat strm_stat;
	u_int32_t x_id;
	XDR xdrs;
	char verf_body[MAX_AUTH_BYTES];
};

/*
 * Usage:
 *	xprt = svc_vc_create(sock, send_buf_size, recv_buf_size);
 *
 * Creates, registers, and returns a (rpc) tcp based transporter.
 * Once *xprt is initialized, it is registered as a transporter
 * see (svc.h, xprt_register).  This routine returns
 * a NULL if a problem occurred.
 *
 * The filedescriptor passed in is expected to refer to a bound, but
 * not yet connected socket.
 *
 * Since streams do buffered io similar to stdio, the caller can specify
 * how big the send and receive buffers are via the second and third parms;
 * 0 => use the system default.
 */
SVCXPRT *
svc_vc_create(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{
	SVCXPRT *xprt;
	struct cf_rendezvous *r = NULL;
	struct __rpc_sockinfo si;
	struct sockaddr_storage sslocal;
	socklen_t slen;
	int one = 1;

	r = mem_alloc(sizeof(*r));
	if (r == NULL) {
		warnx("svc_vc_create: out of memory");
		goto cleanup_svc_vc_create;
	}
	if (!__rpc_fd2sockinfo(fd, &si))
		return NULL;
	r->sendsize = __rpc_get_t_size(si.si_af, si.si_proto, (int)sendsize);
	r->recvsize = __rpc_get_t_size(si.si_af, si.si_proto, (int)recvsize);
	xprt = mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		warnx("svc_vc_create: out of memory");
		goto cleanup_svc_vc_create;
	}
	xprt->xp_tp = NULL;
	xprt->xp_p1 = (caddr_t)(void *)r;
	xprt->xp_p2 = NULL;
	xprt->xp_p3 = NULL;
	xprt->xp_verf = _null_auth;
	svc_vc_rendezvous_ops(xprt);
	xprt->xp_port = (u_short)-1;	/* It is the rendezvouser */
	xprt->xp_fd = fd;

	slen = sizeof (struct sockaddr_storage);
	if (_getsockname(fd, (struct sockaddr *)(void *)&sslocal, &slen) < 0) {
		warnx("svc_vc_create: could not retrieve local addr");
		goto cleanup_svc_vc_create;
	}

	xprt->xp_ltaddr.maxlen = xprt->xp_ltaddr.len = sslocal.ss_len;
	xprt->xp_ltaddr.buf = mem_alloc((size_t)sslocal.ss_len);
	if (xprt->xp_ltaddr.buf == NULL) {
		warnx("svc_vc_create: no mem for local addr");
		goto cleanup_svc_vc_create;
	}
	memcpy(xprt->xp_ltaddr.buf, &sslocal, (size_t)sslocal.ss_len);

	xprt->xp_rtaddr.maxlen = sizeof (struct sockaddr_storage);
	xprt_register(xprt);
	return (xprt);
cleanup_svc_vc_create:
	if (r != NULL)
		mem_free(r, sizeof(*r));
	return (NULL);
}

/*
 * Like svtcp_create(), except the routine takes any *open* UNIX file
 * descriptor as its first input.
 */
SVCXPRT *
svc_fd_create(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{
	struct sockaddr_storage ss;
	socklen_t slen;
	SVCXPRT *ret;

	assert(fd != -1);

	ret = makefd_xprt(fd, sendsize, recvsize);
	if (ret == NULL)
		return NULL;

	slen = sizeof (struct sockaddr_storage);
	if (_getsockname(fd, (struct sockaddr *)(void *)&ss, &slen) < 0) {
		warnx("svc_fd_create: could not retrieve local addr");
		goto freedata;
	}
	ret->xp_ltaddr.maxlen = ret->xp_ltaddr.len = ss.ss_len;
	ret->xp_ltaddr.buf = mem_alloc((size_t)ss.ss_len);
	if (ret->xp_ltaddr.buf == NULL) {
		warnx("svc_fd_create: no mem for local addr");
		goto freedata;
	}
	memcpy(ret->xp_ltaddr.buf, &ss, (size_t)ss.ss_len);

	slen = sizeof (struct sockaddr_storage);
	if (_getpeername(fd, (struct sockaddr *)(void *)&ss, &slen) < 0) {
		warnx("svc_fd_create: could not retrieve remote addr");
		goto freedata;
	}
	ret->xp_rtaddr.maxlen = ret->xp_rtaddr.len = ss.ss_len;
	ret->xp_rtaddr.buf = mem_alloc((size_t)ss.ss_len);
	if (ret->xp_rtaddr.buf == NULL) {
		warnx("svc_fd_create: no mem for local addr");
		goto freedata;
	}
	memcpy(ret->xp_rtaddr.buf, &ss, (size_t)ss.ss_len);
#ifdef PORTMAP
	if (ss.ss_family == AF_INET) {
		ret->xp_raddr = *(struct sockaddr_in *)ret->xp_rtaddr.buf;
		ret->xp_addrlen = sizeof (struct sockaddr_in);
	}
#endif				/* PORTMAP */

	return ret;

freedata:
	if (ret->xp_ltaddr.buf != NULL)
		mem_free(ret->xp_ltaddr.buf, rep->xp_ltaddr.maxlen);

	return NULL;
}

static SVCXPRT *
makefd_xprt(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{
	SVCXPRT *xprt;
	struct cf_conn *cd;
	const char *netid;
	struct __rpc_sockinfo si;
 
	assert(fd != -1);

	xprt = mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		warnx("svc_vc: makefd_xprt: out of memory");
		goto done;
	}
	memset(xprt, 0, sizeof *xprt);
	cd = mem_alloc(sizeof(struct cf_conn));
	if (cd == NULL) {
		warnx("svc_tcp: makefd_xprt: out of memory");
		mem_free(xprt, sizeof(SVCXPRT));
		xprt = NULL;
		goto done;
	}
	cd->strm_stat = XPRT_IDLE;
	xdrrec_create(&(cd->xdrs), sendsize, recvsize,
	    (caddr_t)(void *)xprt, read_vc, write_vc);
	xprt->xp_p1 = (caddr_t)(void *)cd;
	xprt->xp_verf.oa_base = cd->verf_body;
	svc_vc_ops(xprt);  /* truely deals with calls */
	xprt->xp_port = 0;  /* this is a connection, not a rendezvouser */
	xprt->xp_fd = fd;
        if (__rpc_fd2sockinfo(fd, &si) && __rpc_sockinfo2netid(&si, &netid))
		xprt->xp_netid = strdup(netid);

	xprt_register(xprt);
done:
	return (xprt);
}

/*ARGSUSED*/
static bool_t
rendezvous_request(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	int sock;
	struct cf_rendezvous *r;
	struct sockaddr_storage addr;
	socklen_t len;
	struct __rpc_sockinfo si;

	assert(xprt != NULL);
	assert(msg != NULL);

	r = (struct cf_rendezvous *)xprt->xp_p1;
again:
	len = sizeof addr;
	if ((sock = _accept(xprt->xp_fd, (struct sockaddr *)(void *)&addr,
	    &len)) < 0) {
		if (errno == EINTR)
			goto again;
	       return (FALSE);
	}
	/*
	 * make a new transporter (re-uses xprt)
	 */
	xprt = makefd_xprt(sock, r->sendsize, r->recvsize);
	xprt->xp_rtaddr.buf = mem_alloc(len);
	if (xprt->xp_rtaddr.buf == NULL)
		return (FALSE);
	memcpy(xprt->xp_rtaddr.buf, &addr, len);
	xprt->xp_rtaddr.len = len;
	if (addr.ss_family == AF_LOCAL) {
		xprt->xp_raddr = *(struct sockaddr_in *)xprt->xp_rtaddr.buf;
		xprt->xp_addrlen = sizeof (struct sockaddr_in);
	}
#ifdef PORTMAP
	if (addr.ss_family == AF_INET) {
		xprt->xp_raddr = *(struct sockaddr_in *)xprt->xp_rtaddr.buf;
		xprt->xp_addrlen = sizeof (struct sockaddr_in);
	}
#endif				/* PORTMAP */
	if (__rpc_fd2sockinfo(sock, &si) && si.si_proto == IPPROTO_TCP) {
		len = 1;
		/* XXX fvdl - is this useful? */
		_setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &len, sizeof (len));
	}
	return (FALSE); /* there is never an rpc msg to be processed */
}

/*ARGSUSED*/
static enum xprt_stat
rendezvous_stat(xprt)
	SVCXPRT *xprt;
{

	return (XPRT_IDLE);
}

static void
svc_vc_destroy(xprt)
	SVCXPRT *xprt;
{
	struct cf_conn *cd;
	struct cf_rendezvous *r;

	assert(xprt != NULL);

	cd = (struct cf_conn *)xprt->xp_p1;

	xprt_unregister(xprt);
	if (xprt->xp_fd != RPC_ANYFD)
		(void)_close(xprt->xp_fd);
	if (xprt->xp_port != 0) {
		/* a rendezvouser socket */
		r = (struct cf_rendezvous *)xprt->xp_p1;
		mem_free(r, sizeof (struct cf_rendezvous));
		xprt->xp_port = 0;
	} else {
		/* an actual connection socket */
		XDR_DESTROY(&(cd->xdrs));
		mem_free(cd, sizeof(struct cf_conn));
	}
	if (xprt->xp_rtaddr.buf)
		mem_free(xprt->xp_rtaddr.buf, xprt->xp_rtaddr.maxlen);
	if (xprt->xp_ltaddr.buf)
		mem_free(xprt->xp_ltaddr.buf, xprt->xp_ltaddr.maxlen);
	if (xprt->xp_tp)
		free(xprt->xp_tp);
	if (xprt->xp_netid)
		free(xprt->xp_netid);
	mem_free(xprt, sizeof(SVCXPRT));
}

/*ARGSUSED*/
static bool_t
svc_vc_control(xprt, rq, in)
	SVCXPRT *xprt;
	const u_int rq;
	void *in;
{
	return (FALSE);
}

/*
 * reads data from the tcp or uip connection.
 * any error is fatal and the connection is closed.
 * (And a read of zero bytes is a half closed stream => error.)
 * All read operations timeout after 35 seconds.  A timeout is
 * fatal for the connection.
 */
static int
read_vc(xprtp, buf, len)
	caddr_t xprtp;
	caddr_t buf;
	int len;
{
	SVCXPRT *xprt;
	int sock;
	int milliseconds = 35 * 1000;
	struct pollfd pollfd;
	struct sockaddr *sa;
	struct cmessage *cm;

	xprt = (SVCXPRT *)(void *)xprtp;
	assert(xprt != NULL);

	sock = xprt->xp_fd;

	do {
		pollfd.fd = sock;
		pollfd.events = POLLIN;
		pollfd.revents = 0;
		switch (_poll(&pollfd, 1, milliseconds)) {
		case -1:
			if (errno == EINTR)
				continue;
			/*FALLTHROUGH*/
		case 0:
			goto fatal_err;

		default:
			break;
		}
	} while ((pollfd.revents & POLLIN) == 0);

	cm = NULL;
	sa = (struct sockaddr *)xprt->xp_rtaddr.buf;
	if (sa->sa_family == AF_LOCAL) {
		cm = (struct cmessage *)xprt->xp_verf.oa_base;
		if ((len = __msgread_withcred(sock, buf, len, cm)) > 0) {
			xprt->xp_p2 = &cm->cmcred;
			return (len);
		} else
			goto fatal_err;
	} else {
		if ((len = _read(sock, buf, (size_t)len)) > 0)
			return (len);
	}

fatal_err:
	((struct cf_conn *)(xprt->xp_p1))->strm_stat = XPRT_DIED;
	return (-1);
}

/*
 * writes data to the tcp connection.
 * Any error is fatal and the connection is closed.
 */
static int
write_vc(xprtp, buf, len)
	caddr_t xprtp;
	caddr_t buf;
	int len;
{
	SVCXPRT *xprt;
	int i, cnt;
	struct sockaddr *sa;

	xprt = (SVCXPRT *)(void *)xprtp;
	assert(xprt != NULL);
	
	sa = (struct sockaddr *)xprt->xp_rtaddr.buf;
        if (sa->sa_family == AF_LOCAL) {
		for (cnt = len; cnt > 0; cnt -= i, buf += i) {
			if ((i = __msgwrite(xprt->xp_fd, buf,
			    (size_t)cnt)) < 0) {
				((struct cf_conn *)(xprt->xp_p1))->strm_stat =
				    XPRT_DIED;
				return (-1);
			}
		}
	} else {
		for (cnt = len; cnt > 0; cnt -= i, buf += i) {
			if ((i = _write(xprt->xp_fd, buf,
			    (size_t)cnt)) < 0) {
				((struct cf_conn *)(xprt->xp_p1))->strm_stat =
				    XPRT_DIED;
				return (-1);
			}
		}
	}

	return (len);
}

static enum xprt_stat
svc_vc_stat(xprt)
	SVCXPRT *xprt;
{
	struct cf_conn *cd;

	assert(xprt != NULL);

	cd = (struct cf_conn *)(xprt->xp_p1);

	if (cd->strm_stat == XPRT_DIED)
		return (XPRT_DIED);
	if (! xdrrec_eof(&(cd->xdrs)))
		return (XPRT_MOREREQS);
	return (XPRT_IDLE);
}

static bool_t
svc_vc_recv(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct cf_conn *cd;
	XDR *xdrs;

	assert(xprt != NULL);
	assert(msg != NULL);

	cd = (struct cf_conn *)(xprt->xp_p1);
	xdrs = &(cd->xdrs);

	xdrs->x_op = XDR_DECODE;
	(void)xdrrec_skiprecord(xdrs);
	if (xdr_callmsg(xdrs, msg)) {
		cd->x_id = msg->rm_xid;
		return (TRUE);
	}
	cd->strm_stat = XPRT_DIED;
	return (FALSE);
}

static bool_t
svc_vc_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{

	assert(xprt != NULL);
	/* args_ptr may be NULL */
	return ((*xdr_args)(&(((struct cf_conn *)(xprt->xp_p1))->xdrs),
	    args_ptr));
}

static bool_t
svc_vc_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	XDR *xdrs;

	assert(xprt != NULL);
	/* args_ptr may be NULL */

	xdrs = &(((struct cf_conn *)(xprt->xp_p1))->xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static bool_t
svc_vc_reply(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct cf_conn *cd;
	XDR *xdrs;
	bool_t stat;

	assert(xprt != NULL);
	assert(msg != NULL);

	cd = (struct cf_conn *)(xprt->xp_p1);
	xdrs = &(cd->xdrs);

	xdrs->x_op = XDR_ENCODE;
	msg->rm_xid = cd->x_id;
	stat = xdr_replymsg(xdrs, msg);
	(void)xdrrec_endofrecord(xdrs, TRUE);
	return (stat);
}

static void
svc_vc_ops(xprt)
	SVCXPRT *xprt;
{
	static struct xp_ops ops;
	static struct xp_ops2 ops2;
	extern mutex_t ops_lock;

/* VARIABLES PROTECTED BY ops_lock: ops, ops2 */

	mutex_lock(&ops_lock);
	if (ops.xp_recv == NULL) {
		ops.xp_recv = svc_vc_recv;
		ops.xp_stat = svc_vc_stat;
		ops.xp_getargs = svc_vc_getargs;
		ops.xp_reply = svc_vc_reply;
		ops.xp_freeargs = svc_vc_freeargs;
		ops.xp_destroy = svc_vc_destroy;
		ops2.xp_control = svc_vc_control;
	}
	xprt->xp_ops = &ops;
	xprt->xp_ops2 = &ops2;
	mutex_unlock(&ops_lock);
}

static void
svc_vc_rendezvous_ops(xprt)
	SVCXPRT *xprt;
{
	static struct xp_ops ops;
	static struct xp_ops2 ops2;
	extern mutex_t ops_lock;

	mutex_lock(&ops_lock);
	if (ops.xp_recv == NULL) {
		ops.xp_recv = rendezvous_request;
		ops.xp_stat = rendezvous_stat;
		ops.xp_getargs =
		    (bool_t (*) __P((SVCXPRT *, xdrproc_t, caddr_t)))abort;
		ops.xp_reply =
		    (bool_t (*) __P((SVCXPRT *, struct rpc_msg *)))abort;
		ops.xp_freeargs =
		    (bool_t (*) __P((SVCXPRT *, xdrproc_t, caddr_t)))abort,
		ops.xp_destroy = svc_vc_destroy;
		ops2.xp_control = svc_vc_control;
	}
	xprt->xp_ops = &ops;
	xprt->xp_ops2 = &ops2;
	mutex_unlock(&ops_lock);
}

int
__msgread_withcred(sock, buf, cnt, cmp)
	int sock;
	void *buf;
	size_t cnt;
	struct cmessage *cmp;
{
	struct iovec iov[1];
	struct msghdr msg;
	union {
		struct cmsghdr cmsg;
		char control[CMSG_SPACE(sizeof(struct cmsgcred))];
	} cm;
	int ret;

 
	bzero(&cm, sizeof(cm));
	iov[0].iov_base = buf;
	iov[0].iov_len = cnt;
 
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = &cm;
	msg.msg_controllen = CMSG_SPACE(sizeof(struct cmsgcred));
	msg.msg_flags = 0;
 
	ret = _recvmsg(sock, &msg, 0);
	bcopy(&cm.cmsg, &cmp->cmsg, sizeof(cmp->cmsg));
	bcopy(CMSG_DATA(&cm), &cmp->cmcred, sizeof(cmp->cmcred));

	if (msg.msg_controllen == 0 ||
	   (msg.msg_flags & MSG_CTRUNC) != 0)
		return (-1);

	return (ret);
}

static int
__msgwrite(sock, buf, cnt)
	int sock;
	void *buf;
	size_t cnt;
{
	struct iovec iov[1];
	struct msghdr msg;
	struct cmessage cm;
 
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

	return(_sendmsg(sock, &msg, 0));
}

/*
 * Get the effective UID of the sending process. Used by rpcbind and keyserv
 * (AF_LOCAL).
 */
int
__rpc_get_local_uid(SVCXPRT *transp, uid_t *uid)
{
	struct cmsgcred *cmcred;
	struct cmessage *cm;
	struct cmsghdr *cmp;
  
	cm = (struct cmessage *)transp->xp_verf.oa_base;
	
	if (cm == NULL)
		return (-1);
	cmp = &cm->cmsg;
	if (cmp == NULL || cmp->cmsg_level != SOL_SOCKET ||
	   cmp->cmsg_type != SCM_CREDS)
		return (-1);
 
	cmcred = __svc_getcallercreds(transp);
	if (cmcred == NULL)
		return (-1); 
	*uid = cmcred->cmcred_euid;
	return (0);
}
