/*-
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
 *	@(#)vpsfs_vnops.c	8.6 (Berkeley) 5/27/95
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *	...and...
 *	@(#)vpsfs_vnodeops.c 1.20 92/07/07 UCLA Ficus project
 *
 * $FreeBSD: head/sys/fs/nullfs/vpsfs_vnops.c 250505 2013-05-11 11:17:44Z kib $
 */

/*
 * Null Layer
 *
 * (See mount_nullfs(8) for more information.)
 *
 * The null layer duplicates a portion of the filesystem
 * name space under a new name.  In this respect, it is
 * similar to the loopback filesystem.  It differs from
 * the loopback fs in two respects:  it is implemented using
 * a stackable layers techniques, and its "null-node"s stack above
 * all lower-layer vnodes, not just over directory vnodes.
 *
 * The null layer has two purposes.  First, it serves as a demonstration
 * of layering by proving a layer which does nothing.  (It actually
 * does everything the loopback filesystem does, which is slightly
 * more than nothing.)  Second, the null layer can serve as a prototype
 * layer.  Since it provides all necessary layer framework,
 * new filesystem layers can be created very easily be starting
 * with a null layer.
 *
 * The remainder of this man page examines the null layer as a basis
 * for constructing new layers.
 *
 *
 * INSTANTIATING NEW VPSFS LAYERS
 *
 * New null layers are created with mount_nullfs(8).
 * Mount_nullfs(8) takes two arguments, the pathname
 * of the lower vfs (target-pn) and the pathname where the null
 * layer will appear in the namespace (alias-pn).  After
 * the null layer is put into place, the contents
 * of target-pn subtree will be aliased under alias-pn.
 *
 *
 * OPERATION OF A VPSFS LAYER
 *
 * The null layer is the minimum filesystem layer,
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
 * "mount_nullfs /usr/include /dev/layer/null".
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
 * One of the easiest ways to construct new filesystem layers is to make
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
 * by mapping a vnode arguments to the lower layer.
 *
 * The first approach is to call the aliasing layer's bypass routine.
 * This method is most suitable when you wish to invoke the operation
 * currently being handled on the lower layer.  It has the advantage
 * that the bypass routine already must do argument mapping.
 * An example of this is vpsfs_getattrs in the null layer.
 *
 * A second approach is to directly invoke vnode operations on
 * the lower layer with the VOP_OPERATIONNAME interface.
 * The advantage of this method is that it is easy to invoke
 * arbitrary operations on the lower layer.  The disadvantage
 * is that vnode arguments must be manualy mapped.
 *
 */

#ifdef VPS

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
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/taskqueue.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vnode_pager.h>

#include <fs/vpsfs/vpsfs.h>

static int vpsfs_bug_bypass = 0;   /* for debugging:
    enables bypass printf'ing */
SYSCTL_INT(_debug, OID_AUTO, nullfs_bug_bypass, CTLFLAG_RW, 
	&vpsfs_bug_bypass, 0, "");

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
vpsfs_bypass(struct vop_generic_args *ap)
{
	struct vnode **this_vp_p;
	int error;
	struct vnode *old_vps[VDESC_MAX_VPS];
	struct vnode **vps_p[VDESC_MAX_VPS];
	struct vnode ***vppp;
	struct vnodeop_desc *descp = ap->a_desc;
	int reles, i;

	if (vpsfs_bug_bypass)
		printf ("vpsfs_bypass: %s\n", descp->vdesc_name);

#ifdef DIAGNOSTIC
	/*
	 * We require at least one vp.
	 */
	if (descp->vdesc_vp_offsets == NULL ||
	    descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET)
		panic ("vpsfs_bypass: no vp's in map");
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
		     VOPARG_OFFSETTO(struct vnode**,
		     descp->vdesc_vp_offsets[i], ap);
		/*
		 * We're not guaranteed that any but the first vnode
		 * are of our type.  Check for and don't map any
		 * that aren't.
		 * (We must always map first vp or vclean fails.)
		 */
		if (i && (*this_vp_p == NULLVP ||
		    (*this_vp_p)->v_op != &vpsfs_vnodeops)) {
			old_vps[i] = NULLVP;
		} else {
			old_vps[i] = *this_vp_p;
			*(vps_p[i]) = VPSFSVPTOLOWERVP(*this_vp_p);
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
		error = VCALL(ap);
	else {
		printf("vpsfs_bypass: no map for %s\n", descp->vdesc_name);
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
				VOP_UNLOCK(*(vps_p[i]), 0);
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
			error = vpsfs_nodeget(old_vps[0]->v_mount,
			    **vppp, *vppp);
	}

 out:
	VPSFSDEBUG("%s: vps_p[0]=%p old_vps[0]=%p error=%d\n",
	    __func__, vps_p[0], old_vps[0], error);
	return (error);
}

static int
vpsfs_add_writecount(struct vop_add_writecount_args *ap)
{
	struct vnode *lvp, *vp;
	int error;

	vp = ap->a_vp;
	lvp = VPSFSVPTOLOWERVP(vp);
	KASSERT(vp->v_writecount + ap->a_inc >= 0,
	    ("wrong writecount inc"));
	if (vp->v_writecount > 0 && vp->v_writecount + ap->a_inc == 0)
		error = VOP_ADD_WRITECOUNT(lvp, -1);
	else if (vp->v_writecount == 0 && vp->v_writecount + ap->a_inc > 0)
		error = VOP_ADD_WRITECOUNT(lvp, 1);
	else
		error = 0;
	if (error == 0)
		vp->v_writecount += ap->a_inc;
	return (error);
}

/*
 * We have to carry on the locking protocol on the null layer vnodes
 * as we progress through the tree. We also have to enforce read-only
 * if this layer is mounted read-only.
 */
static int
vpsfs_lookup(struct vop_lookup_args *ap)
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	int flags = cnp->cn_flags;
	struct vnode *vp, *ldvp, *lvp;
	int error;
	int deny = 0;

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	/*
	 * Although it is possible to call vpsfs_bypass(), we'll do
	 * a direct call to reduce overhead
	 */
	ldvp = VPSFSVPTOLOWERVP(dvp);
	vp = lvp = NULL;
	error = VOP_LOOKUP(ldvp, &lvp, cnp);
	if (error == EJUSTRETURN && (flags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME))
		error = EROFS;

	if (error == 0 && strcmp(cnp->cn_nameptr, "vpsfs_usage") == 0)
		deny = 1;

	if ((error == 0 || error == EJUSTRETURN) && lvp != NULL) {
		if (ldvp == lvp) {
			*ap->a_vpp = dvp;
			VREF(dvp);
			vrele(lvp);
		} else {
			error = vpsfs_nodeget(dvp->v_mount, lvp, &vp);
			if (error == 0)
				*ap->a_vpp = vp;
			if (deny == 1) {
				VTOVPSFS(vp)->vpsfs_flags |= VPSFSV_FORBIDDEN;
				VPSFSDEBUG("%s: setting VPSFSV_FORBIDDEN vp=%p\n",
				    __func__, vp);
			}
		}
	}
	return (error);
}

static int
vpsfs_open(struct vop_open_args *ap)
{
	struct vnode *vp, *ldvp;
	struct vattr va;
	size_t osize;
	int retval;
	int error;

	vp = ap->a_vp;
	ldvp = VPSFSVPTOLOWERVP(vp);

	if (VTOVPSFS(vp)->vpsfs_flags & VPSFSV_FORBIDDEN)
		return (EPERM);

	/* XXX apparently vop_setattr() is called to set size to
	   0 anyways ! */
	if (0 && ap->a_mode & (FWRITE | O_TRUNC)) {
		vhold(ldvp);
		error = VOP_GETATTR(ldvp, &va, ap->a_cred);
		vdrop(ldvp);
		if (error)
			return (error);
		osize = va.va_size;
		if (osize > 0) {
			VPSFSDEBUG("%s: a_mode & (FWRITE | O_TRUNC): "
			    "osize=%zu\n --> freeing %zu bytes\n",
			    __func__, osize, osize);
			vpsfs_limit_free(
			    MOUNTTOVPSFSMOUNT(vp->v_mount), osize, 0);
		}
	}

	retval = vpsfs_bypass(&ap->a_gen);
	if (retval == 0)
		vp->v_object = ldvp->v_object;
	return (retval);
}

/*
 * Setattr call. Disallow write attempts if the layer is mounted read-only.
 */
static int
vpsfs_setattr(struct vop_setattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	struct vnode *lvp;
	struct vattr va2;
	size_t osize;
	size_t nsize;
	int error;

	if (VTOVPSFS(vp)->vpsfs_flags & VPSFSV_FORBIDDEN)
		return (EPERM);

  	if ((vap->va_flags != VNOVAL ||
	    vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL ||
	    vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL ||
	    vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
#if 1   
	/*
	 * The point of this is to share common files (binaries, libraries)
	 * accross vpsfs mounts by using hard links on the backing layer.
	 * XXX This was only meant as a quick hack for some tests.
	 */
	if (vap->va_flags != VNOVAL) {
		lvp = VPSFSVPTOLOWERVP(vp);
		vhold(lvp);
		error = VOP_GETATTR(lvp, &va2, ap->a_cred);
		vdrop(lvp);
		if (error)
			return (error);

		/* Explicitely check for this flag and deny any
		   operation. */
		if (va2.va_flags & SF_IMMUTABLE) {
			return (EPERM);
		}
	}
#endif  

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

		lvp = VPSFSVPTOLOWERVP(vp);
		vhold(lvp);
		error = VOP_GETATTR(lvp, &va2, ap->a_cred);
		vdrop(lvp);
		if (error)
			return (error);
		osize = va2.va_size;
		nsize = vap->va_size;
		if (nsize > osize) {
			VPSFSDEBUG("%s: allocating %zu bytes: osize=%zu "
			    "nsize=%zu\n",
			    __func__, (nsize - osize), osize, nsize);
			if ((error = vpsfs_limit_alloc(
			    MOUNTTOVPSFSMOUNT(vp->v_mount), (nsize - osize),
			    0)) != 0)
				return (error);
		} else if (osize > nsize) {
			VPSFSDEBUG("%s: freeing %zu bytes: osize=%zu "
			    "nsize=%zu\n",
			    __func__, (osize - nsize), osize, nsize);
			vpsfs_limit_free(MOUNTTOVPSFSMOUNT(vp->v_mount),
			    (osize - nsize), 0);
		} else {
			VPSFSDEBUG("%s: no size change: %zu/%zu\n",
				__func__, osize, nsize);
		}

	}

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

/*
 *  We handle getattr only to change the fsid.
 */
static int
vpsfs_getattr(struct vop_getattr_args *ap)
{
	int error;

	VPSFSDEBUG("%s: ap=%p\n", __func__, ap);

	if ((error = vpsfs_bypass((struct vop_generic_args *)ap)) != 0)
		return (error);

	ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	return (0);
}

/*
 * Handle to disallow write access if mounted read-only.
 */
static int
vpsfs_access(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	accmode_t accmode = ap->a_accmode;

	VPSFSDEBUG("%s: vp=%p accmode=%08x\n", __func__, vp, accmode);

	if (VTOVPSFS(vp)->vpsfs_flags & VPSFSV_FORBIDDEN) {
		VPSFSDEBUG("%s: vp=%p f_deny==1\n", __func__, vp);
		return (EPERM);
	}

	/*
	 * Disallow write attempts on read-only layers;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the filesystem.
	 */
	if (accmode & VWRITE) {
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
	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_accessx(struct vop_accessx_args *ap)
{
	struct vnode *vp = ap->a_vp;
	accmode_t accmode = ap->a_accmode;

	VPSFSDEBUG("%s: vp=%p accmode=%08x\n", __func__, vp, accmode);

	if (VTOVPSFS(vp)->vpsfs_flags & VPSFSV_FORBIDDEN) {
		VPSFSDEBUG("%s: vp=%p f_deny==1\n", __func__, vp);
		return (EPERM);
	}

	/*
	 * Disallow write attempts on read-only layers;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the filesystem.
	 */
	if (accmode & VWRITE) {
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
	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

/*
 * Increasing refcount of lower vnode is needed at least for the case
 * when lower FS is NFS to do sillyrename if the file is in use.
 * Unfortunately v_usecount is incremented in many places in
 * the kernel and, as such, there may be races that result in
 * the NFS client doing an extraneous silly rename, but that seems
 * preferable to not doing a silly rename when it is needed.
 */
static int
vpsfs_remove(struct vop_remove_args *ap)
{
	int retval, vreleit;
	struct vnode *lvp;
	struct vnode *uvp;
	struct vattr *vap;
	struct vattr va;
	size_t osize;
	int error;

	/* --- */
	uvp = ap->a_vp;

	if (VTOVPSFS(uvp)->vpsfs_flags & VPSFSV_FORBIDDEN)
		return (EPERM);

	lvp = VPSFSVPTOLOWERVP(uvp);
	vhold(lvp);
	vap = &va;

	error = VOP_GETATTR(lvp, vap, curthread->td_ucred);
	if (error) {
		vdrop(lvp);
		return (error);
	}
	osize = va.va_size;
	vdrop(lvp);

	if (va.va_nlink == 1) {
		VPSFSDEBUG("%s: freeing %zu bytes, 1 node\n",
		    __func__, osize);
		vpsfs_limit_free(MOUNTTOVPSFSMOUNT(uvp->v_mount),
		    va.va_size, 1);
	} else {
		VPSFSDEBUG("%s: va_nlink=%d, not freeing space, 1 node\n",
		    __func__, va.va_nlink);
		vpsfs_limit_free(MOUNTTOVPSFSMOUNT(uvp->v_mount), 0, 1);
	}

	VPSFSDEBUG("%s: va_nlink=%d uvp=%p lvp=%p uvp->v_usecount=%d "
	    "lvp->v_usecount=%d\n",
	    __func__, va.va_nlink, uvp, lvp,
	    uvp->v_usecount, lvp->v_usecount);

	if (uvp->v_usecount > 1 && va.va_nlink == 1) {
		/*
		 * This vnode is still referenced (open) by some process,
		 * but we are about to delete it.
		 */
		VPSFSDEBUG("%s: #########################\n", __func__);
	}
	/* --- */

	if (vrefcnt(ap->a_vp) > 1) {
		lvp = VPSFSVPTOLOWERVP(ap->a_vp);
		VREF(lvp);
		vreleit = 1;
	} else
		vreleit = 0;

	retval = vpsfs_bypass(&ap->a_gen);
	if (vreleit != 0)
		vrele(lvp);
	return (retval);
}

/*
 * We handle this to eliminate null FS to lower FS
 * file moving. Don't know why we don't allow this,
 * possibly we should.
 */
static int
vpsfs_rename(struct vop_rename_args *ap)
{
	struct vnode *tdvp = ap->a_tdvp;
	struct vnode *fvp = ap->a_fvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tvp = ap->a_tvp;

	if (VTOVPSFS(fvp)->vpsfs_flags & VPSFSV_FORBIDDEN) {
		if (tdvp == tvp)
			vrele(tdvp);
		else
			vput(tdvp);
		if (tvp)
			vput(tvp);
		vrele(fdvp);
		vrele(fvp);
		return (EPERM);
	}

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
	
	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

/*
 * We need to process our own vnode lock and then clear the
 * interlock flag as it applies only to our vnode, not the
 * vnodes below us on the stack.
 */
static int
vpsfs_lock(struct vop_lock1_args *ap)
{
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;
	struct vpsfs_node *nn;
	struct vnode *lvp;
	int error;


	if ((flags & LK_INTERLOCK) == 0) {
		VI_LOCK(vp);
		ap->a_flags = flags |= LK_INTERLOCK;
	}
	nn = VTOVPSFS(vp);
	/*
	 * If we're still active we must ask the lower layer to
	 * lock as ffs has special lock considerations in it's
	 * vop lock.
	 */
	if (nn != NULL && (lvp = VPSFSVPTOLOWERVP(vp)) != NULL) {
		VI_LOCK_FLAGS(lvp, MTX_DUPOK);
		VI_UNLOCK(vp);
		/*
		 * We have to hold the vnode here to solve a potential
		 * reclaim race.  If we're forcibly vgone'd while we
		 * still have refs, a thread could be sleeping inside
		 * the lowervp's vop_lock routine.  When we vgone we will
		 * drop our last ref to the lowervp, which would allow it
		 * to be reclaimed.  The lowervp could then be recycled,
		 * in which case it is not legal to be sleeping in it's VOP.
		 * We prevent it from being recycled by holding the vnode
		 * here.
		 */
		vholdl(lvp);
		error = VOP_LOCK(lvp, flags);

		/*
		 * We might have slept to get the lock and someone might
		 * have clean our vnode already, switching vnode lock from
		 * one in lowervp to v_lock in our own vnode structure.
		 * Handle this case by reacquiring correct lock in
		 * requested mode.
		 */
		if (VTOVPSFS(vp) == NULL && error == 0) {
			ap->a_flags &= ~(LK_TYPE_MASK | LK_INTERLOCK);
			switch (flags & LK_TYPE_MASK) {
			case LK_SHARED:
				ap->a_flags |= LK_SHARED;
				break;
			case LK_UPGRADE:
			case LK_EXCLUSIVE:
				ap->a_flags |= LK_EXCLUSIVE;
				break;
			default:
				panic("Unsupported lock request %d\n",
				    ap->a_flags);
			}
			VOP_UNLOCK(lvp, 0);
			error = vop_stdlock(ap);
		}
		vdrop(lvp);
	} else
		error = vop_stdlock(ap);

	return (error);
}

/*
 * We need to process our own vnode unlock and then clear the
 * interlock flag as it applies only to our vnode, not the
 * vnodes below us on the stack.
 */
static int
vpsfs_unlock(struct vop_unlock_args *ap)
{
	struct vnode *vp = ap->a_vp;
	int flags = ap->a_flags;
	int mtxlkflag = 0;
	struct vpsfs_node *nn;
	struct vnode *lvp;
	int error;

	if ((flags & LK_INTERLOCK) != 0)
		mtxlkflag = 1;
	else if (mtx_owned(VI_MTX(vp)) == 0) {
		VI_LOCK(vp);
		mtxlkflag = 2;
	}
	nn = VTOVPSFS(vp);
	if (nn != NULL && (lvp = VPSFSVPTOLOWERVP(vp)) != NULL) {
		VI_LOCK_FLAGS(lvp, MTX_DUPOK);
		flags |= LK_INTERLOCK;
		vholdl(lvp);
		VI_UNLOCK(vp);
		error = VOP_UNLOCK(lvp, flags);
		vdrop(lvp);
		if (mtxlkflag == 0)
			VI_LOCK(vp);
	} else {
		if (mtxlkflag == 2)
			VI_UNLOCK(vp);
		error = vop_stdunlock(ap);
	}

	return (error);
}

/*
 * Do not allow the VOP_INACTIVE to be passed to the lower layer,
 * since the reference count on the lower vnode is not related to
 * ours.
 */
static int
vpsfs_inactive(struct vop_inactive_args *ap __unused)
{
	struct vnode *vp, *lvp;
	struct vpsfs_node *xp;
	struct mount *mp;
	struct vpsfs_mount *xmp;

	vp = ap->a_vp;
	xp = VTOVPSFS(vp);
	lvp = VPSFSVPTOLOWERVP(vp);
	mp = vp->v_mount;
	xmp = MOUNTTOVPSFSMOUNT(mp);
	if ((xmp->vpsfsm_flags & VPSFSM_CACHE) == 0 ||
	    (xp->vpsfs_flags & VPSFSV_DROP) != 0 ||
	    (lvp->v_vflag & VV_NOSYNC) != 0) {
		/*
		 * If this is the last reference and caching of the
		 * nullfs vnodes is not enabled, or the lower vnode is
		 * deleted, then free up the vnode so as not to tie up
		 * the lower vnodes.
		 */
		vp->v_object = NULL;
		vrecycle(vp);
	}
	return (0);
}

/*
 * Now, the nullfs vnode and, due to the sharing lock, the lower
 * vnode, are exclusively locked, and we shall destroy the null vnode.
 */
static int
vpsfs_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp;
	struct vpsfs_node *xp;
	struct vnode *lowervp;

	vp = ap->a_vp;
	xp = VTOVPSFS(vp);
	lowervp = xp->vpsfs_lowervp;

	KASSERT(lowervp != NULL && vp->v_vnlock != &vp->v_lock,
	    ("Reclaiming inclomplete null vnode %p", vp));

	vpsfs_hashrem(xp);
	/*
	 * Use the interlock to protect the clearing of v_data to
	 * prevent faults in vpsfs_lock().
	 */
	lockmgr(&vp->v_lock, LK_EXCLUSIVE, NULL);
	VI_LOCK(vp);
	vp->v_data = NULL;
	vp->v_object = NULL;
	vp->v_vnlock = &vp->v_lock;
	VI_UNLOCK(vp);

	/*
	 * If we were opened for write, we leased one write reference
	 * to the lower vnode.  If this is a reclamation due to the
	 * forced unmount, undo the reference now.
	 */
	if (vp->v_writecount > 0)
		VOP_ADD_WRITECOUNT(lowervp, -1);
	if ((xp->vpsfs_flags & VPSFSV_NOUNLOCK) != 0)
		vunref(lowervp);
	else
		vput(lowervp);
	free(xp, M_VPSFSNODE);

	return (0);
}

static int
vpsfs_print(struct vop_print_args *ap)
{
	struct vnode *vp = ap->a_vp;

	printf("\tvp=%p, lowervp=%p\n", vp, VTOVPSFS(vp)->vpsfs_lowervp);
	return (0);
}

/* ARGSUSED */
static int
vpsfs_getwritemount(struct vop_getwritemount_args *ap)
{
	struct vpsfs_node *xp;
	struct vnode *lowervp;
	struct vnode *vp;

	vp = ap->a_vp;
	VI_LOCK(vp);
	xp = VTOVPSFS(vp);
	if (xp && (lowervp = xp->vpsfs_lowervp)) {
		VI_LOCK_FLAGS(lowervp, MTX_DUPOK);
		VI_UNLOCK(vp);
		vholdl(lowervp);
		VI_UNLOCK(lowervp);
		VOP_GETWRITEMOUNT(lowervp, ap->a_mpp);
		vdrop(lowervp);
	} else {
		VI_UNLOCK(vp);
		*(ap->a_mpp) = NULL;
	}
	return (0);
}

static int
vpsfs_vptofh(struct vop_vptofh_args *ap)
{
	struct vnode *lvp;

	lvp = VPSFSVPTOLOWERVP(ap->a_vp);
	return VOP_VPTOFH(lvp, ap->a_fhp);
}

static int
vpsfs_vptocnp(struct vop_vptocnp_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vnode **dvp = ap->a_vpp;
	struct vnode *lvp, *ldvp;
	struct ucred *cred = ap->a_cred;
	int error, locked;

	if (0 && vp->v_type == VDIR)
		return (vop_stdvptocnp(ap));

	locked = VOP_ISLOCKED(vp);
	lvp = VPSFSVPTOLOWERVP(vp);
	vhold(lvp);
	VOP_UNLOCK(vp, 0); /* vp is held by vn_vptocnp_locked that
	    called us */
	ldvp = lvp;
	vref(lvp);
	error = vn_vptocnp(&ldvp, cred, ap->a_buf, ap->a_buflen);
	VPSFSDEBUG("vn_vptocnp(&ldvp, ...) = %d ap->a_buf[%s]/[%s] "
	    "*ap->a_buflen=%d\n",
	    error, ap->a_buf, &ap->a_buf[*ap->a_buflen], *ap->a_buflen);
	vdrop(lvp);
	if (error != 0) {
		vn_lock(vp, locked | LK_RETRY);
		return (ENOENT);
	}

	/*
	 * Exclusive lock is required by insmntque1 call in
	 * vpsfs_nodeget()
	 */
	error = vn_lock(ldvp, LK_EXCLUSIVE);
	if (error != 0) {
		vrele(ldvp);
		vn_lock(vp, locked | LK_RETRY);
		return (ENOENT);
	}
	vref(ldvp);
	error = vpsfs_nodeget(vp->v_mount, ldvp, dvp);
	if (error == 0) {
#ifdef DIAGNOSTIC
		VPSFSVPTOLOWERVP(*dvp);
#endif
		VOP_UNLOCK(*dvp, 0); /* keep reference on *dvp */
	}
	vn_lock(vp, locked | LK_RETRY);
	return (error);
}

/* --- */

static int
vpsfs_write(struct vop_write_args *ap)
{
	struct vnode *lvp;
	struct vnode *uvp;
	struct uio *uio;
	struct vattr *vap;
	struct vattr va;
	size_t osize;
	size_t asize;
	int error;

	uio = ap->a_uio;
	uvp = ap->a_vp;
	lvp = VPSFSVPTOLOWERVP(uvp);
	vhold(lvp);
	vap = &va;

	error = VOP_GETATTR(lvp, vap, ap->a_cred);
	if (error) {
		vdrop(lvp);
		return (error);
	}
	osize = va.va_size;
	VPSFSDEBUG("%s: vap->va_type=%d vap->va_size=%llu "
	    "vap->va_bytes=%llu\n",
	    __func__, vap->va_type,
	    (long long unsigned int)vap->va_size,
	    (long long unsigned int)vap->va_bytes);
	vdrop(lvp);

	if ((uio->uio_offset + uio->uio_resid) > osize) {
		VPSFSDEBUG("%s: allocating %zu bytes\n",
		    __func__, ((size_t)uio->uio_offset + uio->uio_resid) -
		    osize);
		asize = (uio->uio_offset + uio->uio_resid) - osize;
		if ((error = vpsfs_limit_alloc(
		    MOUNTTOVPSFSMOUNT(uvp->v_mount), asize, 0)) != 0)
			return (error);
	}

	VPSFSDEBUG("%s: uio->uio_offset=%zu uio->uio_resid=%zu\n",
	    __func__, (size_t)uio->uio_offset, uio->uio_resid);

	error = vpsfs_bypass((struct vop_generic_args *)ap);

	return (error);
}

static int
vpsfs_read(struct vop_read_args *ap)
{

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_readdir(struct vop_readdir_args *ap)
{

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_link(struct vop_link_args *ap)
{
	int error;

	if (VTOVPSFS(ap->a_vp)->vpsfs_flags & VPSFSV_FORBIDDEN)
		return (EPERM);

	VPSFSDEBUG("%s: allocating 1 node\n", __func__);
	if ((error = vpsfs_limit_alloc(
	    MOUNTTOVPSFSMOUNT(ap->a_vp->v_mount), 0, 1)) != 0)
		return (error);

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_create(struct vop_create_args *ap)
{
	int error;

	VPSFSDEBUG("%s: allocating 1 node\n", __func__);
	if ((error = vpsfs_limit_alloc(
	    MOUNTTOVPSFSMOUNT(ap->a_dvp->v_mount), 0, 1)) != 0)
		return (error);

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_symlink(struct vop_symlink_args *ap)
{
	size_t size;
	int error;

	size = strlen(ap->a_target);

	VPSFSDEBUG("%s: allocating %zu bytes, 1 node\n", __func__, size);
	if ((error = vpsfs_limit_alloc(
	    MOUNTTOVPSFSMOUNT(ap->a_dvp->v_mount), size, 1)) != 0)
		return (error);

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_mkdir(struct vop_mkdir_args *ap)
{
	int error;

	VPSFSDEBUG("%s: allocating 1 node\n", __func__);
	if ((error = vpsfs_limit_alloc(
	    MOUNTTOVPSFSMOUNT(ap->a_dvp->v_mount), 0, 1)) != 0)
		return (error);

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_mknod(struct vop_mknod_args *ap)
{
	int error;

	VPSFSDEBUG("%s: allocating 1 node\n", __func__);
	if ((error = vpsfs_limit_alloc(
	    MOUNTTOVPSFSMOUNT(ap->a_dvp->v_mount), 0, 1)) != 0)
		return (error);

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

static int
vpsfs_rmdir(struct vop_rmdir_args *ap)
{

	VPSFSDEBUG("%s: freeing 1 node\n", __func__);
	vpsfs_limit_free(MOUNTTOVPSFSMOUNT(ap->a_vp->v_mount), 0, 1);

	return (vpsfs_bypass((struct vop_generic_args *)ap));
}

/* --- */

/*
 * Global vfs data structures
 */
struct vop_vector vpsfs_vnodeops = {
	.vop_bypass =		vpsfs_bypass,
	.vop_access =		vpsfs_access,
	.vop_accessx =		vpsfs_accessx,
	.vop_advlockpurge =	vop_stdadvlockpurge,
	.vop_bmap =		VOP_EOPNOTSUPP,
	.vop_getattr =		vpsfs_getattr,
	.vop_getwritemount =	vpsfs_getwritemount,
	.vop_inactive =		vpsfs_inactive,
	.vop_islocked =		vop_stdislocked,
	.vop_lock1 =		vpsfs_lock,
	.vop_lookup =		vpsfs_lookup,
	.vop_open =		vpsfs_open,
	.vop_print =		vpsfs_print,
	.vop_reclaim =		vpsfs_reclaim,
	.vop_remove =		vpsfs_remove,
	.vop_rename =		vpsfs_rename,
	.vop_setattr =		vpsfs_setattr,
	.vop_strategy =		VOP_EOPNOTSUPP,
	.vop_unlock =		vpsfs_unlock,
	.vop_vptocnp =		vpsfs_vptocnp,
	.vop_vptofh =		vpsfs_vptofh,
	.vop_add_writecount =	vpsfs_add_writecount,
	.vop_write =		vpsfs_write,
	.vop_read =		vpsfs_read,
	.vop_readdir =		vpsfs_readdir,
	.vop_link =		vpsfs_link,
	.vop_create =		vpsfs_create,
	.vop_symlink =		vpsfs_symlink,
	.vop_mkdir =		vpsfs_mkdir,
	.vop_mknod =		vpsfs_mknod,
	.vop_rmdir =		vpsfs_rmdir,
};

#endif /* VPS */

