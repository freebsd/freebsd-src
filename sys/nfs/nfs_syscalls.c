/*
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
 *	@(#)nfs_syscalls.c	8.3 (Berkeley) 1/4/94
 * $Id: nfs_syscalls.c,v 1.6 1995/05/30 08:12:45 rgrimes Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/namei.h>
#include <sys/syslog.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef ISO
#include <netiso/iso.h>
#endif
#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsrvcache.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>
#include <nfs/nqnfs.h>
#include <nfs/nfsrtt.h>

void	nfsrv_zapsock	__P((struct nfssvc_sock *));

/* Global defs. */
extern u_long nfs_prog, nfs_vers;
#ifdef NFS_SERVER
extern int (*nfsrv_procs[NFS_NPROCS])();
#endif
extern struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
extern int nfs_numasync;
extern time_t nqnfsstarttime;
extern int nqsrv_writeslack;
extern int nfsrtton;
#ifdef NFS_SERVER
struct nfssvc_sock *nfs_udpsock, *nfs_cltpsock;
int nuidhash_max = NFS_MAXUIDHASH;
int nfsd_waiting = 0;
#endif /* NFS_SERVER */
static int nfs_numnfsd = 0;
static int notstarted = 1;
static int modify_flag = 0;
static struct nfsdrt nfsdrt;
void nfsrv_cleancache(), nfsrv_rcv(), nfsrv_wakenfsd(), nfs_sndunlock();
static void nfsd_rt();
void nfsrv_slpderef();

#define	TRUE	1
#define	FALSE	0

static int nfs_asyncdaemon[NFS_MAXASYNCDAEMON];
/*
 * NFS server system calls
 * getfh() lives here too, but maybe should move to kern/vfs_syscalls.c
 */

/*
 * Get file handle system call
 */
struct getfh_args {
	char	*fname;
	fhandle_t *fhp;
};
int
getfh(p, uap, retval)
	struct proc *p;
	register struct getfh_args *uap;
	int *retval;
{
	register struct vnode *vp;
	fhandle_t fh;
	int error;
	struct nameidata nd;

	/*
	 * Must be super user
	 */
	error = suser(p->p_ucred, &p->p_acflag);
	if(error)
		return (error);
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE, uap->fname, p);
	error = namei(&nd);
	if (error)
		return (error);
	vp = nd.ni_vp;
	bzero((caddr_t)&fh, sizeof(fh));
	fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
	error = VFS_VPTOFH(vp, &fh.fh_fid);
	vput(vp);
	if (error)
		return (error);
	error = copyout((caddr_t)&fh, (caddr_t)uap->fhp, sizeof (fh));
	return (error);
}

/*
 * Nfs server psuedo system call for the nfsd's
 * Based on the flag value it either:
 * - adds a socket to the selection list
 * - remains in the kernel as an nfsd
 * - remains in the kernel as an nfsiod
 */
struct nfssvc_args {
	int flag;
	caddr_t argp;
};
int
nfssvc(p, uap, retval)
	struct proc *p;
	register struct nfssvc_args *uap;
	int *retval;
{
	struct nameidata nd;
	struct file *fp;
	struct mbuf *nam;
	struct nfsd_args nfsdarg;
	struct nfsd_srvargs nfsd_srvargs, *nsd = &nfsd_srvargs;
	struct nfsd_cargs ncd;
	struct nfsd *nfsd;
	struct nfssvc_sock *slp;
	struct nfsuid *nuidp;
	struct nfsmount *nmp;
	int error;

	/*
	 * Must be super user
	 */
	error = suser(p->p_ucred, &p->p_acflag);
	if(error)
		return (error);
	while (nfssvc_sockhead_flag & SLP_INIT) {
		 nfssvc_sockhead_flag |= SLP_WANTINIT;
		(void) tsleep((caddr_t)&nfssvc_sockhead, PSOCK, "nfsd init", 0);
	}
	if (0) {
		;
#ifdef NFS_CLIENT
	} else if (uap->flag & NFSSVC_BIOD) {
		error = nfssvc_iod(p);
	} else if (uap->flag & NFSSVC_MNTD) {
		error = copyin(uap->argp, (caddr_t)&ncd, sizeof (ncd));
		if (error)
			return (error);
		NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
			ncd.ncd_dirp, p);
		error = namei(&nd);
		if (error)
			return (error);
		if ((nd.ni_vp->v_flag & VROOT) == 0)
			error = EINVAL;
		nmp = VFSTONFS(nd.ni_vp->v_mount);
		vput(nd.ni_vp);
		if (error)
			return (error);
		if ((nmp->nm_flag & NFSMNT_MNTD) &&
			(uap->flag & NFSSVC_GOTAUTH) == 0)
			return (0);
		nmp->nm_flag |= NFSMNT_MNTD;
		error = nqnfs_clientd(nmp, p->p_ucred, &ncd, uap->flag,
			uap->argp, p);
#endif /* NFS_CLIENT */
#ifdef NFS_SERVER
	} else if (uap->flag & NFSSVC_ADDSOCK) {
		error = copyin(uap->argp, (caddr_t)&nfsdarg, sizeof(nfsdarg));
		if (error)
			return (error);
		error = getsock(p->p_fd, nfsdarg.sock, &fp);
		if (error)
			return (error);
		/*
		 * Get the client address for connected sockets.
		 */
		if (nfsdarg.name == NULL || nfsdarg.namelen == 0)
			nam = (struct mbuf *)0;
		else {
			error = sockargs(&nam, nfsdarg.name, nfsdarg.namelen,
				MT_SONAME);
			if (error)
				return (error);
		}
		error = nfssvc_addsock(fp, nam);
	} else {
		error = copyin(uap->argp, (caddr_t)nsd, sizeof (*nsd));
		if (error)
			return (error);
		if ((uap->flag & NFSSVC_AUTHIN) && ((nfsd = nsd->nsd_nfsd)) &&
			(nfsd->nd_slp->ns_flag & SLP_VALID)) {
			slp = nfsd->nd_slp;

			/*
			 * First check to see if another nfsd has already
			 * added this credential.
			 */
			for (nuidp = NUIDHASH(slp, nsd->nsd_uid)->lh_first;
			    nuidp != 0; nuidp = nuidp->nu_hash.le_next) {
				if (nuidp->nu_uid == nsd->nsd_uid)
					break;
			}
			if (!nuidp) {
			    /*
			     * Nope, so we will.
			     */
			    if (slp->ns_numuids < nuidhash_max) {
				slp->ns_numuids++;
				nuidp = (struct nfsuid *)
				   malloc(sizeof (struct nfsuid), M_NFSUID,
					M_WAITOK);
			    } else
				nuidp = (struct nfsuid *)0;
			    if ((slp->ns_flag & SLP_VALID) == 0) {
				if (nuidp)
				    free((caddr_t)nuidp, M_NFSUID);
			    } else {
				if (nuidp == (struct nfsuid *)0) {
				    nuidp = slp->ns_uidlruhead.tqh_first;
				    LIST_REMOVE(nuidp, nu_hash);
				    TAILQ_REMOVE(&slp->ns_uidlruhead, nuidp,
					nu_lru);
			        }
				nuidp->nu_cr = nsd->nsd_cr;
				if (nuidp->nu_cr.cr_ngroups > NGROUPS)
					nuidp->nu_cr.cr_ngroups = NGROUPS;
				nuidp->nu_cr.cr_ref = 1;
				nuidp->nu_uid = nsd->nsd_uid;
				TAILQ_INSERT_TAIL(&slp->ns_uidlruhead, nuidp,
					nu_lru);
				LIST_INSERT_HEAD(NUIDHASH(slp, nsd->nsd_uid),
					nuidp, nu_hash);

			    }
			}
		}
		if ((uap->flag & NFSSVC_AUTHINFAIL) && (nfsd = nsd->nsd_nfsd))
			nfsd->nd_flag |= NFSD_AUTHFAIL;
		error = nfssvc_nfsd(nsd, uap->argp, p);
#endif /* NFS_SERVER */
	}
	if (error == EINTR || error == ERESTART)
		error = 0;
	return (error);
}

#ifdef NFS_SERVER
/*
 * Adds a socket to the list for servicing by nfsds.
 */
int
nfssvc_addsock(fp, mynam)
	struct file *fp;
	struct mbuf *mynam;
{
	register struct mbuf *m;
	register int siz;
	register struct nfssvc_sock *slp;
	register struct socket *so;
	struct nfssvc_sock *tslp;
	int error, s;

	so = (struct socket *)fp->f_data;
	tslp = (struct nfssvc_sock *)0;
	/*
	 * Add it to the list, as required.
	 */
	if (so->so_proto->pr_protocol == IPPROTO_UDP) {
		tslp = nfs_udpsock;
		if (tslp->ns_flag & SLP_VALID) {
			m_freem(mynam);
			return (EPERM);
		}
#ifdef ISO
	} else if (so->so_proto->pr_protocol == ISOPROTO_CLTP) {
		tslp = nfs_cltpsock;
		if (tslp->ns_flag & SLP_VALID) {
			m_freem(mynam);
			return (EPERM);
		}
#endif /* ISO */
	}
	if (so->so_type == SOCK_STREAM)
		siz = NFS_MAXPACKET + sizeof (u_long);
	else
		siz = NFS_MAXPACKET;
	error = soreserve(so, siz, siz);
	if (error) {
		m_freem(mynam);
		return (error);
	}

	/*
	 * Set protocol specific options { for now TCP only } and
	 * reserve some space. For datagram sockets, this can get called
	 * repeatedly for the same socket, but that isn't harmful.
	 */
	if (so->so_type == SOCK_STREAM) {
		MGET(m, M_WAIT, MT_SOOPTS);
		*mtod(m, int *) = 1;
		m->m_len = sizeof(int);
		sosetopt(so, SOL_SOCKET, SO_KEEPALIVE, m);
	}
	if (so->so_proto->pr_domain->dom_family == AF_INET &&
	    so->so_proto->pr_protocol == IPPROTO_TCP) {
		MGET(m, M_WAIT, MT_SOOPTS);
		*mtod(m, int *) = 1;
		m->m_len = sizeof(int);
		sosetopt(so, IPPROTO_TCP, TCP_NODELAY, m);
	}
	so->so_rcv.sb_flags &= ~SB_NOINTR;
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_flags &= ~SB_NOINTR;
	so->so_snd.sb_timeo = 0;
	if (tslp)
		slp = tslp;
	else {
		slp = (struct nfssvc_sock *)
			malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK);
		bzero((caddr_t)slp, sizeof (struct nfssvc_sock));
		slp->ns_uidhashtbl =
		    hashinit(NUIDHASHSIZ, M_NFSSVC, &slp->ns_uidhash);
		TAILQ_INIT(&slp->ns_uidlruhead);
		TAILQ_INSERT_TAIL(&nfssvc_sockhead, slp, ns_chain);
	}
	slp->ns_so = so;
	slp->ns_nam = mynam;
	fp->f_count++;
	slp->ns_fp = fp;
	s = splnet();
	so->so_upcallarg = (caddr_t)slp;
	so->so_upcall = nfsrv_rcv;
	slp->ns_flag = (SLP_VALID | SLP_NEEDQ);
	nfsrv_wakenfsd(slp);
	splx(s);
	return (0);
}

/*
 * Called by nfssvc() for nfsds. Just loops around servicing rpc requests
 * until it is killed by a signal.
 */
int
nfssvc_nfsd(nsd, argp, p)
	struct nfsd_srvargs *nsd;
	caddr_t argp;
	struct proc *p;
{
	register struct mbuf *m, *nam2;
	register int siz;
	register struct nfssvc_sock *slp;
	register struct socket *so;
	register int *solockp;
	struct nfsd *nd = nsd->nsd_nfsd;
	struct mbuf *mreq, *nam;
	struct timeval starttime;
	struct nfsuid *uidp;
	int error = 0, cacherep, s;
	int sotype;

	s = splnet();
	if (nd == (struct nfsd *)0) {
		nsd->nsd_nfsd = nd = (struct nfsd *)
			malloc(sizeof (struct nfsd), M_NFSD, M_WAITOK);
		bzero((caddr_t)nd, sizeof (struct nfsd));
		nd->nd_procp = p;
		nd->nd_cr.cr_ref = 1;
		TAILQ_INSERT_TAIL(&nfsd_head, nd, nd_chain);
		nd->nd_nqlflag = NQL_NOVAL;
		nfs_numnfsd++;
	}
	/*
	 * Loop getting rpc requests until SIGKILL.
	 */
	for (;;) {
		if ((nd->nd_flag & NFSD_REQINPROG) == 0) {
			while (nd->nd_slp == (struct nfssvc_sock *)0 &&
			    (nfsd_head_flag & NFSD_CHECKSLP) == 0) {
				nd->nd_flag |= NFSD_WAITING;
				nfsd_waiting++;
				error = tsleep((caddr_t)nd, PSOCK | PCATCH, "nfsd", 0);
				nfsd_waiting--;
				if (error)
					goto done;
			}
			if (nd->nd_slp == (struct nfssvc_sock *)0 &&
			    (nfsd_head_flag & NFSD_CHECKSLP) != 0) {
				for (slp = nfssvc_sockhead.tqh_first; slp != 0;
				    slp = slp->ns_chain.tqe_next) {
				    if ((slp->ns_flag & (SLP_VALID | SLP_DOREC))
					== (SLP_VALID | SLP_DOREC)) {
					    slp->ns_flag &= ~SLP_DOREC;
					    slp->ns_sref++;
					    nd->nd_slp = slp;
					    break;
				    }
				}
				if (slp == 0)
					nfsd_head_flag &= ~NFSD_CHECKSLP;
			}
			if ((slp = nd->nd_slp) == (struct nfssvc_sock *)0)
				continue;
			if (slp->ns_flag & SLP_VALID) {
				if (slp->ns_flag & SLP_DISCONN)
					nfsrv_zapsock(slp);
				else if (slp->ns_flag & SLP_NEEDQ) {
					slp->ns_flag &= ~SLP_NEEDQ;
					(void) nfs_sndlock(&slp->ns_solock,
						(struct nfsreq *)0);
					nfsrv_rcv(slp->ns_so, (caddr_t)slp,
						M_WAIT);
					nfs_sndunlock(&slp->ns_solock);
				}
				error = nfsrv_dorec(slp, nd);
				nd->nd_flag |= NFSD_REQINPROG;
			}
		} else {
			error = 0;
			slp = nd->nd_slp;
		}
		if (error || (slp->ns_flag & SLP_VALID) == 0) {
			nd->nd_slp = (struct nfssvc_sock *)0;
			nd->nd_flag &= ~NFSD_REQINPROG;
			nfsrv_slpderef(slp);
			continue;
		}
		splx(s);
		so = slp->ns_so;
		sotype = so->so_type;
		starttime = time;
		if (so->so_proto->pr_flags & PR_CONNREQUIRED)
			solockp = &slp->ns_solock;
		else
			solockp = (int *)0;
		/*
		 * nam == nam2 for connectionless protocols such as UDP
		 * nam2 == NULL for connection based protocols to disable
		 *    recent request caching.
		 */
		nam2 = nd->nd_nam;
		if (nam2) {
			nam = nam2;
			cacherep = RC_CHECKIT;
		} else {
			nam = slp->ns_nam;
			cacherep = RC_DOIT;
		}

		/*
		 * Check to see if authorization is needed.
		 */
		if (nd->nd_flag & NFSD_NEEDAUTH) {
			static int logauth = 0;

			nd->nd_flag &= ~NFSD_NEEDAUTH;
			/*
			 * Check for a mapping already installed.
			 */
			for (uidp = NUIDHASH(slp, nd->nd_cr.cr_uid)->lh_first;
			    uidp != 0; uidp = uidp->nu_hash.le_next) {
				if (uidp->nu_uid == nd->nd_cr.cr_uid)
					break;
			}
			if (!uidp) {
			    nsd->nsd_uid = nd->nd_cr.cr_uid;
			    if (nam2 && logauth++ == 0)
				log(LOG_WARNING, "Kerberized NFS using UDP\n");
			    nsd->nsd_haddr =
			      mtod(nam, struct sockaddr_in *)->sin_addr.s_addr;
			    nsd->nsd_authlen = nd->nd_authlen;
			    if (copyout(nd->nd_authstr, nsd->nsd_authstr,
				nd->nd_authlen) == 0 &&
				copyout((caddr_t)nsd, argp, sizeof (*nsd)) == 0)
				return (ENEEDAUTH);
			    cacherep = RC_DROPIT;
			}
		}
		if (cacherep == RC_CHECKIT)
			cacherep = nfsrv_getcache(nam2, nd, &mreq);

		/*
		 * Check for just starting up for NQNFS and send
		 * fake "try again later" replies to the NQNFS clients.
		 */
		if (notstarted && nqnfsstarttime <= time.tv_sec) {
			if (modify_flag) {
				nqnfsstarttime = time.tv_sec + nqsrv_writeslack;
				modify_flag = 0;
			} else
				notstarted = 0;
		}
		if (notstarted) {
			if (nd->nd_nqlflag == NQL_NOVAL)
				cacherep = RC_DROPIT;
			else if (nd->nd_procnum != NFSPROC_WRITE) {
				nd->nd_procnum = NFSPROC_NOOP;
				nd->nd_repstat = NQNFS_TRYLATER;
				cacherep = RC_DOIT;
			} else
				modify_flag = 1;
		} else if (nd->nd_flag & NFSD_AUTHFAIL) {
			nd->nd_flag &= ~NFSD_AUTHFAIL;
			nd->nd_procnum = NFSPROC_NOOP;
			nd->nd_repstat = NQNFS_AUTHERR;
			cacherep = RC_DOIT;
		}

		switch (cacherep) {
		case RC_DOIT:
			error = (*(nfsrv_procs[nd->nd_procnum]))(nd,
				nd->nd_mrep, nd->nd_md, nd->nd_dpos, &nd->nd_cr,
				nam, &mreq);
			if (nd->nd_cr.cr_ref != 1) {
				printf("nfssvc cref=%d\n", nd->nd_cr.cr_ref);
				panic("nfssvc cref");
			}
			if (error) {
				if (nd->nd_procnum != NQNFSPROC_VACATED)
					nfsstats.srv_errs++;
				if (nam2) {
					nfsrv_updatecache(nam2, nd, FALSE, mreq);
					m_freem(nam2);
				}
				break;
			}
			nfsstats.srvrpccnt[nd->nd_procnum]++;
			if (nam2)
				nfsrv_updatecache(nam2, nd, TRUE, mreq);
			nd->nd_mrep = (struct mbuf *)0;
		case RC_REPLY:
			m = mreq;
			siz = 0;
			while (m) {
				siz += m->m_len;
				m = m->m_next;
			}
			if (siz <= 0 || siz > NFS_MAXPACKET) {
				printf("mbuf siz=%d\n",siz);
				panic("Bad nfs svc reply");
			}
			m = mreq;
			m->m_pkthdr.len = siz;
			m->m_pkthdr.rcvif = (struct ifnet *)0;
			/*
			 * For stream protocols, prepend a Sun RPC
			 * Record Mark.
			 */
			if (sotype == SOCK_STREAM) {
				M_PREPEND(m, NFSX_UNSIGNED, M_WAIT);
				*mtod(m, u_long *) = htonl(0x80000000 | siz);
			}
			if (solockp)
				(void) nfs_sndlock(solockp, (struct nfsreq *)0);
			if (slp->ns_flag & SLP_VALID)
			    error = nfs_send(so, nam2, m, (struct nfsreq *)0);
			else {
			    error = EPIPE;
			    m_freem(m);
			}
			if (nfsrtton)
				nfsd_rt(&starttime, sotype, nd, nam, cacherep);
			if (nam2)
				MFREE(nam2, m);
			if (nd->nd_mrep)
				m_freem(nd->nd_mrep);
			if (error == EPIPE)
				nfsrv_zapsock(slp);
			if (solockp)
				nfs_sndunlock(solockp);
			if (error == EINTR || error == ERESTART) {
				nfsrv_slpderef(slp);
				s = splnet();
				goto done;
			}
			break;
		case RC_DROPIT:
			if (nfsrtton)
				nfsd_rt(&starttime, sotype, nd, nam, cacherep);
			m_freem(nd->nd_mrep);
			m_freem(nam2);
			break;
		};
		s = splnet();
		if (nfsrv_dorec(slp, nd)) {
			nd->nd_flag &= ~NFSD_REQINPROG;
			nd->nd_slp = (struct nfssvc_sock *)0;
			nfsrv_slpderef(slp);
		}
	}
done:
	TAILQ_REMOVE(&nfsd_head, nd, nd_chain);
	splx(s);
	free((caddr_t)nd, M_NFSD);
	nsd->nsd_nfsd = (struct nfsd *)0;
	if (--nfs_numnfsd == 0)
		nfsrv_init(TRUE);	/* Reinitialize everything */
	return (error);
}
#endif /* NFS_SERVER */

#ifdef NFS_CLIENT
/*
 * Asynchronous I/O daemons for client nfs.
 * They do read-ahead and write-behind operations on the block I/O cache.
 * Never returns unless it fails or gets killed.
 */
int
nfssvc_iod(p)
	struct proc *p;
{
	register struct buf *bp;
	register int i, myiod;
	int error = 0;

	/*
	 * Assign my position or return error if too many already running
	 */
	myiod = -1;
	for (i = 0; i < NFS_MAXASYNCDAEMON; i++)
		if (nfs_asyncdaemon[i] == 0) {
			nfs_asyncdaemon[i]++;
			myiod = i;
			break;
		}
	if (myiod == -1)
		return (EBUSY);
	nfs_numasync++;
	/*
	 * Just loop around doin our stuff until SIGKILL
	 */
	for (;;) {
		while (nfs_bufq.tqh_first == NULL && error == 0) {
			nfs_iodwant[myiod] = p;
			error = tsleep((caddr_t)&nfs_iodwant[myiod],
				PWAIT | PCATCH, "nfsidl", 0);
		}
		while ((bp = nfs_bufq.tqh_first) != NULL) {
			/* Take one off the front of the list */
			TAILQ_REMOVE(&nfs_bufq, bp, b_freelist);
			if (bp->b_flags & B_READ)
			    (void) nfs_doio(bp, bp->b_rcred, (struct proc *)0);
			else
			    (void) nfs_doio(bp, bp->b_wcred, (struct proc *)0);
		}
		if (error) {
			nfs_asyncdaemon[myiod] = 0;
			nfs_numasync--;
			return (error);
		}
	}
}

#endif /* NFS_CLIENT */

#ifdef NFS_SERVER
/*
 * Shut down a socket associated with an nfssvc_sock structure.
 * Should be called with the send lock set, if required.
 * The trick here is to increment the sref at the start, so that the nfsds
 * will stop using it and clear ns_flag at the end so that it will not be
 * reassigned during cleanup.
 */
void
nfsrv_zapsock(slp)
	register struct nfssvc_sock *slp;
{
	register struct nfsuid *nuidp, *nnuidp;
	struct socket *so;
	struct file *fp;
	struct mbuf *m;

	slp->ns_flag &= ~SLP_ALLFLAGS;
	fp = slp->ns_fp;
	if (fp) {
		slp->ns_fp = (struct file *)0;
		so = slp->ns_so;
		so->so_upcall = NULL;
		soshutdown(so, 2);
		closef(fp, (struct proc *)0);
		if (slp->ns_nam)
			MFREE(slp->ns_nam, m);
		m_freem(slp->ns_raw);
		m_freem(slp->ns_rec);
		for (nuidp = slp->ns_uidlruhead.tqh_first; nuidp != 0;
		    nuidp = nnuidp) {
			nnuidp = nuidp->nu_lru.tqe_next;
			LIST_REMOVE(nuidp, nu_hash);
			TAILQ_REMOVE(&slp->ns_uidlruhead, nuidp, nu_lru);
			free((caddr_t)nuidp, M_NFSUID);
		}
	}
}

#endif /* NFS_SERVER */

/*
 * Get an authorization string for the uid by having the mount_nfs sitting
 * on this mount point porpous out of the kernel and do it.
 */
int
nfs_getauth(nmp, rep, cred, auth_type, auth_str, auth_len)
	register struct nfsmount *nmp;
	struct nfsreq *rep;
	struct ucred *cred;
	int *auth_type;
	char **auth_str;
	int *auth_len;
{
	int error = 0;

	while ((nmp->nm_flag & NFSMNT_WAITAUTH) == 0) {
		nmp->nm_flag |= NFSMNT_WANTAUTH;
		(void) tsleep((caddr_t)&nmp->nm_authtype, PSOCK,
			"nfsauth1", 2 * hz);
		error = nfs_sigintr(nmp, rep, rep->r_procp);
		if (error) {
			nmp->nm_flag &= ~NFSMNT_WANTAUTH;
			return (error);
		}
	}
	nmp->nm_flag &= ~(NFSMNT_WAITAUTH | NFSMNT_WANTAUTH);
	nmp->nm_authstr = *auth_str = (char *)malloc(RPCAUTH_MAXSIZ, M_TEMP, M_WAITOK);
	nmp->nm_authuid = cred->cr_uid;
	wakeup((caddr_t)&nmp->nm_authstr);

	/*
	 * And wait for mount_nfs to do its stuff.
	 */
	while ((nmp->nm_flag & NFSMNT_HASAUTH) == 0 && error == 0) {
		(void) tsleep((caddr_t)&nmp->nm_authlen, PSOCK,
			"nfsauth2", 2 * hz);
		error = nfs_sigintr(nmp, rep, rep->r_procp);
	}
	if (nmp->nm_flag & NFSMNT_AUTHERR) {
		nmp->nm_flag &= ~NFSMNT_AUTHERR;
		error = EAUTH;
	}
	if (error)
		free((caddr_t)*auth_str, M_TEMP);
	else {
		*auth_type = nmp->nm_authtype;
		*auth_len = nmp->nm_authlen;
	}
	nmp->nm_flag &= ~NFSMNT_HASAUTH;
	nmp->nm_flag |= NFSMNT_WAITAUTH;
	if (nmp->nm_flag & NFSMNT_WANTAUTH) {
		nmp->nm_flag &= ~NFSMNT_WANTAUTH;
		wakeup((caddr_t)&nmp->nm_authtype);
	}
	return (error);
}

#ifdef NFS_SERVER

/*
 * Derefence a server socket structure. If it has no more references and
 * is no longer valid, you can throw it away.
 */
void
nfsrv_slpderef(slp)
	register struct nfssvc_sock *slp;
{
	if (--(slp->ns_sref) == 0 && (slp->ns_flag & SLP_VALID) == 0) {
		TAILQ_REMOVE(&nfssvc_sockhead, slp, ns_chain);
		free((caddr_t)slp, M_NFSSVC);
	}
}

/*
 * Initialize the data structures for the server.
 * Handshake with any new nfsds starting up to avoid any chance of
 * corruption.
 */
void
nfsrv_init(terminating)
	int terminating;
{
	register struct nfssvc_sock *slp, *nslp;

	if (nfssvc_sockhead_flag & SLP_INIT)
		panic("nfsd init");
	nfssvc_sockhead_flag |= SLP_INIT;
	if (terminating) {
		for (slp = nfssvc_sockhead.tqh_first; slp != 0; slp = nslp) {
			nslp = slp->ns_chain.tqe_next;
			if (slp->ns_flag & SLP_VALID)
				nfsrv_zapsock(slp);
			TAILQ_REMOVE(&nfssvc_sockhead, slp, ns_chain);
			free((caddr_t)slp, M_NFSSVC);
		}
		nfsrv_cleancache();	/* And clear out server cache */
	}

	TAILQ_INIT(&nfssvc_sockhead);
	nfssvc_sockhead_flag &= ~SLP_INIT;
	if (nfssvc_sockhead_flag & SLP_WANTINIT) {
		nfssvc_sockhead_flag &= ~SLP_WANTINIT;
		wakeup((caddr_t)&nfssvc_sockhead);
	}

	TAILQ_INIT(&nfsd_head);
	nfsd_head_flag &= ~NFSD_CHECKSLP;

	nfs_udpsock = (struct nfssvc_sock *)
	    malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK);
	bzero((caddr_t)nfs_udpsock, sizeof (struct nfssvc_sock));
	nfs_udpsock->ns_uidhashtbl =
	    hashinit(NUIDHASHSIZ, M_NFSSVC, &nfs_udpsock->ns_uidhash);
	TAILQ_INIT(&nfs_udpsock->ns_uidlruhead);
	TAILQ_INSERT_HEAD(&nfssvc_sockhead, nfs_udpsock, ns_chain);

	nfs_cltpsock = (struct nfssvc_sock *)
	    malloc(sizeof (struct nfssvc_sock), M_NFSSVC, M_WAITOK);
	bzero((caddr_t)nfs_cltpsock, sizeof (struct nfssvc_sock));
	nfs_cltpsock->ns_uidhashtbl =
	    hashinit(NUIDHASHSIZ, M_NFSSVC, &nfs_cltpsock->ns_uidhash);
	TAILQ_INIT(&nfs_cltpsock->ns_uidlruhead);
	TAILQ_INSERT_TAIL(&nfssvc_sockhead, nfs_cltpsock, ns_chain);
}
#endif /* NFS_SERVER */

/*
 * Add entries to the server monitor log.
 */
static void
nfsd_rt(startp, sotype, nd, nam, cacherep)
	struct timeval *startp;
	int sotype;
	register struct nfsd *nd;
	struct mbuf *nam;
	int cacherep;
{
	register struct drt *rt;

	rt = &nfsdrt.drt[nfsdrt.pos];
	if (cacherep == RC_DOIT)
		rt->flag = 0;
	else if (cacherep == RC_REPLY)
		rt->flag = DRT_CACHEREPLY;
	else
		rt->flag = DRT_CACHEDROP;
	if (sotype == SOCK_STREAM)
		rt->flag |= DRT_TCP;
	if (nd->nd_nqlflag != NQL_NOVAL)
		rt->flag |= DRT_NQNFS;
	rt->proc = nd->nd_procnum;
	if (mtod(nam, struct sockaddr *)->sa_family == AF_INET)
		rt->ipadr = mtod(nam, struct sockaddr_in *)->sin_addr.s_addr;
	else
		rt->ipadr = INADDR_ANY;
	rt->resptime = ((time.tv_sec - startp->tv_sec) * 1000000) +
		(time.tv_usec - startp->tv_usec);
	rt->tstamp = time;
	nfsdrt.pos = (nfsdrt.pos + 1) % NFSRTTLOGSIZ;
}
