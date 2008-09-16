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
 *	@(#)nfs_syscalls.c	8.5 (Berkeley) 3/30/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/lockf.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef INET6
#include <net/if.h>
#include <netinet6/in6_var.h>
#endif
#include <nfs/xdr_subs.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsserver/nfs.h>
#include <nfsserver/nfsm_subs.h>
#include <nfsserver/nfsrvcache.h>

static MALLOC_DEFINE(M_NFSSVC, "nfss_srvsock", "Nfs server structure");

MALLOC_DEFINE(M_NFSRVDESC, "nfss_srvdesc", "NFS server socket descriptor");
MALLOC_DEFINE(M_NFSD, "nfss_daemon", "Nfs server daemon structure");

#define	TRUE	1
#define	FALSE	0

SYSCTL_DECL(_vfs_nfsrv);

int		nfsd_waiting = 0;
int		nfsrv_numnfsd = 0;
static int	notstarted = 1;

static int	nfs_privport = 0;
SYSCTL_INT(_vfs_nfsrv, NFS_NFSPRIVPORT, nfs_privport, CTLFLAG_RW,
	    &nfs_privport, 0, "");
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, gatherdelay, CTLFLAG_RW,
	    &nfsrvw_procrastinate, 0, "");
SYSCTL_INT(_vfs_nfsrv, OID_AUTO, gatherdelay_v3, CTLFLAG_RW,
	    &nfsrvw_procrastinate_v3, 0, "");

static int	nfssvc_addsock(struct file *, struct sockaddr *);
static void	nfsrv_zapsock(struct nfssvc_sock *slp);
static int	nfssvc_nfsd(void);

extern u_long sb_max_adj;

/*
 * NFS server system calls
 */

/*
 * Nfs server psuedo system call for the nfsd's
 * Based on the flag value it either:
 * - adds a socket to the selection list
 * - remains in the kernel as an nfsd
 * - remains in the kernel as an nfsiod
 * For INET6 we suppose that nfsd provides only IN6P_IPV6_V6ONLY sockets
 * and that mountd provides
 *  - sockaddr with no IPv4-mapped addresses
 *  - mask for both INET and INET6 families if there is IPv4-mapped overlap
 */
#ifndef _SYS_SYSPROTO_H_
struct nfssvc_args {
	int flag;
	caddr_t argp;
};
#endif
int
nfssvc(struct thread *td, struct nfssvc_args *uap)
{
	struct file *fp;
	struct sockaddr *nam;
	struct nfsd_args nfsdarg;
	int error;

	KASSERT(!mtx_owned(&Giant), ("nfssvc(): called with Giant"));

	error = priv_check(td, PRIV_NFS_DAEMON);
	if (error)
		return (error);
	NFSD_LOCK();
	while (nfssvc_sockhead_flag & SLP_INIT) {
		 nfssvc_sockhead_flag |= SLP_WANTINIT;
		(void) msleep(&nfssvc_sockhead, &nfsd_mtx, PSOCK,
		    "nfsd init", 0);
	}
	NFSD_UNLOCK();
	if (uap->flag & NFSSVC_ADDSOCK) {
		error = copyin(uap->argp, (caddr_t)&nfsdarg, sizeof(nfsdarg));
		if (error)
			return (error);
		if ((error = fget(td, nfsdarg.sock, &fp)) != 0)
			return (error);
		if (fp->f_type != DTYPE_SOCKET) {
			fdrop(fp, td);
			return (error);	/* XXXRW: Should be EINVAL? */
		}
		/*
		 * Get the client address for connected sockets.
		 */
		if (nfsdarg.name == NULL || nfsdarg.namelen == 0)
			nam = NULL;
		else {
			error = getsockaddr(&nam, nfsdarg.name,
					    nfsdarg.namelen);
			if (error) {
				fdrop(fp, td);
				return (error);
			}
		}
		error = nfssvc_addsock(fp, nam);
		fdrop(fp, td);
	} else if (uap->flag & NFSSVC_NFSD) {
		error = nfssvc_nfsd();
	} else {
		error = ENXIO;
	}
	if (error == EINTR || error == ERESTART)
		error = 0;
	return (error);
}

/*
 * Adds a socket to the list for servicing by nfsds.
 */
static int
nfssvc_addsock(struct file *fp, struct sockaddr *mynam)
{
	int siz;
	struct nfssvc_sock *slp;
	struct socket *so;
	int error;

	so = fp->f_data;
#if 0
	/*
	 * XXXRW: If this code is ever enabled, there's a race when running
	 * MPSAFE.
	 */
	tslp = NULL;
	/*
	 * Add it to the list, as required.
	 */
	if (so->so_proto->pr_protocol == IPPROTO_UDP) {
		tslp = nfs_udpsock;
		if (tslp->ns_flag & SLP_VALID) {
			if (mynam != NULL)
				FREE(mynam, M_SONAME);
			return (EPERM);
		}
	}
#endif
	siz = sb_max_adj;
	error = soreserve(so, siz, siz);
	if (error) {
		if (mynam != NULL)
			FREE(mynam, M_SONAME);
		return (error);
	}

	/*
	 * Set protocol specific options { for now TCP only } and
	 * reserve some space. For datagram sockets, this can get called
	 * repeatedly for the same socket, but that isn't harmful.
	 */
	if (so->so_type == SOCK_STREAM) {
		struct sockopt sopt;
		int val;

		bzero(&sopt, sizeof sopt);
		sopt.sopt_dir = SOPT_SET;
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
		sopt.sopt_dir = SOPT_SET;
		sopt.sopt_level = IPPROTO_TCP;
		sopt.sopt_name = TCP_NODELAY;
		sopt.sopt_val = &val;
		sopt.sopt_valsize = sizeof val;
		val = 1;
		sosetopt(so, &sopt);
	}
	SOCKBUF_LOCK(&so->so_rcv);
	so->so_rcv.sb_flags &= ~SB_NOINTR;
	so->so_rcv.sb_timeo = 0;
	SOCKBUF_UNLOCK(&so->so_rcv);
	SOCKBUF_LOCK(&so->so_snd);
	so->so_snd.sb_flags &= ~SB_NOINTR;
	so->so_snd.sb_timeo = 0;
	SOCKBUF_UNLOCK(&so->so_snd);

	slp = (struct nfssvc_sock *)
		malloc(sizeof (struct nfssvc_sock), M_NFSSVC,
		M_WAITOK | M_ZERO);
	STAILQ_INIT(&slp->ns_rec);
	NFSD_LOCK();
	TAILQ_INSERT_TAIL(&nfssvc_sockhead, slp, ns_chain);

	slp->ns_so = so;
	slp->ns_nam = mynam;
	fhold(fp);
	slp->ns_fp = fp;
	SOCKBUF_LOCK(&so->so_rcv);
	so->so_upcallarg = (caddr_t)slp;
	so->so_upcall = nfsrv_rcv;
	so->so_rcv.sb_flags |= SB_UPCALL;
	SOCKBUF_UNLOCK(&so->so_rcv);
	slp->ns_flag = (SLP_VALID | SLP_NEEDQ);
	nfsrv_wakenfsd(slp);
	NFSD_UNLOCK();
	return (0);
}

/*
 * Called by nfssvc() for nfsds. Just loops around servicing rpc requests
 * until it is killed by a signal.
 */
static int
nfssvc_nfsd()
{
	int siz;
	struct nfssvc_sock *slp;
	struct nfsd *nfsd;
	struct nfsrv_descript *nd = NULL;
	struct mbuf *m, *mreq;
	int error = 0, cacherep, s, sotype, writes_todo;
	int procrastinate;
	u_quad_t cur_usec;

#ifndef nolint
	cacherep = RC_DOIT;
	writes_todo = 0;
#endif
	nfsd = (struct nfsd *)
		malloc(sizeof (struct nfsd), M_NFSD, M_WAITOK | M_ZERO);
	s = splnet();
	NFSD_LOCK();

	TAILQ_INSERT_TAIL(&nfsd_head, nfsd, nfsd_chain);
	nfsrv_numnfsd++;

	/*
	 * Loop getting rpc requests until SIGKILL.
	 */
	for (;;) {
		if ((nfsd->nfsd_flag & NFSD_REQINPROG) == 0) {
			while (nfsd->nfsd_slp == NULL &&
			    (nfsd_head_flag & NFSD_CHECKSLP) == 0) {
				nfsd->nfsd_flag |= NFSD_WAITING;
				nfsd_waiting++;
				error = msleep(nfsd, &nfsd_mtx,
				    PSOCK | PCATCH, "-", 0);
				nfsd_waiting--;
				if (error)
					goto done;
			}
			if (nfsd->nfsd_slp == NULL &&
			    (nfsd_head_flag & NFSD_CHECKSLP) != 0) {
				TAILQ_FOREACH(slp, &nfssvc_sockhead, ns_chain) {
				    if ((slp->ns_flag & (SLP_VALID | SLP_DOREC))
					== (SLP_VALID | SLP_DOREC)) {
					    slp->ns_flag &= ~SLP_DOREC;
					    slp->ns_sref++;
					    nfsd->nfsd_slp = slp;
					    break;
				    }
				}
				if (slp == NULL)
					nfsd_head_flag &= ~NFSD_CHECKSLP;
			}
			if ((slp = nfsd->nfsd_slp) == NULL)
				continue;
			if (slp->ns_flag & SLP_VALID) {
				if (slp->ns_flag & SLP_DISCONN)
					nfsrv_zapsock(slp);
				else if (slp->ns_flag & SLP_NEEDQ) {
					slp->ns_flag &= ~SLP_NEEDQ;
					(void) nfs_slplock(slp, 1);
					NFSD_UNLOCK();
					nfsrv_rcv(slp->ns_so, (caddr_t)slp,
						M_WAIT);
					NFSD_LOCK();
					nfs_slpunlock(slp);
				}
				error = nfsrv_dorec(slp, nfsd, &nd);
				cur_usec = nfs_curusec();
				if (error && LIST_FIRST(&slp->ns_tq) &&
				    LIST_FIRST(&slp->ns_tq)->nd_time <= cur_usec) {
					error = 0;
					cacherep = RC_DOIT;
					writes_todo = 1;
				} else
					writes_todo = 0;
				nfsd->nfsd_flag |= NFSD_REQINPROG;
			}
		} else {
			error = 0;
			slp = nfsd->nfsd_slp;
		}
		if (error || (slp->ns_flag & SLP_VALID) == 0) {
			if (nd) {
				if (nd->nd_cr != NULL)
					crfree(nd->nd_cr);
				free((caddr_t)nd, M_NFSRVDESC);
				nd = NULL;
			}
			nfsd->nfsd_slp = NULL;
			nfsd->nfsd_flag &= ~NFSD_REQINPROG;
			nfsrv_slpderef(slp);
			continue;
		}
		splx(s);
		sotype = slp->ns_so->so_type;
		if (nd) {
		    getmicrotime(&nd->nd_starttime);
		    if (nd->nd_nam2)
			nd->nd_nam = nd->nd_nam2;
		    else
			nd->nd_nam = slp->ns_nam;

		    /*
		     * Check to see if authorization is needed.
		     */
		    cacherep = nfsrv_getcache(nd, &mreq);

		    if (nfs_privport) {
			/* Check if source port is privileged */
			u_short port;
			struct sockaddr *nam = nd->nd_nam;
			struct sockaddr_in *sin;

			sin = (struct sockaddr_in *)nam;
			/*
			 * INET/INET6 - same code:
			 *    sin_port and sin6_port are at same offset
			 */
			port = ntohs(sin->sin_port);
			if (port >= IPPORT_RESERVED &&
			    nd->nd_procnum != NFSPROC_NULL) {
#ifdef INET6
			    char b6[INET6_ADDRSTRLEN];
#if defined(KLD_MODULE)
	/* Do not use ip6_sprintf: the nfs module should work without INET6. */
#define ip6_sprintf(buf, a) \
	 (sprintf((buf), "%x:%x:%x:%x:%x:%x:%x:%x", \
		  (a)->s6_addr16[0], (a)->s6_addr16[1], \
		  (a)->s6_addr16[2], (a)->s6_addr16[3], \
		  (a)->s6_addr16[4], (a)->s6_addr16[5], \
		  (a)->s6_addr16[6], (a)->s6_addr16[7]), \
	 (buf))
#endif
#endif
			    nd->nd_procnum = NFSPROC_NOOP;
			    nd->nd_repstat = (NFSERR_AUTHERR | AUTH_TOOWEAK);
			    cacherep = RC_DOIT;
			    printf("NFS request from unprivileged port (%s:%d)\n",
#ifdef INET6
				sin->sin_family == AF_INET6 ?
				    ip6_sprintf(b6, &satosin6(sin)->sin6_addr) :
#if defined(KLD_MODULE)
#undef ip6_sprintf
#endif
#endif
				    inet_ntoa(sin->sin_addr), port);
			}
		    }

		}

		/*
		 * Loop to get all the write rpc relies that have been
		 * gathered together.
		 */
		do {
		    switch (cacherep) {
		    case RC_DOIT:
			if (nd && (nd->nd_flag & ND_NFSV3))
			    procrastinate = nfsrvw_procrastinate_v3;
			else
			    procrastinate = nfsrvw_procrastinate;
			NFSD_UNLOCK();
			if (writes_todo || (!(nd->nd_flag & ND_NFSV3) &&
			    nd->nd_procnum == NFSPROC_WRITE &&
			    procrastinate > 0 && !notstarted))
			    error = nfsrv_writegather(&nd, slp, &mreq);
			else
			    error = (*(nfsrv3_procs[nd->nd_procnum]))(nd,
				slp, &mreq);
			NFSD_LOCK();
			if (mreq == NULL)
				break;
			if (error != 0 && error != NFSERR_RETVOID) {
				nfsrvstats.srv_errs++;
				nfsrv_updatecache(nd, FALSE, mreq);
				if (nd->nd_nam2)
					FREE(nd->nd_nam2, M_SONAME);
				break;
			}
			nfsrvstats.srvrpccnt[nd->nd_procnum]++;
			nfsrv_updatecache(nd, TRUE, mreq);
			nd->nd_mrep = NULL;
			/* FALLTHROUGH */
		    case RC_REPLY:
			NFSD_UNLOCK();
			siz = m_length(mreq, NULL);
			if (siz <= 0 || siz > NFS_MAXPACKET) {
				printf("mbuf siz=%d\n",siz);
				panic("Bad nfs svc reply");
			}
			m = mreq;
			m->m_pkthdr.len = siz;
			m->m_pkthdr.rcvif = NULL;
			/*
			 * For stream protocols, prepend a Sun RPC
			 * Record Mark.
			 */
			if (sotype == SOCK_STREAM) {
				M_PREPEND(m, NFSX_UNSIGNED, M_WAIT);
				*mtod(m, u_int32_t *) = htonl(0x80000000 | siz);
			}
			NFSD_LOCK();
			if (slp->ns_so->so_proto->pr_flags & PR_CONNREQUIRED)
				(void) nfs_slplock(slp, 1);
			if (slp->ns_flag & SLP_VALID) {
			    NFSD_UNLOCK();
			    error = nfsrv_send(slp->ns_so, nd->nd_nam2, m);
			    NFSD_LOCK();
			} else {
			    error = EPIPE;
			    m_freem(m);
			}
			if (nd->nd_nam2)
				FREE(nd->nd_nam2, M_SONAME);
			if (nd->nd_mrep)
				m_freem(nd->nd_mrep);
			if (error == EPIPE)
				nfsrv_zapsock(slp);
			if (slp->ns_so->so_proto->pr_flags & PR_CONNREQUIRED)
				nfs_slpunlock(slp);
			if (error == EINTR || error == ERESTART) {
				if (nd->nd_cr != NULL)
					crfree(nd->nd_cr);
				free((caddr_t)nd, M_NFSRVDESC);
				nfsrv_slpderef(slp);
				s = splnet();
				goto done;
			}
			break;
		    case RC_DROPIT:
			m_freem(nd->nd_mrep);
			if (nd->nd_nam2)
				FREE(nd->nd_nam2, M_SONAME);
			break;
		    };
		    if (nd) {
			if (nd->nd_cr != NULL)
				crfree(nd->nd_cr);
			FREE((caddr_t)nd, M_NFSRVDESC);
			nd = NULL;
		    }

		    /*
		     * Check to see if there are outstanding writes that
		     * need to be serviced.
		     */
		    cur_usec = nfs_curusec();
		    s = splsoftclock();
		    if (LIST_FIRST(&slp->ns_tq) &&
			LIST_FIRST(&slp->ns_tq)->nd_time <= cur_usec) {
			cacherep = RC_DOIT;
			writes_todo = 1;
		    } else
			writes_todo = 0;
		    splx(s);
		} while (writes_todo);
		s = splnet();
		if (nfsrv_dorec(slp, nfsd, &nd)) {
			nfsd->nfsd_flag &= ~NFSD_REQINPROG;
			nfsd->nfsd_slp = NULL;
			nfsrv_slpderef(slp);
		}
		mtx_assert(&Giant, MA_NOTOWNED);
	}
done:
	mtx_assert(&Giant, MA_NOTOWNED);
	TAILQ_REMOVE(&nfsd_head, nfsd, nfsd_chain);
	splx(s);
	free((caddr_t)nfsd, M_NFSD);
	if (--nfsrv_numnfsd == 0)
		nfsrv_init(TRUE);	/* Reinitialize everything */
	NFSD_UNLOCK();
	return (error);
}

/*
 * Shut down a socket associated with an nfssvc_sock structure.
 * Should be called with the send lock set, if required.
 * The trick here is to increment the sref at the start, so that the nfsds
 * will stop using it and clear ns_flag at the end so that it will not be
 * reassigned during cleanup.
 */
static void
nfsrv_zapsock(struct nfssvc_sock *slp)
{
	struct nfsrv_descript *nwp, *nnwp;
	struct socket *so;
	struct file *fp;
	struct nfsrv_rec *rec;
	int s;

	NFSD_LOCK_ASSERT();

	/*
	 * XXXRW: By clearing all flags, other threads/etc should ignore
	 * this slp and we can safely release nfsd_mtx so we can clean
	 * up the slp safely.
	 */
	slp->ns_flag &= ~SLP_ALLFLAGS;
	fp = slp->ns_fp;
	if (fp) {
		NFSD_UNLOCK();
		slp->ns_fp = NULL;
		so = slp->ns_so;
		SOCKBUF_LOCK(&so->so_rcv);
		so->so_rcv.sb_flags &= ~SB_UPCALL;
		so->so_upcall = NULL;
		so->so_upcallarg = NULL;
		SOCKBUF_UNLOCK(&so->so_rcv);
		soshutdown(so, SHUT_RDWR);
		closef(fp, NULL);
		NFSD_LOCK();
		if (slp->ns_nam)
			FREE(slp->ns_nam, M_SONAME);
		m_freem(slp->ns_raw);
		while ((rec = STAILQ_FIRST(&slp->ns_rec)) != NULL) {
			STAILQ_REMOVE_HEAD(&slp->ns_rec, nr_link);
			if (rec->nr_address)
				FREE(rec->nr_address, M_SONAME);
			m_freem(rec->nr_packet);
			free(rec, M_NFSRVDESC);
		}
		s = splsoftclock();
		for (nwp = LIST_FIRST(&slp->ns_tq); nwp; nwp = nnwp) {
			nnwp = LIST_NEXT(nwp, nd_tq);
			LIST_REMOVE(nwp, nd_tq);
			if (nwp->nd_cr != NULL)
				crfree(nwp->nd_cr);
			free((caddr_t)nwp, M_NFSRVDESC);
		}
		LIST_INIT(&slp->ns_tq);
		splx(s);
	}
}

/*
 * Derefence a server socket structure. If it has no more references and
 * is no longer valid, you can throw it away.
 */
void
nfsrv_slpderef(struct nfssvc_sock *slp)
{

	NFSD_LOCK_ASSERT();

	if (--(slp->ns_sref) == 0 && (slp->ns_flag & SLP_VALID) == 0) {
		TAILQ_REMOVE(&nfssvc_sockhead, slp, ns_chain);
		free((caddr_t)slp, M_NFSSVC);
	}
}

/*
 * Lock a socket against others.
 *
 * XXXRW: Wait argument is always 1 in the caller.  Replace with a real
 * sleep lock?
 */
int
nfs_slplock(struct nfssvc_sock *slp, int wait)
{
	int *statep = &slp->ns_solock;

	NFSD_LOCK_ASSERT();

	if (!wait && (*statep & NFSRV_SNDLOCK))
		return(0);	/* already locked, fail */
	while (*statep & NFSRV_SNDLOCK) {
		*statep |= NFSRV_WANTSND;
		(void) msleep(statep, &nfsd_mtx, PZERO - 1, "nfsslplck", 0);
	}
	*statep |= NFSRV_SNDLOCK;
	return (1);
}

/*
 * Unlock the stream socket for others.
 */
void
nfs_slpunlock(struct nfssvc_sock *slp)
{
	int *statep = &slp->ns_solock;

	NFSD_LOCK_ASSERT();

	if ((*statep & NFSRV_SNDLOCK) == 0)
		panic("nfs slpunlock");
	*statep &= ~NFSRV_SNDLOCK;
	if (*statep & NFSRV_WANTSND) {
		*statep &= ~NFSRV_WANTSND;
		wakeup(statep);
	}
}

/*
 * Initialize the data structures for the server.
 * Handshake with any new nfsds starting up to avoid any chance of
 * corruption.
 */
void
nfsrv_init(int terminating)
{
	struct nfssvc_sock *slp, *nslp;

	NFSD_LOCK_ASSERT();

	if (nfssvc_sockhead_flag & SLP_INIT)
		panic("nfsd init");
	nfssvc_sockhead_flag |= SLP_INIT;
	if (terminating) {
		TAILQ_FOREACH_SAFE(slp, &nfssvc_sockhead, ns_chain, nslp) {
			if (slp->ns_flag & SLP_VALID)
				nfsrv_zapsock(slp);
			TAILQ_REMOVE(&nfssvc_sockhead, slp, ns_chain);
			free((caddr_t)slp, M_NFSSVC);
		}
		nfsrv_cleancache();	/* And clear out server cache */
	} else
		nfs_pub.np_valid = 0;

	TAILQ_INIT(&nfssvc_sockhead);
	nfssvc_sockhead_flag &= ~SLP_INIT;
	if (nfssvc_sockhead_flag & SLP_WANTINIT) {
		nfssvc_sockhead_flag &= ~SLP_WANTINIT;
		wakeup(&nfssvc_sockhead);
	}

	TAILQ_INIT(&nfsd_head);
	nfsd_head_flag &= ~NFSD_CHECKSLP;

#if 0
	nfs_udpsock = (struct nfssvc_sock *)
	    malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK | M_ZERO);
	STAILQ_INIT(&nfs_udpsock->ns_rec);
	TAILQ_INSERT_HEAD(&nfssvc_sockhead, nfs_udpsock, ns_chain);

	nfs_cltpsock = (struct nfssvc_sock *)
	    malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK | M_ZERO);
	STAILQ_INIT(&nfs_cltpsock->ns_rec);
	TAILQ_INSERT_TAIL(&nfssvc_sockhead, nfs_cltpsock, ns_chain);
#endif
}
