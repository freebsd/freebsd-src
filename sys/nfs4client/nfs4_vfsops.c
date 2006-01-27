/* $FreeBSD$ */
/* $Id: nfs_vfsops.c,v 1.38 2003/11/05 14:59:01 rees Exp $ */

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
 * Copyright (c) 1989, 1993, 1995
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
 *	@(#)nfs_vfsops.c	8.12 (Berkeley) 5/20/95
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_bootp.h"
#include "opt_nfsroot.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/signalvar.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>

#include <rpc/rpcclnt.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfs4client/nfs4.h>
#include <nfsclient/nfsnode.h>
#include <nfsclient/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfsclient/nfsm_subs.h>
#include <nfsclient/nfsdiskless.h>

#include <nfs4client/nfs4m_subs.h>
#include <nfs4client/nfs4_vfs.h>

#include <nfs4client/nfs4_dev.h>
#include <nfs4client/nfs4_idmap.h>

SYSCTL_NODE(_vfs, OID_AUTO, nfs4, CTLFLAG_RW, 0, "NFS4 filesystem");
SYSCTL_STRUCT(_vfs_nfs4, NFS_NFSSTATS, nfsstats, CTLFLAG_RD,
	&nfsstats, nfsstats, "S,nfsstats");

static void	nfs_decode_args(struct nfsmount *nmp, struct nfs_args *argp);
static void	nfs4_daemon(void *arg);
static int	mountnfs(struct nfs_args *, struct mount *,
		    struct sockaddr *, char *, struct vnode **,
		    struct ucred *cred);
static int	nfs4_do_setclientid(struct nfsmount *nmp, struct ucred *cred);
static vfs_mount_t nfs_mount;
static vfs_cmount_t nfs_cmount;
static vfs_unmount_t nfs_unmount;
static vfs_root_t nfs_root;
static vfs_statfs_t nfs_statfs;
static vfs_sync_t nfs_sync;

/*
 * nfs vfs operations.
 */
static struct vfsops nfs_vfsops = {
	.vfs_init =		nfs4_init,
	.vfs_mount =		nfs_mount,
	.vfs_cmount =		nfs_cmount,
	.vfs_root =		nfs_root,
	.vfs_statfs =		nfs_statfs,
	.vfs_sync =		nfs_sync,
	.vfs_uninit =		nfs4_uninit,
	.vfs_unmount =		nfs_unmount,
};
VFS_SET(nfs_vfsops, nfs4, VFCF_NETWORK);

static struct nfs_rpcops nfs4_rpcops = {
	nfs4_readrpc,
	nfs4_writerpc,
	nfs4_writebp,
	nfs4_readlinkrpc,
	nfs4_invaldir,
	nfs4_commit,
};

/* So that loader and kldload(2) can find us, wherever we are.. */
MODULE_VERSION(nfs4, 1);

void		nfsargs_ntoh(struct nfs_args *);

int
nfs4_init(struct vfsconf *vfsp)
{

	rpcclnt_init();
	nfs4dev_init();
	idmap_init();
	nfsm_v4init();

	return (0);
}

int
nfs4_uninit(struct vfsconf *vfsp)
{

	rpcclnt_uninit();
	nfs4dev_uninit();
	idmap_uninit();

	return (0);
}

/*
 * nfs statfs call
 */
static int
nfs_statfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
	struct vnode *vp;
	struct nfs_statfs *sfp;
	caddr_t bpos, dpos;
	struct nfsmount *nmp = VFSTONFS(mp);
	int error = 0;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	struct nfsnode *np;
	struct nfs4_compound cp;
	struct nfs4_oparg_getattr ga;
	struct nfsv4_fattr *fap = &ga.fa;

#ifndef nolint
	sfp = NULL;
#endif
	error = nfs_nget(mp, (nfsfh_t *)nmp->nm_fh, nmp->nm_fhsize, &np);
	if (error)
		return (error);
	vp = NFSTOV(np);
	nfsstats.rpccnt[NFSPROC_FSSTAT]++;
	mreq = nfsm_reqhead(vp, NFSV4PROC_COMPOUND, NFSX_FH(1));
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	ga.bm = &nfsv4_fsattrbm;
	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "statfs()");
	nfsm_v4build_putfh(&cp, vp);
	nfsm_v4build_getattr(&cp, &ga);
	nfsm_v4build_finalize(&cp);

	nfsm_request(vp, NFSV4PROC_COMPOUND, td, td->td_ucred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putfh(&cp);
	nfsm_v4dissect_getattr(&cp, &ga);

	nfs4_vfsop_statfs(fap, sbp, mp);

nfsmout:
	error = nfs_v4postop(&cp, error);

	vput(vp);
	if (mrep != NULL)
		m_freem(mrep);

	return (error);
}

static void
nfs_decode_args(struct nfsmount *nmp, struct nfs_args *argp)
{
	int s;
	int adjsock;
	int maxio;

	s = splnet();
	/*
	 * Silently clear NFSMNT_NOCONN if it's a TCP mount, it makes
	 * no sense in that context.
	 */
	if (argp->sotype == SOCK_STREAM)
		nmp->nm_flag &= ~NFSMNT_NOCONN;

	nmp->nm_flag &= ~NFSMNT_RDIRPLUS;

	/* Re-bind if rsrvd port requested and wasn't on one */
	adjsock = !(nmp->nm_flag & NFSMNT_RESVPORT)
		  && (argp->flags & NFSMNT_RESVPORT);
	/* Also re-bind if we're switching to/from a connected UDP socket */
	adjsock |= ((nmp->nm_flag & NFSMNT_NOCONN) !=
		    (argp->flags & NFSMNT_NOCONN));

	/* Update flags atomically.  Don't change the lock bits. */
	nmp->nm_flag = argp->flags | nmp->nm_flag;
	splx(s);

	if ((argp->flags & NFSMNT_TIMEO) && argp->timeo > 0) {
		nmp->nm_timeo = (argp->timeo * NFS_HZ + 5) / 10;
		if (nmp->nm_timeo < NFS_MINTIMEO)
			nmp->nm_timeo = NFS_MINTIMEO;
		else if (nmp->nm_timeo > NFS_MAXTIMEO)
			nmp->nm_timeo = NFS_MAXTIMEO;
	}

	if ((argp->flags & NFSMNT_RETRANS) && argp->retrans > 1) {
		nmp->nm_retry = argp->retrans;
		if (nmp->nm_retry > NFS_MAXREXMIT)
			nmp->nm_retry = NFS_MAXREXMIT;
	}

	if (argp->flags & NFSMNT_NFSV3) {
		if (argp->sotype == SOCK_DGRAM)
			maxio = NFS_MAXDGRAMDATA;
		else
			maxio = NFS_MAXDATA;
	} else
		maxio = NFS_V2MAXDATA;

	if ((argp->flags & NFSMNT_WSIZE) && argp->wsize > 0) {
		nmp->nm_wsize = argp->wsize;
		/* Round down to multiple of blocksize */
		nmp->nm_wsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_wsize <= 0)
			nmp->nm_wsize = NFS_FABLKSIZE;
	}
	if (nmp->nm_wsize > maxio)
		nmp->nm_wsize = maxio;
	if (nmp->nm_wsize > MAXBSIZE)
		nmp->nm_wsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_RSIZE) && argp->rsize > 0) {
		nmp->nm_rsize = argp->rsize;
		/* Round down to multiple of blocksize */
		nmp->nm_rsize &= ~(NFS_FABLKSIZE - 1);
		if (nmp->nm_rsize <= 0)
			nmp->nm_rsize = NFS_FABLKSIZE;
	}
	if (nmp->nm_rsize > maxio)
		nmp->nm_rsize = maxio;
	if (nmp->nm_rsize > MAXBSIZE)
		nmp->nm_rsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_READDIRSIZE) && argp->readdirsize > 0) {
		nmp->nm_readdirsize = argp->readdirsize;
	}
	if (nmp->nm_readdirsize > maxio)
		nmp->nm_readdirsize = maxio;
	if (nmp->nm_readdirsize > nmp->nm_rsize)
		nmp->nm_readdirsize = nmp->nm_rsize;

	if ((argp->flags & NFSMNT_ACREGMIN) && argp->acregmin >= 0)
		nmp->nm_acregmin = argp->acregmin;
	else
		nmp->nm_acregmin = NFS_MINATTRTIMO;
	if ((argp->flags & NFSMNT_ACREGMAX) && argp->acregmax >= 0)
		nmp->nm_acregmax = argp->acregmax;
	else
		nmp->nm_acregmax = NFS_MAXATTRTIMO;
	if ((argp->flags & NFSMNT_ACDIRMIN) && argp->acdirmin >= 0)
		nmp->nm_acdirmin = argp->acdirmin;
	else
		nmp->nm_acdirmin = NFS_MINDIRATTRTIMO;
	if ((argp->flags & NFSMNT_ACDIRMAX) && argp->acdirmax >= 0)
		nmp->nm_acdirmax = argp->acdirmax;
	else
		nmp->nm_acdirmax = NFS_MAXDIRATTRTIMO;
	if (nmp->nm_acdirmin > nmp->nm_acdirmax)
		nmp->nm_acdirmin = nmp->nm_acdirmax;
	if (nmp->nm_acregmin > nmp->nm_acregmax)
		nmp->nm_acregmin = nmp->nm_acregmax;

	if ((argp->flags & NFSMNT_MAXGRPS) && argp->maxgrouplist >= 0) {
		if (argp->maxgrouplist <= NFS_MAXGRPS)
			nmp->nm_numgrps = argp->maxgrouplist;
		else
			nmp->nm_numgrps = NFS_MAXGRPS;
	}
	if ((argp->flags & NFSMNT_READAHEAD) && argp->readahead >= 0) {
		if (argp->readahead <= NFS_MAXRAHEAD)
			nmp->nm_readahead = argp->readahead;
		else
			nmp->nm_readahead = NFS_MAXRAHEAD;
	}
	if ((argp->flags & NFSMNT_DEADTHRESH) && argp->deadthresh >= 0) {
		if (argp->deadthresh <= NFS_MAXDEADTHRESH)
			nmp->nm_deadthresh = argp->deadthresh;
		else
			nmp->nm_deadthresh = NFS_MAXDEADTHRESH;
	}

	adjsock |= ((nmp->nm_sotype != argp->sotype) ||
		    (nmp->nm_soproto != argp->proto));
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;

	if (nmp->nm_rpcclnt.rc_so && adjsock) {
		nfs_safedisconnect(nmp);
		if (nmp->nm_sotype == SOCK_DGRAM) {
			while (nfs4_connect(nmp)) {
				printf("nfs_args: retrying connect\n");
				(void) tsleep((caddr_t)&lbolt,
					      PSOCK, "nfscon", 0);
			}
		}
	}
}

/*
 * VFS Operations.
 *
 * mount system call
 * It seems a bit dumb to copyinstr() the host and path here and then
 * bcopy() them in mountnfs(), but I wanted to detect errors before
 * doing the sockargs() call because sockargs() allocates an mbuf and
 * an error after that means that I have to release the mbuf.
 */
/* ARGSUSED */
static int
nfs_cmount(struct mntarg *ma, void *data, int flags, struct thread *td)
{
	struct nfs_args args;
	int error;

	error = copyin(data, (caddr_t)&args, sizeof (struct nfs_args));
	if (error)
		return (error);

	ma = mount_arg(ma, "nfs_args", &args, sizeof args);

	error = kernel_mount(ma, flags);

	 return (error);
}

static int
nfs_mount(struct mount *mp, struct thread *td)
{
	int error;
	struct nfs_args args;
	struct sockaddr *nam;
	struct vnode *vp;
	char hst[MNAMELEN];
	size_t len;

	if (mp->mnt_flag & MNT_ROOTFS) {
		printf("NFSv4: nfs_mountroot not supported\n");
		return EINVAL;
	}
	error = vfs_copyopt(mp->mnt_optnew, "nfs_args", &args, sizeof args);
	if (error)
		return (error);

	if (args.version != NFS_ARGSVERSION)
		return (EPROGMISMATCH);
	if (mp->mnt_flag & MNT_UPDATE) {
		struct nfsmount *nmp = VFSTONFS(mp);

		if (nmp == NULL)
			return (EIO);
		/*
		 * When doing an update, we can't change from or to
		 * v3, switch lockd strategies or change cookie translation
		 */
		args.flags = (args.flags &
		    ~(NFSMNT_NFSV3 | NFSMNT_NFSV4 | NFSMNT_NOLOCKD)) |
		    (nmp->nm_flag &
			(NFSMNT_NFSV3 | NFSMNT_NFSV4 | NFSMNT_NOLOCKD));
		nfs_decode_args(nmp, &args);
		return (0);
	}

	error = copyinstr(args.hostname, hst, MNAMELEN-1, &len);
	if (error)
		return (error);
	bzero(&hst[len], MNAMELEN - len);
	/* sockargs() call must be after above copyin() calls */
	error = getsockaddr(&nam, (caddr_t)args.addr, args.addrlen);
	if (error)
		return (error);
	error = mountnfs(&args, mp, nam, hst, &vp, td->td_ucred);
	return (error);
}

/*
 * renew should be done async
 * should re-scan mount queue each time
 */
struct proc *nfs4_daemonproc;

static int
nfs4_do_renew(struct nfsmount *nmp, struct ucred *cred)
{
	struct nfs4_compound cp;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	caddr_t bpos, dpos;	
	int error;

	mreq = nfsm_reqhead(NULL, NFSV4PROC_COMPOUND, sizeof(uint64_t));
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_do_renew()");
	nfsm_v4build_renew(&cp, nmp->nm_clientid);
	nfsm_v4build_finalize(&cp);

	nfsm_request_mnt(nmp, NFSV4PROC_COMPOUND, curthread, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_renew(&cp);
	nmp->nm_last_renewal = time_second;
	return (0);

 nfsmout:
	error = nfs_v4postop(&cp, error);

	/* XXX */
	if (mrep != NULL)
		m_freem(mrep);
	return (error);
}

static void
nfs4_daemon(void *arg)
{
	struct mount *mp;
	struct nfsmount *nmp;
	int nmounts;

	while (1) {
		nmounts = 0;
		mtx_lock(&mountlist_mtx);
		TAILQ_FOREACH(mp, &mountlist, mnt_list) {
			if (strcmp(mp->mnt_vfc->vfc_name, "nfs4") != 0)
				continue;
			nmounts++;
			nmp = VFSTONFS(mp);
			if (time_second < nmp->nm_last_renewal + nmp->nm_lease_time - 4)
				continue;
			mtx_unlock(&mountlist_mtx);
			mtx_lock(&Giant);
			nfs4_do_renew(nmp, (struct ucred *) arg);
			mtx_unlock(&Giant);
			mtx_lock(&mountlist_mtx);
		}
		mtx_unlock(&mountlist_mtx);

		/* Must kill the daemon here, or module unload will cause a panic */
		if (nmounts == 0) {
			mtx_lock(&Giant);
			nfs4_daemonproc = NULL;
			mtx_unlock(&Giant);
			/*printf("nfsv4 renewd exiting\n");*/
			kthread_exit(0);
		}
		tsleep(&nfs4_daemonproc, PVFS, "nfs4", 2 * hz);
	}
}

/*
 * Common code for mount and mountroot
 */
static int
mountnfs(struct nfs_args *argp, struct mount *mp, struct sockaddr *nam,
    char *hst, struct vnode **vpp, struct ucred *cred)
{
	struct nfsmount *nmp;
	char *rpth, *cp1, *cp2;
	int nlkup = 0, error;
	struct nfs4_compound cp;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	caddr_t bpos, dpos;	
	struct nfs4_oparg_lookup lkup;
	struct nfs4_oparg_getfh gfh;
	struct nfs4_oparg_getattr ga;
	struct thread *td = curthread; /* XXX */

	if (mp->mnt_flag & MNT_UPDATE) {
		nmp = VFSTONFS(mp);
		/* update paths, file handles, etc, here	XXX */
		FREE(nam, M_SONAME);
		return (0);
	} else {
		nmp = uma_zalloc(nfsmount_zone, M_WAITOK);
		bzero((caddr_t)nmp, sizeof (struct nfsmount));
		TAILQ_INIT(&nmp->nm_bufq);
		mp->mnt_data = (qaddr_t)nmp;
	}

	vfs_getnewfsid(mp);
	nmp->nm_mountp = mp;
	nmp->nm_maxfilesize = 0xffffffffLL;
	nmp->nm_timeo = NFS_TIMEO;
	nmp->nm_retry = NFS_RETRANS;
	nmp->nm_wsize = NFS_WSIZE;
	nmp->nm_rsize = NFS_RSIZE;
	nmp->nm_readdirsize = NFS_READDIRSIZE;
	nmp->nm_numgrps = NFS_MAXGRPS;
	nmp->nm_readahead = NFS_DEFRAHEAD;
	nmp->nm_deadthresh = NFS_MAXDEADTHRESH;
	vfs_mountedfrom(mp, hst);
	nmp->nm_nam = nam;
	/* Set up the sockets and per-host congestion */
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;
	nmp->nm_rpcops = &nfs4_rpcops;
	/* XXX */
        mp->mnt_stat.f_iosize = PAGE_SIZE;

	argp->flags |= (NFSMNT_NFSV3 | NFSMNT_NFSV4);

	nfs_decode_args(nmp, argp);

	if ((error = nfs4_connect(nmp)))
		goto bad;

	mreq = nfsm_reqhead(NULL, NFSV4PROC_COMPOUND, NFSX_FH(1));
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	ga.bm = &nfsv4_fsinfobm;
	nfs_v4initcompound(&cp);

	/* Get remote path */
	rpth = hst;
	strsep(&rpth, ":");

	nfsm_v4build_compound(&cp, "mountnfs()");
	nfsm_v4build_putrootfh(&cp);
	for (cp1 = rpth; cp1 && *cp1; cp1 = cp2)  {
		while (*cp1 == '/')
			cp1++;
		if (!*cp1)
			break;
		for (cp2 = cp1; *cp2 && *cp2 != '/'; cp2++)
			;
		lkup.name = cp1;
		lkup.namelen = cp2 - cp1;
		nfsm_v4build_lookup(&cp, &lkup);
		nlkup++;
	}
	nfsm_v4build_getfh(&cp, &gfh);
	nfsm_v4build_getattr(&cp, &ga);
	nfsm_v4build_finalize(&cp);

	nfsm_request_mnt(nmp, NFSV4PROC_COMPOUND, td, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_putrootfh(&cp);
	while (nlkup--)
		nfsm_v4dissect_lookup(&cp);
	nfsm_v4dissect_getfh(&cp, &gfh);
	nfsm_v4dissect_getattr(&cp, &ga);

	nfs4_vfsop_fsinfo(&ga.fa, nmp);
	nmp->nm_state |= NFSSTA_GOTFSINFO;

	/* Copy root fh into nfsmount. */
	nmp->nm_fhsize = gfh.fh_len;
	bcopy(&gfh.fh_val, nmp->nm_fh, nmp->nm_fhsize);
	nmp->nm_last_renewal = time_second;

	if ((error = nfs4_do_setclientid(nmp, cred)) != 0)
		goto nfsmout;

	/* Start renewd if it isn't already running */
	if (nfs4_daemonproc == NULL)
		kthread_create(nfs4_daemon, crdup(cred), &nfs4_daemonproc,
			       (RFPROC|RFMEM), 0, "nfs4rd");

	return (0);
 nfsmout:
	error = nfs_v4postop(&cp, error);

	/* XXX */
	if (mrep != NULL)
		m_freem(mrep);
bad:
	nfs4_disconnect(nmp);
	uma_zfree(nfsmount_zone, nmp);
	FREE(nam, M_SONAME);

	return (error);
}

/*
 * unmount system call
 */
static int
nfs_unmount(struct mount *mp, int mntflags, struct thread *td)
{
	struct nfsmount *nmp;
	int error, flags = 0;

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;
	nmp = VFSTONFS(mp);
	/*
	 * Goes something like this..
	 * - Call vflush(, td) to clear out vnodes for this filesystem
	 * - Close the socket
	 * - Free up the data structures
	 */
	/* In the forced case, cancel any outstanding requests. */
	if (flags & FORCECLOSE) {
		error = nfs_nmcancelreqs(nmp);
		if (error)
			return (error);
		nfs4dev_purge();
	}

	error = vflush(mp, 0, flags, td);
	if (error)
		return (error);

	/*
	 * We are now committed to the unmount.
	 */
	nfs4_disconnect(nmp);
	FREE(nmp->nm_nam, M_SONAME);

	/* XXX there's a race condition here for SMP */
	wakeup(&nfs4_daemonproc);

	uma_zfree(nfsmount_zone, nmp);
	return (0);
}

/*
 * Return root of a filesystem
 */
static int
nfs_root(struct mount *mp, int flags, struct vnode **vpp, struct thread *td)
{
	struct vnode *vp;
	struct nfsmount *nmp;
	struct nfsnode *np;
	int error;

	nmp = VFSTONFS(mp);
	error = nfs_nget(mp, (nfsfh_t *)nmp->nm_fh, nmp->nm_fhsize, &np);
	if (error)
		return (error);
	vp = NFSTOV(np);
	if (vp->v_type == VNON)
	    vp->v_type = VDIR;
	vp->v_vflag |= VV_ROOT;
	*vpp = vp;

	return (0);
}

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
static int
nfs_sync(struct mount *mp, int waitfor, struct thread *td)
{
	struct vnode *vp, *mvp;
	int error, allerror = 0;

	/*
	 * Force stale buffer cache information to be flushed.
	 */
	MNT_ILOCK(mp);
loop:
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		VI_LOCK(vp);
		MNT_IUNLOCK(mp);
		if (VOP_ISLOCKED(vp, NULL) ||
		    vp->v_bufobj.bo_dirty.bv_cnt == 0 ||
		    waitfor == MNT_LAZY) {
			VI_UNLOCK(vp);
			MNT_ILOCK(mp);
			continue;
		}
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, td)) {
			MNT_ILOCK(mp);
			MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
			goto loop;
		}
		error = VOP_FSYNC(vp, waitfor, td);
		if (error)
			allerror = error;
		VOP_UNLOCK(vp, 0, td);
		vrele(vp);

		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	return (allerror);
}

static int
nfs4_do_setclientid(struct nfsmount *nmp, struct ucred *cred)
{
	struct nfs4_oparg_setclientid scid;
	struct nfs4_compound cp;
	struct mbuf *mreq, *mrep = NULL, *md, *mb;
	caddr_t bpos, dpos;	
	struct route ro;
	char *ipsrc = NULL, uaddr[24], name[24];
	int try = 0;
	static unsigned long seq;
	int error;

#ifndef NFS4_USE_RPCCLNT
	return (0);
#endif
	if (nmp->nm_clientid) {
		printf("nfs4_do_setclientid: already have clientid!\n");
		error = 0;
		goto nfsmout;
	}

	/* Try not to re-use clientids */
	if (seq == 0)
		seq = time_second;

#ifdef NFS4_USE_RPCCLNT
	scid.cb_netid = (nmp->nm_rpcclnt.rc_sotype == SOCK_STREAM) ? "tcp" : "udp";
#endif
	scid.cb_netid = "tcp";
	scid.cb_netidlen = 3;
	scid.cb_prog = 0x1234; /* XXX */

	/* Do a route lookup to find our source address for talking to this server */
	bzero(&ro, sizeof ro);

#ifdef NFS4_USE_RPCCLNT
	ro.ro_dst = *nmp->nm_rpcclnt.rc_name;
#endif
	rtalloc(&ro);
	if (ro.ro_rt == NULL) {
		error = EHOSTUNREACH;
		goto nfsmout;
	}
	ipsrc = inet_ntoa(IA_SIN(ifatoia(ro.ro_rt->rt_ifa))->sin_addr);
	sprintf(uaddr, "%s.12.48", ipsrc);
	scid.cb_univaddr = uaddr;
	scid.cb_univaddrlen = strlen(uaddr);
	RTFREE(ro.ro_rt);

 try_again:
	sprintf(name, "%s-%d", ipsrc, (int) ((seq + try) % 1000000L));
	scid.namelen = strlen(name);
	scid.name = name;
	nfs_v4initcompound(&cp);

	mreq = nfsm_reqhead(NULL, NFSV4PROC_COMPOUND, NFSX_FH(1));
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfsm_v4build_compound(&cp, "nfs4_do_setclientid()");
	nfsm_v4build_setclientid(&cp, &scid);
	nfsm_v4build_finalize(&cp);

	nfsm_request_mnt(nmp, NFSV4PROC_COMPOUND, curthread, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_setclientid(&cp, &scid);
	nmp->nm_clientid = scid.clientid;

	error = nfs_v4postop(&cp, error);

	/* Confirm */
	m_freem(mrep);
	mreq = nfsm_reqhead(NULL, NFSV4PROC_COMPOUND, NFSX_FH(1));
	mb = mreq;
	bpos = mtod(mb, caddr_t);

	nfs_v4initcompound(&cp);

	nfsm_v4build_compound(&cp, "nfs4_do_setclientid() (confirm)");
	nfsm_v4build_setclientid_confirm(&cp, &scid);
	nfsm_v4build_finalize(&cp);

	nfsm_request_mnt(nmp, NFSV4PROC_COMPOUND, curthread, cred);
	if (error != 0)
		goto nfsmout;

	nfsm_v4dissect_compound(&cp);
	nfsm_v4dissect_setclientid_confirm(&cp);

 nfsmout:
	error = nfs_v4postop(&cp, error);

	if (mrep)
		m_freem(mrep);
	if (error == NFSERR_CLID_INUSE && (++try < NFS4_SETCLIENTID_MAXTRIES))
		goto try_again;

	return (error);
}
