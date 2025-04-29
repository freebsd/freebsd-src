/*	$NetBSD: tmpfs_vnops.c,v 1.39 2007/07/23 15:41:01 jmmv Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005, 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * tmpfs vnode interface.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dirent.h>
#include <sys/extattr.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/limits.h>
#include <sys/lockf.h>
#include <sys/lock.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smr.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

#include <fs/tmpfs/tmpfs_vnops.h>
#include <fs/tmpfs/tmpfs.h>

SYSCTL_DECL(_vfs_tmpfs);
VFS_SMR_DECLARE;

static volatile int tmpfs_rename_restarts;
SYSCTL_INT(_vfs_tmpfs, OID_AUTO, rename_restarts, CTLFLAG_RD,
    __DEVOLATILE(int *, &tmpfs_rename_restarts), 0,
    "Times rename had to restart due to lock contention");

MALLOC_DEFINE(M_TMPFSEA, "tmpfs extattr", "tmpfs extattr structure");

static int
tmpfs_vn_get_ino_alloc(struct mount *mp, void *arg, int lkflags,
    struct vnode **rvp)
{

	return (tmpfs_alloc_vp(mp, arg, lkflags, rvp));
}

static int
tmpfs_lookup1(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp)
{
	struct tmpfs_dirent *de;
	struct tmpfs_node *dnode, *pnode;
	struct tmpfs_mount *tm;
	int error;

	/* Caller assumes responsibility for ensuring access (VEXEC). */
	dnode = VP_TO_TMPFS_DIR(dvp);
	*vpp = NULLVP;

	/* We cannot be requesting the parent directory of the root node. */
	MPASS(IMPLIES(dnode->tn_type == VDIR &&
	    dnode->tn_dir.tn_parent == dnode,
	    !(cnp->cn_flags & ISDOTDOT)));

	TMPFS_ASSERT_LOCKED(dnode);
	if (dnode->tn_dir.tn_parent == NULL) {
		error = ENOENT;
		goto out;
	}
	if (cnp->cn_flags & ISDOTDOT) {
		tm = VFS_TO_TMPFS(dvp->v_mount);
		pnode = dnode->tn_dir.tn_parent;
		tmpfs_ref_node(pnode);
		error = vn_vget_ino_gen(dvp, tmpfs_vn_get_ino_alloc,
		    pnode, cnp->cn_lkflags, vpp);
		tmpfs_free_node(tm, pnode);
		if (error != 0)
			goto out;
	} else if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		VREF(dvp);
		*vpp = dvp;
		error = 0;
	} else {
		de = tmpfs_dir_lookup(dnode, NULL, cnp);
		if (de != NULL && de->td_node == NULL)
			cnp->cn_flags |= ISWHITEOUT;
		if (de == NULL || de->td_node == NULL) {
			/*
			 * The entry was not found in the directory.
			 * This is OK if we are creating or renaming an
			 * entry and are working on the last component of
			 * the path name.
			 */
			if ((cnp->cn_flags & ISLASTCN) &&
			    (cnp->cn_nameiop == CREATE || \
			    cnp->cn_nameiop == RENAME ||
			    (cnp->cn_nameiop == DELETE &&
			    cnp->cn_flags & DOWHITEOUT &&
			    cnp->cn_flags & ISWHITEOUT))) {
				error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred,
				    curthread);
				if (error != 0)
					goto out;

				error = EJUSTRETURN;
			} else
				error = ENOENT;
		} else {
			struct tmpfs_node *tnode;

			/*
			 * The entry was found, so get its associated
			 * tmpfs_node.
			 */
			tnode = de->td_node;

			/*
			 * If we are not at the last path component and
			 * found a non-directory or non-link entry (which
			 * may itself be pointing to a directory), raise
			 * an error.
			 */
			if ((tnode->tn_type != VDIR &&
			    tnode->tn_type != VLNK) &&
			    !(cnp->cn_flags & ISLASTCN)) {
				error = ENOTDIR;
				goto out;
			}

			/*
			 * If we are deleting or renaming the entry, keep
			 * track of its tmpfs_dirent so that it can be
			 * easily deleted later.
			 */
			if ((cnp->cn_flags & ISLASTCN) &&
			    (cnp->cn_nameiop == DELETE ||
			    cnp->cn_nameiop == RENAME)) {
				error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred,
				    curthread);
				if (error != 0)
					goto out;

				/* Allocate a new vnode on the matching entry. */
				error = tmpfs_alloc_vp(dvp->v_mount, tnode,
				    cnp->cn_lkflags, vpp);
				if (error != 0)
					goto out;

				if ((dnode->tn_mode & S_ISTXT) &&
				  VOP_ACCESS(dvp, VADMIN, cnp->cn_cred,
				  curthread) && VOP_ACCESS(*vpp, VADMIN,
				  cnp->cn_cred, curthread)) {
					error = EPERM;
					vput(*vpp);
					*vpp = NULL;
					goto out;
				}
			} else {
				error = tmpfs_alloc_vp(dvp->v_mount, tnode,
				    cnp->cn_lkflags, vpp);
				if (error != 0)
					goto out;
			}
		}
	}

	/*
	 * Store the result of this lookup in the cache.  Avoid this if the
	 * request was for creation, as it does not improve timings on
	 * emprical tests.
	 */
	if ((cnp->cn_flags & MAKEENTRY) != 0 && tmpfs_use_nc(dvp))
		cache_enter(dvp, *vpp, cnp);

out:
#ifdef INVARIANTS
	/*
	 * If there were no errors, *vpp cannot be null and it must be
	 * locked.
	 */
	if (error == 0) {
		MPASS(*vpp != NULLVP);
		ASSERT_VOP_LOCKED(*vpp, __func__);
	} else {
		MPASS(*vpp == NULL);
	}
#endif

	return (error);
}

static int
tmpfs_cached_lookup(struct vop_cachedlookup_args *v)
{

	return (tmpfs_lookup1(v->a_dvp, v->a_vpp, v->a_cnp));
}

static int
tmpfs_lookup(struct vop_lookup_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	int error;

	/* Check accessibility of requested node as a first step. */
	error = vn_dir_check_exec(dvp, cnp);
	if (error != 0)
		return (error);

	return (tmpfs_lookup1(dvp, vpp, cnp));
}

static int
tmpfs_create(struct vop_create_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;
	int error;

	MPASS(vap->va_type == VREG || vap->va_type == VSOCK);

	error = tmpfs_alloc_file(dvp, vpp, vap, cnp, NULL);
	if (error == 0 && (cnp->cn_flags & MAKEENTRY) != 0 && tmpfs_use_nc(dvp))
		cache_enter(dvp, *vpp, cnp);
	return (error);
}

static int
tmpfs_mknod(struct vop_mknod_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;

	if (vap->va_type != VBLK && vap->va_type != VCHR &&
	    vap->va_type != VFIFO)
		return (EINVAL);

	return (tmpfs_alloc_file(dvp, vpp, vap, cnp, NULL));
}

struct fileops tmpfs_fnops;

static int
tmpfs_open(struct vop_open_args *v)
{
	struct vnode *vp;
	struct tmpfs_node *node;
	struct file *fp;
	int error, mode;

	vp = v->a_vp;
	mode = v->a_mode;
	node = VP_TO_TMPFS_NODE(vp);

	/*
	 * The file is still active but all its names have been removed
	 * (e.g. by a "rmdir $(pwd)").  It cannot be opened any more as
	 * it is about to die.
	 */
	if (node->tn_links < 1)
		return (ENOENT);

	/* If the file is marked append-only, deny write requests. */
	if (node->tn_flags & APPEND && (mode & (FWRITE | O_APPEND)) == FWRITE)
		error = EPERM;
	else {
		error = 0;
		/* For regular files, the call below is nop. */
		KASSERT(vp->v_type != VREG || (node->tn_reg.tn_aobj->flags &
		    OBJ_DEAD) == 0, ("dead object"));
		vnode_create_vobject(vp, node->tn_size, v->a_td);
	}

	fp = v->a_fp;
	MPASS(fp == NULL || fp->f_data == NULL);
	if (error == 0 && fp != NULL && vp->v_type == VREG) {
		tmpfs_ref_node(node);
		finit_vnode(fp, mode, node, &tmpfs_fnops);
	}

	return (error);
}

static int
tmpfs_close(struct vop_close_args *v)
{
	struct vnode *vp = v->a_vp;

	/* Update node times. */
	tmpfs_update(vp);

	return (0);
}

int
tmpfs_fo_close(struct file *fp, struct thread *td)
{
	struct tmpfs_node *node;

	node = fp->f_data;
	if (node != NULL) {
		MPASS(node->tn_type == VREG);
		tmpfs_free_node(node->tn_reg.tn_tmp, node);
	}
	return (vnops.fo_close(fp, td));
}

/*
 * VOP_FPLOOKUP_VEXEC routines are subject to special circumstances, see
 * the comment above cache_fplookup for details.
 */
int
tmpfs_fplookup_vexec(struct vop_fplookup_vexec_args *v)
{
	struct vnode *vp;
	struct tmpfs_node *node;
	struct ucred *cred;
	mode_t all_x, mode;

	vp = v->a_vp;
	node = VP_TO_TMPFS_NODE_SMR(vp);
	if (__predict_false(node == NULL))
		return (EAGAIN);

	all_x = S_IXUSR | S_IXGRP | S_IXOTH;
	mode = atomic_load_short(&node->tn_mode);
	if (__predict_true((mode & all_x) == all_x))
		return (0);

	cred = v->a_cred;
	return (vaccess_vexec_smr(mode, node->tn_uid, node->tn_gid, cred));
}

static int
tmpfs_access_locked(struct vnode *vp, struct tmpfs_node *node,
    accmode_t accmode, struct ucred *cred)
{
#ifdef DEBUG_VFS_LOCKS
	if (!mtx_owned(TMPFS_NODE_MTX(node))) {
		ASSERT_VOP_LOCKED(vp,
		    "tmpfs_access_locked needs locked vnode or node");
	}
#endif

	if ((accmode & VWRITE) != 0 && (node->tn_flags & IMMUTABLE) != 0)
		return (EPERM);
	return (vaccess(vp->v_type, node->tn_mode, node->tn_uid, node->tn_gid,
	    accmode, cred));
}

int
tmpfs_access(struct vop_access_args *v)
{
	struct vnode *vp = v->a_vp;
	struct ucred *cred = v->a_cred;
	struct tmpfs_node *node = VP_TO_TMPFS_NODE(vp);
	mode_t all_x = S_IXUSR | S_IXGRP | S_IXOTH;
	accmode_t accmode = v->a_accmode;

	/*
	 * Common case path lookup.
	 */
	if (__predict_true(accmode == VEXEC &&
	    (node->tn_mode & all_x) == all_x))
		return (0);

	switch (vp->v_type) {
	case VDIR:
		/* FALLTHROUGH */
	case VLNK:
		/* FALLTHROUGH */
	case VREG:
		if ((accmode & VWRITE) != 0 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) != 0)
			return (EROFS);
		break;

	case VBLK:
		/* FALLTHROUGH */
	case VCHR:
		/* FALLTHROUGH */
	case VSOCK:
		/* FALLTHROUGH */
	case VFIFO:
		break;

	default:
		return (EINVAL);
	}

	return (tmpfs_access_locked(vp, node, accmode, cred));
}

int
tmpfs_stat(struct vop_stat_args *v)
{
	struct vnode *vp = v->a_vp;
	struct stat *sb = v->a_sb;
	struct tmpfs_node *node;
	int error;

	node = VP_TO_TMPFS_NODE(vp);

	tmpfs_update_getattr(vp);

	error = vop_stat_helper_pre(v);
	if (__predict_false(error))
		return (error);

	sb->st_dev = vp->v_mount->mnt_stat.f_fsid.val[0];
	sb->st_ino = node->tn_id;
	sb->st_mode = node->tn_mode | VTTOIF(vp->v_type);
	sb->st_nlink = node->tn_links;
	sb->st_uid = node->tn_uid;
	sb->st_gid = node->tn_gid;
	sb->st_rdev = (vp->v_type == VBLK || vp->v_type == VCHR) ?
		node->tn_rdev : NODEV;
	sb->st_size = node->tn_size;
	sb->st_atim.tv_sec = node->tn_atime.tv_sec;
	sb->st_atim.tv_nsec = node->tn_atime.tv_nsec;
	sb->st_mtim.tv_sec = node->tn_mtime.tv_sec;
	sb->st_mtim.tv_nsec = node->tn_mtime.tv_nsec;
	sb->st_ctim.tv_sec = node->tn_ctime.tv_sec;
	sb->st_ctim.tv_nsec = node->tn_ctime.tv_nsec;
	sb->st_birthtim.tv_sec = node->tn_birthtime.tv_sec;
	sb->st_birthtim.tv_nsec = node->tn_birthtime.tv_nsec;
	sb->st_blksize = PAGE_SIZE;
	sb->st_flags = node->tn_flags;
	sb->st_gen = node->tn_gen;
	sb->st_filerev = 0;
	if (vp->v_type == VREG) {
#ifdef __ILP32__
		vm_object_t obj = node->tn_reg.tn_aobj;

		/* Handle torn read */
		VM_OBJECT_RLOCK(obj);
#endif
		sb->st_blocks = ptoa(node->tn_reg.tn_pages);
#ifdef __ILP32__
		VM_OBJECT_RUNLOCK(obj);
#endif
	} else {
		sb->st_blocks = node->tn_size;
	}
	sb->st_blocks /= S_BLKSIZE;
	return (vop_stat_helper_post(v, error));
}

int
tmpfs_getattr(struct vop_getattr_args *v)
{
	struct vnode *vp = v->a_vp;
	struct vattr *vap = v->a_vap;
	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);

	tmpfs_update_getattr(vp);

	vap->va_type = vp->v_type;
	vap->va_mode = node->tn_mode;
	vap->va_nlink = node->tn_links;
	vap->va_uid = node->tn_uid;
	vap->va_gid = node->tn_gid;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_fileid = node->tn_id;
	vap->va_size = node->tn_size;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_atime = node->tn_atime;
	vap->va_mtime = node->tn_mtime;
	vap->va_ctime = node->tn_ctime;
	vap->va_birthtime = node->tn_birthtime;
	vap->va_gen = node->tn_gen;
	vap->va_flags = node->tn_flags;
	vap->va_rdev = (vp->v_type == VBLK || vp->v_type == VCHR) ?
	    node->tn_rdev : NODEV;
	if (vp->v_type == VREG) {
#ifdef __ILP32__
		vm_object_t obj = node->tn_reg.tn_aobj;

		VM_OBJECT_RLOCK(obj);
#endif
		vap->va_bytes = ptoa(node->tn_reg.tn_pages);
#ifdef __ILP32__
		VM_OBJECT_RUNLOCK(obj);
#endif
	} else {
		vap->va_bytes = node->tn_size;
	}
	vap->va_filerev = 0;

	return (0);
}

int
tmpfs_setattr(struct vop_setattr_args *v)
{
	struct vnode *vp = v->a_vp;
	struct vattr *vap = v->a_vap;
	struct ucred *cred = v->a_cred;
	struct thread *td = curthread;

	int error;

	ASSERT_VOP_IN_SEQC(vp);

	error = 0;

	/* Abort if any unsettable attribute is given. */
	if (vap->va_type != VNON ||
	    vap->va_nlink != VNOVAL ||
	    vap->va_fsid != VNOVAL ||
	    vap->va_fileid != VNOVAL ||
	    vap->va_blocksize != VNOVAL ||
	    vap->va_gen != VNOVAL ||
	    vap->va_rdev != VNOVAL ||
	    vap->va_bytes != VNOVAL)
		error = EINVAL;

	if (error == 0 && (vap->va_flags != VNOVAL))
		error = tmpfs_chflags(vp, vap->va_flags, cred, td);

	if (error == 0 && (vap->va_size != VNOVAL))
		error = tmpfs_chsize(vp, vap->va_size, cred, td);

	if (error == 0 && (vap->va_uid != VNOVAL || vap->va_gid != VNOVAL))
		error = tmpfs_chown(vp, vap->va_uid, vap->va_gid, cred, td);

	if (error == 0 && (vap->va_mode != (mode_t)VNOVAL))
		error = tmpfs_chmod(vp, vap->va_mode, cred, td);

	if (error == 0 && ((vap->va_atime.tv_sec != VNOVAL &&
	    vap->va_atime.tv_nsec != VNOVAL) ||
	    (vap->va_mtime.tv_sec != VNOVAL &&
	    vap->va_mtime.tv_nsec != VNOVAL) ||
	    (vap->va_birthtime.tv_sec != VNOVAL &&
	    vap->va_birthtime.tv_nsec != VNOVAL)))
		error = tmpfs_chtimes(vp, vap, cred, td);

	/*
	 * Update the node times.  We give preference to the error codes
	 * generated by this function rather than the ones that may arise
	 * from tmpfs_update.
	 */
	tmpfs_update(vp);

	return (error);
}

static int
tmpfs_read(struct vop_read_args *v)
{
	struct vnode *vp;
	struct uio *uio;
	struct tmpfs_node *node;

	vp = v->a_vp;
	if (vp->v_type != VREG)
		return (EISDIR);
	uio = v->a_uio;
	if (uio->uio_offset < 0)
		return (EINVAL);
	node = VP_TO_TMPFS_NODE(vp);
	tmpfs_set_accessed(VFS_TO_TMPFS(vp->v_mount), node);
	return (uiomove_object(node->tn_reg.tn_aobj, node->tn_size, uio));
}

static int
tmpfs_read_pgcache(struct vop_read_pgcache_args *v)
{
	struct vnode *vp;
	struct tmpfs_node *node;
	vm_object_t object;
	off_t size;
	int error;

	vp = v->a_vp;
	VNPASS((vn_irflag_read(vp) & VIRF_PGREAD) != 0, vp);

	if (v->a_uio->uio_offset < 0)
		return (EINVAL);

	error = EJUSTRETURN;
	vfs_smr_enter();

	node = VP_TO_TMPFS_NODE_SMR(vp);
	if (node == NULL)
		goto out_smr;
	MPASS(node->tn_type == VREG);
	MPASS(node->tn_refcount >= 1);
	object = node->tn_reg.tn_aobj;
	if (object == NULL)
		goto out_smr;

	MPASS(object->type == tmpfs_pager_type);
	MPASS((object->flags & (OBJ_ANON | OBJ_DEAD | OBJ_SWAP)) ==
	    OBJ_SWAP);
	if (!VN_IS_DOOMED(vp)) {
		/* size cannot become shorter due to rangelock. */
		size = node->tn_size;
		tmpfs_set_accessed(node->tn_reg.tn_tmp, node);
		vfs_smr_exit();
		error = uiomove_object(object, size, v->a_uio);
		return (error);
	}
out_smr:
	vfs_smr_exit();
	return (error);
}

static int
tmpfs_write(struct vop_write_args *v)
{
	struct vnode *vp;
	struct uio *uio;
	struct tmpfs_node *node;
	off_t oldsize;
	ssize_t r;
	int error, ioflag;
	mode_t newmode;

	vp = v->a_vp;
	uio = v->a_uio;
	ioflag = v->a_ioflag;
	error = 0;
	node = VP_TO_TMPFS_NODE(vp);
	oldsize = node->tn_size;

	if (uio->uio_offset < 0 || vp->v_type != VREG)
		return (EINVAL);
	if (uio->uio_resid == 0)
		return (0);
	if (ioflag & IO_APPEND)
		uio->uio_offset = node->tn_size;
	error = vn_rlimit_fsizex(vp, uio, VFS_TO_TMPFS(vp->v_mount)->
	    tm_maxfilesize, &r, uio->uio_td);
	if (error != 0) {
		vn_rlimit_fsizex_res(uio, r);
		return (error);
	}

	if (uio->uio_offset + uio->uio_resid > node->tn_size) {
		error = tmpfs_reg_resize(vp, uio->uio_offset + uio->uio_resid,
		    FALSE);
		if (error != 0)
			goto out;
	}

	error = uiomove_object(node->tn_reg.tn_aobj, node->tn_size, uio);
	node->tn_status |= TMPFS_NODE_MODIFIED | TMPFS_NODE_CHANGED;
	node->tn_accessed = true;
	if (node->tn_mode & (S_ISUID | S_ISGID)) {
		if (priv_check_cred(v->a_cred, PRIV_VFS_RETAINSUGID)) {
			newmode = node->tn_mode & ~(S_ISUID | S_ISGID);
			vn_seqc_write_begin(vp);
			atomic_store_short(&node->tn_mode, newmode);
			vn_seqc_write_end(vp);
		}
	}
	if (error != 0)
		(void)tmpfs_reg_resize(vp, oldsize, TRUE);

out:
	MPASS(IMPLIES(error == 0, uio->uio_resid == 0));
	MPASS(IMPLIES(error != 0, oldsize == node->tn_size));

	vn_rlimit_fsizex_res(uio, r);
	return (error);
}

static int
tmpfs_deallocate(struct vop_deallocate_args *v)
{
	return (tmpfs_reg_punch_hole(v->a_vp, v->a_offset, v->a_len));
}

static int
tmpfs_fsync(struct vop_fsync_args *v)
{
	struct vnode *vp = v->a_vp;

	tmpfs_check_mtime(vp);
	tmpfs_update(vp);

	return (0);
}

static int
tmpfs_remove(struct vop_remove_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode *vp = v->a_vp;

	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *dnode;
	struct tmpfs_node *node;

	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto out;
	}

	dnode = VP_TO_TMPFS_DIR(dvp);
	node = VP_TO_TMPFS_NODE(vp);
	tmp = VFS_TO_TMPFS(vp->v_mount);
	de = tmpfs_dir_lookup(dnode, node, v->a_cnp);
	MPASS(de != NULL);

	/* Files marked as immutable or append-only cannot be deleted. */
	if ((node->tn_flags & (IMMUTABLE | APPEND | NOUNLINK)) ||
	    (dnode->tn_flags & APPEND)) {
		error = EPERM;
		goto out;
	}

	/* Remove the entry from the directory; as it is a file, we do not
	 * have to change the number of hard links of the directory. */
	tmpfs_dir_detach(dvp, de);
	if (v->a_cnp->cn_flags & DOWHITEOUT)
		tmpfs_dir_whiteout_add(dvp, v->a_cnp);

	/* Free the directory entry we just deleted.  Note that the node
	 * referred by it will not be removed until the vnode is really
	 * reclaimed. */
	tmpfs_free_dirent(tmp, de);

	node->tn_status |= TMPFS_NODE_CHANGED;
	node->tn_accessed = true;
	error = 0;

out:
	return (error);
}

static int
tmpfs_link(struct vop_link_args *v)
{
	struct vnode *dvp = v->a_tdvp;
	struct vnode *vp = v->a_vp;
	struct componentname *cnp = v->a_cnp;

	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_node *node;

	MPASS(dvp != vp); /* XXX When can this be false? */
	node = VP_TO_TMPFS_NODE(vp);

	/* Ensure that we do not overflow the maximum number of links imposed
	 * by the system. */
	MPASS(node->tn_links <= TMPFS_LINK_MAX);
	if (node->tn_links == TMPFS_LINK_MAX) {
		error = EMLINK;
		goto out;
	}

	/* We cannot create links of files marked immutable or append-only. */
	if (node->tn_flags & (IMMUTABLE | APPEND)) {
		error = EPERM;
		goto out;
	}

	/* Allocate a new directory entry to represent the node. */
	error = tmpfs_alloc_dirent(VFS_TO_TMPFS(vp->v_mount), node,
	    cnp->cn_nameptr, cnp->cn_namelen, &de);
	if (error != 0)
		goto out;

	/* Insert the new directory entry into the appropriate directory. */
	if (cnp->cn_flags & ISWHITEOUT)
		tmpfs_dir_whiteout_remove(dvp, cnp);
	tmpfs_dir_attach(dvp, de);

	/* vp link count has changed, so update node times. */
	node->tn_status |= TMPFS_NODE_CHANGED;
	tmpfs_update(vp);

	error = 0;

out:
	return (error);
}

/*
 * We acquire all but fdvp locks using non-blocking acquisitions.  If we
 * fail to acquire any lock in the path we will drop all held locks,
 * acquire the new lock in a blocking fashion, and then release it and
 * restart the rename.  This acquire/release step ensures that we do not
 * spin on a lock waiting for release.  On error release all vnode locks
 * and decrement references the way tmpfs_rename() would do.
 */
static int
tmpfs_rename_relock(struct vnode *fdvp, struct vnode **fvpp,
    struct vnode *tdvp, struct vnode **tvpp,
    struct componentname *fcnp, struct componentname *tcnp)
{
	struct vnode *nvp;
	struct mount *mp;
	struct tmpfs_dirent *de;
	int error, restarts = 0;

	VOP_UNLOCK(tdvp);
	if (*tvpp != NULL && *tvpp != tdvp)
		VOP_UNLOCK(*tvpp);
	mp = fdvp->v_mount;

relock:
	restarts += 1;
	error = vn_lock(fdvp, LK_EXCLUSIVE);
	if (error)
		goto releout;
	if (vn_lock(tdvp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
		VOP_UNLOCK(fdvp);
		error = vn_lock(tdvp, LK_EXCLUSIVE);
		if (error)
			goto releout;
		VOP_UNLOCK(tdvp);
		goto relock;
	}
	/*
	 * Re-resolve fvp to be certain it still exists and fetch the
	 * correct vnode.
	 */
	de = tmpfs_dir_lookup(VP_TO_TMPFS_DIR(fdvp), NULL, fcnp);
	if (de == NULL) {
		VOP_UNLOCK(fdvp);
		VOP_UNLOCK(tdvp);
		if ((fcnp->cn_flags & ISDOTDOT) != 0 ||
		    (fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.'))
			error = EINVAL;
		else
			error = ENOENT;
		goto releout;
	}
	error = tmpfs_alloc_vp(mp, de->td_node, LK_EXCLUSIVE | LK_NOWAIT, &nvp);
	if (error != 0) {
		VOP_UNLOCK(fdvp);
		VOP_UNLOCK(tdvp);
		if (error != EBUSY)
			goto releout;
		error = tmpfs_alloc_vp(mp, de->td_node, LK_EXCLUSIVE, &nvp);
		if (error != 0)
			goto releout;
		VOP_UNLOCK(nvp);
		/*
		 * Concurrent rename race.
		 */
		if (nvp == tdvp) {
			vrele(nvp);
			error = EINVAL;
			goto releout;
		}
		vrele(*fvpp);
		*fvpp = nvp;
		goto relock;
	}
	vrele(*fvpp);
	*fvpp = nvp;
	VOP_UNLOCK(*fvpp);
	/*
	 * Re-resolve tvp and acquire the vnode lock if present.
	 */
	de = tmpfs_dir_lookup(VP_TO_TMPFS_DIR(tdvp), NULL, tcnp);
	/*
	 * If tvp disappeared we just carry on.
	 */
	if (de == NULL && *tvpp != NULL) {
		vrele(*tvpp);
		*tvpp = NULL;
	}
	/*
	 * Get the tvp ino if the lookup succeeded.  We may have to restart
	 * if the non-blocking acquire fails.
	 */
	if (de != NULL) {
		nvp = NULL;
		error = tmpfs_alloc_vp(mp, de->td_node,
		    LK_EXCLUSIVE | LK_NOWAIT, &nvp);
		if (*tvpp != NULL)
			vrele(*tvpp);
		*tvpp = nvp;
		if (error != 0) {
			VOP_UNLOCK(fdvp);
			VOP_UNLOCK(tdvp);
			if (error != EBUSY)
				goto releout;
			error = tmpfs_alloc_vp(mp, de->td_node, LK_EXCLUSIVE,
			    &nvp);
			if (error != 0)
				goto releout;
			VOP_UNLOCK(nvp);
			/*
			 * fdvp contains fvp, thus tvp (=fdvp) is not empty.
			 */
			if (nvp == fdvp) {
				error = ENOTEMPTY;
				goto releout;
			}
			goto relock;
		}
	}
	tmpfs_rename_restarts += restarts;

	return (0);

releout:
	vrele(fdvp);
	vrele(*fvpp);
	vrele(tdvp);
	if (*tvpp != NULL)
		vrele(*tvpp);
	tmpfs_rename_restarts += restarts;

	return (error);
}

static int
tmpfs_rename(struct vop_rename_args *v)
{
	struct vnode *fdvp = v->a_fdvp;
	struct vnode *fvp = v->a_fvp;
	struct componentname *fcnp = v->a_fcnp;
	struct vnode *tdvp = v->a_tdvp;
	struct vnode *tvp = v->a_tvp;
	struct componentname *tcnp = v->a_tcnp;
	char *newname;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *fdnode;
	struct tmpfs_node *fnode;
	struct tmpfs_node *tnode;
	struct tmpfs_node *tdnode;
	int error;
	bool want_seqc_end;

	want_seqc_end = false;

	/*
	 * Disallow cross-device renames.
	 * XXX Why isn't this done by the caller?
	 */
	if (fvp->v_mount != tdvp->v_mount ||
	    (tvp != NULL && fvp->v_mount != tvp->v_mount)) {
		error = EXDEV;
		goto out;
	}

	/* If source and target are the same file, there is nothing to do. */
	if (fvp == tvp) {
		error = 0;
		goto out;
	}

	/*
	 * If we need to move the directory between entries, lock the
	 * source so that we can safely operate on it.
	 */
	if (fdvp != tdvp && fdvp != tvp) {
		if (vn_lock(fdvp, LK_EXCLUSIVE | LK_NOWAIT) != 0) {
			error = tmpfs_rename_relock(fdvp, &fvp, tdvp, &tvp,
			    fcnp, tcnp);
			if (error != 0)
				return (error);
			ASSERT_VOP_ELOCKED(fdvp,
			    "tmpfs_rename: fdvp not locked");
			ASSERT_VOP_ELOCKED(tdvp,
			    "tmpfs_rename: tdvp not locked");
			if (tvp != NULL)
				ASSERT_VOP_ELOCKED(tvp,
				    "tmpfs_rename: tvp not locked");
			if (fvp == tvp) {
				error = 0;
				goto out_locked;
			}
		}
	}

	/*
	 * Avoid manipulating '.' and '..' entries.
	 */
	if ((fcnp->cn_flags & ISDOTDOT) != 0 ||
	    (fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.')) {
		error = EINVAL;
		goto out_locked;
	}

	if (tvp != NULL)
		vn_seqc_write_begin(tvp);
	vn_seqc_write_begin(tdvp);
	vn_seqc_write_begin(fvp);
	vn_seqc_write_begin(fdvp);
	want_seqc_end = true;

	tmp = VFS_TO_TMPFS(tdvp->v_mount);
	tdnode = VP_TO_TMPFS_DIR(tdvp);
	tnode = (tvp == NULL) ? NULL : VP_TO_TMPFS_NODE(tvp);
	fdnode = VP_TO_TMPFS_DIR(fdvp);
	fnode = VP_TO_TMPFS_NODE(fvp);
	de = tmpfs_dir_lookup(fdnode, fnode, fcnp);

	/*
	 * Entry can disappear before we lock fdvp.
	 */
	if (de == NULL) {
		if ((fcnp->cn_flags & ISDOTDOT) != 0 ||
		    (fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.'))
			error = EINVAL;
		else
			error = ENOENT;
		goto out_locked;
	}
	MPASS(de->td_node == fnode);

	/*
	 * If re-naming a directory to another preexisting directory
	 * ensure that the target directory is empty so that its
	 * removal causes no side effects.
	 * Kern_rename guarantees the destination to be a directory
	 * if the source is one.
	 */
	if (tvp != NULL) {
		MPASS(tnode != NULL);

		if ((tnode->tn_flags & (NOUNLINK | IMMUTABLE | APPEND)) ||
		    (tdnode->tn_flags & (APPEND | IMMUTABLE))) {
			error = EPERM;
			goto out_locked;
		}

		if (fnode->tn_type == VDIR && tnode->tn_type == VDIR) {
			if (tnode->tn_size != 0 &&
			    ((tcnp->cn_flags & IGNOREWHITEOUT) == 0 ||
			    tnode->tn_size > tnode->tn_dir.tn_wht_size)) {
				error = ENOTEMPTY;
				goto out_locked;
			}
		} else if (fnode->tn_type == VDIR && tnode->tn_type != VDIR) {
			error = ENOTDIR;
			goto out_locked;
		} else if (fnode->tn_type != VDIR && tnode->tn_type == VDIR) {
			error = EISDIR;
			goto out_locked;
		} else {
			MPASS(fnode->tn_type != VDIR &&
				tnode->tn_type != VDIR);
		}
	}

	if ((fnode->tn_flags & (NOUNLINK | IMMUTABLE | APPEND))
	    || (fdnode->tn_flags & (APPEND | IMMUTABLE))) {
		error = EPERM;
		goto out_locked;
	}

	/*
	 * Ensure that we have enough memory to hold the new name, if it
	 * has to be changed.
	 */
	if (fcnp->cn_namelen != tcnp->cn_namelen ||
	    bcmp(fcnp->cn_nameptr, tcnp->cn_nameptr, fcnp->cn_namelen) != 0) {
		newname = malloc(tcnp->cn_namelen, M_TMPFSNAME, M_WAITOK);
	} else
		newname = NULL;

	/*
	 * If the node is being moved to another directory, we have to do
	 * the move.
	 */
	if (fdnode != tdnode) {
		/*
		 * In case we are moving a directory, we have to adjust its
		 * parent to point to the new parent.
		 */
		if (de->td_node->tn_type == VDIR) {
			struct tmpfs_node *n;

			TMPFS_NODE_LOCK(fnode);
			error = tmpfs_access_locked(fvp, fnode, VWRITE,
			    tcnp->cn_cred);
			TMPFS_NODE_UNLOCK(fnode);
			if (error) {
				if (newname != NULL)
					free(newname, M_TMPFSNAME);
				goto out_locked;
			}

			/*
			 * Ensure the target directory is not a child of the
			 * directory being moved.  Otherwise, we'd end up
			 * with stale nodes.
			 */
			n = tdnode;
			/*
			 * TMPFS_LOCK guaranties that no nodes are freed while
			 * traversing the list. Nodes can only be marked as
			 * removed: tn_parent == NULL.
			 */
			TMPFS_LOCK(tmp);
			TMPFS_NODE_LOCK(n);
			while (n != n->tn_dir.tn_parent) {
				struct tmpfs_node *parent;

				if (n == fnode) {
					TMPFS_NODE_UNLOCK(n);
					TMPFS_UNLOCK(tmp);
					error = EINVAL;
					if (newname != NULL)
						free(newname, M_TMPFSNAME);
					goto out_locked;
				}
				parent = n->tn_dir.tn_parent;
				TMPFS_NODE_UNLOCK(n);
				if (parent == NULL) {
					n = NULL;
					break;
				}
				TMPFS_NODE_LOCK(parent);
				if (parent->tn_dir.tn_parent == NULL) {
					TMPFS_NODE_UNLOCK(parent);
					n = NULL;
					break;
				}
				n = parent;
			}
			TMPFS_UNLOCK(tmp);
			if (n == NULL) {
				error = EINVAL;
				if (newname != NULL)
					    free(newname, M_TMPFSNAME);
				goto out_locked;
			}
			TMPFS_NODE_UNLOCK(n);

			/* Adjust the parent pointer. */
			TMPFS_VALIDATE_DIR(fnode);
			TMPFS_NODE_LOCK(de->td_node);
			de->td_node->tn_dir.tn_parent = tdnode;
			TMPFS_NODE_UNLOCK(de->td_node);

			/*
			 * As a result of changing the target of the '..'
			 * entry, the link count of the source and target
			 * directories has to be adjusted.
			 */
			TMPFS_NODE_LOCK(tdnode);
			TMPFS_ASSERT_LOCKED(tdnode);
			tdnode->tn_links++;
			TMPFS_NODE_UNLOCK(tdnode);

			TMPFS_NODE_LOCK(fdnode);
			TMPFS_ASSERT_LOCKED(fdnode);
			fdnode->tn_links--;
			TMPFS_NODE_UNLOCK(fdnode);
		}
	}

	/*
	 * Do the move: just remove the entry from the source directory
	 * and insert it into the target one.
	 */
	tmpfs_dir_detach(fdvp, de);

	if (fcnp->cn_flags & DOWHITEOUT)
		tmpfs_dir_whiteout_add(fdvp, fcnp);
	if (tcnp->cn_flags & ISWHITEOUT)
		tmpfs_dir_whiteout_remove(tdvp, tcnp);

	/*
	 * If the name has changed, we need to make it effective by changing
	 * it in the directory entry.
	 */
	if (newname != NULL) {
		MPASS(tcnp->cn_namelen <= MAXNAMLEN);

		free(de->ud.td_name, M_TMPFSNAME);
		de->ud.td_name = newname;
		tmpfs_dirent_init(de, tcnp->cn_nameptr, tcnp->cn_namelen);

		fnode->tn_status |= TMPFS_NODE_CHANGED;
		tdnode->tn_status |= TMPFS_NODE_MODIFIED;
	}

	/*
	 * If we are overwriting an entry, we have to remove the old one
	 * from the target directory.
	 */
	if (tvp != NULL) {
		struct tmpfs_dirent *tde;

		/* Remove the old entry from the target directory. */
		tde = tmpfs_dir_lookup(tdnode, tnode, tcnp);
		tmpfs_dir_detach(tdvp, tde);

		/*
		 * If we are overwriting a directory, per the ENOTEMPTY check
		 * above it must either be empty or contain only whiteout
		 * entries.  In the latter case (which can only happen if
		 * IGNOREWHITEOUT was passed in tcnp->cn_flags), clear the
		 * whiteout entries to avoid leaking memory.
		 */
		if (tnode->tn_type == VDIR && tnode->tn_size > 0)
			tmpfs_dir_clear_whiteouts(tvp);

		/* Update node's ctime because of possible hardlinks. */
		tnode->tn_status |= TMPFS_NODE_CHANGED;
		tmpfs_update(tvp);

		/*
		 * Free the directory entry we just deleted.  Note that the
		 * node referred by it will not be removed until the vnode is
		 * really reclaimed.
		 */
		tmpfs_free_dirent(VFS_TO_TMPFS(tvp->v_mount), tde);
	}

	tmpfs_dir_attach(tdvp, de);

	if (tmpfs_use_nc(fvp)) {
		cache_vop_rename(fdvp, fvp, tdvp, tvp, fcnp, tcnp);
	}

	error = 0;

out_locked:
	if (fdvp != tdvp && fdvp != tvp)
		VOP_UNLOCK(fdvp);

out:
	if (want_seqc_end) {
		if (tvp != NULL)
			vn_seqc_write_end(tvp);
		vn_seqc_write_end(tdvp);
		vn_seqc_write_end(fvp);
		vn_seqc_write_end(fdvp);
	}

	/*
	 * Release target nodes.
	 * XXX: I don't understand when tdvp can be the same as tvp, but
	 * other code takes care of this...
	 */
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp != NULL)
		vput(tvp);

	/* Release source nodes. */
	vrele(fdvp);
	vrele(fvp);

	return (error);
}

static int
tmpfs_mkdir(struct vop_mkdir_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;

	MPASS(vap->va_type == VDIR);

	return (tmpfs_alloc_file(dvp, vpp, vap, cnp, NULL));
}

static int
tmpfs_rmdir(struct vop_rmdir_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode *vp = v->a_vp;
	struct componentname *cnp = v->a_cnp;

	int error;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *dnode;
	struct tmpfs_node *node;

	tmp = VFS_TO_TMPFS(dvp->v_mount);
	dnode = VP_TO_TMPFS_DIR(dvp);
	node = VP_TO_TMPFS_DIR(vp);

	/*
	 * Directories with more than two non-whiteout entries ('.' and '..')
	 * cannot be removed.
	 */
	if (node->tn_size != 0 &&
	    ((cnp->cn_flags & IGNOREWHITEOUT) == 0 ||
	    node->tn_size > node->tn_dir.tn_wht_size)) {
		error = ENOTEMPTY;
		goto out;
	}

	/* Check flags to see if we are allowed to remove the directory. */
	if ((dnode->tn_flags & APPEND)
	    || (node->tn_flags & (NOUNLINK | IMMUTABLE | APPEND))) {
		error = EPERM;
		goto out;
	}

	/* This invariant holds only if we are not trying to remove "..".
	 * We checked for that above so this is safe now. */
	MPASS(node->tn_dir.tn_parent == dnode);

	/* Get the directory entry associated with node (vp).  This was
	 * filled by tmpfs_lookup while looking up the entry. */
	de = tmpfs_dir_lookup(dnode, node, cnp);
	MPASS(TMPFS_DIRENT_MATCHES(de,
	    cnp->cn_nameptr,
	    cnp->cn_namelen));

	/* Detach the directory entry from the directory (dnode). */
	tmpfs_dir_detach(dvp, de);

	/*
	 * If we are removing a directory, per the ENOTEMPTY check above it
	 * must either be empty or contain only whiteout entries.  In the
	 * latter case (which can only happen if IGNOREWHITEOUT was passed
	 * in cnp->cn_flags), clear the whiteout entries to avoid leaking
	 * memory.
	 */
	if (node->tn_size > 0)
		tmpfs_dir_clear_whiteouts(vp);

	if (cnp->cn_flags & DOWHITEOUT)
		tmpfs_dir_whiteout_add(dvp, cnp);

	/* No vnode should be allocated for this entry from this point */
	TMPFS_NODE_LOCK(node);
	node->tn_links--;
	node->tn_dir.tn_parent = NULL;
	node->tn_status |= TMPFS_NODE_CHANGED | TMPFS_NODE_MODIFIED;
	node->tn_accessed = true;

	TMPFS_NODE_UNLOCK(node);

	TMPFS_NODE_LOCK(dnode);
	dnode->tn_links--;
	dnode->tn_status |= TMPFS_NODE_CHANGED | TMPFS_NODE_MODIFIED;
	dnode->tn_accessed = true;
	TMPFS_NODE_UNLOCK(dnode);

	if (tmpfs_use_nc(dvp)) {
		cache_vop_rmdir(dvp, vp);
	}

	/* Free the directory entry we just deleted.  Note that the node
	 * referred by it will not be removed until the vnode is really
	 * reclaimed. */
	tmpfs_free_dirent(tmp, de);

	/* Release the deleted vnode (will destroy the node, notify
	 * interested parties and clean it from the cache). */

	dnode->tn_status |= TMPFS_NODE_CHANGED;
	tmpfs_update(dvp);

	error = 0;

out:
	return (error);
}

static int
tmpfs_symlink(struct vop_symlink_args *v)
{
	struct vnode *dvp = v->a_dvp;
	struct vnode **vpp = v->a_vpp;
	struct componentname *cnp = v->a_cnp;
	struct vattr *vap = v->a_vap;
	const char *target = v->a_target;

#ifdef notyet /* XXX FreeBSD BUG: kern_symlink is not setting VLNK */
	MPASS(vap->va_type == VLNK);
#else
	vap->va_type = VLNK;
#endif

	return (tmpfs_alloc_file(dvp, vpp, vap, cnp, target));
}

static int
tmpfs_readdir(struct vop_readdir_args *va)
{
	struct vnode *vp;
	struct uio *uio;
	struct tmpfs_mount *tm;
	struct tmpfs_node *node;
	uint64_t **cookies;
	int *eofflag, *ncookies;
	ssize_t startresid;
	int error, maxcookies;

	vp = va->a_vp;
	uio = va->a_uio;
	eofflag = va->a_eofflag;
	cookies = va->a_cookies;
	ncookies = va->a_ncookies;

	/* This operation only makes sense on directory nodes. */
	if (vp->v_type != VDIR)
		return (ENOTDIR);

	maxcookies = 0;
	node = VP_TO_TMPFS_DIR(vp);
	tm = VFS_TO_TMPFS(vp->v_mount);

	startresid = uio->uio_resid;

	/* Allocate cookies for NFS and compat modules. */
	if (cookies != NULL && ncookies != NULL) {
		maxcookies = howmany(node->tn_size,
		    sizeof(struct tmpfs_dirent)) + 2;
		*cookies = malloc(maxcookies * sizeof(**cookies), M_TEMP,
		    M_WAITOK);
		*ncookies = 0;
	}

	if (cookies == NULL)
		error = tmpfs_dir_getdents(tm, node, uio, 0, NULL, NULL);
	else
		error = tmpfs_dir_getdents(tm, node, uio, maxcookies, *cookies,
		    ncookies);

	/* Buffer was filled without hitting EOF. */
	if (error == EJUSTRETURN)
		error = (uio->uio_resid != startresid) ? 0 : EINVAL;

	if (error != 0 && cookies != NULL && ncookies != NULL) {
		free(*cookies, M_TEMP);
		*cookies = NULL;
		*ncookies = 0;
	}

	if (eofflag != NULL)
		*eofflag =
		    (error == 0 && uio->uio_offset == TMPFS_DIRCOOKIE_EOF);

	return (error);
}

static int
tmpfs_readlink(struct vop_readlink_args *v)
{
	struct vnode *vp = v->a_vp;
	struct uio *uio = v->a_uio;

	int error;
	struct tmpfs_node *node;

	MPASS(uio->uio_offset == 0);
	MPASS(vp->v_type == VLNK);

	node = VP_TO_TMPFS_NODE(vp);

	error = uiomove(node->tn_link_target, MIN(node->tn_size, uio->uio_resid),
	    uio);
	tmpfs_set_accessed(VFS_TO_TMPFS(vp->v_mount), node);

	return (error);
}

/*
 * VOP_FPLOOKUP_SYMLINK routines are subject to special circumstances, see
 * the comment above cache_fplookup for details.
 *
 * Check tmpfs_alloc_node for tmpfs-specific synchronisation notes.
 */
static int
tmpfs_fplookup_symlink(struct vop_fplookup_symlink_args *v)
{
	struct vnode *vp;
	struct tmpfs_node *node;
	char *symlink;

	vp = v->a_vp;
	node = VP_TO_TMPFS_NODE_SMR(vp);
	if (__predict_false(node == NULL))
		return (EAGAIN);
	if (!atomic_load_char(&node->tn_link_smr))
		return (EAGAIN);
	symlink = atomic_load_ptr(&node->tn_link_target);
	if (symlink == NULL)
		return (EAGAIN);

	return (cache_symlink_resolve(v->a_fpl, symlink, node->tn_size));
}

static int
tmpfs_inactive(struct vop_inactive_args *v)
{
	struct vnode *vp;
	struct tmpfs_node *node;

	vp = v->a_vp;
	node = VP_TO_TMPFS_NODE(vp);
	if (node->tn_links == 0)
		vrecycle(vp);
	else
		tmpfs_check_mtime(vp);
	return (0);
}

static int
tmpfs_need_inactive(struct vop_need_inactive_args *ap)
{
	struct vnode *vp;
	struct tmpfs_node *node;
	struct vm_object *obj;

	vp = ap->a_vp;
	node = VP_TO_TMPFS_NODE(vp);
	if (node->tn_links == 0)
		goto need;
	if (vp->v_type == VREG) {
		obj = vp->v_object;
		if (obj->generation != obj->cleangeneration)
			goto need;
	}
	return (0);
need:
	return (1);
}

int
tmpfs_reclaim(struct vop_reclaim_args *v)
{
	struct vnode *vp;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;
	bool unlock;

	vp = v->a_vp;
	node = VP_TO_TMPFS_NODE(vp);
	tmp = VFS_TO_TMPFS(vp->v_mount);

	if (vp->v_type == VREG)
		tmpfs_destroy_vobject(vp, node->tn_reg.tn_aobj);
	vp->v_object = NULL;

	TMPFS_LOCK(tmp);
	TMPFS_NODE_LOCK(node);
	tmpfs_free_vp(vp);

	/*
	 * If the node referenced by this vnode was deleted by the user,
	 * we must free its associated data structures (now that the vnode
	 * is being reclaimed).
	 */
	unlock = true;
	if (node->tn_links == 0 &&
	    (node->tn_vpstate & TMPFS_VNODE_ALLOCATING) == 0) {
		node->tn_vpstate = TMPFS_VNODE_DOOMED;
		unlock = !tmpfs_free_node_locked(tmp, node, true);
	}

	if (unlock) {
		TMPFS_NODE_UNLOCK(node);
		TMPFS_UNLOCK(tmp);
	}

	MPASS(vp->v_data == NULL);
	return (0);
}

int
tmpfs_print(struct vop_print_args *v)
{
	struct vnode *vp = v->a_vp;

	struct tmpfs_node *node;

	node = VP_TO_TMPFS_NODE(vp);

	printf("tag VT_TMPFS, tmpfs_node %p, flags 0x%lx, links %jd\n",
	    node, node->tn_flags, (uintmax_t)node->tn_links);
	printf("\tmode 0%o, owner %d, group %d, size %jd, status 0x%x\n",
	    node->tn_mode, node->tn_uid, node->tn_gid,
	    (intmax_t)node->tn_size, node->tn_status);

	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);

	printf("\n");

	return (0);
}

int
tmpfs_pathconf(struct vop_pathconf_args *v)
{
	struct vnode *vp = v->a_vp;
	int name = v->a_name;
	long *retval = v->a_retval;

	int error;

	error = 0;

	switch (name) {
	case _PC_LINK_MAX:
		*retval = TMPFS_LINK_MAX;
		break;

	case _PC_SYMLINK_MAX:
		*retval = MAXPATHLEN;
		break;

	case _PC_NAME_MAX:
		*retval = NAME_MAX;
		break;

	case _PC_PIPE_BUF:
		if (vp->v_type == VDIR || vp->v_type == VFIFO)
			*retval = PIPE_BUF;
		else
			error = EINVAL;
		break;

	case _PC_CHOWN_RESTRICTED:
		*retval = 1;
		break;

	case _PC_NO_TRUNC:
		*retval = 1;
		break;

	case _PC_SYNC_IO:
		*retval = 1;
		break;

	case _PC_FILESIZEBITS:
		*retval = 64;
		break;

	case _PC_MIN_HOLE_SIZE:
		*retval = PAGE_SIZE;
		break;

	default:
		error = vop_stdpathconf(v);
	}

	return (error);
}

static int
tmpfs_vptofh(struct vop_vptofh_args *ap)
/*
vop_vptofh {
	IN struct vnode *a_vp;
	IN struct fid *a_fhp;
};
*/
{
	struct tmpfs_fid_data *const tfd = (struct tmpfs_fid_data *)ap->a_fhp;
	struct tmpfs_node *node;
	_Static_assert(sizeof(struct tmpfs_fid_data) <= sizeof(struct fid),
	    "struct tmpfs_fid_data cannot be larger than struct fid");

	node = VP_TO_TMPFS_NODE(ap->a_vp);
	tfd->tfd_len = sizeof(*tfd);
	tfd->tfd_gen = node->tn_gen;
	tfd->tfd_id = node->tn_id;

	return (0);
}

static int
tmpfs_whiteout(struct vop_whiteout_args *ap)
{
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct tmpfs_dirent *de;

	switch (ap->a_flags) {
	case LOOKUP:
		return (0);
	case CREATE:
		de = tmpfs_dir_lookup(VP_TO_TMPFS_DIR(dvp), NULL, cnp);
		if (de != NULL)
			return (de->td_node == NULL ? 0 : EEXIST);
		return (tmpfs_dir_whiteout_add(dvp, cnp));
	case DELETE:
		tmpfs_dir_whiteout_remove(dvp, cnp);
		return (0);
	default:
		panic("tmpfs_whiteout: unknown op");
	}
}

static int
tmpfs_vptocnp_dir(struct tmpfs_node *tn, struct tmpfs_node *tnp,
    struct tmpfs_dirent **pde)
{
	struct tmpfs_dir_cursor dc;
	struct tmpfs_dirent *de;

	for (de = tmpfs_dir_first(tnp, &dc); de != NULL;
	     de = tmpfs_dir_next(tnp, &dc)) {
		if (de->td_node == tn) {
			*pde = de;
			return (0);
		}
	}
	return (ENOENT);
}

static int
tmpfs_vptocnp_fill(struct vnode *vp, struct tmpfs_node *tn,
    struct tmpfs_node *tnp, char *buf, size_t *buflen, struct vnode **dvp)
{
	struct tmpfs_dirent *de;
	int error, i;

	error = vn_vget_ino_gen(vp, tmpfs_vn_get_ino_alloc, tnp, LK_SHARED,
	    dvp);
	if (error != 0)
		return (error);
	error = tmpfs_vptocnp_dir(tn, tnp, &de);
	if (error == 0) {
		i = *buflen;
		i -= de->td_namelen;
		if (i < 0) {
			error = ENOMEM;
		} else {
			bcopy(de->ud.td_name, buf + i, de->td_namelen);
			*buflen = i;
		}
	}
	if (error == 0) {
		if (vp != *dvp)
			VOP_UNLOCK(*dvp);
	} else {
		if (vp != *dvp)
			vput(*dvp);
		else
			vrele(vp);
	}
	return (error);
}

static int
tmpfs_vptocnp(struct vop_vptocnp_args *ap)
{
	struct vnode *vp, **dvp;
	struct tmpfs_node *tn, *tnp, *tnp1;
	struct tmpfs_dirent *de;
	struct tmpfs_mount *tm;
	char *buf;
	size_t *buflen;
	int error;

	vp = ap->a_vp;
	dvp = ap->a_vpp;
	buf = ap->a_buf;
	buflen = ap->a_buflen;

	tm = VFS_TO_TMPFS(vp->v_mount);
	tn = VP_TO_TMPFS_NODE(vp);
	if (tn->tn_type == VDIR) {
		tnp = tn->tn_dir.tn_parent;
		if (tnp == NULL)
			return (ENOENT);
		tmpfs_ref_node(tnp);
		error = tmpfs_vptocnp_fill(vp, tn, tn->tn_dir.tn_parent, buf,
		    buflen, dvp);
		tmpfs_free_node(tm, tnp);
		return (error);
	}
restart:
	TMPFS_LOCK(tm);
restart_locked:
	LIST_FOREACH_SAFE(tnp, &tm->tm_nodes_used, tn_entries, tnp1) {
		if (tnp->tn_type != VDIR)
			continue;
		TMPFS_NODE_LOCK(tnp);
		tmpfs_ref_node(tnp);

		/*
		 * tn_vnode cannot be instantiated while we hold the
		 * node lock, so the directory cannot be changed while
		 * we iterate over it.  Do this to avoid instantiating
		 * vnode for directories which cannot point to our
		 * node.
		 */
		error = tnp->tn_vnode == NULL ? tmpfs_vptocnp_dir(tn, tnp,
		    &de) : 0;

		if (error == 0) {
			TMPFS_NODE_UNLOCK(tnp);
			TMPFS_UNLOCK(tm);
			error = tmpfs_vptocnp_fill(vp, tn, tnp, buf, buflen,
			    dvp);
			if (error == 0) {
				tmpfs_free_node(tm, tnp);
				return (0);
			}
			if (VN_IS_DOOMED(vp)) {
				tmpfs_free_node(tm, tnp);
				return (ENOENT);
			}
			TMPFS_LOCK(tm);
			TMPFS_NODE_LOCK(tnp);
		}
		if (tmpfs_free_node_locked(tm, tnp, false)) {
			goto restart;
		} else {
			KASSERT(tnp->tn_refcount > 0,
			    ("node %p refcount zero", tnp));
			if (tnp->tn_attached) {
				tnp1 = LIST_NEXT(tnp, tn_entries);
				TMPFS_NODE_UNLOCK(tnp);
			} else {
				TMPFS_NODE_UNLOCK(tnp);
				goto restart_locked;
			}
		}
	}
	TMPFS_UNLOCK(tm);
	return (ENOENT);
}

void
tmpfs_extattr_free(struct tmpfs_extattr *ea)
{
	free(ea->ea_name, M_TMPFSEA);
	free(ea->ea_value, M_TMPFSEA);
	free(ea, M_TMPFSEA);
}

static bool
tmpfs_extattr_update_mem(struct tmpfs_mount *tmp, ssize_t size)
{
	TMPFS_LOCK(tmp);
	if (size > 0 &&
	    !tmpfs_pages_check_avail(tmp, howmany(size, PAGE_SIZE))) {
		TMPFS_UNLOCK(tmp);
		return (false);
	}
	if (tmp->tm_ea_memory_inuse + size > tmp->tm_ea_memory_max) {
		TMPFS_UNLOCK(tmp);
		return (false);
	}
	tmp->tm_ea_memory_inuse += size;
	TMPFS_UNLOCK(tmp);
	return (true);
}

static int
tmpfs_deleteextattr(struct vop_deleteextattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;
	struct tmpfs_extattr *ea;
	size_t namelen;
	ssize_t diff;
	int error;

	node = VP_TO_TMPFS_NODE(vp);
	tmp = VFS_TO_TMPFS(vp->v_mount);
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VWRITE);
	if (error != 0)
		return (error);
	if (ap->a_name == NULL || ap->a_name[0] == '\0')
		return (EINVAL);
	namelen = strlen(ap->a_name);
	if (namelen > EXTATTR_MAXNAMELEN)
		return (EINVAL);

	LIST_FOREACH(ea, &node->tn_extattrs, ea_extattrs) {
		if (ea->ea_namespace == ap->a_attrnamespace &&
		    namelen == ea->ea_namelen &&
		    memcmp(ap->a_name, ea->ea_name, namelen) == 0)
			break;
	}

	if (ea == NULL)
		return (ENOATTR);
	LIST_REMOVE(ea, ea_extattrs);
	diff = -(sizeof(struct tmpfs_extattr) + namelen + ea->ea_size);
	tmpfs_extattr_update_mem(tmp, diff);
	tmpfs_extattr_free(ea);
	return (0);
}

static int
tmpfs_getextattr(struct vop_getextattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct tmpfs_node *node;
	struct tmpfs_extattr *ea;
	size_t namelen;
	int error;

	node = VP_TO_TMPFS_NODE(vp);
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VREAD);
	if (error != 0)
		return (error);
	if (ap->a_name == NULL || ap->a_name[0] == '\0')
		return (EINVAL);
	namelen = strlen(ap->a_name);
	if (namelen > EXTATTR_MAXNAMELEN)
		return (EINVAL);

	LIST_FOREACH(ea, &node->tn_extattrs, ea_extattrs) {
		if (ea->ea_namespace == ap->a_attrnamespace &&
		    namelen == ea->ea_namelen &&
		    memcmp(ap->a_name, ea->ea_name, namelen) == 0)
			break;
	}

	if (ea == NULL)
		return (ENOATTR);
	if (ap->a_size != NULL)
		*ap->a_size = ea->ea_size;
	if (ap->a_uio != NULL && ea->ea_size != 0)
		error = uiomove(ea->ea_value, ea->ea_size, ap->a_uio);
	return (error);
}

static int
tmpfs_listextattr(struct vop_listextattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct tmpfs_node *node;
	struct tmpfs_extattr *ea;
	int error;

	node = VP_TO_TMPFS_NODE(vp);
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VREAD);
	if (error != 0)
		return (error);
	if (ap->a_size != NULL)
		*ap->a_size = 0;

	LIST_FOREACH(ea, &node->tn_extattrs, ea_extattrs) {
		if (ea->ea_namespace != ap->a_attrnamespace)
			continue;
		if (ap->a_size != NULL)
			*ap->a_size += ea->ea_namelen + 1;
		if (ap->a_uio != NULL) {
			error = uiomove(&ea->ea_namelen, 1, ap->a_uio);
			if (error != 0)
				break;
			error = uiomove(ea->ea_name, ea->ea_namelen, ap->a_uio);
			if (error != 0)
				break;
		}
	}

	return (error);
}

static int
tmpfs_setextattr(struct vop_setextattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct tmpfs_mount *tmp;
	struct tmpfs_node *node;
	struct tmpfs_extattr *ea;
	struct tmpfs_extattr *new_ea;
	size_t attr_size;
	size_t namelen;
	ssize_t diff;
	int error;

	node = VP_TO_TMPFS_NODE(vp);
	tmp = VFS_TO_TMPFS(vp->v_mount);
	attr_size = ap->a_uio->uio_resid;
	diff = 0;
	if (ap->a_vp->v_type == VCHR || ap->a_vp->v_type == VBLK)
		return (EOPNOTSUPP);
	error = extattr_check_cred(ap->a_vp, ap->a_attrnamespace,
	    ap->a_cred, ap->a_td, VWRITE);
	if (error != 0)
		return (error);
	if (ap->a_name == NULL || ap->a_name[0] == '\0')
		return (EINVAL);
	namelen = strlen(ap->a_name);
	if (namelen > EXTATTR_MAXNAMELEN)
		return (EINVAL);

	LIST_FOREACH(ea, &node->tn_extattrs, ea_extattrs) {
		if (ea->ea_namespace == ap->a_attrnamespace &&
		    namelen == ea->ea_namelen &&
		    memcmp(ap->a_name, ea->ea_name, namelen) == 0) {
			diff -= sizeof(struct tmpfs_extattr) + ea->ea_namelen +
			    ea->ea_size;
			break;
		}
	}

	diff += sizeof(struct tmpfs_extattr) + namelen + attr_size;
	if (!tmpfs_extattr_update_mem(tmp, diff))
		return (ENOSPC);
	new_ea = malloc(sizeof(struct tmpfs_extattr), M_TMPFSEA, M_WAITOK);
	new_ea->ea_namespace = ap->a_attrnamespace;
	new_ea->ea_name = malloc(namelen, M_TMPFSEA, M_WAITOK);
	new_ea->ea_namelen = namelen;
	memcpy(new_ea->ea_name, ap->a_name, namelen);
	if (attr_size != 0) {
		new_ea->ea_value = malloc(attr_size, M_TMPFSEA, M_WAITOK);
		new_ea->ea_size = attr_size;
		error = uiomove(new_ea->ea_value, attr_size, ap->a_uio);
	} else {
		new_ea->ea_value = NULL;
		new_ea->ea_size = 0;
	}
	if (error != 0) {
		tmpfs_extattr_update_mem(tmp, -diff);
		tmpfs_extattr_free(new_ea);
		return (error);
	}
	if (ea != NULL) {
		LIST_REMOVE(ea, ea_extattrs);
		tmpfs_extattr_free(ea);
	}
	LIST_INSERT_HEAD(&node->tn_extattrs, new_ea, ea_extattrs);
	return (0);
}

static off_t
tmpfs_seek_data_locked(vm_object_t obj, off_t noff)
{
	vm_pindex_t p;

	p = swap_pager_seek_data(obj, OFF_TO_IDX(noff));
	return (p == OFF_TO_IDX(noff) ? noff : IDX_TO_OFF(p));
}

static int
tmpfs_seek_clamp(struct tmpfs_node *tn, off_t *noff, bool seekdata)
{
	if (*noff < tn->tn_size)
		return (0);
	if (seekdata)
		return (ENXIO);
	*noff = tn->tn_size;
	return (0);
}

static off_t
tmpfs_seek_hole_locked(vm_object_t obj, off_t noff)
{

	return (IDX_TO_OFF(swap_pager_seek_hole(obj, OFF_TO_IDX(noff))));
}

static int
tmpfs_seek_datahole(struct vnode *vp, off_t *off, bool seekdata)
{
	struct tmpfs_node *tn;
	vm_object_t obj;
	off_t noff;
	int error;

	if (vp->v_type != VREG)
		return (ENOTTY);
	tn = VP_TO_TMPFS_NODE(vp);
	noff = *off;
	if (noff < 0)
		return (ENXIO);
	error = tmpfs_seek_clamp(tn, &noff, seekdata);
	if (error != 0)
		return (error);
	obj = tn->tn_reg.tn_aobj;

	VM_OBJECT_RLOCK(obj);
	noff = seekdata ? tmpfs_seek_data_locked(obj, noff) :
	    tmpfs_seek_hole_locked(obj, noff);
	VM_OBJECT_RUNLOCK(obj);

	error = tmpfs_seek_clamp(tn, &noff, seekdata);
	if (error == 0)
		*off = noff;
	return (error);
}

static int
tmpfs_ioctl(struct vop_ioctl_args *ap)
{
	struct vnode *vp = ap->a_vp;
	int error = 0;

	switch (ap->a_command) {
	case FIOSEEKDATA:
	case FIOSEEKHOLE:
		error = vn_lock(vp, LK_SHARED);
		if (error != 0) {
			error = EBADF;
			break;
		}
		error = tmpfs_seek_datahole(vp, (off_t *)ap->a_data,
		    ap->a_command == FIOSEEKDATA);
		VOP_UNLOCK(vp);
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

/*
 * Vnode operations vector used for files stored in a tmpfs file system.
 */
struct vop_vector tmpfs_vnodeop_entries = {
	.vop_default =			&default_vnodeops,
	.vop_lookup =			vfs_cache_lookup,
	.vop_cachedlookup =		tmpfs_cached_lookup,
	.vop_create =			tmpfs_create,
	.vop_mknod =			tmpfs_mknod,
	.vop_open =			tmpfs_open,
	.vop_close =			tmpfs_close,
	.vop_fplookup_vexec =		tmpfs_fplookup_vexec,
	.vop_fplookup_symlink =		tmpfs_fplookup_symlink,
	.vop_access =			tmpfs_access,
	.vop_stat =			tmpfs_stat,
	.vop_getattr =			tmpfs_getattr,
	.vop_setattr =			tmpfs_setattr,
	.vop_read =			tmpfs_read,
	.vop_read_pgcache =		tmpfs_read_pgcache,
	.vop_write =			tmpfs_write,
	.vop_deallocate =		tmpfs_deallocate,
	.vop_fsync =			tmpfs_fsync,
	.vop_remove =			tmpfs_remove,
	.vop_link =			tmpfs_link,
	.vop_rename =			tmpfs_rename,
	.vop_mkdir =			tmpfs_mkdir,
	.vop_rmdir =			tmpfs_rmdir,
	.vop_symlink =			tmpfs_symlink,
	.vop_readdir =			tmpfs_readdir,
	.vop_readlink =			tmpfs_readlink,
	.vop_inactive =			tmpfs_inactive,
	.vop_need_inactive =		tmpfs_need_inactive,
	.vop_reclaim =			tmpfs_reclaim,
	.vop_print =			tmpfs_print,
	.vop_pathconf =			tmpfs_pathconf,
	.vop_vptofh =			tmpfs_vptofh,
	.vop_whiteout =			tmpfs_whiteout,
	.vop_bmap =			VOP_EOPNOTSUPP,
	.vop_vptocnp =			tmpfs_vptocnp,
	.vop_lock1 =			vop_lock,
	.vop_unlock = 			vop_unlock,
	.vop_islocked = 		vop_islocked,
	.vop_deleteextattr =		tmpfs_deleteextattr,
	.vop_getextattr =		tmpfs_getextattr,
	.vop_listextattr =		tmpfs_listextattr,
	.vop_setextattr =		tmpfs_setextattr,
	.vop_add_writecount =		vop_stdadd_writecount_nomsync,
	.vop_ioctl =			tmpfs_ioctl,
};
VFS_VOP_VECTOR_REGISTER(tmpfs_vnodeop_entries);

/*
 * Same vector for mounts which do not use namecache.
 */
struct vop_vector tmpfs_vnodeop_nonc_entries = {
	.vop_default =			&tmpfs_vnodeop_entries,
	.vop_lookup =			tmpfs_lookup,
};
VFS_VOP_VECTOR_REGISTER(tmpfs_vnodeop_nonc_entries);
