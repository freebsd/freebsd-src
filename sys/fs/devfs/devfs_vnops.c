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

/*
 * TODO:
 *	remove empty directories
 *	mknod: hunt down DE_DELETED, compare name, reinstantiate. 
 *	mkdir: want it ?
 */

#include <opt_devfs.h>
#include <opt_mac.h>
#ifndef NODEVFS

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <fs/devfs/devfs.h>

static int	devfs_access(struct vop_access_args *ap);
static int	devfs_getattr(struct vop_getattr_args *ap);
static int	devfs_ioctl(struct vop_ioctl_args *ap);
static int	devfs_lookupx(struct vop_lookup_args *ap);
static int	devfs_mknod(struct vop_mknod_args *ap);
static int	devfs_pathconf(struct vop_pathconf_args *ap);
static int	devfs_print(struct vop_print_args *ap);
static int	devfs_read(struct vop_read_args *ap);
static int	devfs_readdir(struct vop_readdir_args *ap);
static int	devfs_readlink(struct vop_readlink_args *ap);
static int	devfs_reclaim(struct vop_reclaim_args *ap);
#ifdef MAC
static int	devfs_refreshlabel(struct vop_refreshlabel_args *ap);
#endif
static int	devfs_remove(struct vop_remove_args *ap);
static int	devfs_revoke(struct vop_revoke_args *ap);
static int	devfs_setattr(struct vop_setattr_args *ap);
#ifdef MAC
static int	devfs_setlabel(struct vop_setlabel_args *ap);
#endif
static int	devfs_symlink(struct vop_symlink_args *ap);

/*
 * Construct the fully qualified path name relative to the mountpoint
 */ 
static char *
devfs_fqpn(char *buf, struct vnode *dvp, struct componentname *cnp)
{
	int i;
	struct devfs_dirent *de, *dd;
	struct devfs_mount *dmp;

	dmp = VFSTODEVFS(dvp->v_mount);
	dd = dvp->v_data;
	i = SPECNAMELEN;
	buf[i] = '\0';
	i -= cnp->cn_namelen;
	if (i < 0)
		 return (NULL);
	bcopy(cnp->cn_nameptr, buf + i, cnp->cn_namelen);
	de = dd;
	while (de != dmp->dm_basedir) {
		i--;
		if (i < 0)
			 return (NULL);
		buf[i] = '/';
		i -= de->de_dirent->d_namlen;
		if (i < 0)
			 return (NULL);
		bcopy(de->de_dirent->d_name, buf + i,
		    de->de_dirent->d_namlen);
		de = TAILQ_FIRST(&de->de_dlist);	/* "." */
		de = TAILQ_NEXT(de, de_list);		/* ".." */
		de = de->de_dir;
	}
	return (buf + i);
}

int
devfs_allocv(struct devfs_dirent *de, struct mount *mp, struct vnode **vpp, struct thread *td)
{
	int error;
	struct vnode *vp;
	dev_t dev;

	if (td == NULL)
		td = curthread; /* XXX */
loop:
	vp = de->de_vnode;
	if (vp != NULL) {
		if (vget(vp, LK_EXCLUSIVE, td ? td : curthread))
			goto loop;
		*vpp = vp;
		return (0);
	}
	if (de->de_dirent->d_type == DT_CHR) {
		dev = *devfs_itod(de->de_inode);
		if (dev == NULL)
			return (ENOENT);
	} else {
		dev = NODEV;
	}
	error = getnewvnode(VT_DEVFS, mp, devfs_vnodeop_p, &vp);
	if (error != 0) {
		printf("devfs_allocv: failed to allocate new vnode\n");
		return (error);
	}

	if (de->de_dirent->d_type == DT_CHR) {
		vp->v_type = VCHR;
		vp = addaliasu(vp, dev->si_udev);
		vp->v_op = devfs_specop_p;
	} else if (de->de_dirent->d_type == DT_DIR) {
		vp->v_type = VDIR;
	} else if (de->de_dirent->d_type == DT_LNK) {
		vp->v_type = VLNK;
	} else {
		vp->v_type = VBAD;
	}
	vp->v_data = de;
	de->de_vnode = vp;
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);
#ifdef MAC
	mac_create_devfs_vnode(de, vp);
#endif
	*vpp = vp;
	return (0);
}

static int
devfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dirent *de;

	de = vp->v_data;
	if (vp->v_type == VDIR)
		de = de->de_dir;

	return (vaccess(vp->v_type, de->de_mode, de->de_uid, de->de_gid,
	    ap->a_mode, ap->a_cred, NULL));
}

static int
devfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	int error = 0;
	struct devfs_dirent *de;
	dev_t dev;

	de = vp->v_data;
	if (vp->v_type == VDIR)
		de = de->de_dir;
	bzero((caddr_t) vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = de->de_uid;
	vap->va_gid = de->de_gid;
	vap->va_mode = de->de_mode;
	if (vp->v_type == VLNK) 
		vap->va_size = de->de_dirent->d_namlen;
	else if (vp->v_type == VDIR)
		vap->va_size = vap->va_bytes = DEV_BSIZE;
	else
		vap->va_size = 0;
	if (vp->v_type != VDIR)
		vap->va_bytes = 0;
	vap->va_blocksize = DEV_BSIZE;
	vap->va_type = vp->v_type;

#define fix(aa)							\
	do {							\
		if ((aa).tv_sec == 0) {				\
			(aa).tv_sec = boottime.tv_sec;		\
			(aa).tv_nsec = boottime.tv_usec * 1000; \
		}						\
	} while (0)

	if (vp->v_type != VCHR)  {
		fix(de->de_atime);
		vap->va_atime = de->de_atime;
		fix(de->de_mtime);
		vap->va_mtime = de->de_mtime;
		fix(de->de_ctime);
		vap->va_ctime = de->de_ctime;
	} else {
		dev = vp->v_rdev;
		fix(dev->si_atime);
		vap->va_atime = dev->si_atime;
		fix(dev->si_mtime);
		vap->va_mtime = dev->si_mtime;
		fix(dev->si_ctime);
		vap->va_ctime = dev->si_ctime;
		vap->va_rdev = dev->si_udev;
	}
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_nlink = de->de_links;
	vap->va_fileid = de->de_inode;

	return (error);
}

static int
devfs_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	int error;

	error = devfs_rules_ioctl(ap->a_vp->v_mount, ap->a_command, ap->a_data,
	    ap->a_td);
	return (error);
}

static int
devfs_lookupx(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct componentname *cnp;
	struct vnode *dvp, **vpp;
	struct thread *td;
	struct devfs_dirent *de, *dd;
	struct devfs_mount *dmp;
	dev_t cdev, *cpdev;
	int error, cloned, flags, nameiop;
	char specname[SPECNAMELEN + 1], *pname;

	cnp = ap->a_cnp;
	vpp = ap->a_vpp;
	dvp = ap->a_dvp;
	pname = cnp->cn_nameptr;
	td = cnp->cn_thread;
	flags = cnp->cn_flags;
	nameiop = cnp->cn_nameiop;
	dmp = VFSTODEVFS(dvp->v_mount);
	cloned = 0;
	dd = dvp->v_data;
	
	*vpp = NULLVP;
	cnp->cn_flags &= ~PDIRUNLOCK;

	if ((flags & ISLASTCN) && nameiop == RENAME)
		return (EOPNOTSUPP);

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	if ((flags & ISDOTDOT) && (dvp->v_flag & VROOT))
		return (EIO);

	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, td);
	if (error)
		return (error);

	if (cnp->cn_namelen == 1 && *pname == '.') {
		if ((flags & ISLASTCN) && nameiop != LOOKUP)
			return (EINVAL);
		*vpp = dvp;
		VREF(dvp);
		return (0);
	}

	if (flags & ISDOTDOT) {
		if ((flags & ISLASTCN) && nameiop != LOOKUP)
			return (EINVAL);
		VOP_UNLOCK(dvp, 0, td);
		cnp->cn_flags |= PDIRUNLOCK;
		de = TAILQ_FIRST(&dd->de_dlist);	/* "." */
		de = TAILQ_NEXT(de, de_list);		/* ".." */
		de = de->de_dir;
		error = devfs_allocv(de, dvp->v_mount, vpp, td);
		if (error || ((flags & LOCKPARENT) && (flags & ISLASTCN))) {
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);
			cnp->cn_flags &= ~PDIRUNLOCK;
		}
		return (error);
	}

	devfs_populate(dmp);
	dd = dvp->v_data;
	TAILQ_FOREACH(de, &dd->de_dlist, de_list) {
		if (cnp->cn_namelen != de->de_dirent->d_namlen)
			continue;
		if (bcmp(cnp->cn_nameptr, de->de_dirent->d_name,
		    de->de_dirent->d_namlen) != 0)
			continue;
		if (de->de_flags & DE_WHITEOUT)
			goto notfound;
		goto found;
	}

	if (nameiop == DELETE)
		goto notfound;

	/*
	 * OK, we didn't have an entry for the name we were asked for
	 * so we try to see if anybody can create it on demand.
	 */
	pname = devfs_fqpn(specname, dvp, cnp);
	if (pname == NULL)
		goto notfound;

	cdev = NODEV;
	EVENTHANDLER_INVOKE(dev_clone, pname, strlen(pname), &cdev);
	if (cdev == NODEV)
		goto notfound;

	devfs_populate(dmp);
	dd = dvp->v_data;

	TAILQ_FOREACH(de, &dd->de_dlist, de_list) {
		cpdev = devfs_itod(de->de_inode);
		if (cpdev != NULL && cdev == *cpdev)
			goto found;
		continue;
	}

notfound:

	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & (LOCKPARENT | WANTPARENT)) && (flags & ISLASTCN)) {
		cnp->cn_flags |= SAVENAME;
		if (!(flags & LOCKPARENT)) {
			VOP_UNLOCK(dvp, 0, td);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (EJUSTRETURN);
	}
	return (ENOENT);


found:

	if ((cnp->cn_nameiop == DELETE) && (flags & ISLASTCN)) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, td);
		if (error)
			return (error);
		if (*vpp == dvp) {
			VREF(dvp);
			*vpp = dvp;
			return (0);
		}
		error = devfs_allocv(de, dvp->v_mount, vpp, td);
		if (error)
			return (error);
		if (!(flags & LOCKPARENT)) {
			VOP_UNLOCK(dvp, 0, td);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return (0);
	}
	error = devfs_allocv(de, dvp->v_mount, vpp, td);
	if (error)
		return (error);
	if (!(flags & LOCKPARENT) || !(flags & ISLASTCN)) {
		VOP_UNLOCK(dvp, 0, td);
		cnp->cn_flags |= PDIRUNLOCK;
	}
	return (0);
}

static int
devfs_lookup(struct vop_lookup_args *ap)
{
	int j;
	struct devfs_mount *dmp;

	dmp = VFSTODEVFS(ap->a_dvp->v_mount);
	lockmgr(&dmp->dm_lock, LK_SHARED, 0, curthread);
	j = devfs_lookupx(ap);
	lockmgr(&dmp->dm_lock, LK_RELEASE, 0, curthread);
	return (j);
}

static int
devfs_mknod(struct vop_mknod_args *ap)
/*
struct vop_mknod_args {
        struct vnodeop_desc *a_desc;
        struct vnode *a_dvp;
        struct vnode **a_vpp;
        struct componentname *a_cnp;
        struct vattr *a_vap;
};
*/
{
	struct componentname *cnp;
	struct vnode *dvp, **vpp;
	struct thread *td;
	struct devfs_dirent *dd, *de;
	struct devfs_mount *dmp;
	int cloned, flags, nameiop;
	int error;

	dvp = ap->a_dvp;
	dmp = VFSTODEVFS(dvp->v_mount);
	lockmgr(&dmp->dm_lock, LK_EXCLUSIVE, 0, curthread);

	cnp = ap->a_cnp;
	vpp = ap->a_vpp;
	td = cnp->cn_thread;
	flags = cnp->cn_flags;
	nameiop = cnp->cn_nameiop;
	cloned = 0;
	dd = dvp->v_data;
	
	error = ENOENT;
	TAILQ_FOREACH(de, &dd->de_dlist, de_list) {
		if (cnp->cn_namelen != de->de_dirent->d_namlen)
			continue;
		if (bcmp(cnp->cn_nameptr, de->de_dirent->d_name,
		    de->de_dirent->d_namlen) != 0)
			continue;
		if (de->de_flags & DE_WHITEOUT)
			break;
		goto notfound;
	}
	if (de == NULL)
		goto notfound;
	de->de_flags &= ~DE_WHITEOUT;
	error = devfs_allocv(de, dvp->v_mount, vpp, td);
notfound:
	lockmgr(&dmp->dm_lock, LK_RELEASE, 0, curthread);
	return (error);
}


static int
devfs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _POSIX_MAC_PRESENT:
#ifdef MAC
		/*
		 * If MAC is enabled, devfs automatically supports
		 * trivial non-persistant label storage.
		 */
		*ap->a_retval = 1;
#else
		*ap->a_retval = 0;
#endif /* MAC */
		return (0);
	default:
		return (vop_stdpathconf(ap));
	}
	/* NOTREACHED */
}

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

static int
devfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{

	if (ap->a_vp->v_type != VDIR)
		return (EINVAL);
	return (VOP_READDIR(ap->a_vp, ap->a_uio, ap->a_cred, NULL, NULL, NULL));
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
	int error;
	struct uio *uio;
	struct dirent *dp;
	struct devfs_dirent *dd;
	struct devfs_dirent *de;
	struct devfs_mount *dmp;
	off_t off, oldoff;
	int ncookies = 0;
	u_long *cookiebuf, *cookiep;
	struct dirent *dps, *dpe;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	uio = ap->a_uio;
	if (uio->uio_offset < 0)
		return (EINVAL);

	dmp = VFSTODEVFS(ap->a_vp->v_mount);
	lockmgr(&dmp->dm_lock, LK_SHARED, 0, curthread);
	devfs_populate(dmp);
	error = 0;
	de = ap->a_vp->v_data;
	off = 0;
	oldoff = uio->uio_offset;
	TAILQ_FOREACH(dd, &de->de_dlist, de_list) {
		if (dd->de_flags & DE_WHITEOUT) 
			continue;
		if (dd->de_dirent->d_type == DT_DIR)
			de = dd->de_dir;
		else
			de = dd;
		dp = dd->de_dirent;
		if (dp->d_reclen > uio->uio_resid)
			break;
		dp->d_fileno = de->de_inode;
		if (off >= uio->uio_offset) {
			ncookies++;
			error = uiomove((caddr_t)dp, dp->d_reclen, uio);
			if (error)
				break;
		}
		off += dp->d_reclen;
	}
	if( !error && ap->a_ncookies != NULL && ap->a_cookies != NULL ) {
		MALLOC(cookiebuf, u_long *, ncookies * sizeof(u_long),
                       M_TEMP, M_WAITOK);
		cookiep = cookiebuf;
		dps = (struct dirent *) 
			(uio->uio_iov->iov_base - (uio->uio_offset - oldoff));
		dpe = (struct dirent *) uio->uio_iov->iov_base;
		for( dp = dps; 
			dp < dpe; 
			dp = (struct dirent *)((caddr_t) dp + dp->d_reclen)) {
				oldoff += dp->d_reclen;
				*cookiep++ = (u_long) oldoff;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookiebuf;
    }
	lockmgr(&dmp->dm_lock, LK_RELEASE, 0, curthread);
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
	error = uiomove(de->de_symlink, strlen(de->de_symlink), ap->a_uio);
	return (error);
}

static int
devfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dirent *de;
	int i;

	de = vp->v_data;
	if (de != NULL)
		de->de_vnode = NULL;
	vp->v_data = NULL;
	if (vp->v_rdev != NODEV && vp->v_rdev != NULL) {
		i = vcount(vp);
		if ((vp->v_rdev->si_flags & SI_CHEAPCLONE) && i == 0 &&
		    (vp->v_rdev->si_flags & SI_NAMED))
			destroy_dev(vp->v_rdev);
	}
	return (0);
}

#ifdef MAC
static int
devfs_refreshlabel(ap)
	struct vop_refreshlabel_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
	} */ *ap;
{

	/* Labels are always in sync. */
	return (0);
}
#endif

static int
devfs_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dirent *dd;
	struct devfs_dirent *de;
	struct devfs_mount *dmp = VFSTODEVFS(vp->v_mount);

	lockmgr(&dmp->dm_lock, LK_EXCLUSIVE, 0, curthread);
	dd = ap->a_dvp->v_data;
	de = vp->v_data;
	if (de->de_dirent->d_type == DT_LNK) {
		TAILQ_REMOVE(&dd->de_dlist, de, de_list);
		if (de->de_vnode)
			de->de_vnode->v_data = NULL;
#ifdef MAC
		mac_destroy_devfsdirent(de);
#endif
		FREE(de, M_DEVFS);
	} else {
		de->de_flags |= DE_WHITEOUT;
	}
	lockmgr(&dmp->dm_lock, LK_RELEASE, 0, curthread);
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
	de->de_vnode = NULL;
	vop_revoke(ap);
	return (0);
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
	struct devfs_dirent *de;
	struct vattr *vap;
	struct vnode *vp;
	int c, error;
	uid_t uid;
	gid_t gid;

	vap = ap->a_vap;
	vp = ap->a_vp;
	if ((vap->va_type != VNON) ||
	    (vap->va_nlink != VNOVAL) ||
	    (vap->va_fsid != VNOVAL) ||
	    (vap->va_fileid != VNOVAL) ||
	    (vap->va_blocksize != VNOVAL) ||
	    (vap->va_flags != VNOVAL && vap->va_flags != 0) ||
	    (vap->va_rdev != VNOVAL) ||
	    ((int)vap->va_bytes != VNOVAL) ||
	    (vap->va_gen != VNOVAL)) {
		return (EINVAL);
	}

	de = vp->v_data;
	if (vp->v_type == VDIR)
		de = de->de_dir;

	error = c = 0;
	if (vap->va_uid == (uid_t)VNOVAL)
		uid = de->de_uid;
	else
		uid = vap->va_uid;
	if (vap->va_gid == (gid_t)VNOVAL)
		gid = de->de_gid;
	else
		gid = vap->va_gid;
	if (uid != de->de_uid || gid != de->de_gid) {
		if (((ap->a_cred->cr_uid != de->de_uid) || uid != de->de_uid ||
		    (gid != de->de_gid && !groupmember(gid, ap->a_cred))) &&
		    (error = suser_cred(ap->a_td->td_ucred, PRISON_ROOT)) != 0)
			return (error);
		de->de_uid = uid;
		de->de_gid = gid;
		c = 1;
	}

	if (vap->va_mode != (mode_t)VNOVAL) {
		if ((ap->a_cred->cr_uid != de->de_uid) &&
		    (error = suser_cred(ap->a_td->td_ucred, PRISON_ROOT)))
			return (error);
		de->de_mode = vap->va_mode;
		c = 1;
	}

	if (vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL) {
		/* See the comment in ufs_vnops::ufs_setattr(). */
		if ((error = VOP_ACCESS(vp, VADMIN, ap->a_cred, ap->a_td)) &&
		    ((vap->va_vaflags & VA_UTIMES_NULL) == 0 ||
		    (error = VOP_ACCESS(vp, VWRITE, ap->a_cred, ap->a_td))))
			return (error);
		if (vap->va_atime.tv_sec != VNOVAL) {
			if (vp->v_type == VCHR)
				vp->v_rdev->si_atime = vap->va_atime;
			else
				de->de_atime = vap->va_atime;
		}
		if (vap->va_mtime.tv_sec != VNOVAL) {
			if (vp->v_type == VCHR)
				vp->v_rdev->si_mtime = vap->va_mtime;
			else
				de->de_mtime = vap->va_mtime;
		}
		c = 1;
	}

	if (c) {
		if (vp->v_type == VCHR)
			vfs_timestamp(&vp->v_rdev->si_ctime);
		else
			vfs_timestamp(&de->de_mtime);
	}
	return (0);
}

#ifdef MAC
static int
devfs_setlabel(ap)
	struct vop_setlabel_args /* {
		struct vnode *a_vp;
		struct mac *a_label;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp;
	struct devfs_dirent *de;

	vp = ap->a_vp;
	de = vp->v_data;

	mac_relabel_vnode(ap->a_cred, vp, ap->a_label);
	mac_update_devfsdirent(de, vp);

	return (0);
}
#endif

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
	int i, error;
	struct devfs_dirent *dd;
	struct devfs_dirent *de;
	struct devfs_mount *dmp;

	error = suser(ap->a_cnp->cn_thread);
	if (error)
		return(error);
	dmp = VFSTODEVFS(ap->a_dvp->v_mount);
	dd = ap->a_dvp->v_data;
	de = devfs_newdirent(ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen);
	de->de_uid = 0;
	de->de_gid = 0;
	de->de_mode = 0755;
	de->de_inode = dmp->dm_inode++;
	de->de_dirent->d_type = DT_LNK;
	i = strlen(ap->a_target) + 1;
	MALLOC(de->de_symlink, char *, i, M_DEVFS, M_WAITOK);
	bcopy(ap->a_target, de->de_symlink, i);
	lockmgr(&dmp->dm_lock, LK_EXCLUSIVE, 0, curthread);
	TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
	devfs_allocv(de, ap->a_dvp->v_mount, ap->a_vpp, 0);
#ifdef MAC
	mac_create_vnode(ap->a_cnp->cn_cred, ap->a_dvp, *ap->a_vpp);
	mac_update_devfsdirent(de, *ap->a_vpp);
#endif /* MAC */
	lockmgr(&dmp->dm_lock, LK_RELEASE, 0, curthread);
	return (0);
}

static vop_t **devfs_vnodeop_p;
static struct vnodeopv_entry_desc devfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) devfs_access },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_ioctl_desc,		(vop_t *) devfs_ioctl },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_lookup_desc,		(vop_t *) devfs_lookup },
	{ &vop_mknod_desc,		(vop_t *) devfs_mknod },
	{ &vop_pathconf_desc,		(vop_t *) devfs_pathconf },
	{ &vop_print_desc,		(vop_t *) devfs_print },
	{ &vop_read_desc,		(vop_t *) devfs_read },
	{ &vop_readdir_desc,		(vop_t *) devfs_readdir },
	{ &vop_readlink_desc,		(vop_t *) devfs_readlink },
	{ &vop_reclaim_desc,		(vop_t *) devfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) devfs_remove },
#ifdef MAC
	{ &vop_refreshlabel_desc,	(vop_t *) devfs_refreshlabel },
#endif
	{ &vop_revoke_desc,		(vop_t *) devfs_revoke },
	{ &vop_setattr_desc,		(vop_t *) devfs_setattr },
#ifdef MAC
	{ &vop_setlabel_desc,		(vop_t *) devfs_setlabel },
#endif
	{ &vop_symlink_desc,		(vop_t *) devfs_symlink },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ NULL, NULL }
};
static struct vnodeopv_desc devfs_vnodeop_opv_desc =
	{ &devfs_vnodeop_p, devfs_vnodeop_entries };

VNODEOP_SET(devfs_vnodeop_opv_desc);

static vop_t **devfs_specop_p;
static struct vnodeopv_entry_desc devfs_specop_entries[] = {
	{ &vop_default_desc,		(vop_t *) spec_vnoperate },
	{ &vop_access_desc,		(vop_t *) devfs_access },
	{ &vop_getattr_desc,		(vop_t *) devfs_getattr },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_pathconf_desc,		(vop_t *) devfs_pathconf },
	{ &vop_print_desc,		(vop_t *) devfs_print },
	{ &vop_reclaim_desc,		(vop_t *) devfs_reclaim },
#ifdef MAC
	{ &vop_refreshlabel_desc,	(vop_t *) devfs_refreshlabel },
#endif
	{ &vop_remove_desc,		(vop_t *) devfs_remove },
	{ &vop_revoke_desc,		(vop_t *) devfs_revoke },
	{ &vop_setattr_desc,		(vop_t *) devfs_setattr },
#ifdef MAC
	{ &vop_setlabel_desc,		(vop_t *) devfs_setlabel },
#endif
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ NULL, NULL }
};
static struct vnodeopv_desc devfs_specop_opv_desc =
	{ &devfs_specop_p, devfs_specop_entries };

VNODEOP_SET(devfs_specop_opv_desc);
#endif
