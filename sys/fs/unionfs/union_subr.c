/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1994 Jan-Simon Pendry
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2005, 2006, 2012 Masanori Ozawa <ozawa@ongs.co.jp>, ONGS Inc.
 * Copyright (c) 2006, 2012 Daichi Goto <daichi@freebsd.org>
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
 *	@(#)union_subr.c	8.20 (Berkeley) 5/20/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/resourcevar.h>

#include <machine/atomic.h>

#include <security/mac/mac_framework.h>

#include <vm/uma.h>

#include <fs/unionfs/union.h>

#define NUNIONFSNODECACHE 16
#define UNIONFSHASHMASK (NUNIONFSNODECACHE - 1)

static MALLOC_DEFINE(M_UNIONFSHASH, "UNIONFS hash", "UNIONFS hash table");
MALLOC_DEFINE(M_UNIONFSNODE, "UNIONFS node", "UNIONFS vnode private part");
MALLOC_DEFINE(M_UNIONFSPATH, "UNIONFS path", "UNIONFS path private part");

static struct task unionfs_deferred_rele_task;
static struct mtx unionfs_deferred_rele_lock;
static STAILQ_HEAD(, unionfs_node) unionfs_deferred_rele_list =
    STAILQ_HEAD_INITIALIZER(unionfs_deferred_rele_list);
static TASKQUEUE_DEFINE_THREAD(unionfs_rele);

unsigned int unionfs_ndeferred = 0;
SYSCTL_UINT(_vfs, OID_AUTO, unionfs_ndeferred, CTLFLAG_RD,
    &unionfs_ndeferred, 0, "unionfs deferred vnode release");

static void unionfs_deferred_rele(void *, int);

/*
 * Initialize
 */
int 
unionfs_init(struct vfsconf *vfsp)
{
	UNIONFSDEBUG("unionfs_init\n");	/* printed during system boot */
	TASK_INIT(&unionfs_deferred_rele_task, 0, unionfs_deferred_rele, NULL);
	mtx_init(&unionfs_deferred_rele_lock, "uniondefr", NULL, MTX_DEF); 
	return (0);
}

/*
 * Uninitialize
 */
int 
unionfs_uninit(struct vfsconf *vfsp)
{
	taskqueue_quiesce(taskqueue_unionfs_rele);
	taskqueue_free(taskqueue_unionfs_rele);
	mtx_destroy(&unionfs_deferred_rele_lock);
	return (0);
}

static void
unionfs_deferred_rele(void *arg __unused, int pending __unused)
{
	STAILQ_HEAD(, unionfs_node) local_rele_list;
	struct unionfs_node *unp, *tunp;
	unsigned int ndeferred;

	ndeferred = 0;
	STAILQ_INIT(&local_rele_list);
	mtx_lock(&unionfs_deferred_rele_lock);
	STAILQ_CONCAT(&local_rele_list, &unionfs_deferred_rele_list);
	mtx_unlock(&unionfs_deferred_rele_lock);
	STAILQ_FOREACH_SAFE(unp, &local_rele_list, un_rele, tunp) {
		++ndeferred;
		MPASS(unp->un_dvp != NULL);
		vrele(unp->un_dvp);
		free(unp, M_UNIONFSNODE);
	}

	/* We expect this function to be single-threaded, thus no atomic */
	unionfs_ndeferred += ndeferred;
}

static struct unionfs_node_hashhead *
unionfs_get_hashhead(struct vnode *dvp, struct vnode *lookup)
{
	struct unionfs_node *unp;

	unp = VTOUNIONFS(dvp);

	return (&(unp->un_hashtbl[vfs_hash_index(lookup) & UNIONFSHASHMASK]));
}

/*
 * Attempt to lookup a cached unionfs vnode by upper/lower vp
 * from dvp, with dvp's interlock held.
 */
static struct vnode *
unionfs_get_cached_vnode_locked(struct vnode *lookup, struct vnode *dvp)
{
	struct unionfs_node *unp;
	struct unionfs_node_hashhead *hd;
	struct vnode *vp;

	hd = unionfs_get_hashhead(dvp, lookup);

	LIST_FOREACH(unp, hd, un_hash) {
		if (unp->un_uppervp == lookup ||
		    unp->un_lowervp == lookup) {
			vp = UNIONFSTOV(unp);
			VI_LOCK_FLAGS(vp, MTX_DUPOK);
			vp->v_iflag &= ~VI_OWEINACT;
			if (VN_IS_DOOMED(vp) ||
			    ((vp->v_iflag & VI_DOINGINACT) != 0)) {
				VI_UNLOCK(vp);
				vp = NULLVP;
			} else {
				vrefl(vp);
				VI_UNLOCK(vp);
			}
			return (vp);
		}
	}

	return (NULLVP);
}


/*
 * Get the cached vnode.
 */
static struct vnode *
unionfs_get_cached_vnode(struct vnode *uvp, struct vnode *lvp,
    struct vnode *dvp)
{
	struct vnode *vp;

	vp = NULLVP;
	VI_LOCK(dvp);
	if (uvp != NULLVP)
		vp = unionfs_get_cached_vnode_locked(uvp, dvp);
	else if (lvp != NULLVP)
		vp = unionfs_get_cached_vnode_locked(lvp, dvp);
	VI_UNLOCK(dvp);

	return (vp);
}

/*
 * Add the new vnode into cache.
 */
static struct vnode *
unionfs_ins_cached_vnode(struct unionfs_node *uncp,
    struct vnode *dvp)
{
	struct unionfs_node_hashhead *hd;
	struct vnode *vp;

	ASSERT_VOP_ELOCKED(uncp->un_uppervp, __func__);
	ASSERT_VOP_ELOCKED(uncp->un_lowervp, __func__);
	KASSERT(uncp->un_uppervp == NULLVP || uncp->un_uppervp->v_type == VDIR,
	    ("%s: v_type != VDIR", __func__));
	KASSERT(uncp->un_lowervp == NULLVP || uncp->un_lowervp->v_type == VDIR,
	    ("%s: v_type != VDIR", __func__));

	vp = NULLVP;
	VI_LOCK(dvp);
	if (uncp->un_uppervp != NULL)
		vp = unionfs_get_cached_vnode_locked(uncp->un_uppervp, dvp);
	else if (uncp->un_lowervp != NULL)
		vp = unionfs_get_cached_vnode_locked(uncp->un_lowervp, dvp);
	if (vp == NULLVP) {
		hd = unionfs_get_hashhead(dvp, (uncp->un_uppervp != NULLVP ?
		    uncp->un_uppervp : uncp->un_lowervp));
		LIST_INSERT_HEAD(hd, uncp, un_hash);
	}
	VI_UNLOCK(dvp);

	return (vp);
}

/*
 * Remove the vnode.
 */
static void
unionfs_rem_cached_vnode(struct unionfs_node *unp, struct vnode *dvp)
{
	KASSERT(unp != NULL, ("%s: null node", __func__));
	KASSERT(dvp != NULLVP,
	    ("%s: null parent vnode", __func__));

	VI_LOCK(dvp);
	if (unp->un_hash.le_prev != NULL) {
		LIST_REMOVE(unp, un_hash);
		unp->un_hash.le_next = NULL;
		unp->un_hash.le_prev = NULL;
	}
	VI_UNLOCK(dvp);
}

/*
 * Common cleanup handling for unionfs_nodeget
 * Upper, lower, and parent directory vnodes are expected to be referenced by
 * the caller.  Upper and lower vnodes, if non-NULL, are also expected to be
 * exclusively locked by the caller.
 * This function will return with the caller's locks and references undone.
 */
static void
unionfs_nodeget_cleanup(struct vnode *vp, struct unionfs_node *unp)
{

	/*
	 * Lock and reset the default vnode lock; vgone() expects a locked
	 * vnode, and we're going to reset the vnode ops.
	 */
	lockmgr(&vp->v_lock, LK_EXCLUSIVE, NULL);

	/*
	 * Clear out private data and reset the vnode ops to avoid use of
	 * unionfs vnode ops on a partially constructed vnode.
	 */
	VI_LOCK(vp);
	vp->v_data = NULL;
	vp->v_vnlock = &vp->v_lock;
	vp->v_op = &dead_vnodeops;
	VI_UNLOCK(vp);
	vgone(vp);
	vput(vp);

	if (unp->un_dvp != NULLVP)
		vrele(unp->un_dvp);
	if (unp->un_uppervp != NULLVP)
		vput(unp->un_uppervp);
	if (unp->un_lowervp != NULLVP)
		vput(unp->un_lowervp);
	if (unp->un_hashtbl != NULL)
		hashdestroy(unp->un_hashtbl, M_UNIONFSHASH, UNIONFSHASHMASK);
	free(unp->un_path, M_UNIONFSPATH);
	free(unp, M_UNIONFSNODE);
}

/*
 * Make a new or get existing unionfs node.
 * 
 * uppervp and lowervp should be unlocked. Because if new unionfs vnode is
 * locked, uppervp or lowervp is locked too. In order to prevent dead lock,
 * you should not lock plurality simultaneously.
 */
int
unionfs_nodeget(struct mount *mp, struct vnode *uppervp,
    struct vnode *lowervp, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp)
{
	char	       *path;
	struct unionfs_mount *ump;
	struct unionfs_node *unp;
	struct vnode   *vp;
	u_long		hashmask;
	int		error;
	int		lkflags;
	__enum_uint8(vtype)	vt;

	error = 0;
	ump = MOUNTTOUNIONFSMOUNT(mp);
	lkflags = (cnp ? cnp->cn_lkflags : 0);
	path = (cnp ? cnp->cn_nameptr : NULL);
	*vpp = NULLVP;

	if (uppervp == NULLVP && lowervp == NULLVP)
		panic("%s: upper and lower is null", __func__);

	vt = (uppervp != NULLVP ? uppervp->v_type : lowervp->v_type);

	/* If it has no ISLASTCN flag, path check is skipped. */
	if (cnp && !(cnp->cn_flags & ISLASTCN))
		path = NULL;

	/* check the cache */
	if (dvp != NULLVP && vt == VDIR) {
		vp = unionfs_get_cached_vnode(uppervp, lowervp, dvp);
		if (vp != NULLVP) {
			*vpp = vp;
			goto unionfs_nodeget_out;
		}
	}

	unp = malloc(sizeof(struct unionfs_node),
	    M_UNIONFSNODE, M_WAITOK | M_ZERO);

	error = getnewvnode("unionfs", mp, &unionfs_vnodeops, &vp);
	if (error != 0) {
		free(unp, M_UNIONFSNODE);
		return (error);
	}
	if (dvp != NULLVP)
		vref(dvp);
	if (uppervp != NULLVP)
		vref(uppervp);
	if (lowervp != NULLVP)
		vref(lowervp);

	if (vt == VDIR) {
		unp->un_hashtbl = hashinit(NUNIONFSNODECACHE, M_UNIONFSHASH,
		    &hashmask);
		KASSERT(hashmask == UNIONFSHASHMASK,
		    ("unexpected unionfs hash mask 0x%lx", hashmask));
	}

	unp->un_vnode = vp;
	unp->un_uppervp = uppervp;
	unp->un_lowervp = lowervp;
	unp->un_dvp = dvp;
	if (uppervp != NULLVP)
		vp->v_vnlock = uppervp->v_vnlock;
	else
		vp->v_vnlock = lowervp->v_vnlock;

	if (path != NULL) {
		unp->un_path = malloc(cnp->cn_namelen + 1,
		    M_UNIONFSPATH, M_WAITOK | M_ZERO);
		bcopy(cnp->cn_nameptr, unp->un_path, cnp->cn_namelen);
		unp->un_path[cnp->cn_namelen] = '\0';
		unp->un_pathlen = cnp->cn_namelen;
	}
	vp->v_type = vt;
	vp->v_data = unp;

	/*
	 * TODO: This is an imperfect check, as there's no guarantee that
	 * the underlying filesystems will always return vnode pointers
	 * for the root inodes that match our cached values.  To reduce
	 * the likelihood of failure, for example in the case where either
	 * vnode has been forcibly doomed, we check both pointers and set
	 * VV_ROOT if either matches.
	 */
	if (ump->um_uppervp == uppervp || ump->um_lowervp == lowervp)
		vp->v_vflag |= VV_ROOT;
	KASSERT(dvp != NULL || (vp->v_vflag & VV_ROOT) != 0,
	    ("%s: NULL dvp for non-root vp %p", __func__, vp));

	vn_lock_pair(lowervp, false, LK_EXCLUSIVE, uppervp, false,
	    LK_EXCLUSIVE);
	error = insmntque1(vp, mp);
	if (error != 0) {
		unionfs_nodeget_cleanup(vp, unp);
		return (error);
	}
	if (lowervp != NULL && VN_IS_DOOMED(lowervp)) {
		vput(lowervp);
		unp->un_lowervp = lowervp = NULL;
	}
	if (uppervp != NULL && VN_IS_DOOMED(uppervp)) {
		vput(uppervp);
		unp->un_uppervp = uppervp = NULL;
		if (lowervp != NULLVP)
			vp->v_vnlock = lowervp->v_vnlock;
	}
	if (lowervp == NULL && uppervp == NULL) {
		unionfs_nodeget_cleanup(vp, unp);
		return (ENOENT);
	}

	vn_set_state(vp, VSTATE_CONSTRUCTED);

	if (dvp != NULLVP && vt == VDIR)
		*vpp = unionfs_ins_cached_vnode(unp, dvp);
	if (*vpp != NULLVP) {
		unionfs_nodeget_cleanup(vp, unp);
		vp = *vpp;
	} else {
		if (uppervp != NULL)
			VOP_UNLOCK(uppervp);
		if (lowervp != NULL)
			VOP_UNLOCK(lowervp);
		*vpp = vp;
	}

unionfs_nodeget_out:
	if (lkflags & LK_TYPE_MASK)
		vn_lock(vp, lkflags | LK_RETRY);

	return (0);
}

/*
 * Clean up the unionfs node.
 */
void
unionfs_noderem(struct vnode *vp)
{
	struct unionfs_node *unp, *unp_t1, *unp_t2;
	struct unionfs_node_hashhead *hd;
	struct unionfs_node_status *unsp, *unsp_tmp;
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct vnode   *dvp;
	int		count;
	int		writerefs;

	/*
	 * The root vnode lock may be recursed during unmount, because
	 * it may share the same lock as the unionfs mount's covered vnode,
	 * which is locked across VFS_UNMOUNT().  This lock will then be
	 * recursively taken during the vflush() issued by unionfs_unmount().
	 * But we still only need to lock the unionfs lock once, because only
	 * one of those lock operations was taken against a unionfs vnode and
	 * will be undone against a unionfs vnode.
	 */
	KASSERT(vp->v_vnlock->lk_recurse == 0 || (vp->v_vflag & VV_ROOT) != 0,
	    ("%s: vnode %p locked recursively", __func__, vp));
	if (lockmgr(&vp->v_lock, LK_EXCLUSIVE | LK_NOWAIT, NULL) != 0)
		panic("%s: failed to acquire lock for vnode lock", __func__);

	/*
	 * Use the interlock to protect the clearing of v_data to
	 * prevent faults in unionfs_lock().
	 */
	VI_LOCK(vp);
	unp = VTOUNIONFS(vp);
	lvp = unp->un_lowervp;
	uvp = unp->un_uppervp;
	dvp = unp->un_dvp;
	unp->un_lowervp = unp->un_uppervp = NULLVP;
	vp->v_vnlock = &(vp->v_lock);
	vp->v_data = NULL;
	vp->v_object = NULL;
	if (unp->un_hashtbl != NULL) {
		/*
		 * Clear out any cached child vnodes.  This should only
		 * be necessary during forced unmount, when the vnode may
		 * be reclaimed with a non-zero use count.  Otherwise the
		 * reference held by each child should prevent reclamation.
		 */
		for (count = 0; count <= UNIONFSHASHMASK; count++) {
			hd = unp->un_hashtbl + count;
			LIST_FOREACH_SAFE(unp_t1, hd, un_hash, unp_t2) {
				LIST_REMOVE(unp_t1, un_hash);
				unp_t1->un_hash.le_next = NULL;
				unp_t1->un_hash.le_prev = NULL;
			}
		}
	}
	VI_UNLOCK(vp);

	writerefs = atomic_load_int(&vp->v_writecount);
	VNASSERT(writerefs >= 0, vp,
	    ("%s: write count %d, unexpected text ref", __func__, writerefs));
	/*
	 * If we were opened for write, we leased the write reference
	 * to the lower vnode.  If this is a reclamation due to the
	 * forced unmount, undo the reference now.
	 */
	if (writerefs > 0) {
		VNASSERT(uvp != NULL, vp,
		    ("%s: write reference without upper vnode", __func__));
		VOP_ADD_WRITECOUNT(uvp, -writerefs);
	}
	if (lvp != NULLVP)
		VOP_UNLOCK(lvp);
	if (uvp != NULLVP)
		VOP_UNLOCK(uvp);

	if (dvp != NULLVP)
		unionfs_rem_cached_vnode(unp, dvp);

	if (lvp != NULLVP)
		vrele(lvp);
	if (uvp != NULLVP)
		vrele(uvp);
	if (unp->un_path != NULL) {
		free(unp->un_path, M_UNIONFSPATH);
		unp->un_path = NULL;
		unp->un_pathlen = 0;
	}

	if (unp->un_hashtbl != NULL) {
		hashdestroy(unp->un_hashtbl, M_UNIONFSHASH, UNIONFSHASHMASK);
	}

	LIST_FOREACH_SAFE(unsp, &(unp->un_unshead), uns_list, unsp_tmp) {
		LIST_REMOVE(unsp, uns_list);
		free(unsp, M_TEMP);
	}
	if (dvp != NULLVP) {
		mtx_lock(&unionfs_deferred_rele_lock);
		STAILQ_INSERT_TAIL(&unionfs_deferred_rele_list, unp, un_rele);
		mtx_unlock(&unionfs_deferred_rele_lock);
		taskqueue_enqueue(taskqueue_unionfs_rele,
		    &unionfs_deferred_rele_task);
	} else
		free(unp, M_UNIONFSNODE);
}

/*
 * Get the unionfs node status object for the vnode corresponding to unp,
 * for the process that owns td.  Allocate a new status object if one
 * does not already exist.
 */
void
unionfs_get_node_status(struct unionfs_node *unp, struct thread *td,
    struct unionfs_node_status **unspp)
{
	struct unionfs_node_status *unsp;
	pid_t pid;

	pid = td->td_proc->p_pid;

	KASSERT(NULL != unspp, ("%s: NULL status", __func__));
	ASSERT_VOP_ELOCKED(UNIONFSTOV(unp), __func__);

	LIST_FOREACH(unsp, &(unp->un_unshead), uns_list) {
		if (unsp->uns_pid == pid) {
			*unspp = unsp;
			return;
		}
	}

	/* create a new unionfs node status */
	unsp = malloc(sizeof(struct unionfs_node_status),
	    M_TEMP, M_WAITOK | M_ZERO);

	unsp->uns_pid = pid;
	LIST_INSERT_HEAD(&(unp->un_unshead), unsp, uns_list);

	*unspp = unsp;
}

/*
 * Remove the unionfs node status, if you can.
 * You need exclusive lock this vnode.
 */
void
unionfs_tryrem_node_status(struct unionfs_node *unp,
    struct unionfs_node_status *unsp)
{
	KASSERT(NULL != unsp, ("%s: NULL status", __func__));
	ASSERT_VOP_ELOCKED(UNIONFSTOV(unp), __func__);

	if (0 < unsp->uns_lower_opencnt || 0 < unsp->uns_upper_opencnt)
		return;

	LIST_REMOVE(unsp, uns_list);
	free(unsp, M_TEMP);
}

/*
 * Create upper node attr.
 */
void
unionfs_create_uppervattr_core(struct unionfs_mount *ump, struct vattr *lva,
    struct vattr *uva, struct thread *td)
{
	VATTR_NULL(uva);
	uva->va_type = lva->va_type;
	uva->va_atime = lva->va_atime;
	uva->va_mtime = lva->va_mtime;
	uva->va_ctime = lva->va_ctime;

	switch (ump->um_copymode) {
	case UNIONFS_TRANSPARENT:
		uva->va_mode = lva->va_mode;
		uva->va_uid = lva->va_uid;
		uva->va_gid = lva->va_gid;
		break;
	case UNIONFS_MASQUERADE:
		if (ump->um_uid == lva->va_uid) {
			uva->va_mode = lva->va_mode & 077077;
			uva->va_mode |= (lva->va_type == VDIR ?
			    ump->um_udir : ump->um_ufile) & 0700;
			uva->va_uid = lva->va_uid;
			uva->va_gid = lva->va_gid;
		} else {
			uva->va_mode = (lva->va_type == VDIR ?
			    ump->um_udir : ump->um_ufile);
			uva->va_uid = ump->um_uid;
			uva->va_gid = ump->um_gid;
		}
		break;
	default:		/* UNIONFS_TRADITIONAL */
		uva->va_mode = 0777 & ~td->td_proc->p_pd->pd_cmask;
		uva->va_uid = ump->um_uid;
		uva->va_gid = ump->um_gid;
		break;
	}
}

/*
 * Create upper node attr.
 */
int
unionfs_create_uppervattr(struct unionfs_mount *ump, struct vnode *lvp,
    struct vattr *uva, struct ucred *cred, struct thread *td)
{
	struct vattr	lva;
	int		error;

	if ((error = VOP_GETATTR(lvp, &lva, cred)))
		return (error);

	unionfs_create_uppervattr_core(ump, &lva, uva, td);

	return (error);
}

/*
 * relookup
 * 
 * dvp should be locked on entry and will be locked on return.
 * 
 * If an error is returned, *vpp will be invalid, otherwise it will hold a
 * locked, referenced vnode. If *vpp == dvp then remember that only one
 * LK_EXCLUSIVE lock is held.
 */
int
unionfs_relookup(struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, struct componentname *cn, struct thread *td,
    char *path, int pathlen, u_long nameiop)
{
	int error;
	bool refstart;

	cn->cn_namelen = pathlen;
	cn->cn_pnbuf = path;
	cn->cn_nameiop = nameiop;
	cn->cn_flags = (LOCKPARENT | LOCKLEAF | ISLASTCN);
	cn->cn_lkflags = LK_EXCLUSIVE;
	cn->cn_cred = cnp->cn_cred;
	cn->cn_nameptr = cn->cn_pnbuf;

	refstart = false;
	if (nameiop == DELETE) {
		cn->cn_flags |= (cnp->cn_flags & DOWHITEOUT);
	} else if (nameiop == RENAME) {
		refstart = true;
	} else if (nameiop == CREATE) {
		cn->cn_flags |= NOCACHE;
	}

	vref(dvp);
	VOP_UNLOCK(dvp);

	if ((error = vfs_relookup(dvp, vpp, cn, refstart))) {
		vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
	} else
		vrele(dvp);

	KASSERT(cn->cn_pnbuf == path, ("%s: cn_pnbuf changed", __func__));

	return (error);
}

/*
 * relookup for CREATE namei operation.
 *
 * dvp is unionfs vnode. dvp should be locked.
 *
 * If it called 'unionfs_copyfile' function by unionfs_link etc,
 * VOP_LOOKUP information is broken.
 * So it need relookup in order to create link etc.
 */
int
unionfs_relookup_for_create(struct vnode *dvp, struct componentname *cnp,
    struct thread *td)
{
	struct vnode *udvp;
	struct vnode *vp;
	struct componentname cn;
	int error;

	udvp = UNIONFSVPTOUPPERVP(dvp);
	vp = NULLVP;

	error = unionfs_relookup(udvp, &vp, cnp, &cn, td, cnp->cn_nameptr,
	    cnp->cn_namelen, CREATE);
	if (error)
		return (error);

	if (vp != NULLVP) {
		if (udvp == vp)
			vrele(vp);
		else
			vput(vp);

		error = EEXIST;
	}

	return (error);
}

/*
 * relookup for DELETE namei operation.
 *
 * dvp is unionfs vnode. dvp should be locked.
 */
int
unionfs_relookup_for_delete(struct vnode *dvp, struct componentname *cnp,
    struct thread *td)
{
	struct vnode *udvp;
	struct vnode *vp;
	struct componentname cn;
	int error;

	udvp = UNIONFSVPTOUPPERVP(dvp);
	vp = NULLVP;

	error = unionfs_relookup(udvp, &vp, cnp, &cn, td, cnp->cn_nameptr,
	    cnp->cn_namelen, DELETE);
	if (error)
		return (error);

	if (vp == NULLVP)
		error = ENOENT;
	else {
		if (udvp == vp)
			vrele(vp);
		else
			vput(vp);
	}

	return (error);
}

/*
 * relookup for RENAME namei operation.
 *
 * dvp is unionfs vnode. dvp should be locked.
 */
int
unionfs_relookup_for_rename(struct vnode *dvp, struct componentname *cnp,
    struct thread *td)
{
	struct vnode *udvp;
	struct vnode *vp;
	struct componentname cn;
	int error;

	udvp = UNIONFSVPTOUPPERVP(dvp);
	vp = NULLVP;

	error = unionfs_relookup(udvp, &vp, cnp, &cn, td, cnp->cn_nameptr,
	    cnp->cn_namelen, RENAME);
	if (error)
		return (error);

	if (vp != NULLVP) {
		if (udvp == vp)
			vrele(vp);
		else
			vput(vp);
	}

	return (error);
}

/*
 * Update the unionfs_node.
 * 
 * uvp is new locked upper vnode. unionfs vnode's lock will be exchanged to the
 * uvp's lock and lower's lock will be unlocked.
 */
static void
unionfs_node_update(struct unionfs_node *unp, struct vnode *uvp,
    struct thread *td)
{
	struct unionfs_node_hashhead *hd;
	struct vnode   *vp;
	struct vnode   *lvp;
	struct vnode   *dvp;
	unsigned	count, lockrec;

	vp = UNIONFSTOV(unp);
	lvp = unp->un_lowervp;
	ASSERT_VOP_ELOCKED(lvp, __func__);
	ASSERT_VOP_ELOCKED(uvp, __func__);
	dvp = unp->un_dvp;

	VNASSERT(vp->v_writecount == 0, vp,
	    ("%s: non-zero writecount", __func__));
	/*
	 * Update the upper vnode's lock state to match the lower vnode,
	 * and then switch the unionfs vnode's lock to the upper vnode.
	 */
	lockrec = lvp->v_vnlock->lk_recurse;
	for (count = 0; count < lockrec; count++)
		vn_lock(uvp, LK_EXCLUSIVE | LK_CANRECURSE | LK_RETRY);
	VI_LOCK(vp);
	unp->un_uppervp = uvp;
	vp->v_vnlock = uvp->v_vnlock;
	VI_UNLOCK(vp);

	/*
	 * Re-cache the unionfs vnode against the upper vnode
	 */
	if (dvp != NULLVP && vp->v_type == VDIR) {
		VI_LOCK(dvp);
		if (unp->un_hash.le_prev != NULL) {
			LIST_REMOVE(unp, un_hash);
			hd = unionfs_get_hashhead(dvp, uvp);
			LIST_INSERT_HEAD(hd, unp, un_hash);
		}
		VI_UNLOCK(unp->un_dvp);
	}
}

/*
 * Create a new shadow dir.
 * 
 * udvp should be locked on entry and will be locked on return.
 * 
 * If no error returned, unp will be updated.
 */
int
unionfs_mkshadowdir(struct unionfs_mount *ump, struct vnode *udvp,
    struct unionfs_node *unp, struct componentname *cnp, struct thread *td)
{
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct vattr	va;
	struct vattr	lva;
	struct nameidata nd;
	struct mount   *mp;
	struct ucred   *cred;
	struct ucred   *credbk;
	struct uidinfo *rootinfo;
	int		error;

	if (unp->un_uppervp != NULLVP)
		return (EEXIST);

	lvp = unp->un_lowervp;
	uvp = NULLVP;
	credbk = cnp->cn_cred;

	/* Authority change to root */
	rootinfo = uifind((uid_t)0);
	cred = crdup(cnp->cn_cred);
	/*
	 * The calls to chgproccnt() are needed to compensate for change_ruid()
	 * calling chgproccnt().
	 */
	chgproccnt(cred->cr_ruidinfo, 1, 0);
	change_euid(cred, rootinfo);
	change_ruid(cred, rootinfo);
	change_svuid(cred, (uid_t)0);
	uifree(rootinfo);
	cnp->cn_cred = cred;

	memset(&nd.ni_cnd, 0, sizeof(struct componentname));
	NDPREINIT(&nd);

	if ((error = VOP_GETATTR(lvp, &lva, cnp->cn_cred)))
		goto unionfs_mkshadowdir_abort;

	if ((error = unionfs_relookup(udvp, &uvp, cnp, &nd.ni_cnd, td,
	    cnp->cn_nameptr, cnp->cn_namelen, CREATE)))
		goto unionfs_mkshadowdir_abort;
	if (uvp != NULLVP) {
		if (udvp == uvp)
			vrele(uvp);
		else
			vput(uvp);

		error = EEXIST;
		goto unionfs_mkshadowdir_abort;
	}

	if ((error = vn_start_write(udvp, &mp, V_WAIT | V_PCATCH)))
		goto unionfs_mkshadowdir_abort;
	unionfs_create_uppervattr_core(ump, &lva, &va, td);

	/*
	 * Temporarily NUL-terminate the current pathname component.
	 * This function may be called during lookup operations in which
	 * the current pathname component is not the leaf, meaning that
	 * the NUL terminator is some distance beyond the end of the current
	 * component.  This *should* be fine, as cn_namelen will still
	 * correctly indicate the length of only the current component,
	 * but ZFS in particular does not respect cn_namelen in its VOP_MKDIR
	 * implementation
	 * Note that this assumes nd.ni_cnd.cn_pnbuf was allocated by
	 * something like a local namei() operation and the temporary
	 * NUL-termination will not have an effect on other threads.
	 */
	char *pathend = &nd.ni_cnd.cn_nameptr[nd.ni_cnd.cn_namelen];
	char pathterm = *pathend;
	*pathend = '\0';
	error = VOP_MKDIR(udvp, &uvp, &nd.ni_cnd, &va);
	*pathend = pathterm;

	if (!error) {
		/*
		 * XXX The bug which cannot set uid/gid was corrected.
		 * Ignore errors.
		 */
		va.va_type = VNON;
		VOP_SETATTR(uvp, &va, nd.ni_cnd.cn_cred);

		/*
		 * VOP_SETATTR() may transiently drop uvp's lock, so it's
		 * important to call it before unionfs_node_update() transfers
		 * the unionfs vnode's lock from lvp to uvp; otherwise the
		 * unionfs vnode itself would be transiently unlocked and
		 * potentially doomed.
		 */
		unionfs_node_update(unp, uvp, td);
	}
	vn_finished_write(mp);

unionfs_mkshadowdir_abort:
	cnp->cn_cred = credbk;
	chgproccnt(cred->cr_ruidinfo, -1, 0);
	crfree(cred);

	return (error);
}

static inline void
unionfs_forward_vop_ref(struct vnode *basevp, int *lkflags)
{
	ASSERT_VOP_LOCKED(basevp, __func__);
	*lkflags = VOP_ISLOCKED(basevp);
	vref(basevp);
}

/*
 * Prepare unionfs to issue a forwarded VOP to either the upper or lower
 * FS.  This should be used for any VOP which may drop the vnode lock;
 * it is not required otherwise.
 * The unionfs vnode shares its lock with the base-layer vnode(s); if the
 * base FS must transiently drop its vnode lock, the unionfs vnode may
 * effectively become unlocked.  During that window, a concurrent forced
 * unmount may doom the unionfs vnode, which leads to two significant
 * issues:
 * 1) Completion of, and return from, the unionfs VOP with the unionfs
 *    vnode completely unlocked.  When the unionfs vnode becomes doomed
 *    it stops sharing its lock with the base vnode, so even if the
 *    forwarded VOP reacquires the base vnode lock the unionfs vnode
 *    lock will no longer be held.  This can lead to violation of the
 *    caller's sychronization requirements as well as various failed
 *    locking assertions when DEBUG_VFS_LOCKS is enabled.
 * 2) Loss of reference on the base vnode.  The caller is expected to
 *    hold a v_usecount reference on the unionfs vnode, while the
 *    unionfs vnode holds a reference on the base-layer vnode(s).  But
 *    these references are released when the unionfs vnode becomes
 *    doomed, violating the base layer's expectation that its caller
 *    must hold a reference to prevent vnode recycling.
 *
 * basevp1 and basevp2 represent two base-layer vnodes which are
 * expected to be locked when this function is called.  basevp2
 * may be NULL, but if not NULL basevp1 and basevp2 should represent
 * a parent directory and a filed linked to it, respectively.
 * lkflags1 and lkflags2 are output parameters that will store the
 * current lock status of basevp1 and basevp2, respectively.  They
 * are intended to be passed as the lkflags1 and lkflags2 parameters
 * in the subsequent call to unionfs_forward_vop_finish_pair().
 * lkflags2 may be NULL iff basevp2 is NULL.
 */
void
unionfs_forward_vop_start_pair(struct vnode *basevp1, int *lkflags1,
    struct vnode *basevp2, int *lkflags2)
{
	/*
	 * Take an additional reference on the base-layer vnodes to
	 * avoid loss of reference if the unionfs vnodes are doomed.
	 */
	unionfs_forward_vop_ref(basevp1, lkflags1);
	if (basevp2 != NULL)
		unionfs_forward_vop_ref(basevp2, lkflags2);
}

static inline bool
unionfs_forward_vop_rele(struct vnode *unionvp, struct vnode *basevp,
    int lkflags)
{
	bool unionvp_doomed;

	if (__predict_false(VTOUNIONFS(unionvp) == NULL)) {
		if ((lkflags & LK_EXCLUSIVE) != 0)
			ASSERT_VOP_ELOCKED(basevp, __func__);
		else
			ASSERT_VOP_LOCKED(basevp, __func__);
		unionvp_doomed = true;
	} else {
		vrele(basevp);
		unionvp_doomed = false;
	}

	return (unionvp_doomed);
}


/*
 * Indicate completion of a forwarded VOP previously prepared by
 * unionfs_forward_vop_start_pair().
 * basevp1 and basevp2 must be the same values passed to the prior
 * call to unionfs_forward_vop_start_pair().  unionvp1 and unionvp2
 * must be the unionfs vnodes that were initially above basevp1 and
 * basevp2, respectively.
 * basevp1 and basevp2 (if not NULL) must be locked when this function
 * is called, while unionvp1 and/or unionvp2 may be unlocked if either
 * unionfs vnode has become doomed.
 * lkflags1 and lkflag2 represent the locking flags that should be
 * used to re-lock unionvp1 and unionvp2, respectively, if either
 * vnode has become doomed.
 *
 * Returns true if any unionfs vnode was found to be doomed, false
 * otherwise.
 */
bool
unionfs_forward_vop_finish_pair(
    struct vnode *unionvp1, struct vnode *basevp1, int lkflags1,
    struct vnode *unionvp2, struct vnode *basevp2, int lkflags2)
{
	bool vp1_doomed, vp2_doomed;

	/*
	 * If either vnode is found to have been doomed, set
	 * a flag indicating that it needs to be re-locked.
	 * Otherwise, simply drop the base-vnode reference that
	 * was taken in unionfs_forward_vop_start().
	 */
	vp1_doomed = unionfs_forward_vop_rele(unionvp1, basevp1, lkflags1);

	if (unionvp2 != NULL)
		vp2_doomed = unionfs_forward_vop_rele(unionvp2, basevp2, lkflags2);
	else
		vp2_doomed = false;

	/*
	 * If any of the unionfs vnodes need to be re-locked, that
	 * means the unionfs vnode's lock is now de-coupled from the
	 * corresponding base vnode.  We therefore need to drop the
	 * base vnode lock (since nothing else will after this point),
	 * and also release the reference taken in
	 * unionfs_forward_vop_start_pair().
	 */
	if (__predict_false(vp1_doomed && vp2_doomed))
		VOP_VPUT_PAIR(basevp1, &basevp2, true);
	else if (__predict_false(vp1_doomed)) {
		/*
		 * If basevp1 needs to be unlocked, then we may not
		 * be able to safely unlock it with basevp2 still locked,
		 * for the same reason that an ordinary VFS call would
		 * need to use VOP_VPUT_PAIR() here.  We might be able
		 * to use VOP_VPUT_PAIR(..., false) here, but then we
		 * would need to deal with the possibility of basevp2
		 * changing out from under us, which could result in
		 * either the unionfs vnode becoming doomed or its
		 * upper/lower vp no longer matching basevp2.  Either
		 * scenario would require at least re-locking the unionfs
		 * vnode anyway.
		 */
		if (unionvp2 != NULL) {
			VOP_UNLOCK(unionvp2);
			vp2_doomed = true;
		}
		vput(basevp1);
	} else if (__predict_false(vp2_doomed))
		vput(basevp2);

	if (__predict_false(vp1_doomed || vp2_doomed))
		vn_lock_pair(unionvp1, !vp1_doomed, lkflags1,
		    unionvp2, !vp2_doomed, lkflags2);

	return (vp1_doomed || vp2_doomed);
}

/*
 * Create a new whiteout.
 * 
 * udvp and dvp should be locked on entry and will be locked on return.
 */
int
unionfs_mkwhiteout(struct vnode *dvp, struct vnode *udvp,
    struct componentname *cnp, struct thread *td, char *path, int pathlen)
{
	struct vnode   *wvp;
	struct nameidata nd;
	struct mount   *mp;
	int		error;
	int		lkflags;

	wvp = NULLVP;
	NDPREINIT(&nd);
	if ((error = unionfs_relookup(udvp, &wvp, cnp, &nd.ni_cnd, td, path,
	    pathlen, CREATE))) {
		return (error);
	}
	if (wvp != NULLVP) {
		if (udvp == wvp)
			vrele(wvp);
		else
			vput(wvp);

		return (EEXIST);
	}

	if ((error = vn_start_write(udvp, &mp, V_WAIT | V_PCATCH)))
		goto unionfs_mkwhiteout_free_out;
	unionfs_forward_vop_start(udvp, &lkflags);
	error = VOP_WHITEOUT(udvp, &nd.ni_cnd, CREATE);
	unionfs_forward_vop_finish(dvp, udvp, lkflags);

	vn_finished_write(mp);

unionfs_mkwhiteout_free_out:
	return (error);
}

/*
 * Create a new vnode for create a new shadow file.
 * 
 * If an error is returned, *vpp will be invalid, otherwise it will hold a
 * locked, referenced and opened vnode.
 * 
 * unp is never updated.
 */
static int
unionfs_vn_create_on_upper(struct vnode **vpp, struct vnode *udvp,
    struct unionfs_node *unp, struct vattr *uvap, struct thread *td)
{
	struct unionfs_mount *ump;
	struct vnode   *vp;
	struct vnode   *lvp;
	struct ucred   *cred;
	struct vattr	lva;
	struct nameidata nd;
	int		fmode;
	int		error;

	ump = MOUNTTOUNIONFSMOUNT(UNIONFSTOV(unp)->v_mount);
	vp = NULLVP;
	lvp = unp->un_lowervp;
	cred = td->td_ucred;
	fmode = FFLAGS(O_WRONLY | O_CREAT | O_TRUNC | O_EXCL);
	error = 0;

	if ((error = VOP_GETATTR(lvp, &lva, cred)) != 0)
		return (error);
	unionfs_create_uppervattr_core(ump, &lva, uvap, td);

	if (unp->un_path == NULL)
		panic("%s: NULL un_path", __func__);

	nd.ni_cnd.cn_namelen = unp->un_pathlen;
	nd.ni_cnd.cn_pnbuf = unp->un_path;
	nd.ni_cnd.cn_nameiop = CREATE;
	nd.ni_cnd.cn_flags = LOCKPARENT | LOCKLEAF | ISLASTCN;
	nd.ni_cnd.cn_lkflags = LK_EXCLUSIVE;
	nd.ni_cnd.cn_cred = cred;
	nd.ni_cnd.cn_nameptr = nd.ni_cnd.cn_pnbuf;
	NDPREINIT(&nd);

	vref(udvp);
	if ((error = vfs_relookup(udvp, &vp, &nd.ni_cnd, false)) != 0)
		goto unionfs_vn_create_on_upper_free_out2;
	vrele(udvp);

	if (vp != NULLVP) {
		if (vp == udvp)
			vrele(vp);
		else
			vput(vp);
		error = EEXIST;
		goto unionfs_vn_create_on_upper_free_out1;
	}

	if ((error = VOP_CREATE(udvp, &vp, &nd.ni_cnd, uvap)) != 0)
		goto unionfs_vn_create_on_upper_free_out1;

	if ((error = VOP_OPEN(vp, fmode, cred, td, NULL)) != 0) {
		vput(vp);
		goto unionfs_vn_create_on_upper_free_out1;
	}
	error = VOP_ADD_WRITECOUNT(vp, 1);
	CTR3(KTR_VFS, "%s: vp %p v_writecount increased to %d",
	    __func__, vp, vp->v_writecount);
	if (error == 0) {
		*vpp = vp;
	} else {
		VOP_CLOSE(vp, fmode, cred, td);
	}

unionfs_vn_create_on_upper_free_out1:
	VOP_UNLOCK(udvp);

unionfs_vn_create_on_upper_free_out2:
	KASSERT(nd.ni_cnd.cn_pnbuf == unp->un_path,
	    ("%s: cn_pnbuf changed", __func__));

	return (error);
}

/*
 * Copy from lvp to uvp.
 * 
 * lvp and uvp should be locked and opened on entry and will be locked and
 * opened on return.
 */
static int
unionfs_copyfile_core(struct vnode *lvp, struct vnode *uvp,
    struct ucred *cred, struct thread *td)
{
	char           *buf;
	struct uio	uio;
	struct iovec	iov;
	off_t		offset;
	int		count;
	int		error;
	int		bufoffset;

	error = 0;
	memset(&uio, 0, sizeof(uio));

	uio.uio_td = td;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_offset = 0;

	buf = malloc(MAXBSIZE, M_TEMP, M_WAITOK);

	while (error == 0) {
		offset = uio.uio_offset;

		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		iov.iov_base = buf;
		iov.iov_len = MAXBSIZE;
		uio.uio_resid = iov.iov_len;
		uio.uio_rw = UIO_READ;

		if ((error = VOP_READ(lvp, &uio, 0, cred)) != 0)
			break;
		if ((count = MAXBSIZE - uio.uio_resid) == 0)
			break;

		bufoffset = 0;
		while (bufoffset < count) {
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			iov.iov_base = buf + bufoffset;
			iov.iov_len = count - bufoffset;
			uio.uio_offset = offset + bufoffset;
			uio.uio_resid = iov.iov_len;
			uio.uio_rw = UIO_WRITE;

			if ((error = VOP_WRITE(uvp, &uio, 0, cred)) != 0)
				break;

			bufoffset += (count - bufoffset) - uio.uio_resid;
		}

		uio.uio_offset = offset + bufoffset;
	}

	free(buf, M_TEMP);

	return (error);
}

/*
 * Copy file from lower to upper.
 * 
 * If you need copy of the contents, set 1 to docopy. Otherwise, set 0 to
 * docopy.
 * 
 * If no error returned, unp will be updated.
 */
int
unionfs_copyfile(struct unionfs_node *unp, int docopy, struct ucred *cred,
    struct thread *td)
{
	struct mount   *mp;
	struct vnode   *udvp;
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct vattr	uva;
	int		error;

	lvp = unp->un_lowervp;
	uvp = NULLVP;

	if ((UNIONFSTOV(unp)->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (unp->un_dvp == NULLVP)
		return (EINVAL);
	if (unp->un_uppervp != NULLVP)
		return (EEXIST);
	udvp = VTOUNIONFS(unp->un_dvp)->un_uppervp;
	if (udvp == NULLVP)
		return (EROFS);
	if ((udvp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);

	error = VOP_ACCESS(lvp, VREAD, cred, td);
	if (error != 0)
		return (error);

	if ((error = vn_start_write(udvp, &mp, V_WAIT | V_PCATCH)) != 0)
		return (error);
	error = unionfs_vn_create_on_upper(&uvp, udvp, unp, &uva, td);
	if (error != 0) {
		vn_finished_write(mp);
		return (error);
	}

	if (docopy != 0) {
		error = VOP_OPEN(lvp, FREAD, cred, td, NULL);
		if (error == 0) {
			error = unionfs_copyfile_core(lvp, uvp, cred, td);
			VOP_CLOSE(lvp, FREAD, cred, td);
		}
	}
	VOP_CLOSE(uvp, FWRITE, cred, td);
	VOP_ADD_WRITECOUNT_CHECKED(uvp, -1);
	CTR3(KTR_VFS, "%s: vp %p v_writecount decreased to %d",
	    __func__, uvp, uvp->v_writecount);

	vn_finished_write(mp);

	if (error == 0) {
		/* Reset the attributes. Ignore errors. */
		uva.va_type = VNON;
		VOP_SETATTR(uvp, &uva, cred);
	}

	unionfs_node_update(unp, uvp, td);

	return (error);
}

/*
 * It checks whether vp can rmdir. (check empty)
 *
 * vp is unionfs vnode.
 * vp should be locked.
 */
int
unionfs_check_rmdir(struct vnode *vp, struct ucred *cred, struct thread *td)
{
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *tvp;
	struct dirent  *dp;
	struct dirent  *edp;
	struct componentname cn;
	struct iovec	iov;
	struct uio	uio;
	struct vattr	va;
	int		error;
	int		eofflag;
	int		lookuperr;

	/*
	 * The size of buf needs to be larger than DIRBLKSIZ.
	 */
	char		buf[256 * 6];

	ASSERT_VOP_ELOCKED(vp, __func__);

	eofflag = 0;
	uvp = UNIONFSVPTOUPPERVP(vp);
	lvp = UNIONFSVPTOLOWERVP(vp);

	/* check opaque */
	if ((error = VOP_GETATTR(uvp, &va, cred)) != 0)
		return (error);
	if (va.va_flags & OPAQUE)
		return (0);

	/* open vnode */
#ifdef MAC
	if ((error = mac_vnode_check_open(cred, vp, VEXEC|VREAD)) != 0)
		return (error);
#endif
	if ((error = VOP_ACCESS(vp, VEXEC|VREAD, cred, td)) != 0)
		return (error);
	if ((error = VOP_OPEN(vp, FREAD, cred, td, NULL)) != 0)
		return (error);

	uio.uio_rw = UIO_READ;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_td = td;
	uio.uio_offset = 0;

#ifdef MAC
	error = mac_vnode_check_readdir(td->td_ucred, lvp);
#endif
	while (!error && !eofflag) {
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_resid = iov.iov_len;

		error = VOP_READDIR(lvp, &uio, cred, &eofflag, NULL, NULL);
		if (error != 0)
			break;
		KASSERT(eofflag != 0 || uio.uio_resid < sizeof(buf),
		    ("%s: empty read from lower FS", __func__));

		edp = (struct dirent*)&buf[sizeof(buf) - uio.uio_resid];
		for (dp = (struct dirent*)buf; !error && dp < edp;
		     dp = (struct dirent*)((caddr_t)dp + dp->d_reclen)) {
			if (dp->d_type == DT_WHT || dp->d_fileno == 0 ||
			    (dp->d_namlen == 1 && dp->d_name[0] == '.') ||
			    (dp->d_namlen == 2 && !bcmp(dp->d_name, "..", 2)))
				continue;

			cn.cn_namelen = dp->d_namlen;
			cn.cn_pnbuf = NULL;
			cn.cn_nameptr = dp->d_name;
			cn.cn_nameiop = LOOKUP;
			cn.cn_flags = LOCKPARENT | LOCKLEAF | RDONLY | ISLASTCN;
			cn.cn_lkflags = LK_EXCLUSIVE;
			cn.cn_cred = cred;

			/*
			 * check entry in lower.
			 * Sometimes, readdir function returns
			 * wrong entry.
			 */
			lookuperr = VOP_LOOKUP(lvp, &tvp, &cn);

			if (!lookuperr)
				vput(tvp);
			else
				continue; /* skip entry */

			/*
			 * check entry
			 * If it has no exist/whiteout entry in upper,
			 * directory is not empty.
			 */
			cn.cn_flags = LOCKPARENT | LOCKLEAF | RDONLY | ISLASTCN;
			lookuperr = VOP_LOOKUP(uvp, &tvp, &cn);

			if (!lookuperr)
				vput(tvp);

			/* ignore exist or whiteout entry */
			if (!lookuperr ||
			    (lookuperr == ENOENT && (cn.cn_flags & ISWHITEOUT)))
				continue;

			error = ENOTEMPTY;
		}
	}

	/* close vnode */
	VOP_CLOSE(vp, FREAD, cred, td);

	return (error);
}

