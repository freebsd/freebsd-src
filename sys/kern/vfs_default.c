/*-
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed
 * to Berkeley by John Heidemann of the UCLA Ficus project.
 *
 * Source: * @(#)i405_init.c 2.10 92/04/27 UCLA Ficus project
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/event.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/lockf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/poll.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

static int	vop_nolookup(struct vop_lookup_args *);
static int	vop_norename(struct vop_rename_args *);
static int	vop_nostrategy(struct vop_strategy_args *);
static int	get_next_dirent(struct vnode *vp, struct dirent **dpp,
				char *dirbuf, int dirbuflen, off_t *off,
				char **cpos, int *len, int *eofflag,
				struct thread *td);
static int	dirent_exists(struct vnode *vp, const char *dirname,
			      struct thread *td);

#define DIRENT_MINSIZE (sizeof(struct dirent) - (MAXNAMLEN+1) + 4)

/*
 * This vnode table stores what we want to do if the filesystem doesn't
 * implement a particular VOP.
 *
 * If there is no specific entry here, we will return EOPNOTSUPP.
 *
 * Note that every filesystem has to implement either vop_access
 * or vop_accessx; failing to do so will result in immediate crash
 * due to stack overflow, as vop_stdaccess() calls vop_stdaccessx(),
 * which calls vop_stdaccess() etc.
 */

struct vop_vector default_vnodeops = {
	.vop_default =		NULL,
	.vop_bypass =		VOP_EOPNOTSUPP,

	.vop_access =		vop_stdaccess,
	.vop_accessx =		vop_stdaccessx,
	.vop_advlock =		vop_stdadvlock,
	.vop_advlockasync =	vop_stdadvlockasync,
	.vop_advlockpurge =	vop_stdadvlockpurge,
	.vop_bmap =		vop_stdbmap,
	.vop_close =		VOP_NULL,
	.vop_fsync =		VOP_NULL,
	.vop_getpages =		vop_stdgetpages,
	.vop_getwritemount = 	vop_stdgetwritemount,
	.vop_inactive =		VOP_NULL,
	.vop_ioctl =		VOP_ENOTTY,
	.vop_kqfilter =		vop_stdkqfilter,
	.vop_islocked =		vop_stdislocked,
	.vop_lock1 =		vop_stdlock,
	.vop_lookup =		vop_nolookup,
	.vop_open =		VOP_NULL,
	.vop_pathconf =		VOP_EINVAL,
	.vop_poll =		vop_nopoll,
	.vop_putpages =		vop_stdputpages,
	.vop_readlink =		VOP_EINVAL,
	.vop_rename =		vop_norename,
	.vop_revoke =		VOP_PANIC,
	.vop_strategy =		vop_nostrategy,
	.vop_unlock =		vop_stdunlock,
	.vop_vptocnp =		vop_stdvptocnp,
	.vop_vptofh =		vop_stdvptofh,
};

/*
 * Series of placeholder functions for various error returns for
 * VOPs.
 */

int
vop_eopnotsupp(struct vop_generic_args *ap)
{
	/*
	printf("vop_notsupp[%s]\n", ap->a_desc->vdesc_name);
	*/

	return (EOPNOTSUPP);
}

int
vop_ebadf(struct vop_generic_args *ap)
{

	return (EBADF);
}

int
vop_enotty(struct vop_generic_args *ap)
{

	return (ENOTTY);
}

int
vop_einval(struct vop_generic_args *ap)
{

	return (EINVAL);
}

int
vop_enoent(struct vop_generic_args *ap)
{

	return (ENOENT);
}

int
vop_null(struct vop_generic_args *ap)
{

	return (0);
}

/*
 * Helper function to panic on some bad VOPs in some filesystems.
 */
int
vop_panic(struct vop_generic_args *ap)
{

	panic("filesystem goof: vop_panic[%s]", ap->a_desc->vdesc_name);
}

/*
 * vop_std<something> and vop_no<something> are default functions for use by
 * filesystems that need the "default reasonable" implementation for a
 * particular operation.
 *
 * The documentation for the operations they implement exists (if it exists)
 * in the VOP_<SOMETHING>(9) manpage (all uppercase).
 */

/*
 * Default vop for filesystems that do not support name lookup
 */
static int
vop_nolookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{

	*ap->a_vpp = NULL;
	return (ENOTDIR);
}

/*
 * vop_norename:
 *
 * Handle unlock and reference counting for arguments of vop_rename
 * for filesystems that do not implement rename operation.
 */
static int
vop_norename(struct vop_rename_args *ap)
{

	vop_rename_fail(ap);
	return (EOPNOTSUPP);
}

/*
 *	vop_nostrategy:
 *
 *	Strategy routine for VFS devices that have none.
 *
 *	BIO_ERROR and B_INVAL must be cleared prior to calling any strategy
 *	routine.  Typically this is done for a BIO_READ strategy call.
 *	Typically B_INVAL is assumed to already be clear prior to a write
 *	and should not be cleared manually unless you just made the buffer
 *	invalid.  BIO_ERROR should be cleared either way.
 */

static int
vop_nostrategy (struct vop_strategy_args *ap)
{
	printf("No strategy for buffer at %p\n", ap->a_bp);
	vprint("vnode", ap->a_vp);
	ap->a_bp->b_ioflags |= BIO_ERROR;
	ap->a_bp->b_error = EOPNOTSUPP;
	bufdone(ap->a_bp);
	return (EOPNOTSUPP);
}

static int
get_next_dirent(struct vnode *vp, struct dirent **dpp, char *dirbuf,
		int dirbuflen, off_t *off, char **cpos, int *len,
		int *eofflag, struct thread *td)
{
	int error, reclen;
	struct uio uio;
	struct iovec iov;
	struct dirent *dp;

	KASSERT(VOP_ISLOCKED(vp), ("vp %p is not locked", vp));
	KASSERT(vp->v_type == VDIR, ("vp %p is not a directory", vp));

	if (*len == 0) {
		iov.iov_base = dirbuf;
		iov.iov_len = dirbuflen;

		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = *off;
		uio.uio_resid = dirbuflen;
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = UIO_READ;
		uio.uio_td = td;

		*eofflag = 0;

#ifdef MAC
		error = mac_vnode_check_readdir(td->td_ucred, vp);
		if (error == 0)
#endif
			error = VOP_READDIR(vp, &uio, td->td_ucred, eofflag,
		    		NULL, NULL);
		if (error)
			return (error);

		*off = uio.uio_offset;

		*cpos = dirbuf;
		*len = (dirbuflen - uio.uio_resid);
	}

	dp = (struct dirent *)(*cpos);
	reclen = dp->d_reclen;
	*dpp = dp;

	/* check for malformed directory.. */
	if (reclen < DIRENT_MINSIZE)
		return (EINVAL);

	*cpos += reclen;
	*len -= reclen;

	return (0);
}

/*
 * Check if a named file exists in a given directory vnode.
 */
static int
dirent_exists(struct vnode *vp, const char *dirname, struct thread *td)
{
	char *dirbuf, *cpos;
	int error, eofflag, dirbuflen, len, found;
	off_t off;
	struct dirent *dp;
	struct vattr va;

	KASSERT(VOP_ISLOCKED(vp), ("vp %p is not locked", vp));
	KASSERT(vp->v_type == VDIR, ("vp %p is not a directory", vp));

	found = 0;

	error = VOP_GETATTR(vp, &va, td->td_ucred);
	if (error)
		return (found);

	dirbuflen = DEV_BSIZE;
	if (dirbuflen < va.va_blocksize)
		dirbuflen = va.va_blocksize;
	dirbuf = (char *)malloc(dirbuflen, M_TEMP, M_WAITOK);

	off = 0;
	len = 0;
	do {
		error = get_next_dirent(vp, &dp, dirbuf, dirbuflen, &off,
					&cpos, &len, &eofflag, td);
		if (error)
			goto out;

		if ((dp->d_type != DT_WHT) &&
		    !strcmp(dp->d_name, dirname)) {
			found = 1;
			goto out;
		}
	} while (len > 0 || !eofflag);

out:
	free(dirbuf, M_TEMP);
	return (found);
}

int
vop_stdaccess(struct vop_access_args *ap)
{

	KASSERT((ap->a_accmode & ~(VEXEC | VWRITE | VREAD | VADMIN |
	    VAPPEND)) == 0, ("invalid bit in accmode"));

	return (VOP_ACCESSX(ap->a_vp, ap->a_accmode, ap->a_cred, ap->a_td));
}

int
vop_stdaccessx(struct vop_accessx_args *ap)
{
	int error;
	accmode_t accmode = ap->a_accmode;

	error = vfs_unixify_accmode(&accmode);
	if (error != 0)
		return (error);

	if (accmode == 0)
		return (0);

	return (VOP_ACCESS(ap->a_vp, accmode, ap->a_cred, ap->a_td));
}

/*
 * Advisory record locking support
 */
int
vop_stdadvlock(struct vop_advlock_args *ap)
{
	struct vnode *vp;
	struct ucred *cred;
	struct vattr vattr;
	int error;

	vp = ap->a_vp;
	cred = curthread->td_ucred;
	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &vattr, cred);
	VOP_UNLOCK(vp, 0);
	if (error)
		return (error);

	return (lf_advlock(ap, &(vp->v_lockf), vattr.va_size));
}

int
vop_stdadvlockasync(struct vop_advlockasync_args *ap)
{
	struct vnode *vp;
	struct ucred *cred;
	struct vattr vattr;
	int error;

	vp = ap->a_vp;
	cred = curthread->td_ucred;
	vn_lock(vp, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(vp, &vattr, cred);
	VOP_UNLOCK(vp, 0);
	if (error)
		return (error);

	return (lf_advlockasync(ap, &(vp->v_lockf), vattr.va_size));
}

int
vop_stdadvlockpurge(struct vop_advlockpurge_args *ap)
{
	struct vnode *vp;

	vp = ap->a_vp;
	lf_purgelocks(vp, &vp->v_lockf);
	return (0);
}

/*
 * vop_stdpathconf:
 *
 * Standard implementation of POSIX pathconf, to get information about limits
 * for a filesystem.
 * Override per filesystem for the case where the filesystem has smaller
 * limits.
 */
int
vop_stdpathconf(ap)
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
		case _PC_LINK_MAX:
			*ap->a_retval = LINK_MAX;
			return (0);
		case _PC_MAX_CANON:
			*ap->a_retval = MAX_CANON;
			return (0);
		case _PC_MAX_INPUT:
			*ap->a_retval = MAX_INPUT;
			return (0);
		case _PC_PIPE_BUF:
			*ap->a_retval = PIPE_BUF;
			return (0);
		case _PC_CHOWN_RESTRICTED:
			*ap->a_retval = 1;
			return (0);
		case _PC_VDISABLE:
			*ap->a_retval = _POSIX_VDISABLE;
			return (0);
		default:
			return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * Standard lock, unlock and islocked functions.
 */
int
vop_stdlock(ap)
	struct vop_lock1_args /* {
		struct vnode *a_vp;
		int a_flags;
		char *file;
		int line;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (_lockmgr_args(vp->v_vnlock, ap->a_flags, VI_MTX(vp),
	    LK_WMESG_DEFAULT, LK_PRIO_DEFAULT, LK_TIMO_DEFAULT, ap->a_file,
	    ap->a_line));
}

/* See above. */
int
vop_stdunlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	return (lockmgr(vp->v_vnlock, ap->a_flags | LK_RELEASE, VI_MTX(vp)));
}

/* See above. */
int
vop_stdislocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return (lockstatus(ap->a_vp->v_vnlock));
}

/*
 * Return true for select/poll.
 */
int
vop_nopoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{

	return (poll_no_poll(ap->a_events));
}

/*
 * Implement poll for local filesystems that support it.
 */
int
vop_stdpoll(ap)
	struct vop_poll_args /* {
		struct vnode *a_vp;
		int  a_events;
		struct ucred *a_cred;
		struct thread *a_td;
	} */ *ap;
{
	if (ap->a_events & ~POLLSTANDARD)
		return (vn_pollrecord(ap->a_vp, ap->a_td, ap->a_events));
	return (ap->a_events & (POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM));
}

/*
 * Return our mount point, as we will take charge of the writes.
 */
int
vop_stdgetwritemount(ap)
	struct vop_getwritemount_args /* {
		struct vnode *a_vp;
		struct mount **a_mpp;
	} */ *ap;
{
	struct mount *mp;

	/*
	 * XXX Since this is called unlocked we may be recycled while
	 * attempting to ref the mount.  If this is the case or mountpoint
	 * will be set to NULL.  We only have to prevent this call from
	 * returning with a ref to an incorrect mountpoint.  It is not
	 * harmful to return with a ref to our previous mountpoint.
	 */
	mp = ap->a_vp->v_mount;
	if (mp != NULL) {
		vfs_ref(mp);
		if (mp != ap->a_vp->v_mount) {
			vfs_rel(mp);
			mp = NULL;
		}
	}
	*(ap->a_mpp) = mp;
	return (0);
}

/* XXX Needs good comment and VOP_BMAP(9) manpage */
int
vop_stdbmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct bufobj **a_bop;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{

	if (ap->a_bop != NULL)
		*ap->a_bop = &ap->a_vp->v_bufobj;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(ap->a_vp->v_mount->mnt_stat.f_iosize);
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

int
vop_stdfsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct thread *a_td;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct buf *bp;
	struct bufobj *bo;
	struct buf *nbp;
	int error = 0;
	int maxretry = 1000;     /* large, arbitrarily chosen */

	bo = &vp->v_bufobj;
	BO_LOCK(bo);
loop1:
	/*
	 * MARK/SCAN initialization to avoid infinite loops.
	 */
        TAILQ_FOREACH(bp, &bo->bo_dirty.bv_hd, b_bobufs) {
                bp->b_vflags &= ~BV_SCANNED;
		bp->b_error = 0;
	}

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
loop2:
	TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
		if ((bp->b_vflags & BV_SCANNED) != 0)
			continue;
		bp->b_vflags |= BV_SCANNED;
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL))
			continue;
		BO_UNLOCK(bo);
		KASSERT(bp->b_bufobj == bo,
		    ("bp %p wrong b_bufobj %p should be %p",
		    bp, bp->b_bufobj, bo));
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("fsync: not dirty");
		if ((vp->v_object != NULL) && (bp->b_flags & B_CLUSTEROK)) {
			vfs_bio_awrite(bp);
		} else {
			bremfree(bp);
			bawrite(bp);
		}
		BO_LOCK(bo);
		goto loop2;
	}

	/*
	 * If synchronous the caller expects us to completely resolve all
	 * dirty buffers in the system.  Wait for in-progress I/O to
	 * complete (which could include background bitmap writes), then
	 * retry if dirty blocks still exist.
	 */
	if (ap->a_waitfor == MNT_WAIT) {
		bufobj_wwait(bo, 0, 0);
		if (bo->bo_dirty.bv_cnt > 0) {
			/*
			 * If we are unable to write any of these buffers
			 * then we fail now rather than trying endlessly
			 * to write them out.
			 */
			TAILQ_FOREACH(bp, &bo->bo_dirty.bv_hd, b_bobufs)
				if ((error = bp->b_error) == 0)
					continue;
			if (error == 0 && --maxretry >= 0)
				goto loop1;
			error = EAGAIN;
		}
	}
	BO_UNLOCK(bo);
	if (error == EAGAIN)
		vprint("fsync: giving up on dirty", vp);

	return (error);
}

/* XXX Needs good comment and more info in the manpage (VOP_GETPAGES(9)). */
int
vop_stdgetpages(ap)
	struct vop_getpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_reqpage;
		vm_ooffset_t a_offset;
	} */ *ap;
{

	return vnode_pager_generic_getpages(ap->a_vp, ap->a_m,
	    ap->a_count, ap->a_reqpage);
}

int
vop_stdkqfilter(struct vop_kqfilter_args *ap)
{
	return vfs_kqfilter(ap);
}

/* XXX Needs good comment and more info in the manpage (VOP_PUTPAGES(9)). */
int
vop_stdputpages(ap)
	struct vop_putpages_args /* {
		struct vnode *a_vp;
		vm_page_t *a_m;
		int a_count;
		int a_sync;
		int *a_rtvals;
		vm_ooffset_t a_offset;
	} */ *ap;
{

	return vnode_pager_generic_putpages(ap->a_vp, ap->a_m, ap->a_count,
	     ap->a_sync, ap->a_rtvals);
}

int
vop_stdvptofh(struct vop_vptofh_args *ap)
{
	return (EOPNOTSUPP);
}

int
vop_stdvptocnp(struct vop_vptocnp_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode **dvp = ap->a_vpp;
	struct ucred *cred = ap->a_cred;
	char *buf = ap->a_buf;
	int *buflen = ap->a_buflen;
	char *dirbuf, *cpos;
	int i, error, eofflag, dirbuflen, flags, locked, len, covered;
	off_t off;
	ino_t fileno;
	struct vattr va;
	struct nameidata nd;
	struct thread *td;
	struct dirent *dp;
	struct vnode *mvp;

	i = *buflen;
	error = 0;
	covered = 0;
	td = curthread;

	if (vp->v_type != VDIR)
		return (ENOENT);

	error = VOP_GETATTR(vp, &va, cred);
	if (error)
		return (error);

	VREF(vp);
	locked = VOP_ISLOCKED(vp);
	VOP_UNLOCK(vp, 0);
	NDINIT_ATVP(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_SYSSPACE,
	    "..", vp, td);
	flags = FREAD;
	error = vn_open_cred(&nd, &flags, 0, VN_OPEN_NOAUDIT, cred, NULL);
	if (error) {
		vn_lock(vp, locked | LK_RETRY);
		return (error);
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);

	mvp = *dvp = nd.ni_vp;

	if (vp->v_mount != (*dvp)->v_mount &&
	    ((*dvp)->v_vflag & VV_ROOT) &&
	    ((*dvp)->v_mount->mnt_flag & MNT_UNION)) {
		*dvp = (*dvp)->v_mount->mnt_vnodecovered;
		VREF(mvp);
		VOP_UNLOCK(mvp, 0);
		vn_close(mvp, FREAD, cred, td);
		VREF(*dvp);
		vn_lock(*dvp, LK_EXCLUSIVE | LK_RETRY);
		covered = 1;
	}

	fileno = va.va_fileid;

	dirbuflen = DEV_BSIZE;
	if (dirbuflen < va.va_blocksize)
		dirbuflen = va.va_blocksize;
	dirbuf = (char *)malloc(dirbuflen, M_TEMP, M_WAITOK);

	if ((*dvp)->v_type != VDIR) {
		error = ENOENT;
		goto out;
	}

	off = 0;
	len = 0;
	do {
		/* call VOP_READDIR of parent */
		error = get_next_dirent(*dvp, &dp, dirbuf, dirbuflen, &off,
					&cpos, &len, &eofflag, td);
		if (error)
			goto out;

		if ((dp->d_type != DT_WHT) &&
		    (dp->d_fileno == fileno)) {
			if (covered) {
				VOP_UNLOCK(*dvp, 0);
				vn_lock(mvp, LK_EXCLUSIVE | LK_RETRY);
				if (dirent_exists(mvp, dp->d_name, td)) {
					error = ENOENT;
					VOP_UNLOCK(mvp, 0);
					vn_lock(*dvp, LK_EXCLUSIVE | LK_RETRY);
					goto out;
				}
				VOP_UNLOCK(mvp, 0);
				vn_lock(*dvp, LK_EXCLUSIVE | LK_RETRY);
			}
			i -= dp->d_namlen;

			if (i < 0) {
				error = ENOMEM;
				goto out;
			}
			bcopy(dp->d_name, buf + i, dp->d_namlen);
			error = 0;
			goto out;
		}
	} while (len > 0 || !eofflag);
	error = ENOENT;

out:
	free(dirbuf, M_TEMP);
	if (!error) {
		*buflen = i;
		vhold(*dvp);
	}
	if (covered) {
		vput(*dvp);
		vrele(mvp);
	} else {
		VOP_UNLOCK(mvp, 0);
		vn_close(mvp, FREAD, cred, td);
	}
	vn_lock(vp, locked | LK_RETRY);
	return (error);
}

/*
 * vfs default ops
 * used to fill the vfs function table to get reasonable default return values.
 */
int
vfs_stdroot (mp, flags, vpp)
	struct mount *mp;
	int flags;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

int
vfs_stdstatfs (mp, sbp)
	struct mount *mp;
	struct statfs *sbp;
{

	return (EOPNOTSUPP);
}

int
vfs_stdquotactl (mp, cmds, uid, arg)
	struct mount *mp;
	int cmds;
	uid_t uid;
	void *arg;
{

	return (EOPNOTSUPP);
}

int
vfs_stdsync(mp, waitfor)
	struct mount *mp;
	int waitfor;
{
	struct vnode *vp, *mvp;
	struct thread *td;
	int error, lockreq, allerror = 0;

	td = curthread;
	lockreq = LK_EXCLUSIVE | LK_INTERLOCK;
	if (waitfor != MNT_WAIT)
		lockreq |= LK_NOWAIT;
	/*
	 * Force stale buffer cache information to be flushed.
	 */
	MNT_ILOCK(mp);
loop:
	MNT_VNODE_FOREACH(vp, mp, mvp) {
		/* bv_cnt is an acceptable race here. */
		if (vp->v_bufobj.bo_dirty.bv_cnt == 0)
			continue;
		VI_LOCK(vp);
		MNT_IUNLOCK(mp);
		if ((error = vget(vp, lockreq, td)) != 0) {
			MNT_ILOCK(mp);
			if (error == ENOENT) {
				MNT_VNODE_FOREACH_ABORT_ILOCKED(mp, mvp);
				goto loop;
			}
			continue;
		}
		error = VOP_FSYNC(vp, waitfor, td);
		if (error)
			allerror = error;
		vput(vp);
		MNT_ILOCK(mp);
	}
	MNT_IUNLOCK(mp);
	return (allerror);
}

int
vfs_stdnosync (mp, waitfor)
	struct mount *mp;
	int waitfor;
{

	return (0);
}

int
vfs_stdvget (mp, ino, flags, vpp)
	struct mount *mp;
	ino_t ino;
	int flags;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

int
vfs_stdfhtovp (mp, fhp, vpp)
	struct mount *mp;
	struct fid *fhp;
	struct vnode **vpp;
{

	return (EOPNOTSUPP);
}

int
vfs_stdinit (vfsp)
	struct vfsconf *vfsp;
{

	return (0);
}

int
vfs_stduninit (vfsp)
	struct vfsconf *vfsp;
{

	return(0);
}

int
vfs_stdextattrctl(mp, cmd, filename_vp, attrnamespace, attrname)
	struct mount *mp;
	int cmd;
	struct vnode *filename_vp;
	int attrnamespace;
	const char *attrname;
{

	if (filename_vp != NULL)
		VOP_UNLOCK(filename_vp, 0);
	return (EOPNOTSUPP);
}

int
vfs_stdsysctl(mp, op, req)
	struct mount *mp;
	fsctlop_t op;
	struct sysctl_req *req;
{

	return (EOPNOTSUPP);
}

/* end of vfs default ops */
