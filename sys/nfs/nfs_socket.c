/*
 * Copyright (c) 1989, 1991 The Regents of the University of California.
 * All rights reserved.
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
 *	From:	@(#)nfs_socket.c	7.23 (Berkeley) 4/20/91
 *	$Id: nfs_socket.c,v 1.3 1993/09/09 22:06:05 rgrimes Exp $
 */

/*
 * Socket operations for use by nfs
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "mount.h"
#include "kernel.h"
#include "malloc.h"
#include "mbuf.h"
#include "namei.h"
#include "vnode.h"
#include "domain.h"
#include "protosw.h"
#include "socket.h"
#include "socketvar.h"
#include "syslog.h"
#include "tprintf.h"
#include "../netinet/in.h"
#include "../netinet/tcp.h"

#include "rpcv2.h"
#include "nfsv2.h"
#include "nfs.h"
#include "xdr_subs.h"
#include "nfsm_subs.h"
#include "nfsmount.h"

#define	TRUE	1
#define	FALSE	0

/*
 * External data, mostly RPC constants in XDR form
 */
extern u_long rpc_reply, rpc_msgdenied, rpc_mismatch, rpc_vers, rpc_auth_unix,
	rpc_msgaccepted, rpc_call;
extern u_long nfs_prog, nfs_vers;
/* Maybe these should be bits in a u_long ?? */
extern int nonidempotent[NFS_NPROCS];
static int compressrequest[NFS_NPROCS] = {
	FALSE,
	TRUE,
	TRUE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	FALSE,
	FALSE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
	TRUE,
};
int	nfs_sbwait();
void	nfs_disconnect();
struct mbuf *nfs_compress(), *nfs_uncompress();

int	nfsrv_null(),
	nfsrv_getattr(),
	nfsrv_setattr(),
	nfsrv_lookup(),
	nfsrv_readlink(),
	nfsrv_read(),
	nfsrv_write(),
	nfsrv_create(),
	nfsrv_remove(),
	nfsrv_rename(),
	nfsrv_link(),
	nfsrv_symlink(),
	nfsrv_mkdir(),
	nfsrv_rmdir(),
	nfsrv_readdir(),
	nfsrv_statfs(),
	nfsrv_noop();

int (*nfsrv_procs[NFS_NPROCS])() = {
	nfsrv_null,
	nfsrv_getattr,
	nfsrv_setattr,
	nfsrv_noop,
	nfsrv_lookup,
	nfsrv_readlink,
	nfsrv_read,
	nfsrv_noop,
	nfsrv_write,
	nfsrv_create,
	nfsrv_remove,
	nfsrv_rename,
	nfsrv_link,
	nfsrv_symlink,
	nfsrv_mkdir,
	nfsrv_rmdir,
	nfsrv_readdir,
	nfsrv_statfs,
};

struct nfsreq nfsreqh;
int nfsrexmtthresh = NFS_FISHY;
int nfs_tcpnodelay = 1;

/*
 * Initialize sockets and congestion for a new NFS connection.
 * We do not free the sockaddr if error.
 */
int
nfs_connect(nmp)
	register struct nfsmount *nmp;
{
	register struct socket *so;
	struct sockaddr *saddr;					/* 08 Sep 92*/
	int s, error, bufsize;
	struct mbuf *m;
	struct sockaddr_in *sin;				/* 08 Sep 92*/
	u_short tport;						/* 08 Sep 92*/

	nmp->nm_so = (struct socket *)0;
	saddr = mtod(nmp->nm_nam, struct sockaddr *);		/* 08 Sep 92*/
	if (error = socreate(saddr->sa_family,			/* 08 Sep 92*/
		&nmp->nm_so, nmp->nm_sotype, nmp->nm_soproto))
		goto bad;
	so = nmp->nm_so;
	nmp->nm_soflags = so->so_proto->pr_flags;

	/*
	 * 08 Sep 92
	 *
	 * Some servers require that the client port be a reserved port number.
	 */
	if (saddr->sa_family == AF_INET) {
		MGET(m, M_WAIT, MT_SONAME);
		sin = mtod(m, struct sockaddr_in *);
		sin->sin_len = m->m_len = sizeof (struct sockaddr_in);
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = INADDR_ANY;
		tport = IPPORT_RESERVED - 1;
		sin->sin_port = htons(tport);
		while (sobind(so, m) == EADDRINUSE &&
		       --tport > IPPORT_RESERVED / 2)
			sin->sin_port = htons(tport);
		m_freem(m);
	}

	if (nmp->nm_sotype == SOCK_DGRAM)
		bufsize = min(4 * (nmp->nm_wsize + NFS_MAXPKTHDR),
		    NFS_MAXPACKET);
	else
		bufsize = min(4 * (nmp->nm_wsize + NFS_MAXPKTHDR + sizeof(u_long)),
		    NFS_MAXPACKET + sizeof(u_long));
	if (error = soreserve(so, bufsize, bufsize))
		goto bad;

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
		if (error = soconnect(so, nmp->nm_nam))
			goto bad;

		/*
		 * Wait for the connection to complete. Cribbed from the
		 * connect system call but with the wait at negative prio.
		 */
		s = splnet();
		while ((so->so_state & SS_ISCONNECTING) && so->so_error == 0)
			(void) tsleep((caddr_t)&so->so_timeo, PSOCK, "nfscon", 0);
		splx(s);
		if (so->so_error) {
			error = so->so_error;
			goto bad;
		}
	}
	if (nmp->nm_sotype == SOCK_DGRAM) {
		if (nmp->nm_flag & (NFSMNT_SOFT | NFSMNT_SPONGY | NFSMNT_INT)) {
			so->so_rcv.sb_timeo = (5 * hz);
			so->so_snd.sb_timeo = (5 * hz);
		} else {
			so->so_rcv.sb_timeo = 0;
			so->so_snd.sb_timeo = 0;
		}
		nmp->nm_rto = NFS_TIMEO;
	} else {
		if (nmp->nm_flag & (NFSMNT_SOFT | NFSMNT_SPONGY | NFSMNT_INT)) {
			so->so_rcv.sb_timeo = (5 * hz);
			so->so_snd.sb_timeo = (5 * hz);
		} else {
			so->so_rcv.sb_timeo = 0;
			so->so_snd.sb_timeo = 0;
		}
		if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
			MGET(m, M_WAIT, MT_SOOPTS);
			*mtod(m, int *) = 1;
			m->m_len = sizeof(int);
			sosetopt(so, SOL_SOCKET, SO_KEEPALIVE, m);
		}
		if (so->so_proto->pr_domain->dom_family == AF_INET &&
		    so->so_proto->pr_protocol == IPPROTO_TCP &&
		    nfs_tcpnodelay) {
			MGET(m, M_WAIT, MT_SOOPTS);
			*mtod(m, int *) = 1;
			m->m_len = sizeof(int);
			sosetopt(so, IPPROTO_TCP, TCP_NODELAY, m);
		}
		nmp->nm_rto = 10 * NFS_TIMEO;		/* XXX */
	}
	so->so_rcv.sb_flags |= SB_NOINTR;
	so->so_snd.sb_flags |= SB_NOINTR;

	/* Initialize other non-zero congestion variables */
	nmp->nm_window = 2;			/* Initial send window */
	nmp->nm_ssthresh = NFS_MAXWINDOW;	/* Slowstart threshold */
	nmp->nm_rttvar = nmp->nm_rto << 1;
	nmp->nm_sent = 0;
	nmp->nm_currexmit = 0;
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
 * nb: Must be called with the nfs_solock() set on the mount point.
 */
int
nfs_reconnect(rep, nmp)
	register struct nfsreq *rep;
	register struct nfsmount *nmp;
{
	register struct nfsreq *rp;
	int error;

	nfs_msg(rep->r_procp, nmp->nm_mountp->mnt_stat.f_mntfromname,
	    "trying reconnect");
	while (error = nfs_connect(nmp)) {
#ifdef lint
		error = error;
#endif /* lint */
		if ((nmp->nm_flag & NFSMNT_INT) && nfs_sigintr(rep->r_procp))
			return (EINTR);
		(void) tsleep((caddr_t)&lbolt, PSOCK, "nfscon", 0);
	}
	nfs_msg(rep->r_procp, nmp->nm_mountp->mnt_stat.f_mntfromname,
	    "reconnected");

	/*
	 * Loop through outstanding request list and fix up all requests
	 * on old socket.
	 */
	rp = nfsreqh.r_next;
	while (rp != &nfsreqh) {
		if (rp->r_nmp == nmp)
			rp->r_flags |= R_MUSTRESEND;
		rp = rp->r_next;
	}
	return (0);
}

/*
 * NFS disconnect. Clean up and unlink.
 */
void
nfs_disconnect(nmp)
	register struct nfsmount *nmp;
{
	register struct socket *so;

	if (nmp->nm_so) {
		so = nmp->nm_so;
		nmp->nm_so = (struct socket *)0;
		soshutdown(so, 2);
		soclose(so);
	}
}

/*
 * This is the nfs send routine. For connection based socket types, it
 * must be called with an nfs_solock() on the socket.
 * "rep == NULL" indicates that it has been called from a server.
 */
int
nfs_send(so, nam, top, rep)
	register struct socket *so;
	struct mbuf *nam;
	register struct mbuf *top;
	struct nfsreq *rep;
{
	struct mbuf *sendnam;
	int error, soflags;

	if (rep) {
		if (rep->r_flags & R_SOFTTERM) {
			m_freem(top);
			return (EINTR);
		}
		if (rep->r_nmp->nm_so == NULL &&
		    (error = nfs_reconnect(rep, rep->r_nmp)))
			return (error);
		rep->r_flags &= ~R_MUSTRESEND;
		so = rep->r_nmp->nm_so;
		soflags = rep->r_nmp->nm_soflags;
	} else
		soflags = so->so_proto->pr_flags;
	if ((soflags & PR_CONNREQUIRED) || (so->so_state & SS_ISCONNECTED))
		sendnam = (struct mbuf *)0;
	else
		sendnam = nam;

	error = sosend(so, sendnam, (struct uio *)0, top,
		(struct mbuf *)0, 0);
	if (error == EWOULDBLOCK && rep) {
		if (rep->r_flags & R_SOFTTERM)
			error = EINTR;
		else {
			rep->r_flags |= R_MUSTRESEND;
			error = 0;
		}
	}
	/*
	 * Ignore socket errors??
	 */
	if (error && error != EINTR && error != ERESTART)
		error = 0;
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
int
nfs_receive(so, aname, mp, rep)
	register struct socket *so;
	struct mbuf **aname;
	struct mbuf **mp;
	register struct nfsreq *rep;
{
	struct uio auio;
	struct iovec aio;
	register struct mbuf *m;
	struct mbuf *m2 = 0, *mnew, **mbp;
	caddr_t fcp, tcp = 0;
	u_long len;
	struct mbuf **getnam;
	int error, siz, mlen, soflags, rcvflg;

	/*
	 * Set up arguments for soreceive()
	 */
	*mp = (struct mbuf *)0;
	*aname = (struct mbuf *)0;
	if (rep)
		soflags = rep->r_nmp->nm_soflags;
	else
		soflags = so->so_proto->pr_flags;

	/*
	 * For reliable protocols, lock against other senders/receivers
	 * in case a reconnect is necessary.
	 * For SOCK_STREAM, first get the Record Mark to find out how much
	 * more there is to get.
	 * We must lock the socket against other receivers
	 * until we have an entire rpc request/reply.
	 */
	if (soflags & PR_CONNREQUIRED) {
tryagain:
		/*
		 * Check for fatal errors and resending request.
		 */
		if (rep) {
			/*
			 * Ugh: If a reconnect attempt just happened, nm_so
			 * would have changed. NULL indicates a failed
			 * attempt that has essentially shut down this
			 * mount point.
			 */
			if (rep->r_mrep || (so = rep->r_nmp->nm_so) == NULL ||
				(rep->r_flags & R_SOFTTERM))
				return (EINTR);
			while (rep->r_flags & R_MUSTRESEND) {
				m = m_copym(rep->r_mreq, 0, M_COPYALL, M_WAIT);
				nfsstats.rpcretries++;
				if (error = nfs_send(so, rep->r_nmp->nm_nam, m,
					rep))
					goto errout;
			}
		}
		if ((soflags & PR_ATOMIC) == 0) {
			aio.iov_base = (caddr_t) &len;
			aio.iov_len = sizeof(u_long);
			auio.uio_iov = &aio;
			auio.uio_iovcnt = 1;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = UIO_READ;
			auio.uio_procp = (struct proc *)0;
			auio.uio_offset = 0;
			auio.uio_resid = sizeof(u_long);
			do {
			    rcvflg = MSG_WAITALL;
			    error = soreceive(so, (struct mbuf **)0, &auio,
				(struct mbuf **)0, (struct mbuf **)0, &rcvflg);
			    if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
				if (rep->r_flags & R_MUSTRESEND)
					goto tryagain;
			    }
			} while (error == EWOULDBLOCK);
			if (!error && auio.uio_resid > 0) {
			    if (rep)
				log(LOG_INFO,
				   "short receive (%d/%d) from nfs server %s\n",
				   sizeof(u_long) - auio.uio_resid,
				   sizeof(u_long),
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
			    if (rep)
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
			    error =  soreceive(so, (struct mbuf **)0,
				&auio, mp, (struct mbuf **)0, &rcvflg);
			} while (error == EWOULDBLOCK || error == EINTR ||
				 error == ERESTART);
			if (!error && auio.uio_resid > 0) {
			    if (rep)
				log(LOG_INFO,
				   "short receive (%d/%d) from nfs server %s\n",
				   len - auio.uio_resid, len,
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			    error = EPIPE;
			}
		} else {
			auio.uio_resid = len = 1000000;	/* Anything Big */
			do {
			    rcvflg = 0;
			    error =  soreceive(so, (struct mbuf **)0,
				&auio, mp, (struct mbuf **)0, &rcvflg);
			    if (error == EWOULDBLOCK && rep) {
				if (rep->r_flags & R_SOFTTERM)
					return (EINTR);
				if (rep->r_flags & R_MUSTRESEND)
					goto tryagain;
			    }
			} while (error == EWOULDBLOCK);
			if (!error && *mp == NULL)
				error = EPIPE;
			len -= auio.uio_resid;
		}
errout:
		if (error && rep && error != EINTR && error != ERESTART) {
			m_freem(*mp);
			*mp = (struct mbuf *)0;
			if (error != EPIPE && rep)
				log(LOG_INFO,
				    "receive error %d from nfs server %s\n",
				    error,
				 rep->r_nmp->nm_mountp->mnt_stat.f_mntfromname);
			nfs_disconnect(rep->r_nmp);
			error = nfs_reconnect(rep, rep->r_nmp);
			if (!error)
				goto tryagain;
		}
	} else {
		if (so->so_state & SS_ISCONNECTED)
			getnam = (struct mbuf **)0;
		else
			getnam = aname;
		auio.uio_resid = len = 1000000;
		do {
			rcvflg = 0;
			error =  soreceive(so, getnam, &auio, mp,
				(struct mbuf **)0, &rcvflg);
			if (error == EWOULDBLOCK && rep &&
			    (rep->r_flags & R_SOFTTERM))
				return (EINTR);
		} while (error == EWOULDBLOCK);
		len -= auio.uio_resid;
	}
	if (error) {
		m_freem(*mp);
		*mp = (struct mbuf *)0;
	}
	/*
	 * Search for any mbufs that are not a multiple of 4 bytes long.
	 * These could cause pointer alignment problems, so copy them to
	 * well aligned mbufs.
	 */
	m = *mp;
	mbp = mp;
	while (m) {
		/*
		 * All this for something that may never happen.
		 */
		if (m->m_next && (m->m_len & 0x3)) {
			printf("nfs_rcv odd length!\n");
			mlen = 0;
			while (m) {
				fcp = mtod(m, caddr_t);
				while (m->m_len > 0) {
					if (mlen == 0) {
						MGET(m2, M_WAIT, MT_DATA);
						if (len >= MINCLSIZE)
							MCLGET(m2, M_WAIT);
						m2->m_len = 0;
						mlen = M_TRAILINGSPACE(m2);
						tcp = mtod(m2, caddr_t);
						*mbp = m2;
						mbp = &m2->m_next;
					}
					siz = MIN(mlen, m->m_len);
					bcopy(fcp, tcp, siz);
					m2->m_len += siz;
					mlen -= siz;
					len -= siz;
					tcp += siz;
					m->m_len -= siz;
					fcp += siz;
				}
				MFREE(m, mnew);
				m = mnew;
			}
			break;
		}
		len -= m->m_len;
		mbp = &m->m_next;
		m = m->m_next;
	}
	return (error);
}

/*
 * Implement receipt of reply on a socket.
 * We must search through the list of received datagrams matching them
 * with outstanding requests using the xid, until ours is found.
 */
/* ARGSUSED */
int
nfs_reply(nmp, myrep)
	struct nfsmount *nmp;
	struct nfsreq *myrep;
{
	register struct mbuf *m;
	register struct nfsreq *rep;
	register int error = 0;
	u_long rxid;
	struct mbuf *mp, *nam;
/*	char *cp; */
/*	int cnt, xfer; */

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
		nfs_solock(&nmp->nm_flag);
		/* Already received, bye bye */
		if (myrep->r_mrep != NULL) {
			nfs_sounlock(&nmp->nm_flag);
			return (0);
		}
		/*
		 * Get the next Rpc reply off the socket
		 */
		if (error = nfs_receive(nmp->nm_so, &nam, &mp, myrep)) {
			nfs_sounlock(&nmp->nm_flag);

			/*
			 * Ignore routing errors on connectionless protocols??
			 */
			if (NFSIGNORE_SOERROR(nmp->nm_soflags, error)) {
				nmp->nm_so->so_error = 0;
				continue;
			}

			/*
			 * Otherwise cleanup and return a fatal error.
			 */
			if (myrep->r_flags & R_TIMING) {
				myrep->r_flags &= ~R_TIMING;
				nmp->nm_rtt = -1;
			}
			if (myrep->r_flags & R_SENT) {
				myrep->r_flags &= ~R_SENT;
				nmp->nm_sent--;
			}
			return (error);
		}
	
		/*
		 * Get the xid and check that it is an rpc reply
		 */
		m = mp;
		while (m && m->m_len == 0)
			m = m->m_next;
		if (m == NULL) {
			nfsstats.rpcinvalid++;
			m_freem(mp);
			nfs_sounlock(&nmp->nm_flag);
			continue;
		}
		bcopy(mtod(m, caddr_t), (caddr_t)&rxid, NFSX_UNSIGNED);
		/*
		 * Loop through the request list to match up the reply
		 * Iff no match, just drop the datagram
		 */
		m = mp;
		rep = nfsreqh.r_next;
		while (rep != &nfsreqh) {
			if (rep->r_mrep == NULL && rxid == rep->r_xid) {
				/* Found it.. */
				rep->r_mrep = m;
				/*
				 * Update timing
				 */
				if (rep->r_flags & R_TIMING) {
					nfs_updatetimer(rep->r_nmp);
					rep->r_flags &= ~R_TIMING;
					rep->r_nmp->nm_rtt = -1;
				}
				if (rep->r_flags & R_SENT) {
					rep->r_flags &= ~R_SENT;
					rep->r_nmp->nm_sent--;
				}
				break;
			}
			rep = rep->r_next;
		}
		nfs_sounlock(&nmp->nm_flag);
		if (nam)
			m_freem(nam);
		/*
		 * If not matched to a request, drop it.
		 * If it's mine, get out.
		 */
		if (rep == &nfsreqh) {
			nfsstats.rpcunexpected++;
			m_freem(m);
		} else if (rep == myrep)
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
int
nfs_request(vp, mreq, xid, procnum, procp, tryhard, mp, mrp, mdp, dposp)
	struct vnode *vp;
	struct mbuf *mreq;
	u_long xid;
	int procnum;
	struct proc *procp;
	int tryhard;
	struct mount *mp;
	struct mbuf **mrp;
	struct mbuf **mdp;
	caddr_t *dposp;
{
	register struct mbuf *m, *mrep;
	register struct nfsreq *rep;
	register u_long *tl;
	register int len;
	struct nfsmount *nmp;
	struct mbuf *md;
	struct nfsreq *reph;
	caddr_t dpos;
	char *cp2;
	int t1;
	int s, compressed;
	int error = 0;

	nmp = VFSTONFS(mp);
	m = mreq;
	MALLOC(rep, struct nfsreq *, sizeof(struct nfsreq), M_NFSREQ, M_WAITOK);
	rep->r_xid = xid;
	rep->r_nmp = nmp;
	rep->r_vp = vp;
	rep->r_procp = procp;
	if ((nmp->nm_flag & NFSMNT_SOFT) ||
	    ((nmp->nm_flag & NFSMNT_SPONGY) && !tryhard))
		rep->r_retry = nmp->nm_retry;
	else
		rep->r_retry = NFS_MAXREXMIT + 1;	/* past clip limit */
	rep->r_flags = rep->r_rexmit = 0;
	/*
	 * Three cases:
	 * - non-idempotent requests on SOCK_DGRAM use NFS_MINIDEMTIMEO
	 * - idempotent requests on SOCK_DGRAM use 0
	 * - Reliable transports, NFS_RELIABLETIMEO
	 *   Timeouts are still done on reliable transports to ensure detection
	 *   of excessive connection delay.
	 */
	if (nmp->nm_sotype != SOCK_DGRAM)
		rep->r_timerinit = -NFS_RELIABLETIMEO;
	else if (nonidempotent[procnum])
		rep->r_timerinit = -NFS_MINIDEMTIMEO;
	else
		rep->r_timerinit = 0;
	rep->r_timer = rep->r_timerinit;
	rep->r_mrep = NULL;
	len = 0;
	while (m) {
		len += m->m_len;
		m = m->m_next;
	}
	mreq->m_pkthdr.len = len;
	mreq->m_pkthdr.rcvif = (struct ifnet *)0;
	compressed = 0;
	m = mreq;
	if ((nmp->nm_flag & NFSMNT_COMPRESS) && compressrequest[procnum]) {
		mreq = nfs_compress(mreq);
		if (mreq != m) {
			len = mreq->m_pkthdr.len;
			compressed++;
		}
	}
	/*
	 * For non-atomic protocols, insert a Sun RPC Record Mark.
	 */
	if ((nmp->nm_soflags & PR_ATOMIC) == 0) {
		M_PREPEND(mreq, sizeof(u_long), M_WAIT);
		*mtod(mreq, u_long *) = htonl(0x80000000 | len);
	}
	rep->r_mreq = mreq;

	/*
	 * Do the client side RPC.
	 */
	nfsstats.rpcrequests++;
	/*
	 * Chain request into list of outstanding requests. Be sure
	 * to put it LAST so timer finds oldest requests first.
	 */
	s = splnet();
	reph = &nfsreqh;
	reph->r_prev->r_next = rep;
	rep->r_prev = reph->r_prev;
	reph->r_prev = rep;
	rep->r_next = reph;
	/*
	 * If backing off another request or avoiding congestion, don't
	 * send this one now but let timer do it. If not timing a request,
	 * do it now.
	 */
	if (nmp->nm_sent <= 0 || nmp->nm_sotype != SOCK_DGRAM ||
	    (nmp->nm_currexmit == 0 && nmp->nm_sent < nmp->nm_window)) {
		nmp->nm_sent++;
		rep->r_flags |= R_SENT;
		if (nmp->nm_rtt == -1) {
			nmp->nm_rtt = 0;
			rep->r_flags |= R_TIMING;
		}
		splx(s);
		m = m_copym(mreq, 0, M_COPYALL, M_WAIT);
		if (nmp->nm_soflags & PR_CONNREQUIRED)
			nfs_solock(&nmp->nm_flag);
		error = nfs_send(nmp->nm_so, nmp->nm_nam, m, rep);
		if (nmp->nm_soflags & PR_CONNREQUIRED)
			nfs_sounlock(&nmp->nm_flag);
		if (error && NFSIGNORE_SOERROR(nmp->nm_soflags, error))
			nmp->nm_so->so_error = error = 0;
	} else
		splx(s);

	/*
	 * Wait for the reply from our send or the timer's.
	 */
	if (!error)
		error = nfs_reply(nmp, rep);

	/*
	 * RPC done, unlink the request.
	 */
	s = splnet();
	rep->r_prev->r_next = rep->r_next;
	rep->r_next->r_prev = rep->r_prev;
	splx(s);

	/*
	 * If there was a successful reply and a tprintf msg.
	 * tprintf a response.
	 */
	if (!error && (rep->r_flags & R_TPRINTFMSG))
		nfs_msg(rep->r_procp, nmp->nm_mountp->mnt_stat.f_mntfromname,
		    "is alive again");
	m_freem(rep->r_mreq);
	mrep = rep->r_mrep;
	FREE((caddr_t)rep, M_NFSREQ);
	if (error)
		return (error);

	if (compressed)
		mrep = nfs_uncompress(mrep);
	md = mrep;
	/*
	 * break down the rpc header and check if ok
	 */
	dpos = mtod(md, caddr_t);
	nfsm_disect(tl, u_long *, 5*NFSX_UNSIGNED);
	tl += 2;
	if (*tl++ == rpc_msgdenied) {
		if (*tl == rpc_mismatch)
			error = EOPNOTSUPP;
		else
			error = EACCES;
		m_freem(mrep);
		return (error);
	}
	/*
	 * skip over the auth_verf, someday we may want to cache auth_short's
	 * for nfs_reqhead(), but for now just dump it
	 */
	if (*++tl != 0) {
		len = nfsm_rndup(fxdr_unsigned(long, *tl));
		nfsm_adv(len);
	}
	nfsm_disect(tl, u_long *, NFSX_UNSIGNED);
	/* 0 == ok */
	if (*tl == 0) {
		nfsm_disect(tl, u_long *, NFSX_UNSIGNED);
		if (*tl != 0) {
			error = fxdr_unsigned(int, *tl);
			m_freem(mrep);
			return (error);
		}
		*mrp = mrep;
		*mdp = md;
		*dposp = dpos;
		return (0);
	}
	m_freem(mrep);
	return (EPROTONOSUPPORT);
nfsmout:
	return (error);
}

/*
 * Get a request for the server main loop
 * - receive a request via. nfs_soreceive()
 * - verify it
 * - fill in the cred struct.
 */
int
nfs_getreq(so, prog, vers, maxproc, nam, mrp, mdp, dposp, retxid, procnum, cr,
	msk, mtch, wascomp, repstat)				/* 08 Aug 92*/
	struct socket *so;
	u_long prog;
	u_long vers;
	int maxproc;
	struct mbuf **nam;
	struct mbuf **mrp;
	struct mbuf **mdp;
	caddr_t *dposp;
	u_long *retxid;
	u_long *procnum;
	register struct ucred *cr;
	struct mbuf *msk, *mtch;
	int *wascomp, *repstat;					/* 08 Aug 92*/
{
	register int i;
	register u_long *tl;
	register long t1;
	caddr_t dpos, cp2;
	int error = 0;
	struct mbuf *mrep, *md;
	int len;

	*repstat = 0;						/* 08 Aug 92*/
	if (so->so_proto->pr_flags & PR_CONNREQUIRED) {
		error = nfs_receive(so, nam, &mrep, (struct nfsreq *)0);
	} else {
		mrep = (struct mbuf *)0;
		do {
			if (mrep) {
				m_freem(*nam);
				m_freem(mrep);
			}
			error = nfs_receive(so, nam, &mrep, (struct nfsreq *)0);
		} while (!error && nfs_badnam(*nam, msk, mtch));
	}
	if (error)
		return (error);
	md = mrep;
	mrep = nfs_uncompress(mrep);
	if (mrep != md) {
		*wascomp = 1;
		md = mrep;
	} else
		*wascomp = 0;
	dpos = mtod(mrep, caddr_t);
	nfsm_disect(tl, u_long *, 10*NFSX_UNSIGNED);
	*retxid = *tl++;
	if (*tl++ != rpc_call || *tl++ != rpc_vers) {		/* 08 Aug 92*/
		*mrp = mrep;
		*procnum = NFSPROC_NOOP;
		*repstat = ERPCMISMATCH;
		return (0);
	}
	if (*tl++ != prog) {
		*mrp = mrep;					/* 08 Aug 92*/
		*procnum = NFSPROC_NOOP;
		*repstat = EPROGUNAVAIL;
		return (0);
	}
	if (*tl++ != vers) {
		*mrp = mrep;					/* 08 Aug 92*/
		*procnum = NFSPROC_NOOP;
		*repstat = EPROGMISMATCH;
		return (0);
	}
	*procnum = fxdr_unsigned(u_long, *tl++);
	if (*procnum == NFSPROC_NULL) {
		*mrp = mrep;
		return (0);
	}
	if (*procnum > maxproc || *tl++ != rpc_auth_unix) {
		*mrp = mrep;					/* 08 Aug 92*/
		*procnum = NFSPROC_NOOP;
		*repstat = EPROCUNAVAIL;
		return (0);
	}
	len = fxdr_unsigned(int, *tl++);
	if (len < 0 || len > RPCAUTH_MAXSIZ) {
		m_freem(mrep);
		return (EBADRPC);
	}
	len = fxdr_unsigned(int, *++tl);
	if (len < 0 || len > NFS_MAXNAMLEN) {
		m_freem(mrep);
		return (EBADRPC);
	}
	nfsm_adv(nfsm_rndup(len));
	nfsm_disect(tl, u_long *, 3*NFSX_UNSIGNED);
	cr->cr_uid = fxdr_unsigned(uid_t, *tl++);
	cr->cr_gid = fxdr_unsigned(gid_t, *tl++);
	len = fxdr_unsigned(int, *tl);
	if (len < 0 || len > RPCAUTH_UNIXGIDS) {
		m_freem(mrep);
		return (EBADRPC);
	}
	nfsm_disect(tl, u_long *, (len + 2)*NFSX_UNSIGNED);
	for (i = 1; i <= len; i++)
		if (i < NGROUPS)
			cr->cr_groups[i] = fxdr_unsigned(gid_t, *tl++);
		else
			tl++;
	cr->cr_ngroups = (len >= NGROUPS) ? NGROUPS : (len + 1);
	/*
	 * Do we have any use for the verifier.
	 * According to the "Remote Procedure Call Protocol Spec." it
	 * should be AUTH_NULL, but some clients make it AUTH_UNIX?
	 * For now, just skip over it
	 */
	len = fxdr_unsigned(int, *++tl);
	if (len < 0 || len > RPCAUTH_MAXSIZ) {
		m_freem(mrep);
		return (EBADRPC);
	}
	if (len > 0)
		nfsm_adv(nfsm_rndup(len));
	*mrp = mrep;
	*mdp = md;
	*dposp = dpos;
	return (0);
nfsmout:
	return (error);
}

/*
 * Generate the rpc reply header
 * siz arg. is used to decide if adding a cluster is worthwhile
 */
int
nfs_rephead(siz, retxid, err, mrq, mbp, bposp)
	int siz;
	u_long retxid;
	int err;
	struct mbuf **mrq;
	struct mbuf **mbp;
	caddr_t *bposp;
{
	register u_long *tl;
	register long t1;
	caddr_t bpos;
	struct mbuf *mreq, *mb, *mb2;

	NFSMGETHDR(mreq);
	mb = mreq;
	if ((siz+RPC_REPLYSIZ) > MHLEN)
		MCLGET(mreq, M_WAIT);
	tl = mtod(mreq, u_long *);
	mreq->m_len = 6*NFSX_UNSIGNED;
	bpos = ((caddr_t)tl)+mreq->m_len;
	*tl++ = retxid;
	*tl++ = rpc_reply;
	if (err == ERPCMISMATCH) {
		*tl++ = rpc_msgdenied;
		*tl++ = rpc_mismatch;
		*tl++ = txdr_unsigned(2);
		*tl = txdr_unsigned(2);
	} else {
		*tl++ = rpc_msgaccepted;
		*tl++ = 0;
		*tl++ = 0;
		switch (err) {
		case EPROGUNAVAIL:
			*tl = txdr_unsigned(RPC_PROGUNAVAIL);
			break;
		case EPROGMISMATCH:
			*tl = txdr_unsigned(RPC_PROGMISMATCH);
			nfsm_build(tl, u_long *, 2*NFSX_UNSIGNED);
			*tl++ = txdr_unsigned(2);
			*tl = txdr_unsigned(2);	/* someday 3 */
			break;
		case EPROCUNAVAIL:
			*tl = txdr_unsigned(RPC_PROCUNAVAIL);
			break;
		default:
			*tl = 0;
			if (err != VNOVAL) {
				nfsm_build(tl, u_long *, NFSX_UNSIGNED);
				*tl = txdr_unsigned(err);
			}
			break;
		};
	}
	*mrq = mreq;
	*mbp = mb;
	*bposp = bpos;
	if (err != 0 && err != VNOVAL)
		nfsstats.srvrpc_errs++;
	return (0);
}

/*
 * Nfs timer routine
 * Scan the nfsreq list and retranmit any requests that have timed out
 * To avoid retransmission attempts on STREAM sockets (in the future) make
 * sure to set the r_retry field to 0 (implies nm_retry == 0).
 */
void
nfs_timer(arg)
     caddr_t arg;
{
	register struct nfsreq *rep;
	register struct mbuf *m;
	register struct socket *so;
	register struct nfsmount *nmp;
	int s, error;

	s = splnet();
	for (rep = nfsreqh.r_next; rep != &nfsreqh; rep = rep->r_next) {
		nmp = rep->r_nmp;
		if (rep->r_mrep || (rep->r_flags & R_SOFTTERM) ||
		    (so = nmp->nm_so) == NULL)
			continue;
		if ((nmp->nm_flag & NFSMNT_INT) && nfs_sigintr(rep->r_procp)) {
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (rep->r_flags & R_TIMING)	/* update rtt in mount */
			nmp->nm_rtt++;
		/* If not timed out */
		if (++rep->r_timer < nmp->nm_rto)
			continue;
		/* Do backoff and save new timeout in mount */
		if (rep->r_flags & R_TIMING) {
			nfs_backofftimer(nmp);
			rep->r_flags &= ~R_TIMING;
			nmp->nm_rtt = -1;
		}
		if (rep->r_flags & R_SENT) {
			rep->r_flags &= ~R_SENT;
			nmp->nm_sent--;
		}

		/*
		 * Check for too many retries on soft mount.
		 * nb: For hard mounts, r_retry == NFS_MAXREXMIT+1
		 */
		if (++rep->r_rexmit > NFS_MAXREXMIT)
			rep->r_rexmit = NFS_MAXREXMIT;

		/*
		 * Check for server not responding
		 */
		if ((rep->r_flags & R_TPRINTFMSG) == 0 &&
		     rep->r_rexmit > NFS_FISHY) {
			nfs_msg(rep->r_procp,
			    nmp->nm_mountp->mnt_stat.f_mntfromname,
			    "not responding");
			rep->r_flags |= R_TPRINTFMSG;
		}
		if (rep->r_rexmit >= rep->r_retry) {	/* too many */
			nfsstats.rpctimeouts++;
			rep->r_flags |= R_SOFTTERM;
			continue;
		}
		if (nmp->nm_sotype != SOCK_DGRAM)
			continue;

		/*
		 * If there is enough space and the window allows..
		 *	Resend it
		 */
		if (sbspace(&so->so_snd) >= rep->r_mreq->m_pkthdr.len &&
		       nmp->nm_sent < nmp->nm_window &&
		       (m = m_copym(rep->r_mreq, 0, M_COPYALL, M_DONTWAIT))){
			nfsstats.rpcretries++;
			if ((nmp->nm_flag & NFSMNT_NOCONN) == 0)
			    error = (*so->so_proto->pr_usrreq)(so, PRU_SEND, m,
			    (struct mbuf *)0, (struct mbuf *)0,
			    (struct mbuf *)0);
			else
			    error = (*so->so_proto->pr_usrreq)(so, PRU_SEND, m,
			    nmp->nm_nam, (struct mbuf *)0, (struct mbuf *)0);
			if (error) {
				if (NFSIGNORE_SOERROR(nmp->nm_soflags, error))
					so->so_error = 0;
			} else {
				/*
				 * We need to time the request even though we
				 * are retransmitting.
				 */
				nmp->nm_rtt = 0;
				nmp->nm_sent++;
				rep->r_flags |= (R_SENT|R_TIMING);
				rep->r_timer = rep->r_timerinit;
			}
		}
	}
	splx(s);
	timeout(nfs_timer, (caddr_t)0, hz/NFS_HZ);
}

/*
 * NFS timer update and backoff. The "Jacobson/Karels/Karn" scheme is
 * used here. The timer state is held in the nfsmount structure and
 * a single request is used to clock the response. When successful
 * the rtt smoothing in nfs_updatetimer is used, when failed the backoff
 * is done by nfs_backofftimer. We also log failure messages in these
 * routines.
 *
 * Congestion variables are held in the nfshost structure which
 * is referenced by nfsmounts and shared per-server. This separation
 * makes it possible to do per-mount timing which allows varying disk
 * access times to be dealt with, while preserving a network oriented
 * congestion control scheme.
 *
 * The windowing implements the Jacobson/Karels slowstart algorithm
 * with adjusted scaling factors. We start with one request, then send
 * 4 more after each success until the ssthresh limit is reached, then
 * we increment at a rate proportional to the window. On failure, we
 * remember 3/4 the current window and clamp the send limit to 1. Note
 * ICMP source quench is not reflected in so->so_error so we ignore that
 * for now.
 *
 * NFS behaves much more like a transport protocol with these changes,
 * shedding the teenage pedal-to-the-metal tendencies of "other"
 * implementations.
 *
 * Timers and congestion avoidance by Tom Talpey, Open Software Foundation.
 */

/*
 * The TCP algorithm was not forgiving enough. Because the NFS server
 * responds only after performing lookups/diskio/etc, we have to be
 * more prepared to accept a spiky variance. The TCP algorithm is:
 * TCP_RTO(nmp) ((((nmp)->nm_srtt >> 2) + (nmp)->nm_rttvar) >> 1)
 */
#define NFS_RTO(nmp)	(((nmp)->nm_srtt >> 3) + (nmp)->nm_rttvar)

void
nfs_updatetimer(nmp)
	register struct nfsmount *nmp;
{

	/* If retransmitted, clear and return */
	if (nmp->nm_rexmit || nmp->nm_currexmit) {
		nmp->nm_rexmit = nmp->nm_currexmit = 0;
		return;
	}
	/* If have a measurement, do smoothing */
	if (nmp->nm_srtt) {
		register short delta;
		delta = nmp->nm_rtt - (nmp->nm_srtt >> 3);
		if ((nmp->nm_srtt += delta) <= 0)
			nmp->nm_srtt = 1;
		if (delta < 0)
			delta = -delta;
		delta -= (nmp->nm_rttvar >> 2);
		if ((nmp->nm_rttvar += delta) <= 0)
			nmp->nm_rttvar = 1;
	/* Else initialize */
	} else {
		nmp->nm_rttvar = nmp->nm_rtt << 1;
		if (nmp->nm_rttvar == 0) nmp->nm_rttvar = 2;
		nmp->nm_srtt = nmp->nm_rttvar << 2;
	}
	/* Compute new Retransmission TimeOut and clip */
	nmp->nm_rto = NFS_RTO(nmp);
	if (nmp->nm_rto < NFS_MINTIMEO)
		nmp->nm_rto = NFS_MINTIMEO;
	else if (nmp->nm_rto > NFS_MAXTIMEO)
		nmp->nm_rto = NFS_MAXTIMEO;

	/* Update window estimate */
	if (nmp->nm_window < nmp->nm_ssthresh)	/* quickly */
		nmp->nm_window += 4;
	else {						/* slowly */
		register long incr = ++nmp->nm_winext;
		incr = (incr * incr) / nmp->nm_window;
		if (incr > 0) {
			nmp->nm_winext = 0;
			++nmp->nm_window;
		}
	}
	if (nmp->nm_window > NFS_MAXWINDOW)
		nmp->nm_window = NFS_MAXWINDOW;
}

void
nfs_backofftimer(nmp)
	register struct nfsmount *nmp;
{
	register unsigned long newrto;

	/* Clip shift count */
	if (++nmp->nm_rexmit > 8 * sizeof nmp->nm_rto)
		nmp->nm_rexmit = 8 * sizeof nmp->nm_rto;
	/* Back off RTO exponentially */
	newrto = NFS_RTO(nmp);
	newrto <<= (nmp->nm_rexmit - 1);
	if (newrto == 0 || newrto > NFS_MAXTIMEO)
		newrto = NFS_MAXTIMEO;
	nmp->nm_rto = newrto;

	/* If too many retries, message, assume a bogus RTT and re-measure */
	if (nmp->nm_currexmit < nmp->nm_rexmit) {
		nmp->nm_currexmit = nmp->nm_rexmit;
		if (nmp->nm_currexmit >= nfsrexmtthresh) {
			if (nmp->nm_currexmit == nfsrexmtthresh) {
				nmp->nm_rttvar += (nmp->nm_srtt >> 2);
				nmp->nm_srtt = 0;
			}
		}
	}
	/* Close down window but remember this point (3/4 current) for later */
	nmp->nm_ssthresh = ((nmp->nm_window << 1) + nmp->nm_window) >> 2;
	nmp->nm_window = 1;
	nmp->nm_winext = 0;
}

/*
 * Test for a termination signal pending on procp.
 * This is used for NFSMNT_INT mounts.
 */
int
nfs_sigintr(p)
	register struct proc *p;
{
	if (p && p->p_sig && (((p->p_sig &~ p->p_sigmask) &~ p->p_sigignore) &
	    NFSINT_SIGMASK))
		return (1);
	else
		return (0);
}

void
nfs_msg(p, server, msg)
	struct proc *p;
	const char *server, *msg;
{
	tpr_t tpr;

	if (p)
		tpr = tprintf_open(p);
	else
		tpr = NULL;
	tprintf(tpr, "nfs server %s: %s\n", server, msg);
	tprintf_close(tpr);
}

/*
 * Lock a socket against others.
 * Necessary for STREAM sockets to ensure you get an entire rpc request/reply
 * and also to avoid race conditions between the processes with nfs requests
 * in progress when a reconnect is necessary.
 */
void
nfs_solock(flagp)
	register int *flagp;
{

	while (*flagp & NFSMNT_SCKLOCK) {
		*flagp |= NFSMNT_WANTSCK;
		(void) tsleep((caddr_t)flagp, PZERO-1, "nfsolck", 0);
	}
	*flagp |= NFSMNT_SCKLOCK;
}

/*
 * Unlock the stream socket for others.
 */
void
nfs_sounlock(flagp)
	register int *flagp;
{

	if ((*flagp & NFSMNT_SCKLOCK) == 0)
		panic("nfs sounlock");
	*flagp &= ~NFSMNT_SCKLOCK;
	if (*flagp & NFSMNT_WANTSCK) {
		*flagp &= ~NFSMNT_WANTSCK;
		wakeup((caddr_t)flagp);
	}
}

/*
 * This function compares two net addresses by family and returns TRUE
 * if they are the same.
 * If there is any doubt, return FALSE.
 */
int
nfs_netaddr_match(nam1, nam2)
	struct mbuf *nam1, *nam2;
{
	register struct sockaddr *saddr1, *saddr2;

	saddr1 = mtod(nam1, struct sockaddr *);
	saddr2 = mtod(nam2, struct sockaddr *);
	if (saddr1->sa_family != saddr2->sa_family)
		return (0);

	/*
	 * Must do each address family separately since unused fields
	 * are undefined values and not always zeroed.
	 */
	switch (saddr1->sa_family) {
	case AF_INET:
		if (((struct sockaddr_in *)saddr1)->sin_addr.s_addr ==
		    ((struct sockaddr_in *)saddr2)->sin_addr.s_addr)
			return (1);
		break;
	default:
		break;
	};
	return (0);
}

/*
 * Check the hostname fields for nfsd's mask and match fields.
 * By address family:
 * - Bitwise AND the mask with the host address field
 * - Compare for == with match
 * return TRUE if not equal
 */
int
nfs_badnam(nam, msk, mtch)
	register struct mbuf *nam, *msk, *mtch;
{
	switch (mtod(nam, struct sockaddr *)->sa_family) {
	case AF_INET:
		return ((mtod(nam, struct sockaddr_in *)->sin_addr.s_addr &
			 mtod(msk, struct sockaddr_in *)->sin_addr.s_addr) !=
			 mtod(mtch, struct sockaddr_in *)->sin_addr.s_addr);
	default:
		printf("nfs_badmatch, unknown sa_family\n");
		return (0);
	};
}
