/*	$NetBSD: svc_dg.c,v 1.4 2000/07/06 03:10:35 christos Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2009, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * - Neither the name of Sun Microsystems, Inc. nor the names of its 
 *   contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

/*
 * svc_dg.c, Server side for connectionless RPC.
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <net/vnet.h>

#include <rpc/rpc.h>

#include <rpc/rpc_com.h>

static enum xprt_stat svc_dg_stat(SVCXPRT *);
static bool_t svc_dg_recv(SVCXPRT *, struct rpc_msg *,
    struct sockaddr **, struct mbuf **);
static bool_t svc_dg_reply(SVCXPRT *, struct rpc_msg *,
    struct sockaddr *, struct mbuf *, uint32_t *);
static void svc_dg_destroy(SVCXPRT *);
static bool_t svc_dg_control(SVCXPRT *, const u_int, void *);
static int svc_dg_soupcall(struct socket *so, void *arg, int waitflag);

static const struct xp_ops svc_dg_ops = {
	.xp_recv =	svc_dg_recv,
	.xp_stat =	svc_dg_stat,
	.xp_reply =	svc_dg_reply,
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
	int error;

	if (jailed(curthread->td_ucred))
		return (NULL);
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

	xprt = svc_xprt_alloc();
	sx_init(&xprt->xp_lock, "xprt->xp_lock");
	xprt->xp_pool = pool;
	xprt->xp_socket = so;
	xprt->xp_p1 = NULL;
	xprt->xp_p2 = NULL;
	xprt->xp_ops = &svc_dg_ops;

	xprt->xp_ltaddr.ss_len = sizeof(xprt->xp_ltaddr);
	error = sosockaddr(so, (struct sockaddr *)&xprt->xp_ltaddr);
	if (error)
		goto freedata;

	xprt_register(xprt);

	SOCK_RECVBUF_LOCK(so);
	soupcall_set(so, SO_RCV, svc_dg_soupcall, xprt);
	SOCK_RECVBUF_UNLOCK(so);

	return (xprt);
freedata:
	(void) printf(svc_dg_str, __no_mem_str);
	svc_xprt_free(xprt);

	return (NULL);
}

/*ARGSUSED*/
static enum xprt_stat
svc_dg_stat(SVCXPRT *xprt)
{

	if (soreadable(xprt->xp_socket))
		return (XPRT_MOREREQS);

	return (XPRT_IDLE);
}

static bool_t
svc_dg_recv(SVCXPRT *xprt, struct rpc_msg *msg,
    struct sockaddr **addrp, struct mbuf **mp)
{
	struct uio uio;
	struct sockaddr *raddr;
	struct mbuf *mreq;
	XDR xdrs;
	int error, rcvflag;

	/*
	 * Serialise access to the socket.
	 */
	sx_xlock(&xprt->xp_lock);

	/*
	 * The socket upcall calls xprt_active() which will eventually
	 * cause the server to call us here. We attempt to read a
	 * packet from the socket and process it. If the read fails,
	 * we have drained all pending requests so we call
	 * xprt_inactive().
	 */
	uio.uio_resid = 1000000000;
	uio.uio_td = curthread;
	mreq = NULL;
	rcvflag = MSG_DONTWAIT;
	error = soreceive(xprt->xp_socket, &raddr, &uio, &mreq, NULL, &rcvflag);

	if (error == EWOULDBLOCK) {
		/*
		 * We must re-test for readability after taking the
		 * lock to protect us in the case where a new packet
		 * arrives on the socket after our call to soreceive
		 * fails with EWOULDBLOCK. The pool lock protects us
		 * from racing the upcall after our soreadable() call
		 * returns false.
		 */
		SOCK_RECVBUF_LOCK(xprt->xp_socket);
		if (!soreadable(xprt->xp_socket))
			xprt_inactive_self(xprt);
		SOCK_RECVBUF_UNLOCK(xprt->xp_socket);
		sx_xunlock(&xprt->xp_lock);
		return (FALSE);
	}

	if (error) {
		SOCK_RECVBUF_LOCK(xprt->xp_socket);
		soupcall_clear(xprt->xp_socket, SO_RCV);
		SOCK_RECVBUF_UNLOCK(xprt->xp_socket);
		xprt_inactive_self(xprt);
		sx_xunlock(&xprt->xp_lock);
		return (FALSE);
	}

	sx_xunlock(&xprt->xp_lock);

	xdrmbuf_create(&xdrs, mreq, XDR_DECODE);
	if (! xdr_callmsg(&xdrs, msg)) {
		XDR_DESTROY(&xdrs);
		return (FALSE);
	}

	*addrp = raddr;
	*mp = xdrmbuf_getall(&xdrs);
	XDR_DESTROY(&xdrs);

	return (TRUE);
}

static bool_t
svc_dg_reply(SVCXPRT *xprt, struct rpc_msg *msg,
    struct sockaddr *addr, struct mbuf *m, uint32_t *seq)
{
	XDR xdrs;
	struct mbuf *mrep;
	bool_t stat = TRUE;
	int error;

	mrep = m_gethdr(M_WAITOK, MT_DATA);

	xdrmbuf_create(&xdrs, mrep, XDR_ENCODE);

	if (msg->rm_reply.rp_stat == MSG_ACCEPTED &&
	    msg->rm_reply.rp_acpt.ar_stat == SUCCESS) {
		if (!xdr_replymsg(&xdrs, msg))
			stat = FALSE;
		else
			(void)xdr_putmbuf(&xdrs, m);
	} else {
		stat = xdr_replymsg(&xdrs, msg);
	}

	if (stat) {
		m_fixhdr(mrep);
		error = sosend(xprt->xp_socket, addr, NULL, mrep, NULL,
		    0, curthread);
		if (!error) {
			stat = TRUE;
		}
	} else {
		m_freem(mrep);
	}

	XDR_DESTROY(&xdrs);
	xprt->xp_p2 = NULL;

	return (stat);
}

static void
svc_dg_destroy(SVCXPRT *xprt)
{

	SOCK_RECVBUF_LOCK(xprt->xp_socket);
	soupcall_clear(xprt->xp_socket, SO_RCV);
	SOCK_RECVBUF_UNLOCK(xprt->xp_socket);

	sx_destroy(&xprt->xp_lock);
	if (xprt->xp_socket)
		(void)soclose(xprt->xp_socket);

	if (xprt->xp_netid)
		(void) mem_free(xprt->xp_netid, strlen(xprt->xp_netid) + 1);
	svc_xprt_free(xprt);
}

static bool_t
/*ARGSUSED*/
svc_dg_control(SVCXPRT *xprt, const u_int rq, void *in)
{

	return (FALSE);
}

static int
svc_dg_soupcall(struct socket *so, void *arg, int waitflag)
{
	SVCXPRT *xprt = (SVCXPRT *) arg;

	xprt_active(xprt);
	return (SU_OK);
}
