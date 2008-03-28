/*	$NetBSD: svc_vc.c,v 1.7 2000/08/03 00:01:53 fvdl Exp $	*/

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
static char *sccsid2 = "@(#)svc_tcp.c 1.21 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)svc_tcp.c	2.2 88/08/01 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * svc_vc.c, Server side for Connection Oriented based RPC. 
 *
 * Actually implements two flavors of transporter -
 * a tcp rendezvouser (a listner and connection establisher)
 * and a record/tcp stream.
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
#include <netinet/tcp.h>

#include <rpc/rpc.h>

#include <rpc/rpc_com.h>

static bool_t svc_vc_rendezvous_recv(SVCXPRT *, struct rpc_msg *);
static enum xprt_stat svc_vc_rendezvous_stat(SVCXPRT *);
static void svc_vc_rendezvous_destroy(SVCXPRT *);
static bool_t svc_vc_null(void);
static void svc_vc_destroy(SVCXPRT *);
static enum xprt_stat svc_vc_stat(SVCXPRT *);
static bool_t svc_vc_recv(SVCXPRT *, struct rpc_msg *);
static bool_t svc_vc_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t svc_vc_freeargs(SVCXPRT *, xdrproc_t, void *);
static bool_t svc_vc_reply(SVCXPRT *, struct rpc_msg *);
static bool_t svc_vc_control(SVCXPRT *xprt, const u_int rq, void *in);
static bool_t svc_vc_rendezvous_control (SVCXPRT *xprt, const u_int rq,
    void *in);
static SVCXPRT *svc_vc_create_conn(SVCPOOL *pool, struct socket *so,
    struct sockaddr *raddr);
static int svc_vc_accept(struct socket *head, struct socket **sop);
static void svc_vc_soupcall(struct socket *so, void *arg, int waitflag);

static struct xp_ops svc_vc_rendezvous_ops = {
	.xp_recv =	svc_vc_rendezvous_recv,
	.xp_stat =	svc_vc_rendezvous_stat,
	.xp_getargs =	(bool_t (*)(SVCXPRT *, xdrproc_t, void *))svc_vc_null,
	.xp_reply =	(bool_t (*)(SVCXPRT *, struct rpc_msg *))svc_vc_null,
	.xp_freeargs =	(bool_t (*)(SVCXPRT *, xdrproc_t, void *))svc_vc_null,
	.xp_destroy =	svc_vc_rendezvous_destroy,
	.xp_control =	svc_vc_rendezvous_control
};

static struct xp_ops svc_vc_ops = {
	.xp_recv =	svc_vc_recv,
	.xp_stat =	svc_vc_stat,
	.xp_getargs =	svc_vc_getargs,
	.xp_reply =	svc_vc_reply,
	.xp_freeargs =	svc_vc_freeargs,
	.xp_destroy =	svc_vc_destroy,
	.xp_control =	svc_vc_control
};

struct cf_conn {  /* kept in xprt->xp_p1 for actual connection */
	enum xprt_stat strm_stat;
	struct mbuf *mpending;	/* unparsed data read from the socket */
	struct mbuf *mreq;	/* current record being built from mpending */
	uint32_t resid;		/* number of bytes needed for fragment */
	bool_t eor;		/* reading last fragment of current record */
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
svc_vc_create(SVCPOOL *pool, struct socket *so, size_t sendsize,
    size_t recvsize)
{
	SVCXPRT *xprt;
	struct sockaddr* sa;
	int error;

	xprt = mem_alloc(sizeof(SVCXPRT));
	mtx_init(&xprt->xp_lock, "xprt->xp_lock", NULL, MTX_DEF);
	xprt->xp_pool = pool;
	xprt->xp_socket = so;
	xprt->xp_p1 = NULL;
	xprt->xp_p2 = NULL;
	xprt->xp_p3 = NULL;
	xprt->xp_verf = _null_auth;
	xprt->xp_ops = &svc_vc_rendezvous_ops;

	error = so->so_proto->pr_usrreqs->pru_sockaddr(so, &sa);
	if (error)
		goto cleanup_svc_vc_create;

	xprt->xp_ltaddr.buf = mem_alloc(sizeof (struct sockaddr_storage));
	xprt->xp_ltaddr.maxlen = sizeof (struct sockaddr_storage);
	xprt->xp_ltaddr.len = sa->sa_len;
	memcpy(xprt->xp_ltaddr.buf, sa, sa->sa_len);
	free(sa, M_SONAME);

	xprt->xp_rtaddr.maxlen = 0;

	xprt_register(xprt);

	solisten(so, SOMAXCONN, curthread);

	SOCKBUF_LOCK(&so->so_rcv);
	so->so_upcallarg = xprt;
	so->so_upcall = svc_vc_soupcall;
	so->so_rcv.sb_flags |= SB_UPCALL;
	SOCKBUF_UNLOCK(&so->so_rcv);

	return (xprt);
cleanup_svc_vc_create:
	if (xprt)
		mem_free(xprt, sizeof(*xprt));
	return (NULL);
}

/*
 * Create a new transport for a socket optained via soaccept().
 */
SVCXPRT *
svc_vc_create_conn(SVCPOOL *pool, struct socket *so, struct sockaddr *raddr)
{
	SVCXPRT *xprt = NULL;
	struct cf_conn *cd = NULL;
	struct sockaddr* sa = NULL;
	int error;

	cd = mem_alloc(sizeof(*cd));
	cd->strm_stat = XPRT_IDLE;

	xprt = mem_alloc(sizeof(SVCXPRT));
	mtx_init(&xprt->xp_lock, "xprt->xp_lock", NULL, MTX_DEF);
	xprt->xp_pool = pool;
	xprt->xp_socket = so;
	xprt->xp_p1 = cd;
	xprt->xp_p2 = NULL;
	xprt->xp_p3 = NULL;
	xprt->xp_verf = _null_auth;
	xprt->xp_ops = &svc_vc_ops;

	xprt->xp_rtaddr.buf = mem_alloc(sizeof (struct sockaddr_storage));
	xprt->xp_rtaddr.maxlen = sizeof (struct sockaddr_storage);
	xprt->xp_rtaddr.len = raddr->sa_len;
	memcpy(xprt->xp_rtaddr.buf, raddr, raddr->sa_len);

	error = so->so_proto->pr_usrreqs->pru_sockaddr(so, &sa);
	if (error)
		goto cleanup_svc_vc_create;

	xprt->xp_ltaddr.buf = mem_alloc(sizeof (struct sockaddr_storage));
	xprt->xp_ltaddr.maxlen = sizeof (struct sockaddr_storage);
	xprt->xp_ltaddr.len = sa->sa_len;
	memcpy(xprt->xp_ltaddr.buf, sa, sa->sa_len);
	free(sa, M_SONAME);

	xprt_register(xprt);

	SOCKBUF_LOCK(&so->so_rcv);
	so->so_upcallarg = xprt;
	so->so_upcall = svc_vc_soupcall;
	so->so_rcv.sb_flags |= SB_UPCALL;
	SOCKBUF_UNLOCK(&so->so_rcv);

	/*
	 * Throw the transport into the active list in case it already
	 * has some data buffered.
	 */
	mtx_lock(&xprt->xp_lock);
	xprt_active(xprt);
	mtx_unlock(&xprt->xp_lock);

	return (xprt);
cleanup_svc_vc_create:
	if (xprt) {
		if (xprt->xp_ltaddr.buf)
			mem_free(xprt->xp_ltaddr.buf,
			    sizeof(struct sockaddr_storage));
		if (xprt->xp_rtaddr.buf)
			mem_free(xprt->xp_rtaddr.buf,
			    sizeof(struct sockaddr_storage));
		mem_free(xprt, sizeof(*xprt));
	}
	if (cd)
		mem_free(cd, sizeof(*cd));
	return (NULL);
}

/*
 * This does all of the accept except the final call to soaccept. The
 * caller will call soaccept after dropping its locks (soaccept may
 * call malloc).
 */
int
svc_vc_accept(struct socket *head, struct socket **sop)
{
	int error = 0;
	struct socket *so;

	if ((head->so_options & SO_ACCEPTCONN) == 0) {
		error = EINVAL;
		goto done;
	}
#ifdef MAC
	SOCK_LOCK(head);
	error = mac_socket_check_accept(td->td_ucred, head);
	SOCK_UNLOCK(head);
	if (error != 0)
		goto done;
#endif
	ACCEPT_LOCK();
	if (TAILQ_EMPTY(&head->so_comp)) {
		ACCEPT_UNLOCK();
		error = EWOULDBLOCK;
		goto done;
	}
	so = TAILQ_FIRST(&head->so_comp);
	KASSERT(!(so->so_qstate & SQ_INCOMP), ("svc_vc_accept: so SQ_INCOMP"));
	KASSERT(so->so_qstate & SQ_COMP, ("svc_vc_accept: so not SQ_COMP"));

	/*
	 * Before changing the flags on the socket, we have to bump the
	 * reference count.  Otherwise, if the protocol calls sofree(),
	 * the socket will be released due to a zero refcount.
	 * XXX might not need soref() since this is simpler than kern_accept.
	 */
	SOCK_LOCK(so);			/* soref() and so_state update */
	soref(so);			/* file descriptor reference */

	TAILQ_REMOVE(&head->so_comp, so, so_list);
	head->so_qlen--;
	so->so_state |= (head->so_state & SS_NBIO);
	so->so_qstate &= ~SQ_COMP;
	so->so_head = NULL;

	SOCK_UNLOCK(so);
	ACCEPT_UNLOCK();

	*sop = so;

	/* connection has been removed from the listen queue */
	KNOTE_UNLOCKED(&head->so_rcv.sb_sel.si_note, 0);
done:
	return (error);
}

/*ARGSUSED*/
static bool_t
svc_vc_rendezvous_recv(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct socket *so = NULL;
	struct sockaddr *sa = NULL;
	struct sockopt opt;
	int one = 1;
	int error;

	/*
	 * The socket upcall calls xprt_active() which will eventually
	 * cause the server to call us here. We attempt to accept a
	 * connection from the socket and turn it into a new
	 * transport. If the accept fails, we have drained all pending
	 * connections so we call xprt_inactive().
	 *
	 * The lock protects us in the case where a new connection arrives
	 * on the socket after our call to accept fails with
	 * EWOULDBLOCK - the call to xprt_active() in the upcall will
	 * happen only after our call to xprt_inactive() which ensures
	 * that we will remain active. It might be possible to use
	 * SOCKBUF_LOCK for this - its not clear to me what locks are
	 * held during the upcall.
	 */
	mtx_lock(&xprt->xp_lock);

	error = svc_vc_accept(xprt->xp_socket, &so);

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

	sa = 0;
	error = soaccept(so, &sa);

	if (!error) {
		bzero(&opt, sizeof(struct sockopt));
		opt.sopt_dir = SOPT_SET;
		opt.sopt_level = IPPROTO_TCP;
		opt.sopt_name = TCP_NODELAY;
		opt.sopt_val = &one;
		opt.sopt_valsize = sizeof(one);
		error = sosetopt(so, &opt);
	}

	if (error) {
		/*
		 * XXX not sure if I need to call sofree or soclose here.
		 */
		if (sa)
			free(sa, M_SONAME);
		return (FALSE);
	}

	/*
	 * svc_vc_create_conn will call xprt_register - we don't need
	 * to do anything with the new connection.
	 */
	svc_vc_create_conn(xprt->xp_pool, so, sa);
	free(sa, M_SONAME);

	return (FALSE); /* there is never an rpc msg to be processed */
}

/*ARGSUSED*/
static enum xprt_stat
svc_vc_rendezvous_stat(SVCXPRT *xprt)
{

	return (XPRT_IDLE);
}

static void
svc_vc_destroy_common(SVCXPRT *xprt)
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

static void
svc_vc_rendezvous_destroy(SVCXPRT *xprt)
{

	svc_vc_destroy_common(xprt);
}

static void
svc_vc_destroy(SVCXPRT *xprt)
{
	struct cf_conn *cd = (struct cf_conn *)xprt->xp_p1;

	svc_vc_destroy_common(xprt);

	if (cd->mreq)
		m_freem(cd->mreq);
	if (cd->mpending)
		m_freem(cd->mpending);
	mem_free(cd, sizeof(*cd));
}

/*ARGSUSED*/
static bool_t
svc_vc_control(SVCXPRT *xprt, const u_int rq, void *in)
{
	return (FALSE);
}

static bool_t
svc_vc_rendezvous_control(SVCXPRT *xprt, const u_int rq, void *in)
{

	return (FALSE);
}

static enum xprt_stat
svc_vc_stat(SVCXPRT *xprt)
{
	struct cf_conn *cd;
	struct mbuf *m;
	size_t n;

	cd = (struct cf_conn *)(xprt->xp_p1);

	if (cd->strm_stat == XPRT_DIED)
		return (XPRT_DIED);

	/*
	 * Return XPRT_MOREREQS if we have buffered data and we are
	 * mid-record or if we have enough data for a record marker.
	 */
	if (cd->mpending) {
		if (cd->resid)
			return (XPRT_MOREREQS);
		n = 0;
		m = cd->mpending;
		while (m && n < sizeof(uint32_t)) {
			n += m->m_len;
			m = m->m_next;
		}
		if (n >= sizeof(uint32_t))
			return (XPRT_MOREREQS);
	}

	return (XPRT_IDLE);
}

static bool_t
svc_vc_recv(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct cf_conn *cd = (struct cf_conn *) xprt->xp_p1;
	struct uio uio;
	struct mbuf *m;
	int error, rcvflag;

	for (;;) {
		/*
		 * If we have an mbuf chain in cd->mpending, try to parse a
		 * record from it, leaving the result in cd->mreq. If we don't
		 * have a complete record, leave the partial result in
		 * cd->mreq and try to read more from the socket.
		 */
		if (cd->mpending) {
			/*
			 * If cd->resid is non-zero, we have part of the
			 * record already, otherwise we are expecting a record
			 * marker.
			 */
			if (!cd->resid) {
				/*
				 * See if there is enough data buffered to
				 * make up a record marker. Make sure we can
				 * handle the case where the record marker is
				 * split across more than one mbuf.
				 */
				size_t n = 0;
				uint32_t header;

				m = cd->mpending;
				while (n < sizeof(uint32_t) && m) {
					n += m->m_len;
					m = m->m_next;
				}
				if (n < sizeof(uint32_t))
					goto readmore;
				cd->mpending = m_pullup(cd->mpending, sizeof(uint32_t));
				memcpy(&header, mtod(cd->mpending, uint32_t *),
				    sizeof(header));
				header = ntohl(header);
				cd->eor = (header & 0x80000000) != 0;
				cd->resid = header & 0x7fffffff;
				m_adj(cd->mpending, sizeof(uint32_t));
			}

			/*
			 * Start pulling off mbufs from cd->mpending
			 * until we either have a complete record or
			 * we run out of data. We use m_split to pull
			 * data - it will pull as much as possible and
			 * split the last mbuf if necessary.
			 */
			while (cd->mpending && cd->resid) {
				m = cd->mpending;
				cd->mpending = m_split(cd->mpending, cd->resid,
				    M_WAIT);
				if (cd->mreq)
					m_last(cd->mreq)->m_next = m;
				else
					cd->mreq = m;
				while (m) {
					cd->resid -= m->m_len;
					m = m->m_next;
				}
			}

			/*
			 * If cd->resid is zero now, we have managed to
			 * receive a record fragment from the stream. Check
			 * for the end-of-record mark to see if we need more.
			 */
			if (cd->resid == 0) {
				if (!cd->eor)
					continue;

				/*
				 * Success - we have a complete record in
				 * cd->mreq.
				 */
				xdrmbuf_create(&xprt->xp_xdrreq, cd->mreq, XDR_DECODE);
				cd->mreq = NULL;
				if (! xdr_callmsg(&xprt->xp_xdrreq, msg)) {
					XDR_DESTROY(&xprt->xp_xdrreq);
					return (FALSE);
				}
				xprt->xp_xid = msg->rm_xid;

				return (TRUE);
			}
		}

	readmore:
		/*
		 * The socket upcall calls xprt_active() which will eventually
		 * cause the server to call us here. We attempt to
		 * read as much as possible from the socket and put
		 * the result in cd->mpending. If the read fails,
		 * we have drained both cd->mpending and the socket so
		 * we can call xprt_inactive().
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
		m = NULL;
		rcvflag = MSG_DONTWAIT;
		error = soreceive(xprt->xp_socket, NULL, &uio, &m, NULL,
		    &rcvflag);

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
			cd->strm_stat = XPRT_DIED;
			mtx_unlock(&xprt->xp_lock);
			return (FALSE);
		}

		if (!m) {
			/*
			 * EOF - the other end has closed the socket.
			 */
			cd->strm_stat = XPRT_DIED;
			mtx_unlock(&xprt->xp_lock);
			return (FALSE);
		}

		if (cd->mpending)
			m_last(cd->mpending)->m_next = m;
		else
			cd->mpending = m;

		mtx_unlock(&xprt->xp_lock);
	}
}

static bool_t
svc_vc_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
{

	return (xdr_args(&xprt->xp_xdrreq, args_ptr));
}

static bool_t
svc_vc_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, void *args_ptr)
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

static bool_t
svc_vc_reply(SVCXPRT *xprt, struct rpc_msg *msg)
{
	struct mbuf *mrep;
	bool_t stat = FALSE;
	int error;

	/*
	 * Leave space for record mark.
	 */
	MGETHDR(mrep, M_WAIT, MT_DATA);
	MCLGET(mrep, M_WAIT);
	mrep->m_len = 0;
	mrep->m_data += sizeof(uint32_t);

	xdrmbuf_create(&xprt->xp_xdrrep, mrep, XDR_ENCODE);
	msg->rm_xid = xprt->xp_xid;
	if (xdr_replymsg(&xprt->xp_xdrrep, msg)) {
		m_fixhdr(mrep);

		/*
		 * Prepend a record marker containing the reply length.
		 */
		M_PREPEND(mrep, sizeof(uint32_t), M_WAIT);
		*mtod(mrep, uint32_t *) =
			htonl(0x80000000 | (mrep->m_pkthdr.len
				- sizeof(uint32_t)));
		error = sosend(xprt->xp_socket, NULL, NULL, mrep, NULL,
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
svc_vc_null()
{

	return (FALSE);
}

static void
svc_vc_soupcall(struct socket *so, void *arg, int waitflag)
{
	SVCXPRT *xprt = (SVCXPRT *) arg;

	mtx_lock(&xprt->xp_lock);
	xprt_active(xprt);
	mtx_unlock(&xprt->xp_lock);
}

#if 0
/*
 * Get the effective UID of the sending process. Used by rpcbind, keyserv
 * and rpc.yppasswdd on AF_LOCAL.
 */
int
__rpc_get_local_uid(SVCXPRT *transp, uid_t *uid) {
	int sock, ret;
	gid_t egid;
	uid_t euid;
	struct sockaddr *sa;

	sock = transp->xp_fd;
	sa = (struct sockaddr *)transp->xp_rtaddr.buf;
	if (sa->sa_family == AF_LOCAL) {
		ret = getpeereid(sock, &euid, &egid);
		if (ret == 0)
			*uid = euid;
		return (ret);
	} else
		return (-1);
}
#endif
