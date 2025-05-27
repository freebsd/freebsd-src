/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993, 1994, 1995 Jan-Simon Pendry.
 * Copyright (c) 1992, 1993, 1994, 1995
 *      The Regents of the University of California.
 * Copyright (c) 2005, 2006, 2012 Masanori Ozawa <ozawa@ongs.co.jp>, ONGS Inc.
 * Copyright (c) 2006, 2012 Daichi Goto <daichi@freebsd.org>
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
 * 3. Neither the name of the University nor the names of its contributors
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
#include <sys/kdb.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/dirent.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>

#include <fs/unionfs/union.h>

#include <machine/atomic.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vnode_pager.h>

#if 0
#define UNIONFS_INTERNAL_DEBUG(msg, args...)    printf(msg, ## args)
#define UNIONFS_IDBG_RENAME
#else
#define UNIONFS_INTERNAL_DEBUG(msg, args...)
#endif

#define KASSERT_UNIONFS_VNODE(vp) \
	VNASSERT(((vp)->v_op == &unionfs_vnodeops), vp, \
	    ("%s: non-unionfs vnode", __func__))

static bool
unionfs_lookup_isroot(struct componentname *cnp, struct vnode *dvp)
{
	struct nameidata *ndp;

	if (dvp == NULL)
		return (false);
	if ((dvp->v_vflag & VV_ROOT) != 0)
		return (true);
	ndp = vfs_lookup_nameidata(cnp);
	if (ndp == NULL)
		return (false);
	return (vfs_lookup_isroot(ndp, dvp));
}

static int
unionfs_lookup(struct vop_cachedlookup_args *ap)
{
	struct unionfs_node *dunp, *unp;
	struct vnode   *dvp, *udvp, *ldvp, *vp, *uvp, *lvp, *dtmpvp;
	struct vattr	va;
	struct componentname *cnp;
	struct thread  *td;
	uint64_t	cnflags;
	u_long		nameiop;
	int		lockflag;
	int		lkflags;
	int		error, uerror, lerror;

	lockflag = 0;
	error = uerror = lerror = ENOENT;
	cnp = ap->a_cnp;
	nameiop = cnp->cn_nameiop;
	cnflags = cnp->cn_flags;
	dvp = ap->a_dvp;
	dunp = VTOUNIONFS(dvp);
	udvp = dunp->un_uppervp;
	ldvp = dunp->un_lowervp;
	vp = uvp = lvp = NULLVP;
	td = curthread;
	*(ap->a_vpp) = NULLVP;

	UNIONFS_INTERNAL_DEBUG(
	    "unionfs_lookup: enter: nameiop=%ld, flags=%lx, path=%s\n",
	    nameiop, cnflags, cnp->cn_nameptr);

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * If read-only and op is not LOOKUP, will return EROFS.
	 */
	if ((cnflags & ISLASTCN) &&
	    (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    LOOKUP != nameiop)
		return (EROFS);

	/*
	 * Note that a lookup is in-flight, and block if another lookup
	 * is already in-flight against dvp.  This is done because we may
	 * end up dropping dvp's lock to look up a lower vnode or to create
	 * a shadow directory, opening up the possibility of parallel lookups
	 * against the same directory creating duplicate unionfs vnodes for
	 * the same file(s).  Note that if this function encounters an
	 * in-progress lookup for the directory, it will block until the
	 * lookup is complete and then return ERELOOKUP to allow any
	 * existing unionfs vnode to be loaded from the VFS cache.
	 * This is really a hack; filesystems that support MNTK_LOOKUP_SHARED
	 * (which unionfs currently doesn't) seem to deal with this by using
	 * the vfs_hash_* functions to manage a per-mount vnode cache keyed
	 * by the inode number (or some roughly equivalent unique ID
	 * usually assocated with the storage medium).  It may make sense
	 * for unionfs to adopt something similar as a replacement for its
	 * current half-baked directory-only cache implementation, particularly
	 * if we want to support MNTK_LOOKUP_SHARED here.
	 */
	error = unionfs_set_in_progress_flag(dvp, UNIONFS_LOOKUP_IN_PROGRESS);
	if (error != 0)
		return (error);
	/*
	 * lookup dotdot
	 */
	if (cnflags & ISDOTDOT) {
		if (LOOKUP != nameiop && udvp == NULLVP) {
			error = EROFS;
			goto unionfs_lookup_return;
		}

		if (unionfs_lookup_isroot(cnp, udvp) ||
		    unionfs_lookup_isroot(cnp, ldvp)) {
			error = ENOENT;
			goto unionfs_lookup_return;
		}

		if (udvp != NULLVP)
			dtmpvp = udvp;
		else
			dtmpvp = ldvp;

		unionfs_forward_vop_start(dtmpvp, &lkflags);
		error = VOP_LOOKUP(dtmpvp, &vp, cnp);
		unionfs_forward_vop_finish(dvp, dtmpvp, lkflags);

		/*
		 * Drop the lock and reference on vp.  If the lookup was
		 * successful, we'll either need to exchange vp's lock and
		 * reference for the unionfs parent vnode's lock and
		 * reference, or (if dvp was reclaimed) we'll need to drop
		 * vp's lock and reference to return early.
		 */
		if (vp != NULLVP)
			vput(vp);
		dunp = VTOUNIONFS(dvp);
		if (error == 0 && dunp == NULL)
			error = ENOENT;

		if (error == 0) {
			dtmpvp = dunp->un_dvp;
			vref(dtmpvp);
			VOP_UNLOCK(dvp);
			*(ap->a_vpp) = dtmpvp;

			vn_lock(dtmpvp, cnp->cn_lkflags | LK_RETRY);

			if (VN_IS_DOOMED(dtmpvp)) {
				vput(dtmpvp);
				*(ap->a_vpp) = NULLVP;
				error = ENOENT;
			}
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
		}

		goto unionfs_lookup_cleanup;
	}

	/*
	 * Lookup lower layer.  We do this before looking up the the upper
	 * layer, as we may drop the upper parent directory's lock, and we
	 * want to ensure the upper parent remains locked from the point of
	 * lookup through any ensuing VOP that may require it to be locked.
	 * The cost of this is that we may end up performing an unnecessary
	 * lower layer lookup if a whiteout is present in the upper layer.
	 */
	if (ldvp != NULLVP && !(cnflags & DOWHITEOUT)) {
		struct componentname lcn;
		bool is_dot;

		if (udvp != NULLVP) {
			vref(ldvp);
			VOP_UNLOCK(dvp);
			vn_lock(ldvp, LK_EXCLUSIVE | LK_RETRY);
		}

		lcn = *cnp;
		/* always op is LOOKUP */
		lcn.cn_nameiop = LOOKUP;
		lcn.cn_flags = cnflags;
		is_dot = false;

		if (udvp == NULLVP)
			unionfs_forward_vop_start(ldvp, &lkflags);
		lerror = VOP_LOOKUP(ldvp, &lvp, &lcn);
		if (udvp == NULLVP &&
		    unionfs_forward_vop_finish(dvp, ldvp, lkflags)) {
			if (lvp != NULLVP)
				VOP_UNLOCK(lvp);
			error =  ENOENT;
			goto unionfs_lookup_cleanup;
		}

		if (udvp == NULLVP)
			cnp->cn_flags = lcn.cn_flags;

		if (lerror == 0) {
			if (ldvp == lvp) {	/* is dot */
				vrele(lvp);
				*(ap->a_vpp) = dvp;
				vref(dvp);
				is_dot = true;
				error = lerror;
			} else if (lvp != NULLVP)
				VOP_UNLOCK(lvp);
		}

		if (udvp != NULLVP) {
			vput(ldvp);
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
			if (VN_IS_DOOMED(dvp))
				error = ENOENT;
		}
		if (is_dot)
			goto unionfs_lookup_return;
		else if (error != 0)
			goto unionfs_lookup_cleanup;
	}
	/*
	 * lookup upper layer
	 */
	if (udvp != NULLVP) {
		bool iswhiteout = false;

		unionfs_forward_vop_start(udvp, &lkflags);
		uerror = VOP_LOOKUP(udvp, &uvp, cnp);
		if (unionfs_forward_vop_finish(dvp, udvp, lkflags)) {
			if (uvp != NULLVP)
				VOP_UNLOCK(uvp);
			error = ENOENT;
			goto unionfs_lookup_cleanup;
		}

		if (uerror == 0) {
			if (udvp == uvp) {	/* is dot */
				if (lvp != NULLVP)
					vrele(lvp);
				vrele(uvp);
				*(ap->a_vpp) = dvp;
				vref(dvp);

				error = uerror;
				goto unionfs_lookup_return;
			} else if (uvp != NULLVP)
				VOP_UNLOCK(uvp);
		}

		/* check whiteout */
		if ((uerror == ENOENT || uerror == EJUSTRETURN) &&
		    (cnp->cn_flags & ISWHITEOUT))
			iswhiteout = true;
		else if (VOP_GETATTR(udvp, &va, cnp->cn_cred) == 0 &&
		    (va.va_flags & OPAQUE))
			iswhiteout = true;

		if (iswhiteout && lvp != NULLVP) {
			vrele(lvp);
			lvp = NULLVP;
		}

#if 0
		UNIONFS_INTERNAL_DEBUG(
		    "unionfs_lookup: debug: whiteout=%d, path=%s\n",
		    iswhiteout, cnp->cn_nameptr);
#endif
	}

	/*
	 * check lookup result
	 */
	if (uvp == NULLVP && lvp == NULLVP) {
		error = (udvp != NULLVP ? uerror : lerror);
		goto unionfs_lookup_return;
	}

	/*
	 * check vnode type
	 */
	if (uvp != NULLVP && lvp != NULLVP && uvp->v_type != lvp->v_type) {
		vrele(lvp);
		lvp = NULLVP;
	}

	/*
	 * check shadow dir
	 */
	if (uerror != 0 && uerror != EJUSTRETURN && udvp != NULLVP &&
	    lerror == 0 && lvp != NULLVP && lvp->v_type == VDIR &&
	    !(dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (1 < cnp->cn_namelen || '.' != *(cnp->cn_nameptr))) {
		/* get unionfs vnode in order to create a new shadow dir. */
		error = unionfs_nodeget(dvp->v_mount, NULLVP, lvp, dvp, &vp,
		    cnp);
		if (error != 0)
			goto unionfs_lookup_cleanup;

		if (LK_SHARED == (cnp->cn_lkflags & LK_TYPE_MASK))
			VOP_UNLOCK(vp);
		if (LK_EXCLUSIVE != VOP_ISLOCKED(vp)) {
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			lockflag = 1;
		}
		unp = VTOUNIONFS(vp);
		if (unp == NULL)
			error = ENOENT;
		else
			error = unionfs_mkshadowdir(dvp, vp, cnp, td);
		if (lockflag != 0)
			VOP_UNLOCK(vp);
		if (error != 0) {
			UNIONFSDEBUG(
			    "unionfs_lookup: Unable to create shadow dir.");
			if ((cnp->cn_lkflags & LK_TYPE_MASK) == LK_EXCLUSIVE)
				vput(vp);
			else
				vrele(vp);
			goto unionfs_lookup_cleanup;
		}
		/*
		 * TODO: Since unionfs_mkshadowdir() relocks udvp after
		 * creating the new directory, return ERELOOKUP here?
		 */
		if ((cnp->cn_lkflags & LK_TYPE_MASK) == LK_SHARED)
			vn_lock(vp, LK_SHARED | LK_RETRY);
	}
	/*
	 * get unionfs vnode.
	 */
	else {
		if (uvp != NULLVP)
			error = uerror;
		else
			error = lerror;
		if (error != 0)
			goto unionfs_lookup_cleanup;
		error = unionfs_nodeget(dvp->v_mount, uvp, lvp,
		    dvp, &vp, cnp);
		if (error != 0) {
			UNIONFSDEBUG(
			    "unionfs_lookup: Unable to create unionfs vnode.");
			goto unionfs_lookup_cleanup;
		}
	}

	if (VN_IS_DOOMED(dvp) || VN_IS_DOOMED(vp)) {
		error = ENOENT;
		vput(vp);
		goto unionfs_lookup_cleanup;
	}

	*(ap->a_vpp) = vp;

	if (cnflags & MAKEENTRY)
		cache_enter(dvp, vp, cnp);

unionfs_lookup_cleanup:
	if (uvp != NULLVP)
		vrele(uvp);
	if (lvp != NULLVP)
		vrele(lvp);

	if (error == ENOENT && (cnflags & MAKEENTRY) != 0 &&
	    !VN_IS_DOOMED(dvp))
		cache_enter(dvp, NULLVP, cnp);

unionfs_lookup_return:
	unionfs_clear_in_progress_flag(dvp, UNIONFS_LOOKUP_IN_PROGRESS);

	UNIONFS_INTERNAL_DEBUG("unionfs_lookup: leave (%d)\n", error);

	return (error);
}

static int
unionfs_create(struct vop_create_args *ap)
{
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *vp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_create: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_dvp);

	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;
	error = EROFS;

	if (udvp != NULLVP) {
		int lkflags;
		bool vp_created = false;
		unionfs_forward_vop_start(udvp, &lkflags);
		error = VOP_CREATE(udvp, &vp, cnp, ap->a_vap);
		if (error == 0)
			vp_created = true;
		if (__predict_false(unionfs_forward_vop_finish(ap->a_dvp, udvp,
		    lkflags)) && error == 0) {
			error = ENOENT;
		}
		if (error == 0) {
			VOP_UNLOCK(vp);
			error = unionfs_nodeget(ap->a_dvp->v_mount, vp, NULLVP,
			    ap->a_dvp, ap->a_vpp, cnp);
			vrele(vp);
		} else if (vp_created)
			vput(vp);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_create: leave (%d)\n", error);

	return (error);
}

static int
unionfs_whiteout(struct vop_whiteout_args *ap)
{
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_whiteout: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_dvp);

	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;
	error = EOPNOTSUPP;

	if (udvp != NULLVP) {
		int lkflags;
		switch (ap->a_flags) {
		case CREATE:
		case DELETE:
		case LOOKUP:
			unionfs_forward_vop_start(udvp, &lkflags);
			error = VOP_WHITEOUT(udvp, cnp, ap->a_flags);
			unionfs_forward_vop_finish(ap->a_dvp, udvp, lkflags);
			break;
		default:
			error = EINVAL;
			break;
		}
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_whiteout: leave (%d)\n", error);

	return (error);
}

static int
unionfs_mknod(struct vop_mknod_args *ap)
{
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *vp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_mknod: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_dvp);

	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	udvp = dunp->un_uppervp;
	error = EROFS;

	if (udvp != NULLVP) {
		int lkflags;
		bool vp_created = false;
		unionfs_forward_vop_start(udvp, &lkflags);
		error = VOP_MKNOD(udvp, &vp, cnp, ap->a_vap);
		if (error == 0)
			vp_created = true;
		if (__predict_false(unionfs_forward_vop_finish(ap->a_dvp, udvp,
		    lkflags)) && error == 0) {
			error = ENOENT;
		}
		if (error == 0) {
			VOP_UNLOCK(vp);
			error = unionfs_nodeget(ap->a_dvp->v_mount, vp, NULLVP,
			    ap->a_dvp, ap->a_vpp, cnp);
			vrele(vp);
		} else if (vp_created)
			vput(vp);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_mknod: leave (%d)\n", error);

	return (error);
}

enum unionfs_lkupgrade {
	UNIONFS_LKUPGRADE_SUCCESS, /* lock successfully upgraded */
	UNIONFS_LKUPGRADE_ALREADY, /* lock already held exclusive */
	UNIONFS_LKUPGRADE_DOOMED   /* lock was upgraded, but vnode reclaimed */
};

static inline enum unionfs_lkupgrade
unionfs_upgrade_lock(struct vnode *vp)
{
	ASSERT_VOP_LOCKED(vp, __func__);

	if (VOP_ISLOCKED(vp) == LK_EXCLUSIVE)
		return (UNIONFS_LKUPGRADE_ALREADY);

	if (vn_lock(vp, LK_UPGRADE) != 0) {
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		if (VN_IS_DOOMED(vp))
			return (UNIONFS_LKUPGRADE_DOOMED);
	}
	return (UNIONFS_LKUPGRADE_SUCCESS);
}

static inline void
unionfs_downgrade_lock(struct vnode *vp, enum unionfs_lkupgrade status)
{
	if (status != UNIONFS_LKUPGRADE_ALREADY)
		vn_lock(vp, LK_DOWNGRADE | LK_RETRY);
}

/*
 * Exchange the default (upper vnode) lock on a unionfs vnode for the lower
 * vnode lock, in support of operations that require access to the lower vnode
 * even when an upper vnode is present.  We don't use vn_lock_pair() to hold
 * both vnodes at the same time, primarily because the caller may proceed
 * to issue VOPs to the lower layer which re-lock or perform other operations
 * which may not be safe in the presence of a locked vnode from another FS.
 * Moreover, vn_lock_pair()'s deadlock resolution approach can introduce
 * additional overhead that isn't necessary on these paths.
 *
 * vp must be a locked unionfs vnode; the lock state of this vnode is
 * returned through *lkflags for later use in unionfs_unlock_lvp().
 *
 * Returns the locked lower vnode, or NULL if the lower vnode (and therefore
 * also the unionfs vnode above it) has been doomed.
 */
static struct vnode *
unionfs_lock_lvp(struct vnode *vp, int *lkflags)
{
	struct unionfs_node *unp;
	struct vnode *lvp;

	unp = VTOUNIONFS(vp);
	lvp = unp->un_lowervp;
	ASSERT_VOP_LOCKED(vp, __func__);
	ASSERT_VOP_UNLOCKED(lvp, __func__);
	*lkflags = VOP_ISLOCKED(vp);
	vref(lvp);
	VOP_UNLOCK(vp);
	vn_lock(lvp, *lkflags | LK_RETRY);
	if (VN_IS_DOOMED(lvp)) {
		vput(lvp);
		lvp = NULLVP;
		vn_lock(vp, *lkflags | LK_RETRY);
	}
	return (lvp);
}

/*
 * Undo a previous call to unionfs_lock_lvp(), restoring the default lock
 * on the unionfs vnode.  This function reloads and returns the vnode
 * private data for the unionfs vnode, which will be NULL if the unionfs
 * vnode became doomed while its lock was dropped.  The caller must check
 * for this case.
 */
static struct unionfs_node *
unionfs_unlock_lvp(struct vnode *vp, struct vnode *lvp, int lkflags)
{
	ASSERT_VOP_LOCKED(lvp, __func__);
	ASSERT_VOP_UNLOCKED(vp, __func__);
	vput(lvp);
	vn_lock(vp, lkflags | LK_RETRY);
	return (VTOUNIONFS(vp));
}

static int
unionfs_open(struct vop_open_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *vp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *targetvp;
	struct ucred   *cred;
	struct thread  *td;
	int		error;
	int		lkflags;
	enum unionfs_lkupgrade lkstatus;
	bool		lock_lvp, open_lvp;

	UNIONFS_INTERNAL_DEBUG("unionfs_open: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = 0;
	vp = ap->a_vp;
	targetvp = NULLVP;
	cred = ap->a_cred;
	td = ap->a_td;
	open_lvp = lock_lvp = false;

	/*
	 * The executable loader path may call this function with vp locked
	 * shared.  If the vnode is reclaimed while upgrading, we can't safely
	 * use unp or do anything else unionfs- specific.
	 */
	lkstatus = unionfs_upgrade_lock(vp);
	if (lkstatus == UNIONFS_LKUPGRADE_DOOMED) {
		error = ENOENT;
		goto unionfs_open_cleanup;
	}

	unp = VTOUNIONFS(vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	unionfs_get_node_status(unp, td, &unsp);

	if (unsp->uns_lower_opencnt > 0 || unsp->uns_upper_opencnt > 0) {
		/* vnode is already opend. */
		if (unsp->uns_upper_opencnt > 0)
			targetvp = uvp;
		else
			targetvp = lvp;

		if (targetvp == lvp &&
		    (ap->a_mode & FWRITE) && lvp->v_type == VREG)
			targetvp = NULLVP;
	}
	if (targetvp == NULLVP) {
		if (uvp == NULLVP) {
			if ((ap->a_mode & FWRITE) && lvp->v_type == VREG) {
				error = unionfs_copyfile(vp,
				    !(ap->a_mode & O_TRUNC), cred, td);
				if (error != 0) {
					unp = VTOUNIONFS(vp);
					goto unionfs_open_abort;
				}
				targetvp = uvp = unp->un_uppervp;
			} else
				targetvp = lvp;
		} else
			targetvp = uvp;
	}

	if (targetvp == uvp && uvp->v_type == VDIR && lvp != NULLVP &&
	    unsp->uns_lower_opencnt <= 0)
		open_lvp = true;
	else if (targetvp == lvp && uvp != NULLVP)
		lock_lvp = true;

	if (lock_lvp) {
		unp = NULL;
		lvp = unionfs_lock_lvp(vp, &lkflags);
		if (lvp == NULLVP) {
			error = ENOENT;
			goto unionfs_open_abort;
		}
	} else
		unionfs_forward_vop_start(targetvp, &lkflags);

	error = VOP_OPEN(targetvp, ap->a_mode, cred, td, ap->a_fp);

	if (lock_lvp) {
		unp = unionfs_unlock_lvp(vp, lvp, lkflags);
		if (unp == NULL && error == 0)
			error = ENOENT;
	} else if (unionfs_forward_vop_finish(vp, targetvp, lkflags))
		error = error ? error : ENOENT;

	if (error != 0)
		goto unionfs_open_abort;

	if (targetvp == uvp) {
		if (open_lvp) {
			unp = NULL;
			lvp = unionfs_lock_lvp(vp, &lkflags);
			if (lvp == NULLVP) {
				error = ENOENT;
				goto unionfs_open_abort;
			}
			/* open lower for readdir */
			error = VOP_OPEN(lvp, FREAD, cred, td, NULL);
			unp = unionfs_unlock_lvp(vp, lvp, lkflags);
			if (unp == NULL) {
				error = error ? error : ENOENT;
				goto unionfs_open_abort;
			}
			if (error != 0) {
				unionfs_forward_vop_start(uvp, &lkflags);
				VOP_CLOSE(uvp, ap->a_mode, cred, td);
				if (unionfs_forward_vop_finish(vp, uvp, lkflags))
					unp = NULL;
				goto unionfs_open_abort;
			}
			unsp->uns_node_flag |= UNS_OPENL_4_READDIR;
			unsp->uns_lower_opencnt++;
		}
		unsp->uns_upper_opencnt++;
	} else {
		unsp->uns_lower_opencnt++;
		unsp->uns_lower_openmode = ap->a_mode;
	}
	vp->v_object = targetvp->v_object;

unionfs_open_abort:

	if (error != 0 && unp != NULL)
		unionfs_tryrem_node_status(unp, unsp);

unionfs_open_cleanup:
	unionfs_downgrade_lock(vp, lkstatus);

	UNIONFS_INTERNAL_DEBUG("unionfs_open: leave (%d)\n", error);

	return (error);
}

static int
unionfs_close(struct vop_close_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct ucred   *cred;
	struct thread  *td;
	struct vnode   *vp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *ovp;
	int		error;
	int		lkflags;
	enum unionfs_lkupgrade lkstatus;
	bool		lock_lvp;

	UNIONFS_INTERNAL_DEBUG("unionfs_close: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	vp = ap->a_vp;
	cred = ap->a_cred;
	td = ap->a_td;
	error = 0;
	lock_lvp = false;

	/*
	 * If the vnode is reclaimed while upgrading, we can't safely use unp
	 * or do anything else unionfs- specific.
	 */
	lkstatus = unionfs_upgrade_lock(vp);
	if (lkstatus == UNIONFS_LKUPGRADE_DOOMED)
		goto unionfs_close_cleanup;

	unp = VTOUNIONFS(vp);
	lvp = unp->un_lowervp;
	uvp = unp->un_uppervp;
	unsp = unionfs_find_node_status(unp, td);

	if (unsp == NULL ||
	    (unsp->uns_lower_opencnt <= 0 && unsp->uns_upper_opencnt <= 0)) {
#ifdef DIAGNOSTIC
		if (unsp != NULL)
			printf("unionfs_close: warning: open count is 0\n");
#endif
		if (uvp != NULLVP)
			ovp = uvp;
		else
			ovp = lvp;
	} else if (unsp->uns_upper_opencnt > 0)
		ovp = uvp;
	else
		ovp = lvp;

	if (ovp == lvp && uvp != NULLVP) {
		lock_lvp = true;
		unp = NULL;
		lvp = unionfs_lock_lvp(vp, &lkflags);
		if (lvp == NULLVP) {
			error = ENOENT;
			goto unionfs_close_abort;
		}
	} else
		unionfs_forward_vop_start(ovp, &lkflags);

	error = VOP_CLOSE(ovp, ap->a_fflag, cred, td);

	if (lock_lvp) {
		unp = unionfs_unlock_lvp(vp, lvp, lkflags);
		if (unp == NULL && error == 0)
			error = ENOENT;
	} else if (unionfs_forward_vop_finish(vp, ovp, lkflags))
		error = error ? error : ENOENT;

	if (error != 0)
		goto unionfs_close_abort;

	vp->v_object = ovp->v_object;

	if (ovp == uvp) {
		if (unsp != NULL && ((--unsp->uns_upper_opencnt) == 0)) {
			if (unsp->uns_node_flag & UNS_OPENL_4_READDIR) {
				unp = NULL;
				lvp = unionfs_lock_lvp(vp, &lkflags);
				if (lvp == NULLVP) {
					error = ENOENT;
					goto unionfs_close_abort;
				}
				VOP_CLOSE(lvp, FREAD, cred, td);
				unp = unionfs_unlock_lvp(vp, lvp, lkflags);
				if (unp == NULL) {
					error = ENOENT;
					goto unionfs_close_abort;
				}
				unsp->uns_node_flag &= ~UNS_OPENL_4_READDIR;
				unsp->uns_lower_opencnt--;
			}
			if (unsp->uns_lower_opencnt > 0)
				vp->v_object = lvp->v_object;
		}
	} else if (unsp != NULL)
		unsp->uns_lower_opencnt--;

unionfs_close_abort:
	if (unp != NULL && unsp != NULL)
		unionfs_tryrem_node_status(unp, unsp);

unionfs_close_cleanup:
	unionfs_downgrade_lock(vp, lkstatus);

	UNIONFS_INTERNAL_DEBUG("unionfs_close: leave (%d)\n", error);

	return (error);
}

/*
 * Check the access mode toward shadow file/dir.
 */
static int
unionfs_check_corrected_access(accmode_t accmode, struct vattr *va,
    struct ucred *cred)
{
	uid_t		uid;	/* upper side vnode's uid */
	gid_t		gid;	/* upper side vnode's gid */
	u_short		vmode;	/* upper side vnode's mode */
	u_short		mask;

	mask = 0;
	uid = va->va_uid;
	gid = va->va_gid;
	vmode = va->va_mode;

	/* check owner */
	if (cred->cr_uid == uid) {
		if (accmode & VEXEC)
			mask |= S_IXUSR;
		if (accmode & VREAD)
			mask |= S_IRUSR;
		if (accmode & VWRITE)
			mask |= S_IWUSR;
		return ((vmode & mask) == mask ? 0 : EACCES);
	}

	/* check group */
	if (groupmember(gid, cred)) {
		if (accmode & VEXEC)
			mask |= S_IXGRP;
		if (accmode & VREAD)
			mask |= S_IRGRP;
		if (accmode & VWRITE)
			mask |= S_IWGRP;
		return ((vmode & mask) == mask ? 0 : EACCES);
	}

	/* check other */
	if (accmode & VEXEC)
		mask |= S_IXOTH;
	if (accmode & VREAD)
		mask |= S_IROTH;
	if (accmode & VWRITE)
		mask |= S_IWOTH;

	return ((vmode & mask) == mask ? 0 : EACCES);
}

static int
unionfs_access(struct vop_access_args *ap)
{
	struct unionfs_mount *ump;
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct thread  *td;
	struct vattr	va;
	accmode_t	accmode;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_access: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	td = ap->a_td;
	accmode = ap->a_accmode;
	error = EACCES;

	if ((accmode & VWRITE) &&
	    (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (ap->a_vp->v_type) {
		case VREG:
		case VDIR:
		case VLNK:
			return (EROFS);
		default:
			break;
		}
	}

	if (uvp != NULLVP) {
		error = VOP_ACCESS(uvp, accmode, ap->a_cred, td);

		UNIONFS_INTERNAL_DEBUG("unionfs_access: leave (%d)\n", error);

		return (error);
	}

	if (lvp != NULLVP) {
		if (accmode & VWRITE) {
			if ((ump->um_uppermp->mnt_flag & MNT_RDONLY) != 0) {
				switch (ap->a_vp->v_type) {
				case VREG:
				case VDIR:
				case VLNK:
					return (EROFS);
				default:
					break;
				}
			} else if (ap->a_vp->v_type == VREG ||
			    ap->a_vp->v_type == VDIR) {
				/* check shadow file/dir */
				if (ump->um_copymode != UNIONFS_TRANSPARENT) {
					error = unionfs_create_uppervattr(ump,
					    lvp, &va, ap->a_cred, td);
					if (error != 0)
						return (error);

					error = unionfs_check_corrected_access(
					    accmode, &va, ap->a_cred);
					if (error != 0)
						return (error);
				}
			}
			accmode &= ~(VWRITE | VAPPEND);
			accmode |= VREAD; /* will copy to upper */
		}
		error = VOP_ACCESS(lvp, accmode, ap->a_cred, td);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_access: leave (%d)\n", error);

	return (error);
}

static int
unionfs_getattr(struct vop_getattr_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_mount *ump;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct thread  *td;
	struct vattr	va;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_getattr: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	td = curthread;

	if (uvp != NULLVP) {
		if ((error = VOP_GETATTR(uvp, ap->a_vap, ap->a_cred)) == 0)
			ap->a_vap->va_fsid =
			    ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];

		UNIONFS_INTERNAL_DEBUG(
		    "unionfs_getattr: leave mode=%o, uid=%d, gid=%d (%d)\n",
		    ap->a_vap->va_mode, ap->a_vap->va_uid,
		    ap->a_vap->va_gid, error);

		return (error);
	}

	error = VOP_GETATTR(lvp, ap->a_vap, ap->a_cred);

	if (error == 0 && (ump->um_uppermp->mnt_flag & MNT_RDONLY) == 0) {
		/* correct the attr toward shadow file/dir. */
		if (ap->a_vp->v_type == VREG || ap->a_vp->v_type == VDIR) {
			unionfs_create_uppervattr_core(ump, ap->a_vap, &va, td);
			ap->a_vap->va_mode = va.va_mode;
			ap->a_vap->va_uid = va.va_uid;
			ap->a_vap->va_gid = va.va_gid;
		}
	}

	if (error == 0)
		ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];

	UNIONFS_INTERNAL_DEBUG(
	    "unionfs_getattr: leave mode=%o, uid=%d, gid=%d (%d)\n",
	    ap->a_vap->va_mode, ap->a_vap->va_uid, ap->a_vap->va_gid, error);

	return (error);
}

static int
unionfs_setattr(struct vop_setattr_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct thread  *td;
	struct vattr   *vap;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_setattr: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = EROFS;
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	td = curthread;
	vap = ap->a_vap;

	if ((ap->a_vp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	     vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	     vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL))
		return (EROFS);

	if (uvp == NULLVP && lvp->v_type == VREG) {
		error = unionfs_copyfile(ap->a_vp, (vap->va_size != 0),
		    ap->a_cred, td);
		if (error != 0)
			return (error);
		uvp = unp->un_uppervp;
	}

	if (uvp != NULLVP) {
		int lkflags;
		unionfs_forward_vop_start(uvp, &lkflags);
		error = VOP_SETATTR(uvp, vap, ap->a_cred);
		unionfs_forward_vop_finish(ap->a_vp, uvp, lkflags);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_setattr: leave (%d)\n", error);

	return (error);
}

static int
unionfs_read(struct vop_read_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *tvp;
	int		error;

	/* UNIONFS_INTERNAL_DEBUG("unionfs_read: enter\n"); */

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	error = VOP_READ(tvp, ap->a_uio, ap->a_ioflag, ap->a_cred);

	/* UNIONFS_INTERNAL_DEBUG("unionfs_read: leave (%d)\n", error); */

	return (error);
}

static int
unionfs_write(struct vop_write_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *tvp;
	int		error;
	int		lkflags;

	/* UNIONFS_INTERNAL_DEBUG("unionfs_write: enter\n"); */

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	unionfs_forward_vop_start(tvp, &lkflags);
	error = VOP_WRITE(tvp, ap->a_uio, ap->a_ioflag, ap->a_cred);
	unionfs_forward_vop_finish(ap->a_vp, tvp, lkflags);

	/* UNIONFS_INTERNAL_DEBUG("unionfs_write: leave (%d)\n", error); */

	return (error);
}

static int
unionfs_ioctl(struct vop_ioctl_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *ovp;
	int error;

	UNIONFS_INTERNAL_DEBUG("unionfs_ioctl: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

 	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY);
	unp = VTOUNIONFS(ap->a_vp);
	unionfs_get_node_status(unp, ap->a_td, &unsp);
	ovp = (unsp->uns_upper_opencnt ? unp->un_uppervp : unp->un_lowervp);
	unionfs_tryrem_node_status(unp, unsp);
	VOP_UNLOCK(ap->a_vp);

	if (ovp == NULLVP)
		return (EBADF);

	error = VOP_IOCTL(ovp, ap->a_command, ap->a_data, ap->a_fflag,
	    ap->a_cred, ap->a_td);

	UNIONFS_INTERNAL_DEBUG("unionfs_ioctl: leave (%d)\n", error);

	return (error);
}

static int
unionfs_poll(struct vop_poll_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode *ovp;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

 	vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY);
	unp = VTOUNIONFS(ap->a_vp);
	unionfs_get_node_status(unp, ap->a_td, &unsp);
	ovp = (unsp->uns_upper_opencnt ? unp->un_uppervp : unp->un_lowervp);
	unionfs_tryrem_node_status(unp, unsp);
	VOP_UNLOCK(ap->a_vp);

	if (ovp == NULLVP)
		return (EBADF);

	return (VOP_POLL(ovp, ap->a_events, ap->a_cred, ap->a_td));
}

static int
unionfs_fsync(struct vop_fsync_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode *ovp;
	enum unionfs_lkupgrade lkstatus;
	int error, lkflags;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	lkstatus = unionfs_upgrade_lock(ap->a_vp);
	if (lkstatus == UNIONFS_LKUPGRADE_DOOMED) {
		unionfs_downgrade_lock(ap->a_vp, lkstatus);
		return (ENOENT);
	}
	unionfs_get_node_status(unp, ap->a_td, &unsp);
	ovp = (unsp->uns_upper_opencnt ? unp->un_uppervp : unp->un_lowervp);
	unionfs_tryrem_node_status(unp, unsp);

	unionfs_downgrade_lock(ap->a_vp, lkstatus);

	if (ovp == NULLVP)
		return (EBADF);

	unionfs_forward_vop_start(ovp, &lkflags);
	error = VOP_FSYNC(ovp, ap->a_waitfor, ap->a_td);
	unionfs_forward_vop_finish(ap->a_vp, ovp, lkflags);

	return (error);
}

static int
unionfs_remove(struct vop_remove_args *ap)
{
	char	       *path;
	struct unionfs_node *dunp;
	struct unionfs_node *unp;
	struct unionfs_mount *ump;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct componentname *cnp;
	struct thread  *td;
	int		error;
	int		pathlen;

	UNIONFS_INTERNAL_DEBUG("unionfs_remove: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_dvp);
	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = 0;
	dunp = VTOUNIONFS(ap->a_dvp);
	udvp = dunp->un_uppervp;
	cnp = ap->a_cnp;
	td = curthread;

	ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	path = unp->un_path;
	pathlen = unp->un_pathlen;

	if (udvp == NULLVP)
		return (EROFS);

	if (uvp != NULLVP) {
		int udvp_lkflags, uvp_lkflags;
		if (ump == NULL || ump->um_whitemode == UNIONFS_WHITE_ALWAYS ||
		    lvp != NULLVP)
			cnp->cn_flags |= DOWHITEOUT;
		unionfs_forward_vop_start_pair(udvp, &udvp_lkflags,
		    uvp, &uvp_lkflags);
		error = VOP_REMOVE(udvp, uvp, cnp);
		unionfs_forward_vop_finish_pair(ap->a_dvp, udvp, udvp_lkflags,
		    ap->a_vp, uvp, uvp_lkflags);
	} else if (lvp != NULLVP) {
		error = unionfs_mkwhiteout(ap->a_dvp, ap->a_vp, cnp, td,
		    path, pathlen);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_remove: leave (%d)\n", error);

	return (error);
}

static int
unionfs_link(struct vop_link_args *ap)
{
	struct unionfs_node *dunp;
	struct unionfs_node *unp;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct componentname *cnp;
	struct thread  *td;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_link: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_tdvp);
	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = 0;
	dunp = VTOUNIONFS(ap->a_tdvp);
	unp = NULL;
	udvp = dunp->un_uppervp;
	uvp = NULLVP;
	cnp = ap->a_cnp;
	td = curthread;

	if (udvp == NULLVP)
		return (EROFS);

	unp = VTOUNIONFS(ap->a_vp);

	if (unp->un_uppervp == NULLVP) {
		if (ap->a_vp->v_type != VREG)
			return (EOPNOTSUPP);

		VOP_UNLOCK(ap->a_tdvp);
		error = unionfs_copyfile(ap->a_vp, 1, cnp->cn_cred, td);
		vn_lock(ap->a_tdvp, LK_EXCLUSIVE | LK_RETRY);
		if (error == 0)
			error = ERELOOKUP;
		return (error);
	}
	uvp = unp->un_uppervp;

	if (error == 0) {
		int udvp_lkflags, uvp_lkflags;
		unionfs_forward_vop_start_pair(udvp, &udvp_lkflags,
		    uvp, &uvp_lkflags);
		error = VOP_LINK(udvp, uvp, cnp);
		unionfs_forward_vop_finish_pair(ap->a_tdvp, udvp, udvp_lkflags,
		    ap->a_vp, uvp, uvp_lkflags);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_link: leave (%d)\n", error);

	return (error);
}

static int
unionfs_rename(struct vop_rename_args *ap)
{
	struct vnode   *fdvp;
	struct vnode   *fvp;
	struct componentname *fcnp;
	struct vnode   *tdvp;
	struct vnode   *tvp;
	struct componentname *tcnp;
	struct thread  *td;

	/* rename target vnodes */
	struct vnode   *rfdvp;
	struct vnode   *rfvp;
	struct vnode   *rtdvp;
	struct vnode   *rtvp;

	struct unionfs_node *unp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_rename: enter\n");

	error = 0;
	fdvp = ap->a_fdvp;
	fvp = ap->a_fvp;
	fcnp = ap->a_fcnp;
	tdvp = ap->a_tdvp;
	tvp = ap->a_tvp;
	tcnp = ap->a_tcnp;
	td = curthread;
	rfdvp = fdvp;
	rfvp = fvp;
	rtdvp = tdvp;
	rtvp = tvp;

	/* check for cross device rename */
	if (fvp->v_mount != tdvp->v_mount ||
	    (tvp != NULLVP && fvp->v_mount != tvp->v_mount)) {
		if (fvp->v_op != &unionfs_vnodeops)
			error = ENODEV;
		else
			error = EXDEV;
		goto unionfs_rename_abort;
	}

	/* Renaming a file to itself has no effect. */
	if (fvp == tvp)
		goto unionfs_rename_abort;

	KASSERT_UNIONFS_VNODE(tdvp);
	if (tvp != NULLVP)
		KASSERT_UNIONFS_VNODE(tvp);
	if (fdvp != tdvp)
		VI_LOCK(fdvp);
	unp = VTOUNIONFS(fdvp);
	if (unp == NULL) {
		if (fdvp != tdvp)
			VI_UNLOCK(fdvp);
		error = ENOENT;
		goto unionfs_rename_abort;
	}
#ifdef UNIONFS_IDBG_RENAME
	UNIONFS_INTERNAL_DEBUG("fdvp=%p, ufdvp=%p, lfdvp=%p\n",
	    fdvp, unp->un_uppervp, unp->un_lowervp);
#endif
	if (unp->un_uppervp == NULLVP) {
		error = ENODEV;
	} else {
		rfdvp = unp->un_uppervp;
		vref(rfdvp);
	}
	if (fdvp != tdvp)
		VI_UNLOCK(fdvp);
	if (error != 0)
		goto unionfs_rename_abort;

	VI_LOCK(fvp);
	unp = VTOUNIONFS(fvp);
	if (unp == NULL) {
		VI_UNLOCK(fvp);
		error = ENOENT;
		goto unionfs_rename_abort;
	}

#ifdef UNIONFS_IDBG_RENAME
	UNIONFS_INTERNAL_DEBUG("fvp=%p, ufvp=%p, lfvp=%p\n",
	    fvp, unp->un_uppervp, unp->un_lowervp);
#endif
	/*
	 * If we only have a lower vnode, copy the source file to the upper
	 * FS so that the rename operation can be issued against the upper FS.
	 */
	if (unp->un_uppervp == NULLVP) {
		bool unlock_fdvp = false, relock_tdvp = false;
		VI_UNLOCK(fvp);
		if (tvp != NULLVP)
			VOP_UNLOCK(tvp);
		if (fvp->v_type == VREG) {
			/*
			 * For regular files, unionfs_copyfile() will expect
			 * fdvp's upper parent directory vnode to be unlocked
			 * and will temporarily lock it.  If fdvp == tdvp, we
			 * should unlock tdvp to avoid recursion on tdvp's
			 * lock.  If fdvp != tdvp, we should also unlock tdvp
			 * to avoid potential deadlock due to holding tdvp's
			 * lock while locking unrelated vnodes associated with
			 * fdvp/fvp.
			 */
			VOP_UNLOCK(tdvp);
			relock_tdvp = true;
		} else if (fvp->v_type == VDIR && tdvp != fdvp) {
			/*
			 * For directories, unionfs_mkshadowdir() will expect
			 * fdvp's upper parent directory vnode to be locked
			 * and will temporarily unlock it.  If fdvp == tdvp,
			 * we can therefore leave tdvp locked.  If fdvp !=
			 * tdvp, we should exchange the lock on tdvp for a
			 * lock on fdvp.
			 */
			VOP_UNLOCK(tdvp);
			unlock_fdvp = true;
			relock_tdvp = true;
			vn_lock(fdvp, LK_EXCLUSIVE | LK_RETRY);
		}
		vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY);
		unp = VTOUNIONFS(fvp);
		if (unp == NULL)
			error = ENOENT;
		else if (unp->un_uppervp == NULLVP) {
			switch (fvp->v_type) {
			case VREG:
				error = unionfs_copyfile(fvp, 1, fcnp->cn_cred, td);
				break;
			case VDIR:
				error = unionfs_mkshadowdir(fdvp, fvp, fcnp, td);
				break;
			default:
				error = ENODEV;
				break;
			}
		}
		VOP_UNLOCK(fvp);
		if (unlock_fdvp)
			VOP_UNLOCK(fdvp);
		if (relock_tdvp)
			vn_lock(tdvp, LK_EXCLUSIVE | LK_RETRY);
		if (tvp != NULLVP)
			vn_lock(tvp, LK_EXCLUSIVE | LK_RETRY);
		/*
		 * Since we've dropped tdvp's lock at some point in the copy
		 * sequence above, force the caller to re-drive the lookup
		 * in case the relationship between tdvp and tvp has changed.
		 */
		if (error == 0)
			error = ERELOOKUP;
		goto unionfs_rename_abort;
	}

	if (unp->un_lowervp != NULLVP)
		fcnp->cn_flags |= DOWHITEOUT;
	rfvp = unp->un_uppervp;
	vref(rfvp);

	VI_UNLOCK(fvp);

	unp = VTOUNIONFS(tdvp);

#ifdef UNIONFS_IDBG_RENAME
	UNIONFS_INTERNAL_DEBUG("tdvp=%p, utdvp=%p, ltdvp=%p\n",
	    tdvp, unp->un_uppervp, unp->un_lowervp);
#endif
	if (unp->un_uppervp == NULLVP) {
		error = ENODEV;
		goto unionfs_rename_abort;
	}
	rtdvp = unp->un_uppervp;
	vref(rtdvp);

	if (tvp != NULLVP) {
		unp = VTOUNIONFS(tvp);
		if (unp == NULL) {
			error = ENOENT;
			goto unionfs_rename_abort;
		}
#ifdef UNIONFS_IDBG_RENAME
		UNIONFS_INTERNAL_DEBUG("tvp=%p, utvp=%p, ltvp=%p\n",
		    tvp, unp->un_uppervp, unp->un_lowervp);
#endif
		if (unp->un_uppervp == NULLVP)
			rtvp = NULLVP;
		else {
			if (tvp->v_type == VDIR) {
				error = EINVAL;
				goto unionfs_rename_abort;
			}
			rtvp = unp->un_uppervp;
			vref(rtvp);
		}
	}

	if (rfvp == rtvp)
		goto unionfs_rename_abort;

	error = VOP_RENAME(rfdvp, rfvp, fcnp, rtdvp, rtvp, tcnp);

	if (error == 0) {
		if (rtvp != NULLVP && rtvp->v_type == VDIR)
			cache_purge(tdvp);
		if (fvp->v_type == VDIR && fdvp != tdvp)
			cache_purge(fdvp);
	}

	if (tdvp != rtdvp)
		vrele(tdvp);
	if (tvp != rtvp && tvp != NULLVP) {
		if (rtvp == NULLVP)
			vput(tvp);
		else
			vrele(tvp);
	}
	if (fdvp != rfdvp)
		vrele(fdvp);
	if (fvp != rfvp)
		vrele(fvp);

	UNIONFS_INTERNAL_DEBUG("unionfs_rename: leave (%d)\n", error);

	return (error);

unionfs_rename_abort:
	vput(tdvp);
	if (tdvp != rtdvp)
		vrele(rtdvp);
	if (tvp != NULLVP) {
		if (tdvp != tvp)
			vput(tvp);
		else
			vrele(tvp);
	}
	if (tvp != rtvp && rtvp != NULLVP)
		vrele(rtvp);
	if (fdvp != rfdvp)
		vrele(rfdvp);
	if (fvp != rfvp)
		vrele(rfvp);
	vrele(fdvp);
	vrele(fvp);

	UNIONFS_INTERNAL_DEBUG("unionfs_rename: leave (%d)\n", error);

	return (error);
}

static int
unionfs_mkdir(struct vop_mkdir_args *ap)
{
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *dvp;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct vattr	va;
	int		error;
	int		lkflags;

	UNIONFS_INTERNAL_DEBUG("unionfs_mkdir: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_dvp);

	error = EROFS;
	dvp = ap->a_dvp;
	dunp = VTOUNIONFS(dvp);
	cnp = ap->a_cnp;
	lkflags = cnp->cn_lkflags;
	udvp = dunp->un_uppervp;

	if (udvp != NULLVP) {
		/* check opaque */
		if (!(cnp->cn_flags & ISWHITEOUT)) {
			error = VOP_GETATTR(udvp, &va, cnp->cn_cred);
			if (error != 0)
				goto unionfs_mkdir_cleanup;
			if ((va.va_flags & OPAQUE) != 0)
				cnp->cn_flags |= ISWHITEOUT;
		}

		int udvp_lkflags;
		bool uvp_created = false;
		unionfs_forward_vop_start(udvp, &udvp_lkflags);
		error = VOP_MKDIR(udvp, &uvp, cnp, ap->a_vap);
		if (error == 0)
			uvp_created = true;
		if (__predict_false(unionfs_forward_vop_finish(dvp, udvp,
		    udvp_lkflags)) && error == 0)
			error = ENOENT;
		if (error == 0) {
			VOP_UNLOCK(uvp);
			cnp->cn_lkflags = LK_EXCLUSIVE;
			error = unionfs_nodeget(dvp->v_mount, uvp, NULLVP,
			    dvp, ap->a_vpp, cnp);
			vrele(uvp);
			cnp->cn_lkflags = lkflags;
		} else if (uvp_created)
			vput(uvp);
	}

unionfs_mkdir_cleanup:
	UNIONFS_INTERNAL_DEBUG("unionfs_mkdir: leave (%d)\n", error);

	return (error);
}

static int
unionfs_rmdir(struct vop_rmdir_args *ap)
{
	struct unionfs_node *dunp;
	struct unionfs_node *unp;
	struct unionfs_mount *ump;
	struct componentname *cnp;
	struct thread  *td;
	struct vnode   *udvp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_rmdir: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_dvp);
	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = 0;
	dunp = VTOUNIONFS(ap->a_dvp);
	unp = VTOUNIONFS(ap->a_vp);
	cnp = ap->a_cnp;
	td = curthread;
	udvp = dunp->un_uppervp;
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;

	if (udvp == NULLVP)
		return (EROFS);

	if (udvp == uvp)
		return (EOPNOTSUPP);

	if (uvp != NULLVP) {
		if (lvp != NULLVP) {
			/*
			 * We need to keep dvp and vp's upper vnodes locked
			 * going into the VOP_RMDIR() call, but the empty
			 * directory check also requires the lower vnode lock.
			 * For this third, cross-filesystem lock we use a
			 * similar approach taken by various FS' VOP_RENAME
			 * implementations (which require 2-4 vnode locks).
			 * First we attempt a NOWAIT acquisition, then if
			 * that fails we drops the other two vnode locks,
			 * acquire lvp's lock in the normal fashion to reduce
			 * the likelihood of spinning on it in the future,
			 * then drop, reacquire the other locks, and return
			 * ERELOOKUP to re-drive the lookup in case the dvp->
			 * vp relationship has changed.
			 */
			if (vn_lock(lvp, LK_SHARED | LK_NOWAIT) != 0) {
				VOP_UNLOCK(ap->a_vp);
				VOP_UNLOCK(ap->a_dvp);
				vn_lock(lvp, LK_SHARED | LK_RETRY);
				VOP_UNLOCK(lvp);
				vn_lock(ap->a_dvp, LK_EXCLUSIVE | LK_RETRY);
				vn_lock(ap->a_vp, LK_EXCLUSIVE | LK_RETRY);
				return (ERELOOKUP);
			}
			error = unionfs_check_rmdir(ap->a_vp, cnp->cn_cred, td);
			/*
			 * It's possible for a direct operation on the lower FS
			 * to make the lower directory non-empty after we drop
			 * the lock, but it's also possible for the upper-layer
			 * VOP_RMDIR to relock udvp/uvp which would lead to
			 * LOR if we kept lvp locked across that call.
			 */
			VOP_UNLOCK(lvp);
			if (error != 0)
				return (error);
		}
		ump = MOUNTTOUNIONFSMOUNT(ap->a_vp->v_mount);
		if (ump->um_whitemode == UNIONFS_WHITE_ALWAYS || lvp != NULLVP)
			cnp->cn_flags |= (DOWHITEOUT | IGNOREWHITEOUT);
		int udvp_lkflags, uvp_lkflags;
		unionfs_forward_vop_start_pair(udvp, &udvp_lkflags,
		    uvp, &uvp_lkflags);
		error = VOP_RMDIR(udvp, uvp, cnp);
		unionfs_forward_vop_finish_pair(ap->a_dvp, udvp, udvp_lkflags,
		    ap->a_vp, uvp, uvp_lkflags);
	} else if (lvp != NULLVP) {
		error = unionfs_mkwhiteout(ap->a_dvp, ap->a_vp, cnp, td,
		    unp->un_path, unp->un_pathlen);
	}

	if (error == 0) {
		cache_purge(ap->a_dvp);
		cache_purge(ap->a_vp);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_rmdir: leave (%d)\n", error);

	return (error);
}

static int
unionfs_symlink(struct vop_symlink_args *ap)
{
	struct unionfs_node *dunp;
	struct componentname *cnp;
	struct vnode   *udvp;
	struct vnode   *uvp;
	int		error;
	int		lkflags;

	UNIONFS_INTERNAL_DEBUG("unionfs_symlink: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_dvp);

	error = EROFS;
	dunp = VTOUNIONFS(ap->a_dvp);
	cnp = ap->a_cnp;
	lkflags = cnp->cn_lkflags;
	udvp = dunp->un_uppervp;

	if (udvp != NULLVP) {
		int udvp_lkflags;
		bool uvp_created = false;
		unionfs_forward_vop_start(udvp, &udvp_lkflags);
		error = VOP_SYMLINK(udvp, &uvp, cnp, ap->a_vap, ap->a_target);
		if (error == 0)
			uvp_created = true;
		if (__predict_false(unionfs_forward_vop_finish(ap->a_dvp, udvp,
		    udvp_lkflags)) && error == 0)
			error = ENOENT;
		if (error == 0) {
			VOP_UNLOCK(uvp);
			cnp->cn_lkflags = LK_EXCLUSIVE;
			error = unionfs_nodeget(ap->a_dvp->v_mount, uvp, NULLVP,
			    ap->a_dvp, ap->a_vpp, cnp);
			vrele(uvp);
			cnp->cn_lkflags = lkflags;
		} else if (uvp_created)
			vput(uvp);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_symlink: leave (%d)\n", error);

	return (error);
}

static int
unionfs_readdir(struct vop_readdir_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct uio     *uio;
	struct vnode   *vp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct thread  *td;
	struct vattr    va;

	uint64_t	*cookies_bk;
	int		error;
	int		eofflag;
	int		lkflags;
	int		ncookies_bk;
	int		uio_offset_bk;
	enum unionfs_lkupgrade lkstatus;

	UNIONFS_INTERNAL_DEBUG("unionfs_readdir: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = 0;
	eofflag = 0;
	uio_offset_bk = 0;
	uio = ap->a_uio;
	uvp = NULLVP;
	lvp = NULLVP;
	td = uio->uio_td;
	ncookies_bk = 0;
	cookies_bk = NULL;

	vp = ap->a_vp;
	if (vp->v_type != VDIR)
		return (ENOTDIR);

	/*
	 * If the vnode is reclaimed while upgrading, we can't safely use unp
	 * or do anything else unionfs- specific.
	 */
	lkstatus = unionfs_upgrade_lock(vp);
	if (lkstatus == UNIONFS_LKUPGRADE_DOOMED)
		error = EBADF;
	if (error == 0) {
		unp = VTOUNIONFS(vp);
		uvp = unp->un_uppervp;
		lvp = unp->un_lowervp;
		/* check the open count. unionfs needs open before readdir. */
		unionfs_get_node_status(unp, td, &unsp);
		if ((uvp != NULLVP && unsp->uns_upper_opencnt <= 0) ||
			(lvp != NULLVP && unsp->uns_lower_opencnt <= 0)) {
			unionfs_tryrem_node_status(unp, unsp);
			error = EBADF;
		}
	}
	unionfs_downgrade_lock(vp, lkstatus);
	if (error != 0)
		goto unionfs_readdir_exit;

	/* check opaque */
	if (uvp != NULLVP && lvp != NULLVP) {
		if ((error = VOP_GETATTR(uvp, &va, ap->a_cred)) != 0)
			goto unionfs_readdir_exit;
		if (va.va_flags & OPAQUE)
			lvp = NULLVP;
	}

	/* upper only */
	if (uvp != NULLVP && lvp == NULLVP) {
		unionfs_forward_vop_start(uvp, &lkflags);
		error = VOP_READDIR(uvp, uio, ap->a_cred, ap->a_eofflag,
		    ap->a_ncookies, ap->a_cookies);
		if (unionfs_forward_vop_finish(vp, uvp, lkflags))
			error = error ? error : ENOENT;
		else
			unsp->uns_readdir_status = 0;

		goto unionfs_readdir_exit;
	}

	/* lower only */
	if (uvp == NULLVP && lvp != NULLVP) {
		unionfs_forward_vop_start(lvp, &lkflags);
		error = VOP_READDIR(lvp, uio, ap->a_cred, ap->a_eofflag,
		    ap->a_ncookies, ap->a_cookies);
		if (unionfs_forward_vop_finish(vp, lvp, lkflags))
			error = error ? error : ENOENT;
		else
			unsp->uns_readdir_status = 2;

		goto unionfs_readdir_exit;
	}

	/*
	 * readdir upper and lower
	 */
	KASSERT(uvp != NULLVP, ("unionfs_readdir: null upper vp"));
	KASSERT(lvp != NULLVP, ("unionfs_readdir: null lower vp"));

	if (uio->uio_offset == 0)
		unsp->uns_readdir_status = 0;

	if (unsp->uns_readdir_status == 0) {
		/* read upper */
		unionfs_forward_vop_start(uvp, &lkflags);
		error = VOP_READDIR(uvp, uio, ap->a_cred, &eofflag,
				    ap->a_ncookies, ap->a_cookies);
		if (unionfs_forward_vop_finish(vp, uvp, lkflags) && error == 0)
			error = ENOENT;
		if (error != 0 || eofflag == 0)
			goto unionfs_readdir_exit;
		unsp->uns_readdir_status = 1;

		/*
		 * UFS(and other FS) needs size of uio_resid larger than
		 * DIRBLKSIZ.
		 * size of DIRBLKSIZ equals DEV_BSIZE.
		 * (see: ufs/ufs/ufs_vnops.c ufs_readdir func , ufs/ufs/dir.h)
		 */
		if (uio->uio_resid <= (uio->uio_resid & (DEV_BSIZE -1)))
			goto unionfs_readdir_exit;

		/*
		 * Backup cookies.
		 * It prepares to readdir in lower.
		 */
		if (ap->a_ncookies != NULL) {
			ncookies_bk = *(ap->a_ncookies);
			*(ap->a_ncookies) = 0;
		}
		if (ap->a_cookies != NULL) {
			cookies_bk = *(ap->a_cookies);
			*(ap->a_cookies) = NULL;
		}
	}

	/* initialize for readdir in lower */
	if (unsp->uns_readdir_status == 1) {
		unsp->uns_readdir_status = 2;
		/*
		 * Backup uio_offset. See the comment after the
		 * VOP_READDIR call on the lower layer.
		 */
		uio_offset_bk = uio->uio_offset;
		uio->uio_offset = 0;
	}

	lvp = unionfs_lock_lvp(vp, &lkflags);
	if (lvp == NULL) {
		error = ENOENT;
		goto unionfs_readdir_exit;
	}

	/* read lower */
	error = VOP_READDIR(lvp, uio, ap->a_cred, ap->a_eofflag,
			    ap->a_ncookies, ap->a_cookies);


	unp = unionfs_unlock_lvp(vp, lvp, lkflags);
	if (unp == NULL && error == 0)
		error = ENOENT;


	/*
	 * We can't return an uio_offset of 0: this would trigger an
	 * infinite loop, because the next call to unionfs_readdir would
	 * always restart with the upper layer (uio_offset == 0) and
	 * always return some data.
	 *
	 * This happens when the lower layer root directory is removed.
	 * (A root directory deleting of unionfs should not be permitted.
	 *  But current VFS can not do it.)
	 */
	if (uio->uio_offset == 0)
		uio->uio_offset = uio_offset_bk;

	if (cookies_bk != NULL) {
		/* merge cookies */
		int		size;
		uint64_t         *newcookies, *pos;

		size = *(ap->a_ncookies) + ncookies_bk;
		newcookies = (uint64_t *) malloc(size * sizeof(*newcookies),
		    M_TEMP, M_WAITOK);
		pos = newcookies;

		memcpy(pos, cookies_bk, ncookies_bk * sizeof(*newcookies));
		pos += ncookies_bk;
		memcpy(pos, *(ap->a_cookies),
		    *(ap->a_ncookies) * sizeof(*newcookies));
		free(cookies_bk, M_TEMP);
		free(*(ap->a_cookies), M_TEMP);
		*(ap->a_ncookies) = size;
		*(ap->a_cookies) = newcookies;
	}

unionfs_readdir_exit:
	if (error != 0 && ap->a_eofflag != NULL)
		*(ap->a_eofflag) = 1;

	UNIONFS_INTERNAL_DEBUG("unionfs_readdir: leave (%d)\n", error);

	return (error);
}

static int
unionfs_readlink(struct vop_readlink_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;
	int error;

	UNIONFS_INTERNAL_DEBUG("unionfs_readlink: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	error = VOP_READLINK(vp, ap->a_uio, ap->a_cred);

	UNIONFS_INTERNAL_DEBUG("unionfs_readlink: leave (%d)\n", error);

	return (error);
}

static int
unionfs_getwritemount(struct vop_getwritemount_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *vp, *ovp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_getwritemount: enter\n");

	error = 0;
	vp = ap->a_vp;
	uvp = NULLVP;

	VI_LOCK(vp);
	unp = VTOUNIONFS(vp);
	if (unp != NULL)
		uvp = unp->un_uppervp;

	/*
	 * If our node has no upper vnode, check the parent directory.
	 * We may be initiating a write operation that will produce a
	 * new upper vnode through CoW.
	 */
	if (uvp == NULLVP && unp != NULL) {
		ovp = vp;
		vp = unp->un_dvp;
		/*
		 * Only the root vnode should have an empty parent, but it
		 * should not have an empty uppervp, so we shouldn't get here.
		 */
		VNASSERT(vp != NULL, ovp, ("%s: NULL parent vnode", __func__));
		VI_UNLOCK(ovp);
		VI_LOCK(vp);
		unp = VTOUNIONFS(vp);
		if (unp != NULL)
			uvp = unp->un_uppervp;
		if (uvp == NULLVP)
			error = EACCES;
	}

	if (uvp != NULLVP) {
		vholdnz(uvp);
		VI_UNLOCK(vp);
		error = VOP_GETWRITEMOUNT(uvp, ap->a_mpp);
		vdrop(uvp);
	} else {
		VI_UNLOCK(vp);
		*(ap->a_mpp) = NULL;
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_getwritemount: leave (%d)\n", error);

	return (error);
}

static int
unionfs_inactive(struct vop_inactive_args *ap)
{
	ap->a_vp->v_object = NULL;
	vrecycle(ap->a_vp);
	return (0);
}

static int
unionfs_reclaim(struct vop_reclaim_args *ap)
{
	/* UNIONFS_INTERNAL_DEBUG("unionfs_reclaim: enter\n"); */

	unionfs_noderem(ap->a_vp);

	/* UNIONFS_INTERNAL_DEBUG("unionfs_reclaim: leave\n"); */

	return (0);
}

static int
unionfs_print(struct vop_print_args *ap)
{
	struct unionfs_node *unp;
	/* struct unionfs_node_status *unsp; */

	unp = VTOUNIONFS(ap->a_vp);
	/* unionfs_get_node_status(unp, curthread, &unsp); */

	printf("unionfs_vp=%p, uppervp=%p, lowervp=%p\n",
	    ap->a_vp, unp->un_uppervp, unp->un_lowervp);
	/*
	printf("unionfs opencnt: uppervp=%d, lowervp=%d\n",
	    unsp->uns_upper_opencnt, unsp->uns_lower_opencnt);
	*/

	if (unp->un_uppervp != NULLVP)
		vn_printf(unp->un_uppervp, "unionfs: upper ");
	if (unp->un_lowervp != NULLVP)
		vn_printf(unp->un_lowervp, "unionfs: lower ");

	return (0);
}

static int
unionfs_lock(struct vop_lock1_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;
	struct vnode   *tvp;
	int		error;
	int		flags;
	bool		lvp_locked;

	error = 0;
	flags = ap->a_flags;
	vp = ap->a_vp;

	if (LK_RELEASE == (flags & LK_TYPE_MASK) || !(flags & LK_TYPE_MASK))
		return (VOP_UNLOCK_FLAGS(vp, flags | LK_RELEASE));

unionfs_lock_restart:
	/*
	 * We currently need the interlock here to ensure we can safely
	 * access the unionfs vnode's private data.  We may be able to
	 * eliminate this extra locking by instead using vfs_smr_enter()
	 * and vn_load_v_data_smr() here in conjunction with an SMR UMA
	 * zone for unionfs nodes.
	 */
	if ((flags & LK_INTERLOCK) == 0)
		VI_LOCK(vp);
	else
		flags &= ~LK_INTERLOCK;

	unp = VTOUNIONFS(vp);
	if (unp == NULL) {
		VI_UNLOCK(vp);
		ap->a_flags = flags;
		return (vop_stdlock(ap));
	}

	if (unp->un_uppervp != NULL) {
		tvp = unp->un_uppervp;
		lvp_locked = false;
	} else {
		tvp = unp->un_lowervp;
		lvp_locked = true;
	}

	/*
	 * During unmount, the root vnode lock may be taken recursively,
	 * because it may share the same v_vnlock field as the vnode covered by
	 * the unionfs mount.  The covered vnode is locked across VFS_UNMOUNT(),
	 * and the same lock may be taken recursively here during vflush()
	 * issued by unionfs_unmount().
	 */
	if ((flags & LK_TYPE_MASK) == LK_EXCLUSIVE &&
	    (vp->v_vflag & VV_ROOT) != 0)
		flags |= LK_CANRECURSE;

	vholdnz(tvp);
	VI_UNLOCK(vp);
	error = VOP_LOCK(tvp, flags);
	vdrop(tvp);
	if (error == 0 && (lvp_locked || VTOUNIONFS(vp) == NULL)) {
		/*
		 * After dropping the interlock above, there exists a window
		 * in which another thread may acquire the lower vnode lock
		 * and then either doom the unionfs vnode or create an upper
		 * vnode.  In either case, we will effectively be holding the
		 * wrong lock, so we must drop the lower vnode lock and
		 * restart the lock operation.
		 *
		 * If unp is not already NULL, we assume that we can safely
		 * access it because we currently hold lvp's lock.
		 * unionfs_noderem() acquires lvp's lock before freeing
		 * the vnode private data, ensuring it can't be concurrently
		 * freed while we are using it here.  Likewise,
		 * unionfs_node_update() acquires lvp's lock before installing
		 * an upper vnode.  Without those guarantees, we would need to
		 * reacquire the vnode interlock here.
		 * Note that unionfs_noderem() doesn't acquire lvp's lock if
		 * this is the root vnode, but the root vnode should always
		 * have an upper vnode and therefore we should never use its
		 * lower vnode lock here.
		 */
		unp = VTOUNIONFS(vp);
		if (unp == NULL || unp->un_uppervp != NULLVP) {
			VOP_UNLOCK(tvp);
			/*
			 * If we previously held the lock, the upgrade may
			 * have temporarily dropped the lock, in which case
			 * concurrent dooming or copy-up will necessitate
			 * acquiring a different lock.  Since we never held
			 * the new lock, LK_UPGRADE must be cleared here to
			 * avoid triggering a lockmgr panic.
			 */
			if (flags & LK_UPGRADE)
				flags = (flags & ~LK_TYPE_MASK) | LK_EXCLUSIVE;
			VNASSERT((flags & LK_DOWNGRADE) == 0, vp,
			    ("%s: vnode doomed during downgrade", __func__));
			goto unionfs_lock_restart;
		}
	}

	return (error);
}

static int
unionfs_unlock(struct vop_unlock_args *ap)
{
	struct vnode   *vp;
	struct vnode   *tvp;
	struct unionfs_node *unp;
	int		error;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	vp = ap->a_vp;

	unp = VTOUNIONFS(vp);
	if (unp == NULL)
		return (vop_stdunlock(ap));

	tvp = (unp->un_uppervp != NULL ? unp->un_uppervp : unp->un_lowervp);

	vholdnz(tvp);
	error = VOP_UNLOCK(tvp);
	vdrop(tvp);

	return (error);
}

static int
unionfs_pathconf(struct vop_pathconf_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	return (VOP_PATHCONF(vp, ap->a_name, ap->a_retval));
}

static int
unionfs_advlock(struct vop_advlock_args *ap)
{
	struct unionfs_node *unp;
	struct unionfs_node_status *unsp;
	struct vnode   *vp;
	struct vnode   *uvp;
	struct thread  *td;
	int error;

	UNIONFS_INTERNAL_DEBUG("unionfs_advlock: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	vp = ap->a_vp;
	td = curthread;

	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;

	if (uvp == NULLVP) {
		error = unionfs_copyfile(ap->a_vp, 1, td->td_ucred, td);
		if (error != 0)
			goto unionfs_advlock_abort;
		uvp = unp->un_uppervp;

		unionfs_get_node_status(unp, td, &unsp);
		if (unsp->uns_lower_opencnt > 0) {
			/* try reopen the vnode */
			error = VOP_OPEN(uvp, unsp->uns_lower_openmode,
				td->td_ucred, td, NULL);
			if (error)
				goto unionfs_advlock_abort;
			unsp->uns_upper_opencnt++;
			VOP_CLOSE(unp->un_lowervp, unsp->uns_lower_openmode,
			    td->td_ucred, td);
			unsp->uns_lower_opencnt--;
		} else
			unionfs_tryrem_node_status(unp, unsp);
	}

	VOP_UNLOCK(vp);

	error = VOP_ADVLOCK(uvp, ap->a_id, ap->a_op, ap->a_fl, ap->a_flags);

	UNIONFS_INTERNAL_DEBUG("unionfs_advlock: leave (%d)\n", error);

	return error;

unionfs_advlock_abort:
	VOP_UNLOCK(vp);

	UNIONFS_INTERNAL_DEBUG("unionfs_advlock: leave (%d)\n", error);

	return error;
}

static int
unionfs_strategy(struct vop_strategy_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

#ifdef DIAGNOSTIC
	if (vp == NULLVP)
		panic("unionfs_strategy: nullvp");

	if (ap->a_bp->b_iocmd == BIO_WRITE && vp == unp->un_lowervp)
		panic("unionfs_strategy: writing to lowervp");
#endif

	return (VOP_STRATEGY(vp, ap->a_bp));
}

static int
unionfs_getacl(struct vop_getacl_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;
	int		error;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	UNIONFS_INTERNAL_DEBUG("unionfs_getacl: enter\n");

	error = VOP_GETACL(vp, ap->a_type, ap->a_aclp, ap->a_cred, ap->a_td);

	UNIONFS_INTERNAL_DEBUG("unionfs_getacl: leave (%d)\n", error);

	return (error);
}

static int
unionfs_setacl(struct vop_setacl_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct thread  *td;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_setacl: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = EROFS;
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	td = ap->a_td;

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	if (uvp == NULLVP && lvp->v_type == VREG) {
		if ((error = unionfs_copyfile(ap->a_vp, 1, ap->a_cred, td)) != 0)
			return (error);
		uvp = unp->un_uppervp;
	}

	if (uvp != NULLVP) {
		int lkflags;
		unionfs_forward_vop_start(uvp, &lkflags);
		error = VOP_SETACL(uvp, ap->a_type, ap->a_aclp, ap->a_cred, td);
		unionfs_forward_vop_finish(ap->a_vp, uvp, lkflags);
	}

	UNIONFS_INTERNAL_DEBUG("unionfs_setacl: leave (%d)\n", error);

	return (error);
}

static int
unionfs_aclcheck(struct vop_aclcheck_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_aclcheck: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	vp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	error = VOP_ACLCHECK(vp, ap->a_type, ap->a_aclp, ap->a_cred, ap->a_td);

	UNIONFS_INTERNAL_DEBUG("unionfs_aclcheck: leave (%d)\n", error);

	return (error);
}

static int
unionfs_openextattr(struct vop_openextattr_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;
	struct vnode   *tvp;
	int		error;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	vp = ap->a_vp;
	unp = VTOUNIONFS(vp);
	tvp = (unp->un_uppervp != NULLVP ? unp->un_uppervp : unp->un_lowervp);

	if ((tvp == unp->un_uppervp && (unp->un_flag & UNIONFS_OPENEXTU)) ||
	    (tvp == unp->un_lowervp && (unp->un_flag & UNIONFS_OPENEXTL)))
		return (EBUSY);

	error = VOP_OPENEXTATTR(tvp, ap->a_cred, ap->a_td);

	if (error == 0) {
		if (vn_lock(vp, LK_UPGRADE) != 0)
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		if (!VN_IS_DOOMED(vp)) {
			if (tvp == unp->un_uppervp)
				unp->un_flag |= UNIONFS_OPENEXTU;
			else
				unp->un_flag |= UNIONFS_OPENEXTL;
		}
		vn_lock(vp, LK_DOWNGRADE | LK_RETRY);
	}

	return (error);
}

static int
unionfs_closeextattr(struct vop_closeextattr_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;
	struct vnode   *tvp;
	int		error;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	vp = ap->a_vp;
	unp = VTOUNIONFS(vp);
	tvp = NULLVP;

	if (unp->un_flag & UNIONFS_OPENEXTU)
		tvp = unp->un_uppervp;
	else if (unp->un_flag & UNIONFS_OPENEXTL)
		tvp = unp->un_lowervp;

	if (tvp == NULLVP)
		return (EOPNOTSUPP);

	error = VOP_CLOSEEXTATTR(tvp, ap->a_commit, ap->a_cred, ap->a_td);

	if (error == 0) {
		if (vn_lock(vp, LK_UPGRADE) != 0)
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		if (!VN_IS_DOOMED(vp)) {
			if (tvp == unp->un_uppervp)
				unp->un_flag &= ~UNIONFS_OPENEXTU;
			else
				unp->un_flag &= ~UNIONFS_OPENEXTL;
		}
		vn_lock(vp, LK_DOWNGRADE | LK_RETRY);
	}

	return (error);
}

static int
unionfs_getextattr(struct vop_getextattr_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *vp;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	vp = NULLVP;

	if (unp->un_flag & UNIONFS_OPENEXTU)
		vp = unp->un_uppervp;
	else if (unp->un_flag & UNIONFS_OPENEXTL)
		vp = unp->un_lowervp;

	if (vp == NULLVP)
		return (EOPNOTSUPP);

	return (VOP_GETEXTATTR(vp, ap->a_attrnamespace, ap->a_name,
	    ap->a_uio, ap->a_size, ap->a_cred, ap->a_td));
}

static int
unionfs_setextattr(struct vop_setextattr_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *ovp;
	struct ucred   *cred;
	struct thread  *td;
	int		error;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = EROFS;
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	ovp = NULLVP;
	cred = ap->a_cred;
	td = ap->a_td;

	UNIONFS_INTERNAL_DEBUG("unionfs_setextattr: enter (un_flag=%x)\n",
	    unp->un_flag);

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	if (unp->un_flag & UNIONFS_OPENEXTU)
		ovp = unp->un_uppervp;
	else if (unp->un_flag & UNIONFS_OPENEXTL)
		ovp = unp->un_lowervp;

	if (ovp == NULLVP)
		return (EOPNOTSUPP);

	if (ovp == lvp && lvp->v_type == VREG) {
		VOP_CLOSEEXTATTR(lvp, 0, cred, td);
		if (uvp == NULLVP &&
		    (error = unionfs_copyfile(ap->a_vp, 1, cred, td)) != 0) {
unionfs_setextattr_reopen:
			unp = VTOUNIONFS(ap->a_vp);
			if (unp != NULL && (unp->un_flag & UNIONFS_OPENEXTL) &&
			    VOP_OPENEXTATTR(lvp, cred, td)) {
#ifdef DIAGNOSTIC
				panic("unionfs: VOP_OPENEXTATTR failed");
#endif
				unp->un_flag &= ~UNIONFS_OPENEXTL;
			}
			goto unionfs_setextattr_abort;
		}
		uvp = unp->un_uppervp;
		if ((error = VOP_OPENEXTATTR(uvp, cred, td)) != 0)
			goto unionfs_setextattr_reopen;
		unp->un_flag &= ~UNIONFS_OPENEXTL;
		unp->un_flag |= UNIONFS_OPENEXTU;
		ovp = uvp;
	}

	if (ovp == uvp) {
		int lkflags;
		unionfs_forward_vop_start(ovp, &lkflags);
		error = VOP_SETEXTATTR(ovp, ap->a_attrnamespace, ap->a_name,
		    ap->a_uio, cred, td);
		unionfs_forward_vop_finish(ap->a_vp, ovp, lkflags);
	}

unionfs_setextattr_abort:
	UNIONFS_INTERNAL_DEBUG("unionfs_setextattr: leave (%d)\n", error);

	return (error);
}

static int
unionfs_listextattr(struct vop_listextattr_args *ap)
{
	struct unionfs_node *unp;
	struct vnode *vp;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	unp = VTOUNIONFS(ap->a_vp);
	vp = NULLVP;

	if (unp->un_flag & UNIONFS_OPENEXTU)
		vp = unp->un_uppervp;
	else if (unp->un_flag & UNIONFS_OPENEXTL)
		vp = unp->un_lowervp;

	if (vp == NULLVP)
		return (EOPNOTSUPP);

	return (VOP_LISTEXTATTR(vp, ap->a_attrnamespace, ap->a_uio,
	    ap->a_size, ap->a_cred, ap->a_td));
}

static int
unionfs_deleteextattr(struct vop_deleteextattr_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *ovp;
	struct ucred   *cred;
	struct thread  *td;
	int		error;

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = EROFS;
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	ovp = NULLVP;
	cred = ap->a_cred;
	td = ap->a_td;

	UNIONFS_INTERNAL_DEBUG("unionfs_deleteextattr: enter (un_flag=%x)\n",
	    unp->un_flag);

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	if (unp->un_flag & UNIONFS_OPENEXTU)
		ovp = unp->un_uppervp;
	else if (unp->un_flag & UNIONFS_OPENEXTL)
		ovp = unp->un_lowervp;

	if (ovp == NULLVP)
		return (EOPNOTSUPP);

	if (ovp == lvp && lvp->v_type == VREG) {
		VOP_CLOSEEXTATTR(lvp, 0, cred, td);
		if (uvp == NULLVP &&
		    (error = unionfs_copyfile(ap->a_vp, 1, cred, td)) != 0) {
unionfs_deleteextattr_reopen:
			unp = VTOUNIONFS(ap->a_vp);
			if (unp != NULL && (unp->un_flag & UNIONFS_OPENEXTL) &&
			    VOP_OPENEXTATTR(lvp, cred, td)) {
#ifdef DIAGNOSTIC
				panic("unionfs: VOP_OPENEXTATTR failed");
#endif
				unp->un_flag &= ~UNIONFS_OPENEXTL;
			}
			goto unionfs_deleteextattr_abort;
		}
		uvp = unp->un_uppervp;
		if ((error = VOP_OPENEXTATTR(uvp, cred, td)) != 0)
			goto unionfs_deleteextattr_reopen;
		unp->un_flag &= ~UNIONFS_OPENEXTL;
		unp->un_flag |= UNIONFS_OPENEXTU;
		ovp = uvp;
	}

	if (ovp == uvp)
		error = VOP_DELETEEXTATTR(ovp, ap->a_attrnamespace, ap->a_name,
		    ap->a_cred, ap->a_td);

unionfs_deleteextattr_abort:
	UNIONFS_INTERNAL_DEBUG("unionfs_deleteextattr: leave (%d)\n", error);

	return (error);
}

static int
unionfs_setlabel(struct vop_setlabel_args *ap)
{
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct thread  *td;
	int		error;

	UNIONFS_INTERNAL_DEBUG("unionfs_setlabel: enter\n");

	KASSERT_UNIONFS_VNODE(ap->a_vp);

	error = EROFS;
	unp = VTOUNIONFS(ap->a_vp);
	uvp = unp->un_uppervp;
	lvp = unp->un_lowervp;
	td = ap->a_td;

	if (ap->a_vp->v_mount->mnt_flag & MNT_RDONLY)
		return (EROFS);

	if (uvp == NULLVP && lvp->v_type == VREG) {
		if ((error = unionfs_copyfile(ap->a_vp, 1, ap->a_cred, td)) != 0)
			return (error);
		uvp = unp->un_uppervp;
	}

	if (uvp != NULLVP)
		error = VOP_SETLABEL(uvp, ap->a_label, ap->a_cred, td);

	UNIONFS_INTERNAL_DEBUG("unionfs_setlabel: leave (%d)\n", error);

	return (error);
}

static int
unionfs_vptofh(struct vop_vptofh_args *ap)
{
	return (EOPNOTSUPP);
}

static int
unionfs_add_writecount(struct vop_add_writecount_args *ap)
{
	struct vnode *tvp, *vp;
	struct unionfs_node *unp;
	int error, writerefs __diagused;

	vp = ap->a_vp;
	unp = VTOUNIONFS(vp);
	tvp = unp->un_uppervp;
	KASSERT(tvp != NULL,
	    ("%s: adding write ref without upper vnode", __func__));
	error = VOP_ADD_WRITECOUNT(tvp, ap->a_inc);
	if (error != 0)
		return (error);
	/*
	 * We need to track the write refs we've passed to the underlying
	 * vnodes so that we can undo them in case we are forcibly unmounted.
	 */
	writerefs = atomic_fetchadd_int(&vp->v_writecount, ap->a_inc);
	/* text refs are bypassed to lowervp */
	VNASSERT(writerefs >= 0, vp,
	    ("%s: invalid write count %d", __func__, writerefs));
	VNASSERT(writerefs + ap->a_inc >= 0, vp,
	    ("%s: invalid write count inc %d + %d", __func__,
	    writerefs, ap->a_inc));
	return (0);
}

static int
unionfs_vput_pair(struct vop_vput_pair_args *ap)
{
	struct mount *mp;
	struct vnode *dvp, *vp, **vpp, *lvp, *uvp, *tvp, *tdvp, *tempvp;
	struct unionfs_node *dunp, *unp;
	int error, res;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	vp = NULLVP;
	lvp = NULLVP;
	uvp = NULLVP;
	tvp = NULLVP;
	unp = NULL;

	dunp = VTOUNIONFS(dvp);
	if (dunp->un_uppervp != NULL)
		tdvp = dunp->un_uppervp;
	else
		tdvp = dunp->un_lowervp;

	/*
	 * Underlying vnodes should be locked because the encompassing unionfs
	 * node is locked, but will not be referenced, as the reference will
	 * only be on the unionfs node.  Reference them now so that the vput()s
	 * performed by VOP_VPUT_PAIR() will have a reference to drop.
	 */
	vref(tdvp);

	if (vpp != NULL)
		vp = *vpp;

	if (vp != NULLVP) {
		unp = VTOUNIONFS(vp);
		uvp = unp->un_uppervp;
		lvp = unp->un_lowervp;
		if (uvp != NULLVP)
			tvp = uvp;
		else
			tvp = lvp;
		vref(tvp);

		/*
		 * If we're being asked to return a locked child vnode, then
		 * we may need to create a replacement vnode in case the
		 * original is reclaimed while the lock is dropped.  In that
		 * case we'll need to ensure the mount and the underlying
		 * vnodes aren't also recycled during that window.
		 */
		if (!ap->a_unlock_vp) {
			vhold(vp);
			if (uvp != NULLVP)
				vhold(uvp);
			if (lvp != NULLVP)
				vhold(lvp);
			mp = vp->v_mount;
			vfs_ref(mp);
		}
	}

	ASSERT_VOP_LOCKED(tdvp, __func__);
	ASSERT_VOP_LOCKED(tvp, __func__);

	if (tdvp == dunp->un_uppervp && tvp != NULLVP && tvp == lvp) {
		vput(tvp);
		vput(tdvp);
		res = 0;
	} else {
		res = VOP_VPUT_PAIR(tdvp, tvp != NULLVP ? &tvp : NULL, true);
	}

	ASSERT_VOP_UNLOCKED(tdvp, __func__);
	ASSERT_VOP_UNLOCKED(tvp, __func__);

	/*
	 * VOP_VPUT_PAIR() dropped the references we added to the underlying
	 * vnodes, now drop the caller's reference to the unionfs vnodes.
	 */
	if (vp != NULLVP && ap->a_unlock_vp)
		vrele(vp);
	vrele(dvp);

	if (vp == NULLVP || ap->a_unlock_vp)
		return (res);

	/*
	 * We're being asked to return a locked vnode.  At this point, the
	 * underlying vnodes have been unlocked, so vp may have been reclaimed.
	 */
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (vp->v_data == NULL && vfs_busy(mp, MBF_NOWAIT) == 0) {
		vput(vp);
		error = unionfs_nodeget(mp, uvp, lvp, dvp, &tempvp, NULL);
		if (error == 0) {
			vn_lock(tempvp, LK_EXCLUSIVE | LK_RETRY);
			*vpp = tempvp;
		} else
			vget(vp, LK_EXCLUSIVE | LK_RETRY);
		vfs_unbusy(mp);
	}
	if (lvp != NULLVP)
		vdrop(lvp);
	if (uvp != NULLVP)
		vdrop(uvp);
	vdrop(vp);
	vfs_rel(mp);

	return (res);
}

static int
unionfs_set_text(struct vop_set_text_args *ap)
{
	struct vnode *tvp;
	struct unionfs_node *unp;
	int error;

	/*
	 * We assume text refs are managed against lvp/uvp through the
	 * executable mapping backed by its VM object.  We therefore don't
	 * need to track leased text refs in the case of a forcible unmount.
	 */
	unp = VTOUNIONFS(ap->a_vp);
	ASSERT_VOP_LOCKED(ap->a_vp, __func__);
	tvp = unp->un_uppervp != NULL ? unp->un_uppervp : unp->un_lowervp;
	error = VOP_SET_TEXT(tvp);
	return (error);
}

static int
unionfs_unset_text(struct vop_unset_text_args *ap)
{
	struct vnode *tvp;
	struct unionfs_node *unp;

	ASSERT_VOP_LOCKED(ap->a_vp, __func__);
	unp = VTOUNIONFS(ap->a_vp);
	tvp = unp->un_uppervp != NULL ? unp->un_uppervp : unp->un_lowervp;
	VOP_UNSET_TEXT_CHECKED(tvp);
	return (0);
}

static int
unionfs_unp_bind(struct vop_unp_bind_args *ap)
{
	struct vnode *tvp;
	struct unionfs_node *unp;

	ASSERT_VOP_LOCKED(ap->a_vp, __func__);
	unp = VTOUNIONFS(ap->a_vp);
	tvp = unp->un_uppervp != NULL ? unp->un_uppervp : unp->un_lowervp;
	VOP_UNP_BIND(tvp, ap->a_unpcb);
	return (0);
}

static int
unionfs_unp_connect(struct vop_unp_connect_args *ap)
{
	struct vnode *tvp;
	struct unionfs_node *unp;

	ASSERT_VOP_LOCKED(ap->a_vp, __func__);
	unp = VTOUNIONFS(ap->a_vp);
	tvp = unp->un_uppervp != NULL ? unp->un_uppervp : unp->un_lowervp;
	VOP_UNP_CONNECT(tvp, ap->a_unpcb);
	return (0);
}

static int
unionfs_unp_detach(struct vop_unp_detach_args *ap)
{
	struct vnode *tvp;
	struct unionfs_node *unp;

	tvp = NULL;
	/*
	 * VOP_UNP_DETACH() is not guaranteed to be called with the unionfs
	 * vnode locked, so we take the interlock to prevent a concurrent
	 * unmount from freeing the unionfs private data.
	 */
	VI_LOCK(ap->a_vp);
	unp = VTOUNIONFS(ap->a_vp);
	if (unp != NULL) {
		tvp = unp->un_uppervp != NULL ?
		    unp->un_uppervp : unp->un_lowervp;
		/*
		 * Hold the target vnode to prevent a concurrent unionfs
		 * unmount from causing it to be recycled once the interlock
		 * is dropped.
		 */
		vholdnz(tvp);
	}
	VI_UNLOCK(ap->a_vp);
	if (tvp != NULL) {
		VOP_UNP_DETACH(tvp);
		vdrop(tvp);
	}
	return (0);
}

struct vop_vector unionfs_vnodeops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		unionfs_access,
	.vop_aclcheck =		unionfs_aclcheck,
	.vop_advlock =		unionfs_advlock,
	.vop_bmap =		VOP_EOPNOTSUPP,
	.vop_cachedlookup =	unionfs_lookup,
	.vop_close =		unionfs_close,
	.vop_closeextattr =	unionfs_closeextattr,
	.vop_create =		unionfs_create,
	.vop_deleteextattr =	unionfs_deleteextattr,
	.vop_fsync =		unionfs_fsync,
	.vop_getacl =		unionfs_getacl,
	.vop_getattr =		unionfs_getattr,
	.vop_getextattr =	unionfs_getextattr,
	.vop_getwritemount =	unionfs_getwritemount,
	.vop_inactive =		unionfs_inactive,
	.vop_need_inactive =	vop_stdneed_inactive,
	.vop_islocked =		vop_stdislocked,
	.vop_ioctl =		unionfs_ioctl,
	.vop_link =		unionfs_link,
	.vop_listextattr =	unionfs_listextattr,
	.vop_lock1 =		unionfs_lock,
	.vop_lookup =		vfs_cache_lookup,
	.vop_mkdir =		unionfs_mkdir,
	.vop_mknod =		unionfs_mknod,
	.vop_open =		unionfs_open,
	.vop_openextattr =	unionfs_openextattr,
	.vop_pathconf =		unionfs_pathconf,
	.vop_poll =		unionfs_poll,
	.vop_print =		unionfs_print,
	.vop_read =		unionfs_read,
	.vop_readdir =		unionfs_readdir,
	.vop_readlink =		unionfs_readlink,
	.vop_reclaim =		unionfs_reclaim,
	.vop_remove =		unionfs_remove,
	.vop_rename =		unionfs_rename,
	.vop_rmdir =		unionfs_rmdir,
	.vop_setacl =		unionfs_setacl,
	.vop_setattr =		unionfs_setattr,
	.vop_setextattr =	unionfs_setextattr,
	.vop_setlabel =		unionfs_setlabel,
	.vop_strategy =		unionfs_strategy,
	.vop_symlink =		unionfs_symlink,
	.vop_unlock =		unionfs_unlock,
	.vop_whiteout =		unionfs_whiteout,
	.vop_write =		unionfs_write,
	.vop_vptofh =		unionfs_vptofh,
	.vop_add_writecount =	unionfs_add_writecount,
	.vop_vput_pair =	unionfs_vput_pair,
	.vop_set_text =		unionfs_set_text,
	.vop_unset_text = 	unionfs_unset_text,
	.vop_unp_bind =		unionfs_unp_bind,
	.vop_unp_connect =	unionfs_unp_connect,
	.vop_unp_detach =	unionfs_unp_detach,
};
VFS_VOP_VECTOR_REGISTER(unionfs_vnodeops);
