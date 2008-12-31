/*	$NetBSD: svc_dg.c,v 1.4 2000/07/06 03:10:35 christos Exp $	*/

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

#if defined(LIBC_SCCS) && !defined(lint)
#ident	"@(#)svc_dg.c	1.17	94/04/24 SMI"
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/rpc/svc_dg.c,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * svc_dg.c, Server side for connectionless RPC.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <rpc/rpc.h>

#include <rpc/rpc_com.h>

static enum xprt_stat svc_dg_stat(SVCXPRT *);
static bool_t svc_dg_recv(SVCXPRT *, struct rpc_msg *);
static bool_t svc_dg_reply(SVCXPRT *, struct rpc_msg *);
static bool_t svc_dg_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t svc_dg_freeargs(SVCXPRT *, xdrproc_t, void *);
static void svc_dg_destroy(SVCXPRT *);
static bool_t svc_dg_control(SVCXPRT *, const u_int, void *);
static void svc_dg_soupcall(struct socket *so, void *arg, int waitflag);

static struct xp_ops svc_dg_ops = {
	.xp_recv =	svc_dg_recv,
	.xp_stat =	svc_dg_stat,
	.xp_getargs =	svc_dg_getargs,
	.xp_reply =	svc_dg_reply,
	.xp_freeargs =	svc_dg_freeargs,
	.xp_destroy =	svc_dg_destroy,
	.xp_control =	svc_dg_control,
};

/*
 * Usage:
 *	xprt = svc_dg_create(sock, sendsize, recvsize);
 * Does other connectionless specific initializations.
 * Once *xprt is initialized, it is registered.
 * see (svc.h, xprt_register). If recvsize or sendsize are 0 suitable
 * system defaults are chosen.
 * The routines returns NULL if a problem occurred.
 */
static const char svc_dg_str[] = "svc_dg_create: %s";
static const char svc_dg_err1[] = "could not get transport information";
static const char svc_dg_err2[] = "transport does not support data transfer";
static const char __no_mem_str[] = "out of memory";

SVCXPRT *
svc_dg_create(SVCPOOL *pool, struct socket *so, size_t sendsize,
    size_t recvsize)
{
	SVCXPRT *xprt;
	struct __rpc_sockinfo si;
	struct sockaddr* sa;
	int error;

	if (!__rpc_socket2sockinfo(so, &si)) {
		printf(svc_dg_str, svc_dg_err1);
		return (NULL);
	}
	/*
	 * Find the receive and the send size
	 */
	sendsize = __rpc_get_t_size(si.si_af, si.si_proto, (int)sendsize);
	recvsize = __rpc_get_t_size(si.si_af, si.si_proto, (int)recvsize);
	if ((sendsize == 0) || (recvsize == 0)) {
		printf(svc_dg_str, svc_dg_err2);
		return (NULL);
	}

	xprt = mem_alloc(sizeof (SVCXPRT));
	memset(xprt, 0, sizeof (SVCXPRT));
	mtx_init(&xprt->xp_lock, "xprt->xp_lock", NULL, MTX_DEF);
	xprt->xp_pool = pool;
	xprt->xp_socket = so;
	xprt->xp_p1 = NULL;
	xprt->xp_p2 = NULL;
	xprt->xp_ops = &svc_dg_ops;

	error = so->so_proto->pr_usrreqs->pru_sockaddr(so, &sa);
	if (error)
		goto freedata;

	xprt->xp_ltaddr.buf = mem_alloc(sizeof (struct sockaddr_storage));
	xprt->xp_ltaddr.maxlen = sizeof (struct sockaddr_storage);
	xprt->xp_ltaddr.len = sa->sa_len;
	memcpy(xprt->xp_ltaddr.buf, sa, sa->sa_len);
	free(sa, M_SONAME);

	xprt->xp_rtaddr.buf = mem_alloc(sizeof (struct sockaddr_storage));
	xprt->xp_rtaddr.maxlen = sizeof (struct sockaddr_storage);
	xprt->xp_rtaddr.len = 0;

	xprt_register(xprt);

	SOCKBUF_LOCK(&so->so_rcv);
	so->so_upcallarg = xprt;
	so->so_upcall = svc_dg_soupcall;
	so->so_rcv.sb_flags |= SB_UPCALL;
	SOCKBUF_UNLOCK(&so->so_rcv);

	return (xprt);
freedata:
	(void) printf(svc_dg_str, __no_mem_str);
	if (xprt) {
		(void) mem_free(xprt, sizeof (SVCXPRT));
	}
	return (NULL);
}

/*ARGSUSED*/
static enum xprt_stat
svc_dg_stat(SVCXPRT *xprt)
{

	return (XPRT_IDLE);
}

static bool_t
svc_dg_recv(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct uio uio;
	struct sockaddr *raddr;
	struct mbuf *mreq;
	int error, rcvflag;

	/*
	 * The socket upcall calls xprt_active() which will eventually
	 * cause the server to call us here. We attempt to read a
	 * packet from the socket and process it. If the read fails,
	 * we have drained all pending requests so we call
	 * xprt_inactive().
	 *
	 * The lock protects us in the case where a new packet arrives
	 * on the socket after our call to soreceive fails with
	 * EWOULDBLOCK - the call to xprt_active() in the upcall will
	 * happen only after our call to xprt_inactive() which ensures
	 * that we will remain active. It might be possible to use
	 * SOCKBUF_LOCK for this - its not clear to me what locks are
	 * held during the upcall.
	 */
	mtx_lock(&xprt->xp_lock);

	uio.uio_resid = 1000000000;
	uio.uio_td = curthread;
	mreq = NULL;
	rcvflag = MSG_DONTWAIT;
	error = soreceive(xprt->xp_socket, &raddr, &uio, &mreq, NULL, &rcvflag);

	if (error == EWOULDBLOCK) {
		xprt_inactive(xprt);
		mtx_unlock(&xprt->xp_lock);
		return (FALSE);
	}

	if (error) {
		SOCKBUF_LOCK(&xprt->xp_socket->so_rcv);
		xprt->xp_socket->so_upcallarg = NULL;
		xprt->xp_socket->so_upcall = NULL;
		xprt->xp_socket->so_rcv.sb_flags &= ~SB_UPCALL;
		SOCKBUF_UNLOCK(&xprt->xp_socket->so_rcv);
		xprt_inactive(xprt);
		mtx_unlock(&xprt->xp_lock);
		return (FALSE);
	}

	mtx_unlock(&xprt->xp_lock);

	KASSERT(raddr->sa_len < xprt->xp_rtaddr.maxlen,
	    ("Unexpected remote address length"));
	memcpy(xprt->xp_rtaddr.buf, raddr, raddr->sa_len);
	xprt->xp_rtaddr.len = raddr->sa_len;
	free(raddr, M_SONAME);

	xdrmbuf_create(&xprt->xp_xdrreq, mreq, XDR_DECODE);
	if (! xdr_callmsg(&xprt->xp_xdrreq, msg)) {
		XDR_DESTROY(&xprt->xp_xdrreq);
		return (FALSE);
	}
	xprt->xp_xid = msg->rm_xid;

	return (TRUE);
}

static bool_t
svc_dg_reply(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct mbuf *mrep;
	bool_t stat = FALSE;
	int error;

	MGETHDR(mrep, M_WAIT, MT_DATA);
	MCLGET(mrep, M_WAIT);
	mrep->m_len = 0;

	xdrmbuf_create(&xprt->xp_xdrrep, mrep, XDR_ENCODE);
	msg->rm_xid = xprt->xp_xid;
	if (xdr_replymsg(&xprt->xp_xdrrep, msg)) {
		m_fixhdr(mrep);
		error = sosend(xprt->xp_socket,
		    (struct sockaddr *) xprt->xp_rtaddr.buf, NULL, mrep, NULL,
		    0, curthread);
		if (!error) {
			stat = TRUE;
		}
	} else {
		m_freem(mrep);
	}

	/*
	 * This frees the request mbuf chain as well. The reply mbuf
	 * chain was consumed by sosend.
	 */
	XDR_DESTROY(&xprt->xp_xdrreq);
	XDR_DESTROY(&xprt->xp_xdrrep);
	xprt->xp_p2 = NULL;

	return (stat);
}

static bool_t
svc_dg_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
{

	return (xdr_args(&xprt->xp_xdrreq, args_ptr));
}

static bool_t
svc_dg_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
{
	XDR xdrs;

	/*
	 * Free the request mbuf here - this allows us to handle
	 * protocols where not all requests have replies
	 * (i.e. NLM). Note that xdrmbuf_destroy handles being called
	 * twice correctly - the mbuf will only be freed once.
	 */
	XDR_DESTROY(&xprt->xp_xdrreq);

	xdrs.x_op = XDR_FREE;
	return (xdr_args(&xdrs, args_ptr));
}

static void
svc_dg_destroy(SVCXPRT *xprt)
{
	SOCKBUF_LOCK(&xprt->xp_socket->so_rcv);
	xprt->xp_socket->so_upcallarg = NULL;
	xprt->xp_socket->so_upcall = NULL;
	xprt->xp_socket->so_rcv.sb_flags &= ~SB_UPCALL;
	SOCKBUF_UNLOCK(&xprt->xp_socket->so_rcv);

	xprt_unregister(xprt);

	mtx_destroy(&xprt->xp_lock);
	if (xprt->xp_socket)
		(void)soclose(xprt->xp_socket);

	if (xprt->xp_rtaddr.buf)
		(void) mem_free(xprt->xp_rtaddr.buf, xprt->xp_rtaddr.maxlen);
	if (xprt->xp_ltaddr.buf)
		(void) mem_free(xprt->xp_ltaddr.buf, xprt->xp_ltaddr.maxlen);
	(void) mem_free(xprt, sizeof (SVCXPRT));
}

static bool_t
/*ARGSUSED*/
svc_dg_control(xprt, rq, in)
	SVCXPRT *xprt;
	const u_int	rq;
	void		*in;
{

	return (FALSE);
}

static void
svc_dg_soupcall(struct socket *so, void *arg, int waitflag)
{
	SVCXPRT *xprt = (SVCXPRT *) arg;

	mtx_lock(&xprt->xp_lock);
	xprt_active(xprt);
	mtx_unlock(&xprt->xp_lock);
}
