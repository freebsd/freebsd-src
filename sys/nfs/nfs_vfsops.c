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
 *	@(#)nfs_vfsops.c	8.3 (Berkeley) 1/4/94
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/nfs.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfsdiskless.h>
#include <nfs/nqnfs.h>

/*
 * nfs vfs operations.
 */
struct vfsops nfs_vfsops = {
	nfs_mount,
	nfs_start,
	nfs_unmount,
	nfs_root,
	nfs_quotactl,
	nfs_statfs,
	nfs_sync,
	nfs_vget,
	nfs_fhtovp,
	nfs_vptofh,
	nfs_init,
};

/*
 * This structure must be filled in by a primary bootstrap or bootstrap
 * server for a diskless/dataless machine. It is initialized below just
 * to ensure that it is allocated to initialized data (.data not .bss).
 */
struct nfs_diskless nfs_diskless = { 0 };

extern u_long nfs_procids[NFS_NPROCS];
extern u_long nfs_prog, nfs_vers;
void nfs_disconnect __P((struct nfsmount *));
void nfsargs_ntoh __P((struct nfs_args *));
static struct mount *nfs_mountdiskless __P((char *, char *, int,
    struct sockaddr_in *, struct nfs_args *, register struct vnode **));

#define TRUE	1
#define	FALSE	0

/*
 * nfs statfs call
 */
int
nfs_statfs(mp, sbp, p)
	struct mount *mp;
	register struct statfs *sbp;
	struct proc *p;
{
	register struct vnode *vp;
	register struct nfsv2_statfs *sfp;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	int error = 0, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	struct ucred *cred;
	struct nfsnode *np;

	nmp = VFSTONFS(mp);
	isnq = (nmp->nm_flag & NFSMNT_NQNFS);
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		return (error);
	vp = NFSTOV(np);
	nfsstats.rpccnt[NFSPROC_STATFS]++;
	cred = crget();
	cred->cr_ngroups = 1;
	nfsm_reqhead(vp, NFSPROC_STATFS, NFSX_FH);
	nfsm_fhtom(vp);
	nfsm_request(vp, NFSPROC_STATFS, p, cred);
	nfsm_dissect(sfp, struct nfsv2_statfs *, NFSX_STATFS(isnq));
	sbp->f_type = MOUNT_NFS;
	sbp->f_flags = nmp->nm_flag;
	sbp->f_iosize = NFS_MAXDGRAMDATA;
	sbp->f_bsize = fxdr_unsigned(long, sfp->sf_bsize);
	sbp->f_blocks = fxdr_unsigned(long, sfp->sf_blocks);
	sbp->f_bfree = fxdr_unsigned(long, sfp->sf_bfree);
	sbp->f_bavail = fxdr_unsigned(long, sfp->sf_bavail);
	if (isnq) {
		sbp->f_files = fxdr_unsigned(long, sfp->sf_files);
		sbp->f_ffree = fxdr_unsigned(long, sfp->sf_ffree);
	} else {
		sbp->f_files = 0;
		sbp->f_ffree = 0;
	}
	if (sbp != &mp->mnt_stat) {
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	nfsm_reqdone;
	vrele(vp);
	crfree(cred);
	return (error);
}

/*
 * Mount a remote root fs via. nfs. This depends on the info in the
 * nfs_diskless structure that has been filled in properly by some primary
 * bootstrap.
 * It goes something like this:
 * - do enough of "ifconfig" by calling ifioctl() so that the system
 *   can talk to the server
 * - If nfs_diskless.mygateway is filled in, use that address as
 *   a default gateway.
 * - hand craft the swap nfs vnode hanging off a fake mount point
 *	if swdevt[0].sw_dev == NODEV
 * - build the rootfs mount point and call mountnfs() to do the rest.
 */
int
nfs_mountroot()
{
	register struct mount *mp;
	register struct nfs_diskless *nd = &nfs_diskless;
	struct socket *so;
	struct vnode *vp;
	struct proc *p = curproc;		/* XXX */
	int error, i;

	/*
	 * XXX time must be non-zero when we init the interface or else
	 * the arp code will wedge...
	 */
	if (time.tv_sec == 0)
		time.tv_sec = 1;

#ifdef notyet
	/* Set up swap credentials. */
	proc0.p_ucred->cr_uid = ntohl(nd->swap_ucred.cr_uid);
	proc0.p_ucred->cr_gid = ntohl(nd->swap_ucred.cr_gid);
	if ((proc0.p_ucred->cr_ngroups = ntohs(nd->swap_ucred.cr_ngroups)) >
		NGROUPS)
		proc0.p_ucred->cr_ngroups = NGROUPS;
	for (i = 0; i < proc0.p_ucred->cr_ngroups; i++)
	    proc0.p_ucred->cr_groups[i] = ntohl(nd->swap_ucred.cr_groups[i]);
#endif

	/*
	 * Do enough of ifconfig(8) so that the critical net interface can
	 * talk to the server.
	 */
	if (error = socreate(nd->myif.ifra_addr.sa_family, &so, SOCK_DGRAM, 0))
		panic("nfs_mountroot: socreate: %d", error);
	if (error = ifioctl(so, SIOCAIFADDR, (caddr_t)&nd->myif, p))
		panic("nfs_mountroot: SIOCAIFADDR: %d", error);
	soclose(so);

	/*
	 * If the gateway field is filled in, set it as the default route.
	 */
	if (nd->mygateway.sin_len != 0) {
		struct sockaddr_in mask, sin;

		bzero((caddr_t)&mask, sizeof(mask));
		sin = mask;
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof(sin);
		if (error = rtrequest(RTM_ADD, (struct sockaddr *)&sin,
		    (struct sockaddr *)&nd->mygateway,
		    (struct sockaddr *)&mask,
		    RTF_UP | RTF_GATEWAY, (struct rtentry **)0))
			panic("nfs_mountroot: RTM_ADD: %d", error);
	}

	/*
	 * If swapping to an nfs node (indicated by swdevt[0].sw_dev == NODEV):
	 * Create a fake mount point just for the swap vnode so that the
	 * swap file can be on a different server from the rootfs.
	 */
	if (swdevt[0].sw_dev == NODEV) {
		nd->swap_args.fh = (nfsv2fh_t *)nd->swap_fh;
		(void) nfs_mountdiskless(nd->swap_hostnam, "/swap", 0,
		    &nd->swap_saddr, &nd->swap_args, &vp);
	
		/*
		 * Since the swap file is not the root dir of a file system,
		 * hack it to a regular file.
		 */
		vp->v_type = VREG;
		vp->v_flag = 0;
		swapdev_vp = vp;
		VREF(vp);
		swdevt[0].sw_vp = vp;
		swdevt[0].sw_nblks = ntohl(nd->swap_nblks);
	} else if (bdevvp(swapdev, &swapdev_vp))
		panic("nfs_mountroot: can't setup swapdev_vp");

	/*
	 * Create the rootfs mount point.
	 */
	nd->root_args.fh = (nfsv2fh_t *)nd->root_fh;
	mp = nfs_mountdiskless(nd->root_hostnam, "/", MNT_RDONLY,
	    &nd->root_saddr, &nd->root_args, &vp);

	if (vfs_lock(mp))
		panic("nfs_mountroot: vfs_lock");
	TAILQ_INSERT_TAIL(&mountlist, mp, mnt_list);
	mp->mnt_flag |= MNT_ROOTFS;
	mp->mnt_vnodecovered = NULLVP;
	vfs_unlock(mp);
	rootvp = vp;

	/*
	 * This is not really an nfs issue, but it is much easier to
	 * set hostname here and then let the "/etc/rc.xxx" files
	 * mount the right /var based upon its preset value.
	 */
	bcopy(nd->my_hostnam, hostname, MAXHOSTNAMELEN);
	hostname[MAXHOSTNAMELEN - 1] = '\0';
	for (i = 0; i < MAXHOSTNAMELEN; i++)
		if (hostname[i] == '\0')
			break;
	hostnamelen = i;
	inittodr(ntohl(nd->root_time));
	return (0);
}

/*
 * Internal version of mount system call for diskless setup.
 */
static struct mount *
nfs_mountdiskless(path, which, mountflag, sin, args, vpp)
	char *path;
	char *which;
	int mountflag;
	struct sockaddr_in *sin;
	struct nfs_args *args;
	register struct vnode **vpp;
{
	register struct mount *mp;
	register struct mbuf *m;
	register int error;

	mp = (struct mount *)malloc((u_long)sizeof(struct mount),
	    M_MOUNT, M_NOWAIT);
	if (mp == NULL)
		panic("nfs_mountroot: %s mount malloc", which);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	mp->mnt_op = &nfs_vfsops;
	mp->mnt_flag = mountflag;

	MGET(m, MT_SONAME, M_DONTWAIT);
	if (m == NULL)
		panic("nfs_mountroot: %s mount mbuf", which);
	bcopy((caddr_t)sin, mtod(m, caddr_t), sin->sin_len);
	m->m_len = sin->sin_len;
	nfsargs_ntoh(args);
	if (error = mountnfs(args, mp, m, which, path, vpp))
		panic("nfs_mountroot: mount %s on %s: %d", path, which, error);

	return (mp);
}

/*
 * Convert the integer fields of the nfs_args structure from net byte order
 * to host byte order. Called by nfs_mountroot() above.
 */
void
nfsargs_ntoh(nfsp)
	register struct nfs_args *nfsp;
{

	NTOHL(nfsp->sotype);
	NTOHL(nfsp->proto);
	NTOHL(nfsp->flags);
	NTOHL(nfsp->wsize);
	NTOHL(nfsp->rsize);
	NTOHL(nfsp->timeo);
	NTOHL(nfsp->retrans);
	NTOHL(nfsp->maxgrouplist);
	NTOHL(nfsp->readahead);
	NTOHL(nfsp->leaseterm);
	NTOHL(nfsp->deadthresh);
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
int
nfs_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	int error;
	struct nfs_args args;
	struct mbuf *nam;
	struct vnode *vp;
	char pth[MNAMELEN], hst[MNAMELEN];
	u_int len;
	nfsv2fh_t nfh;

	if (error = copyin(data, (caddr_t)&args, sizeof (struct nfs_args)))
		return (error);
	if (error = copyin((caddr_t)args.fh, (caddr_t)&nfh, sizeof (nfsv2fh_t)))
		return (error);
	if (error = copyinstr(path, pth, MNAMELEN-1, &len))
		return (error);
	bzero(&pth[len], MNAMELEN - len);
	if (error = copyinstr(args.hostname, hst, MNAMELEN-1, &len))
		return (error);
	bzero(&hst[len], MNAMELEN - len);
	/* sockargs() call must be after above copyin() calls */
	if (error = sockargs(&nam, (caddr_t)args.addr,
		args.addrlen, MT_SONAME))
		return (error);
	args.fh = &nfh;
	error = mountnfs(&args, mp, nam, pth, hst, &vp);
	return (error);
}

/*
 * Common code for mount and mountroot
 */
int
mountnfs(argp, mp, nam, pth, hst, vpp)
	register struct nfs_args *argp;
	register struct mount *mp;
	struct mbuf *nam;
	char *pth, *hst;
	struct vnode **vpp;
{
	register struct nfsmount *nmp;
	struct nfsnode *np;
	int error;

	if (mp->mnt_flag & MNT_UPDATE) {
		nmp = VFSTONFS(mp);
		/* update paths, file handles, etc, here	XXX */
		m_freem(nam);
		return (0);
	} else {
		MALLOC(nmp, struct nfsmount *, sizeof (struct nfsmount),
		    M_NFSMNT, M_WAITOK);
		bzero((caddr_t)nmp, sizeof (struct nfsmount));
		mp->mnt_data = (qaddr_t)nmp;
	}
	getnewfsid(mp, MOUNT_NFS);
	nmp->nm_mountp = mp;
	nmp->nm_flag = argp->flags;
	if ((nmp->nm_flag & (NFSMNT_NQNFS | NFSMNT_MYWRITE)) ==
		(NFSMNT_NQNFS | NFSMNT_MYWRITE)) {
		error = EPERM;
		goto bad;
	}
	if (nmp->nm_flag & NFSMNT_NQNFS)
		/*
		 * We have to set mnt_maxsymlink to a non-zero value so
		 * that COMPAT_43 routines will know that we are setting
		 * the d_type field in directories (and can zero it for
		 * unsuspecting binaries).
		 */
		mp->mnt_maxsymlinklen = 1;
	nmp->nm_timeo = NFS_TIMEO;
	nmp->nm_retry = NFS_RETRANS;
	nmp->nm_wsize = NFS_WSIZE;
	nmp->nm_rsize = NFS_RSIZE;
	nmp->nm_numgrps = NFS_MAXGRPS;
	nmp->nm_readahead = NFS_DEFRAHEAD;
	nmp->nm_leaseterm = NQ_DEFLEASE;
	nmp->nm_deadthresh = NQ_DEADTHRESH;
	nmp->nm_tnext = (struct nfsnode *)nmp;
	nmp->nm_tprev = (struct nfsnode *)nmp;
	nmp->nm_inprog = NULLVP;
	bcopy((caddr_t)argp->fh, (caddr_t)&nmp->nm_fh, sizeof(nfsv2fh_t));
	mp->mnt_stat.f_type = MOUNT_NFS;
	bcopy(hst, mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy(pth, mp->mnt_stat.f_mntonname, MNAMELEN);
	nmp->nm_nam = nam;

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

	if ((argp->flags & NFSMNT_WSIZE) && argp->wsize > 0) {
		nmp->nm_wsize = argp->wsize;
		/* Round down to multiple of blocksize */
		nmp->nm_wsize &= ~0x1ff;
		if (nmp->nm_wsize <= 0)
			nmp->nm_wsize = 512;
		else if (nmp->nm_wsize > NFS_MAXDATA)
			nmp->nm_wsize = NFS_MAXDATA;
	}
	if (nmp->nm_wsize > MAXBSIZE)
		nmp->nm_wsize = MAXBSIZE;

	if ((argp->flags & NFSMNT_RSIZE) && argp->rsize > 0) {
		nmp->nm_rsize = argp->rsize;
		/* Round down to multiple of blocksize */
		nmp->nm_rsize &= ~0x1ff;
		if (nmp->nm_rsize <= 0)
			nmp->nm_rsize = 512;
		else if (nmp->nm_rsize > NFS_MAXDATA)
			nmp->nm_rsize = NFS_MAXDATA;
	}
	if (nmp->nm_rsize > MAXBSIZE)
		nmp->nm_rsize = MAXBSIZE;
	if ((argp->flags & NFSMNT_MAXGRPS) && argp->maxgrouplist >= 0 &&
		argp->maxgrouplist <= NFS_MAXGRPS)
		nmp->nm_numgrps = argp->maxgrouplist;
	if ((argp->flags & NFSMNT_READAHEAD) && argp->readahead >= 0 &&
		argp->readahead <= NFS_MAXRAHEAD)
		nmp->nm_readahead = argp->readahead;
	if ((argp->flags & NFSMNT_LEASETERM) && argp->leaseterm >= 2 &&
		argp->leaseterm <= NQ_MAXLEASE)
		nmp->nm_leaseterm = argp->leaseterm;
	if ((argp->flags & NFSMNT_DEADTHRESH) && argp->deadthresh >= 1 &&
		argp->deadthresh <= NQ_NEVERDEAD)
		nmp->nm_deadthresh = argp->deadthresh;
	/* Set up the sockets and per-host congestion */
	nmp->nm_sotype = argp->sotype;
	nmp->nm_soproto = argp->proto;

	/*
	 * For Connection based sockets (TCP,...) defer the connect until
	 * the first request, in case the server is not responding.
	 */
	if (nmp->nm_sotype == SOCK_DGRAM &&
		(error = nfs_connect(nmp, (struct nfsreq *)0)))
		goto bad;

	/*
	 * This is silly, but it has to be set so that vinifod() works.
	 * We do not want to do an nfs_statfs() here since we can get
	 * stuck on a dead server and we are holding a lock on the mount
	 * point.
	 */
	mp->mnt_stat.f_iosize = NFS_MAXDGRAMDATA;
	/*
	 * A reference count is needed on the nfsnode representing the
	 * remote root.  If this object is not persistent, then backward
	 * traversals of the mount point (i.e. "..") will not work if
	 * the nfsnode gets flushed out of the cache. Ufs does not have
	 * this problem, because one can identify root inodes by their
	 * number == ROOTINO (2).
	 */
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		goto bad;
	*vpp = NFSTOV(np);

	return (0);
bad:
	nfs_disconnect(nmp);
	free((caddr_t)nmp, M_NFSMNT);
	m_freem(nam);
	return (error);
}

/*
 * unmount system call
 */
int
nfs_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	register struct nfsmount *nmp;
	struct nfsnode *np;
	struct vnode *vp;
	int error, flags = 0;
	extern int doforce;

	if (mntflags & MNT_FORCE) {
		if (!doforce || (mp->mnt_flag & MNT_ROOTFS))
			return (EINVAL);
		flags |= FORCECLOSE;
	}
	nmp = VFSTONFS(mp);
	/*
	 * Goes something like this..
	 * - Check for activity on the root vnode (other than ourselves).
	 * - Call vflush() to clear out vnodes for this file system,
	 *   except for the root vnode.
	 * - Decrement reference on the vnode representing remote root.
	 * - Close the socket
	 * - Free up the data structures
	 */
	/*
	 * We need to decrement the ref. count on the nfsnode representing
	 * the remote root.  See comment in mountnfs().  The VFS unmount()
	 * has done vput on this vnode, otherwise we would get deadlock!
	 */
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		return(error);
	vp = NFSTOV(np);
	if (vp->v_usecount > 2) {
		vput(vp);
		return (EBUSY);
	}

	/*
	 * Must handshake with nqnfs_clientd() if it is active.
	 */
	nmp->nm_flag |= NFSMNT_DISMINPROG;
	while (nmp->nm_inprog != NULLVP)
		(void) tsleep((caddr_t)&lbolt, PSOCK, "nfsdism", 0);
	if (error = vflush(mp, vp, flags)) {
		vput(vp);
		nmp->nm_flag &= ~NFSMNT_DISMINPROG;
		return (error);
	}

	/*
	 * We are now committed to the unmount.
	 * For NQNFS, let the server daemon free the nfsmount structure.
	 */
	if (nmp->nm_flag & (NFSMNT_NQNFS | NFSMNT_KERB))
		nmp->nm_flag |= NFSMNT_DISMNT;

	/*
	 * There are two reference counts to get rid of here.
	 */
	vrele(vp);
	vrele(vp);
	vgone(vp);
	nfs_disconnect(nmp);
	m_freem(nmp->nm_nam);

	if ((nmp->nm_flag & (NFSMNT_NQNFS | NFSMNT_KERB)) == 0)
		free((caddr_t)nmp, M_NFSMNT);
	return (0);
}

/*
 * Return root of a filesystem
 */
int
nfs_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	register struct vnode *vp;
	struct nfsmount *nmp;
	struct nfsnode *np;
	int error;

	nmp = VFSTONFS(mp);
	if (error = nfs_nget(mp, &nmp->nm_fh, &np))
		return (error);
	vp = NFSTOV(np);
	vp->v_type = VDIR;
	vp->v_flag = VROOT;
	*vpp = vp;
	return (0);
}

extern int syncprt;

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
int
nfs_sync(mp, waitfor, cred, p)
	struct mount *mp;
	int waitfor;
	struct ucred *cred;
	struct proc *p;
{
	register struct vnode *vp;
	int error, allerror = 0;

	/*
	 * Force stale buffer cache information to be flushed.
	 */
loop:
	for (vp = mp->mnt_vnodelist.lh_first;
	     vp != NULL;
	     vp = vp->v_mntvnodes.le_next) {
		/*
		 * If the vnode that we are about to sync is no longer
		 * associated with this mount point, start over.
		 */
		if (vp->v_mount != mp)
			goto loop;
		if (VOP_ISLOCKED(vp) || vp->v_dirtyblkhd.lh_first == NULL)
			continue;
		if (vget(vp, 1))
			goto loop;
		if (error = VOP_FSYNC(vp, cred, waitfor, p))
			allerror = error;
		vput(vp);
	}
	return (allerror);
}

/*
 * NFS flat namespace lookup.
 * Currently unsupported.
 */
/* ARGSUSED */
int
nfs_vget(mp, ino, vpp)
	struct mount *mp;
	ino_t ino;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

/*
 * At this point, this should never happen
 */
/* ARGSUSED */
int
nfs_fhtovp(mp, fhp, nam, vpp, exflagsp, credanonp)
	register struct mount *mp;
	struct fid *fhp;
	struct mbuf *nam;
	struct vnode **vpp;
	int *exflagsp;
	struct ucred **credanonp;
{

	return (EINVAL);
}

/*
 * Vnode pointer to File handle, should never happen either
 */
/* ARGSUSED */
int
nfs_vptofh(vp, fhp)
	struct vnode *vp;
	struct fid *fhp;
{

	return (EINVAL);
}

/*
 * Vfs start routine, a no-op.
 */
/* ARGSUSED */
int
nfs_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
int
nfs_quotactl(mp, cmd, uid, arg, p)
	struct mount *mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	struct proc *p;
{

	return (EOPNOTSUPP);
}
