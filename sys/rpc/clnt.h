/*	$NetBSD: clnt.h,v 1.14 2000/06/02 22:57:55 fvdl Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010, Oracle America, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the "Oracle America, Inc." nor the names of its
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
 * clnt.h - Client side remote procedure call interface.
 *
 * Copyright (c) 1986-1991,1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RPC_CLNT_H_
#define _RPC_CLNT_H_
#include <rpc/clnt_stat.h>
#include <sys/cdefs.h>
#include <sys/refcount.h>
#include <rpc/netconfig.h>
#include <sys/un.h>

/*
 * Well-known IPV6 RPC broadcast address.
 */
#define RPCB_MULTICAST_ADDR "ff02::202"

/*
 * the following errors are in general unrecoverable.  The caller
 * should give up rather than retry.
 */
#define IS_UNRECOVERABLE_RPC(s) (((s) == RPC_AUTHERROR) || \
	((s) == RPC_CANTENCODEARGS) || \
	((s) == RPC_CANTDECODERES) || \
	((s) == RPC_VERSMISMATCH) || \
	((s) == RPC_PROCUNAVAIL) || \
	((s) == RPC_PROGUNAVAIL) || \
	((s) == RPC_PROGVERSMISMATCH) || \
	((s) == RPC_CANTDECODEARGS))

/*
 * Error info.
 */
struct rpc_err {
	enum clnt_stat re_status;
	union {
		int RE_errno;		/* related system error */
		enum auth_stat RE_why;	/* why the auth error occurred */
		struct {
			rpcvers_t low;	/* lowest version supported */
			rpcvers_t high;	/* highest version supported */
		} RE_vers;
		struct {		/* maybe meaningful if RPC_FAILED */
			int32_t s1;
			int32_t s2;
		} RE_lb;		/* life boot & debugging only */
	} ru;
#define	re_errno	ru.RE_errno
#define	re_why		ru.RE_why
#define	re_vers		ru.RE_vers
#define	re_lb		ru.RE_lb
};

/*
 * Functions of this type may be used to receive notification when RPC
 * calls have to be re-transmitted etc.
 */
typedef void rpc_feedback(int cmd, int procnum, void *);

/*
 * Timers used for the pseudo-transport protocol when using datagrams
 */
struct rpc_timers {
	u_short		rt_srtt;	/* smoothed round-trip time */
	u_short		rt_deviate;	/* estimated deviation */
	u_long		rt_rtxcur;	/* current (backed-off) rto */
};

/*
 * A structure used with CLNT_CALL_EXT to pass extra information used
 * while processing an RPC call.
 */
struct rpc_callextra {
	AUTH		*rc_auth;	/* auth handle to use for this call */
	rpc_feedback	*rc_feedback;	/* callback for retransmits etc. */
	void		*rc_feedback_arg; /* argument for callback */
	struct rpc_timers *rc_timers;	  /* optional RTT timers */
	struct rpc_err	rc_err;		/* detailed call status */
};

/*
 * Client rpc handle.
 * Created by individual implementations
 * Client is responsible for initializing auth, see e.g. auth_none.c.
 */
typedef struct __rpc_client {
	volatile u_int cl_refs;			/* reference count */
	AUTH	*cl_auth;			/* authenticator */
	const struct clnt_ops {
		/* call remote procedure */
		enum clnt_stat	(*cl_call)(struct __rpc_client *,
		    struct rpc_callextra *, rpcproc_t,
		    struct mbuf *, struct mbuf **, struct timeval);
		/* abort a call */
		void		(*cl_abort)(struct __rpc_client *);
		/* get specific error code */
		void		(*cl_geterr)(struct __rpc_client *,
					struct rpc_err *);
		/* frees results */
		bool_t		(*cl_freeres)(struct __rpc_client *,
					xdrproc_t, void *);
		/* close the connection and terminate pending RPCs */
		void		(*cl_close)(struct __rpc_client *);
		/* destroy this structure */
		void		(*cl_destroy)(struct __rpc_client *);
		/* the ioctl() of rpc */
		bool_t          (*cl_control)(struct __rpc_client *, u_int,
				    void *);
	} *cl_ops;
	void 			*cl_private;	/* private stuff */
	char			*cl_netid;	/* network token */
	char			*cl_tp;		/* device name */
} CLIENT;

/*      
 * Feedback values used for possible congestion and rate control
 */
#define FEEDBACK_OK		1	/* no retransmits */    
#define FEEDBACK_REXMIT1	2	/* first retransmit */
#define FEEDBACK_REXMIT2	3	/* second and subsequent retransmit */
#define FEEDBACK_RECONNECT	4	/* client reconnect */

/* Used to set version of portmapper used in broadcast */
  
#define CLCR_SET_LOWVERS	3
#define CLCR_GET_LOWVERS	4
 
#define RPCSMALLMSGSIZE 400	/* a more reasonable packet size */

/*
 * client side rpc interface ops
 *
 * Parameter types are:
 *
 */

#define CLNT_ACQUIRE(rh)			\
	refcount_acquire(&(rh)->cl_refs)
#define CLNT_RELEASE(rh)			\
	if (refcount_release(&(rh)->cl_refs))	\
		CLNT_DESTROY(rh)

/*
 * void
 * CLNT_CLOSE(rh);
 * 	CLIENT *rh;
 */
#define	CLNT_CLOSE(rh)	((*(rh)->cl_ops->cl_close)(rh))

enum clnt_stat clnt_call_private(CLIENT *, struct rpc_callextra *, rpcproc_t,
    xdrproc_t, void *, xdrproc_t, void *, struct timeval);

/*
 * enum clnt_stat
 * CLNT_CALL_MBUF(rh, ext, proc, mreq, mrepp, timeout)
 * 	CLIENT *rh;
 *	struct rpc_callextra *ext;
 *	rpcproc_t proc;
 *	struct mbuf *mreq;
 *	struct mbuf **mrepp;
 *	struct timeval timeout;
 *
 * Call arguments in mreq which is consumed by the call (even if there
 * is an error). Results returned in *mrepp.
 */
#define	CLNT_CALL_MBUF(rh, ext, proc, mreq, mrepp, secs)	\
	((*(rh)->cl_ops->cl_call)(rh, ext, proc, mreq, mrepp, secs))

/*
 * enum clnt_stat
 * CLNT_CALL_EXT(rh, ext, proc, xargs, argsp, xres, resp, timeout)
 * 	CLIENT *rh;
 *	struct rpc_callextra *ext;
 *	rpcproc_t proc;
 *	xdrproc_t xargs;
 *	void *argsp;
 *	xdrproc_t xres;
 *	void *resp;
 *	struct timeval timeout;
 */
#define	CLNT_CALL_EXT(rh, ext, proc, xargs, argsp, xres, resp, secs)	\
	clnt_call_private(rh, ext, proc, xargs,				\
		argsp, xres, resp, secs)

/*
 * enum clnt_stat
 * CLNT_CALL(rh, proc, xargs, argsp, xres, resp, timeout)
 * 	CLIENT *rh;
 *	rpcproc_t proc;
 *	xdrproc_t xargs;
 *	void *argsp;
 *	xdrproc_t xres;
 *	void *resp;
 *	struct timeval timeout;
 */
#define	CLNT_CALL(rh, proc, xargs, argsp, xres, resp, secs)	\
	clnt_call_private(rh, NULL, proc, xargs,		\
		argsp, xres, resp, secs)
#define	clnt_call(rh, proc, xargs, argsp, xres, resp, secs)	\
	clnt_call_private(rh, NULL, proc, xargs,		\
		argsp, xres, resp, secs)

/*
 * void
 * CLNT_ABORT(rh);
 * 	CLIENT *rh;
 */
#define	CLNT_ABORT(rh)	((*(rh)->cl_ops->cl_abort)(rh))
#define	clnt_abort(rh)	((*(rh)->cl_ops->cl_abort)(rh))

/*
 * struct rpc_err
 * CLNT_GETERR(rh);
 * 	CLIENT *rh;
 */
#define	CLNT_GETERR(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))
#define	clnt_geterr(rh,errp)	((*(rh)->cl_ops->cl_geterr)(rh, errp))


/*
 * bool_t
 * CLNT_FREERES(rh, xres, resp);
 * 	CLIENT *rh;
 *	xdrproc_t xres;
 *	void *resp;
 */
#define	CLNT_FREERES(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))
#define	clnt_freeres(rh,xres,resp) ((*(rh)->cl_ops->cl_freeres)(rh,xres,resp))

/*
 * bool_t
 * CLNT_CONTROL(cl, request, info)
 *      CLIENT *cl;
 *      u_int request;
 *      char *info;
 */
#define	CLNT_CONTROL(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))
#define	clnt_control(cl,rq,in) ((*(cl)->cl_ops->cl_control)(cl,rq,in))

/*
 * control operations that apply to both udp and tcp transports
 */
#define CLSET_TIMEOUT		1	/* set timeout (timeval) */
#define CLGET_TIMEOUT		2	/* get timeout (timeval) */
#define CLGET_SERVER_ADDR	3	/* get server's address (sockaddr) */
#define CLGET_FD		6	/* get connections file descriptor */
#define CLGET_SVC_ADDR		7	/* get server's address (netbuf) */
#define CLSET_FD_CLOSE		8	/* close fd while clnt_destroy */
#define CLSET_FD_NCLOSE		9	/* Do not close fd while clnt_destroy */
#define CLGET_XID 		10	/* Get xid */
#define CLSET_XID		11	/* Set xid */
#define CLGET_VERS		12	/* Get version number */
#define CLSET_VERS		13	/* Set version number */
#define CLGET_PROG		14	/* Get program number */
#define CLSET_PROG		15	/* Set program number */
#define CLSET_SVC_ADDR		16	/* get server's address (netbuf) */
#define CLSET_PUSH_TIMOD	17	/* push timod if not already present */
#define CLSET_POP_TIMOD		18	/* pop timod */
/*
 * Connectionless only control operations
 */
#define CLSET_RETRY_TIMEOUT 4   /* set retry timeout (timeval) */
#define CLGET_RETRY_TIMEOUT 5   /* get retry timeout (timeval) */
#define CLSET_ASYNC		19
#define CLSET_CONNECT		20	/* Use connect() for UDP. (int) */

/*
 * Kernel control operations. The default msleep string is "rpcrecv",
 * and sleeps are non-interruptible by default.
 */
#define CLSET_WAITCHAN		21	/* set string to use in msleep call */
#define CLGET_WAITCHAN		22	/* get string used in msleep call */
#define CLSET_INTERRUPTIBLE	23	/* set interruptible flag */
#define CLGET_INTERRUPTIBLE	24	/* set interruptible flag */
#define CLSET_RETRIES		25	/* set retry count for reconnect */
#define CLGET_RETRIES		26	/* get retry count for reconnect */
#define CLSET_PRIVPORT		27	/* set privileged source port flag */
#define CLGET_PRIVPORT		28	/* get privileged source port flag */
#define CLSET_BACKCHANNEL	29	/* set backchannel for socket */
#define	CLSET_TLS		30	/* set TLS for socket */
#define	CLSET_BLOCKRCV		31	/* Temporarily block reception */
#define	CLSET_TLSCERTNAME	32	/* TLS certificate file name */
/* Structure used as the argument for CLSET_RECONUPCALL. */
struct rpc_reconupcall {
	void	(*call)(CLIENT *, void *, struct ucred *);
	void	*arg;
};
#define	CLSET_RECONUPCALL	33	/* Reconnect upcall */

/*
 * void
 * CLNT_DESTROY(rh);
 * 	CLIENT *rh;
 */
#define	CLNT_DESTROY(rh)	((*(rh)->cl_ops->cl_destroy)(rh))
#define	clnt_destroy(rh)	((*(rh)->cl_ops->cl_destroy)(rh))


/*
 * RPCTEST is a test program which is accessible on every rpc
 * transport/port.  It is used for testing, performance evaluation,
 * and network administration.
 */

#define RPCTEST_PROGRAM		((rpcprog_t)1)
#define RPCTEST_VERSION		((rpcvers_t)1)
#define RPCTEST_NULL_PROC	((rpcproc_t)2)
#define RPCTEST_NULL_BATCH_PROC	((rpcproc_t)3)

/*
 * By convention, procedure 0 takes null arguments and returns them
 */

#define NULLPROC ((rpcproc_t)0)

/*
 * Below are the client handle creation routines for the various
 * implementations of client side rpc.  They can return NULL if a
 * creation failure occurs.
 */

/*
 * Generic client creation routine. Supported protocols are those that
 * belong to the nettype namespace (/etc/netconfig).
 */
__BEGIN_DECLS
/*
 *	struct socket *so;			-- socket
 *	struct sockaddr *svcaddr;		-- servers address
 *	rpcprog_t prog;				-- program number
 *	rpcvers_t vers;				-- version number
 *	size_t sendsz;				-- buffer recv size
 *	size_t recvsz;				-- buffer send size
 */
extern CLIENT *clnt_dg_create(struct socket *so,
    struct sockaddr *svcaddr, rpcprog_t program, rpcvers_t version,
    size_t sendsz, size_t recvsz);

/*
 * netlink(4) client that would multicast calls on genetlink(4) family
 * named "rpcnl" (with dynamic id).  The server counterpart of this
 * client is a userland application that uses libc/rpc/svc_nl.c to
 * receive the calls and send replies.
 *
 *	const char *name			-- multicast group name
 */
extern CLIENT *client_nl_create(const char *name, const rpcprog_t prog,
    const rpcvers_t version);

/*
 *	struct socket *so;			-- socket
 *	struct sockaddr *svcaddr;		-- servers address
 *	rpcprog_t prog;				-- program number
 *	rpcvers_t vers;				-- version number
 *	size_t sendsz;				-- buffer recv size
 *	size_t recvsz;				-- buffer send size
 *	int intrflag;				-- is it interruptible
 */
extern CLIENT *clnt_vc_create(struct socket *so,
    struct sockaddr *svcaddr, rpcprog_t program, rpcvers_t version,
    size_t sendsz, size_t recvsz, int intrflag);

/*
 *	struct netconfig *nconf;		-- network type
 *	struct sockaddr *svcaddr;		-- servers address
 *	rpcprog_t prog;				-- program number
 *	rpcvers_t vers;				-- version number
 *	size_t sendsz;				-- buffer recv size
 *	size_t recvsz;				-- buffer send size
 */
extern CLIENT *clnt_reconnect_create(struct netconfig *nconf,
    struct sockaddr *svcaddr, rpcprog_t program, rpcvers_t version,
    size_t sendsz, size_t recvsz);
__END_DECLS


/*
 * Print why creation failed
 */
__BEGIN_DECLS
extern void clnt_pcreateerror(const char *);			/* stderr */
extern char *clnt_spcreateerror(const char *);			/* string */
__END_DECLS

/*
 * Like clnt_perror(), but is more verbose in its output
 */
__BEGIN_DECLS
extern void clnt_perrno(enum clnt_stat);		/* stderr */
extern char *clnt_sperrno(enum clnt_stat);		/* string */
__END_DECLS

/*
 * Print an English error message, given the client error code
 */
__BEGIN_DECLS
extern void clnt_perror(CLIENT *, const char *);	 	/* stderr */
extern char *clnt_sperror(CLIENT *, const char *);		/* string */
__END_DECLS


/*
 * If a creation fails, the following allows the user to figure out why.
 */
struct rpc_createerr {
	enum clnt_stat cf_stat;
	struct rpc_err cf_error; /* useful when cf_stat == RPC_PMAPFAILURE */
};

extern struct rpc_createerr rpc_createerr;

#endif /* !_RPC_CLNT_H_ */
