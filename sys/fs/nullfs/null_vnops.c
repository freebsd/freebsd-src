/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	@(#)null_vnops.c	8.6 (Berkeley) 5/27/95
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *	...and...
 *	@(#)null_vnodeops.c 1.20 92/07/07 UCLA Ficus project
 *
 * $FreeBSD$
 */

/*
 * Null Layer
 *
 * (See mount_null(8) for more information.)
 *
 * The null layer duplicates a portion of the file system
 * name space under a new name.  In this respect, it is
 * similar to the loopback file system.  It differs from
 * the loopback fs in two respects:  it is implemented using
 * a stackable layers techniques, and its "null-node"s stack above
 * all lower-layer vnodes, not just over directory vnodes.
 *
 * The null layer has two purposes.  First, it serves as a demonstration
 * of layering by proving a layer which does nothing.  (It actually
 * does everything the loopback file system does, which is slightly
 * more than nothing.)  Second, the null layer can serve as a prototype
 * layer.  Since it provides all necessary layer framework,
 * new file system layers can be created very easily be starting
 * with a null layer.
 *
 * The remainder of this man page examines the null layer as a basis
 * for constructing new layers.
 *
 *
 * INSTANTIATING NEW NULL LAYERS
 *
 * New null layers are created with mount_null(8).
 * Mount_null(8) takes two arguments, the pathname
 * of the lower vfs (target-pn) and the pathname where the null
 * layer will appear in the namespace (alias-pn).  After
 * the null layer is put into place, the contents
 * of target-pn subtree will be aliased under alias-pn.
 *
 *
 * OPERATION OF A NULL LAYER
 *
 * The null layer is the minimum file system layer,
 * simply bypassing all possible operations to the lower layer
 * for processing there.  The majority of its activity centers
 * on the bypass routine, through which nearly all vnode operations
 * pass.
 *
 * The bypass routine accepts arbitrary vnode operations for
 * handling by the lower layer.  It begins by examing vnode
 * operation arguments and replacing any null-nodes by their
 * lower-layer equivlants.  It then invokes the operation
 * on the lower layer.  Finally, it replaces the null-nodes
 * in the arguments and, if a vnode is return by the operation,
 * stacks a null-node on top of the returned vnode.
 *
 * Although bypass handles most operations, vop_getattr, vop_lock,
 * vop_unlock, vop_inactive, vop_reclaim, and vop_print are not
 * bypassed. Vop_getattr must change the fsid being returned.
 * Vop_lock and vop_unlock must handle any locking for the
 * current vnode as well as pass the lock request down.
 * Vop_inactive and vop_reclaim are not bypassed so that
 * they can handle freeing null-layer specific data. Vop_print
 * is not bypassed to avoid excessive debugging information.
 * Also, certain vnode operations change the locking state within
 * the operation (create, mknod, remove, link, rename, mkdir, rmdir,
 * and symlink). Ideally these operations should not change the
 * lock state, but should be changed to let the caller of the
 * function unlock them. Otherwise all intermediate vnode layers
 * (such as union, umapfs, etc) must catch these functions to do
 * the necessary locking at their layer.
 *
 *
 * INSTANTIATING VNODE STACKS
 *
 * Mounting associates the null layer with a lower layer,
 * effect stacking two VFSes.  Vnode stacks are instead
 * created on demand as files are accessed.
 *
 * The initial mount creates a single vnode stack for the
 * root of the new null layer.  All other vnode stacks
 * are created as a result of vnode operations on
 * this or other null vnode stacks.
 *
 * New vnode stacks come into existance as a result of
 * an operation which returns a vnode.
 * The bypass routine stacks a null-node above the new
 * vnode before returning it to the caller.
 *
 * For example, imagine mounting a null layer with
 * "mount_null /usr/include /dev/layer/null".
 * Changing directory to /dev/layer/null will assign
 * the root null-node (which was created when the null layer was mounted).
 * Now consider opening "sys".  A vop_lookup would be
 * done on the root null-node.  This operation would bypass through
 * to the lower layer which would return a vnode representing
 * the UFS "sys".  Null_bypass then builds a null-node
 * aliasing the UFS "sys" and returns this to the caller.
 * Later operations on the null-node "sys" will repeat this
 * process when constructing other vnode stacks.
 *
 *
 * CREATING OTHER FILE SYSTEM LAYERS
 *
 * One of the easiest ways to construct new file system layers is to make
 * a copy of the null layer, rename all files and variables, and
 * then begin modifing the copy.  Sed can be used to easily rename
 * all variables.
 *
 * The umap layer is an example of a layer descended from the
 * null layer.
 *
 *
 * INVOKING OPERATIONS ON LOWER LAYERS
 *
 * There are two techniques to invoke operations on a lower layer
 * when the operation cannot be completely bypassed.  Each method
 * is appropriate in different situations.  In both cases,
 * it is the responsibility of the aliasing layer to make
 * the operation arguments "correct" for the lower layer
 * by mapping an vnode arguments to the lower layer.
 *
 * The first approach is to call the aliasing layer's bypass routine.
 * This method is most suitable when you wish to invoke the operation
 * currently being handled on the lower layer.  It has the advantage
 * that the bypass routine already must do argument mapping.
 * An example of this is null_getattrs in the null layer.
 *
 * A second approach is to directly invoke vnode operations on
 * the lower layer with the VOP_OPERATIONNAME interface.
 * The advantage of this method is that it is easy to invoke
 * arbitrary operations on the lower layer.  The disadvantage
 * is that vnode arguments must be manualy mapped.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <fs/nullfs/null.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vnode_pager.h>

static int null_bug_bypass = 0;   /* for debugging: enables bypass printf'ing */
SYSCTL_INT(_debug, OID_AUTO, nullfs_bug_bypass, CTLFLAG_RW, 
	&null_bug_bypass, 0, "");

static int	null_access(struct vop_access_args *ap);
static int	null_createvobject(struct vop_createvobject_args *ap);
static int	null_destroyvobject(struct vop_destroyvobject_args *ap);
static int	null_getattr(struct vop_getattr_args *ap);
static int	null_getvobject(struct vop_getvobject_args *ap);
static int	null_inactive(struct vop_inactive_args *ap);
static int	null_islocked(struct vop_islocked_args *ap);
static int	null_lock(struct vop_lock_args *ap);
static int	null_lookup(struct vop_lookup_args *ap);
static int	null_open(struct vop_open_args *ap);
static int	null_print(struct vop_print_args *ap);
static int	null_reclaim(struct vop_reclaim_args *ap);
static int	null_rename(struct vop_rename_args *ap);
static int	null_setattr(struct vop_setattr_args *ap);
static int	null_unlock(struct vop_unlock_args *ap);

/*
 * This is the 10-Apr-92 bypass routine.
 *    This version has been optimized for speed, throwing away some
 * safety checks.  It should still always work, but it's not as
 * robust to programmer errors.
 *
 * In general, we map all vnodes going down and unmap them on the way back.
 * As an exception to this, vnodes can be marked "unmapped" by setting
 * the Nth bit in operation's vdesc_flags.
 *
 * Also, some BSD vnode operations have the side effect of vrele'ing
 * their arguments.  With stacking, the reference counts are held
 * by the upper node, not the lower one, so we must handle these
 * side-effects here.  This is not of concern in Sun-derived systems
 * since there are no such side-effects.
 *
 * This makes the following assumptions:
 * - only one returned vpp
 * - no INOUT vpp's (Sun's vop_open has one of these)
 * - the vnode operation vector of the first vnode should be used
 *   to determine what implementation of the op should be invoked
 * - all mapped vnodes are of our vnode-type (NEEDSWORK:
 *   problems on rmdir'ing mount points and renaming?)
 */
int
null_bypass(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap;
{
	register struct vnode **this_vp_p;
	int error;
	struct vnode *old_vps[VDESC_MAX_VPS];
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i;

	if (null_bug_bypass)
		printf ("null_bypass: %s\n", descp->vdesc_name);

#ifdef DIAGNOSTIC
	/*
	 * We require at least one vp.
	 */
	if (descp->vdesc_vp_offsets == NULL ||
	    descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET)
		panic ("null_bypass: no vp's in map");
#endif

	/*
	 * Map the vnodes going in.
	 * Later, we'll invoke the operation based on
	 * the first mapped vnode's operation vector.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		vps_p[i] = this_vp_p =
			VOPARG_OFFSETTO(struct vnode**,descp->vdesc_vp_offsets[i],ap);
		/*
		 * We're not guaranteed that any but the first vnode
		 * are of our type.  Check for and don't map any
		 * that aren't.  (We must always map first vp or vclean fails.)
		 */
		if (i && (*this_vp_p == NULLVP ||
		    (*this_vp_p)->v_op != null_vnodeop_p)) {
			old_vps[i] = NULLVP;
		} else {
			old_vps[i] = *this_vp_p;
			*(vps_p[i]) = NULLVPTOLOWERVP(*this_vp_p);
			/*
			 * XXX - Several operations have the side effect
			 * of vrele'ing their vp's.  We must account for
			 * that.  (This should go away in the future.)
			 */
			if (reles & VDESC_VP0_WILLRELE)
				VREF(*this_vp_p);
		}

	}

	/*
	 * Call the operation on the lower layer
	 * with the modified argument structure.
	 */
	if (vps_p[0] && *vps_p[0])
		error = VCALL(*(vps_p[0]), descp->vdesc_offset, ap);
	else {
		printf("null_bypass: no map for %s\n", descp->vdesc_name);
		error = EINVAL;
	}

	/*
	 * Maintain the illusion of call-by-value
	 * by restoring vnodes in the argument structure
	 * to their original value.
	 */
	reles = descp->vdesc_flags;
	for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
		if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET)
			break;   /* bail out at end of list */
		if (old_vps[i]) {
			*(vps_p[i]) = old_vps[i];
#if 0
			if (reles & VDESC_VP0_WILLUNLOCK)
				VOP_UNLOCK(*(vps_p[i]), LK_THISLAYER, curproc);
#endif
			if (reles & VDESC_VP0_WILLRELE)
				vrele(*(vps_p[i]));
		}
	}

	/*
	 * Map the possible out-going vpp
	 * (Assumes that the lower layer always returns
	 * a VREF'ed vpp unless it gets an error.)
	 */
	if (descp->vdesc_vpp_offset != VDESC_NO_OFFSET &&
	    !(descp->vdesc_flags & VDESC_NOMAP_VPP) &&
	    !error) {
		/*
		 * XXX - even though some ops have vpp returned vp's,
		 * several ops actually vrele this before returning.
		 * We must avoid these ops.
		 * (This should go away when these ops are regularized.)
		 */
		if (descp->vdesc_flags & VDESC_VPP_WILLRELE)
			goto out;
		vppp = VOPARG_OFFSETTO(struct vnode***,
				 descp->vdesc_vpp_offset,ap);
		if (*vppp)
			error = null_node_create(old_vps[0]->v_mount, **vppp, *vppp);
	}

 out:
	return (error);
}

/*
 * We have to carry on the locking protocol on the null layer vnodes
 * as we progress through the tree. We also have to enforce read-only
 * if this layer is mounted read-only.
 */
static int
null_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct proc *p = cnp->cn_proc;
	int flags = cnp->cn_flags;
	struct vnode *vp, *ldvp, *lvp;
	int error;

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	/*
	 * Although it is possible to call null_bypass(), we'll do
	 * a direct call to reduce overhead
	 */
	ldvp = NULLVPTOLOWERVP(dvp);
	vp = lvp = NULL;
	error = VOP_LOOKUP(ldvp, &lvp, cnp);
	if (error == EJUSTRETURN && (flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		error = EROFS;

	/*
	 * Rely only on the PDIRUNLOCK flag which should be carefully
	 * tracked by underlying filesystem.
	 */
	if (cnp->cn_flags & PDIRUNLOCK)
		VOP_UNLOCK(dvp, LK_THISLAYER, p);
	if ((error == 0 || error == EJUSTRETURN) && lvp != NULL) {
		if (ldvp == lvp) {
			*ap->a_vpp = dvp;
			VREF(dvp);
			vrele(lvp);
		} else {
			error = null_node_create(dvp->v_mount, lvp, &vp);
			if (error == 0)
				*ap->a_vpp = vp;
		}
	}
	return (error);
}

/*
 * Setattr call. Disallow write attempts if the layer is mounted read-only.
 */
int
null_setattr(ap)
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

  	if ((vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VCHR:
 		case VBLK:
 		case VSOCK:
 		case VFIFO:
			if (vap->va_flags != VNOVAL)
				return (EOPNOTSUPP);
			return (0);
		case VREG:
		case VLNK:
 		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
		}
	}

	return (null_bypass((struct vop_generic_args *)ap));
}

/*
 *  We handle getattr only to change the fsid.
 */
static int
null_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	int error;

	if ((error = null_bypass((struct vop_generic_args *)ap)) != 0)
		return (error);

	ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	return (0);
}

/*
 * Handle to disallow write access if mounted read-only.
 */
static int
null_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	mode_t mode = ap->a_mode;

	/*
	 * Disallow write attempts on read-only layers;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			break;
		default:
			break;
		}
	}
	return (null_bypass((struct vop_generic_args *)ap));
}

/*
 * We must handle open to be able to catch MNT_NODEV and friends.
 */
static int
null_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *lvp = NULLVPTOLOWERVP(ap->a_vp);

	if ((vp->v_mount->mnt_flag & MNT_NODEV) &&
	    (lvp->v_type == VBLK || lvp->v_type == VCHR))
		return ENXIO;

	return (null_bypass((struct vop_generic_args *)ap));
}

/*
 * We handle this to eliminate null FS to lower FS
 * file moving. Don't know why we don't allow this,
 * possibly we should.
 */
static int
null_rename(ap)
	struct vop_rename_args /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tvp = ap->a_tvp;

	/* Check for cross-device rename. */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		vrele(fdvp);
		vrele(fvp);
		return (EXDEV);
	}
	
	return (null_bypass((struct vop_generic_args *)ap));
}

/*
 * We need to process our own vnode lock and then clear the
 * interlock flag as it applies only to our vnode, not the
 * vnodes below us on the stack.
 */
static int
null_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;
	struct proc *p = ap->a_p;
	struct vnode *lvp;
	int error;

	if (flags & LK_THISLAYER) {
		if (vp->v_vnlock != NULL)
			return 0;	/* lock is shared across layers */
		error = lockmgr(&vp->v_lock, flags & ~LK_THISLAYER,
		    &vp->v_interlock, p);
		return (error);
	}

	if (vp->v_vnlock != NULL) {
		/*
		 * The lower level has exported a struct lock to us. Use
		 * it so that all vnodes in the stack lock and unlock
		 * simultaneously. Note: we don't DRAIN the lock as DRAIN
		 * decommissions the lock - just because our vnode is
		 * going away doesn't mean the struct lock below us is.
		 * LK_EXCLUSIVE is fine.
		 */
		if ((flags & LK_TYPE_MASK) == LK_DRAIN) {
			NULLFSDEBUG("null_lock: avoiding LK_DRAIN\n");
			return(lockmgr(vp->v_vnlock,
				(flags & ~LK_TYPE_MASK) | LK_EXCLUSIVE,
				&vp->v_interlock, p));
		}
		return(lockmgr(vp->v_vnlock, flags, &vp->v_interlock, p));
	} else {
		/*
		 * To prevent race conditions involving doing a lookup
		 * on "..", we have to lock the lower node, then lock our
		 * node. Most of the time it won't matter that we lock our
		 * node (as any locking would need the lower one locked
		 * first). But we can LK_DRAIN the upper lock as a step
		 * towards decomissioning it.
		 */
		lvp = NULLVPTOLOWERVP(vp);
		if (lvp == NULL)
			return (lockmgr(&vp->v_lock, flags, &vp->v_interlock, p));
		if (flags & LK_INTERLOCK) {
			mtx_unlock(&vp->v_interlock);
			flags &= ~LK_INTERLOCK;
		}
		if ((flags & LK_TYPE_MASK) == LK_DRAIN) {
			error = VOP_LOCK(lvp,
				(flags & ~LK_TYPE_MASK) | LK_EXCLUSIVE, p);
		} else
			error = VOP_LOCK(lvp, flags, p);
		if (error)
			return (error);	
		error = lockmgr(&vp->v_lock, flags, &vp->v_interlock, p);
		if (error)
			VOP_UNLOCK(lvp, 0, p);
		return (error);
	}
}

/*
 * We need to process our own vnode unlock and then clear the
 * interlock flag as it applies only to our vnode, not the
 * vnodes below us on the stack.
 */
static int
null_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
		int a_flags;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;
	struct proc *p = ap->a_p;
	struct vnode *lvp;

	if (vp->v_vnlock != NULL) {
		if (flags & LK_THISLAYER)
			return 0;	/* the lock is shared across layers */
		flags &= ~LK_THISLAYER;
		return (lockmgr(vp->v_vnlock, flags | LK_RELEASE,
			&vp->v_interlock, p));
	}
	lvp = NULLVPTOLOWERVP(vp);
	if (lvp == NULL)
		return (lockmgr(&vp->v_lock, flags | LK_RELEASE, &vp->v_interlock, p));
	if ((flags & LK_THISLAYER) == 0) {
		if (flags & LK_INTERLOCK) {
			mtx_unlock(&vp->v_interlock);
			flags &= ~LK_INTERLOCK;
		}
		VOP_UNLOCK(lvp, flags & ~LK_INTERLOCK, p);
	} else
		flags &= ~LK_THISLAYER;
	return (lockmgr(&vp->v_lock, flags | LK_RELEASE, &vp->v_interlock, p));
}

static int
null_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;

	if (vp->v_vnlock != NULL)
		return (lockstatus(vp->v_vnlock, p));
	return (lockstatus(&vp->v_lock, p));
}

/*
 * There is no way to tell that someone issued remove/rmdir operation
 * on the underlying filesystem. For now we just have to release lowevrp
 * as soon as possible.
 */
static int
null_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	struct null_node *xp = VTONULL(vp);
	struct vnode *lowervp = xp->null_lowervp;

	lockmgr(&null_hashlock, LK_EXCLUSIVE, NULL, p);
	LIST_REMOVE(xp, null_hash);
	lockmgr(&null_hashlock, LK_RELEASE, NULL, p);

	xp->null_lowervp = NULLVP;
	if (vp->v_vnlock != NULL) {
		vp->v_vnlock = &vp->v_lock;	/* we no longer share the lock */
	} else
		VOP_UNLOCK(vp, LK_THISLAYER, p);

	vput(lowervp);
	/*
	 * Now it is safe to drop references to the lower vnode.
	 * VOP_INACTIVE() will be called by vrele() if necessary.
	 */
	vrele (lowervp);

	return (0);
}

/*
 * We can free memory in null_inactive, but we do this
 * here. (Possible to guard vp->v_data to point somewhere)
 */
static int
null_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	void *vdata = vp->v_data;

	vp->v_data = NULL;
	FREE(vdata, M_NULLFSNODE);

	return (0);
}

static int
null_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	printf ("\ttag VT_NULLFS, vp=%p, lowervp=%p\n", vp, NULLVPTOLOWERVP(vp));
	return (0);
}

/*
 * Let an underlying filesystem do the work
 */
static int
null_createvobject(ap)
	struct vop_createvobject_args /* {
		struct vnode *vp;
		struct ucred *cred;
		struct proc *p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *lowervp = VTONULL(vp) ? NULLVPTOLOWERVP(vp) : NULL;
	int error;

	if (vp->v_type == VNON || lowervp == NULL)
		return 0;
	error = VOP_CREATEVOBJECT(lowervp, ap->a_cred, ap->a_p);
	if (error)
		return (error);
	vp->v_flag |= VOBJBUF;
	return (0);
}

/*
 * We have nothing to destroy and this operation shouldn't be bypassed.
 */
static int
null_destroyvobject(ap)
	struct vop_destroyvobject_args /* {
		struct vnode *vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	vp->v_flag &= ~VOBJBUF;
	return (0);
}

static int
null_getvobject(ap)
	struct vop_getvobject_args /* {
		struct vnode *vp;
		struct vm_object **objpp;
	} */ *ap;
{
	struct vnode *lvp = NULLVPTOLOWERVP(ap->a_vp);

	if (lvp == NULL)
		return EINVAL;
	return (VOP_GETVOBJECT(lvp, ap->a_objpp));
}

/*
 * Global vfs data structures
 */
vop_t **null_vnodeop_p;
static struct vnodeopv_entry_desc null_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) null_bypass },

	{ &vop_access_desc,		(vop_t *) null_access },
	{ &vop_bmap_desc,		(vop_t *) vop_eopnotsupp },
	{ &vop_createvobject_desc,	(vop_t *) null_createvobject },
	{ &vop_destroyvobject_desc,	(vop_t *) null_destroyvobject },
	{ &vop_getattr_desc,		(vop_t *) null_getattr },
	{ &vop_getvobject_desc,		(vop_t *) null_getvobject },
	{ &vop_getwritemount_desc,	(vop_t *) vop_stdgetwritemount},
	{ &vop_inactive_desc,		(vop_t *) null_inactive },
	{ &vop_islocked_desc,		(vop_t *) null_islocked },
	{ &vop_lock_desc,		(vop_t *) null_lock },
	{ &vop_lookup_desc,		(vop_t *) null_lookup },
	{ &vop_open_desc,		(vop_t *) null_open },
	{ &vop_print_desc,		(vop_t *) null_print },
	{ &vop_reclaim_desc,		(vop_t *) null_reclaim },
	{ &vop_rename_desc,		(vop_t *) null_rename },
	{ &vop_setattr_desc,		(vop_t *) null_setattr },
	{ &vop_strategy_desc,		(vop_t *) vop_eopnotsupp },
	{ &vop_unlock_desc,		(vop_t *) null_unlock },
	{ NULL, NULL }
};
static struct vnodeopv_desc null_vnodeop_opv_desc =
	{ &null_vnodeop_p, null_vnodeop_entries };

VNODEOP_SET(null_vnodeop_opv_desc);
