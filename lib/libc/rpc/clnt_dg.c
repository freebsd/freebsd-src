/*	$NetBSD: clnt_dg.c,v 1.4 2000/07/14 08:40:41 fvdl Exp $	*/
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
/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

/* #ident	"@(#)clnt_dg.c	1.23	94/04/22 SMI" */

#if 0
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)clnt_dg.c 1.19 89/03/16 Copyr 1988 Sun Micro";
#endif
#endif

/*
 * Implements a connectionless client side RPC.
 */

#include "namespace.h"
#include "reentrant.h"
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include "un-namespace.h"
#include "rpc_com.h"


#define	RPC_MAX_BACKOFF		30 /* seconds */


static struct clnt_ops *clnt_dg_ops(void);
static bool_t time_not_ok(struct timeval *);
static enum clnt_stat clnt_dg_call __P((CLIENT *, rpcproc_t, xdrproc_t, caddr_t,
					xdrproc_t, caddr_t, struct timeval));
static void clnt_dg_geterr(CLIENT *, struct rpc_err *);
static bool_t clnt_dg_freeres(CLIENT *, xdrproc_t, caddr_t);
static void clnt_dg_abort(CLIENT *);
static bool_t clnt_dg_control(CLIENT *, u_int, char *);
static void clnt_dg_destroy(CLIENT *);
static int __rpc_timeval_to_msec(struct timeval *);




/*
 *	This machinery implements per-fd locks for MT-safety.  It is not
 *	sufficient to do per-CLIENT handle locks for MT-safety because a
 *	user may create more than one CLIENT handle with the same fd behind
 *	it.  Therfore, we allocate an array of flags (dg_fd_locks), protected
 *	by the clnt_fd_lock mutex, and an array (dg_cv) of condition variables
 *	similarly protected.  Dg_fd_lock[fd] == 1 => a call is activte on some
 *	CLIENT handle created for that fd.
 *	The current implementation holds locks across the entire RPC and reply,
 *	including retransmissions.  Yes, this is silly, and as soon as this
 *	code is proven to work, this should be the first thing fixed.  One step
 *	at a time.
 */
static int	*dg_fd_locks;
extern mutex_t clnt_fd_lock;
static cond_t	*dg_cv;
#define	release_fd_lock(fd, mask) {		\
	mutex_lock(&clnt_fd_lock);	\
	dg_fd_locks[fd] = 0;		\
	mutex_unlock(&clnt_fd_lock);	\
	thr_sigsetmask(SIG_SETMASK, &(mask), (sigset_t *) NULL);	\
	cond_signal(&dg_cv[fd]);	\
}

static const char mem_err_clnt_dg[] = "clnt_dg_create: out of memory";

/* VARIABLES PROTECTED BY clnt_fd_lock: dg_fd_locks, dg_cv */

/*
 * Private data kept per client handle
 */
struct cu_data {
	int			cu_fd;		/* connections fd */
	bool_t			cu_closeit;	/* opened by library */
	struct sockaddr_storage	cu_raddr;	/* remote address */
	int			cu_rlen;
	struct timeval		cu_wait;	/* retransmit interval */
	struct timeval		cu_total;	/* total time for the call */
	struct rpc_err		cu_error;
	XDR			cu_outxdrs;
	u_int			cu_xdrpos;
	u_int			cu_sendsz;	/* send size */
	char			*cu_outbuf;
	u_int			cu_recvsz;	/* recv size */
	struct pollfd		pfdp;
	int			cu_async;
	int			cu_connect;	/* Use connect(). */
	int			cu_connected;	/* Have done connect(). */
	char			cu_inbuf[1];
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
clnt_dg_create(fd, svcaddr, program, version, sendsz, recvsz)
	int fd;				/* open file descriptor */
	const struct netbuf *svcaddr;	/* servers address */
	rpcprog_t program;		/* program number */
	rpcvers_t version;		/* version number */
	u_int sendsz;			/* buffer recv size */
	u_int recvsz;			/* buffer send size */
{
	CLIENT *cl = NULL;		/* client handle */
	struct cu_data *cu = NULL;	/* private data */
	struct timeval now;
	struct rpc_msg call_msg;
	sigset_t mask;
	sigset_t newmask;
	struct __rpc_sockinfo si;
	int one = 1;

	sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
	if (dg_fd_locks == (int *) NULL) {
		int cv_allocsz;
		size_t fd_allocsz;
		int dtbsize = __rpc_dtbsize();

		fd_allocsz = dtbsize * sizeof (int);
		dg_fd_locks = (int *) mem_alloc(fd_allocsz);
		if (dg_fd_locks == (int *) NULL) {
			mutex_unlock(&clnt_fd_lock);
			thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
			goto err1;
		} else
			memset(dg_fd_locks, '\0', fd_allocsz);

		cv_allocsz = dtbsize * sizeof (cond_t);
		dg_cv = (cond_t *) mem_alloc(cv_allocsz);
		if (dg_cv == (cond_t *) NULL) {
			mem_free(dg_fd_locks, fd_allocsz);
			dg_fd_locks = (int *) NULL;
			mutex_unlock(&clnt_fd_lock);
			thr_sigsetmask(SIG_SETMASK, &(mask), NULL);
			goto err1;
		} else {
			int i;

			for (i = 0; i < dtbsize; i++)
				cond_init(&dg_cv[i], 0, (void *) 0);
		}
	}

	mutex_unlock(&clnt_fd_lock);
	thr_sigsetmask(SIG_SETMASK, &(mask), NULL);

	if (svcaddr == NULL) {
		rpc_createerr.cf_stat = RPC_UNKNOWNADDR;
		return (NULL);
	}

	if (!__rpc_fd2sockinfo(fd, &si)) {
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

	if ((cl = mem_alloc(sizeof (CLIENT))) == NULL)
		goto err1;
	/*
	 * Should be multiple of 4 for XDR.
	 */
	sendsz = ((sendsz + 3) / 4) * 4;
	recvsz = ((recvsz + 3) / 4) * 4;
	cu = mem_alloc(sizeof (*cu) + sendsz + recvsz);
	if (cu == NULL)
		goto err1;
	(void) memcpy(&cu->cu_raddr, svcaddr->buf, (size_t)svcaddr->len);
	cu->cu_rlen = svcaddr->len;
	cu->cu_outbuf = &cu->cu_inbuf[recvsz];
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
	(void) gettimeofday(&now, NULL);
	call_msg.rm_xid = __RPC_GETXID(&now);
	call_msg.rm_call.cb_prog = program;
	call_msg.rm_call.cb_vers = version;
	xdrmem_create(&(cu->cu_outxdrs), cu->cu_outbuf, sendsz, XDR_ENCODE);
	if (! xdr_callhdr(&(cu->cu_outxdrs), &call_msg)) {
		rpc_createerr.cf_stat = RPC_CANTENCODEARGS;  /* XXX */
		rpc_createerr.cf_error.re_errno = 0;
		goto err2;
	}
	cu->cu_xdrpos = XDR_GETPOS(&(cu->cu_outxdrs));

	/* XXX fvdl - do we still want this? */
#if 0
	(void)bindresvport_sa(fd, (struct sockaddr *)svcaddr->buf);
#endif
	_ioctl(fd, FIONBIO, (char *)(void *)&one);

	/*
	 * By default, closeit is always FALSE. It is users responsibility
	 * to do a close on it, else the user may use clnt_control
	 * to let clnt_destroy do it for him/her.
	 */
	cu->cu_closeit = FALSE;
	cu->cu_fd = fd;
	cl->cl_ops = clnt_dg_ops();
	cl->cl_private = (caddr_t)(void *)cu;
	cl->cl_auth = authnone_create();
	cl->cl_tp = NULL;
	cl->cl_netid = NULL;
	cu->pfdp.fd = cu->cu_fd;
	cu->pfdp.events = POLLIN | POLLPRI | POLLRDNORM | POLLRDBAND;
	return (cl);
err1:
	warnx(mem_err_clnt_dg);
	rpc_createerr.cf_stat = RPC_SYSTEMERROR;
	rpc_createerr.cf_error.re_errno = errno;
err2:
	if (cl) {
		mem_free(cl, sizeof (CLIENT));
		if (cu)
			mem_free(cu, sizeof (*cu) + sendsz + recvsz);
	}
	return (NULL);
}

static enum clnt_stat
clnt_dg_call(cl, proc, xargs, argsp, xresults, resultsp, utimeout)
	CLIENT	*cl;			/* client handle */
	rpcproc_t	proc;		/* procedure number */
	xdrproc_t	xargs;		/* xdr routine for args */
	caddr_t		argsp;		/* pointer to args */
	xdrproc_t	xresults;	/* xdr routine for results */
	caddr_t		resultsp;	/* pointer to results */
	struct timeval	utimeout;	/* seconds to wait before giving up */
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	XDR *xdrs;
	size_t outlen;
	struct rpc_msg reply_msg;
	XDR reply_xdrs;
	struct timeval time_waited;
	bool_t ok;
	int nrefreshes = 2;		/* number of times to refresh cred */
	struct timeval timeout;
	struct timeval retransmit_time;
	struct timeval startime, curtime;
	int firsttimeout = 1;
	struct sockaddr *sa;
	sigset_t mask;
	sigset_t newmask;
	socklen_t inlen, salen;
	ssize_t recvlen = 0;
	int rpc_lock_value;
	u_int32_t xid;

	outlen = 0;
	sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
	while (dg_fd_locks[cu->cu_fd])
		cond_wait(&dg_cv[cu->cu_fd], &clnt_fd_lock);
	if (__isthreaded)
		rpc_lock_value = 1;
	else
		rpc_lock_value = 0;
	dg_fd_locks[cu->cu_fd] = rpc_lock_value;
	mutex_unlock(&clnt_fd_lock);
	if (cu->cu_total.tv_usec == -1) {
		timeout = utimeout;	/* use supplied timeout */
	} else {
		timeout = cu->cu_total;	/* use default timeout */
	}

	if (cu->cu_connect && !cu->cu_connected) {
		if (_connect(cu->cu_fd, (struct sockaddr *)&cu->cu_raddr,
		    cu->cu_rlen) < 0) {
			release_fd_lock(cu->cu_fd, mask);
			cu->cu_error.re_errno = errno;
			return (cu->cu_error.re_status = RPC_CANTSEND);
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
	retransmit_time = cu->cu_wait;

call_again:
	xdrs = &(cu->cu_outxdrs);
	if (cu->cu_async == TRUE && xargs == NULL)
		goto get_reply;
	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, cu->cu_xdrpos);
	/*
	 * the transaction is the first thing in the out buffer
	 * XXX Yes, and it's in network byte order, so we should to
	 * be careful when we increment it, shouldn't we.
	 */
	xid = ntohl(*(u_int32_t *)(void *)(cu->cu_outbuf));
	xid++;
	*(u_int32_t *)(void *)(cu->cu_outbuf) = htonl(xid);

	if ((! XDR_PUTINT32(xdrs, &proc)) ||
	    (! AUTH_MARSHALL(cl->cl_auth, xdrs)) ||
	    (! (*xargs)(xdrs, argsp))) {
		release_fd_lock(cu->cu_fd, mask);
		return (cu->cu_error.re_status = RPC_CANTENCODEARGS);
	}
	outlen = (size_t)XDR_GETPOS(xdrs);

send_again:
	if (_sendto(cu->cu_fd, cu->cu_outbuf, outlen, 0, sa, salen) != outlen) {
		cu->cu_error.re_errno = errno;
		release_fd_lock(cu->cu_fd, mask);
		return (cu->cu_error.re_status = RPC_CANTSEND);
	}

	/*
	 * Hack to provide rpc-based message passing
	 */
	if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
		release_fd_lock(cu->cu_fd, mask);
		return (cu->cu_error.re_status = RPC_TIMEDOUT);
	}

get_reply:

	/*
	 * sub-optimal code appears here because we have
	 * some clock time to spare while the packets are in flight.
	 * (We assume that this is actually only executed once.)
	 */
	reply_msg.acpted_rply.ar_verf = _null_auth;
	reply_msg.acpted_rply.ar_results.where = resultsp;
	reply_msg.acpted_rply.ar_results.proc = xresults;


	for (;;) {
		switch (_poll(&cu->pfdp, 1,
		    __rpc_timeval_to_msec(&retransmit_time))) {
		case 0:
			time_waited.tv_sec += retransmit_time.tv_sec;
			time_waited.tv_usec += retransmit_time.tv_usec;
			while (time_waited.tv_usec >= 1000000) {
				time_waited.tv_sec++;
				time_waited.tv_usec -= 1000000;
			}
			/* update retransmit_time */
			if (retransmit_time.tv_sec < RPC_MAX_BACKOFF) {
				retransmit_time.tv_usec *= 2;
				retransmit_time.tv_sec *= 2;
				while (retransmit_time.tv_usec >= 1000000) {
					retransmit_time.tv_sec++;
					retransmit_time.tv_usec -= 1000000;
				}
			}

			if ((time_waited.tv_sec < timeout.tv_sec) ||
			    ((time_waited.tv_sec == timeout.tv_sec) &&
				(time_waited.tv_usec < timeout.tv_usec)))
				goto send_again;
			release_fd_lock(cu->cu_fd, mask);
			return (cu->cu_error.re_status = RPC_TIMEDOUT);

		case -1:
			if (errno == EBADF) {
				cu->cu_error.re_errno = errno;
				release_fd_lock(cu->cu_fd, mask);
				return (cu->cu_error.re_status = RPC_CANTRECV);
			}
			if (errno != EINTR) {
				errno = 0; /* reset it */
				continue;
			}
			/* interrupted by another signal, update time_waited */
			if (firsttimeout) {
				/*
				 * Could have done gettimeofday before clnt_call
				 * but that means 1 more system call per each
				 * clnt_call, so do it after first time out
				 */
				if (gettimeofday(&startime,
					(struct timezone *) NULL) == -1) {
					errno = 0;
					continue;
				}
				firsttimeout = 0;
				errno = 0;
				continue;
			};
			if (gettimeofday(&curtime,
				(struct timezone *) NULL) == -1) {
				errno = 0;
				continue;
			};
			time_waited.tv_sec += curtime.tv_sec - startime.tv_sec;
			time_waited.tv_usec += curtime.tv_usec -
							startime.tv_usec;
			while (time_waited.tv_usec < 0) {
				time_waited.tv_sec--;
				time_waited.tv_usec += 1000000;
			};
			while (time_waited.tv_usec >= 1000000) {
				time_waited.tv_sec++;
				time_waited.tv_usec -= 1000000;
			}
			startime.tv_sec = curtime.tv_sec;
			startime.tv_usec = curtime.tv_usec;
			if ((time_waited.tv_sec > timeout.tv_sec) ||
				((time_waited.tv_sec == timeout.tv_sec) &&
				(time_waited.tv_usec > timeout.tv_usec))) {
				release_fd_lock(cu->cu_fd, mask);
				return (cu->cu_error.re_status = RPC_TIMEDOUT);
			}
			errno = 0; /* reset it */
			continue;
		};

		if (cu->pfdp.revents & POLLNVAL || (cu->pfdp.revents == 0)) {
			cu->cu_error.re_status = RPC_CANTRECV;
			/*
			 *	Note:  we're faking errno here because we
			 *	previously would have expected _poll() to
			 *	return -1 with errno EBADF.  Poll(BA_OS)
			 *	returns 0 and sets the POLLNVAL revents flag
			 *	instead.
			 */
			cu->cu_error.re_errno = errno = EBADF;
			release_fd_lock(cu->cu_fd, mask);
			return (-1);
		}

		/* We have some data now */
		do {
			if (errno == EINTR) {
				/*
				 * Must make sure errno was not already
				 * EINTR in case _recvfrom() returns -1.
				 */
				errno = 0;
			}
			recvlen = _recvfrom(cu->cu_fd, cu->cu_inbuf,
			    cu->cu_recvsz, 0, NULL, NULL);
		} while (recvlen < 0 && errno == EINTR);
		if (recvlen < 0) {
			if (errno == EWOULDBLOCK)
				continue;
			cu->cu_error.re_errno = errno;
			release_fd_lock(cu->cu_fd, mask);
			return (cu->cu_error.re_status = RPC_CANTRECV);
		}
		if (recvlen < sizeof (u_int32_t))
			continue;
		/* see if reply transaction id matches sent id */
		if (cu->cu_async == FALSE &&
		    *((u_int32_t *)(void *)(cu->cu_inbuf)) !=
		    *((u_int32_t *)(void *)(cu->cu_outbuf)))
			continue;
		/* we now assume we have the proper reply */
		break;
	}
	inlen = (socklen_t)recvlen;

	/*
	 * now decode and validate the response
	 */

	xdrmem_create(&reply_xdrs, cu->cu_inbuf, (u_int)inlen, XDR_DECODE);
	ok = xdr_replymsg(&reply_xdrs, &reply_msg);
	/* XDR_DESTROY(&reply_xdrs);	save a few cycles on noop destroy */
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
				xdrs->x_op = XDR_FREE;
				(void) xdr_opaque_auth(xdrs,
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
	release_fd_lock(cu->cu_fd, mask);
	return (cu->cu_error.re_status);
}

static void
clnt_dg_geterr(cl, errp)
	CLIENT *cl;
	struct rpc_err *errp;
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;

	*errp = cu->cu_error;
}

static bool_t
clnt_dg_freeres(cl, xdr_res, res_ptr)
	CLIENT *cl;
	xdrproc_t xdr_res;
	caddr_t res_ptr;
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	XDR *xdrs = &(cu->cu_outxdrs);
	bool_t dummy;
	sigset_t mask;
	sigset_t newmask;

	sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
	while (dg_fd_locks[cu->cu_fd])
		cond_wait(&dg_cv[cu->cu_fd], &clnt_fd_lock);
	xdrs->x_op = XDR_FREE;
	dummy = (*xdr_res)(xdrs, res_ptr);
	mutex_unlock(&clnt_fd_lock);
	thr_sigsetmask(SIG_SETMASK, &mask, NULL);
	cond_signal(&dg_cv[cu->cu_fd]);
	return (dummy);
}

/*ARGSUSED*/
static void
clnt_dg_abort(h)
	CLIENT *h;
{
}

static bool_t
clnt_dg_control(cl, request, info)
	CLIENT *cl;
	u_int request;
	char *info;
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	struct netbuf *addr;
	sigset_t mask;
	sigset_t newmask;
	int rpc_lock_value;

	sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
	while (dg_fd_locks[cu->cu_fd])
		cond_wait(&dg_cv[cu->cu_fd], &clnt_fd_lock);
	if (__isthreaded)
                rpc_lock_value = 1;
        else
                rpc_lock_value = 0;
	dg_fd_locks[cu->cu_fd] = rpc_lock_value;
	mutex_unlock(&clnt_fd_lock);
	switch (request) {
	case CLSET_FD_CLOSE:
		cu->cu_closeit = TRUE;
		release_fd_lock(cu->cu_fd, mask);
		return (TRUE);
	case CLSET_FD_NCLOSE:
		cu->cu_closeit = FALSE;
		release_fd_lock(cu->cu_fd, mask);
		return (TRUE);
	}

	/* for other requests which use info */
	if (info == NULL) {
		release_fd_lock(cu->cu_fd, mask);
		return (FALSE);
	}
	switch (request) {
	case CLSET_TIMEOUT:
		if (time_not_ok((struct timeval *)(void *)info)) {
			release_fd_lock(cu->cu_fd, mask);
			return (FALSE);
		}
		cu->cu_total = *(struct timeval *)(void *)info;
		break;
	case CLGET_TIMEOUT:
		*(struct timeval *)(void *)info = cu->cu_total;
		break;
	case CLGET_SERVER_ADDR:		/* Give him the fd address */
		/* Now obsolete. Only for backward compatibility */
		(void) memcpy(info, &cu->cu_raddr, (size_t)cu->cu_rlen);
		break;
	case CLSET_RETRY_TIMEOUT:
		if (time_not_ok((struct timeval *)(void *)info)) {
			release_fd_lock(cu->cu_fd, mask);
			return (FALSE);
		}
		cu->cu_wait = *(struct timeval *)(void *)info;
		break;
	case CLGET_RETRY_TIMEOUT:
		*(struct timeval *)(void *)info = cu->cu_wait;
		break;
	case CLGET_FD:
		*(int *)(void *)info = cu->cu_fd;
		break;
	case CLGET_SVC_ADDR:
		addr = (struct netbuf *)(void *)info;
		addr->buf = &cu->cu_raddr;
		addr->len = cu->cu_rlen;
		addr->maxlen = sizeof cu->cu_raddr;
		break;
	case CLSET_SVC_ADDR:		/* set to new address */
		addr = (struct netbuf *)(void *)info;
		if (addr->len < sizeof cu->cu_raddr) {
			release_fd_lock(cu->cu_fd, mask);
			return (FALSE);
		}
		(void) memcpy(&cu->cu_raddr, addr->buf, addr->len);
		cu->cu_rlen = addr->len;
		break;
	case CLGET_XID:
		/*
		 * use the knowledge that xid is the
		 * first element in the call structure *.
		 * This will get the xid of the PREVIOUS call
		 */
		*(u_int32_t *)(void *)info =
		    ntohl(*(u_int32_t *)(void *)cu->cu_outbuf);
		break;

	case CLSET_XID:
		/* This will set the xid of the NEXT call */
		*(u_int32_t *)(void *)cu->cu_outbuf =
		    htonl(*(u_int32_t *)(void *)info - 1);
		/* decrement by 1 as clnt_dg_call() increments once */
		break;

	case CLGET_VERS:
		/*
		 * This RELIES on the information that, in the call body,
		 * the version number field is the fifth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		*(u_int32_t *)(void *)info =
		    ntohl(*(u_int32_t *)(void *)(cu->cu_outbuf +
		    4 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_VERS:
		*(u_int32_t *)(void *)(cu->cu_outbuf + 4 * BYTES_PER_XDR_UNIT)
			= htonl(*(u_int32_t *)(void *)info);
		break;

	case CLGET_PROG:
		/*
		 * This RELIES on the information that, in the call body,
		 * the program number field is the fourth field from the
		 * begining of the RPC header. MUST be changed if the
		 * call_struct is changed
		 */
		*(u_int32_t *)(void *)info =
		    ntohl(*(u_int32_t *)(void *)(cu->cu_outbuf +
		    3 * BYTES_PER_XDR_UNIT));
		break;

	case CLSET_PROG:
		*(u_int32_t *)(void *)(cu->cu_outbuf + 3 * BYTES_PER_XDR_UNIT)
			= htonl(*(u_int32_t *)(void *)info);
		break;
	case CLSET_ASYNC:
		cu->cu_async = *(int *)(void *)info;
		break;
	case CLSET_CONNECT:
		cu->cu_connect = *(int *)(void *)info;
		break;
	default:
		release_fd_lock(cu->cu_fd, mask);
		return (FALSE);
	}
	release_fd_lock(cu->cu_fd, mask);
	return (TRUE);
}

static void
clnt_dg_destroy(cl)
	CLIENT *cl;
{
	struct cu_data *cu = (struct cu_data *)cl->cl_private;
	int cu_fd = cu->cu_fd;
	sigset_t mask;
	sigset_t newmask;

	sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&clnt_fd_lock);
	while (dg_fd_locks[cu_fd])
		cond_wait(&dg_cv[cu_fd], &clnt_fd_lock);
	if (cu->cu_closeit)
		(void)_close(cu_fd);
	XDR_DESTROY(&(cu->cu_outxdrs));
	mem_free(cu, (sizeof (*cu) + cu->cu_sendsz + cu->cu_recvsz));
	if (cl->cl_netid && cl->cl_netid[0])
		mem_free(cl->cl_netid, strlen(cl->cl_netid) +1);
	if (cl->cl_tp && cl->cl_tp[0])
		mem_free(cl->cl_tp, strlen(cl->cl_tp) +1);
	mem_free(cl, sizeof (CLIENT));
	mutex_unlock(&clnt_fd_lock);
	thr_sigsetmask(SIG_SETMASK, &mask, NULL);
	cond_signal(&dg_cv[cu_fd]);
}

static struct clnt_ops *
clnt_dg_ops()
{
	static struct clnt_ops ops;
	extern mutex_t	ops_lock;
	sigset_t mask;
	sigset_t newmask;

/* VARIABLES PROTECTED BY ops_lock: ops */

	sigfillset(&newmask);
	thr_sigsetmask(SIG_SETMASK, &newmask, &mask);
	mutex_lock(&ops_lock);
	if (ops.cl_call == NULL) {
		ops.cl_call = clnt_dg_call;
		ops.cl_abort = clnt_dg_abort;
		ops.cl_geterr = clnt_dg_geterr;
		ops.cl_freeres = clnt_dg_freeres;
		ops.cl_destroy = clnt_dg_destroy;
		ops.cl_control = clnt_dg_control;
	}
	mutex_unlock(&ops_lock);
	thr_sigsetmask(SIG_SETMASK, &mask, NULL);
	return (&ops);
}

/*
 * Make sure that the time is not garbage.  -1 value is allowed.
 */
static bool_t
time_not_ok(t)
	struct timeval *t;
{
	return (t->tv_sec < -1 || t->tv_sec > 100000000 ||
		t->tv_usec < -1 || t->tv_usec > 1000000);
}


/*
 *	Convert from timevals (used by select) to milliseconds (used by poll).
 */
static int
__rpc_timeval_to_msec(t)
	struct timeval	*t;
{
	int	t1, tmp;

	/*
	 *	We're really returning t->tv_sec * 1000 + (t->tv_usec / 1000)
	 *	but try to do so efficiently.  Note:  1000 = 1024 - 16 - 8.
	 */
	tmp = (int)t->tv_sec << 3;
	t1 = -tmp;
	t1 += t1 << 1;
	t1 += tmp << 7;
	if (t->tv_usec)
		t1 += (int)(t->tv_usec / 1000);

	return (t1);
}
