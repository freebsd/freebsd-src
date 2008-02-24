/* $FreeBSD: src/sys/rpc/rpcclnt.c,v 1.20 2007/08/06 14:26:03 rwatson Exp $ */
/* $Id: rpcclnt.c,v 1.9 2003/11/05 14:59:03 rees Exp $ */

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
 * Copyright (c) 1989, 1991, 1993, 1995 The Regents of the University of
 * California.  All rights reserved.
 * 
 * This code is derived from software contributed to Berkeley by Rick Macklem at
 * The University of Guelph.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer. 2.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 3. All advertising
 * materials mentioning features or use of this software must display the
 * following acknowledgement: This product includes software developed by the
 * University of California, Berkeley and its contributors. 4. Neither the
 * name of the University nor the names of its contributors may be used to
 * endorse or promote products derived from this software without specific
 * prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * @(#)nfs_socket.c	8.5 (Berkeley) 3/30/95
 */

/* XXX: kill ugly debug strings */
/* XXX: get rid of proct, as it is not even being used... (or keep it so v{2,3}
 *      can run, but clean it up! */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/lock.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>

#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mutex.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nfs/rpcv2.h>

#include <rpc/rpcm_subs.h>
#include <rpc/rpcclnt.h>

/* memory management */
#ifdef __OpenBSD__
struct pool     rpctask_pool;
struct pool     rpcclnt_pool;
#define RPCTASKPOOL_LWM 10
#define RPCTASKPOOL_HWM 40
#else
static          MALLOC_DEFINE(M_RPC, "rpcclnt", "rpc state");
#endif

#define RPC_RETURN(X) do { RPCDEBUG("returning %d", X); return X; }while(0)

/*
 * Estimate rto for an nfs rpc sent via. an unreliable datagram. Use the mean
 * and mean deviation of rtt for the appropriate type of rpc for the frequent
 * rpcs and a default for the others. The justification for doing "other"
 * this way is that these rpcs happen so infrequently that timer est. would
 * probably be stale. Also, since many of these rpcs are non-idempotent, a
 * conservative timeout is desired. getattr, lookup - A+2D read, write     -
 * A+4D other           - nm_timeo
 */
#define	RPC_RTO(n, t) \
	((t) == 0 ? (n)->rc_timeo : \
	 ((t) < 3 ? \
	  (((((n)->rc_srtt[t-1] + 3) >> 2) + (n)->rc_sdrtt[t-1] + 1) >> 1) : \
	  ((((n)->rc_srtt[t-1] + 7) >> 3) + (n)->rc_sdrtt[t-1] + 1)))

#define	RPC_SRTT(s,r)	(r)->r_rpcclnt->rc_srtt[rpcclnt_proct((s),\
				(r)->r_procnum) - 1]

#define	RPC_SDRTT(s,r)	(r)->r_rpcclnt->rc_sdrtt[rpcclnt_proct((s),\
				(r)->r_procnum) - 1]


/*
 * There is a congestion window for outstanding rpcs maintained per mount
 * point. The cwnd size is adjusted in roughly the way that: Van Jacobson,
 * Congestion avoidance and Control, In "Proceedings of SIGCOMM '88". ACM,
 * August 1988. describes for TCP. The cwnd size is chopped in half on a
 * retransmit timeout and incremented by 1/cwnd when each rpc reply is
 * received and a full cwnd of rpcs is in progress. (The sent count and cwnd
 * are scaled for integer arith.) Variants of "slow start" were tried and
 * were found to be too much of a performance hit (ave. rtt 3 times larger),
 * I suspect due to the large rtt that nfs rpcs have.
 */
#define	RPC_CWNDSCALE	256
#define	RPC_MAXCWND	(RPC_CWNDSCALE * 32)
static const int      rpcclnt_backoff[8] = {2, 4, 8, 16, 32, 64, 128, 256,};

/* XXX ugly debug strings */
#define RPC_ERRSTR_ACCEPTED_SIZE 6
char *rpc_errstr_accepted[RPC_ERRSTR_ACCEPTED_SIZE] = {
	"",			/* no good message... */
	"remote server hasn't exported program.",
	"remote server can't support version number.",
	"program can't support procedure.",
	"procedure can't decode params.",
	"remote error.  remote side memory allocation failure?"
};

char *rpc_errstr_denied[2] = {
	"remote server doesnt support rpc version 2!",
	"remote server authentication error."
};

#define RPC_ERRSTR_AUTH_SIZE 6
char *rpc_errstr_auth[RPC_ERRSTR_AUTH_SIZE] = {
	"",
	"auth error: bad credential (seal broken).",
	"auth error: client must begin new session.",
	"auth error: bad verifier (seal broken).",
	"auth error: verifier expired or replayed.",
	"auth error: rejected for security reasons.",
};

/*
 * Static data, mostly RPC constants in XDR form
 */
static u_int32_t rpc_reply, rpc_call, rpc_vers;

/*
 * rpc_msgdenied, rpc_mismatch, rpc_auth_unix, rpc_msgaccepted,
 * rpc_autherr, rpc_auth_kerb;
 */

static u_int32_t rpcclnt_xid = 0;
static u_int32_t rpcclnt_xid_touched = 0;
struct rpcstats rpcstats;
int      rpcclnt_ticks;

SYSCTL_NODE(_kern, OID_AUTO, rpc, CTLFLAG_RD, 0, "RPC Subsystem");

SYSCTL_UINT(_kern_rpc, OID_AUTO, retries, CTLFLAG_RD, &rpcstats.rpcretries, 0, "retries");
SYSCTL_UINT(_kern_rpc, OID_AUTO, request, CTLFLAG_RD, &rpcstats.rpcrequests, 0, "request");
SYSCTL_UINT(_kern_rpc, OID_AUTO, timeouts, CTLFLAG_RD, &rpcstats.rpctimeouts, 0, "timeouts");
SYSCTL_UINT(_kern_rpc, OID_AUTO, unexpected, CTLFLAG_RD, &rpcstats.rpcunexpected, 0, "unexpected");
SYSCTL_UINT(_kern_rpc, OID_AUTO, invalid, CTLFLAG_RD, &rpcstats.rpcinvalid, 0, "invalid");


#ifdef RPCCLNT_DEBUG
int             rpcdebugon = 0;
SYSCTL_UINT(_kern_rpc, OID_AUTO, debug_on, CTLFLAG_RW, &rpcdebugon, 0, "RPC Debug messages");
#endif

/*
 * Queue head for rpctask's
 */
static 
TAILQ_HEAD(, rpctask) rpctask_q;
struct callout	rpcclnt_callout;

#ifdef __OpenBSD__
static int             rpcclnt_send(struct socket *, struct mbuf *, struct mbuf *, struct rpctask *);
static int             rpcclnt_receive(struct rpctask *, struct mbuf **, struct mbuf **, RPC_EXEC_CTX);
#else
static int             rpcclnt_send(struct socket *, struct sockaddr *, struct mbuf *, struct rpctask *);
static int             rpcclnt_receive(struct rpctask *, struct sockaddr **, struct mbuf **, RPC_EXEC_CTX);
#endif

static int             rpcclnt_msg(RPC_EXEC_CTX, const char *, char *);

static int             rpcclnt_reply(struct rpctask *, RPC_EXEC_CTX);
static void            rpcclnt_timer(void *);
static int             rpcclnt_sndlock(int *, struct rpctask *);
static void            rpcclnt_sndunlock(int *);
static int             rpcclnt_rcvlock(struct rpctask *);
static void            rpcclnt_rcvunlock(int *);
#if 0
void            rpcclnt_realign(struct mbuf *, int);
#else
static void	rpcclnt_realign(struct mbuf **, int);
#endif

static struct mbuf    *rpcclnt_buildheader(struct rpcclnt *, int, struct mbuf *, u_int32_t, int *, struct mbuf **, struct ucred *);
static int             rpcm_disct(struct mbuf **, caddr_t *, int, int, caddr_t *);
static u_int32_t       rpcclnt_proct(struct rpcclnt *, u_int32_t);
static int             rpc_adv(struct mbuf **, caddr_t *, int, int);
static void     rpcclnt_softterm(struct rpctask * task);

static int rpcauth_buildheader(struct rpc_auth * auth, struct ucred *, struct mbuf **, caddr_t *);

void
rpcclnt_init(void)
{
#ifdef __OpenBSD__
	static struct timeout rpcclnt_timer_to;
#endif

	rpcclnt_ticks = (hz * RPC_TICKINTVL + 500) / 1000;
	if (rpcclnt_ticks < 1)
		rpcclnt_ticks = 1;
	rpcstats.rpcretries = 0;
	rpcstats.rpcrequests = 0;
	rpcstats.rpctimeouts = 0;
	rpcstats.rpcunexpected = 0;
	rpcstats.rpcinvalid = 0;

	/*
	 * rpc constants how about actually using more than one of these!
	 */

	rpc_reply = txdr_unsigned(RPC_REPLY);
	rpc_vers = txdr_unsigned(RPC_VER2);
	rpc_call = txdr_unsigned(RPC_CALL);
#if 0
	rpc_msgdenied = txdr_unsigned(RPC_MSGDENIED);
	rpc_msgaccepted = txdr_unsigned(RPC_MSGACCEPTED);
	rpc_mismatch = txdr_unsigned(RPC_MISMATCH);
	rpc_autherr = txdr_unsigned(RPC_AUTHERR);
	rpc_auth_unix = txdr_unsigned(RPCAUTH_UNIX);
	rpc_auth_kerb = txdr_unsigned(RPCAUTH_KERB4);
#endif

	/* initialize rpctask queue */
	TAILQ_INIT(&rpctask_q);

#ifdef __OpenBSD__
	/* initialize pools */
	pool_init(&rpctask_pool, sizeof(struct rpctask), 0, 0, RPCTASKPOOL_LWM,
		  "rpctask_p", NULL);
	pool_setlowat(&rpctask_pool, RPCTASKPOOL_LWM);
	pool_sethiwat(&rpctask_pool, RPCTASKPOOL_HWM);

	pool_init(&rpcclnt_pool, sizeof(struct rpcclnt), 0, 0, 1, "rpcclnt_p", NULL);

	/* initialize timers */
	timeout_set(&rpcclnt_timer_to, rpcclnt_timer, &rpcclnt_timer_to);
	rpcclnt_timer(&rpcclnt_timer_to);
#else /* !__OpenBSD__ */
	callout_init(&rpcclnt_callout, 0);
#endif /* !__OpenBSD__ */

	RPCDEBUG("rpc initialed");

	return;
}

void
rpcclnt_uninit(void)
{
  	RPCDEBUG("uninit");
	/* XXX delete sysctl variables? */
	callout_stop(&rpcclnt_callout);
}

int
rpcclnt_setup(clnt, program, addr, sotype, soproto, auth, max_read_size, max_write_size, flags)
    struct rpcclnt * clnt;
    struct rpc_program * program;
    struct sockaddr * addr;
    int sotype;
    int soproto;
    struct rpc_auth * auth;
    int max_read_size;
    int max_write_size;
    int flags;
{
	if (clnt == NULL || program == NULL || addr == NULL || auth == NULL)
	  RPC_RETURN (EFAULT);

	if (program->prog_name == NULL)
	  RPC_RETURN (EFAULT);
	clnt->rc_prog = program;

	clnt->rc_name = addr;
	clnt->rc_sotype = sotype;
	clnt->rc_soproto = soproto;
	clnt->rc_auth = auth;
	clnt->rc_rsize = max_read_size;
	clnt->rc_wsize = max_write_size;
	clnt->rc_flag = flags;

	clnt->rc_proctlen = 0;
	clnt->rc_proct = NULL;

	RPC_RETURN (0);
}

/*
 * Initialize sockets and congestion for a new RPC connection. We do not free
 * the sockaddr if error.
 */
int
rpcclnt_connect(rpc, td)
	struct rpcclnt *rpc;
	RPC_EXEC_CTX td;
{
	struct socket  *so;
	int             s, error, rcvreserve, sndreserve;
	struct sockaddr *saddr;

#ifdef __OpenBSD__
	struct sockaddr_in *sin;
	struct mbuf    *m;
#else
	struct sockaddr_in sin;

	int             soarg;
	struct sockopt  opt;
#endif

	if (rpc == NULL) {
		RPCDEBUG("no rpcclnt struct!\n");
		RPC_RETURN(EFAULT);
	}

	/* create the socket */
	rpc->rc_so = NULL;

	saddr = rpc->rc_name;

	error = socreate(saddr->sa_family, &rpc->rc_so, rpc->rc_sotype,
			 rpc->rc_soproto, td->td_ucred, td);
	if (error) {
		RPCDEBUG("error %d in socreate()", error);
		RPC_RETURN(error);
	}
	so = rpc->rc_so;
	rpc->rc_soflags = so->so_proto->pr_flags;

	/*
	 * Some servers require that the client port be a reserved port
	 * number. We always allocate a reserved port, as this prevents
	 * filehandle disclosure through UDP port capture.
	 */
	if (saddr->sa_family == AF_INET) {
#ifdef __OpenBSD__
		struct mbuf    *mopt;
		int            *ip;
#endif

#ifdef __OpenBSD__
		MGET(mopt, M_TRYWAIT, MT_SOOPTS);
		mopt->m_len = sizeof(int);
		ip = mtod(mopt, int *);
		*ip = IP_PORTRANGE_LOW;

		error = sosetopt(so, IPPROTO_IP, IP_PORTRANGE, mopt);
#else
		soarg = IP_PORTRANGE_LOW;
		bzero(&opt, sizeof(struct sockopt));
		opt.sopt_dir = SOPT_SET;
		opt.sopt_level = IPPROTO_IP;
		opt.sopt_name = IP_PORTRANGE;
		opt.sopt_val = &soarg;
		opt.sopt_valsize = sizeof(soarg);

		error = sosetopt(so, &opt);
#endif
		if (error)
			goto bad;

#ifdef __OpenBSD__
		MGET(m, M_TRYWAIT, MT_SONAME);
		sin = mtod(m, struct sockaddr_in *);
		sin->sin_len = m->m_len = sizeof(struct sockaddr_in);
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = INADDR_ANY;
		sin->sin_port = htons(0);
		error = sobind(so, m);
		m_freem(m);
#else
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		sin.sin_port = htons(0);
		/*
		 * &thread0 gives us root credentials to ensure sobind
		 * will give us a reserved ephemeral port.
		 */
		error = sobind(so, (struct sockaddr *) & sin, &thread0);
#endif
		if (error)
			goto bad;

#ifdef __OpenBSD__
		MGET(mopt, M_TRYWAIT, MT_SOOPTS);
		mopt->m_len = sizeof(int);
		ip = mtod(mopt, int *);
		*ip = IP_PORTRANGE_DEFAULT;
		error = sosetopt(so, IPPROTO_IP, IP_PORTRANGE, mopt);
#else
		soarg = IP_PORTRANGE_DEFAULT;
		bzero(&opt, sizeof(struct sockopt));
		opt.sopt_dir = SOPT_SET;
		opt.sopt_level = IPPROTO_IP;
		opt.sopt_name = IP_PORTRANGE;
		opt.sopt_val = &soarg;
		opt.sopt_valsize = sizeof(soarg);
		error = sosetopt(so, &opt);
#endif
		if (error)
			goto bad;
	}
	/*
	 * Protocols that do not require connections may be optionally left
	 * unconnected for servers that reply from a port other than
	 * NFS_PORT.
	 */
	if (rpc->rc_flag & RPCCLNT_NOCONN) {
		if (rpc->rc_soflags & PR_CONNREQUIRED) {
			error = ENOTCONN;
			goto bad;
		}
	} else {
		error = soconnect(so, saddr, td);
		if (error)
			goto bad;

		/*
		 * Wait for the connection to complete. Cribbed from the
		 * connect system call but with the wait timing out so that
		 * interruptible mounts don't hang here for a long time.
		 */
#ifdef __OpenBSD__
		s = splsoftnet();
#else
		s = splnet();
#endif
		while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
			(void)tsleep((caddr_t) & so->so_timeo, PSOCK,
				     "rpc", 2 * hz);

			/*
			 * XXX needs to catch interrupt signals. something
			 * like this: if ((so->so_state & SS_ISCONNECTING) &&
			 * so->so_error == 0 && rep && (error =
			 * nfs_sigintr(nmp, rep, rep->r_td)) != 0) {
			 * so->so_state &= ~SS_ISCONNECTING; splx(s); goto
			 * bad; }
			 */
		}
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			splx(s);
			goto bad;
		}
		splx(s);
	}
	if (rpc->rc_flag & (RPCCLNT_SOFT | RPCCLNT_INT)) {
		so->so_rcv.sb_timeo = (5 * hz);
		so->so_snd.sb_timeo = (5 * hz);
	} else {
		so->so_rcv.sb_timeo = 0;
		so->so_snd.sb_timeo = 0;
	}


	if (rpc->rc_sotype == SOCK_DGRAM) {
		sndreserve = rpc->rc_wsize + RPC_MAXPKTHDR;
		rcvreserve = rpc->rc_rsize + RPC_MAXPKTHDR;
	} else if (rpc->rc_sotype == SOCK_SEQPACKET) {
		sndreserve = (rpc->rc_wsize + RPC_MAXPKTHDR) * 2;
		rcvreserve = (rpc->rc_rsize + RPC_MAXPKTHDR) * 2;
	} else {
		if (rpc->rc_sotype != SOCK_STREAM)
			panic("rpcclnt_connect() bad sotype");
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
#ifdef __OpenBSD__
			MGET(m, M_TRYWAIT, MT_SOOPTS);
			*mtod(m, int32_t *) = 1;
			m->m_len = sizeof(int32_t);
			sosetopt(so, SOL_SOCKET, SO_KEEPALIVE, m);
#else
			soarg = 1;

			bzero(&opt, sizeof(struct sockopt));
			opt.sopt_dir = SOPT_SET;
			opt.sopt_level = SOL_SOCKET;
			opt.sopt_name = SO_KEEPALIVE;
			opt.sopt_val = &soarg;
			opt.sopt_valsize = sizeof(soarg);
			sosetopt(so, &opt);
#endif
		}
		if (so->so_proto->pr_protocol == IPPROTO_TCP) {
#ifdef __OpenBSD__
			MGET(m, M_TRYWAIT, MT_SOOPTS);
			*mtod(m, int32_t *) = 1;
			m->m_len = sizeof(int32_t);
			sosetopt(so, IPPROTO_TCP, TCP_NODELAY, m);
#else
			soarg = 1;

			bzero(&opt, sizeof(struct sockopt));
			opt.sopt_dir = SOPT_SET;
			opt.sopt_level = IPPROTO_TCP;
			opt.sopt_name = TCP_NODELAY;
			opt.sopt_val = &soarg;
			opt.sopt_valsize = sizeof(soarg);
			sosetopt(so, &opt);
#endif
		}
		sndreserve = (rpc->rc_wsize + RPC_MAXPKTHDR +
			      sizeof(u_int32_t)) * 2;
		rcvreserve = (rpc->rc_rsize + RPC_MAXPKTHDR +
			      sizeof(u_int32_t)) * 2;
	}
	error = soreserve(so, sndreserve, rcvreserve);
	if (error)
		goto bad;
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;

	/* Initialize other non-zero congestion variables */
	rpc->rc_srtt[0] = rpc->rc_srtt[1] = rpc->rc_srtt[2] =
		 rpc->rc_srtt[3] = (RPC_TIMEO << 3);
	rpc->rc_sdrtt[0] = rpc->rc_sdrtt[1] = rpc->rc_sdrtt[2] =
		rpc->rc_sdrtt[3] = 0;
	rpc->rc_cwnd = RPC_MAXCWND / 2;	/* Initial send window */
	rpc->rc_sent = 0;
	rpc->rc_timeouts = 0;
	RPC_RETURN(0);

bad:
	rpcclnt_disconnect(rpc);
	RPC_RETURN(error);
}


/*
 * Reconnect routine:
 * Called when a connection is broken on a reliable protocol.
 * - clean up the old socket
 * - rpcclnt_connect() again
 * - set R_MUSTRESEND for all outstanding requests on mount point
 * If this fails the mount point is DEAD!
 * nb: Must be called with the rpcclnt_sndlock() set on the mount point.
 */
int
rpcclnt_reconnect(rep, td)
	struct rpctask *rep;
	RPC_EXEC_CTX td;
{
	struct rpctask *rp;
	struct rpcclnt *rpc = rep->r_rpcclnt;
	int             error;

	rpcclnt_disconnect(rpc);
	while ((error = rpcclnt_connect(rpc, td)) != 0) {
		if (error == EINTR || error == ERESTART)
			RPC_RETURN(EINTR);
		tsleep(&lbolt, PSOCK, "rpccon", 0);
	}

	/*
	 * Loop through outstanding request list and fix up all requests on
	 * old socket.
	 */
	for (rp = TAILQ_FIRST(&rpctask_q); rp != NULL;
	     rp = TAILQ_NEXT(rp, r_chain)) {
		if (rp->r_rpcclnt == rpc)
			rp->r_flags |= R_MUSTRESEND;
	}
	RPC_RETURN(0);
}

/*
 * RPC transport disconnect. Clean up and unlink.
 */
void
rpcclnt_disconnect(rpc)
	struct rpcclnt *rpc;
{
	struct socket  *so;

	if (rpc->rc_so) {
		so = rpc->rc_so;
		rpc->rc_so = NULL;
		soshutdown(so, 2);
		soclose(so);
	}
}

void
rpcclnt_safedisconnect(struct rpcclnt * rpc)
{
	struct rpctask  dummytask;

	bzero(&dummytask, sizeof(dummytask));
	dummytask.r_rpcclnt = rpc;
	rpcclnt_rcvlock(&dummytask);
	rpcclnt_disconnect(rpc);
	rpcclnt_rcvunlock(&rpc->rc_flag);
}

/*
 * This is the rpc send routine. For connection based socket types, it
 * must be called with an rpcclnt_sndlock() on the socket.
 * "rep == NULL" indicates that it has been called from a server.
 * For the client side:
 * - return EINTR if the RPC is terminated, 0 otherwise
 * - set R_MUSTRESEND if the send fails for any reason
 * - do any cleanup required by recoverable socket errors (?)
 * For the server side:
 * - return EINTR or ERESTART if interrupted by a signal
 * - return EPIPE if a connection is lost for connection based sockets (TCP...)
 * - do any cleanup required by recoverable socket errors (?)
 */
static int
rpcclnt_send(so, nam, top, rep)
	struct socket  *so;
#ifdef __OpenBSD__
	struct mbuf    *nam;
#else
	struct sockaddr *nam;
#endif
	struct mbuf    *top;
	struct rpctask *rep;
{
#ifdef __OpenBSD__
	struct mbuf    *sendnam;
#else
	struct sockaddr *sendnam;
	struct thread  *td = curthread;
#endif
	int error, soflags, flags;

	if (rep) {
		if (rep->r_flags & R_SOFTTERM) {
			m_freem(top);
			RPC_RETURN(EINTR);
		}
		if ((so = rep->r_rpcclnt->rc_so) == NULL) {
			rep->r_flags |= R_MUSTRESEND;
			m_freem(top);
			RPC_RETURN(0);
		}
		rep->r_flags &= ~R_MUSTRESEND;
		soflags = rep->r_rpcclnt->rc_soflags;
	} else
		soflags = so->so_proto->pr_flags;

	if ((soflags & PR_CONNREQUIRED) || (so->so_state & SS_ISCONNECTED))
		sendnam = NULL;
	else
		sendnam = nam;

	if (so->so_type == SOCK_SEQPACKET)
		flags = MSG_EOR;
	else
		flags = 0;

	/*
	 * XXXRW: If/when this code becomes MPSAFE itself, Giant might have
	 * to be conditionally acquired earlier for the stack so has to avoid
	 * lock order reversals with any locks held over rpcclnt_send().
	 */
	error = sosend(so, sendnam, NULL, top, NULL, flags, td);
	if (error) {
		if (rep) {
			log(LOG_INFO, "rpc send error %d for service %s\n", error,
			    rep->r_rpcclnt->rc_prog->prog_name);
			/*
			 * Deal with errors for the client side.
			 */
			if (rep->r_flags & R_SOFTTERM)
				error = EINTR;
			else
				rep->r_flags |= R_MUSTRESEND;
		} else
			log(LOG_INFO, "rpc service send error %d\n", error);

		/*
		 * Handle any recoverable (soft) socket errors here.
		 */
		if (error != EINTR && error != ERESTART &&
		    error != EWOULDBLOCK && error != EPIPE)
			error = 0;
	}
	RPC_RETURN(error);
}

/*
 * Receive a Sun RPC Request/Reply. For SOCK_DGRAM, the work is all done by
 * soreceive(), but for SOCK_STREAM we must deal with the Record Mark and
 * consolidate the data into a new mbuf list. nb: Sometimes TCP passes the
 * data up to soreceive() in long lists of small mbufs. For SOCK_STREAM we
 * must be very careful to read an entire record once we have read any of it,
 * even if the system call has been interrupted.
 */
static int
rpcclnt_receive(rep, aname, mp, td)
	struct rpctask *rep;
#ifdef __OpenBSD__
	struct mbuf   **aname;
#else
	struct sockaddr **aname;
#endif
	struct mbuf   **mp;
	RPC_EXEC_CTX  td;
{
	struct socket  *so;
	struct uio      auio;
	struct iovec    aio;
	struct mbuf    *m;
	struct mbuf    *control;
	u_int32_t       len;
#ifdef __OpenBSD__
	struct mbuf   **getnam;
#else
	struct sockaddr **getnam;
#endif
	int error, sotype, rcvflg;

	/*
	 * Set up arguments for soreceive()
	 */
	*mp = NULL;
	*aname = NULL;
	sotype = rep->r_rpcclnt->rc_sotype;

	/*
	 * For reliable protocols, lock against other senders/receivers in
	 * case a reconnect is necessary. For SOCK_STREAM, first get the
	 * Record Mark to find out how much more there is to get. We must
	 * lock the socket against other receivers until we have an entire
	 * rpc request/reply.
	 */
	if (sotype != SOCK_DGRAM) {
		error = rpcclnt_sndlock(&rep->r_rpcclnt->rc_flag, rep);
		if (error)
			RPC_RETURN(error);
tryagain:
		/*
		 * Check for fatal errors and resending request.
		 */
		/*
		 * Ugh: If a reconnect attempt just happened, rc_so would
		 * have changed. NULL indicates a failed attempt that has
		 * essentially shut down this mount point.
		 */
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM)) {
			rpcclnt_sndunlock(&rep->r_rpcclnt->rc_flag);
			RPC_RETURN(EINTR);
		}
		so = rep->r_rpcclnt->rc_so;
		if (!so) {
			error = rpcclnt_reconnect(rep, td);
			if (error) {
				rpcclnt_sndunlock(&rep->r_rpcclnt->rc_flag);
				RPC_RETURN(error);
			}
			goto tryagain;
		}
		while (rep->r_flags & R_MUSTRESEND) {
			m = m_copym(rep->r_mreq, 0, M_COPYALL, M_TRYWAIT);
			rpcstats.rpcretries++;
			error = rpcclnt_send(so, rep->r_rpcclnt->rc_name, m, rep);
			if (error) {
				if (error == EINTR || error == ERESTART ||
				    (error = rpcclnt_reconnect(rep, td)) != 0) {
					rpcclnt_sndunlock(&rep->r_rpcclnt->rc_flag);
					RPC_RETURN(error);
				}
				goto tryagain;
			}
		}
		rpcclnt_sndunlock(&rep->r_rpcclnt->rc_flag);
		if (sotype == SOCK_STREAM) {
			aio.iov_base = (caddr_t) & len;
			aio.iov_len = sizeof(u_int32_t);
			auio.uio_iov = &aio;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = UIO_READ;
			auio.uio_offset = 0;
			auio.uio_resid = sizeof(u_int32_t);
#ifdef __OpenBSD__
			auio.uio_procp = td;
#else
			auio.uio_td = td;
#endif
			do {
				rcvflg = MSG_WAITALL;
				error = soreceive(so, NULL, &auio, NULL, NULL, &rcvflg);
				if (error == EWOULDBLOCK && rep) {
					if (rep->r_flags & R_SOFTTERM)
						RPC_RETURN(EINTR);
				}
			} while (error == EWOULDBLOCK);
			if (!error && auio.uio_resid > 0) {
				log(LOG_INFO,
				"short receive (%zu/%zu) from rpc server %s\n",
				    sizeof(u_int32_t) - auio.uio_resid,
				    sizeof(u_int32_t),
				    rep->r_rpcclnt->rc_prog->prog_name);
				error = EPIPE;
			}
			if (error)
				goto errout;
			len = ntohl(len) & ~0x80000000;
			/*
			 * This is SERIOUS! We are out of sync with the
			 * sender and forcing a disconnect/reconnect is all I
			 * can do.
			 */
			if (len > RPC_MAXPACKET) {
				log(LOG_ERR, "%s (%d) from rpc server %s\n",
				    "impossible packet length",
				    len,
				    rep->r_rpcclnt->rc_prog->prog_name);
				error = EFBIG;
				goto errout;
			}
			auio.uio_resid = len;
			do {
				rcvflg = MSG_WAITALL;
				error = soreceive(so, NULL, &auio, mp, NULL, &rcvflg);
			} while (error == EWOULDBLOCK || error == EINTR ||
				 error == ERESTART);
			if (!error && auio.uio_resid > 0) {
				log(LOG_INFO,
				"short receive (%d/%d) from rpc server %s\n",
				    len - auio.uio_resid, len,
				    rep->r_rpcclnt->rc_prog->prog_name);
				error = EPIPE;
			}
		} else {
			/*
			 * NB: Since uio_resid is big, MSG_WAITALL is ignored
			 * and soreceive() will return when it has either a
			 * control msg or a data msg. We have no use for
			 * control msg., but must grab them and then throw
			 * them away so we know what is going on.
			 */
			auio.uio_resid = len = 100000000;	/* Anything Big */
#ifdef __OpenBSD__
			auio.uio_procp = td;
#else
			auio.uio_td = td;
#endif
			do {
				rcvflg = 0;
				error = soreceive(so, NULL, &auio, mp, &control, &rcvflg);
				if (control)
					m_freem(control);
				if (error == EWOULDBLOCK && rep) {
					if (rep->r_flags & R_SOFTTERM)
						RPC_RETURN(EINTR);
				}
			} while (error == EWOULDBLOCK ||
				 (!error && *mp == NULL && control));
			if ((rcvflg & MSG_EOR) == 0)
				printf("Egad!!\n");
			if (!error && *mp == NULL)
				error = EPIPE;
			len -= auio.uio_resid;
		}
errout:
		if (error && error != EINTR && error != ERESTART) {
			m_freem(*mp);
			*mp = (struct mbuf *) 0;
			if (error != EPIPE)
				log(LOG_INFO,
				    "receive error %d from rpc server %s\n",
				    error,
				    rep->r_rpcclnt->rc_prog->prog_name);
			error = rpcclnt_sndlock(&rep->r_rpcclnt->rc_flag, rep);
			if (!error)
				error = rpcclnt_reconnect(rep, td);
			if (!error)
				goto tryagain;
		}
	} else {
		if ((so = rep->r_rpcclnt->rc_so) == NULL)
			RPC_RETURN(EACCES);
		if (so->so_state & SS_ISCONNECTED)
			getnam = NULL;
		else
			getnam = aname;
		auio.uio_resid = len = 1000000;
#ifdef __OpenBSD__
		auio.uio_procp = td;
#else
		auio.uio_td = td;
#endif

		do {
			rcvflg = 0;
			error = soreceive(so, getnam, &auio, mp, NULL, &rcvflg);
			RPCDEBUG("soreceive returns %d", error);
			if (error == EWOULDBLOCK && (rep->r_flags & R_SOFTTERM)) {
				RPCDEBUG("wouldblock && softerm -> EINTR");
				RPC_RETURN(EINTR);
			}
		} while (error == EWOULDBLOCK);
		len -= auio.uio_resid;
	}
	if (error) {
		m_freem(*mp);
		*mp = NULL;
	} else {
		/*
		 * Search for any mbufs that are not a multiple of 4 bytes
		 * long or with m_data not longword aligned. These could
		 * cause pointer alignment problems, so copy them to well
		 * aligned mbufs.
		 */
		rpcclnt_realign(mp, 5 * RPCX_UNSIGNED);
	}
	RPC_RETURN(error);
}


/*
 * Implement receipt of reply on a socket. We must search through the list of
 * received datagrams matching them with outstanding requests using the xid,
 * until ours is found.
 */
/* ARGSUSED */
static int
rpcclnt_reply(myrep, td)
	struct rpctask *myrep;
	RPC_EXEC_CTX td;
{
	struct rpctask *rep;
	struct rpcclnt *rpc = myrep->r_rpcclnt;
	int32_t         t1;
	struct mbuf    *mrep, *md;
#ifdef __OpenBSD__
	struct mbuf    *nam;
#else
	struct sockaddr *nam;
#endif
	u_int32_t       rxid, *tl;
	caddr_t         dpos, cp2;
	int             error;

	/*
	 * Loop around until we get our own reply
	 */
	for (;;) {
		/*
		 * Lock against other receivers so that I don't get stuck in
		 * sbwait() after someone else has received my reply for me.
		 * Also necessary for connection based protocols to avoid
		 * race conditions during a reconnect.
		 */
		error = rpcclnt_rcvlock(myrep);
		if (error)
			RPC_RETURN(error);
		/* Already received, bye bye */
		if (myrep->r_mrep != NULL) {
			rpcclnt_rcvunlock(&rpc->rc_flag);
			RPC_RETURN(0);
		}
		/*
		 * Get the next Rpc reply off the socket
		 */
		error = rpcclnt_receive(myrep, &nam, &mrep, td);

		rpcclnt_rcvunlock(&rpc->rc_flag);

		if (error) {
			/*
			 * Ignore routing errors on connectionless
			 * protocols??
			 */
			if (RPCIGNORE_SOERROR(rpc->rc_soflags, error)) {
				rpc->rc_so->so_error = 0;
				if (myrep->r_flags & R_GETONEREP)
					RPC_RETURN(0);
				RPCDEBUG("ingoring routing error on connectionless protocol.");
				continue;
			}
			RPC_RETURN(error);
		}
#ifdef __OpenBSD__
		if (nam)
			m_freem(nam);
#else
		if (nam)
			FREE(nam, M_SONAME);
#endif

		/*
		 * Get the xid and check that it is an rpc reply
		 */
		md = mrep;
		dpos = mtod(md, caddr_t);
		rpcm_dissect(tl, u_int32_t *, 2 * RPCX_UNSIGNED);
		rxid = *tl++;
		if (*tl != rpc_reply) {
			rpcstats.rpcinvalid++;
			m_freem(mrep);
rpcmout:
			if (myrep->r_flags & R_GETONEREP)
				RPC_RETURN(0);
			continue;
		}
		/*
		 * Loop through the request list to match up the reply Iff no
		 * match, just drop the datagram
		 */
		TAILQ_FOREACH(rep, &rpctask_q, r_chain) {
			if (rep->r_mrep == NULL && rxid == rep->r_xid) {
				/* Found it.. */
				rep->r_mrep = mrep;
				rep->r_md = md;
				rep->r_dpos = dpos;

				/*
				 * Update congestion window. Do the additive
				 * increase of one rpc/rtt.
				 */
				if (rpc->rc_cwnd <= rpc->rc_sent) {
					rpc->rc_cwnd +=
						(RPC_CWNDSCALE * RPC_CWNDSCALE +
					(rpc->rc_cwnd >> 1)) / rpc->rc_cwnd;
					if (rpc->rc_cwnd > RPC_MAXCWND)
						rpc->rc_cwnd = RPC_MAXCWND;
				}
				rep->r_flags &= ~R_SENT;
				rpc->rc_sent -= RPC_CWNDSCALE;
				/*
				 * Update rtt using a gain of 0.125 on the
				 * mean and a gain of 0.25 on the deviation.
				 */
				if (rep->r_flags & R_TIMING) {
					/*
					 * Since the timer resolution of
					 * NFS_HZ is so course, it can often
					 * result in r_rtt == 0. Since r_rtt
					 * == N means that the actual rtt is
					 * between N+dt and N+2-dt ticks, add
					 * 1.
					 */
					t1 = rep->r_rtt + 1;
					t1 -= (RPC_SRTT(rpc, rep) >> 3);
					RPC_SRTT(rpc, rep) += t1;
					if (t1 < 0)
						t1 = -t1;
					t1 -= (RPC_SDRTT(rpc, rep) >> 2);
					RPC_SDRTT(rpc, rep) += t1;
				}
				rpc->rc_timeouts = 0;
				break;
			}
		}
		/*
		 * If not matched to a request, drop it. If it's mine, get
		 * out.
		 */
		if (rep == 0) {
			rpcstats.rpcunexpected++;
			RPCDEBUG("rpc reply not matched\n");
			m_freem(mrep);
		} else if (rep == myrep) {
			if (rep->r_mrep == NULL)
				panic("rpcreply nil");
			RPC_RETURN(0);
		}
		if (myrep->r_flags & R_GETONEREP)
			RPC_RETURN(0);
	}
}

/* XXX: ignores tryagain! */
/*
 * code from nfs_request - goes something like this
 *	- fill in task struct
 *	- links task into list
 *	- calls rpcclnt_send() for first transmit
 *	- calls rpcclnt_reply() to get reply
 *	- fills in reply (which should be initialized prior to
 *	  calling), which is valid when 0 is returned and is
 *	  NEVER freed in this function
 * 
 * nb: always frees the request header, but NEVER frees 'mrest'
 * 
 * rpcclnt_setauth() should be used before calling this. EAUTH is returned if
 * authentication fails.
 *
 * note that reply->result_* are invalid unless reply->type ==
 * RPC_MSGACCEPTED and reply->status == RPC_SUCCESS and that reply->verf_*
 * are invalid unless reply->type == RPC_MSGACCEPTED
 */
int
rpcclnt_request(rpc, mrest, procnum, td, cred, reply)
	struct rpcclnt *rpc;
	struct mbuf    *mrest;
	int             procnum;
	RPC_EXEC_CTX	td;
	struct ucred   *cred;
	struct rpc_reply *reply;
{
	struct mbuf    *m, *mrep;
	struct rpctask *task;
	u_int32_t      *tl;
	struct mbuf    *md, *mheadend;
	caddr_t         dpos, cp2;
	int             t1, s, error = 0, mrest_len;
	u_int32_t       xid;

#ifdef __OpenBSD__
	task = pool_get(&rpctask_pool, PR_WAITOK);
#else
	MALLOC(task, struct rpctask *, sizeof(struct rpctask), M_RPC, (M_WAITOK | M_ZERO));
#endif

	task->r_rpcclnt = rpc;
	task->r_procnum = procnum;
	task->r_td = td;

	mrest_len = m_length(mrest, NULL);

	m = rpcclnt_buildheader(rpc, procnum, mrest, mrest_len, &xid, &mheadend,
	    cred);
	/*
	 * This can happen if the auth_type is neither UNIX or NULL
	 */
	if (m == NULL) {
#ifdef __OpenBSD__
		pool_put(&rpctask_pool, task);
#else
		FREE(task, M_RPC);
#endif
		error = EPROTONOSUPPORT;
		goto rpcmout;
	}

	/*
	 * For stream protocols, insert a Sun RPC Record Mark.
	 */
	if (rpc->rc_sotype == SOCK_STREAM) {
		M_PREPEND(m, RPCX_UNSIGNED, M_TRYWAIT);
		*mtod(m, u_int32_t *) = htonl(0x80000000 |
					 (m->m_pkthdr.len - RPCX_UNSIGNED));
	}
	task->r_mreq = m;
	task->r_xid = xid;

	if (rpc->rc_flag & RPCCLNT_SOFT)
		task->r_retry = rpc->rc_retry;
	else
		task->r_retry = RPC_MAXREXMIT + 1;	/* past clip limit */
	task->r_rtt = task->r_rexmit = 0;

	if (rpcclnt_proct(rpc, procnum) > 0)
		task->r_flags = R_TIMING;
	else
		task->r_flags = 0;
	task->r_mrep = NULL;

	/*
	 * Do the client side RPC.
	 */
	rpcstats.rpcrequests++;

	/*
	 * Chain request into list of outstanding requests. Be sure to put it
	 * LAST so timer finds oldest requests first.
	 */
	s = splsoftclock();
	if (TAILQ_EMPTY(&rpctask_q))
		callout_reset(&rpcclnt_callout, rpcclnt_ticks, rpcclnt_timer,
		    NULL);
	TAILQ_INSERT_TAIL(&rpctask_q, task, r_chain);

	/*
	 * If backing off another request or avoiding congestion, don't send
	 * this one now but let timer do it. If not timing a request, do it
	 * now.
	 */
	if (rpc->rc_so && (rpc->rc_sotype != SOCK_DGRAM ||
			   (rpc->rc_flag & RPCCLNT_DUMBTIMR) ||
			   rpc->rc_sent < rpc->rc_cwnd)) {
		splx(s);

		if (rpc->rc_soflags & PR_CONNREQUIRED)
			error = rpcclnt_sndlock(&rpc->rc_flag, task);
		if (!error) {
			error = rpcclnt_send(rpc->rc_so, rpc->rc_name,
					     m_copym(m, 0, M_COPYALL, M_TRYWAIT),
					     task);
			if (rpc->rc_soflags & PR_CONNREQUIRED)
				rpcclnt_sndunlock(&rpc->rc_flag);
		}
		if (!error && (task->r_flags & R_MUSTRESEND) == 0) {
			rpc->rc_sent += RPC_CWNDSCALE;
			task->r_flags |= R_SENT;
		}
	} else {
		splx(s);
		task->r_rtt = -1;
	}

	/*
	 * Wait for the reply from our send or the timer's.
	 */
	if (!error || error == EPIPE)
		error = rpcclnt_reply(task, td);

	/*
	 * RPC done, unlink the request.
	 */
	s = splsoftclock();
	TAILQ_REMOVE(&rpctask_q, task, r_chain);
	if (TAILQ_EMPTY(&rpctask_q))
		callout_stop(&rpcclnt_callout);
	splx(s);

	/*
	 * Decrement the outstanding request count.
	 */
	if (task->r_flags & R_SENT) {
		task->r_flags &= ~R_SENT;	/* paranoia */
		rpc->rc_sent -= RPC_CWNDSCALE;
	}
	/*
	 * If there was a successful reply and a tprintf msg. tprintf a
	 * response.
	 */
	if (!error && (task->r_flags & R_TPRINTFMSG)) {
		mtx_lock(&Giant);
		rpcclnt_msg(task->r_td, rpc->rc_prog->prog_name,
			    "is alive again");
		mtx_unlock(&Giant);
	}

	/* free request header (leaving mrest) */
	mheadend->m_next = NULL;
	m_freem(task->r_mreq);

	/* initialize reply */
	reply->mrep = task->r_mrep;
	reply->verf_md = NULL;
	reply->result_md = NULL;

	mrep = task->r_mrep;
	md = task->r_md;
	dpos = task->r_dpos;

	/* task structure is no longer needed */
#ifdef __OpenBSD__
	pool_put(&rpctask_pool, task);
#else
	FREE(task, M_RPC);
#endif

	if (error)
		goto rpcmout;

	/*
	 * break down the rpc header and check if ok
	 */

	rpcm_dissect(tl, u_int32_t *, RPCX_UNSIGNED);
	reply->stat.type = fxdr_unsigned(u_int32_t, *tl);

	if (reply->stat.type == RPC_MSGDENIED) {
		rpcm_dissect(tl, u_int32_t *, RPCX_UNSIGNED);
		reply->stat.status = fxdr_unsigned(u_int32_t, *tl);

		switch (reply->stat.status) {
		case RPC_MISMATCH:
			rpcm_dissect(tl, u_int32_t *, 2 * RPCX_UNSIGNED);
			reply->stat.mismatch_info.low = fxdr_unsigned(u_int32_t, *tl++);
			reply->stat.mismatch_info.high = fxdr_unsigned(u_int32_t, *tl);
			error = EOPNOTSUPP;
			break;
		case RPC_AUTHERR:
			rpcm_dissect(tl, u_int32_t *, RPCX_UNSIGNED);
			reply->stat.autherr = fxdr_unsigned(u_int32_t, *tl);
			error = EACCES;
			break;
		default:
			error = EBADRPC;
			break;
		}
		goto rpcmout;
	} else if (reply->stat.type != RPC_MSGACCEPTED) {
		error = EBADRPC;
  		goto rpcmout;
  	}

	rpcm_dissect(tl, u_int32_t *, 2 * RPCX_UNSIGNED);

	reply->verf_md = md;
	reply->verf_dpos = dpos;

	reply->verf_type = fxdr_unsigned(u_int32_t, *tl++);
	reply->verf_size = fxdr_unsigned(u_int32_t, *tl);

	if (reply->verf_size != 0)
		rpcm_adv(rpcm_rndup(reply->verf_size));

	rpcm_dissect(tl, u_int32_t *, RPCX_UNSIGNED);
	reply->stat.status = fxdr_unsigned(u_int32_t, *tl);

	if (reply->stat.status == RPC_SUCCESS) {
		if ((uint32_t)(dpos - mtod(md, caddr_t)) >= md->m_len) {
			RPCDEBUG("where is the next mbuf?");
			RPCDEBUG("%d -> %d",
			    (int)(dpos - mtod(md, caddr_t)), md->m_len);
			if (md->m_next == NULL) {
				error = EBADRPC;
				goto rpcmout;
			} else {
				reply->result_md = md->m_next;
				reply->result_dpos = mtod(reply->result_md,
				    caddr_t);
			}
		} else {
			reply->result_md = md;
			reply->result_dpos = dpos;
		}
	} else if (reply->stat.status == RPC_PROGMISMATCH) {
		rpcm_dissect(tl, u_int32_t *, 2 * RPCX_UNSIGNED);
		reply->stat.mismatch_info.low = fxdr_unsigned(u_int32_t, *tl++);
		reply->stat.mismatch_info.high = fxdr_unsigned(u_int32_t, *tl);
		error = EOPNOTSUPP;
		goto rpcmout;
	} else {
		error = EPROTONOSUPPORT;
		goto rpcmout;
	}
	error = 0;

rpcmout:
	RPC_RETURN(error);
}


/*
 * RPC timer routine
 * Scan the rpctask list and retranmit any requests that have timed out.
 * To avoid retransmission attempts on STREAM sockets (in the future) make
 * sure to set the r_retry field to 0 (implies nm_retry == 0).
 */
void
rpcclnt_timer(arg)
	void           *arg;
{
#ifdef __OpenBSD__
	struct timeout *to = (struct timeout *) arg;
#endif
	struct rpctask *rep;
	struct mbuf    *m;
	struct socket  *so;
	struct rpcclnt *rpc;
	int             timeo;
	int             s, error;

#ifndef __OpenBSD__
	struct thread  *td = curthread;
#endif

#ifdef __OpenBSD__
	s = splsoftnet();
#else
	s = splnet();
#endif
	mtx_lock(&Giant);	/* rpc_msg -> tprintf */
	TAILQ_FOREACH(rep, &rpctask_q, r_chain) {
		rpc = rep->r_rpcclnt;
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM))
			continue;
		if (rpcclnt_sigintr(rpc, rep, rep->r_td)) {
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (rep->r_rtt >= 0) {
			rep->r_rtt++;
			if (rpc->rc_flag & RPCCLNT_DUMBTIMR)
				timeo = rpc->rc_timeo;
			else
				timeo = RPC_RTO(rpc, rpcclnt_proct(rep->r_rpcclnt,
							   rep->r_procnum));
			if (rpc->rc_timeouts > 0)
				timeo *= rpcclnt_backoff[rpc->rc_timeouts - 1];
			if (rep->r_rtt <= timeo)
				continue;
			if (rpc->rc_timeouts < 8)
				rpc->rc_timeouts++;
		}
		/*
		 * Check for server not responding
		 */
		if ((rep->r_flags & R_TPRINTFMSG) == 0 &&
		    rep->r_rexmit > rpc->rc_deadthresh) {
			rpcclnt_msg(rep->r_td, rpc->rc_prog->prog_name,
				    "not responding");
			rep->r_flags |= R_TPRINTFMSG;
		}
		if (rep->r_rexmit >= rep->r_retry) {	/* too many */
			rpcstats.rpctimeouts++;
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (rpc->rc_sotype != SOCK_DGRAM) {
			if (++rep->r_rexmit > RPC_MAXREXMIT)
				rep->r_rexmit = RPC_MAXREXMIT;
			continue;
		}
		if ((so = rpc->rc_so) == NULL)
			continue;

		/*
		 * If there is enough space and the window allows.. Resend it
		 * Set r_rtt to -1 in case we fail to send it now.
		 */
		rep->r_rtt = -1;
		if (sbspace(&so->so_snd) >= rep->r_mreq->m_pkthdr.len &&
		    ((rpc->rc_flag & RPCCLNT_DUMBTIMR) ||
		     (rep->r_flags & R_SENT) ||
		     rpc->rc_sent < rpc->rc_cwnd) &&
		    (m = m_copym(rep->r_mreq, 0, M_COPYALL, M_DONTWAIT))) {
			if ((rpc->rc_flag & RPCCLNT_NOCONN) == 0)
				error = (*so->so_proto->pr_usrreqs->pru_send) (so, 0, m,
							    NULL, NULL, td);
			else
				error = (*so->so_proto->pr_usrreqs->pru_send)(so, 0, m, rpc->rc_name, NULL, td);
			if (error) {
				if (RPCIGNORE_SOERROR(rpc->rc_soflags, error))
					so->so_error = 0;
			} else {
				/*
				 * Iff first send, start timing else turn
				 * timing off, backoff timer and divide
				 * congestion window by 2.
				 */
				if (rep->r_flags & R_SENT) {
					rep->r_flags &= ~R_TIMING;
					if (++rep->r_rexmit > RPC_MAXREXMIT)
						rep->r_rexmit = RPC_MAXREXMIT;
					rpc->rc_cwnd >>= 1;
					if (rpc->rc_cwnd < RPC_CWNDSCALE)
						rpc->rc_cwnd = RPC_CWNDSCALE;
					rpcstats.rpcretries++;
				} else {
					rep->r_flags |= R_SENT;
					rpc->rc_sent += RPC_CWNDSCALE;
				}
				rep->r_rtt = 0;
			}
		}
	}
	mtx_unlock(&Giant);	/* rpc_msg -> tprintf */
	splx(s);

#ifdef __OpenBSD__
	timeout_add(rpcclnt_timer, to, rpcclnt_ticks);
#else
	callout_reset(&rpcclnt_callout, rpcclnt_ticks, rpcclnt_timer, NULL);
#endif
}

/*
 * Test for a termination condition pending on the process. This is used for
 * RPCCLNT_INT mounts.
 */
int
rpcclnt_sigintr(rpc, task, pr)
	struct rpcclnt *rpc;
	struct rpctask *task;
	RPC_EXEC_CTX pr;
{
	struct proc    *p;

	sigset_t        tmpset;

	if (rpc == NULL) 
		return EFAULT;

	/* XXX deal with forced unmounts */

	if (task && (task->r_flags & R_SOFTTERM))
		RPC_RETURN(EINTR);

	if (!(rpc->rc_flag & RPCCLNT_INT))
		RPC_RETURN(0);

	if (pr == NULL)
		return (0);

#ifdef __OpenBSD__
	p = pr;
	if (p && p->p_siglist &&
	    (((p->p_siglist & ~p->p_sigmask) & ~p->p_sigignore) &
	     RPCINT_SIGMASK))
		RPC_RETURN(EINTR);
#else
	p = pr->td_proc;
	PROC_LOCK(p);
	tmpset = p->p_siglist;
	SIGSETNAND(tmpset, pr->td_sigmask);
	mtx_lock(&p->p_sigacts->ps_mtx);
	SIGSETNAND(tmpset, p->p_sigacts->ps_sigignore);
	mtx_unlock(&p->p_sigacts->ps_mtx);
	if (SIGNOTEMPTY(p->p_siglist) && RPCCLNTINT_SIGMASK(tmpset)) {
		PROC_UNLOCK(p);
		RPC_RETURN(EINTR);
	}
	PROC_UNLOCK(p);
#endif
	RPC_RETURN(0);
}

/*
 * Lock a socket against others. Necessary for STREAM sockets to ensure you
 * get an entire rpc request/reply and also to avoid race conditions between
 * the processes with nfs requests in progress when a reconnect is necessary.
 */
static int
rpcclnt_sndlock(flagp, task)
	int            *flagp;
	struct rpctask *task;
{
	RPC_EXEC_CTX p;
	int             slpflag = 0, slptimeo = 0;

	p = task->r_td;
	if (task->r_rpcclnt->rc_flag & RPCCLNT_INT)
		slpflag = PCATCH;
	while (*flagp & RPCCLNT_SNDLOCK) {
		if (rpcclnt_sigintr(task->r_rpcclnt, task, p))
			RPC_RETURN(EINTR);
		*flagp |= RPCCLNT_WANTSND;
		(void)tsleep((caddr_t) flagp, slpflag | (PZERO - 1), "rpcsndlck",
			     slptimeo);
		if (slpflag == PCATCH) {
			slpflag = 0;
			slptimeo = 2 * hz;
		}
	}
	*flagp |= RPCCLNT_SNDLOCK;
	RPC_RETURN(0);
}

/*
 * Unlock the stream socket for others.
 */
static void
rpcclnt_sndunlock(flagp)
	int            *flagp;
{

	if ((*flagp & RPCCLNT_SNDLOCK) == 0)
		panic("rpc sndunlock");
	*flagp &= ~RPCCLNT_SNDLOCK;
	if (*flagp & RPCCLNT_WANTSND) {
		*flagp &= ~RPCCLNT_WANTSND;
		wakeup((caddr_t) flagp);
	}
}

static int
rpcclnt_rcvlock(task)
	struct rpctask *task;
{
	int            *flagp = &task->r_rpcclnt->rc_flag;
	int             slpflag, slptimeo = 0;

	if (*flagp & RPCCLNT_INT)
		slpflag = PCATCH;
	else
		slpflag = 0;
	while (*flagp & RPCCLNT_RCVLOCK) {
		if (rpcclnt_sigintr(task->r_rpcclnt, task, task->r_td))
			RPC_RETURN(EINTR);
		*flagp |= RPCCLNT_WANTRCV;
		(void)tsleep((caddr_t) flagp, slpflag | (PZERO - 1), "rpcrcvlk",
			     slptimeo);
		if (slpflag == PCATCH) {
			slpflag = 0;
			slptimeo = 2 * hz;
		}
	}
	*flagp |= RPCCLNT_RCVLOCK;
	RPC_RETURN(0);
}

/*
 * Unlock the stream socket for others.
 */
static void
rpcclnt_rcvunlock(flagp)
	int            *flagp;
{

	if ((*flagp & RPCCLNT_RCVLOCK) == 0)
		panic("nfs rcvunlock");
	*flagp &= ~RPCCLNT_RCVLOCK;
	if (*flagp & RPCCLNT_WANTRCV) {
		*flagp &= ~RPCCLNT_WANTRCV;
		wakeup((caddr_t) flagp);
	}
}

#if 0
/*
 * Check for badly aligned mbuf data areas and realign data in an mbuf list
 * by copying the data areas up, as required.
 */
void
rpcclnt_realign(m, hsiz)
	struct mbuf    *m;
	int             hsiz;
{
	struct mbuf    *m2;
	int             siz, mlen, olen;
	caddr_t         tcp, fcp;
	struct mbuf    *mnew;

	while (m) {
		/*
		 * This never happens for UDP, rarely happens for TCP but
		 * frequently happens for iso transport.
		 */
		if ((m->m_len & 0x3) || (mtod(m, long)&0x3)) {
			olen = m->m_len;
			fcp = mtod(m, caddr_t);
			if ((long)fcp & 0x3) {
				if (m->m_flags & M_PKTHDR)
					m_tag_delete_chain(m, NULL);
				m->m_flags &= ~M_PKTHDR;
				if (m->m_flags & M_EXT)
					m->m_data = m->m_ext.ext_buf +
						((m->m_ext.ext_size - olen) & ~0x3);
				else
					m->m_data = m->m_dat;
			}
			m->m_len = 0;
			tcp = mtod(m, caddr_t);
			mnew = m;
			m2 = m->m_next;

			/*
			 * If possible, only put the first invariant part of
			 * the RPC header in the first mbuf.
			 */
			mlen = M_TRAILINGSPACE(m);
			if (olen <= hsiz && mlen > hsiz)
				mlen = hsiz;

			/* Loop through the mbuf list consolidating data. */
			while (m) {
				while (olen > 0) {
					if (mlen == 0) {
						if (m2->m_flags & M_PKTHDR)
							m_tag_delete_chain(m2, NULL);
						m2->m_flags &= ~M_PKTHDR;
						if (m2->m_flags & M_EXT)
							m2->m_data = m2->m_ext.ext_buf;
						else
							m2->m_data = m2->m_dat;
						m2->m_len = 0;
						mlen = M_TRAILINGSPACE(m2);
						tcp = mtod(m2, caddr_t);
						mnew = m2;
						m2 = m2->m_next;
					}
					siz = min(mlen, olen);
					if (tcp != fcp)
						bcopy(fcp, tcp, siz);
					mnew->m_len += siz;
					mlen -= siz;
					olen -= siz;
					tcp += siz;
					fcp += siz;
				}
				m = m->m_next;
				if (m) {
					olen = m->m_len;
					fcp = mtod(m, caddr_t);
				}
			}

			/*
			 * Finally, set m_len == 0 for any trailing mbufs
			 * that have been copied out of.
			 */
			while (m2) {
				m2->m_len = 0;
				m2 = m2->m_next;
			}
			return;
		}
		m = m->m_next;
	}
}
#else
static void
rpcclnt_realign(struct mbuf **pm, int hsiz)
{
	struct mbuf *m;
	struct mbuf *n = NULL;
	int off = 0;

	RPCDEBUG("in rpcclnt_realign()");

	while ((m = *pm) != NULL) {
	    if ((m->m_len & 0x3) || (mtod(m, intptr_t) & 0x3)) {
	        MGET(n, M_TRYWAIT, MT_DATA);
	        if (m->m_len >= MINCLSIZE) {
	            MCLGET(n, M_TRYWAIT);
	        }
	        n->m_len = 0;
	        break;
	    }
	    pm = &m->m_next;
	}

	/*
	* If n is non-NULL, loop on m copying data, then replace the
	* portion of the chain that had to be realigned.
	*/
	if (n != NULL) {
	    while (m) {
	        m_copyback(n, off, m->m_len, mtod(m, caddr_t));
	        off += m->m_len;
	        m = m->m_next;
	    }
	    m_freem(*pm);
	    *pm = n;
	}

	RPCDEBUG("leave rpcclnt_realign()");
}
#endif

static int
rpcclnt_msg(p, server, msg)
	RPC_EXEC_CTX   p;
	const char     *server;
	char           *msg;
{
#ifdef __OpenBSD__
	tpr_t           tpr;
	struct proc    *pr = p;

	if (p)
		tpr = tprintf_open(p);
	else
		tpr = NULL;
	tprintf(tpr, "rpc server %s: %s\n", server, msg);
	tprintf_close(tpr);
	RPC_RETURN(0);
#else
	GIANT_REQUIRED;

	tprintf(p ? p->td_proc : NULL, LOG_INFO,
		"nfs server %s: %s\n", server, msg);
	RPC_RETURN(0);
#endif
}

/*
 * Build the RPC header and fill in the authorization info. The authorization
 * string argument is only used when the credentials come from outside of the
 * kernel (AUTH_KERB). (likewise, the ucred is only used when inside the
 * kernel) Returns the head of the mbuf list.
 */
static struct mbuf    *
rpcclnt_buildheader(rc, procid, mrest, mrest_len, xidp, mheadend, cred)
	struct rpcclnt *rc;
	int             procid;
	struct mbuf    *mrest;
	u_int32_t       mrest_len;
	int            *xidp;
	struct mbuf   **mheadend;
	struct ucred * cred;
{
	/* register */ struct mbuf *mb;
	register u_int32_t *tl;
	/* register */ caddr_t bpos;
	struct mbuf *mreq, *mb2;
	int error;

	MGETHDR(mb, M_TRYWAIT, MT_DATA);
	if (6 * RPCX_UNSIGNED >= MINCLSIZE) {
		MCLGET(mb, M_TRYWAIT);
	} else if (6 * RPCX_UNSIGNED < MHLEN) {
		MH_ALIGN(mb, 6 * RPCX_UNSIGNED);
	} else {
	  	RPCDEBUG("mbuf too small");
		panic("cheap bailout");
	}
	mb->m_len = 0;
	mreq = mb;
	bpos = mtod(mb, caddr_t);

	/*
	 * First the RPC header.
	 */
	rpcm_build(tl, u_int32_t *, 6 * RPCX_UNSIGNED);

	/* Get a new (non-zero) xid */
	if ((rpcclnt_xid == 0) && (rpcclnt_xid_touched == 0)) {
		rpcclnt_xid = arc4random();
		rpcclnt_xid_touched = 1;
	} else {
		while ((*xidp = arc4random() % 256) == 0);
		rpcclnt_xid += *xidp;
	}

	/* XXX: funky... */
	*tl++ = *xidp = txdr_unsigned(rpcclnt_xid);

	*tl++ = rpc_call;
	*tl++ = rpc_vers;
	*tl++ = txdr_unsigned(rc->rc_prog->prog_id);
	*tl++ = txdr_unsigned(rc->rc_prog->prog_version);
	*tl++ = txdr_unsigned(procid);

	if ((error = rpcauth_buildheader(rc->rc_auth, cred, &mb, &bpos))) {
		m_freem(mreq);
		RPCDEBUG("rpcauth_buildheader failed %d", error);
		return NULL;
	}

	mb->m_next = mrest;
	*mheadend = mb;
	mreq->m_pkthdr.len = m_length(mreq, NULL);
	mreq->m_pkthdr.rcvif = NULL;
	return (mreq);
}

/*
 * Help break down an mbuf chain by setting the first siz bytes contiguous
 * pointed to by returned val. This is used by the macros rpcm_dissect and
 * rpcm_dissecton for tough cases. (The macros use the vars. dpos and dpos2)
 */
static int
rpcm_disct(mdp, dposp, siz, left, cp2)
	struct mbuf   **mdp;
	caddr_t        *dposp;
	int             siz;
	int             left;
	caddr_t        *cp2;
{
	struct mbuf    *mp, *mp2;
	int             siz2, xfer;
	caddr_t         p;

	mp = *mdp;
	while (left == 0) {
		*mdp = mp = mp->m_next;
		if (mp == NULL)
			RPC_RETURN(EBADRPC);
		left = mp->m_len;
		*dposp = mtod(mp, caddr_t);
	}
	if (left >= siz) {
		*cp2 = *dposp;
		*dposp += siz;
	} else if (mp->m_next == NULL) {
		RPC_RETURN(EBADRPC);
	} else if (siz > MHLEN) {
		panic("rpc S too big");
	} else {
		MGET(mp2, M_TRYWAIT, MT_DATA);
		mp2->m_next = mp->m_next;
		mp->m_next = mp2;
		mp->m_len -= left;
		mp = mp2;
		*cp2 = p = mtod(mp, caddr_t);
		bcopy(*dposp, p, left);	/* Copy what was left */
		siz2 = siz - left;
		p += left;
		mp2 = mp->m_next;
		/* Loop around copying up the siz2 bytes */
		while (siz2 > 0) {
			if (mp2 == NULL)
				RPC_RETURN(EBADRPC);
			xfer = (siz2 > mp2->m_len) ? mp2->m_len : siz2;
			if (xfer > 0) {
				bcopy(mtod(mp2, caddr_t), p, xfer);
				RPCMADV(mp2, xfer);
				mp2->m_len -= xfer;
				p += xfer;
				siz2 -= xfer;
			}
			if (siz2 > 0)
				mp2 = mp2->m_next;
		}
		mp->m_len = siz;
		*mdp = mp2;
		*dposp = mtod(mp2, caddr_t);
	}
	RPC_RETURN(0);
}



static u_int32_t
rpcclnt_proct(rpc, procid)
	struct rpcclnt *rpc;
	u_int32_t       procid;
{
	if (rpc->rc_proctlen != 0 && rpc->rc_proct != NULL &&
	    procid < rpc->rc_proctlen) {
		return rpc->rc_proct[procid];
	}
	return (0);
}

static int
rpc_adv(mdp, dposp, offs, left)
	struct mbuf   **mdp;
	caddr_t        *dposp;
	int             offs;
	int             left;
{
	struct mbuf    *m;
	int             s;

	m = *mdp;
	s = left;
	while (s < offs) {
		offs -= s;
		m = m->m_next;
		if (m == NULL)
			RPC_RETURN(EBADRPC);
		s = m->m_len;
	}
	*mdp = m;
	*dposp = mtod(m, caddr_t) + offs;
	RPC_RETURN(0);
}

int
rpcclnt_cancelreqs(rpc)
	struct rpcclnt *rpc;
{
	struct rpctask *task;
	int             i, s;

	s = splnet();
	TAILQ_FOREACH(task, &rpctask_q, r_chain) {
		if (rpc != task->r_rpcclnt || task->r_mrep != NULL ||
		    (task->r_flags & R_SOFTTERM))
			continue;
		rpcclnt_softterm(task);
	}
	splx(s);

	for (i = 0; i < 30; i++) {
		s = splnet();
		TAILQ_FOREACH(task, &rpctask_q, r_chain) {
			if (rpc == task->r_rpcclnt)
				break;
		}
		splx(s);
		if (task == NULL)
			return (0);
		tsleep(&lbolt, PSOCK, "nfscancel", 0);
	}
	return (EBUSY);
}

static void
rpcclnt_softterm(struct rpctask * task)
{
	task->r_flags |= R_SOFTTERM;
	if (task->r_flags & R_SENT) {
		task->r_rpcclnt->rc_sent -= RPC_CWNDSCALE;
		task->r_flags &= ~R_SENT;
	}
}


#ifndef __OpenBSD__
/* called by rpcclnt_get() */
void
rpcclnt_create(struct rpcclnt ** rpc)
{
	MALLOC(*rpc, struct rpcclnt *, sizeof(struct rpcclnt), M_RPC, M_WAITOK | M_ZERO);
}

/* called by rpcclnt_put() */
void
rpcclnt_destroy(struct rpcclnt * rpc)
{
	if (rpc != NULL) {
		FREE(rpc, M_RPC);
	} else {
		RPCDEBUG("attempting to free a NULL rpcclnt (not dereferenced)");
	}
}
#endif				/* !__OpenBSD__ */


/* XXX: add a lock around the auth structure in struct rpcclnt and make this
 * call safe for calling durring a connection */
static int
rpcauth_buildheader(struct rpc_auth * auth, struct ucred * cred, struct mbuf ** mhdr, caddr_t * bp)
{
	size_t authsiz, verfsiz;
	uint32_t mlen, grpsiz;
	register struct mbuf *mb, *mb2;
	caddr_t bpos;
	register u_int32_t *tl;
	register int i;

	if (auth == NULL || mhdr == NULL)
	  return EFAULT;

	switch (auth->auth_type) {
	case RPCAUTH_NULL:
	  	authsiz = 0;
		verfsiz = 0;
	break;
	case RPCAUTH_UNIX:
		authsiz = (5 + cred->cr_ngroups) * RPCX_UNSIGNED;
		verfsiz = 0;
	break;
	default:
		return EPROTONOSUPPORT;
	break;
	};

	mlen = rpcm_rndup(authsiz) + rpcm_rndup(verfsiz) + 4 * RPCX_UNSIGNED;

	mb = *mhdr;
	bpos = *bp;

	rpcm_build(tl, u_int32_t *, mlen);

	*bp = bpos;
	*mhdr = mb;

	*tl++ = txdr_unsigned(auth->auth_type);
	*tl++ = txdr_unsigned(authsiz);
	switch (auth->auth_type) {
	case RPCAUTH_UNIX:
	 	*tl++ = 0;
	 	*tl++ = 0; 

		*tl++ = txdr_unsigned(cred->cr_uid);
		*tl++ = txdr_unsigned(cred->cr_groups[0]);
		grpsiz = cred->cr_ngroups;
		*tl++ = txdr_unsigned(grpsiz);
		/* XXX: groups[0] is already sent... */
		for (i = 0 ; i < grpsiz ; i++) {
			 *tl++ = txdr_unsigned(cred->cr_groups[i]);
		}

		/* null verification header */
		*tl++ = txdr_unsigned(RPCAUTH_NULL);
		*tl++ = 0;
	break;
	case RPCAUTH_NULL:
		/* just a null verf header */
		*tl++ = txdr_unsigned(RPCAUTH_NULL);
		*tl = 0;
	break;
	default:
		panic("inconsistent rpc auth type");
	break;
	}

	return 0;
}
