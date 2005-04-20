/*-
 * Copyright (c) 1994, 1995 The Regents of the University of California.
 * Copyright (c) 1994, 1995 Jan-Simon Pendry.
 * All rights reserved.
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
 *	@(#)union_vfsops.c	8.20 (Berkeley) 5/20/95
 * $FreeBSD$
 */

/*
 * Union Layer
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/filedesc.h>
#include <fs/unionfs/union.h>

static MALLOC_DEFINE(M_UNIONFSMNT, "UNION mount", "UNION mount structure");

extern vfs_init_t       union_init;
static vfs_root_t       union_root;
static vfs_mount_t	union_mount;
static vfs_statfs_t	union_statfs;
static vfs_unmount_t    union_unmount;

/*
 * Mount union filesystem.
 */
static int
union_mount(mp, td)
	struct mount *mp;
	struct thread *td;
{
	int error = 0;
	struct vfsoptlist *opts;
	struct vnode *lowerrootvp = NULLVP;
	struct vnode *upperrootvp = NULLVP;
	struct union_mount *um = 0;
	struct ucred *cred = 0;
	char *cp = 0, *target;
	int op;
	int len;
	size_t size;
	struct componentname fakecn;
	struct nameidata nd, *ndp = &nd;

	UDEBUG(("union_mount(mp = %p)\n", (void *)mp));

	opts = mp->mnt_optnew;
	/*
	 * Disable clustered write, otherwise system becomes unstable.
	 */
	mp->mnt_flag |= MNT_NOCLUSTERW;

	/*
	 * Update is a no-op
	 */
	if (mp->mnt_flag & MNT_UPDATE)
		/*
		 * Need to provide:
		 * 1. a way to convert between rdonly and rdwr mounts.
		 * 2. support for nfs exports.
		 */
		return (EOPNOTSUPP);

	/*
	 * Get arguments.
	 */
	error = vfs_getopt(opts, "target", (void **)&target, &len);
	if (error || target[len - 1] != '\0')
		return (EINVAL);

	op = 0;
	if (vfs_getopt(opts, "below", NULL, NULL) == 0)
		op = UNMNT_BELOW;
	if (vfs_getopt(opts, "replace", NULL, NULL) == 0) {
		/* These options are mutually exclusive. */
		if (op)
			return (EINVAL);
		op = UNMNT_REPLACE;
	}
	/*
	 * UNMNT_ABOVE is the default.
	 */
	if (op == 0)
		op = UNMNT_ABOVE;

	/*
	 * Obtain lower vnode.  Vnode is stored in mp->mnt_vnodecovered.
	 * We need to reference it but not lock it.
	 */

	lowerrootvp = mp->mnt_vnodecovered;
	VREF(lowerrootvp);

#if 0
	/*
	 * Unlock lower node to avoid deadlock.
	 */
	if (lowerrootvp->v_op == union_vnodeop_p)
		VOP_UNLOCK(lowerrootvp, 0, td);
#endif

	/*
	 * Obtain upper vnode by calling namei() on the path.  The
	 * upperrootvp will be turned referenced but not locked.
	 */
	NDINIT(ndp, LOOKUP, FOLLOW|WANTPARENT, UIO_SYSSPACE, target, td);

	error = namei(ndp);

#if 0
	if (lowerrootvp->v_op == union_vnodeop_p)
		vn_lock(lowerrootvp, LK_EXCLUSIVE | LK_RETRY, td);
#endif
	if (error)
		goto bad;

	NDFREE(ndp, NDF_ONLY_PNBUF);
	upperrootvp = ndp->ni_vp;
	vrele(ndp->ni_dvp);
	ndp->ni_dvp = NULL;

	UDEBUG(("mount_root UPPERVP %p locked = %d\n", upperrootvp,
	    VOP_ISLOCKED(upperrootvp, NULL)));

	/*
	 * Check multi union mount to avoid `lock myself again' panic.
	 * Also require that it be a directory.
	 */
	if (upperrootvp == VTOUNION(lowerrootvp)->un_uppervp) {
#ifdef DIAGNOSTIC
		printf("union_mount: multi union mount?\n");
#endif
		error = EDEADLK;
		goto bad;
	}

	if (upperrootvp->v_type != VDIR) {
		error = EINVAL;
		goto bad;
	}

	/*
	 * Allocate our union_mount structure and populate the fields.
	 * The vnode references are stored in the union_mount as held,
	 * unlocked references.  Depending on the _BELOW flag, the
	 * filesystems are viewed in a different order.  In effect this
	 * is the same as providing a mount-under option to the mount
	 * syscall.
	 */

	um = (struct union_mount *) malloc(sizeof(struct union_mount),
				M_UNIONFSMNT, M_WAITOK | M_ZERO);

	um->um_op = op;

	switch (um->um_op) {
	case UNMNT_ABOVE:
		um->um_lowervp = lowerrootvp;
		um->um_uppervp = upperrootvp;
		upperrootvp = NULL;
		lowerrootvp = NULL;
		break;

	case UNMNT_BELOW:
		um->um_lowervp = upperrootvp;
		um->um_uppervp = lowerrootvp;
		upperrootvp = NULL;
		lowerrootvp = NULL;
		break;

	case UNMNT_REPLACE:
		vrele(lowerrootvp);
		lowerrootvp = NULL;
		um->um_uppervp = upperrootvp;
		um->um_lowervp = lowerrootvp;
		upperrootvp = NULL;
		break;

	default:
		error = EINVAL;
		goto bad;
	}

	/*
	 * Unless the mount is readonly, ensure that the top layer
	 * supports whiteout operations.
	 */
	if ((mp->mnt_flag & MNT_RDONLY) == 0) {
		/*
		 * XXX Fake up a struct componentname with only cn_nameiop
		 * and cn_thread valid; union_whiteout() needs to use the
		 * thread pointer to lock the vnode.
		 */
		bzero(&fakecn, sizeof(fakecn));
		fakecn.cn_nameiop = LOOKUP;
		fakecn.cn_thread = td;
		error = VOP_WHITEOUT(um->um_uppervp, &fakecn, LOOKUP);
		if (error)
			goto bad;
	}

	um->um_cred = crhold(td->td_ucred);
	FILEDESC_LOCK_FAST(td->td_proc->p_fd);
	um->um_cmode = UN_DIRMODE &~ td->td_proc->p_fd->fd_cmask;
	FILEDESC_UNLOCK_FAST(td->td_proc->p_fd);

	/*
	 * Depending on what you think the MNT_LOCAL flag might mean,
	 * you may want the && to be || on the conditional below.
	 * At the moment it has been defined that the filesystem is
	 * only local if it is all local, ie the MNT_LOCAL flag implies
	 * that the entire namespace is local.  If you think the MNT_LOCAL
	 * flag implies that some of the files might be stored locally
	 * then you will want to change the conditional.
	 */
	if (um->um_op == UNMNT_ABOVE) {
		if (((um->um_lowervp == NULLVP) ||
		     (um->um_lowervp->v_mount->mnt_flag & MNT_LOCAL)) &&
		    (um->um_uppervp->v_mount->mnt_flag & MNT_LOCAL))
			mp->mnt_flag |= MNT_LOCAL;
	}

	/*
	 * Copy in the upper layer's RDONLY flag.  This is for the benefit
	 * of lookup() which explicitly checks the flag, rather than asking
	 * the filesystem for its own opinion.  This means, that an update
	 * mount of the underlying filesystem to go from rdonly to rdwr
	 * will leave the unioned view as read-only.
	 */
	mp->mnt_flag |= (um->um_uppervp->v_mount->mnt_flag & MNT_RDONLY);

	mp->mnt_data = (qaddr_t) um;
	vfs_getnewfsid(mp);

	switch (um->um_op) {
	case UNMNT_ABOVE:
		cp = "<above>:";
		break;
	case UNMNT_BELOW:
		cp = "<below>:";
		break;
	case UNMNT_REPLACE:
		cp = "";
		break;
	}
	len = strlen(cp);
	bcopy(cp, mp->mnt_stat.f_mntfromname, len);

	cp = mp->mnt_stat.f_mntfromname + len;
	len = MNAMELEN - len;

	(void) copystr(target, cp, len - 1, &size);
	bzero(cp + size, len - size);

	(void)union_statfs(mp, &mp->mnt_stat, td);

	UDEBUG(("union_mount: from %s, on %s\n",
		mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname));
	return (0);

bad:
	if (um) {
		if (um->um_uppervp)
			vrele(um->um_uppervp);
		if (um->um_lowervp)
			vrele(um->um_lowervp);
		/* XXX other fields */
		free(um, M_UNIONFSMNT);
	}
	if (cred)
		crfree(cred);
	if (upperrootvp)
		vrele(upperrootvp);
	if (lowerrootvp)
		vrele(lowerrootvp);
	return (error);
}

/*
 * Free reference to union layer.
 */
static int
union_unmount(mp, mntflags, td)
	struct mount *mp;
	int mntflags;
	struct thread *td;
{
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);
	int error;
	int freeing;
	int flags = 0;

	UDEBUG(("union_unmount(mp = %p)\n", (void *)mp));

	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/*
	 * Keep flushing vnodes from the mount list.
	 * This is needed because of the un_pvp held
	 * reference to the parent vnode.
	 * If more vnodes have been freed on a given pass,
	 * the try again.  The loop will iterate at most
	 * (d) times, where (d) is the maximum tree depth
	 * in the filesystem.
	 */
	for (freeing = 0; (error = vflush(mp, 0, flags, td)) != 0;) {
		int n;

		/* count #vnodes held on mount list */
		n = mp->mnt_nvnodelistsize;

		/* if this is unchanged then stop */
		if (n == freeing)
			break;

		/* otherwise try once more time */
		freeing = n;
	}

	/*
	 * If the most recent vflush failed, the filesystem is still busy.
	 */
	if (error)
		return (error);

	/*
	 * Discard references to upper and lower target vnodes.
	 */
	if (um->um_lowervp)
		vrele(um->um_lowervp);
	vrele(um->um_uppervp);
	crfree(um->um_cred);
	/*
	 * Finally, throw away the union_mount structure.
	 */
	free(mp->mnt_data, M_UNIONFSMNT);	/* XXX */
	mp->mnt_data = 0;
	return (0);
}

static int
union_root(mp, vpp, td)
	struct mount *mp;
	struct vnode **vpp;
	struct thread *td;
{
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);
	int error;

	/*
	 * Supply an unlocked reference to um_uppervp and to um_lowervp.  It
	 * is possible for um_uppervp to be locked without the associated
	 * root union_node being locked.  We let union_allocvp() deal with
	 * it.
	 */
	UDEBUG(("union_root UPPERVP %p locked = %d\n", um->um_uppervp,
	    VOP_ISLOCKED(um->um_uppervp, NULL)));

	VREF(um->um_uppervp);
	if (um->um_lowervp)
		VREF(um->um_lowervp);

	error = union_allocvp(vpp, mp, NULLVP, NULLVP, NULL, 
		    um->um_uppervp, um->um_lowervp, 1);
	UDEBUG(("error %d\n", error));
	UDEBUG(("union_root2 UPPERVP %p locked = %d\n", um->um_uppervp,
	    VOP_ISLOCKED(um->um_uppervp, NULL)));

	return (error);
}

static int
union_statfs(mp, sbp, td)
	struct mount *mp;
	struct statfs *sbp;
	struct thread *td;
{
	int error;
	struct union_mount *um = MOUNTTOUNIONMOUNT(mp);
	struct statfs mstat;
	int lbsize;

	UDEBUG(("union_statfs(mp = %p, lvp = %p, uvp = %p)\n",
	    (void *)mp, (void *)um->um_lowervp, (void *)um->um_uppervp));

	bzero(&mstat, sizeof(mstat));

	if (um->um_lowervp) {
		error = VFS_STATFS(um->um_lowervp->v_mount, &mstat, td);
		if (error)
			return (error);
	}

	/*
	 * Now copy across the "interesting" information and fake the rest.
	 */
#if 0
	sbp->f_type = mstat.f_type;
	sbp->f_flags = mstat.f_flags;
	sbp->f_bsize = mstat.f_bsize;
	sbp->f_iosize = mstat.f_iosize;
#endif
	lbsize = mstat.f_bsize;
	sbp->f_blocks = mstat.f_blocks;
	sbp->f_bfree = mstat.f_bfree;
	sbp->f_bavail = mstat.f_bavail;
	sbp->f_files = mstat.f_files;
	sbp->f_ffree = mstat.f_ffree;

	error = VFS_STATFS(um->um_uppervp->v_mount, &mstat, td);
	if (error)
		return (error);

	sbp->f_flags = mstat.f_flags;
	sbp->f_bsize = mstat.f_bsize;
	sbp->f_iosize = mstat.f_iosize;

	/*
	 * If the lower and upper blocksizes differ, then frig the
	 * block counts so that the sizes reported by df make some
	 * kind of sense.  None of this makes sense though.
	 */

	if (mstat.f_bsize != lbsize)
		sbp->f_blocks = ((off_t) sbp->f_blocks * lbsize) / mstat.f_bsize;

	/*
	 * The "total" fields count total resources in all layers,
	 * the "free" fields count only those resources which are
	 * free in the upper layer (since only the upper layer
	 * is writeable).
	 */
	sbp->f_blocks += mstat.f_blocks;
	sbp->f_bfree = mstat.f_bfree;
	sbp->f_bavail = mstat.f_bavail;
	sbp->f_files += mstat.f_files;
	sbp->f_ffree = mstat.f_ffree;

	if (sbp != &mp->mnt_stat) {
		sbp->f_type = mp->mnt_vfc->vfc_typenum;
		bcopy(&mp->mnt_stat.f_fsid, &sbp->f_fsid, sizeof(sbp->f_fsid));
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
	}
	return (0);
}

static struct vfsops union_vfsops = {
	.vfs_init = 		union_init,
	.vfs_mount =		union_mount,
	.vfs_root =		union_root,
	.vfs_statfs =		union_statfs,
	.vfs_unmount =		union_unmount,
};

VFS_SET(union_vfsops, unionfs, VFCF_LOOPBACK);
