/*
 * Copyright (c) 1992, 1993, 1994 The Regents of the University of California.
 * Copyright (c) 1992, 1993, 1994 Jan-Simon Pendry.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
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
 *	@(#)union_vnops.c	8.6 (Berkeley) 2/17/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <miscfs/union/union.h>

#define FIXUP(un) { \
	if (((un)->un_flags & UN_ULOCK) == 0) { \
		union_fixup(un); \
	} \
}

static void
union_fixup(un)
	struct union_node *un;
{

	VOP_LOCK(un->un_uppervp);
	un->un_flags |= UN_ULOCK;
}

static int
union_lookup1(udvp, dvp, vpp, cnp)
	struct vnode *udvp;
	struct vnode *dvp;
	struct vnode **vpp;
	struct componentname *cnp;
{
	int error;
	struct vnode *tdvp;
	struct mount *mp;

	/*
	 * If stepping up the directory tree, check for going
	 * back across the mount point, in which case do what
	 * lookup would do by stepping back down the mount
	 * hierarchy.
	 */
	if (cnp->cn_flags & ISDOTDOT) {
		for (;;) {
			/*
			 * Don't do the NOCROSSMOUNT check
			 * at this level.  By definition,
			 * union fs deals with namespaces, not
			 * filesystems.
			 */
			if ((dvp->v_flag & VROOT) == 0)
				break;

			tdvp = dvp;
			dvp = dvp->v_mount->mnt_vnodecovered;
			vput(tdvp);
			VREF(dvp);
			VOP_LOCK(dvp);
		}
	}

        error = VOP_LOOKUP(dvp, &tdvp, cnp);
	if (error)
		return (error);

	/*
	 * The parent directory will have been unlocked, unless lookup
	 * found the last component.  In which case, re-lock the node
	 * here to allow it to be unlocked again (phew) in union_lookup.
	 */
	if (dvp != tdvp && !(cnp->cn_flags & ISLASTCN))
		VOP_LOCK(dvp);

	dvp = tdvp;

	/*
	 * Lastly check if the current node is a mount point in
	 * which case walk up the mount hierarchy making sure not to
	 * bump into the root of the mount tree (ie. dvp != udvp).
	 */
	while (dvp != udvp && (dvp->v_type == VDIR) &&
	       (mp = dvp->v_mountedhere)) {

		if (mp->mnt_flag & MNT_MLOCK) {
			mp->mnt_flag |= MNT_MWAIT;
			sleep((caddr_t) mp, PVFS);
			continue;
		}

		if (error = VFS_ROOT(mp, &tdvp)) {
			vput(dvp);
			return (error);
		}

		vput(dvp);
		dvp = tdvp;
	}

	*vpp = dvp;
	return (0);
}

int
union_lookup(ap)
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	int error;
	int uerror, lerror;
	struct vnode *uppervp, *lowervp;
	struct vnode *upperdvp, *lowerdvp;
	struct vnode *dvp = ap->a_dvp;
	struct union_node *dun = VTOUNION(dvp);
	struct componentname *cnp = ap->a_cnp;
	int lockparent = cnp->cn_flags & LOCKPARENT;
	int rdonly = cnp->cn_flags & RDONLY;
	struct union_mount *um = MOUNTTOUNIONMOUNT(dvp->v_mount);
	struct ucred *saved_cred;

	cnp->cn_flags |= LOCKPARENT;

	upperdvp = dun->un_uppervp;
	lowerdvp = dun->un_lowervp;
	uppervp = NULLVP;
	lowervp = NULLVP;

	/*
	 * do the lookup in the upper level.
	 * if that level comsumes additional pathnames,
	 * then assume that something special is going
	 * on and just return that vnode.
	 */
	if (upperdvp) {
		FIXUP(dun);
		uerror = union_lookup1(um->um_uppervp, upperdvp,
					&uppervp, cnp);
		/*if (uppervp == upperdvp)
			dun->un_flags |= UN_KLOCK;*/

		if (cnp->cn_consume != 0) {
			*ap->a_vpp = uppervp;
			if (!lockparent)
				cnp->cn_flags &= ~LOCKPARENT;
			return (uerror);
		}
	} else {
		uerror = ENOENT;
	}

	/*
	 * in a similar way to the upper layer, do the lookup
	 * in the lower layer.   this time, if there is some
	 * component magic going on, then vput whatever we got
	 * back from the upper layer and return the lower vnode
	 * instead.
	 */
	if (lowerdvp) {
		int nameiop;

		VOP_LOCK(lowerdvp);

		/*
		 * Only do a LOOKUP on the bottom node, since
		 * we won't be making changes to it anyway.
		 */
		nameiop = cnp->cn_nameiop;
		cnp->cn_nameiop = LOOKUP;
		if (um->um_op == UNMNT_BELOW) {
			saved_cred = cnp->cn_cred;
			cnp->cn_cred = um->um_cred;
		}
		lerror = union_lookup1(um->um_lowervp, lowerdvp,
				&lowervp, cnp);
		if (um->um_op == UNMNT_BELOW)
			cnp->cn_cred = saved_cred;
		cnp->cn_nameiop = nameiop;

		if (lowervp != lowerdvp)
			VOP_UNLOCK(lowerdvp);

		if (cnp->cn_consume != 0) {
			if (uppervp) {
				if (uppervp == upperdvp)
					vrele(uppervp);
				else
					vput(uppervp);
				uppervp = NULLVP;
			}
			*ap->a_vpp = lowervp;
			if (!lockparent)
				cnp->cn_flags &= ~LOCKPARENT;
			return (lerror);
		}
	} else {
		lerror = ENOENT;
	}

	if (!lockparent)
		cnp->cn_flags &= ~LOCKPARENT;

	/*
	 * at this point, we have uerror and lerror indicating
	 * possible errors with the lookups in the upper and lower
	 * layers.  additionally, uppervp and lowervp are (locked)
	 * references to existing vnodes in the upper and lower layers.
	 *
	 * there are now three cases to consider.
	 * 1. if both layers returned an error, then return whatever
	 *    error the upper layer generated.
	 *
	 * 2. if the top layer failed and the bottom layer succeeded
	 *    then two subcases occur.
	 *    a.  the bottom vnode is not a directory, in which
	 *	  case just return a new union vnode referencing
	 *	  an empty top layer and the existing bottom layer.
	 *    b.  the bottom vnode is a directory, in which case
	 *	  create a new directory in the top-level and
	 *	  continue as in case 3.
	 *
	 * 3. if the top layer succeeded then return a new union
	 *    vnode referencing whatever the new top layer and
	 *    whatever the bottom layer returned.
	 */

	*ap->a_vpp = NULLVP;

	/* case 1. */
	if ((uerror != 0) && (lerror != 0)) {
		return (uerror);
	}

	/* case 2. */
	if (uerror != 0 /* && (lerror == 0) */ ) {
		if (lowervp->v_type == VDIR) { /* case 2b. */
			dun->un_flags &= ~UN_ULOCK;
			VOP_UNLOCK(upperdvp);
			uerror = union_mkshadow(um, upperdvp, cnp, &uppervp);
			VOP_LOCK(upperdvp);
			dun->un_flags |= UN_ULOCK;

			if (uerror) {
				if (lowervp) {
					vput(lowervp);
					lowervp = NULLVP;
				}
				return (uerror);
			}
		}
	}

	if (lowervp)
		VOP_UNLOCK(lowervp);

	error = union_allocvp(ap->a_vpp, dvp->v_mount, dvp, upperdvp, cnp,
			      uppervp, lowervp);

	if (error) {
		if (uppervp)
			vput(uppervp);
		if (lowervp)
			vrele(lowervp);
	} else {
		if (*ap->a_vpp != dvp)
			if (!lockparent || !(cnp->cn_flags & ISLASTCN))
				VOP_UNLOCK(dvp);
	}

	return (error);
}

int
union_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;

	if (dvp) {
		int error;
		struct vnode *vp;

		FIXUP(un);

		VREF(dvp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_dvp);
		error = VOP_CREATE(dvp, &vp, ap->a_cnp, ap->a_vap);
		if (error)
			return (error);

		error = union_allocvp(
				ap->a_vpp,
				ap->a_dvp->v_mount,
				ap->a_dvp,
				NULLVP,
				ap->a_cnp,
				vp,
				NULLVP);
		if (error)
			vput(vp);
		return (error);
	}

	vput(ap->a_dvp);
	return (EROFS);
}

int
union_mknod(ap)
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;

	if (dvp) {
		int error;
		struct vnode *vp;

		FIXUP(un);

		VREF(dvp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_dvp);
		error = VOP_MKNOD(dvp, &vp, ap->a_cnp, ap->a_vap);
		if (error)
			return (error);

		if (vp) {
			error = union_allocvp(
					ap->a_vpp,
					ap->a_dvp->v_mount,
					ap->a_dvp,
					NULLVP,
					ap->a_cnp,
					vp,
					NULLVP);
			if (error)
				vput(vp);
		}
		return (error);
	}

	vput(ap->a_dvp);
	return (EROFS);
}

int
union_open(ap)
	struct vop_open_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *tvp;
	int mode = ap->a_mode;
	struct ucred *cred = ap->a_cred;
	struct proc *p = ap->a_p;
	int error;

	/*
	 * If there is an existing upper vp then simply open that.
	 */
	tvp = un->un_uppervp;
	if (tvp == NULLVP) {
		/*
		 * If the lower vnode is being opened for writing, then
		 * copy the file contents to the upper vnode and open that,
		 * otherwise can simply open the lower vnode.
		 */
		tvp = un->un_lowervp;
		if ((ap->a_mode & FWRITE) && (tvp->v_type == VREG)) {
			struct vnode *vp;
			int i;

			/*
			 * Open the named file in the upper layer.  Note that
			 * the file may have come into existence *since* the
			 * lookup was done, since the upper layer may really
			 * be a loopback mount of some other filesystem...
			 * so open the file with exclusive create and barf if
			 * it already exists.
			 * XXX - perhaps should re-lookup the node (once more
			 * with feeling) and simply open that.  Who knows.
			 */
			error = union_vn_create(&vp, un, p);
			if (error)
				return (error);

			/* at this point, uppervp is locked */
			union_newupper(un, vp);
			un->un_flags |= UN_ULOCK;

			/*
			 * Now, if the file is being opened with truncation,
			 * then the (new) upper vnode is ready to fly,
			 * otherwise the data from the lower vnode must be
			 * copied to the upper layer first.  This only works
			 * for regular files (check is made above).
			 */
			if ((mode & O_TRUNC) == 0) {
				/*
				 * XXX - should not ignore errors
				 * from VOP_CLOSE
				 */
				VOP_LOCK(tvp);
				error = VOP_OPEN(tvp, FREAD, cred, p);
				if (error == 0) {
					error = union_copyfile(p, cred,
						       tvp, un->un_uppervp);
					VOP_UNLOCK(tvp);
					(void) VOP_CLOSE(tvp, FREAD);
				} else {
					VOP_UNLOCK(tvp);
				}

#ifdef UNION_DIAGNOSTIC
				if (!error)
					uprintf("union: copied up %s\n",
								un->un_path);
#endif
			}

			un->un_flags &= ~UN_ULOCK;
			VOP_UNLOCK(un->un_uppervp);
			union_vn_close(un->un_uppervp, FWRITE, cred, p);
			VOP_LOCK(un->un_uppervp);
			un->un_flags |= UN_ULOCK;

			/*
			 * Subsequent IOs will go to the top layer, so
			 * call close on the lower vnode and open on the
			 * upper vnode to ensure that the filesystem keeps
			 * its references counts right.  This doesn't do
			 * the right thing with (cred) and (FREAD) though.
			 * Ignoring error returns is not righ, either.
			 */
			for (i = 0; i < un->un_openl; i++) {
				(void) VOP_CLOSE(tvp, FREAD);
				(void) VOP_OPEN(un->un_uppervp, FREAD, cred, p);
			}
			un->un_openl = 0;

			if (error == 0)
				error = VOP_OPEN(un->un_uppervp, mode, cred, p);
			return (error);
		}

		/*
		 * Just open the lower vnode
		 */
		un->un_openl++;
		VOP_LOCK(tvp);
		error = VOP_OPEN(tvp, mode, cred, p);
		VOP_UNLOCK(tvp);

		return (error);
	}

	FIXUP(un);

	error = VOP_OPEN(tvp, mode, cred, p);

	return (error);
}

int
union_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *vp;

	if (un->un_uppervp) {
		vp = un->un_uppervp;
	} else {
#ifdef UNION_DIAGNOSTIC
		if (un->un_openl <= 0)
			panic("union: un_openl cnt");
#endif
		--un->un_openl;
		vp = un->un_lowervp;
	}

	return (VOP_CLOSE(vp, ap->a_fflag, ap->a_cred, ap->a_p));
}

/*
 * Check access permission on the union vnode.
 * The access check being enforced is to check
 * against both the underlying vnode, and any
 * copied vnode.  This ensures that no additional
 * file permissions are given away simply because
 * the user caused an implicit file copy.
 */
int
union_access(ap)
	struct vop_access_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	int error = EACCES;
	struct vnode *vp;

	if (vp = un->un_uppervp) {
		FIXUP(un);
		return (VOP_ACCESS(vp, ap->a_mode, ap->a_cred, ap->a_p));
	}

	if (vp = un->un_lowervp) {
		VOP_LOCK(vp);
		error = VOP_ACCESS(vp, ap->a_mode, ap->a_cred, ap->a_p);
		if (error == 0) {
			struct union_mount *um = MOUNTTOUNIONMOUNT(vp->v_mount);

			if (um->um_op == UNMNT_BELOW)
				error = VOP_ACCESS(vp, ap->a_mode,
						um->um_cred, ap->a_p);
		}
		VOP_UNLOCK(vp);
		if (error)
			return (error);
	}

	return (error);
}

/*
 *  We handle getattr only to change the fsid.
 */
int
union_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	int error;
	struct union_node *un = VTOUNION(ap->a_vp);
	struct vnode *vp = un->un_uppervp;
	struct vattr *vap;
	struct vattr va;


	/*
	 * Some programs walk the filesystem hierarchy by counting
	 * links to directories to avoid stat'ing all the time.
	 * This means the link count on directories needs to be "correct".
	 * The only way to do that is to call getattr on both layers
	 * and fix up the link count.  The link count will not necessarily
	 * be accurate but will be large enough to defeat the tree walkers.
	 */

	vap = ap->a_vap;

	vp = un->un_uppervp;
	if (vp != NULLVP) {
		FIXUP(un);
		error = VOP_GETATTR(vp, vap, ap->a_cred, ap->a_p);
		if (error)
			return (error);
	}

	if (vp == NULLVP) {
		vp = un->un_lowervp;
	} else if (vp->v_type == VDIR) {
		vp = un->un_lowervp;
		vap = &va;
	} else {
		vp = NULLVP;
	}

	if (vp != NULLVP) {
		VOP_LOCK(vp);
		error = VOP_GETATTR(vp, vap, ap->a_cred, ap->a_p);
		VOP_UNLOCK(vp);
		if (error)
			return (error);
	}

	if ((vap != ap->a_vap) && (vap->va_type == VDIR))
		ap->a_vap->va_nlink += vap->va_nlink;

	vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	return (0);
}

int
union_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);
	int error;

	/*
	 * Handle case of truncating lower object to zero size,
	 * by creating a zero length upper object.  This is to
	 * handle the case of open with O_TRUNC and O_CREAT.
	 */
	if ((un->un_uppervp == NULLVP) &&
	    /* assert(un->un_lowervp != NULLVP) */
	    (un->un_lowervp->v_type == VREG) &&
	    (ap->a_vap->va_size == 0)) {
		struct vnode *vp;

		error = union_vn_create(&vp, un, ap->a_p);
		if (error)
			return (error);

		/* at this point, uppervp is locked */
		union_newupper(un, vp);

		VOP_UNLOCK(vp);
		union_vn_close(un->un_uppervp, FWRITE, ap->a_cred, ap->a_p);
		VOP_LOCK(vp);
		un->un_flags |= UN_ULOCK;
	}

	/*
	 * Try to set attributes in upper layer,
	 * otherwise return read-only filesystem error.
	 */
	if (un->un_uppervp != NULLVP) {
		FIXUP(un);
		error = VOP_SETATTR(un->un_uppervp, ap->a_vap,
					ap->a_cred, ap->a_p);
	} else {
		error = EROFS;
	}

	return (error);
}

int
union_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		VOP_LOCK(vp);
	else
		FIXUP(VTOUNION(ap->a_vp));
	error = VOP_READ(vp, ap->a_uio, ap->a_ioflag, ap->a_cred);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_write(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		VOP_LOCK(vp);
	else
		FIXUP(VTOUNION(ap->a_vp));
	error = VOP_WRITE(vp, ap->a_uio, ap->a_ioflag, ap->a_cred);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int  a_command;
		caddr_t  a_data;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (VOP_IOCTL(OTHERVP(ap->a_vp), ap->a_command, ap->a_data,
				ap->a_fflag, ap->a_cred, ap->a_p));
}

int
union_select(ap)
	struct vop_select_args /* {
		struct vnode *a_vp;
		int  a_which;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (VOP_SELECT(OTHERVP(ap->a_vp), ap->a_which, ap->a_fflags,
				ap->a_cred, ap->a_p));
}

int
union_mmap(ap)
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (VOP_MMAP(OTHERVP(ap->a_vp), ap->a_fflags,
				ap->a_cred, ap->a_p));
}

int
union_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int  a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	int error = 0;
	struct vnode *targetvp = OTHERVP(ap->a_vp);

	if (targetvp) {
		int dolock = (targetvp == LOWERVP(ap->a_vp));

		if (dolock)
			VOP_LOCK(targetvp);
		else
			FIXUP(VTOUNION(ap->a_vp));
		error = VOP_FSYNC(targetvp, ap->a_cred,
					ap->a_waitfor, ap->a_p);
		if (dolock)
			VOP_UNLOCK(targetvp);
	}

	return (error);
}

int
union_seek(ap)
	struct vop_seek_args /* {
		struct vnode *a_vp;
		off_t  a_oldoff;
		off_t  a_newoff;
		struct ucred *a_cred;
	} */ *ap;
{

	return (VOP_SEEK(OTHERVP(ap->a_vp), ap->a_oldoff, ap->a_newoff, ap->a_cred));
}

int
union_remove(ap)
	struct vop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	int error;
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct union_node *un = VTOUNION(ap->a_vp);

	if (dun->un_uppervp && un->un_uppervp) {
		struct vnode *dvp = dun->un_uppervp;
		struct vnode *vp = un->un_uppervp;

		FIXUP(dun);
		VREF(dvp);
		dun->un_flags |= UN_KLOCK;
		vput(ap->a_dvp);
		FIXUP(un);
		VREF(vp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_vp);

		error = VOP_REMOVE(dvp, vp, ap->a_cnp);
		if (!error)
			union_removed_upper(un);

		/*
		 * XXX: should create a whiteout here
		 */
	} else {
		/*
		 * XXX: should create a whiteout here
		 */
		vput(ap->a_dvp);
		vput(ap->a_vp);
		error = EROFS;
	}

	return (error);
}

int
union_link(ap)
	struct vop_link_args /* {
		struct vnode *a_vp;
		struct vnode *a_tdvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	int error;
	struct union_node *dun = VTOUNION(ap->a_vp);
	struct union_node *un = VTOUNION(ap->a_tdvp);

	if (dun->un_uppervp && un->un_uppervp) {
		struct vnode *dvp = dun->un_uppervp;
		struct vnode *vp = un->un_uppervp;

		FIXUP(dun);
		VREF(dvp);
		dun->un_flags |= UN_KLOCK;
		vput(ap->a_vp);
		FIXUP(un);
		VREF(vp);
		vrele(ap->a_tdvp);

		error = VOP_LINK(dvp, vp, ap->a_cnp);
	} else {
		/*
		 * XXX: need to copy to upper layer
		 * and do the link there.
		 */
		vput(ap->a_vp);
		vrele(ap->a_tdvp);
		error = EROFS;
	}

	return (error);
}

int
union_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	int error;

	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *tvp = ap->a_tvp;

	if (fdvp->v_op == union_vnodeop_p) {	/* always true */
		struct union_node *un = VTOUNION(fdvp);
		if (un->un_uppervp == NULLVP) {
			error = EROFS;
			goto bad;
		}

		FIXUP(un);
		fdvp = un->un_uppervp;
		VREF(fdvp);
		vrele(ap->a_fdvp);
	}

	if (fvp->v_op == union_vnodeop_p) {	/* always true */
		struct union_node *un = VTOUNION(fvp);
		if (un->un_uppervp == NULLVP) {
			error = EROFS;
			goto bad;
		}

		FIXUP(un);
		fvp = un->un_uppervp;
		VREF(fvp);
		vrele(ap->a_fvp);
	}

	if (tdvp->v_op == union_vnodeop_p) {
		struct union_node *un = VTOUNION(tdvp);
		if (un->un_uppervp == NULLVP) {
			error = EROFS;
			goto bad;
		}

		tdvp = un->un_uppervp;
		VREF(tdvp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_tdvp);
	}

	if (tvp && tvp->v_op == union_vnodeop_p) {
		struct union_node *un = VTOUNION(tvp);
		if (un->un_uppervp == NULLVP) {
			error = EROFS;
			goto bad;
		}

		tvp = un->un_uppervp;
		VREF(tvp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_tvp);
	}

	return (VOP_RENAME(fdvp, fvp, ap->a_fcnp, tdvp, tvp, ap->a_tcnp));

bad:
	vrele(fdvp);
	vrele(fvp);
	vput(tdvp);
	if (tvp)
		vput(tvp);

	return (error);
}

int
union_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;

	if (dvp) {
		int error;
		struct vnode *vp;

		FIXUP(un);
		VREF(dvp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_dvp);
		error = VOP_MKDIR(dvp, &vp, ap->a_cnp, ap->a_vap);
		if (error)
			return (error);

		error = union_allocvp(
				ap->a_vpp,
				ap->a_dvp->v_mount,
				ap->a_dvp,
				NULLVP,
				ap->a_cnp,
				vp,
				NULLVP);
		if (error)
			vput(vp);
		return (error);
	}

	vput(ap->a_dvp);
	return (EROFS);
}

int
union_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	int error;
	struct union_node *dun = VTOUNION(ap->a_dvp);
	struct union_node *un = VTOUNION(ap->a_vp);

	if (dun->un_uppervp && un->un_uppervp) {
		struct vnode *dvp = dun->un_uppervp;
		struct vnode *vp = un->un_uppervp;

		FIXUP(dun);
		VREF(dvp);
		dun->un_flags |= UN_KLOCK;
		vput(ap->a_dvp);
		FIXUP(un);
		VREF(vp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_vp);

		error = VOP_RMDIR(dvp, vp, ap->a_cnp);
		if (!error)
			union_removed_upper(un);

		/*
		 * XXX: should create a whiteout here
		 */
	} else {
		/*
		 * XXX: should create a whiteout here
		 */
		vput(ap->a_dvp);
		vput(ap->a_vp);
		error = EROFS;
	}

	return (error);
}

int
union_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	struct union_node *un = VTOUNION(ap->a_dvp);
	struct vnode *dvp = un->un_uppervp;

	if (dvp) {
		int error;
		struct vnode *vp;
		struct mount *mp = ap->a_dvp->v_mount;

		FIXUP(un);
		VREF(dvp);
		un->un_flags |= UN_KLOCK;
		vput(ap->a_dvp);
		error = VOP_SYMLINK(dvp, &vp, ap->a_cnp,
					ap->a_vap, ap->a_target);
		*ap->a_vpp = NULLVP;
		return (error);
	}

	vput(ap->a_dvp);
	return (EROFS);
}

/*
 * union_readdir works in concert with getdirentries and
 * readdir(3) to provide a list of entries in the unioned
 * directories.  getdirentries is responsible for walking
 * down the union stack.  readdir(3) is responsible for
 * eliminating duplicate names from the returned data stream.
 */
int
union_readdir(ap)
	struct vop_readdir_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	int error = 0;
	struct union_node *un = VTOUNION(ap->a_vp);

	if (un->un_uppervp) {
		FIXUP(un);
		error = VOP_READDIR(un->un_uppervp, ap->a_uio, ap->a_cred);
	}

	return (error);
}

int
union_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		VOP_LOCK(vp);
	else
		FIXUP(VTOUNION(ap->a_vp));
	error = VOP_READLINK(vp, ap->a_uio, ap->a_cred);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_abortop(ap)
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	int error;
	struct vnode *vp = OTHERVP(ap->a_dvp);
	struct union_node *un = VTOUNION(ap->a_dvp);
	int islocked = un->un_flags & UN_LOCKED;
	int dolock = (vp == LOWERVP(ap->a_dvp));

	if (islocked) {
		if (dolock)
			VOP_LOCK(vp);
		else
			FIXUP(VTOUNION(ap->a_dvp));
	}
	error = VOP_ABORTOP(vp, ap->a_cnp);
	if (islocked && dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	/*
	 * Do nothing (and _don't_ bypass).
	 * Wait to vrele lowervp until reclaim,
	 * so that until then our union_node is in the
	 * cache and reusable.
	 *
	 * NEEDSWORK: Someday, consider inactive'ing
	 * the lowervp and then trying to reactivate it
	 * with capabilities (v_id)
	 * like they do in the name lookup cache code.
	 * That's too much work for now.
	 */

#ifdef UNION_DIAGNOSTIC
	struct union_node *un = VTOUNION(ap->a_vp);

	if (un->un_flags & UN_LOCKED)
		panic("union: inactivating locked node");
#endif

	return (0);
}

int
union_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	union_freevp(ap->a_vp);

	return (0);
}

int
union_lock(ap)
	struct vop_lock_args *ap;
{
	struct vnode *vp = ap->a_vp;
	struct union_node *un;

start:
	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		sleep((caddr_t)vp, PINOD);
	}

	un = VTOUNION(vp);

	if (un->un_uppervp) {
		if ((un->un_flags & UN_ULOCK) == 0) {
			un->un_flags |= UN_ULOCK;
			VOP_LOCK(un->un_uppervp);
		}
#ifdef DIAGNOSTIC
		if (un->un_flags & UN_KLOCK)
			panic("union: dangling upper lock");
#endif
	}

	if (un->un_flags & UN_LOCKED) {
#ifdef DIAGNOSTIC
		if (curproc && un->un_pid == curproc->p_pid &&
			    un->un_pid > -1 && curproc->p_pid > -1)
			panic("union: locking against myself");
#endif
		un->un_flags |= UN_WANT;
		sleep((caddr_t) &un->un_flags, PINOD);
		goto start;
	}

#ifdef DIAGNOSTIC
	if (curproc)
		un->un_pid = curproc->p_pid;
	else
		un->un_pid = -1;
#endif

	un->un_flags |= UN_LOCKED;
	return (0);
}

int
union_unlock(ap)
	struct vop_lock_args *ap;
{
	struct union_node *un = VTOUNION(ap->a_vp);

#ifdef DIAGNOSTIC
	if ((un->un_flags & UN_LOCKED) == 0)
		panic("union: unlock unlocked node");
	if (curproc && un->un_pid != curproc->p_pid &&
			curproc->p_pid > -1 && un->un_pid > -1)
		panic("union: unlocking other process's union node");
#endif

	un->un_flags &= ~UN_LOCKED;

	if ((un->un_flags & (UN_ULOCK|UN_KLOCK)) == UN_ULOCK)
		VOP_UNLOCK(un->un_uppervp);

	un->un_flags &= ~(UN_ULOCK|UN_KLOCK);

	if (un->un_flags & UN_WANT) {
		un->un_flags &= ~UN_WANT;
		wakeup((caddr_t) &un->un_flags);
	}

#ifdef DIAGNOSTIC
	un->un_pid = 0;
#endif

	return (0);
}

int
union_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap;
{
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		VOP_LOCK(vp);
	else
		FIXUP(VTOUNION(ap->a_vp));
	error = VOP_BMAP(vp, ap->a_bn, ap->a_vpp, ap->a_bnp, ap->a_runp);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	printf("\ttag VT_UNION, vp=%x, uppervp=%x, lowervp=%x\n",
			vp, UPPERVP(vp), LOWERVP(vp));
	return (0);
}

int
union_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return ((VTOUNION(ap->a_vp)->un_flags & UN_LOCKED) ? 1 : 0);
}

int
union_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{
	int error;
	struct vnode *vp = OTHERVP(ap->a_vp);
	int dolock = (vp == LOWERVP(ap->a_vp));

	if (dolock)
		VOP_LOCK(vp);
	else
		FIXUP(VTOUNION(ap->a_vp));
	error = VOP_PATHCONF(vp, ap->a_name, ap->a_retval);
	if (dolock)
		VOP_UNLOCK(vp);

	return (error);
}

int
union_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{

	return (VOP_ADVLOCK(OTHERVP(ap->a_vp), ap->a_id, ap->a_op,
				ap->a_fl, ap->a_flags));
}


/*
 * XXX - vop_strategy must be hand coded because it has no
 * vnode in its arguments.
 * This goes away with a merged VM/buffer cache.
 */
int
union_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	struct buf *bp = ap->a_bp;
	int error;
	struct vnode *savedvp;

	savedvp = bp->b_vp;
	bp->b_vp = OTHERVP(bp->b_vp);

#ifdef DIAGNOSTIC
	if (bp->b_vp == NULLVP)
		panic("union_strategy: nil vp");
	if (((bp->b_flags & B_READ) == 0) &&
	    (bp->b_vp == LOWERVP(savedvp)))
		panic("union_strategy: writing to lowervp");
#endif

	error = VOP_STRATEGY(bp);
	bp->b_vp = savedvp;

	return (error);
}

/*
 * Global vfs data structures
 */
int (**union_vnodeop_p)();
struct vnodeopv_entry_desc union_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, union_lookup },		/* lookup */
	{ &vop_create_desc, union_create },		/* create */
	{ &vop_mknod_desc, union_mknod },		/* mknod */
	{ &vop_open_desc, union_open },			/* open */
	{ &vop_close_desc, union_close },		/* close */
	{ &vop_access_desc, union_access },		/* access */
	{ &vop_getattr_desc, union_getattr },		/* getattr */
	{ &vop_setattr_desc, union_setattr },		/* setattr */
	{ &vop_read_desc, union_read },			/* read */
	{ &vop_write_desc, union_write },		/* write */
	{ &vop_ioctl_desc, union_ioctl },		/* ioctl */
	{ &vop_select_desc, union_select },		/* select */
	{ &vop_mmap_desc, union_mmap },			/* mmap */
	{ &vop_fsync_desc, union_fsync },		/* fsync */
	{ &vop_seek_desc, union_seek },			/* seek */
	{ &vop_remove_desc, union_remove },		/* remove */
	{ &vop_link_desc, union_link },			/* link */
	{ &vop_rename_desc, union_rename },		/* rename */
	{ &vop_mkdir_desc, union_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, union_rmdir },		/* rmdir */
	{ &vop_symlink_desc, union_symlink },		/* symlink */
	{ &vop_readdir_desc, union_readdir },		/* readdir */
	{ &vop_readlink_desc, union_readlink },		/* readlink */
	{ &vop_abortop_desc, union_abortop },		/* abortop */
	{ &vop_inactive_desc, union_inactive },		/* inactive */
	{ &vop_reclaim_desc, union_reclaim },		/* reclaim */
	{ &vop_lock_desc, union_lock },			/* lock */
	{ &vop_unlock_desc, union_unlock },		/* unlock */
	{ &vop_bmap_desc, union_bmap },			/* bmap */
	{ &vop_strategy_desc, union_strategy },		/* strategy */
	{ &vop_print_desc, union_print },		/* print */
	{ &vop_islocked_desc, union_islocked },		/* islocked */
	{ &vop_pathconf_desc, union_pathconf },		/* pathconf */
	{ &vop_advlock_desc, union_advlock },		/* advlock */
#ifdef notdef
	{ &vop_blkatoff_desc, union_blkatoff },		/* blkatoff */
	{ &vop_valloc_desc, union_valloc },		/* valloc */
	{ &vop_vfree_desc, union_vfree },		/* vfree */
	{ &vop_truncate_desc, union_truncate },		/* truncate */
	{ &vop_update_desc, union_update },		/* update */
	{ &vop_bwrite_desc, union_bwrite },		/* bwrite */
#endif
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc union_vnodeop_opv_desc =
	{ &union_vnodeop_p, union_vnodeop_entries };
