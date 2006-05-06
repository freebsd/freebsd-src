/* $FreeBSD: src/sys/rpc/rpcclnt.h,v 1.5 2005/01/07 02:29:21 imp Exp $ */
/*	$OpenBSD: nfsmount.h,v 1.11 2002/03/14 01:27:13 millert Exp $	*/
/*	$NetBSD: nfsmount.h,v 1.10 1996/02/18 11:54:03 fvdl Exp $	*/

/*-
 * copyright (c) 2003
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and redistribute
 * this software and such derivative works for any purpose, so long as the name
 * of the university of michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software without specific,
 * written prior authorization.  if the above copyright notice or any other
 * identification of the university of michigan is included in any copy of any
 * portion of this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the university
 * of michigan as to its fitness for any purpose, and without warranty by the
 * university of michigan of any kind, either express or implied, including
 * without limitation the implied warranties of merchantability and fitness for
 * a particular purpose. the regents of the university of michigan shall not be
 * liable for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising out of or in
 * connection with the use of the software, even if it has been or is hereafter
 * advised of the possibility of such damages.
 */

/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfsmount.h	8.3 (Berkeley) 3/30/95
 */


#ifndef _RPCCLNT_H_
#define _RPCCLNT_H_

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#ifdef __OpenBSD__
	#define RPC_EXEC_CTX struct proc *
#else
	#define RPC_EXEC_CTX struct thread *
#endif


#ifdef RPCCLNT_DEBUG
#define RPCDEBUG(args...) do{	\
	if(rpcdebugon != 0){	\
		printf("%s(): ", __func__);\
		printf(args);	\
		printf("\n");	\
	}}while(0)
#else
#define RPCDEBUG(args...)
#endif

/* from nfs/nfs.h */
#define RPC_TICKINTVL     10


/* from nfs/nfsproto.h */
#define RPC_MAXDATA     32768
#define RPC_MAXPKTHDR   404
#define RPC_MAXPACKET   (RPC_MAXPKTHDR + RPC_MAXDATA)

#define RPCX_UNSIGNED   4

/* defines for rpcclnt's rc_flags
   XXX these flags came from the NFSMNT_* flags in OpenBSD's sys/mount.h */
#define RPCCLNT_SOFT            0x001 /* soft mount (hard is details) */
#define RPCCLNT_INT             0x002 /* allow interrupts on hard mounts */
#define RPCCLNT_NOCONN          0x004 /* dont connect the socket (udp) */
#define RPCCLNT_DUMBTIMR        0x010

#define RPCCLNT_SNDLOCK         0x100
#define RPCCLNT_WANTSND         0x200
#define RPCCLNT_RCVLOCK         0x400
#define RPCCLNT_WANTRCV         0x800


/* Flag values for r_flags */
#define R_TIMING        0x01            /* timing request (in mntp) */
#define R_SENT          0x02            /* request has been sent */
#define R_SOFTTERM      0x04            /* soft mnt, too many retries */
#define R_INTR          0x08            /* intr mnt, signal pending */
#define R_SOCKERR       0x10            /* Fatal error on socket */
#define R_TPRINTFMSG    0x20            /* Did a tprintf msg. */
#define R_MUSTRESEND    0x40            /* Must resend request */
#define R_GETONEREP     0x80            /* Probe for one reply only */


#define RPC_HZ          (hz / rpcclnt_ticks) /* Ticks/sec */
#define RPC_TIMEO       (1 * RPC_HZ)    /* Default timeout = 1 second */

#define RPC_MAXREXMIT   100             /* Stop counting after this many */


#define RPCIGNORE_SOERROR(s, e) \
                ((e) != EINTR && (e) != ERESTART && (e) != EWOULDBLOCK && \
                ((s) & PR_CONNREQUIRED) == 0)

#define RPCINT_SIGMASK  (sigmask(SIGINT)|sigmask(SIGTERM)|sigmask(SIGKILL)| \
                         sigmask(SIGHUP)|sigmask(SIGQUIT))

#define RPCMADV(m, s)   (m)->m_data += (s)

#define RPCAUTH_ROOTCREDS NULL

#define RPCCLNTINT_SIGMASK(set)             \
  (SIGISMEMBER(set, SIGINT) || SIGISMEMBER(set, SIGTERM) || \
	 SIGISMEMBER(set, SIGHUP) || SIGISMEMBER(set, SIGKILL) || \
	 SIGISMEMBER(set, SIGQUIT))


#define fxdr_unsigned(t, v) ((t)ntohl((int32_t)(v)))
#define txdr_unsigned(v)  (htonl((int32_t)(v)))


/* global rpcstats 
 * XXX should be per rpcclnt */
struct rpcstats {
	int	rpcretries;
	int rpcrequests;
	int rpctimeouts;
	int rpcunexpected;
	int rpcinvalid;
};

struct rpc_program {
	u_int32_t prog_id;
	u_int32_t prog_version;
	char * prog_name;
};

struct rpc_auth {
	unsigned int auth_type;
};

struct rpctask {
	TAILQ_ENTRY(rpctask) r_chain;
	struct mbuf     *r_mreq;
	struct mbuf     *r_mrep;
	struct mbuf     *r_md;
	caddr_t         r_dpos;

	struct rpcclnt  *r_rpcclnt;

	u_int32_t       r_xid;
	int             r_flags;        /* flags on request, see below */
	int             r_retry;        /* max retransmission count */
	int             r_rexmit;       /* current retrans count */
	int             r_timer;        /* tick counter on reply */
	int             r_procnum;      /* NFS procedure number */
	int             r_rtt;          /* RTT for rpc */
	RPC_EXEC_CTX    r_td;
};

struct rpc_reply {
	struct {
		u_int32_t type;
		u_int32_t status;

		/* used only when reply == RPC_MSGDENIED and
		 * status == RPC_AUTHERR */
		u_int32_t autherr;

		/* rpc mismatch info if reply == RPC_MSGDENIED and
	 	* status == RPC_MISMATCH */
		struct {
			u_int32_t low;
			u_int32_t high;
		} mismatch_info;
	} stat;

	/* head of the mbuf chain */
	struct mbuf * mrep;

	/* mbuf and position of the verification opaque data
	 * note that this is only valid when stat.reply == RPC_MSGACCEPTED */
	u_int32_t verf_type;
	u_int32_t verf_size;
	struct mbuf * verf_md;
	caddr_t verf_dpos;

	/* mbuf and postion of the result of the rpc request */
	struct mbuf * result_md;
	caddr_t result_dpos;
};


/*
 * RPC Client connection context.
 * One allocated on every NFS mount.
 * Holds RPC specific information for mount.
 */
/* XXX: please note that all pointer type variables are just set (not copied),
 *      so it is up to the user to free these values */
struct	rpcclnt {
	int	rc_flag;		/* For RPCCLNT_* flags  */

	int	rc_wsize;		/* Max size of the request data */
	int	rc_rsize;		/* Max size of the response data */
	struct	sockaddr *rc_name;		
	struct	socket *rc_so;		/* Rpc socket */
	int	rc_sotype;		/* Type of socket */
	int	rc_soproto;		/* and protocol */
	int	rc_soflags;		/* pr_flags for socket protocol */

	int	rc_timeo;		/* Init timer for NFSMNT_DUMBTIMR */
	int	rc_retry;		/* Max retries */
	int	rc_srtt[4];		/* Timers for rpcs */
	int	rc_sdrtt[4];
	int	rc_sent;		/* Request send count */
	int	rc_cwnd;		/* Request send window */
	int	rc_timeouts;		/* Request timeouts */

/* XXX: this is not being set!!!! */
	int	rc_deadthresh;		/* Threshold of timeouts-->dead server*/


	/* authentication: */
	/* currently can be RPCAUTH_NULL, RPCAUTH_KERBV4, RPCAUTH_UNIX */
	/* should be kept in XDR form */
	int	rc_authtype;		/* Authenticator type */


#if 0
	/* RPCAUTH_KERB4 */
	int	rc_authlen;		/* and length */
	char	*rc_authstr;		/* Authenticator string */
	int	rc_verflen;
	char	*rc_verfstr;		/* and the verifier */
#endif

	/* RPCAUTH_UNIX*/
	struct rpc_auth * rc_auth;	/* authentication */

#if 0
	/* stored in XDR form (network byte order) */
	unsigned int rc_progid;		/* program id */
	unsigned int rc_progvers;	/* program version */

	/* name of server for log messages */
	const char *rc_servername;	/* for printing error messages */
#else 
	struct rpc_program * rc_prog;
#endif

	/* XXX: this should be removed */
	int rc_proctlen;		/* if == 0 then rc_proct == NULL */
	int * rc_proct;
};

#ifdef __OpenBSD__
extern struct pool rpcreply_pool;
extern struct pool rpcclnt_pool;
#else 
/* MALLOC_DECLARE(M_RPC); */
#endif
extern int rpcdebugon;


#ifdef __OpenBSD__
#define rpcclnt_get(X)	\
	do {	\
	(X) = pool_get(&rpcclnt_pool, PR_WAITOK);	\
	bzero((X), sizeof(struct rpcclnt));	\
	}while(0)

#define rpcclnt_put(X)	\
	do {	\
	if ((X) != NULL){	\
		pool_put(&rpcclnt_pool, (X));	\
	}}while(0)

#else /* !__OpenBSD__ */

/* usage count for module (un)loading */
extern unsigned int rpcclnt_usage;
extern struct mtx rpcclnt_usage_mutex;

void rpcclnt_create(struct rpcclnt ** rpc);
void rpcclnt_destroy(struct rpcclnt * rpc);

#define rpcclnt_get(X) rpcclnt_create(&(X))
#define rpcclnt_put(X) rpcclnt_destroy(X)

#ifdef RPCCLNT_TEST
struct rpcclnt_test_args {
	int nothing;
};
int rpcclnt_test(struct thread *, struct rpcclnt_test_args *);

#define RPC_RETURN(X) do { RPCDEBUG("returning %d", X); return X; }while(0)
#endif /* RPCCLNT_TEST */

#endif /* !__OpenBSD__ */

void rpcclnt_init(void);
void rpcclnt_uninit(void);
#if 0
int rpcclnt_setup(struct rpcclnt *, int, struct sockaddr *, int, int, int, int, const char *, int, int);
#endif

int rpcclnt_setup(struct rpcclnt *, struct rpc_program *, struct sockaddr *, int, int, struct rpc_auth *, int, int, int);


int rpcclnt_connect(struct rpcclnt *, RPC_EXEC_CTX td);
int rpcclnt_reconnect(struct rpctask *, RPC_EXEC_CTX td);
void rpcclnt_disconnect(struct rpcclnt *);
void rpcclnt_safedisconnect(struct rpcclnt *);

void rpcclnt_setauth(struct rpcclnt *, u_int32_t,  u_int32_t, char *, u_int32_t, char *, struct ucred *);
int rpcclnt_request(struct rpcclnt *, struct mbuf *, int, RPC_EXEC_CTX, struct ucred *, struct rpc_reply *);
int rpcclnt_err(struct rpc_reply *);

int rpcclnt_cancelreqs(struct rpcclnt *);
int rpcclnt_sigintr(struct rpcclnt *, struct rpctask *, RPC_EXEC_CTX);


#endif /* _RPCCLNT_H_ */
