/*
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)portal_vfsops.c	8.11 (Berkeley) 5/14/95
 *
 * $Id: portal_vfsops.c,v 1.17 1997/08/16 19:15:17 wollman Exp $
 */

/*
 * Portal Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <miscfs/portal/portal.h>

static MALLOC_DEFINE(M_PORTALFSMNT, "PORTAL mount", "PORTAL mount structure");

static int	portal_init __P((struct vfsconf *));
static int	portal_mount __P((struct mount *mp, char *path, caddr_t data,
				  struct nameidata *ndp, struct proc *p));
static int	portal_start __P((struct mount *mp, int flags, struct proc *p));
static int	portal_unmount __P((struct mount *mp, int mntflags,
				    struct proc *p));
static int	portal_root __P((struct mount *mp, struct vnode **vpp));
static int	portal_statfs __P((struct mount *mp, struct statfs *sbp,
				   struct proc *p));

static int
portal_init(vfsp)
	struct vfsconf *vfsp;
{

	return (0);
}

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
static int
portal_mount(mp, path, data, ndp, p)
	struct mount *mp;
	char *path;
	caddr_t data;
	struct nameidata *ndp;
	struct proc *p;
{
	struct file *fp;
	struct portal_args args;
	struct portalmount *fmp;
	struct socket *so;
	struct vnode *rvp;
	struct portalnode *pn;
	u_int size;
	int error;

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	error = copyin(data, (caddr_t) &args, sizeof(struct portal_args));
	if (error)
		return (error);

	error = getsock(p->p_fd, args.pa_socket, &fp);
	if (error)
		return (error);
	so = (struct socket *) fp->f_data;
	if (so->so_proto->pr_domain->dom_family != AF_UNIX)
		return (ESOCKTNOSUPPORT);

	MALLOC(pn, struct portalnode *, sizeof(struct portalnode),
		M_TEMP, M_WAITOK);

	MALLOC(fmp, struct portalmount *, sizeof(struct portalmount),
		M_PORTALFSMNT, M_WAITOK);	/* XXX */

	error = getnewvnode(VT_PORTAL, mp, portal_vnodeop_p, &rvp); /* XXX */
	if (error) {
		FREE(fmp, M_PORTALFSMNT);
		FREE(pn, M_TEMP);
		return (error);
	}

	rvp->v_data = pn;
	rvp->v_type = VDIR;
	rvp->v_flag |= VROOT;
	VTOPORTAL(rvp)->pt_arg = 0;
	VTOPORTAL(rvp)->pt_size = 0;
	VTOPORTAL(rvp)->pt_fileid = PORTAL_ROOTFILEID;
	fmp->pm_root = rvp;
	fmp->pm_server = fp; fp->f_count++;

	mp->mnt_flag |= MNT_LOCAL;
	mp->mnt_data = (qaddr_t) fmp;
	vfs_getnewfsid(mp);

	(void)copyinstr(path, mp->mnt_stat.f_mntonname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntonname + size, MNAMELEN - size);
	(void)copyinstr(args.pa_config,
	    mp->mnt_stat.f_mntfromname, MNAMELEN - 1, &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);

#ifdef notdef
	bzero(mp->mnt_stat.f_mntfromname, MNAMELEN);
	bcopy("portal", mp->mnt_stat.f_mntfromname, sizeof("portal"));
#endif

	return (0);
}

static int
portal_start(mp, flags, p)
	struct mount *mp;
	int flags;
	struct proc *p;
{

	return (0);
}

static int
portal_unmount(mp, mntflags, p)
	struct mount *mp;
	int mntflags;
	struct proc *p;
{
	struct vnode *rootvp = VFSTOPORTAL(mp)->pm_root;
	int error, flags = 0;


	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/*
	 * Clear out buffer cache.  I don't think we
	 * ever get anything cached at this level at the
	 * moment, but who knows...
	 */
#ifdef notyet
	mntflushbuf(mp, 0);
	if (mntinvalbuf(mp, 1))
		return (EBUSY);
#endif
	if (rootvp->v_usecount > 1)
		return (EBUSY);
	error = vflush(mp, rootvp, flags);
	if (error)
		return (error);

	/*
	 * Release reference on underlying root vnode
	 */
	vrele(rootvp);
	/*
	 * And blow it away for future re-use
	 */
	vgone(rootvp);
	/*
	 * Shutdown the socket.  This will cause the select in the
	 * daemon to wake up, and then the accept will get ECONNABORTED
	 * which it interprets as a request to go and bury itself.
	 */
	soshutdown((struct socket *) VFSTOPORTAL(mp)->pm_server->f_data, 2);
	/*
	 * Discard reference to underlying file.  Must call closef because
	 * this may be the last reference.
	 */
	closef(VFSTOPORTAL(mp)->pm_server, (struct proc *) 0);
	/*
	 * Finally, throw away the portalmount structure
	 */
	free(mp->mnt_data, M_PORTALFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return (0);
}

static int
portal_root(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct proc *p = curproc;	/* XXX */
	struct vnode *vp;

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOPORTAL(mp)->pm_root;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
	*vpp = vp;
	return (0);
}

static int
portal_statfs(mp, sbp, p)
	struct mount *mp;
	struct statfs *sbp;
	struct proc *p;
{

	sbp->f_flags = 0;
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1;		/* Allow for "." */
	sbp->f_ffree = 0;		/* See comments above */
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	return (0);
}

#define portal_fhtovp ((int (*) __P((struct mount *, struct fid *, \
	    struct sockaddr *, struct vnode **, int *, struct ucred **)))eopnotsupp)
#define portal_quotactl ((int (*) __P((struct mount *, int, uid_t, caddr_t, \
	    struct proc *)))eopnotsupp)
#define portal_sync ((int (*) __P((struct mount *, int, struct ucred *, \
	    struct proc *)))nullop)
#define portal_sysctl ((int (*) __P((int *, u_int, void *, size_t *, void *, \
	    size_t, struct proc *)))eopnotsupp)
#define portal_vget ((int (*) __P((struct mount *, ino_t, struct vnode **))) \
	    eopnotsupp)
#define portal_vptofh ((int (*) __P((struct vnode *, struct fid *)))eopnotsupp)

static struct vfsops portal_vfsops = {
	portal_mount,
	portal_start,
	portal_unmount,
	portal_root,
	portal_quotactl,
	portal_statfs,
	portal_sync,
	portal_vget,
	portal_fhtovp,
	portal_vptofh,
	portal_init,
};

VFS_SET(portal_vfsops, portal, MOUNT_PORTAL, VFCF_SYNTHETIC);
