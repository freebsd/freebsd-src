/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	From:	@(#)nfs_syscalls.c	7.26 (Berkeley) 4/16/91
 *	$Id: nfs_syscalls.c,v 1.3 1993/09/09 22:06:09 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "kernel.h"
#include "file.h"
#include "stat.h"
#include "namei.h"
#include "vnode.h"
#include "mount.h"
#include "proc.h"
#include "malloc.h"
#include "buf.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "domain.h"
#include "protosw.h"

#include "../netinet/in.h"
#include "../netinet/tcp.h"

#include "nfsv2.h"
#include "nfs.h"
#include "nfsrvcache.h"

/* Global defs. */
extern u_long nfs_prog, nfs_vers;
extern int (*nfsrv_procs[NFS_NPROCS])();
extern struct buf nfs_bqueue;
extern int nfs_numasync;
extern struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
extern int nfs_tcpnodelay;
struct mbuf *nfs_compress();

#define	TRUE	1
#define	FALSE	0

static int nfs_asyncdaemon[NFS_MAXASYNCDAEMON];
static int compressreply[NFS_NPROCS] = {
	FALSE,
	TRUE,
	TRUE,
	FALSE,
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
	TRUE,
};
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

/* ARGSUSED */
int
getfh(p, uap, retval)
	struct proc *p;
	register struct getfh_args *uap;
	int *retval;
{
	register struct nameidata *ndp;
	register struct vnode *vp;
	fhandle_t fh;
	int error;
	struct nameidata nd;

	/*
	 * Must be super user
	 */
	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	ndp = &nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;
	if (error = namei(ndp, p))
		return (error);
	vp = ndp->ni_vp;
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
 * Never returns unless it fails or gets killed
 */

struct nfssvc_args {
	int s;
	caddr_t mskval;
	int msklen;
	caddr_t mtchval;
	int mtchlen;
};

/* ARGSUSED */
int
nfssvc(p, uap, retval)
	struct proc *p;
	register struct nfssvc_args *uap;
	int *retval;
{
	register struct mbuf *m;
	register int siz;
	register struct ucred *cr;
	struct file *fp;
	struct mbuf *mreq, *mrep, *nam, *md;
	struct mbuf msk, mtch;
	struct socket *so;
	caddr_t dpos;
	int procid, repstat, error, cacherep, wascomp;
	u_long retxid;

	/*
	 * Must be super user
	 */
	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
	if (error = getsock(p->p_fd, uap->s, &fp))
		return (error);
	so = (struct socket *)fp->f_data;
	if (sosendallatonce(so))
		siz = NFS_MAXPACKET;
	else
		siz = NFS_MAXPACKET + sizeof(u_long);
	if (error = soreserve(so, siz, siz))
		goto bad;
	if (error = sockargs(&nam, uap->mskval, uap->msklen, MT_SONAME))
		goto bad;
	bcopy((caddr_t)nam, (caddr_t)&msk, sizeof (struct mbuf));
	msk.m_data = msk.m_dat;
	m_freem(nam);
	if (error = sockargs(&nam, uap->mtchval, uap->mtchlen, MT_SONAME))
		goto bad;
	bcopy((caddr_t)nam, (caddr_t)&mtch, sizeof (struct mbuf));
	mtch.m_data = mtch.m_dat;
	m_freem(nam);

	/* Copy the cred so others don't see changes */
	cr = p->p_ucred = crcopy(p->p_ucred);

	/*
	 * Set protocol specific options { for now TCP only } and
	 * reserve some space. For datagram sockets, this can get called
	 * repeatedly for the same socket, but that isn't harmful.
	 */
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
	so->so_rcv.sb_flags &= ~SB_NOINTR;
	so->so_rcv.sb_timeo = 0;
	so->so_snd.sb_flags &= ~SB_NOINTR;
	so->so_snd.sb_timeo = 0;

	/*
	 * Just loop around doin our stuff until SIGKILL
	 */
	for (;;) {
		if (error = nfs_getreq(so, nfs_prog, nfs_vers, NFS_NPROCS-1,
		   &nam, &mrep, &md, &dpos, &retxid, &procid, cr,
/* 08 Sep 92*/	   &msk, &mtch, &wascomp, &repstat)) {
			if (nam)
				m_freem(nam);
			if (error == EPIPE || error == EINTR ||
			    error == ERESTART) {
				error = 0;
				goto bad;
			}
			so->so_error = 0;
			continue;
		}

		if (nam)
			cacherep = nfsrv_getcache(nam, retxid, procid, &mreq);
		else
			cacherep = RC_DOIT;
		switch (cacherep) {
		case RC_DOIT:
			if (error = (*(nfsrv_procs[procid]))(mrep, md, dpos,
				cr, retxid, &mreq, &repstat, p)) {
				nfsstats.srv_errs++;
				if (nam) {
					nfsrv_updatecache(nam, retxid, procid,
						FALSE, repstat, mreq);
					m_freem(nam);
				}
				break;
			}
			nfsstats.srvrpccnt[procid]++;
			if (nam)
				nfsrv_updatecache(nam, retxid, procid, TRUE,
					repstat, mreq);
			mrep = (struct mbuf *)0;
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
			mreq->m_pkthdr.len = siz;
			mreq->m_pkthdr.rcvif = (struct ifnet *)0;
			if (wascomp && compressreply[procid]) {
				mreq = nfs_compress(mreq);
				siz = mreq->m_pkthdr.len;
			}
			/*
			 * For non-atomic protocols, prepend a Sun RPC
			 * Record Mark.
			 */
			if (!sosendallatonce(so)) {
				M_PREPEND(mreq, sizeof(u_long), M_WAIT);
				*mtod(mreq, u_long *) = htonl(0x80000000 | siz);
			}
			error = nfs_send(so, nam, mreq, (struct nfsreq *)0);
			if (nam)
				m_freem(nam);
			if (mrep)
				m_freem(mrep);
			if (error) {
				if (error == EPIPE || error == EINTR ||
				    error == ERESTART)
					goto bad;
				so->so_error = 0;
			}
			break;
		case RC_DROPIT:
			m_freem(mrep);
			m_freem(nam);
			break;
		};
	}
bad:
	return (error);
}

/*
 * Nfs pseudo system call for asynchronous i/o daemons.
 * These babies just pretend to be disk interrupt service routines
 * for client nfs. They are mainly here for read ahead/write behind.
 * Never returns unless it fails or gets killed
 */
/* ARGSUSED */
int
async_daemon(p, uap, retval)
	struct proc *p;
	struct args *uap;
	int *retval;
{
	register struct buf *bp, *dp;
	register int i, myiod;
	int error;

	/*
	 * Must be super user
	 */
	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
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
	dp = &nfs_bqueue;
	/*
	 * Just loop around doin our stuff until SIGKILL
	 */
	for (;;) {
		while (dp->b_actf == NULL && error == 0) {
			nfs_iodwant[myiod] = p;
			error = tsleep((caddr_t)&nfs_iodwant[myiod],
				PWAIT | PCATCH, "nfsidl", 0);
			nfs_iodwant[myiod] = (struct proc *)0;
		}
		while (dp->b_actf != NULL) {
			/* Take one off the end of the list */
			bp = dp->b_actl;
			if (bp->b_actl == dp) {
				dp->b_actf = dp->b_actl = (struct buf *)0;
			} else {
				dp->b_actl = bp->b_actl;
				bp->b_actl->b_actf = dp;
			}
			(void) nfs_doio(bp);
		}
		if (error) {
			nfs_asyncdaemon[myiod] = 0;
			nfs_numasync--;
			return (error);
		}
	}
}
