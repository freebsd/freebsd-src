#define DEBUG 1
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2000
 *	Poul-Henning Kamp.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the University nor the names of its contributors
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
 *	@(#)kernfs_vnops.c	8.15 (Berkeley) 5/21/95
 * From: FreeBSD: src/sys/miscfs/kernfs/kernfs_vnops.c 1.43
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vmmeter.h>
#include <sys/time.h>
#include <sys/conf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/resource.h>
#include <sys/eventhandler.h>

#define DEVFS_INTERN
#include <fs/devfs/devfs.h>

#define KSTRING	256		/* Largest I/O available via this filesystem */
#define	UIO_MX 32

static int	devfs_access __P((struct vop_access_args *ap));
static int	devfs_badop __P((void));
static int	devfs_getattr __P((struct vop_getattr_args *ap));
static int	devfs_lookup __P((struct vop_lookup_args *ap));
static int	devfs_print __P((struct vop_print_args *ap));
static int	devfs_readdir __P((struct vop_readdir_args *ap));
static int	devfs_readlink __P((struct vop_readlink_args *ap));
static int	devfs_reclaim __P((struct vop_reclaim_args *ap));
static int	devfs_remove __P((struct vop_remove_args *ap));
static int	devfs_revoke __P((struct vop_revoke_args *ap));
static int	devfs_setattr __P((struct vop_setattr_args *ap));
static int	devfs_symlink __P((struct vop_symlink_args *ap));


static int
devfs_allocv(struct devfs_dirent *de, struct mount *mp, struct vnode **vpp, struct proc *p)
{
	int error;
	struct vnode *vp;

loop:
	vp = de->de_vnode;
	if (vp != NULL) {
		if (vget(vp, 0, p ? p : curproc))
			goto loop;
		*vpp = vp;
		return (0);
	}
	error = getnewvnode(VT_DEVFS, mp, devfs_vnodeop_p, &vp);
	if (error != 0) {
		printf("devfs_allocv: failed to allocate new vnode\n");
		return (error);
	}

	if (de->de_dirent->d_type == DT_CHR) {
		vp->v_type = VCHR;
		vp = addaliasu(vp, devfs_inot[de->de_inode]->si_udev);
		vp->v_op = devfs_specop_p;
		vp->v_data = de;
	} else if (de->de_dirent->d_type == DT_DIR) {
		vp->v_type = VDIR;
		vp->v_data = de->de_dir;
		TAILQ_FIRST(&de->de_dir->dd_list)->de_vnode = vp;
	} else if (de->de_dirent->d_type == DT_LNK) {
		vp->v_type = VLNK;
		vp->v_data = de;
	} else {
		vp->v_type = VBAD;
		vp->v_data = de;
	}
	de->de_vnode = vp;
	vhold(vp);
	*vpp = vp;
	return (0);
}
/*
 * vp is the current namei directory
 * ndp is the name to locate in that directory...
 */
static int
devfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname = cnp->cn_nameptr;
	struct proc *p = cnp->cn_proc;
	struct devfs_dir *dd;
	struct devfs_dirent *de;
	struct devfs_mount *fmp;
	dev_t cdev;
	int error, cloned;

	*vpp = NULLVP;

	VOP_UNLOCK(dvp, 0, p);
	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		vn_lock(dvp, LK_SHARED | LK_RETRY, p);
		return (0);
	}

	cloned = 0;

	fmp = (struct devfs_mount *)dvp->v_mount->mnt_data;
again:

	devfs_populate(fmp);
	dd = dvp->v_data;
	TAILQ_FOREACH(de, &dd->dd_list, de_list) {
		if (cnp->cn_namelen != de->de_dirent->d_namlen)
			continue;
		if (bcmp(cnp->cn_nameptr, de->de_dirent->d_name, de->de_dirent->d_namlen) != 0)
			continue;
		goto found;
	}

	if (!cloned) {
		/* OK, we didn't have that one, so lets try to create it on the fly... */
		cdev = NODEV;
		EVENTHANDLER_INVOKE(devfs_clone, cnp->cn_nameptr, cnp->cn_namelen, &cdev);
#if 0
		printf("cloned %s -> %p %s\n", cnp->cn_nameptr, cdev,
		    cdev == NODEV ? "NODEV" : cdev->si_name);
#endif
		if (cdev != NODEV) {
			cloned = 1;
			goto again;
		}
	}

	/* No luck, too bad. */

	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
	    (cnp->cn_flags & ISLASTCN)) {
		cnp->cn_flags |= SAVENAME;
		if (!(cnp->cn_flags & LOCKPARENT))
			VOP_UNLOCK(dvp, 0, p);
		return (EJUSTRETURN);
	} else {
		vn_lock(dvp, LK_SHARED | LK_RETRY, p);
		return (ENOENT);
	}


found:

	error = devfs_allocv(de, dvp->v_mount, vpp, p);
	if (error != 0) {
		vn_lock(dvp, LK_SHARED | LK_RETRY, p);
		return (error);
	}
	if ((cnp->cn_nameiop == DELETE) && (cnp->cn_flags & ISLASTCN)) {
		if (*vpp == dvp) {
			VREF(dvp);
			*vpp = dvp;
			return (0);
		}
		VREF(*vpp);
		if (!(cnp->cn_flags & LOCKPARENT))
			VOP_UNLOCK(dvp, 0, p);
		return (0);
	}
	vn_lock(*vpp, LK_SHARED | LK_RETRY, p);
	if (!(cnp->cn_flags & LOCKPARENT))
		VOP_UNLOCK(dvp, 0, p);
	return (0);
}

static int
devfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	mode_t amode = ap->a_mode;
	struct devfs_dirent *de = vp->v_data;
	mode_t fmode = de->de_mode;

	/* Some files are simply not modifiable. */
	if ((amode & VWRITE) && (fmode & (S_IWUSR|S_IWGRP|S_IWOTH)) == 0)
		return (EPERM);

	return (vaccess(vp->v_type, de->de_mode, de->de_uid, de->de_gid,
	    ap->a_mode, ap->a_cred));
}

static int
devfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	int error = 0;
	struct devfs_dirent *de;
	struct devfs_dir *dd;

	if (vp->v_type == VDIR) {
		dd = vp->v_data;
		de = TAILQ_FIRST(&dd->dd_list);
	} else {
		de = vp->v_data;
	}
	bzero((caddr_t) vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = de->de_uid;
	vap->va_gid = de->de_gid;
	vap->va_mode = de->de_mode;
	vap->va_size = 0;
	vap->va_blocksize = DEV_BSIZE;
	vap->va_atime = de->de_atime;
	vap->va_mtime = de->de_mtime;
	vap->va_ctime = de->de_ctime;
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_rdev = 0;
	vap->va_bytes = 0;
	vap->va_nlink = 1;
	vap->va_fileid = de->de_inode;

#if 0
	if (vp->v_flag & VROOT) {
#ifdef DEBUG
		printf("devfs_getattr: stat rootdir\n");
#endif
		vap->va_type = VDIR;
		vap->va_nlink = 2;
		vap->va_fileid = 2;
		vap->va_size = DEV_BSIZE;
	} else
#endif

	if (de->de_dirent->d_type == DT_DIR) {
		vap->va_type = VDIR;
	} else if (de->de_dirent->d_type == DT_LNK) {
		vap->va_type = VLNK;
	} else if (de->de_dirent->d_type == DT_CHR) {
		vap->va_type = VCHR;
		vap->va_rdev = devfs_inot[de->de_inode]->si_udev;
	}

#ifdef DEBUG
	if (error)
		printf("devfs_getattr: return error %d\n", error);
#endif
	return (error);
}

static int
devfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct devfs_dir *dd;
	struct devfs_dirent *de;

	if (ap->a_vp->v_type == VDIR) {
		dd = ap->a_vp->v_data;
		de = TAILQ_FIRST(&dd->dd_list);
	} else {
		de = ap->a_vp->v_data;
	}

	if (ap->a_vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);
	if (ap->a_vap->va_uid != (uid_t)VNOVAL)
		de->de_uid = ap->a_vap->va_uid;
	if (ap->a_vap->va_gid != (gid_t)VNOVAL)
		de->de_gid = ap->a_vap->va_gid;
	if (ap->a_vap->va_mode != (mode_t)VNOVAL)
		de->de_mode = ap->a_vap->va_mode;
	if (ap->a_vap->va_atime.tv_sec != VNOVAL)
		de->de_atime = ap->a_vap->va_atime;
	if (ap->a_vap->va_mtime.tv_sec != VNOVAL)
		de->de_mtime = ap->a_vap->va_mtime;

	/*
	 * Silently ignore attribute changes.
	 * This allows for open with truncate to have no
	 * effect until some data is written.  I want to
	 * do it this way because all writes are atomic.
	 */
	return (0);
}

static int
devfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	int error, i;
	struct uio *uio = ap->a_uio;
	struct dirent *dp;
	struct devfs_dir *dd;
	struct devfs_dirent *de;
	off_t off;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	i = (u_int)off / UIO_MX;
	error = 0;
	dd = ap->a_vp->v_data;
	de = TAILQ_FIRST(&dd->dd_list);
	off = 0;
	while (uio->uio_resid >= UIO_MX && de != NULL) {
		dp = de->de_dirent;
		dp->d_fileno = de->de_inode;
		if (off >= uio->uio_offset)
			if ((error = uiomove((caddr_t)dp, dp->d_reclen, uio)) != 0)
				break;
		off += dp->d_reclen;
		de = TAILQ_NEXT(de, de_list);
	}

	uio->uio_offset = off;

	return (error);
}

static int
devfs_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cead;
	} */ *ap;
{
	int error;
	struct devfs_dirent *de;

	de = ap->a_vp->v_data;
	error = uiomove(de->de_symlink, strlen(de->de_symlink) + 1, ap->a_uio);
	return (error);
}

static int
devfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	vp->v_data = NULL;
	return (0);
}

static int
devfs_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dir *dd;
	struct devfs_dirent *de;

	dd = ap->a_dvp->v_data;
	de = vp->v_data;
	devfs_delete(dd, de);
	return (0);
}

/*
 * Revoke is called on a tty when a terminal session ends.  The vnode
 * is orphaned by setting v_op to deadfs so we need to let go of it
 * as well so that we create a new one next time around.
 */
static int
devfs_revoke(ap)
	struct vop_revoke_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dirent *de;

	de = vp->v_data;
	vdrop(de->de_vnode);
	de->de_vnode = NULL;
	vop_revoke(ap);
	return (0);
}

static int
devfs_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	int i;
	struct devfs_dir *dd;
	struct devfs_dirent *de;
	struct devfs_mount *fmp;

	fmp = (struct devfs_mount *)ap->a_dvp->v_mount->mnt_data;
	dd = ap->a_dvp->v_data;
	de = devfs_newdirent(ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen);
	de->de_uid = 0;
	de->de_gid = 0;
	de->de_mode = 0642;
	de->de_inode = fmp->dm_inode++;
	de->de_dirent->d_type = DT_LNK;
	i = strlen(ap->a_target) + 1;
	MALLOC(de->de_symlink, char *, i, M_DEVFS, M_WAITOK);
	bcopy(ap->a_target, de->de_symlink, i);
	TAILQ_INSERT_TAIL(&dd->dd_list, de, de_list);
	devfs_allocv(de, ap->a_dvp->v_mount, ap->a_vpp, 0);
	VREF(*(ap->a_vpp));
	return (0);
}

/*
 * Print out the contents of a devfs vnode.
 */
/* ARGSUSED */
static int
devfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	printf("tag VT_DEVFS, devfs vnode\n");
	return (0);
}

/*
 * Kernfs "should never get here" operation
 */
static int
devfs_badop()
{
	return (EIO);
}

vop_t	**devfs_vnodeop_p;
static struct vnodeopv_entry_desc devfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) devfs_access },
	{ &vop_bmap_desc,		(vop_t *) devfs_badop },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_lookup_desc,		(vop_t *) devfs_lookup },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_print_desc,		(vop_t *) devfs_print },
	{ &vop_readdir_desc,		(vop_t *) devfs_readdir },
	{ &vop_readlink_desc,		(vop_t *) devfs_readlink },
	{ &vop_reclaim_desc,		(vop_t *) devfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) devfs_remove },
	{ &vop_revoke_desc,		(vop_t *) devfs_revoke },
	{ &vop_setattr_desc,		(vop_t *) devfs_setattr },
	{ &vop_symlink_desc,		(vop_t *) devfs_symlink },
	{ NULL, NULL }
};
static struct vnodeopv_desc devfs_vnodeop_opv_desc =
	{ &devfs_vnodeop_p, devfs_vnodeop_entries };

VNODEOP_SET(devfs_vnodeop_opv_desc);

#if 0
int
foo(ap)
	struct vop_generic_args *ap;
{
	int i;

	i = spec_vnoperate(ap);
	printf("foo(%s) = %d\n", ap->a_desc->vdesc_name, i);
	return (i);
}
#endif

vop_t	**devfs_specop_p;
static struct vnodeopv_entry_desc devfs_specop_entries[] = {
#if 1
	{ &vop_default_desc,		(vop_t *) spec_vnoperate },
#else
	{ &vop_default_desc,		(vop_t *) foo },
	{ &vop_lock_desc,		(vop_t *) spec_vnoperate },
	{ &vop_unlock_desc,		(vop_t *) spec_vnoperate },
	{ &vop_lease_desc,		(vop_t *) spec_vnoperate },
	{ &vop_strategy_desc,		(vop_t *) spec_vnoperate },
	{ &vop_bmap_desc,		(vop_t *) spec_vnoperate },
#endif
	{ &vop_access_desc,		(vop_t *) devfs_access },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_print_desc,		(vop_t *) devfs_print },
	{ &vop_reclaim_desc,		(vop_t *) devfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) devfs_remove },
	{ &vop_revoke_desc,		(vop_t *) devfs_revoke },
	{ &vop_setattr_desc,		(vop_t *) devfs_setattr },
	{ NULL, NULL }
};
static struct vnodeopv_desc devfs_specop_opv_desc =
	{ &devfs_specop_p, devfs_specop_entries };

VNODEOP_SET(devfs_specop_opv_desc);
