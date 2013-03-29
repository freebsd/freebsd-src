/*-
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
 *
 * $FreeBSD$
 */

#ifndef _RPC_KRPC_H_
#define	_RPC_KRPC_H_

#ifdef _KERNEL
/*
 * Definitions now shared between client and server RPC for backchannels.
 */
#define MCALL_MSG_SIZE 24

/*
 * A pending RPC request which awaits a reply. Requests which have
 * received their reply will have cr_xid set to zero and cr_mrep to
 * the mbuf chain of the reply.
 */
struct ct_request {
	TAILQ_ENTRY(ct_request) cr_link;
	uint32_t		cr_xid;		/* XID of request */
	struct mbuf		*cr_mrep;	/* reply received by upcall */
	int			cr_error;	/* any error from upcall */
	char			cr_verf[MAX_AUTH_BYTES]; /* reply verf */
};

TAILQ_HEAD(ct_request_list, ct_request);

struct rc_data {
	struct mtx		rc_lock;
	struct sockaddr_storage	rc_addr; /* server address */
	struct netconfig*	rc_nconf; /* network type */
	rpcprog_t		rc_prog;  /* program number */
	rpcvers_t		rc_vers;  /* version number */
	size_t			rc_sendsz;
	size_t			rc_recvsz;
	struct timeval		rc_timeout;
	struct timeval		rc_retry;
	int			rc_retries;
	int			rc_privport;
	char			*rc_waitchan;
	int			rc_intr;
	int			rc_connecting;
	int			rc_closed;
	struct ucred		*rc_ucred;
	CLIENT*			rc_client; /* underlying RPC client */
	struct rpc_err		rc_err;
	void			*rc_backchannel;
};

struct ct_data {
	struct mtx	ct_lock;
	int		ct_threads;	/* number of threads in clnt_vc_call */
	bool_t		ct_closing;	/* TRUE if we are closing */
	bool_t		ct_closed;	/* TRUE if we are closed */
	struct socket	*ct_socket;	/* connection socket */
	bool_t		ct_closeit;	/* close it on destroy */
	struct timeval	ct_wait;	/* wait interval in milliseconds */
	struct sockaddr_storage	ct_addr; /* remote addr */
	struct rpc_err	ct_error;
	uint32_t	ct_xid;
	char		ct_mcallc[MCALL_MSG_SIZE]; /* marshalled callmsg */
	size_t		ct_mpos;	/* pos after marshal */
	const char	*ct_waitchan;
	int		ct_waitflag;
	struct mbuf	*ct_record;	/* current reply record */
	size_t		ct_record_resid; /* how much left of reply to read */
	bool_t		ct_record_eor;	 /* true if reading last fragment */
	struct ct_request_list ct_pending;
	int		ct_upcallrefs;	/* Ref cnt of upcalls in prog. */
	SVCXPRT		*ct_backchannelxprt; /* xprt for backchannel */
};

struct cf_conn {  /* kept in xprt->xp_p1 for actual connection */
	enum xprt_stat strm_stat;
	struct mbuf *mpending;	/* unparsed data read from the socket */
	struct mbuf *mreq;	/* current record being built from mpending */
	uint32_t resid;		/* number of bytes needed for fragment */
	bool_t eor;		/* reading last fragment of current record */
};

#endif	/* _KERNEL */

#endif	/* _RPC_KRPC_H_ */
