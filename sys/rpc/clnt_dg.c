/*	$NetBSD: clnt_dg.c,v 1.4 2000/07/14 08:40:41 fvdl Exp $	*/

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
#ident	"@(#)clnt_dg.c	1.23	94/04/22 SMI"
static char sccsid[] = "@(#)clnt_dg.c 1.19 89/03/16 Copyr 1988 Sun Micro";
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Implements a connectionless client side RPC.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <rpc/rpc.h>
#include <rpc/rpc_com.h>


#ifdef _FREEFALL_CONFIG
/*
 * Disable RPC exponential back-off for FreeBSD.org systems.
 */
#define	RPC_MAX_BACKOFF		1 /* second */
#else
#define	RPC_MAX_BACKOFF		30 /* seconds */
#endif

static bool_t time_not_ok(struct timeval *);
static enum clnt_stat clnt_dg_call(CLIENT *, rpcproc_t, xdrproc_t, void *,
	    xdrproc_t, void *, struct timeval);
static void clnt_dg_geterr(CLIENT *, struct rpc_err *);
static bool_t clnt_dg_freeres(CLIENT *, xdrproc_t, void *);
static void clnt_dg_abort(CLIENT *);
static bool_t clnt_dg_control(CLIENT *, u_int, void *);
static void clnt_dg_destroy(CLIENT *);
static void clnt_dg_soupcall(struct socket *so, void *arg, int waitflag);

static struct clnt_ops clnt_dg_ops = {
	.cl_call =	clnt_dg_call,
	.cl_abort =	clnt_dg_abort,
	.cl_geterr =	clnt_dg_geterr,
	.cl_freeres =	clnt_dg_freeres,
	.cl_destroy =	clnt_dg_destroy,
	.cl_control =	clnt_dg_control
};

static const char mem_err_clnt_dg[] = "clnt_dg_create: out of memory";

/*
 * A pending RPC request which awaits a reply.
 */
struct cu_request {
	TAILQ_ENTRY(cu_request) cr_link;
	uint32_t		cr_xid;		/* XID of request */
	struct mbuf		*cr_mrep;	/* reply received by upcall */
	int			cr_error;	/* any error from upcall */
};

TAILQ_HEAD(cu_request_list, cu_request);

#define MCALL_MSG_SIZE 24

/*
 * This structure is pointed to by the socket's so_upcallarg
 * member. It is separate from the client private data to facilitate
 * multiple clients sharing the same socket. The cs_lock mutex is used
 * to protect all fields of this structure, the socket's receive
 * buffer SOCKBUF_LOCK is used to ensure that exactly one of these
 * structures is installed on the socket.
 */
struct cu_socket {
	struct mtx		cs_lock;
	int			cs_refs;	/* Count of clients */
	struct cu_request_list	cs_pending;	/* Requests awaiting replies */
	
};

/*
 * Private data kept per client handle
 */
struct cu_data {
	struct socket		*cu_socket;	/* connection socket */
	bool_t			cu_closeit;	/* opened by library */
	struct sockaddr_storage	cu_raddr;	/* remote address */
	int			cu_rlen;
	struct timeval		cu_wait;	/* retransmit interval */
	struct timeval		cu_total;	/* total time for the call */
	struct rpc_err		cu_error;
	uint32_t		cu_xid;
	char			cu_mcallc[MCALL_MSG_SIZE]; /* marshalled callmsg */
	size_t			cu_mcalllen;
	size_t			cu_sendsz;	/* send size */
	size_t			cu_recvsz;	/* recv size */
	int			cu_async;
	int			cu_connect;	/* Use connect(). */
	int			cu_connected;	/* Have done connect(). */
	const char		*cu_waitchan;
	int			cu_waitflag;
};

/*
 * Connection less client creation returns with client handle parameters.
 * Default options are set, which the user can change using clnt_control().
 * fd should be open and bound.
 * NB: The rpch->cl_auth is initialized to null authentication.
 * 	Caller may wish to set this something more useful.
 *
 * sendsz and recvsz are the maximum allowable packet sizes that can be
 * sent and received. Normally they are the same, but they can be
 * changed to improve the program efficiency and buffer allocation.
 * If they are 0, use the transport default.
 *
 * If svcaddr is NULL, returns NULL.
 */
CLIENT *
clnt_dg_create(
	struct socket *so,
	struct sockaddr *svcaddr,	/* servers address */
	rpcprog_t program,		/* program number */
	rpcvers_t version,		/* version number */
	size_t sendsz,			/* buffer recv size */
	size_t recvsz)			/* buffer send size */
{
	CLIENT *cl = NULL;		/* client handle */
	struct cu_data *cu = NULL;	/* private data */
	struct cu_socket *cs = NULL;
	struct timeval now;
	struct rpc_msg call_msg;
	struct __rpc_sockinfo si;
	XDR xdrs;

	if (svcaddr == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (NULL);
	}

	if (!__rpc_socket2sockinfo(so, &si)) {
		rpc_createerr.cf_stat = RPC_TLIERROR;
		rpc_createerr.cf_error.re_errno = 0;
		return (NULL);
	}

	/*
	 * Find the receive and the send size
	 */
	sendsz = __rpc_get_t_size(si.si_af, si.si_proto, (int)sendsz);
	recvsz = __rpc_get_t_size(si.si_af, si.si_proto, (int)recvsz);
	if ((sendsz == 0) || (recvsz == 0)) {
		rpc_createerr.cf_stat = RPC_TLIERROR; /* XXX */
		rpc_createerr.cf_error.re_errno = 0;
		return (NULL);
	}

	cl = mem_alloc(sizeof (CLIENT));

	/*
	 * Should be multiple of 4 for XDR.
	 */
	sendsz = ((sendsz + 3) / 4) * 4;
	recvsz = ((recvsz + 3) / 4) * 4;
	cu = mem_alloc(sizeof (*cu));
	(void) memcpy(&cu->cu_raddr, svcaddr, (size_t)svcaddr->sa_len);
	cu->cu_rlen = svcaddr->sa_len;
	/* Other values can also be set through clnt_control() */
	cu->cu_wait.tv_sec = 15;	/* heuristically chosen */
	cu->cu_wait.tv_usec = 0;
	cu->cu_total.tv_sec = -1;
	cu->cu_total.tv_usec = -1;
	cu->cu_sendsz = sendsz;
	cu->cu_recvsz = recvsz;
	cu->cu_async = FALSE;
	cu->cu_connect = FALSE;
	cu->cu_connected = FALSE;
	cu->cu_waitchan = "rpcrecv";
	cu->cu_waitflag = 0;
	(void) getmicrotime(&now);
	cu->cu_xid = __RPC_GETXID(&now);
	call_msg.rm_xid = cu->cu_xid;
	call_msg.rm_call.cb_prog = program;
	call_msg.rm_call.cb_vers = version;
	xdrmem_create(&xdrs, cu->cu_mcallc, MCALL_MSG_SIZE, XDR_ENCODE);
	if (! xdr_callhdr(&xdrs, &call_msg)) {
		rpc_createerr.cf_stat = RPC_CANTENCODEARGS;  /* XXX */
		rpc_createerr.cf_error.re_errno = 0;
		goto err2;
	}
	cu->cu_mcalllen = XDR_GETPOS(&xdrs);;

	/*
	 * By default, closeit is always FALSE. It is users responsibility
	 * to do a close on it, else the user may use clnt_control
	 * to let clnt_destroy do it for him/her.
	 */
	cu->cu_closeit = FALSE;
	cu->cu_socket = so;

	SOCKBUF_LOCK(&so->so_rcv);
recheck_socket:
	if (so->so_upcall) {
		if (so->so_upcall != clnt_dg_soupcall) {
			SOCKBUF_UNLOCK(&so->so_rcv);
			printf("clnt_dg_create(): socket already has an incompatible upcall\n");
			goto err2;
		}
		cs = (struct cu_socket *) so->so_upcallarg;
		mtx_lock(&cs->cs_lock);
		cs->cs_refs++;
		mtx_unlock(&cs->cs_lock);
	} else {
		/*
		 * We are the first on this socket - allocate the
		 * structure and install it in the socket.
		 */
		SOCKBUF_UNLOCK(&cu->cu_socket->so_rcv);
		cs = mem_alloc(sizeof(*cs));
		SOCKBUF_LOCK(&cu->cu_socket->so_rcv);
		if (so->so_upcall) {
			/*
			 * We have lost a race with some other client.
			 */
			mem_free(cs, sizeof(*cs));
			goto recheck_socket;
		}
		mtx_init(&cs->cs_lock, "cs->cs_lock", NULL, MTX_DEF);
		cs->cs_refs = 1;
		TAILQ_INIT(&cs->cs_pending);
		so->so_upcallarg = cs;
		so->so_upcall = clnt_dg_soupcall;
		so->so_rcv.sb_flags |= SB_UPCALL;
	}
	SOCKBUF_UNLOCK(&so->so_rcv);

	cl->cl_ops = &clnt_dg_ops;
	cl->cl_private = (caddr_t)(void *)cu;
	cl->cl_auth = authnone_create();
	cl->cl_tp = NULL;
	cl->cl_netid = NULL;
	return (cl);
err2:
	if (cl) {
		mem_free(cl, sizeof (CLIENT));
		if (cu)
			mem_free(cu, sizeof (*cu));
	}
	return (NULL);
}

static enum clnt_stat
clnt_dg_call(
	CLIENT	*cl,			/* client handle */
	rpcproc_t	proc,		/* procedure number */
	xdrproc_t	xargs,		/* xdr routine for args */
	void		*argsp,		/* pointer to args */
	xdrproc_t	xresults,	/* xdr routine for results */
	void		*resultsp,	/* pointer to results */
	struct timeval	utimeout)	/* seconds to wait before giving up */
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	struct cu_socket *cs = (struct cu_socket *) cu->cu_socket->so_upcallarg;
	XDR xdrs;
	struct rpc_msg reply_msg;
	bool_t ok;
	int nrefreshes = 2;		/* number of times to refresh cred */
	struct timeval timeout;
	struct timeval retransmit_time;
	struct timeval next_sendtime, starttime, time_waited, tv;
	struct sockaddr *sa;
	socklen_t salen;
	uint32_t xid;
	struct mbuf *mreq = NULL;
	struct cu_request cr;
	int error;

	mtx_lock(&cs->cs_lock);

	cr.cr_mrep = NULL;
	cr.cr_error = 0;

	if (cu->cu_total.tv_usec == -1) {
		timeout = utimeout;	/* use supplied timeout */
	} else {
		timeout = cu->cu_total;	/* use default timeout */
	}

	if (cu->cu_connect && !cu->cu_connected) {
		mtx_unlock(&cs->cs_lock);
		error = soconnect(cu->cu_socket,
		    (struct sockaddr *)&cu->cu_raddr, curthread);
		mtx_lock(&cs->cs_lock);
		if (error) {
			cu->cu_error.re_errno = error;
			cu->cu_error.re_status = RPC_CANTSEND;
			goto out;
		}
		cu->cu_connected = 1;
	}
	if (cu->cu_connected) {
		sa = NULL;
		salen = 0;
	} else {
		sa = (struct sockaddr *)&cu->cu_raddr;
		salen = cu->cu_rlen;
	}
	time_waited.tv_sec = 0;
	time_waited.tv_usec = 0;
	retransmit_time = next_sendtime = cu->cu_wait;

	getmicrotime(&starttime);

call_again:
	mtx_assert(&cs->cs_lock, MA_OWNED);

	cu->cu_xid++;
	xid = cu->cu_xid;

send_again:
	mtx_unlock(&cs->cs_lock);

	MGETHDR(mreq, M_WAIT, MT_DATA);
	MCLGET(mreq, M_WAIT);
	mreq->m_len = 0;
	m_append(mreq, cu->cu_mcalllen, cu->cu_mcallc);

	/*
	 * The XID is the first thing in the request.
	 */
	*mtod(mreq, uint32_t *) = htonl(xid);

	xdrmbuf_create(&xdrs, mreq, XDR_ENCODE);

	if (cu->cu_async == TRUE && xargs == NULL)
		goto get_reply;

	if ((! XDR_PUTINT32(&xdrs, &proc)) ||
	    (! AUTH_MARSHALL(cl->cl_auth, &xdrs)) ||
	    (! (*xargs)(&xdrs, argsp))) {
		cu->cu_error.re_status = RPC_CANTENCODEARGS;
		mtx_lock(&cs->cs_lock);
		goto out;
	}
	m_fixhdr(mreq);

	cr.cr_xid = xid;
	mtx_lock(&cs->cs_lock);
	TAILQ_INSERT_TAIL(&cs->cs_pending, &cr, cr_link);
	mtx_unlock(&cs->cs_lock);

	/*
	 * sosend consumes mreq.
	 */
	error = sosend(cu->cu_socket, sa, NULL, mreq, NULL, 0, curthread);
	mreq = NULL;

	/*
	 * sub-optimal code appears here because we have
	 * some clock time to spare while the packets are in flight.
	 * (We assume that this is actually only executed once.)
	 */
	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = resultsp;
	reply_msg.acpted_rply.ar_results.proc = xresults;

	mtx_lock(&cs->cs_lock);
	if (error) {
		TAILQ_REMOVE(&cs->cs_pending, &cr, cr_link);

		cu->cu_error.re_errno = error;
		cu->cu_error.re_status = RPC_CANTSEND;
		goto out;
	}

	/*
	 * Check to see if we got an upcall while waiting for the
	 * lock. In both these cases, the request has been removed
	 * from cs->cs_pending.
	 */
	if (cr.cr_error) {
		cu->cu_error.re_errno = cr.cr_error;
		cu->cu_error.re_status = RPC_CANTRECV;
		goto out;
	}
	if (cr.cr_mrep) {
		goto got_reply;
	}

	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		if (cr.cr_xid)
			TAILQ_REMOVE(&cs->cs_pending, &cr, cr_link);
		cu->cu_error.re_status = RPC_TIMEDOUT;
		goto out;
	}

get_reply:
	for (;;) {
		/* Decide how long to wait. */
		if (timevalcmp(&next_sendtime, &timeout, <)) {
			tv = next_sendtime;
		} else {
			tv = timeout;
		}
		timevalsub(&tv, &time_waited);
		if (tv.tv_sec < 0 || tv.tv_usec < 0)
			tv.tv_sec = tv.tv_usec = 0;

		error = msleep(&cr, &cs->cs_lock, cu->cu_waitflag,
		    cu->cu_waitchan, tvtohz(&tv));

		if (!error) {
			/*
			 * We were woken up by the upcall.  If the
			 * upcall had a receive error, report that,
			 * otherwise we have a reply.
			 */
			if (cr.cr_error) {
				cu->cu_error.re_errno = cr.cr_error;
				cu->cu_error.re_status = RPC_CANTRECV;
				goto out;
			}
			break;
		}

		/*
		 * The sleep returned an error so our request is still
		 * on the list. If we got EWOULDBLOCK, we may want to
		 * re-send the request.
		 */
		if (error != EWOULDBLOCK) {
			if (cr.cr_xid)
				TAILQ_REMOVE(&cs->cs_pending, &cr, cr_link);
			cu->cu_error.re_errno = error;
			if (error == EINTR)
				cu->cu_error.re_status = RPC_INTR;
			else
				cu->cu_error.re_status = RPC_CANTRECV;
			goto out;
		}

		getmicrotime(&tv);
		time_waited = tv;
		timevalsub(&time_waited, &starttime);

		/* Check for timeout. */
		if (timevalcmp(&time_waited, &timeout, >)) {
			if (cr.cr_xid)
				TAILQ_REMOVE(&cs->cs_pending, &cr, cr_link);
			cu->cu_error.re_errno = EWOULDBLOCK;
			cu->cu_error.re_status = RPC_TIMEDOUT;
			goto out;
		}

		/* Retransmit if necessary. */		
		if (timevalcmp(&time_waited, &next_sendtime, >)) {
			if (cr.cr_xid)
				TAILQ_REMOVE(&cs->cs_pending, &cr, cr_link);
			/* update retransmit_time */
			if (retransmit_time.tv_sec < RPC_MAX_BACKOFF)
				timevaladd(&retransmit_time, &retransmit_time);
			timevaladd(&next_sendtime, &retransmit_time);
			goto send_again;
		}
	}

got_reply:
	/*
	 * Now decode and validate the response. We need to drop the
	 * lock since xdr_replymsg may end up sleeping in malloc.
	 */
	mtx_unlock(&cs->cs_lock);

	xdrmbuf_create(&xdrs, cr.cr_mrep, XDR_DECODE);
	ok = xdr_replymsg(&xdrs, &reply_msg);
	XDR_DESTROY(&xdrs);
	cr.cr_mrep = NULL;

	mtx_lock(&cs->cs_lock);

	if (ok) {
		if ((reply_msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
			(reply_msg.acpted_rply.ar_stat == SUCCESS))
			cu->cu_error.re_status = RPC_SUCCESS;
		else
			_seterr_reply(&reply_msg, &(cu->cu_error));

		if (cu->cu_error.re_status == RPC_SUCCESS) {
			if (! AUTH_VALIDATE(cl->cl_auth,
					    &reply_msg.acpted_rply.ar_verf)) {
				cu->cu_error.re_status = RPC_AUTHERROR;
				cu->cu_error.re_why = AUTH_INVALIDRESP;
			}
			if (reply_msg.acpted_rply.ar_verf.oa_base != NULL) {
				xdrs.x_op = XDR_FREE;
				(void) xdr_opaque_auth(&xdrs,
					&(reply_msg.acpted_rply.ar_verf));
			}
		}		/* end successful completion */
		/*
		 * If unsuccesful AND error is an authentication error
		 * then refresh credentials and try again, else break
		 */
		else if (cu->cu_error.re_status == RPC_AUTHERROR)
			/* maybe our credentials need to be refreshed ... */
			if (nrefreshes > 0 &&
			    AUTH_REFRESH(cl->cl_auth, &reply_msg)) {
				nrefreshes--;
				goto call_again;
			}
		/* end of unsuccessful completion */
	}	/* end of valid reply message */
	else {
		cu->cu_error.re_status = RPC_CANTDECODERES;

	}
out:
	mtx_assert(&cs->cs_lock, MA_OWNED);

	if (mreq)
		m_freem(mreq);
	if (cr.cr_mrep)
		m_freem(cr.cr_mrep);

	mtx_unlock(&cs->cs_lock);
	return (cu->cu_error.re_status);
}

static void
clnt_dg_geterr(CLIENT *cl, struct rpc_err *errp)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;

	*errp = cu->cu_error;
}

static bool_t
clnt_dg_freeres(CLIENT *cl, xdrproc_t xdr_res, void *res_ptr)
{
	XDR xdrs;
	bool_t dummy;

	xdrs.x_op = XDR_FREE;
	dummy = (*xdr_res)(&xdrs, res_ptr);

	return (dummy);
}

/*ARGSUSED*/
static void
clnt_dg_abort(CLIENT *h)
{
}

static bool_t
clnt_dg_control(CLIENT *cl, u_int request, void *info)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	struct cu_socket *cs = (struct cu_socket *) cu->cu_socket->so_upcallarg;
	struct sockaddr *addr;

	mtx_lock(&cs->cs_lock);

	switch (request) {
	case CLSET_FD_CLOSE:
		cu->cu_closeit = TRUE;
		mtx_unlock(&cs->cs_lock);
		return (TRUE);
	case CLSET_FD_NCLOSE:
		cu->cu_closeit = FALSE;
		mtx_unlock(&cs->cs_lock);
		return (TRUE);
	}

	/* for other requests which use info */
	if (info == NULL) {
		mtx_unlock(&cs->cs_lock);
		return (FALSE);
	}
	switch (request) {
	case CLSET_TIMEOUT:
		if (time_not_ok((struct timeval *)info)) {
			mtx_unlock(&cs->cs_lock);
			return (FALSE);
		}
		cu->cu_total = *(struct timeval *)info;
		break;
	case CLGET_TIMEOUT:
		*(struct timeval *)info = cu->cu_total;
		break;
	case CLSET_RETRY_TIMEOUT:
		if (time_not_ok((struct timeval *)info)) {
			mtx_unlock(&cs->cs_lock);
			return (FALSE);
		}
		cu->cu_wait = *(struct timeval *)info;
		break;
	case CLGET_RETRY_TIMEOUT:
		*(struct timeval *)info = cu->cu_wait;
		break;
	case CLGET_SVC_ADDR:
		/*
		 * Slightly different semantics to userland - we use
		 * sockaddr instead of netbuf.
		 */
		memcpy(info, &cu->cu_raddr, cu->cu_raddr.ss_len);
		break;
	case CLSET_SVC_ADDR:		/* set to new address */
		addr = (struct sockaddr *)info;
		(void) memcpy(&cu->cu_raddr, addr, addr->sa_len);
		break;
	case CLGET_XID:
		*(uint32_t *)info = cu->cu_xid;
		break;

	case CLSET_XID:
		/* This will set the xid of the NEXT call */
		/* decrement by 1 as clnt_dg_call() increments once */
		cu->cu_xid = *(uint32_t *)info - 1;
		break;

	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		*(uint32_t *)info =
		    ntohl(*(uint32_t *)(void *)(cu->cu_mcallc +
		    4 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_VERS:
		*(uint32_t *)(void *)(cu->cu_mcallc + 4 * BYTES_PER_XDR_UNIT)
			= htonl(*(uint32_t *)info);
		break;

	case CLGET_PROG:
		/*
		 * This RELIES on the information that, in the call body,
		 * the program number field is the fourth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		*(uint32_t *)info =
		    ntohl(*(uint32_t *)(void *)(cu->cu_mcallc +
		    3 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_PROG:
		*(uint32_t *)(void *)(cu->cu_mcallc + 3 * BYTES_PER_XDR_UNIT)
			= htonl(*(uint32_t *)info);
		break;
	case CLSET_ASYNC:
		cu->cu_async = *(int *)info;
		break;
	case CLSET_CONNECT:
		cu->cu_connect = *(int *)info;
		break;
	case CLSET_WAITCHAN:
		cu->cu_waitchan = *(const char **)info;
		break;
	case CLGET_WAITCHAN:
		*(const char **) info = cu->cu_waitchan;
		break;
	case CLSET_INTERRUPTIBLE:
		if (*(int *) info)
			cu->cu_waitflag = PCATCH;
		else
			cu->cu_waitflag = 0;
		break;
	case CLGET_INTERRUPTIBLE:
		if (cu->cu_waitflag)
			*(int *) info = TRUE;
		else
			*(int *) info = FALSE;
		break;
	default:
		mtx_unlock(&cs->cs_lock);
		return (FALSE);
	}
	mtx_unlock(&cs->cs_lock);
	return (TRUE);
}

static void
clnt_dg_destroy(CLIENT *cl)
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	struct cu_socket *cs = (struct cu_socket *) cu->cu_socket->so_upcallarg;
	struct socket *so = NULL;
	bool_t lastsocketref;

	SOCKBUF_LOCK(&cu->cu_socket->so_rcv);

	mtx_lock(&cs->cs_lock);
	cs->cs_refs--;
	if (cs->cs_refs == 0) {
		cu->cu_socket->so_upcallarg = NULL;
		cu->cu_socket->so_upcall = NULL;
		cu->cu_socket->so_rcv.sb_flags &= ~SB_UPCALL;
		mtx_destroy(&cs->cs_lock);
		SOCKBUF_UNLOCK(&cu->cu_socket->so_rcv);
		mem_free(cs, sizeof(*cs));
		lastsocketref = TRUE;
	} else {
		mtx_unlock(&cs->cs_lock);
		SOCKBUF_UNLOCK(&cu->cu_socket->so_rcv);
		lastsocketref = FALSE;
	}

	if (cu->cu_closeit) {
		KASSERT(lastsocketref, ("clnt_dg_destroy(): closing a socket "
			"shared with other clients"));
		so = cu->cu_socket;
		cu->cu_socket = NULL;
	}

	if (so)
		soclose(so);

	if (cl->cl_netid && cl->cl_netid[0])
		mem_free(cl->cl_netid, strlen(cl->cl_netid) +1);
	if (cl->cl_tp && cl->cl_tp[0])
		mem_free(cl->cl_tp, strlen(cl->cl_tp) +1);
	mem_free(cu, sizeof (*cu));
	mem_free(cl, sizeof (CLIENT));
}

/*
 * Make sure that the time is not garbage.  -1 value is allowed.
 */
static bool_t
time_not_ok(struct timeval *t)
{
	return (t->tv_sec < -1 || t->tv_sec > 100000000 ||
		t->tv_usec < -1 || t->tv_usec > 1000000);
}

void
clnt_dg_soupcall(struct socket *so, void *arg, int waitflag)
{
	struct cu_socket *cs = (struct cu_socket *) arg;
	struct uio uio;
	struct mbuf *m;
	struct mbuf *control;
	struct cu_request *cr;
	int error, rcvflag, foundreq;
	uint32_t xid;

	uio.uio_resid = 1000000000;
	uio.uio_td = curthread;
	do {
		m = NULL;
		control = NULL;
		rcvflag = MSG_DONTWAIT;
		error = soreceive(so, NULL, &uio, &m, &control, &rcvflag);
		if (control)
			m_freem(control);

		if (error == EWOULDBLOCK)
			break;

		/*
		 * If there was an error, wake up all pending
		 * requests.
		 */
		if (error) {
			mtx_lock(&cs->cs_lock);
			TAILQ_FOREACH(cr, &cs->cs_pending, cr_link) {
				cr->cr_error = error;
				wakeup(cr);
			}
			TAILQ_INIT(&cs->cs_pending);
			mtx_unlock(&cs->cs_lock);
			break;
		}

		/*
		 * The XID is in the first uint32_t of the reply.
		 */
		m = m_pullup(m, sizeof(xid));
		if (!m)
			break;
		xid = ntohl(*mtod(m, uint32_t *));

		/*
		 * Attempt to match this reply with a pending request.
		 */
		mtx_lock(&cs->cs_lock);
		foundreq = 0;
		TAILQ_FOREACH(cr, &cs->cs_pending, cr_link) {
			if (cr->cr_xid == xid) {
				/*
				 * This one matches. We snip it out of
				 * the pending list and leave the
				 * reply mbuf in cr->cr_mrep. Set the
				 * XID to zero so that clnt_dg_call
				 * can know not to repeat the
				 * TAILQ_REMOVE.
				 */
				TAILQ_REMOVE(&cs->cs_pending, cr, cr_link);
				cr->cr_xid = 0;
				cr->cr_mrep = m;
				cr->cr_error = 0;
				foundreq = 1;
				wakeup(cr);
				break;
			}
		}
		mtx_unlock(&cs->cs_lock);

		/*
		 * If we didn't find the matching request, just drop
		 * it - its probably a repeated reply.
		 */
		if (!foundreq)
			m_freem(m);
	} while (m);
}

