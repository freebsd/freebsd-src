/*-
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
 * $FreeBSD: src/sys/fs/portalfs/portal_vfsops.c,v 1.60 2007/03/13 01:50:23 tegge Exp $
 */

/*
 * Portal Filesystem
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domain.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/file.h>		/* Must come after sys/malloc.h */
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/vnode.h>

#include <fs/portalfs/portal.h>

static MALLOC_DEFINE(M_PORTALFSMNT, "portal_mount", "PORTAL mount structure");

static vfs_unmount_t	portal_unmount;
static vfs_root_t	portal_root;
static vfs_statfs_t	portal_statfs;

static const char *portal_opts[] = {
	"socket", "config",
	NULL
};

static int
portal_cmount(struct mntarg *ma, void *data, int flags, struct thread *td)
{
	struct portal_args args;
	int error;

	if (data == NULL)
		return (EINVAL);
	error = copyin(data, &args, sizeof args);
	if (error)
		return (error);

	ma = mount_argf(ma, "socket", "%d", args.pa_socket);
	ma = mount_argsu(ma, "config", args.pa_config, MAXPATHLEN);
	error = kernel_mount(ma, flags);

	return (error);
}

/*
 * Mount the per-process file descriptors (/dev/fd)
 */
static int
portal_mount(struct mount *mp, struct thread *td)
{
	struct file *fp;
	struct portalmount *fmp;
	struct socket *so;
	struct vnode *rvp;
	struct portalnode *pn;
	int error, v;
	char *p;

	if (vfs_filteropt(mp->mnt_optnew, portal_opts))
		return (EINVAL);

	error = vfs_scanopt(mp->mnt_optnew, "socket", "%d", &v);
	if (error != 1)
		return (EINVAL);
	error = vfs_getopt(mp->mnt_optnew, "config", (void **)&p, NULL);
	if (error)
		return (error);

	if ((error = fget(td, v, &fp)) != 0)
		return (error);
        if (fp->f_type != DTYPE_SOCKET) {
		fdrop(fp, td);
                return(ENOTSOCK);
	}
	so = fp->f_data;	/* XXX race against userland */
	if (so->so_proto->pr_domain->dom_family != AF_UNIX) {
		fdrop(fp, td);
		return (ESOCKTNOSUPPORT);
	}

	MALLOC(pn, struct portalnode *, sizeof(struct portalnode),
		M_TEMP, M_WAITOK);

	MALLOC(fmp, struct portalmount *, sizeof(struct portalmount),
		M_PORTALFSMNT, M_WAITOK);	/* XXX */

	error = getnewvnode("portal", mp, &portal_vnodeops, &rvp); /* XXX */
	if (error) {
		FREE(fmp, M_PORTALFSMNT);
		FREE(pn, M_TEMP);
		fdrop(fp, td);
		return (error);
	}

	error = insmntque(rvp, mp);	/* XXX: Too early for mpsafe fs */
	if (error != 0) {
		FREE(fmp, M_PORTALFSMNT);
		FREE(pn, M_TEMP);
		fdrop(fp, td);
		return (error);
	}
	rvp->v_data = pn;
	rvp->v_type = VDIR;
	rvp->v_vflag |= VV_ROOT;
	VTOPORTAL(rvp)->pt_arg = 0;
	VTOPORTAL(rvp)->pt_size = 0;
	VTOPORTAL(rvp)->pt_fileid = PORTAL_ROOTFILEID;
	fmp->pm_root = rvp;
	fhold(fp);
	fmp->pm_server = fp;

	MNT_ILOCK(mp);
	mp->mnt_flag |= MNT_LOCAL;
	MNT_IUNLOCK(mp);
	mp->mnt_data = (qaddr_t) fmp;
	vfs_getnewfsid(mp);

	vfs_mountedfrom(mp, p);
	fdrop(fp, td);
	return (0);
}

static int
portal_unmount(mp, mntflags, td)
	struct mount *mp;
	int mntflags;
	struct thread *td;
{
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
	/* There is 1 extra root vnode reference (pm_root). */
	error = vflush(mp, 1, flags, td);
	if (error)
		return (error);

	/*
	 * Shutdown the socket.  This will cause the select in the
	 * daemon to wake up, and then the accept will get ECONNABORTED
	 * which it interprets as a request to go and bury itself.
	 */
	soshutdown(VFSTOPORTAL(mp)->pm_server->f_data, 2);
	/*
	 * Discard reference to underlying file.  Must call closef because
	 * this may be the last reference.
	 */
	closef(VFSTOPORTAL(mp)->pm_server, (struct thread *) 0);
	/*
	 * Finally, throw away the portalmount structure
	 */
	free(mp->mnt_data, M_PORTALFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return (0);
}

static int
portal_root(mp, flags, vpp, td)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
	struct thread *td;
{
	struct vnode *vp;

	/*
	 * Return locked reference to root.
	 */
	vp = VFSTOPORTAL(mp)->pm_root;
	VREF(vp);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
	*vpp = vp;
	return (0);
}

static int
portal_statfs(mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{

	sbp->f_flags = 0;
	sbp->f_bsize = DEV_BSIZE;
	sbp->f_iosize = DEV_BSIZE;
	sbp->f_blocks = 2;		/* 1K to keep df happy */
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = 1;		/* Allow for "." */
	sbp->f_ffree = 0;		/* See comments above */
	return (0);
}

static struct vfsops portal_vfsops = {
	.vfs_cmount =		portal_cmount,
	.vfs_mount =		portal_mount,
	.vfs_root =		portal_root,
	.vfs_statfs =		portal_statfs,
	.vfs_unmount =		portal_unmount,
};

VFS_SET(portal_vfsops, portalfs, VFCF_SYNTHETIC);
