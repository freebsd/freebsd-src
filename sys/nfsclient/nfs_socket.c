/*
 * Copyright (c) 1989, 1991, 1993, 1995
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)nfs_socket.c	8.5 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Socket operations for use by nfs
 */

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vnode.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsmount.h>
#include <nfsclient/nfsnode.h>

#define	TRUE	1
#define	FALSE	0

/*
 * Estimate rto for an nfs rpc sent via. an unreliable datagram.
 * Use the mean and mean deviation of rtt for the appropriate type of rpc
 * for the frequent rpcs and a default for the others.
 * The justification for doing "other" this way is that these rpcs
 * happen so infrequently that timer est. would probably be stale.
 * Also, since many of these rpcs are
 * non-idempotent, a conservative timeout is desired.
 * getattr, lookup - A+2D
 * read, write     - A+4D
 * other           - nm_timeo
 */
#define	NFS_RTO(n, t) \
	((t) == 0 ? (n)->nm_timeo : \
	 ((t) < 3 ? \
	  (((((n)->nm_srtt[t-1] + 3) >> 2) + (n)->nm_sdrtt[t-1] + 1) >> 1) : \
	  ((((n)->nm_srtt[t-1] + 7) >> 3) + (n)->nm_sdrtt[t-1] + 1)))
#define	NFS_SRTT(r)	(r)->r_nmp->nm_srtt[proct[(r)->r_procnum] - 1]
#define	NFS_SDRTT(r)	(r)->r_nmp->nm_sdrtt[proct[(r)->r_procnum] - 1]

/*
 * Defines which timer to use for the procnum.
 * 0 - default
 * 1 - getattr
 * 2 - lookup
 * 3 - read
 * 4 - write
 */
static int proct[NFS_NPROCS] = {
	0, 1, 0, 2, 1, 3, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0,
};

static int	nfs_realign_test;
static int	nfs_realign_count;
static int	nfs_bufpackets = 4;

SYSCTL_DECL(_vfs_nfs);

SYSCTL_INT(_vfs_nfs, OID_AUTO, realign_test, CTLFLAG_RW, &nfs_realign_test, 0, "");
SYSCTL_INT(_vfs_nfs, OID_AUTO, realign_count, CTLFLAG_RW, &nfs_realign_count, 0, "");
SYSCTL_INT(_vfs_nfs, OID_AUTO, bufpackets, CTLFLAG_RW, &nfs_bufpackets, 0, "");


/*
 * There is a congestion window for outstanding rpcs maintained per mount
 * point. The cwnd size is adjusted in roughly the way that:
 * Van Jacobson, Congestion avoidance and Control, In "Proceedings of
 * SIGCOMM '88". ACM, August 1988.
 * describes for TCP. The cwnd size is chopped in half on a retransmit timeout
 * and incremented by 1/cwnd when each rpc reply is received and a full cwnd
 * of rpcs is in progress.
 * (The sent count and cwnd are scaled for integer arith.)
 * Variants of "slow start" were tried and were found to be too much of a
 * performance hit (ave. rtt 3 times larger),
 * I suspect due to the large rtt that nfs rpcs have.
 */
#define	NFS_CWNDSCALE	256
#define	NFS_MAXCWND	(NFS_CWNDSCALE * 32)
#define	NFS_NBACKOFF	8
static int nfs_backoff[NFS_NBACKOFF] = { 2, 4, 8, 16, 32, 64, 128, 256, };
struct callout_handle	nfs_timer_handle;

static int	nfs_msg(struct thread *, char *, char *);
static int	nfs_rcvlock(struct nfsreq *);
static void	nfs_rcvunlock(struct nfsreq *);
static void	nfs_realign(struct mbuf **pm, int hsiz);
static int	nfs_receive(struct nfsreq *rep, struct sockaddr **aname,
		    struct mbuf **mp);
static int	nfs_reply(struct nfsreq *);
static void	nfs_softterm(struct nfsreq *rep);
static int	nfs_reconnect(struct nfsreq *rep);

/*
 * Initialize sockets and congestion for a new NFS connection.
 * We do not free the sockaddr if error.
 */
int
nfs_connect(struct nfsmount *nmp, struct nfsreq *rep)
{
	struct socket *so;
	int s, error, rcvreserve, sndreserve;
	int pktscale;
	struct sockaddr *saddr;
	struct thread *td = &thread0; /* only used for socreate and sobind */

	nmp->nm_so = NULL;
	saddr = nmp->nm_nam;
	error = socreate(saddr->sa_family, &nmp->nm_so, nmp->nm_sotype,
		nmp->nm_soproto, nmp->nm_mountp->mnt_cred, td);
	if (error)
		goto bad;
	so = nmp->nm_so;
	nmp->nm_soflags = so->so_proto->pr_flags;

	/*
	 * Some servers require that the client port be a reserved port number.
	 */
	if (nmp->nm_flag & NFSMNT_RESVPORT) {
		struct sockopt sopt;
		int ip, ip2, len;
		struct sockaddr_in6 ssin;
		struct sockaddr *sa;

		bzero(&sopt, sizeof sopt);
		switch(saddr->sa_family) {
		case AF_INET:
			sopt.sopt_level = IPPROTO_IP;
			sopt.sopt_name = IP_PORTRANGE;
			ip = IP_PORTRANGE_LOW;
			ip2 = IP_PORTRANGE_DEFAULT;
			len = sizeof (struct sockaddr_in);
			break;
#ifdef INET6
		case AF_INET6:
			sopt.sopt_level = IPPROTO_IPV6;
			sopt.sopt_name = IPV6_PORTRANGE;
			ip = IPV6_PORTRANGE_LOW;
			ip2 = IPV6_PORTRANGE_DEFAULT;
			len = sizeof (struct sockaddr_in6);
			break;
#endif
		default:
			goto noresvport;
		}
		sa = (struct sockaddr *)&ssin;
		bzero(sa, len);
		sa->sa_len = len;
		sa->sa_family = saddr->sa_family;
		sopt.sopt_dir = SOPT_SET;
		sopt.sopt_val = (void *)&ip;
		sopt.sopt_valsize = sizeof(ip);
		error = sosetopt(so, &sopt);
		if (error)
			goto bad;
		error = sobind(so, sa, td);
		if (error)
			goto bad;
		ip = ip2;
		error = sosetopt(so, &sopt);
		if (error)
			goto bad;
	noresvport: ;
	}

	/*
	 * Protocols that do not require connections may be optionally left
	 * unconnected for servers that reply from a port other than NFS_PORT.
	 */
	if (nmp->nm_flag & NFSMNT_NOCONN) {
		if (nmp->nm_soflags & PR_CONNREQUIRED) {
			error = ENOTCONN;
			goto bad;
		}
	} else {
		error = soconnect(so, nmp->nm_nam, td);
		if (error)
			goto bad;

		/*
		 * Wait for the connection to complete. Cribbed from the
		 * connect system call but with the wait timing out so
		 * that interruptible mounts don't hang here for a long time.
		 */
		s = splnet();
		while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0) {
			(void) tsleep((caddr_t)&so->so_timeo,
			    PSOCK, "nfscon", 2 * hz);
			if ((so->so_state & SS_ISCONNECTING) &&
			    so->so_error == 0 && rep &&
			    (error = nfs_sigintr(nmp, rep, rep->r_td)) != 0) {
				so->so_state &= ~SS_ISCONNECTING;
				splx(s);
				goto bad;
			}
		}
		if (so->so_error) {
			error = so->so_error;
			so->so_error = 0;
			splx(s);
			goto bad;
		}
		splx(s);
	}
	so->so_rcv.sb_timeo = 5 * hz;
	so->so_snd.sb_timeo = 5 * hz;

	/*
	 * Get buffer reservation size from sysctl, but impose reasonable
	 * limits.
	 */
	pktscale = nfs_bufpackets;
	if (pktscale < 2)
		pktscale = 2;
	if (pktscale > 64)
		pktscale = 64;

	if (nmp->nm_sotype == SOCK_DGRAM) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * pktscale;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * pktscale;
	} else if (nmp->nm_sotype == SOCK_SEQPACKET) {
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR) * pktscale;
		rcvreserve = (max(nmp->nm_rsize, nmp->nm_readdirsize) +
		    NFS_MAXPKTHDR) * pktscale;
	} else {
		if (nmp->nm_sotype != SOCK_STREAM)
			panic("nfscon sotype");
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
			struct sockopt sopt;
			int val;

			bzero(&sopt, sizeof sopt);
			sopt.sopt_level = SOL_SOCKET;
			sopt.sopt_name = SO_KEEPALIVE;
			sopt.sopt_val = &val;
			sopt.sopt_valsize = sizeof val;
			val = 1;
			sosetopt(so, &sopt);
		}
		if (so->so_proto->pr_protocol == IPPROTO_TCP) {
			struct sockopt sopt;
			int val;

			bzero(&sopt, sizeof sopt);
			sopt.sopt_level = IPPROTO_TCP;
			sopt.sopt_name = TCP_NODELAY;
			sopt.sopt_val = &val;
			sopt.sopt_valsize = sizeof val;
			val = 1;
			sosetopt(so, &sopt);
		}
		sndreserve = (nmp->nm_wsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * pktscale;
		rcvreserve = (nmp->nm_rsize + NFS_MAXPKTHDR +
		    sizeof (u_int32_t)) * pktscale;
	}
	error = soreserve(so, sndreserve, rcvreserve);
	if (error)
		goto bad;
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;

	/* Initialize other non-zero congestion variables */
	nmp->nm_srtt[0] = nmp->nm_srtt[1] = nmp->nm_srtt[2] =
		nmp->nm_srtt[3] = (NFS_TIMEO << 3);
	nmp->nm_sdrtt[0] = nmp->nm_sdrtt[1] = nmp->nm_sdrtt[2] =
		nmp->nm_sdrtt[3] = 0;
	nmp->nm_cwnd = NFS_MAXCWND / 2;	    /* Initial send window */
	nmp->nm_sent = 0;
	nmp->nm_timeouts = 0;
	return (0);

bad:
	nfs_disconnect(nmp);
	return (error);
}

/*
 * Reconnect routine:
 * Called when a connection is broken on a reliable protocol.
 * - clean up the old socket
 * - nfs_connect() again
 * - set R_MUSTRESEND for all outstanding requests on mount point
 * If this fails the mount point is DEAD!
 * nb: Must be called with the nfs_sndlock() set on the mount point.
 */
static int
nfs_reconnect(struct nfsreq *rep)
{
	struct nfsreq *rp;
	struct nfsmount *nmp = rep->r_nmp;
	int error;

	nfs_disconnect(nmp);
	while ((error = nfs_connect(nmp, rep)) != 0) {
		if (error == EINTR || error == ERESTART)
			return (EINTR);
		(void) tsleep((caddr_t)&lbolt, PSOCK, "nfscon", 0);
	}

	/*
	 * Loop through outstanding request list and fix up all requests
	 * on old socket.
	 */
	TAILQ_FOREACH(rp, &nfs_reqq, r_chain) {
		if (rp->r_nmp == nmp)
			rp->r_flags |= R_MUSTRESEND;
	}
	return (0);
}

/*
 * NFS disconnect. Clean up and unlink.
 */
void
nfs_disconnect(struct nfsmount *nmp)
{
	struct socket *so;

	if (nmp->nm_so) {
		so = nmp->nm_so;
		nmp->nm_so = NULL;
		soshutdown(so, 2);
		soclose(so);
	}
}

void
nfs_safedisconnect(struct nfsmount *nmp)
{
	struct nfsreq dummyreq;

	bzero(&dummyreq, sizeof(dummyreq));
	dummyreq.r_nmp = nmp;
	nfs_rcvlock(&dummyreq);
	nfs_disconnect(nmp);
	nfs_rcvunlock(&dummyreq);
}

/*
 * This is the nfs send routine. For connection based socket types, it
 * must be called with an nfs_sndlock() on the socket.
 * - return EINTR if the RPC is terminated, 0 otherwise
 * - set R_MUSTRESEND if the send fails for any reason
 * - do any cleanup required by recoverable socket errors (?)
 */
int
nfs_send(struct socket *so, struct sockaddr *nam, struct mbuf *top,
    struct nfsreq *rep)
{
	struct sockaddr *sendnam;
	int error, soflags, flags;

	KASSERT(rep, ("nfs_send: called with rep == NULL"));

	if (rep->r_flags & R_SOFTTERM) {
		m_freem(top);
		return (EINTR);
	}
	if ((so = rep->r_nmp->nm_so) == NULL) {
		rep->r_flags |= R_MUSTRESEND;
		m_freem(top);
		return (0);
	}
	rep->r_flags &= ~R_MUSTRESEND;
	soflags = rep->r_nmp->nm_soflags;

	if ((soflags & PR_CONNREQUIRED) || (so->so_state & SS_ISCONNECTED))
		sendnam = NULL;
	else
		sendnam = nam;
	if (so->so_type == SOCK_SEQPACKET)
		flags = MSG_EOR;
	else
		flags = 0;

	error = so->so_proto->pr_usrreqs->pru_sosend(so, sendnam, 0, top, 0,
						     flags, curthread /*XXX*/);
	if (error == ENOBUFS && so->so_type == SOCK_DGRAM) {
		error = 0;
		rep->r_flags |= R_MUSTRESEND;
	}

	if (error) {
		log(LOG_INFO, "nfs send error %d for server %s\n", error,
		    rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
		/*
		 * Deal with errors for the client side.
		 */
		if (rep->r_flags & R_SOFTTERM)
			error = EINTR;
		else
			rep->r_flags |= R_MUSTRESEND;

		/*
		 * Handle any recoverable (soft) socket errors here. (?)
		 */
		if (error != EINTR && error != ERESTART &&
			error != EWOULDBLOCK && error != EPIPE)
			error = 0;
	}
	return (error);
}

/*
 * Receive a Sun RPC Request/Reply. For SOCK_DGRAM, the work is all
 * done by soreceive(), but for SOCK_STREAM we must deal with the Record
 * Mark and consolidate the data into a new mbuf list.
 * nb: Sometimes TCP passes the data up to soreceive() in long lists of
 *     small mbufs.
 * For SOCK_STREAM we must be very careful to read an entire record once
 * we have read any of it, even if the system call has been interrupted.
 */
static int
nfs_receive(struct nfsreq *rep, struct sockaddr **aname, struct mbuf **mp)
{
	struct socket *so;
	struct uio auio;
	struct iovec aio;
	struct mbuf *m;
	struct mbuf *control;
	u_int32_t len;
	struct sockaddr **getnam;
	int error, sotype, rcvflg;
	struct thread *td = curthread;	/* XXX */

	/*
	 * Set up arguments for soreceive()
	 */
	*mp = NULL;
	*aname = NULL;
	sotype = rep->r_nmp->nm_sotype;

	/*
	 * For reliable protocols, lock against other senders/receivers
	 * in case a reconnect is necessary.
	 * For SOCK_STREAM, first get the Record Mark to find out how much
	 * more there is to get.
	 * We must lock the socket against other receivers
	 * until we have an entire rpc request/reply.
	 */
	if (sotype != SOCK_DGRAM) {
		error = nfs_sndlock(rep);
		if (error)
			return (error);
tryagain:
		/*
		 * Check for fatal errors and resending request.
		 */
		/*
		 * Ugh: If a reconnect attempt just happened, nm_so
		 * would have changed. NULL indicates a failed
		 * attempt that has essentially shut down this
		 * mount point.
		 */
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM)) {
			nfs_sndunlock(rep);
			return (EINTR);
		}
		so = rep->r_nmp->nm_so;
		if (!so) {
			error = nfs_reconnect(rep);
			if (error) {
				nfs_sndunlock(rep);
				return (error);
			}
			goto tryagain;
		}
		while (rep->r_flags & R_MUSTRESEND) {
			m = m_copym(rep->r_mreq, 0, M_COPYALL, M_TRYWAIT);
			nfsstats.rpcretries++;
			error = nfs_send(so, rep->r_nmp->nm_nam, m, rep);
			if (error) {
				if (error == EINTR || error == ERESTART ||
				    (error = nfs_reconnect(rep)) != 0) {
					nfs_sndunlock(rep);
					return (error);
				}
				goto tryagain;
			}
		}
		nfs_sndunlock(rep);
		if (sotype == SOCK_STREAM) {
			aio.iov_base = (caddr_t) &len;
			aio.iov_len = sizeof(u_int32_t);
			auio.uio_iov = &aio;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = UIO_READ;
			auio.uio_offset = 0;
			auio.uio_resid = sizeof(u_int32_t);
			auio.uio_td = td;
			do {
			   rcvflg = MSG_WAITALL;
			   error = so->so_proto->pr_usrreqs->pru_soreceive
				   (so, NULL, &auio, NULL, NULL, &rcvflg);
			   if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
			   }
			} while (error == EWOULDBLOCK);
			if (!error && auio.uio_resid > 0) {
			    /*
			     * Don't log a 0 byte receive; it means
			     * that the socket has been closed, and
			     * can happen during normal operation
			     * (forcible unmount or Solaris server).
			     */
			    if (auio.uio_resid != sizeof (u_int32_t))
			    log(LOG_INFO,
				 "short receive (%d/%d) from nfs server %s\n",
				 (int)(sizeof(u_int32_t) - auio.uio_resid),
				 (int)sizeof(u_int32_t),
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EPIPE;
			}
			if (error)
				goto errout;
			len = ntohl(len) & ~0x80000000;
			/*
			 * This is SERIOUS! We are out of sync with the sender
			 * and forcing a disconnect/reconnect is all I can do.
			 */
			if (len > NFS_MAXPACKET) {
			    log(LOG_ERR, "%s (%d) from nfs server %s\n",
				"impossible packet length",
				len,
				rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EFBIG;
			    goto errout;
			}
			auio.uio_resid = len;
			do {
			    rcvflg = MSG_WAITALL;
			    error =  so->so_proto->pr_usrreqs->pru_soreceive
				    (so, NULL,
				     &auio, mp, NULL, &rcvflg);
			} while (error == EWOULDBLOCK || error == EINTR ||
				 error == ERESTART);
			if (!error && auio.uio_resid > 0) {
			    if (len != auio.uio_resid)
			    log(LOG_INFO,
				"short receive (%d/%d) from nfs server %s\n",
				len - auio.uio_resid, len,
				rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EPIPE;
			}
		} else {
			/*
			 * NB: Since uio_resid is big, MSG_WAITALL is ignored
			 * and soreceive() will return when it has either a
			 * control msg or a data msg.
			 * We have no use for control msg., but must grab them
			 * and then throw them away so we know what is going
			 * on.
			 */
			auio.uio_resid = len = 100000000; /* Anything Big */
			auio.uio_td = td;
			do {
			    rcvflg = 0;
			    error =  so->so_proto->pr_usrreqs->pru_soreceive
				    (so, NULL,
				&auio, mp, &control, &rcvflg);
			    if (control)
				m_freem(control);
			    if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
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
			*mp = NULL;
			if (error != EPIPE)
				log(LOG_INFO,
				    "receive error %d from nfs server %s\n",
				    error,
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			error = nfs_sndlock(rep);
			if (!error) {
				error = nfs_reconnect(rep);
				if (!error)
					goto tryagain;
				else
					nfs_sndunlock(rep);
			}
		}
	} else {
		if ((so = rep->r_nmp->nm_so) == NULL)
			return (EACCES);
		if (so->so_state & SS_ISCONNECTED)
			getnam = NULL;
		else
			getnam = aname;
		auio.uio_resid = len = 1000000;
		auio.uio_td = td;
		do {
			rcvflg = 0;
			error =  so->so_proto->pr_usrreqs->pru_soreceive
				(so, getnam, &auio, mp,
				NULL, &rcvflg);
			if (error == EWOULDBLOCK &&
			    (rep->r_flags & R_SOFTTERM))
				return (EINTR);
		} while (error == EWOULDBLOCK);
		len -= auio.uio_resid;
	}
	if (error) {
		m_freem(*mp);
		*mp = NULL;
	}
	/*
	 * Search for any mbufs that are not a multiple of 4 bytes long
	 * or with m_data not longword aligned.
	 * These could cause pointer alignment problems, so copy them to
	 * well aligned mbufs.
	 */
	nfs_realign(mp, 5 * NFSX_UNSIGNED);
	return (error);
}

/*
 * Implement receipt of reply on a socket.
 * We must search through the list of received datagrams matching them
 * with outstanding requests using the xid, until ours is found.
 */
/* ARGSUSED */
static int
nfs_reply(struct nfsreq *myrep)
{
	struct nfsreq *rep;
	struct nfsmount *nmp = myrep->r_nmp;
	int32_t t1;
	struct mbuf *mrep, *md;
	struct sockaddr *nam;
	u_int32_t rxid, *tl;
	caddr_t dpos;
	int error;

	/*
	 * Loop around until we get our own reply
	 */
	for (;;) {
		/*
		 * Lock against other receivers so that I don't get stuck in
		 * sbwait() after someone else has received my reply for me.
		 * Also necessary for connection based protocols to avoid
		 * race conditions during a reconnect.
		 * If nfs_rcvlock() returns EALREADY, that means that
		 * the reply has already been recieved by another
		 * process and we can return immediately.  In this
		 * case, the lock is not taken to avoid races with
		 * other processes.
		 */
		error = nfs_rcvlock(myrep);
		if (error == EALREADY)
			return (0);
		if (error)
			return (error);
		/*
		 * Get the next Rpc reply off the socket
		 */
		error = nfs_receive(myrep, &nam, &mrep);
		nfs_rcvunlock(myrep);
		if (error) {

			/*
			 * Ignore routing errors on connectionless protocols??
			 */
			if (NFSIGNORE_SOERROR(nmp->nm_soflags, error)) {
				nmp->nm_so->so_error = 0;
				if (myrep->r_flags & R_GETONEREP)
					return (0);
				continue;
			}
			return (error);
		}
		if (nam)
			FREE(nam, M_SONAME);

		/*
		 * Get the xid and check that it is an rpc reply
		 */
		md = mrep;
		dpos = mtod(md, caddr_t);
		tl = nfsm_dissect(u_int32_t *, 2 * NFSX_UNSIGNED);
		rxid = *tl++;
		if (*tl != rpc_reply) {
			nfsstats.rpcinvalid++;
			m_freem(mrep);
nfsmout:
			if (myrep->r_flags & R_GETONEREP)
				return (0);
			continue;
		}

		/*
		 * Loop through the request list to match up the reply
		 * Iff no match, just drop the datagram
		 */
		TAILQ_FOREACH(rep, &nfs_reqq, r_chain) {
			if (rep->r_mrep == NULL && rxid == rep->r_xid) {
				/* Found it.. */
				rep->r_mrep = mrep;
				rep->r_md = md;
				rep->r_dpos = dpos;
				/*
				 * Update congestion window.
				 * Do the additive increase of
				 * one rpc/rtt.
				 */
				if (nmp->nm_cwnd <= nmp->nm_sent) {
					nmp->nm_cwnd +=
					   (NFS_CWNDSCALE * NFS_CWNDSCALE +
					   (nmp->nm_cwnd >> 1)) / nmp->nm_cwnd;
					if (nmp->nm_cwnd > NFS_MAXCWND)
						nmp->nm_cwnd = NFS_MAXCWND;
				}
				if (rep->r_flags & R_SENT) {
					rep->r_flags &= ~R_SENT;
					nmp->nm_sent -= NFS_CWNDSCALE;
				}
				/*
				 * Update rtt using a gain of 0.125 on the mean
				 * and a gain of 0.25 on the deviation.
				 */
				if (rep->r_flags & R_TIMING) {
					/*
					 * Since the timer resolution of
					 * NFS_HZ is so course, it can often
					 * result in r_rtt == 0. Since
					 * r_rtt == N means that the actual
					 * rtt is between N+dt and N+2-dt ticks,
					 * add 1.
					 */
					t1 = rep->r_rtt + 1;
					t1 -= (NFS_SRTT(rep) >> 3);
					NFS_SRTT(rep) += t1;
					if (t1 < 0)
						t1 = -t1;
					t1 -= (NFS_SDRTT(rep) >> 2);
					NFS_SDRTT(rep) += t1;
				}
				nmp->nm_timeouts = 0;
				break;
			}
		}
		/*
		 * If not matched to a request, drop it.
		 * If it's mine, get out.
		 */
		if (rep == 0) {
			nfsstats.rpcunexpected++;
			m_freem(mrep);
		} else if (rep == myrep) {
			if (rep->r_mrep == NULL)
				panic("nfsreply nil");
			return (0);
		}
		if (myrep->r_flags & R_GETONEREP)
			return (0);
	}
}

/*
 * nfs_request - goes something like this
 *	- fill in request struct
 *	- links it into list
 *	- calls nfs_send() for first transmit
 *	- calls nfs_receive() to get reply
 *	- break down rpc header and return with nfs reply pointed to
 *	  by mrep or error
 * nb: always frees up mreq mbuf list
 */
/* XXX overloaded before */
#define	NQ_TRYLATERDEL	15	/* Initial try later delay (sec) */

int
nfs_request(struct vnode *vp, struct mbuf *mrest, int procnum,
    struct thread *td, struct ucred *cred, struct mbuf **mrp,
    struct mbuf **mdp, caddr_t *dposp)
{
	struct mbuf *mrep, *m2;
	struct nfsreq *rep;
	u_int32_t *tl;
	int i;
	struct nfsmount *nmp;
	struct mbuf *m, *md, *mheadend;
	time_t waituntil;
	caddr_t dpos;
	int s, error = 0, mrest_len, auth_len, auth_type;
	int trylater_delay = NQ_TRYLATERDEL, trylater_cnt = 0;
	u_int32_t xid;

	/* Reject requests while attempting a forced unmount. */
	if (vp->v_mount->mnt_kern_flag & MNTK_UNMOUNTF) {
		m_freem(mrest);
		return (ESTALE);
	}
	nmp = VFSTONFS(vp->v_mount);
	MALLOC(rep, struct nfsreq *, sizeof(struct nfsreq), M_NFSREQ, M_WAITOK);
	rep->r_nmp = nmp;
	rep->r_vp = vp;
	rep->r_td = td;
	rep->r_procnum = procnum;
	mrest_len = m_length(mrest, NULL);

	/*
	 * Get the RPC header with authorization.
	 */
	auth_type = RPCAUTH_UNIX;
	if (cred->cr_ngroups < 1)
		panic("nfsreq nogrps");
	auth_len = ((((cred->cr_ngroups - 1) > nmp->nm_numgrps) ?
		nmp->nm_numgrps : (cred->cr_ngroups - 1)) << 2) +
		5 * NFSX_UNSIGNED;
	m = nfsm_rpchead(cred, nmp->nm_flag, procnum, auth_type, auth_len,
	     mrest, mrest_len, &mheadend, &xid);

	/*
	 * For stream protocols, insert a Sun RPC Record Mark.
	 */
	if (nmp->nm_sotype == SOCK_STREAM) {
		M_PREPEND(m, NFSX_UNSIGNED, M_TRYWAIT);
		*mtod(m, u_int32_t *) = htonl(0x80000000 |
			 (m->m_pkthdr.len - NFSX_UNSIGNED));
	}
	rep->r_mreq = m;
	rep->r_xid = xid;
tryagain:
	if (nmp->nm_flag & NFSMNT_SOFT)
		rep->r_retry = nmp->nm_retry;
	else
		rep->r_retry = NFS_MAXREXMIT + 1;	/* past clip limit */
	rep->r_rtt = rep->r_rexmit = 0;
	if (proct[procnum] > 0)
		rep->r_flags = R_TIMING;
	else
		rep->r_flags = 0;
	rep->r_mrep = NULL;

	/*
	 * Do the client side RPC.
	 */
	nfsstats.rpcrequests++;
	/*
	 * Chain request into list of outstanding requests. Be sure
	 * to put it LAST so timer finds oldest requests first.
	 */
	s = splsoftclock();
	TAILQ_INSERT_TAIL(&nfs_reqq, rep, r_chain);

	/*
	 * If backing off another request or avoiding congestion, don't
	 * send this one now but let timer do it. If not timing a request,
	 * do it now.
	 */
	if (nmp->nm_so && (nmp->nm_sotype != SOCK_DGRAM ||
		(nmp->nm_flag & NFSMNT_DUMBTIMR) ||
		nmp->nm_sent < nmp->nm_cwnd)) {
		splx(s);
		if (nmp->nm_soflags & PR_CONNREQUIRED)
			error = nfs_sndlock(rep);
		if (!error) {
			m2 = m_copym(m, 0, M_COPYALL, M_TRYWAIT);
			error = nfs_send(nmp->nm_so, nmp->nm_nam, m2, rep);
			if (nmp->nm_soflags & PR_CONNREQUIRED)
				nfs_sndunlock(rep);
		}
		if (!error && (rep->r_flags & R_MUSTRESEND) == 0) {
			nmp->nm_sent += NFS_CWNDSCALE;
			rep->r_flags |= R_SENT;
		}
	} else {
		splx(s);
		rep->r_rtt = -1;
	}

	/*
	 * Wait for the reply from our send or the timer's.
	 */
	if (!error || error == EPIPE)
		error = nfs_reply(rep);

	/*
	 * RPC done, unlink the request.
	 */
	s = splsoftclock();
	TAILQ_REMOVE(&nfs_reqq, rep, r_chain);
	splx(s);

	/*
	 * Decrement the outstanding request count.
	 */
	if (rep->r_flags & R_SENT) {
		rep->r_flags &= ~R_SENT;	/* paranoia */
		nmp->nm_sent -= NFS_CWNDSCALE;
	}

	/*
	 * If there was a successful reply and a tprintf msg.
	 * tprintf a response.
	 */
	if (!error && (rep->r_flags & R_TPRINTFMSG))
		nfs_msg(rep->r_td, nmp->nm_mountp->mnt_stat.f_mntfromname,
		    "is alive again");
	mrep = rep->r_mrep;
	md = rep->r_md;
	dpos = rep->r_dpos;
	if (error) {
		m_freem(rep->r_mreq);
		free((caddr_t)rep, M_NFSREQ);
		return (error);
	}

	/*
	 * break down the rpc header and check if ok
	 */
	tl = nfsm_dissect(u_int32_t *, 3 * NFSX_UNSIGNED);
	if (*tl++ == rpc_msgdenied) {
		if (*tl == rpc_mismatch)
			error = EOPNOTSUPP;
		else
			error = EACCES;
		m_freem(mrep);
		m_freem(rep->r_mreq);
		free((caddr_t)rep, M_NFSREQ);
		return (error);
	}

	/*
	 * Just throw away any verifyer (ie: kerberos etc).
	 */
	i = fxdr_unsigned(int, *tl++);		/* verf type */
	i = fxdr_unsigned(int32_t, *tl);	/* len */
	if (i > 0)
		nfsm_adv(nfsm_rndup(i));
	tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
	/* 0 == ok */
	if (*tl == 0) {
		tl = nfsm_dissect(u_int32_t *, NFSX_UNSIGNED);
		if (*tl != 0) {
			error = fxdr_unsigned(int, *tl);
			if ((nmp->nm_flag & NFSMNT_NFSV3) &&
				error == NFSERR_TRYLATER) {
				m_freem(mrep);
				error = 0;
				waituntil = time_second + trylater_delay;
				while (time_second < waituntil)
					(void) tsleep((caddr_t)&lbolt,
						PSOCK, "nqnfstry", 0);
				trylater_delay *= nfs_backoff[trylater_cnt];
				if (trylater_cnt < NFS_NBACKOFF - 1)
					trylater_cnt++;
				goto tryagain;
			}

			/*
			 * If the File Handle was stale, invalidate the
			 * lookup cache, just in case.
			 */
			if (error == ESTALE)
				cache_purge(vp);
			if (nmp->nm_flag & NFSMNT_NFSV3) {
				*mrp = mrep;
				*mdp = md;
				*dposp = dpos;
				error |= NFSERR_RETERR;
			} else
				m_freem(mrep);
			m_freem(rep->r_mreq);
			free((caddr_t)rep, M_NFSREQ);
			return (error);
		}

		*mrp = mrep;
		*mdp = md;
		*dposp = dpos;
		m_freem(rep->r_mreq);
		FREE((caddr_t)rep, M_NFSREQ);
		return (0);
	}
	m_freem(mrep);
	error = EPROTONOSUPPORT;
nfsmout:
	m_freem(rep->r_mreq);
	free((caddr_t)rep, M_NFSREQ);
	return (error);
}

/*
 * Nfs timer routine
 * Scan the nfsreq list and retranmit any requests that have timed out
 * To avoid retransmission attempts on STREAM sockets (in the future) make
 * sure to set the r_retry field to 0 (implies nm_retry == 0).
 */
void
nfs_timer(void *arg)
{
	struct nfsreq *rep;
	struct mbuf *m;
	struct socket *so;
	struct nfsmount *nmp;
	int timeo;
	int s, error;
	struct thread *td;

	td = &thread0; /* XXX for credentials, may break if sleep */
	s = splnet();
	TAILQ_FOREACH(rep, &nfs_reqq, r_chain) {
		nmp = rep->r_nmp;
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM))
			continue;
		if (nfs_sigintr(nmp, rep, rep->r_td)) {
			nfs_softterm(rep);
			continue;
		}
		if (rep->r_rtt >= 0) {
			rep->r_rtt++;
			if (nmp->nm_flag & NFSMNT_DUMBTIMR)
				timeo = nmp->nm_timeo;
			else
				timeo = NFS_RTO(nmp, proct[rep->r_procnum]);
			if (nmp->nm_timeouts > 0)
				timeo *= nfs_backoff[nmp->nm_timeouts - 1];
			if (rep->r_rtt <= timeo)
				continue;
			if (nmp->nm_timeouts < NFS_NBACKOFF)
				nmp->nm_timeouts++;
		}
		/*
		 * Check for server not responding
		 */
		if ((rep->r_flags & R_TPRINTFMSG) == 0 &&
		     rep->r_rexmit > nmp->nm_deadthresh) {
			nfs_msg(rep->r_td,
			    nmp->nm_mountp->mnt_stat.f_mntfromname,
			    "not responding");
			rep->r_flags |= R_TPRINTFMSG;
		}
		if (rep->r_rexmit >= rep->r_retry) {	/* too many */
			nfsstats.rpctimeouts++;
			nfs_softterm(rep);
			continue;
		}
		if (nmp->nm_sotype != SOCK_DGRAM) {
			if (++rep->r_rexmit > NFS_MAXREXMIT)
				rep->r_rexmit = NFS_MAXREXMIT;
			continue;
		}
		if ((so = nmp->nm_so) == NULL)
			continue;

		/*
		 * If there is enough space and the window allows..
		 *	Resend it
		 * Set r_rtt to -1 in case we fail to send it now.
		 */
		rep->r_rtt = -1;
		if (sbspace(&so->so_snd) >= rep->r_mreq->m_pkthdr.len &&
		   ((nmp->nm_flag & NFSMNT_DUMBTIMR) ||
		    (rep->r_flags & R_SENT) ||
		    nmp->nm_sent < nmp->nm_cwnd) &&
		   (m = m_copym(rep->r_mreq, 0, M_COPYALL, M_DONTWAIT))){
			if ((nmp->nm_flag & NFSMNT_NOCONN) == 0)
			    error = (*so->so_proto->pr_usrreqs->pru_send)
				    (so, 0, m, NULL, NULL, td);
			else
			    error = (*so->so_proto->pr_usrreqs->pru_send)
				    (so, 0, m, nmp->nm_nam, NULL, td);
			if (error) {
				if (NFSIGNORE_SOERROR(nmp->nm_soflags, error))
					so->so_error = 0;
			} else {
				/*
				 * Iff first send, start timing
				 * else turn timing off, backoff timer
				 * and divide congestion window by 2.
				 */
				if (rep->r_flags & R_SENT) {
					rep->r_flags &= ~R_TIMING;
					if (++rep->r_rexmit > NFS_MAXREXMIT)
						rep->r_rexmit = NFS_MAXREXMIT;
					nmp->nm_cwnd >>= 1;
					if (nmp->nm_cwnd < NFS_CWNDSCALE)
						nmp->nm_cwnd = NFS_CWNDSCALE;
					nfsstats.rpcretries++;
				} else {
					rep->r_flags |= R_SENT;
					nmp->nm_sent += NFS_CWNDSCALE;
				}
				rep->r_rtt = 0;
			}
		}
	}
	splx(s);
	nfs_timer_handle = timeout(nfs_timer, NULL, nfs_ticks);
}

/*
 * Mark all of an nfs mount's outstanding requests with R_SOFTTERM and
 * wait for all requests to complete. This is used by forced unmounts
 * to terminate any outstanding RPCs.
 */
int
nfs_nmcancelreqs(nmp)
	struct nfsmount *nmp;
{
	struct nfsreq *req;
	int i, s;

	s = splnet();
	TAILQ_FOREACH(req, &nfs_reqq, r_chain) {
		if (nmp != req->r_nmp || req->r_mrep != NULL ||
		    (req->r_flags & R_SOFTTERM))
			continue;
		nfs_softterm(req);
	}
	splx(s);

	for (i = 0; i < 30; i++) {
		s = splnet();
		TAILQ_FOREACH(req, &nfs_reqq, r_chain) {
			if (nmp == req->r_nmp)
				break;
		}
		splx(s);
		if (req == NULL)
			return (0);
		tsleep(&lbolt, PSOCK, "nfscancel", 0);
	}
	return (EBUSY);
}

/*
 * Flag a request as being about to terminate (due to NFSMNT_INT/NFSMNT_SOFT).
 * The nm_send count is decremented now to avoid deadlocks when the process in
 * soreceive() hasn't yet managed to send its own request.
 */

static void
nfs_softterm(struct nfsreq *rep)
{

	rep->r_flags |= R_SOFTTERM;
	if (rep->r_flags & R_SENT) {
		rep->r_nmp->nm_sent -= NFS_CWNDSCALE;
		rep->r_flags &= ~R_SENT;
	}
}

/*
 * Test for a termination condition pending on the process.
 * This is used for NFSMNT_INT mounts.
 */
int
nfs_sigintr(struct nfsmount *nmp, struct nfsreq *rep, struct thread *td)
{
	struct proc *p;
	sigset_t tmpset;

	if (rep && (rep->r_flags & R_SOFTTERM))
		return (EINTR);
	/* Terminate all requests while attempting a forced unmount. */
	if (nmp->nm_mountp->mnt_kern_flag & MNTK_UNMOUNTF)
		return (EINTR);
	if (!(nmp->nm_flag & NFSMNT_INT))
		return (0);
	if (td == NULL)
		return (0);

	p = td->td_proc;
	PROC_LOCK(p);
	tmpset = p->p_siglist;
	SIGSETNAND(tmpset, p->p_sigmask);
	SIGSETNAND(tmpset, p->p_sigignore);
	if (SIGNOTEMPTY(p->p_siglist) && NFSINT_SIGMASK(tmpset)) {
		PROC_UNLOCK(p);
		return (EINTR);
	}
	PROC_UNLOCK(p);

	return (0);
}

/*
 * Lock a socket against others.
 * Necessary for STREAM sockets to ensure you get an entire rpc request/reply
 * and also to avoid race conditions between the processes with nfs requests
 * in progress when a reconnect is necessary.
 */
int
nfs_sndlock(struct nfsreq *rep)
{
	int *statep = &rep->r_nmp->nm_state;
	struct thread *td;
	int slpflag = 0, slptimeo = 0;

	if (rep) {
		td = rep->r_td;
		if (rep->r_nmp->nm_flag & NFSMNT_INT)
			slpflag = PCATCH;
	} else
		td = NULL;
	while (*statep & NFSSTA_SNDLOCK) {
		if (nfs_sigintr(rep->r_nmp, rep, td))
			return (EINTR);
		*statep |= NFSSTA_WANTSND;
		(void) tsleep((caddr_t)statep, slpflag | (PZERO - 1),
			"nfsndlck", slptimeo);
		if (slpflag == PCATCH) {
			slpflag = 0;
			slptimeo = 2 * hz;
		}
	}
	*statep |= NFSSTA_SNDLOCK;
	return (0);
}

/*
 * Unlock the stream socket for others.
 */
void
nfs_sndunlock(struct nfsreq *rep)
{
	int *statep = &rep->r_nmp->nm_state;

	if ((*statep & NFSSTA_SNDLOCK) == 0)
		panic("nfs sndunlock");
	*statep &= ~NFSSTA_SNDLOCK;
	if (*statep & NFSSTA_WANTSND) {
		*statep &= ~NFSSTA_WANTSND;
		wakeup((caddr_t)statep);
	}
}

static int
nfs_rcvlock(struct nfsreq *rep)
{
	int *statep = &rep->r_nmp->nm_state;
	int slpflag, slptimeo = 0;

	if (rep->r_nmp->nm_flag & NFSMNT_INT)
		slpflag = PCATCH;
	else
		slpflag = 0;
	while (*statep & NFSSTA_RCVLOCK) {
		if (nfs_sigintr(rep->r_nmp, rep, rep->r_td))
			return (EINTR);
		*statep |= NFSSTA_WANTRCV;
		(void) tsleep((caddr_t)statep, slpflag | (PZERO - 1), "nfsrcvlk",
			slptimeo);
		/*
		 * If our reply was recieved while we were sleeping,
		 * then just return without taking the lock to avoid a
		 * situation where a single iod could 'capture' the
		 * recieve lock.
		 */
		if (rep->r_mrep != NULL)
			return (EALREADY);
		if (slpflag == PCATCH) {
			slpflag = 0;
			slptimeo = 2 * hz;
		}
	}
	/* Always fail if our request has been cancelled. */
	if (rep != NULL && (rep->r_flags & R_SOFTTERM))
		return (EINTR);
	*statep |= NFSSTA_RCVLOCK;
	return (0);
}

/*
 * Unlock the stream socket for others.
 */
static void
nfs_rcvunlock(struct nfsreq *rep)
{
	int *statep = &rep->r_nmp->nm_state;

	if ((*statep & NFSSTA_RCVLOCK) == 0)
		panic("nfs rcvunlock");
	*statep &= ~NFSSTA_RCVLOCK;
	if (*statep & NFSSTA_WANTRCV) {
		*statep &= ~NFSSTA_WANTRCV;
		wakeup((caddr_t)statep);
	}
}

/*
 *	nfs_realign:
 *
 *	Check for badly aligned mbuf data and realign by copying the unaligned
 *	portion of the data into a new mbuf chain and freeing the portions
 *	of the old chain that were replaced.
 *
 *	We cannot simply realign the data within the existing mbuf chain
 *	because the underlying buffers may contain other rpc commands and
 *	we cannot afford to overwrite them.
 *
 *	We would prefer to avoid this situation entirely.  The situation does
 *	not occur with NFS/UDP and is supposed to only occassionally occur
 *	with TCP.  Use vfs.nfs.realign_count and realign_test to check this.
 */
static void
nfs_realign(struct mbuf **pm, int hsiz)
{
	struct mbuf *m;
	struct mbuf *n = NULL;
	int off = 0;

	++nfs_realign_test;
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
		++nfs_realign_count;
		while (m) {
			m_copyback(n, off, m->m_len, mtod(m, caddr_t));
			off += m->m_len;
			m = m->m_next;
		}
		m_freem(*pm);
		*pm = n;
	}
}


static int
nfs_msg(struct thread *td, char *server, char *msg)
{

	tprintf(td ? td->td_proc : NULL, LOG_INFO,
	    "nfs server %s: %s\n", server, msg);
	return (0);
}
