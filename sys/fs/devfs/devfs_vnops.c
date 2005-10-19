/*-
 * Copyright (c) 2000-2004
 *	Poul-Henning Kamp.  All rights reserved.
 * Copyright (c) 1989, 1992-1993, 1995
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
 *	mkdir: want it ?
 */

#include <opt_devfs.h>
#include <opt_mac.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

static struct vop_vector devfs_vnodeops;
static struct vop_vector devfs_specops;
static struct fileops devfs_ops_f;

#include <fs/devfs/devfs.h>
#include <fs/devfs/devfs_int.h>

static int
devfs_fp_check(struct file *fp, struct cdev **devp, struct cdevsw **dswp)
{

	*devp = fp->f_vnode->v_rdev;
	if (*devp != fp->f_data)
		return (ENXIO);
	KASSERT((*devp)->si_refcount > 0,
	    ("devfs: un-referenced struct cdev *(%s)", devtoname(*devp)));
	*dswp = dev_refthread(*devp);
	if (*dswp == NULL)
		return (ENXIO);
	return (0);
}

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
	while (de != dmp->dm_rootdir) {
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
	struct cdev *dev;

	KASSERT(td == curthread, ("devfs_allocv: td != curthread"));
loop:
	vp = de->de_vnode;
	if (vp != NULL) {
		if (vget(vp, LK_EXCLUSIVE, td))
			goto loop;
		*vpp = vp;
		return (0);
	}
	if (de->de_dirent->d_type == DT_CHR) {
		if (!(de->de_cdp->cdp_flags & CDP_ACTIVE))
			return (ENOENT);
		dev = &de->de_cdp->cdp_c;
	} else {
		dev = NULL;
	}
	error = getnewvnode("devfs", mp, &devfs_vnodeops, &vp);
	if (error != 0) {
		printf("devfs_allocv: failed to allocate new vnode\n");
		return (error);
	}

	if (de->de_dirent->d_type == DT_CHR) {
		vp->v_type = VCHR;
		VI_LOCK(vp);
		dev_lock();
		dev_refl(dev);
		vp->v_rdev = dev;
		KASSERT(vp->v_usecount == 1,
		    ("%s %d (%d)\n", __func__, __LINE__, vp->v_usecount));
		dev->si_usecount += vp->v_usecount;
		dev_unlock();
		VI_UNLOCK(vp);
		vp->v_op = &devfs_specops;
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
	mac_associate_vnode_devfs(mp, de, vp);
#endif
	*vpp = vp;
	return (0);
}

static int
devfs_access(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dirent *de;
	int error;

	de = vp->v_data;
	if (vp->v_type == VDIR)
		de = de->de_dir;

	error = vaccess(vp->v_type, de->de_mode, de->de_uid, de->de_gid,
	    ap->a_mode, ap->a_cred, NULL);
	if (!error)
		return (error);
	if (error != EACCES)
		return (error);
	/* We do, however, allow access to the controlling terminal */
	if (!(ap->a_td->td_proc->p_flag & P_CONTROLT))
		return (error);
	if (ap->a_td->td_proc->p_session->s_ttyvp == de->de_vnode)
		return (0);
	return (error);
}

/* ARGSUSED */
static int
devfs_advlock(struct vop_advlock_args *ap)
{

	return (ap->a_flags & F_FLOCK ? EOPNOTSUPP : EINVAL);
}

/* ARGSUSED */
static int
devfs_close(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp, *oldvp;
	struct thread *td = ap->a_td;
	struct cdev *dev = vp->v_rdev;
	struct cdevsw *dsw;
	int error;

	/*
	 * Hack: a tty device that is a controlling terminal
	 * has a reference from the session structure.
	 * We cannot easily tell that a character device is
	 * a controlling terminal, unless it is the closing
	 * process' controlling terminal.  In that case,
	 * if the reference count is 2 (this last descriptor
	 * plus the session), release the reference from the session.
	 */

	/*
	 * This needs to be rewritten to take the vp interlock into
	 * consideration.
	 */

	oldvp = NULL;
	sx_xlock(&proctree_lock);
	if (td && vp == td->td_proc->p_session->s_ttyvp) {
		SESS_LOCK(td->td_proc->p_session);
		VI_LOCK(vp);
		if (count_dev(dev) == 2 && (vp->v_iflag & VI_DOOMED) == 0) {
			td->td_proc->p_session->s_ttyvp = NULL;
			oldvp = vp;
		}
		VI_UNLOCK(vp);
		SESS_UNLOCK(td->td_proc->p_session);
	}
	sx_xunlock(&proctree_lock);
	if (oldvp != NULL)
		vrele(oldvp);
	/*
	 * We do not want to really close the device if it
	 * is still in use unless we are trying to close it
	 * forcibly. Since every use (buffer, vnode, swap, cmap)
	 * holds a reference to the vnode, and because we mark
	 * any other vnodes that alias this device, when the
	 * sum of the reference counts on all the aliased
	 * vnodes descends to one, we are on last close.
	 */
	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);
	VI_LOCK(vp);
	if (vp->v_iflag & VI_DOOMED) {
		/* Forced close. */
	} else if (dsw->d_flags & D_TRACKCLOSE) {
		/* Keep device updated on status. */
	} else if (count_dev(dev) > 1) {
		VI_UNLOCK(vp);
		dev_relthread(dev);
		return (0);
	}
	VI_UNLOCK(vp);
	KASSERT(dev->si_refcount > 0,
	    ("devfs_close() on un-referenced struct cdev *(%s)", devtoname(dev)));
	if (!(dsw->d_flags & D_NEEDGIANT)) {
		DROP_GIANT();
		error = dsw->d_close(dev, ap->a_fflag, S_IFCHR, td);
		PICKUP_GIANT();
	} else {
		error = dsw->d_close(dev, ap->a_fflag, S_IFCHR, td);
	}
	dev_relthread(dev);
	return (error);
}

static int
devfs_close_f(struct file *fp, struct thread *td)
{

	return (vnops.fo_close(fp, td));
}

/* ARGSUSED */
static int
devfs_fsync(struct vop_fsync_args *ap)
{
	if (!vn_isdisk(ap->a_vp, NULL))
		return (0);

	return (vop_stdfsync(ap));
}

static int
devfs_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	int error = 0;
	struct devfs_dirent *de;
	struct cdev *dev;

	de = vp->v_data;
	KASSERT(de != NULL, ("Null dirent in devfs_getattr vp=%p", vp));
	if (vp->v_type == VDIR) {
		de = de->de_dir;
		KASSERT(de != NULL,
		    ("Null dir dirent in devfs_getattr vp=%p", vp));
	}
	bzero((caddr_t) vap, sizeof(*vap));
	vattr_null(vap);
	vap->va_uid = de->de_uid;
	vap->va_gid = de->de_gid;
	vap->va_mode = de->de_mode;
	if (vp->v_type == VLNK)
		vap->va_size = strlen(de->de_symlink);
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

		vap->va_rdev = dev->si_priv->cdp_inode;
	}
	vap->va_gen = 0;
	vap->va_flags = 0;
	vap->va_nlink = de->de_links;
	vap->va_fileid = de->de_inode;

	return (error);
}

/* ARGSUSED */
static int
devfs_ioctl_f(struct file *fp, u_long com, void *data, struct ucred *cred, struct thread *td)
{
	struct cdev *dev;
	struct cdevsw *dsw;
	struct vnode *vp;
	struct vnode *vpold;
	int error, i;
	const char *p;
	struct fiodgname_arg *fgn;

	error = devfs_fp_check(fp, &dev, &dsw);
	if (error)
		return (error);

	if (com == FIODTYPE) {
		*(int *)data = dsw->d_flags & D_TYPEMASK;
		dev_relthread(dev);
		return (0);
	} else if (com == FIODGNAME) {
		fgn = data;
		p = devtoname(dev);
		i = strlen(p) + 1;
		if (i > fgn->len)
			error = EINVAL;
		else
			error = copyout(p, fgn->buf, i);
		dev_relthread(dev);
		return (error);
	}
	error = dsw->d_ioctl(dev, com, data, fp->f_flag, td);
	dev_relthread(dev);
	if (error == ENOIOCTL)
		error = ENOTTY;
	if (error == 0 && com == TIOCSCTTY) {
		vp = fp->f_vnode;

		/* Do nothing if reassigning same control tty */
		sx_slock(&proctree_lock);
		if (td->td_proc->p_session->s_ttyvp == vp) {
			sx_sunlock(&proctree_lock);
			return (0);
		}

		mtx_lock(&Giant);

		vpold = td->td_proc->p_session->s_ttyvp;
		VREF(vp);
		SESS_LOCK(td->td_proc->p_session);
		td->td_proc->p_session->s_ttyvp = vp;
		SESS_UNLOCK(td->td_proc->p_session);

		sx_sunlock(&proctree_lock);

		/* Get rid of reference to old control tty */
		if (vpold)
			vrele(vpold);
		mtx_unlock(&Giant);
	}
	return (error);
}

/* ARGSUSED */
static int
devfs_kqfilter_f(struct file *fp, struct knote *kn)
{
	struct cdev *dev;
	struct cdevsw *dsw;
	int error;

	error = devfs_fp_check(fp, &dev, &dsw);
	if (error)
		return (error);
	error = dsw->d_kqfilter(dev, kn);
	dev_relthread(dev);
	return (error);
}

static int
devfs_lookupx(struct vop_lookup_args *ap)
{
	struct componentname *cnp;
	struct vnode *dvp, **vpp;
	struct thread *td;
	struct devfs_dirent *de, *dd;
	struct devfs_dirent **dde;
	struct devfs_mount *dmp;
	struct cdev *cdev;
	int error, flags, nameiop;
	char specname[SPECNAMELEN + 1], *pname;

	cnp = ap->a_cnp;
	vpp = ap->a_vpp;
	dvp = ap->a_dvp;
	pname = cnp->cn_nameptr;
	td = cnp->cn_thread;
	flags = cnp->cn_flags;
	nameiop = cnp->cn_nameiop;
	dmp = VFSTODEVFS(dvp->v_mount);
	dd = dvp->v_data;
	*vpp = NULLVP;

	if ((flags & ISLASTCN) && nameiop == RENAME)
		return (EOPNOTSUPP);

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	if ((flags & ISDOTDOT) && (dvp->v_vflag & VV_ROOT))
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
		de = TAILQ_FIRST(&dd->de_dlist);	/* "." */
		de = TAILQ_NEXT(de, de_list);		/* ".." */
		de = de->de_dir;
		error = devfs_allocv(de, dvp->v_mount, vpp, td);
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, td);
		return (error);
	}

	devfs_populate(dmp);
	dd = dvp->v_data;
	de = devfs_find(dd, cnp->cn_nameptr, cnp->cn_namelen);
	while (de == NULL) {	/* While(...) so we can use break */

		if (nameiop == DELETE)
			return (ENOENT);

		/*
		 * OK, we didn't have an entry for the name we were asked for
		 * so we try to see if anybody can create it on demand.
		 */
		pname = devfs_fqpn(specname, dvp, cnp);
		if (pname == NULL)
			break;

		cdev = NULL;
		EVENTHANDLER_INVOKE(dev_clone,
		    td->td_ucred, pname, strlen(pname), &cdev);
		if (cdev == NULL)
			break;

		devfs_populate(dmp);

		dev_lock();
		dde = &cdev->si_priv->cdp_dirents[dmp->dm_idx];
		if (dde != NULL && *dde != NULL)
			de = *dde;
		dev_unlock();
		dev_rel(cdev);
		break;
	}

	if (de == NULL || de->de_flags & DE_WHITEOUT) {
		if ((nameiop == CREATE || nameiop == RENAME) &&
		    (flags & (LOCKPARENT | WANTPARENT)) && (flags & ISLASTCN)) {
			cnp->cn_flags |= SAVENAME;
			return (EJUSTRETURN);
		}
		return (ENOENT);
	}

	if ((cnp->cn_nameiop == DELETE) && (flags & ISLASTCN)) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, td);
		if (error)
			return (error);
		if (*vpp == dvp) {
			VREF(dvp);
			*vpp = dvp;
			return (0);
		}
	}
	error = devfs_allocv(de, dvp->v_mount, vpp, td);
	return (error);
}

static int
devfs_lookup(struct vop_lookup_args *ap)
{
	int j;
	struct devfs_mount *dmp;

	dmp = VFSTODEVFS(ap->a_dvp->v_mount);
	sx_xlock(&dmp->dm_lock);
	j = devfs_lookupx(ap);
	sx_xunlock(&dmp->dm_lock);
	return (j);
}

static int
devfs_mknod(struct vop_mknod_args *ap)
{
	struct componentname *cnp;
	struct vnode *dvp, **vpp;
	struct thread *td;
	struct devfs_dirent *dd, *de;
	struct devfs_mount *dmp;
	int error;

	/*
	 * The only type of node we should be creating here is a
	 * character device, for anything else return EOPNOTSUPP.
	 */
	if (ap->a_vap->va_type != VCHR)
		return (EOPNOTSUPP);
	dvp = ap->a_dvp;
	dmp = VFSTODEVFS(dvp->v_mount);
	sx_xlock(&dmp->dm_lock);

	cnp = ap->a_cnp;
	vpp = ap->a_vpp;
	td = cnp->cn_thread;
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
	sx_xunlock(&dmp->dm_lock);
	return (error);
}

/* ARGSUSED */
static int
devfs_open(struct vop_open_args *ap)
{
	struct thread *td = ap->a_td;
	struct vnode *vp = ap->a_vp;
	struct cdev *dev = vp->v_rdev;
	struct file *fp;
	int error;
	struct cdevsw *dsw;

	if (vp->v_type == VBLK)
		return (ENXIO);

	if (dev == NULL)
		return (ENXIO);

	/* Make this field valid before any I/O in d_open. */
	if (dev->si_iosize_max == 0)
		dev->si_iosize_max = DFLTPHYS;

	if (vn_isdisk(vp, NULL) &&
	    ap->a_cred != FSCRED && (ap->a_mode & FWRITE)) {
		/*
		* When running in very secure mode, do not allow
		* opens for writing of any disks.
		* XXX: should be in geom_dev.c, but we lack the cred there.
		*/
		error = securelevel_ge(td->td_ucred, 2);
		if (error)
			return (error);
	}

	dsw = dev_refthread(dev);
	if (dsw == NULL)
		return (ENXIO);

	/* XXX: Special casing of ttys for deadfs.  Probably redundant. */
	if (dsw->d_flags & D_TTY)
		vp->v_vflag |= VV_ISTTY;

	VOP_UNLOCK(vp, 0, td);

	if(!(dsw->d_flags & D_NEEDGIANT)) {
		DROP_GIANT();
		if (dsw->d_fdopen != NULL)
			error = dsw->d_fdopen(dev, ap->a_mode, td, ap->a_fdidx);
		else
			error = dsw->d_open(dev, ap->a_mode, S_IFCHR, td);
		PICKUP_GIANT();
	} else {
		if (dsw->d_fdopen != NULL)
			error = dsw->d_fdopen(dev, ap->a_mode, td, ap->a_fdidx);
		else
			error = dsw->d_open(dev, ap->a_mode, S_IFCHR, td);
	}

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, td);

	dev_relthread(dev);

	if (error)
		return (error);

#if 0	/* /dev/console */
	KASSERT(ap->a_fdidx >= 0,
	     ("Could not vnode bypass device on fd %d", ap->a_fdidx));
#else
	if(ap->a_fdidx < 0)
		return (error);
#endif
	/*
	 * This is a pretty disgustingly long chain, but I am not
	 * sure there is any better way.  Passing the fdidx into
	 * VOP_OPEN() offers us more information than just passing
	 * the file *.
	 */
	fp = ap->a_td->td_proc->p_fd->fd_ofiles[ap->a_fdidx];
	KASSERT(fp->f_ops == &badfileops,
	     ("Could not vnode bypass device on fdops %p", fp->f_ops));
	fp->f_ops = &devfs_ops_f;
	fp->f_data = dev;
	return (error);
}

static int
devfs_pathconf(struct vop_pathconf_args *ap)
{

	switch (ap->a_name) {
	case _PC_MAC_PRESENT:
#ifdef MAC
		/*
		 * If MAC is enabled, devfs automatically supports
		 * trivial non-persistant label storage.
		 */
		*ap->a_retval = 1;
#else
		*ap->a_retval = 0;
#endif
		return (0);
	default:
		return (vop_stdpathconf(ap));
	}
	/* NOTREACHED */
}

/* ARGSUSED */
static int
devfs_poll_f(struct file *fp, int events, struct ucred *cred, struct thread *td)
{
	struct cdev *dev;
	struct cdevsw *dsw;
	int error;

	error = devfs_fp_check(fp, &dev, &dsw);
	if (error)
		return (error);
	error = dsw->d_poll(dev, events, td);
	dev_relthread(dev);
	return(error);
}

/*
 * Print out the contents of a special device vnode.
 */
static int
devfs_print(struct vop_print_args *ap)
{

	printf("\tdev %s\n", devtoname(ap->a_vp->v_rdev));
	return (0);
}

/* ARGSUSED */
static int
devfs_read_f(struct file *fp, struct uio *uio, struct ucred *cred, int flags, struct thread *td)
{
	struct cdev *dev;
	int ioflag, error, resid;
	struct cdevsw *dsw;

	error = devfs_fp_check(fp, &dev, &dsw);
	if (error)
		return (error);
	resid = uio->uio_resid;
	ioflag = fp->f_flag & (O_NONBLOCK | O_DIRECT);
	if (ioflag & O_DIRECT)
		ioflag |= IO_DIRECT;

	if ((flags & FOF_OFFSET) == 0)
		uio->uio_offset = fp->f_offset;

	error = dsw->d_read(dev, uio, ioflag);
	dev_relthread(dev);
	if (uio->uio_resid != resid || (error == 0 && resid != 0))
		vfs_timestamp(&dev->si_atime);

	if ((flags & FOF_OFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	return (error);
}

static int
devfs_readdir(struct vop_readdir_args *ap)
{
	int error;
	struct uio *uio;
	struct dirent *dp;
	struct devfs_dirent *dd;
	struct devfs_dirent *de;
	struct devfs_mount *dmp;
	off_t off, oldoff;

	if (ap->a_vp->v_type != VDIR)
		return (ENOTDIR);

	uio = ap->a_uio;
	if (uio->uio_offset < 0)
		return (EINVAL);

	dmp = VFSTODEVFS(ap->a_vp->v_mount);
	sx_xlock(&dmp->dm_lock);
	devfs_populate(dmp);
	error = 0;
	de = ap->a_vp->v_data;
	off = 0;
	oldoff = uio->uio_offset;
	TAILQ_FOREACH(dd, &de->de_dlist, de_list) {
		KASSERT(dd->de_cdp != (void *)0xdeadc0de, ("%s %d\n", __func__, __LINE__));
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
			error = vfs_read_dirent(ap, dp, off);
			if (error)
				break;
		}
		off += dp->d_reclen;
	}
	sx_xunlock(&dmp->dm_lock);
	uio->uio_offset = off;
	return (error);
}

static int
devfs_readlink(struct vop_readlink_args *ap)
{
	struct devfs_dirent *de;

	de = ap->a_vp->v_data;
	return (uiomove(de->de_symlink, strlen(de->de_symlink), ap->a_uio));
}

static int
devfs_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dirent *de;
	struct cdev *dev;

	de = vp->v_data;
	if (de != NULL)
		de->de_vnode = NULL;
	vp->v_data = NULL;
	vnode_destroy_vobject(vp);

	dev = vp->v_rdev;
	vp->v_rdev = NULL;

	if (dev == NULL)
		return (0);

	dev_lock();
	dev->si_usecount -= vp->v_usecount;
	dev_unlock();
	dev_rel(dev);
	return (0);
}

static int
devfs_remove(struct vop_remove_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct devfs_dirent *dd;
	struct devfs_dirent *de;
	struct devfs_mount *dmp = VFSTODEVFS(vp->v_mount);

	sx_xlock(&dmp->dm_lock);
	dd = ap->a_dvp->v_data;
	de = vp->v_data;
	if (de->de_cdp == NULL) {
		TAILQ_REMOVE(&dd->de_dlist, de, de_list);
		devfs_delete(dmp, de);
	} else {
		de->de_flags |= DE_WHITEOUT;
	}
	sx_xunlock(&dmp->dm_lock);
	return (0);
}

/*
 * Revoke is called on a tty when a terminal session ends.  The vnode
 * is orphaned by setting v_op to deadfs so we need to let go of it
 * as well so that we create a new one next time around.
 *
 * XXX: locking :-(
 * XXX: We mess around with other mountpoints without holding their sxlock.
 * XXX: We hold the devlock() when we zero their vnode pointer, but is that
 * XXX: enough ?
 */
static int
devfs_revoke(struct vop_revoke_args *ap)
{
	struct vnode *vp = ap->a_vp, *vp2;
	struct cdev *dev;
	struct cdev_priv *cdp;
	struct devfs_dirent *de;
	int i;

	KASSERT((ap->a_flags & REVOKEALL) != 0, ("devfs_revoke !REVOKEALL"));

	dev = vp->v_rdev;
	cdp = dev->si_priv;
	for (;;) {
		dev_lock();
		vp2 = NULL;
		for (i = 0; i <= cdp->cdp_maxdirent; i++) {
			de = cdp->cdp_dirents[i];
			if (de == NULL)
				continue;
			vp2 = de->de_vnode;
			de->de_vnode = NULL;
			if (vp2 != NULL)
				break;
		}
		dev_unlock();
		if (vp2 != NULL) {
			vgone(vp2);
			continue;
		}
		break;
	}
	return (0);
}

static int
devfs_rioctl(struct vop_ioctl_args *ap)
{
	int error;
	struct devfs_mount *dmp;

	dmp = VFSTODEVFS(ap->a_vp->v_mount);
	sx_xlock(&dmp->dm_lock);
	devfs_populate(dmp);
	error = devfs_rules_ioctl(dmp, ap->a_command, ap->a_data, ap->a_td);
	sx_xunlock(&dmp->dm_lock);
	return (error);
}

static int
devfs_rread(struct vop_read_args *ap)
{

	if (ap->a_vp->v_type != VDIR)
		return (EINVAL);
	return (VOP_READDIR(ap->a_vp, ap->a_uio, ap->a_cred, NULL, NULL, NULL));
}

static int
devfs_setattr(struct vop_setattr_args *ap)
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
		    (error = suser_cred(ap->a_td->td_ucred, SUSER_ALLOWJAIL)) != 0)
			return (error);
		de->de_uid = uid;
		de->de_gid = gid;
		c = 1;
	}

	if (vap->va_mode != (mode_t)VNOVAL) {
		if ((ap->a_cred->cr_uid != de->de_uid) &&
		    (error = suser_cred(ap->a_td->td_ucred, SUSER_ALLOWJAIL)))
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
devfs_setlabel(struct vop_setlabel_args *ap)
{
	struct vnode *vp;
	struct devfs_dirent *de;

	vp = ap->a_vp;
	de = vp->v_data;

	mac_relabel_vnode(ap->a_cred, vp, ap->a_label);
	mac_update_devfsdirent(vp->v_mount, de, vp);

	return (0);
}
#endif

static int
devfs_stat_f(struct file *fp, struct stat *sb, struct ucred *cred, struct thread *td)
{

	return (vnops.fo_stat(fp, sb, cred, td));
}

static int
devfs_symlink(struct vop_symlink_args *ap)
{
	int i, error;
	struct devfs_dirent *dd;
	struct devfs_dirent *de;
	struct devfs_mount *dmp;
	struct thread *td;

	td = ap->a_cnp->cn_thread;
	KASSERT(td == curthread, ("devfs_symlink: td != curthread"));
	error = suser(td);
	if (error)
		return(error);
	dmp = VFSTODEVFS(ap->a_dvp->v_mount);
	dd = ap->a_dvp->v_data;
	de = devfs_newdirent(ap->a_cnp->cn_nameptr, ap->a_cnp->cn_namelen);
	de->de_uid = 0;
	de->de_gid = 0;
	de->de_mode = 0755;
	de->de_inode = alloc_unr(devfs_inos);
	de->de_dirent->d_type = DT_LNK;
	i = strlen(ap->a_target) + 1;
	de->de_symlink = malloc(i, M_DEVFS, M_WAITOK);
	bcopy(ap->a_target, de->de_symlink, i);
	sx_xlock(&dmp->dm_lock);
#ifdef MAC
	mac_create_devfs_symlink(ap->a_cnp->cn_cred, dmp->dm_mount, dd, de);
#endif
	TAILQ_INSERT_TAIL(&dd->de_dlist, de, de_list);
	devfs_allocv(de, ap->a_dvp->v_mount, ap->a_vpp, td);
	sx_xunlock(&dmp->dm_lock);
	return (0);
}

/* ARGSUSED */
static int
devfs_write_f(struct file *fp, struct uio *uio, struct ucred *cred, int flags, struct thread *td)
{
	struct cdev *dev;
	int error, ioflag, resid;
	struct cdevsw *dsw;

	error = devfs_fp_check(fp, &dev, &dsw);
	if (error)
		return (error);
	KASSERT(uio->uio_td == td, ("uio_td %p is not td %p", uio->uio_td, td));
	ioflag = fp->f_flag & (O_NONBLOCK | O_DIRECT | O_FSYNC);
	if (ioflag & O_DIRECT)
		ioflag |= IO_DIRECT;
	if ((flags & FOF_OFFSET) == 0)
		uio->uio_offset = fp->f_offset;

	resid = uio->uio_resid;

	error = dsw->d_write(dev, uio, ioflag);
	dev_relthread(dev);
	if (uio->uio_resid != resid || (error == 0 && resid != 0)) {
		vfs_timestamp(&dev->si_ctime);
		dev->si_mtime = dev->si_ctime;
	}

	if ((flags & FOF_OFFSET) == 0)
		fp->f_offset = uio->uio_offset;
	fp->f_nextoff = uio->uio_offset;
	return (error);
}

dev_t
dev2udev(struct cdev *x)
{
	if (x == NULL)
		return (NODEV);
	return (x->si_priv->cdp_inode);
}

static struct fileops devfs_ops_f = {
	.fo_read =	devfs_read_f,
	.fo_write =	devfs_write_f,
	.fo_ioctl =	devfs_ioctl_f,
	.fo_poll =	devfs_poll_f,
	.fo_kqfilter =	devfs_kqfilter_f,
	.fo_stat =	devfs_stat_f,
	.fo_close =	devfs_close_f,
	.fo_flags =	DFLAG_PASSABLE | DFLAG_SEEKABLE
};

static struct vop_vector devfs_vnodeops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		devfs_access,
	.vop_getattr =		devfs_getattr,
	.vop_ioctl =		devfs_rioctl,
	.vop_lookup =		devfs_lookup,
	.vop_mknod =		devfs_mknod,
	.vop_pathconf =		devfs_pathconf,
	.vop_read =		devfs_rread,
	.vop_readdir =		devfs_readdir,
	.vop_readlink =		devfs_readlink,
	.vop_reclaim =		devfs_reclaim,
	.vop_remove =		devfs_remove,
	.vop_revoke =		devfs_revoke,
	.vop_setattr =		devfs_setattr,
#ifdef MAC
	.vop_setlabel =		devfs_setlabel,
#endif
	.vop_symlink =		devfs_symlink,
};

static struct vop_vector devfs_specops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		devfs_access,
	.vop_advlock =		devfs_advlock,
	.vop_bmap =		VOP_PANIC,
	.vop_close =		devfs_close,
	.vop_create =		VOP_PANIC,
	.vop_fsync =		devfs_fsync,
	.vop_getattr =		devfs_getattr,
	.vop_lease =		VOP_NULL,
	.vop_link =		VOP_PANIC,
	.vop_mkdir =		VOP_PANIC,
	.vop_mknod =		VOP_PANIC,
	.vop_open =		devfs_open,
	.vop_pathconf =		devfs_pathconf,
	.vop_print =		devfs_print,
	.vop_read =		VOP_PANIC,
	.vop_readdir =		VOP_PANIC,
	.vop_readlink =		VOP_PANIC,
	.vop_reallocblks =	VOP_PANIC,
	.vop_reclaim =		devfs_reclaim,
	.vop_remove =		devfs_remove,
	.vop_rename =		VOP_PANIC,
	.vop_revoke =		devfs_revoke,
	.vop_rmdir =		VOP_PANIC,
	.vop_setattr =		devfs_setattr,
#ifdef MAC
	.vop_setlabel =		devfs_setlabel,
#endif
	.vop_strategy =		VOP_PANIC,
	.vop_symlink =		VOP_PANIC,
	.vop_write =		VOP_PANIC,
};

/*
 * Our calling convention to the device drivers used to be that we passed
 * vnode.h IO_* flags to read()/write(), but we're moving to fcntl.h O_ 
 * flags instead since that's what open(), close() and ioctl() takes and
 * we don't really want vnode.h in device drivers.
 * We solved the source compatibility by redefining some vnode flags to
 * be the same as the fcntl ones and by sending down the bitwise OR of
 * the respective fcntl/vnode flags.  These CTASSERTS make sure nobody
 * pulls the rug out under this.
 */
CTASSERT(O_NONBLOCK == IO_NDELAY);
CTASSERT(O_FSYNC == IO_SYNC);
