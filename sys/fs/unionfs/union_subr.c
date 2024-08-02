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

	vp = NULLVP;
	VI_LOCK(dvp);
	if (uncp->un_uppervp != NULLVP) {
		ASSERT_VOP_ELOCKED(uncp->un_uppervp, __func__);
		KASSERT(uncp->un_uppervp->v_type == VDIR,
		    ("%s: v_type != VDIR", __func__));
		vp = unionfs_get_cached_vnode_locked(uncp->un_uppervp, dvp);
	} else if (uncp->un_lowervp != NULLVP) {
		ASSERT_VOP_ELOCKED(uncp->un_lowervp, __func__);
		KASSERT(uncp->un_lowervp->v_type == VDIR,
		    ("%s: v_type != VDIR", __func__));
		vp = unionfs_get_cached_vnode_locked(uncp->un_lowervp, dvp);
	}
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
	if (unp->un_uppervp != NULLVP) {
		vput(unp->un_uppervp);
		if (unp->un_lowervp != NULLVP)
			vrele(unp->un_lowervp);
	} else if (unp->un_lowervp != NULLVP)
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
		panic("%s: upper and lower are both null", __func__);

	vt = (uppervp != NULLVP ? uppervp->v_type : lowervp->v_type);

	/* If it has no ISLASTCN flag, path check is skipped. */
	if (cnp && !(cnp->cn_flags & ISLASTCN))
		path = NULL;

	/* check the cache */
	if (dvp != NULLVP && vt == VDIR) {
		vp = unionfs_get_cached_vnode(uppervp, lowervp, dvp);
		if (vp != NULLVP) {
			*vpp = vp;
			if (lkflags != 0)
				vn_lock(*vpp, lkflags | LK_RETRY);
			return (0);
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


	/*
	 * NOTE: There is still a possibility for cross-filesystem locking here.
	 * If dvp has an upper FS component and is locked, while the new vnode
	 * created here only has a lower-layer FS component, then we will end
	 * up taking a lower-FS lock while holding an upper-FS lock.
	 * That situation could be dealt with here using vn_lock_pair().
	 * However, that would only address one instance out of many in which
	 * a child vnode lock is taken while holding a lock on its parent
	 * directory. This is done in many places in common VFS code, as well as
	 * a few places within unionfs (which could lead to the same cross-FS
	 * locking issue if, for example, the upper FS is another nested unionfs
	 * instance).  Additionally, it is unclear under what circumstances this
	 * specific lock sequence (a directory on one FS followed by a child of
	 * its 'peer' directory on another FS) would present the practical
	 * possibility of deadlock due to some other agent on the system
	 * attempting to lock those two specific vnodes in the opposite order.
	 */
	if (uppervp != NULLVP)
		vn_lock(uppervp, LK_EXCLUSIVE | LK_RETRY);
	else
		vn_lock(lowervp, LK_EXCLUSIVE | LK_RETRY);
	error = insmntque1(vp, mp);
	if (error != 0) {
		unionfs_nodeget_cleanup(vp, unp);
		return (error);
	}
	/*
	 * lowervp and uppervp should only be doomed by a forced unmount of
	 * their respective filesystems, but that can only happen if the
	 * unionfs instance is first unmounted.  We also effectively hold the
	 * lock on the new unionfs vnode at this point.  Therefore, if a
	 * unionfs umount has not yet reached the point at which the above
	 * insmntque1() would fail, then its vflush() call will end up
	 * blocked on our vnode lock, effectively also preventing unmount
	 * of the underlying filesystems.
	 */
	VNASSERT(lowervp == NULLVP || !VN_IS_DOOMED(lowervp), vp,
	    ("%s: doomed lowervp %p", __func__, lowervp));
	VNASSERT(uppervp == NULLVP || !VN_IS_DOOMED(uppervp), vp,
	    ("%s: doomed lowervp %p", __func__, uppervp));

	vn_set_state(vp, VSTATE_CONSTRUCTED);

	if (dvp != NULLVP && vt == VDIR)
		*vpp = unionfs_ins_cached_vnode(unp, dvp);
	if (*vpp != NULLVP) {
		unionfs_nodeget_cleanup(vp, unp);
		if (lkflags != 0)
			vn_lock(*vpp, lkflags | LK_RETRY);
		return (0);
	} else
		*vpp = vp;

	if ((lkflags & LK_SHARED) != 0)
		vn_lock(vp, LK_DOWNGRADE);
	else if ((lkflags & LK_EXCLUSIVE) == 0)
		VOP_UNLOCK(vp);

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
	bool		unlock_lvp;

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

	unp = VTOUNIONFS(vp);
	VNASSERT(unp != NULL, vp, ("%s: already reclaimed", __func__));
	lvp = unp->un_lowervp;
	uvp = unp->un_uppervp;
	dvp = unp->un_dvp;
	unlock_lvp = (uvp == NULLVP);

	/*
	 * Lock the lower vnode in addition to the upper vnode lock in order
	 * to synchronize against any unionfs_lock() operation which may still
	 * hold the lower vnode lock.  We do not need to do this for the root
	 * vnode, as the root vnode should always have both upper and lower
	 * base vnodes for its entire lifecycled, so unionfs_lock() should
	 * never attempt to lock its lower vnode in the first place.
	 * Moreover, during unmount of a non-"below" unionfs mount, the lower
	 * root vnode will already be locked as it is the covered vnode.
	 */
	if (uvp != NULLVP && lvp != NULLVP && (vp->v_vflag & VV_ROOT) == 0) {
		vn_lock_pair(uvp, true, LK_EXCLUSIVE, lvp, false, LK_EXCLUSIVE);
		unlock_lvp = true;
	}

	if (lockmgr(&vp->v_lock, LK_EXCLUSIVE | LK_NOWAIT, NULL) != 0)
		panic("%s: failed to acquire lock for vnode lock", __func__);
	/*
	 * Use the interlock to protect the clearing of v_data to
	 * prevent faults in unionfs_lock().
	 */
	VI_LOCK(vp);
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
	if (uvp != NULLVP)
		vput(uvp);
	if (unlock_lvp)
		vput(lvp);
	else if (lvp != NULLVP)
		vrele(lvp);

	if (dvp != NULLVP)
		unionfs_rem_cached_vnode(unp, dvp);

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
 * Find the unionfs node status object for the vnode corresponding to unp,
 * for the process that owns td.  Return NULL if no such object exists.
 */
struct unionfs_node_status *
unionfs_find_node_status(struct unionfs_node *unp, struct thread *td)
{
	struct unionfs_node_status *unsp;
	pid_t pid;

	pid = td->td_proc->p_pid;

	ASSERT_VOP_ELOCKED(UNIONFSTOV(unp), __func__);

	LIST_FOREACH(unsp, &(unp->un_unshead), uns_list) {
		if (unsp->uns_pid == pid) {
			return (unsp);
		}
	}

	return (NULL);
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
	unsp = unionfs_find_node_status(unp, td);
	if (unsp == NULL) {
		/* create a new unionfs node status */
		unsp = malloc(sizeof(struct unionfs_node_status),
		    M_TEMP, M_WAITOK | M_ZERO);

		unsp->uns_pid = pid;
		LIST_INSERT_HEAD(&(unp->un_unshead), unsp, uns_list);
	}

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

	for (count = 0; count < lockrec + 1; count++)
		VOP_UNLOCK(lvp);
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
 * Mark a unionfs operation as being in progress, sleeping if the
 * same operation is already in progress.
 * This is useful, for example, during copy-up operations in which
 * we may drop the target vnode lock, but we want to avoid the
 * possibility of a concurrent copy-up on the same vnode triggering
 * a spurious failure.
 */
int
unionfs_set_in_progress_flag(struct vnode *vp, unsigned int flag)
{
	struct unionfs_node *unp;
	int error;

	error = 0;
	ASSERT_VOP_ELOCKED(vp, __func__);
	VI_LOCK(vp);
	unp = VTOUNIONFS(vp);
	while (error == 0 && (unp->un_flag & flag) != 0) {
		VOP_UNLOCK(vp);
		error = msleep(vp, VI_MTX(vp), PCATCH | PDROP, "unioncp", 0);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		VI_LOCK(vp);
		if (error == 0) {
			/*
			 * If we waited on a concurrent copy-up and that
			 * copy-up was successful, return a non-fatal
			 * indication that the desired operation is already
			 * complete.  If we waited on a concurrent lookup,
			 * return ERELOOKUP to indicate the VFS cache should
			 * be re-queried to avoid creating a duplicate unionfs
			 * vnode.
			 */
			unp = VTOUNIONFS(vp);
			if (unp == NULL)
				error = ENOENT;
			else if (flag == UNIONFS_COPY_IN_PROGRESS &&
			    unp->un_uppervp != NULLVP)
				error = EJUSTRETURN;
			else if (flag == UNIONFS_LOOKUP_IN_PROGRESS)
				error = ERELOOKUP;
		}
	}
	if (error == 0)
		unp->un_flag |= flag;
	VI_UNLOCK(vp);

	return (error);
}

void
unionfs_clear_in_progress_flag(struct vnode *vp, unsigned int flag)
{
	struct unionfs_node *unp;

	ASSERT_VOP_ELOCKED(vp, __func__);
	unp = VTOUNIONFS(vp);
	VI_LOCK(vp);
	if (unp != NULL) {
		VNASSERT((unp->un_flag & flag) != 0, vp,
		    ("%s: copy not in progress", __func__));
		unp->un_flag &= ~flag;
	}
	wakeup(vp);
	VI_UNLOCK(vp);
}

/*
 * Create a new shadow dir.
 * 
 * dvp and vp are unionfs vnodes representing a parent directory and
 * child file, should be locked on entry, and will be locked on return.
 * 
 * If no error returned, unp will be updated.
 */
int
unionfs_mkshadowdir(struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp, struct thread *td)
{
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct vnode   *udvp;
	struct vattr	va;
	struct vattr	lva;
	struct nameidata nd;
	struct mount   *mp;
	struct ucred   *cred;
	struct ucred   *credbk;
	struct uidinfo *rootinfo;
	struct unionfs_mount *ump;
	struct unionfs_node *dunp;
	struct unionfs_node *unp;
	int		error;

	ASSERT_VOP_ELOCKED(dvp, __func__);
	ASSERT_VOP_ELOCKED(vp, __func__);
	ump = MOUNTTOUNIONFSMOUNT(vp->v_mount);
	unp = VTOUNIONFS(vp);
	if (unp->un_uppervp != NULLVP)
		return (EEXIST);
	dunp = VTOUNIONFS(dvp);
	udvp = dunp->un_uppervp;

	error = unionfs_set_in_progress_flag(vp, UNIONFS_COPY_IN_PROGRESS);
	if (error == EJUSTRETURN)
		return (0);
	else if (error != 0)
		return (error);

	lvp = unp->un_lowervp;
	uvp = NULLVP;
	credbk = cnp->cn_cred;

	/* Authority change to root */
	rootinfo = uifind((uid_t)0);
	cred = crdup(cnp->cn_cred);
	change_euid(cred, rootinfo);
	change_ruid(cred, rootinfo);
	change_svuid(cred, (uid_t)0);
	uifree(rootinfo);
	cnp->cn_cred = cred;

	memset(&nd.ni_cnd, 0, sizeof(struct componentname));
	NDPREINIT(&nd);

	if ((error = VOP_GETATTR(lvp, &lva, cnp->cn_cred)))
		goto unionfs_mkshadowdir_finish;

	vref(udvp);
	VOP_UNLOCK(vp);
	if ((error = unionfs_relookup(udvp, &uvp, cnp, &nd.ni_cnd, td,
	    cnp->cn_nameptr, cnp->cn_namelen, CREATE))) {
		/*
		 * When handling error cases here, we drop udvp's lock and
		 * then jump to exit code that relocks dvp, which in most
		 * cases will effectively relock udvp.  However, this is
		 * not guaranteed to be the case, as various calls made
		 * here (such as unionfs_relookup() above and VOP_MKDIR()
		 * below) may unlock and then relock udvp, allowing dvp to
		 * be reclaimed in the meantime.  In such a situation dvp
		 * will no longer share its lock with udvp.  Since
		 * performance isn't a concern for these error cases, it
		 * makes more sense to reuse the common code that locks
		 * dvp on exit than to explicitly check for reclamation
		 * of dvp.
		 */
		vput(udvp);
		goto unionfs_mkshadowdir_relock;
	}
	if (uvp != NULLVP) {
		if (udvp == uvp)
			vrele(uvp);
		else
			vput(uvp);

		error = EEXIST;
		vput(udvp);
		goto unionfs_mkshadowdir_relock;
	}

	if ((error = vn_start_write(udvp, &mp, V_WAIT | V_PCATCH))) {
		vput(udvp);
		goto unionfs_mkshadowdir_relock;
	}
	unionfs_create_uppervattr_core(ump, &lva, &va, td);

	/*
	 * Temporarily NUL-terminate the current pathname component.
	 * This function may be called during lookup operations in which
	 * the current pathname component is not the leaf, meaning that
	 * the NUL terminator is some distance beyond the end of the current
	 * component.  This *should* be fine, as cn_namelen will still
	 * correctly indicate the length of only the current component,
	 * but ZFS in particular does not respect cn_namelen in its VOP_MKDIR
	 * implementation.
	 * Note that this assumes nd.ni_cnd.cn_pnbuf was allocated by
	 * something like a local namei() operation and the temporary
	 * NUL-termination will not have an effect on other threads.
	 */
	char *pathend = &nd.ni_cnd.cn_nameptr[nd.ni_cnd.cn_namelen];
	char pathterm = *pathend;
	*pathend = '\0';
	error = VOP_MKDIR(udvp, &uvp, &nd.ni_cnd, &va);
	*pathend = pathterm;
	if (error != 0) {
		/*
		 * See the comment after unionfs_relookup() above for an
		 * explanation of why we unlock udvp here only to relock
		 * dvp on exit.
		 */
		vput(udvp);
		vn_finished_write(mp);
		goto unionfs_mkshadowdir_relock;
	}

	/*
	 * XXX The bug which cannot set uid/gid was corrected.
	 * Ignore errors.
	 */
	va.va_type = VNON;
	/*
	 * VOP_SETATTR() may transiently drop uvp's lock, so it's
	 * important to call it before unionfs_node_update() transfers
	 * the unionfs vnode's lock from lvp to uvp; otherwise the
	 * unionfs vnode itself would be transiently unlocked and
	 * potentially doomed.
	 */
	VOP_SETATTR(uvp, &va, nd.ni_cnd.cn_cred);

	/*
	 * uvp may become doomed during VOP_VPUT_PAIR() if the implementation
	 * must temporarily drop uvp's lock.  However, since we hold a
	 * reference to uvp from the VOP_MKDIR() call above, this would require
	 * a forcible unmount of uvp's filesystem, which in turn can only
	 * happen if our unionfs instance is first forcibly unmounted.  We'll
	 * therefore catch this case in the NULL check of unp below.
	 */
	VOP_VPUT_PAIR(udvp, &uvp, false);
	vn_finished_write(mp);
	vn_lock_pair(vp, false, LK_EXCLUSIVE, uvp, true, LK_EXCLUSIVE);
	unp = VTOUNIONFS(vp);
	if (unp == NULL) {
		vput(uvp);
		error = ENOENT;
	} else
		unionfs_node_update(unp, uvp, td);
	VOP_UNLOCK(vp);

unionfs_mkshadowdir_relock:
	vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	if (error == 0 && (VN_IS_DOOMED(dvp) || VN_IS_DOOMED(vp)))
		error = ENOENT;

unionfs_mkshadowdir_finish:
	unionfs_clear_in_progress_flag(vp, UNIONFS_COPY_IN_PROGRESS);
	cnp->cn_cred = credbk;
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
 * dvp and vp are unionfs vnodes representing a parent directory and
 * child file, should be locked on entry, and will be locked on return.
 */
int
unionfs_mkwhiteout(struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp, struct thread *td, char *path, int pathlen)
{
	struct vnode   *udvp;
	struct vnode   *wvp;
	struct nameidata nd;
	struct mount   *mp;
	int		error;
	bool		dvp_locked;

	ASSERT_VOP_ELOCKED(dvp, __func__);
	ASSERT_VOP_ELOCKED(vp, __func__);

	udvp = VTOUNIONFS(dvp)->un_uppervp;
	wvp = NULLVP;
	NDPREINIT(&nd);
	vref(udvp);
	VOP_UNLOCK(vp);
	if ((error = unionfs_relookup(udvp, &wvp, cnp, &nd.ni_cnd, td, path,
	    pathlen, CREATE))) {
		goto unionfs_mkwhiteout_cleanup;
	}
	if (wvp != NULLVP) {
		if (udvp == wvp)
			vrele(wvp);
		else
			vput(wvp);

		if (nd.ni_cnd.cn_flags & ISWHITEOUT)
			error = 0;
		else
			error = EEXIST;
		goto unionfs_mkwhiteout_cleanup;
	}

	if ((error = vn_start_write(udvp, &mp, V_WAIT | V_PCATCH)))
		goto unionfs_mkwhiteout_cleanup;
	error = VOP_WHITEOUT(udvp, &nd.ni_cnd, CREATE);
	vn_finished_write(mp);

unionfs_mkwhiteout_cleanup:
	if (VTOUNIONFS(dvp) == NULL) {
		vput(udvp);
		dvp_locked = false;
	} else {
		vrele(udvp);
		dvp_locked = true;
	}
	vn_lock_pair(dvp, dvp_locked, LK_EXCLUSIVE, vp, false, LK_EXCLUSIVE);
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
    struct vnode *vp, struct vattr *uvap, struct thread *td)
{
	struct unionfs_mount *ump;
	struct unionfs_node *unp;
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct ucred   *cred;
	struct vattr	lva;
	struct nameidata nd;
	int		fmode;
	int		error;

	ASSERT_VOP_ELOCKED(vp, __func__);
	unp = VTOUNIONFS(vp);
	ump = MOUNTTOUNIONFSMOUNT(UNIONFSTOV(unp)->v_mount);
	uvp = NULLVP;
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
	VOP_UNLOCK(vp);
	if ((error = vfs_relookup(udvp, &uvp, &nd.ni_cnd, false)) != 0) {
		vrele(udvp);
		return (error);
	}

	if (uvp != NULLVP) {
		if (uvp == udvp)
			vrele(uvp);
		else
			vput(uvp);
		error = EEXIST;
		goto unionfs_vn_create_on_upper_cleanup;
	}

	if ((error = VOP_CREATE(udvp, &uvp, &nd.ni_cnd, uvap)) != 0)
		goto unionfs_vn_create_on_upper_cleanup;

	if ((error = VOP_OPEN(uvp, fmode, cred, td, NULL)) != 0) {
		vput(uvp);
		goto unionfs_vn_create_on_upper_cleanup;
	}
	error = VOP_ADD_WRITECOUNT(uvp, 1);
	CTR3(KTR_VFS, "%s: vp %p v_writecount increased to %d",
	    __func__, uvp, uvp->v_writecount);
	if (error == 0) {
		*vpp = uvp;
	} else {
		VOP_CLOSE(uvp, fmode, cred, td);
	}

unionfs_vn_create_on_upper_cleanup:
	vput(udvp);
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
 * vp is a unionfs vnode that should be locked on entry and will be
 * locked on return.
 * 
 * If no error returned, unp will be updated.
 */
int
unionfs_copyfile(struct vnode *vp, int docopy, struct ucred *cred,
    struct thread *td)
{
	struct unionfs_node *unp;
	struct unionfs_node *dunp;
	struct mount   *mp;
	struct vnode   *udvp;
	struct vnode   *lvp;
	struct vnode   *uvp;
	struct vattr	uva;
	int		error;

	ASSERT_VOP_ELOCKED(vp, __func__);
	unp = VTOUNIONFS(vp);
	lvp = unp->un_lowervp;
	uvp = NULLVP;

	if ((UNIONFSTOV(unp)->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (unp->un_dvp == NULLVP)
		return (EINVAL);
	if (unp->un_uppervp != NULLVP)
		return (EEXIST);

	udvp = NULLVP;
	VI_LOCK(unp->un_dvp);
	dunp = VTOUNIONFS(unp->un_dvp);
	if (dunp != NULL)
		udvp = dunp->un_uppervp;
	VI_UNLOCK(unp->un_dvp);

	if (udvp == NULLVP)
		return (EROFS);
	if ((udvp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	ASSERT_VOP_UNLOCKED(udvp, __func__);

	error = unionfs_set_in_progress_flag(vp, UNIONFS_COPY_IN_PROGRESS);
	if (error == EJUSTRETURN)
		return (0);
	else if (error != 0)
		return (error);

	error = VOP_ACCESS(lvp, VREAD, cred, td);
	if (error != 0)
		goto unionfs_copyfile_cleanup;

	if ((error = vn_start_write(udvp, &mp, V_WAIT | V_PCATCH)) != 0)
		goto unionfs_copyfile_cleanup;
	error = unionfs_vn_create_on_upper(&uvp, udvp, vp, &uva, td);
	if (error != 0) {
		vn_finished_write(mp);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
		goto unionfs_copyfile_cleanup;
	}

	/*
	 * Note that it's still possible for e.g. VOP_WRITE to relock
	 * uvp below while holding vp[=lvp] locked.  Replacing
	 * unionfs_copyfile_core with vn_generic_copy_file_range() will
	 * allow us to avoid the problem by moving this vn_lock_pair()
	 * call much later.
	 */
	vn_lock_pair(vp, false, LK_EXCLUSIVE, uvp, true, LK_EXCLUSIVE);
	unp = VTOUNIONFS(vp);
	if (unp == NULL) {
		error = ENOENT;
		goto unionfs_copyfile_cleanup;
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
		unionfs_node_update(unp, uvp, td);
	}

unionfs_copyfile_cleanup:
	unionfs_clear_in_progress_flag(vp, UNIONFS_COPY_IN_PROGRESS);
	return (error);
}

/*
 * Determine if the unionfs view of a directory is empty such that
 * an rmdir operation can be permitted.
 *
 * We assume the VOP_RMDIR() against the upper layer vnode will take
 * care of this check for us where the upper FS is concerned, so here
 * we concentrate on the lower FS.  We need to check for the presence
 * of files other than "." and ".." in the lower FS directory and
 * then cross-check any files we find against the upper FS to see if
 * a whiteout is present (in which case we treat the lower file as
 * non-present).
 *
 * The logic here is based heavily on vn_dir_check_empty().
 *
 * vp should be a locked unionfs node, and vp's lowervp should also be
 * locked.
 */
int
unionfs_check_rmdir(struct vnode *vp, struct ucred *cred, struct thread *td)
{
	struct vnode   *uvp;
	struct vnode   *lvp;
	struct vnode   *tvp;
	char *dirbuf;
	size_t dirbuflen, len;
	off_t off;
	struct dirent  *dp;
	struct componentname cn;
	struct vattr	va;
	int		error;
	int		eofflag;

	eofflag = 0;
	lvp = UNIONFSVPTOLOWERVP(vp);
	uvp = UNIONFSVPTOUPPERVP(vp);

	/*
	 * Note that the locking here still isn't ideal: We expect the caller
	 * to hold both the upper and lower layer locks as well as the upper
	 * parent directory lock, which it can do in a manner that avoids
	 * deadlock.  However, if the cross-check logic below needs to call
	 * VOP_LOOKUP(), that may relock the upper vnode and lock any found
	 * child vnode in a way that doesn't protect against deadlock given
	 * the other held locks.  Beyond that, the various other VOPs we issue
	 * below, such as VOP_OPEN() and VOP_READDIR(), may also re-lock the
	 * lower vnode.
	 * We might instead just handoff between the upper vnode lock
	 * (and its parent directory lock) and the lower vnode lock as needed,
	 * so that the lower lock is never held at the same time as the upper
	 * locks, but that opens up a wider window in which the upper
	 * directory (and also the lower directory if it isn't truly
	 * read-only) may change while the relevant lock is dropped.  But
	 * since re-locking may happen here and open up such a window anyway,
	 * perhaps that is a worthwile tradeoff?  Or perhaps we can ultimately
	 * do sufficient tracking of empty state within the unionfs vnode
	 * (in conjunction with upcalls from the lower FSes to notify us
	 * of out-of-band state changes) that we can avoid these costly checks
	 * altogether.
	 */
	ASSERT_VOP_LOCKED(lvp, __func__);
	ASSERT_VOP_ELOCKED(uvp, __func__);

	if ((error = VOP_GETATTR(uvp, &va, cred)) != 0)
		return (error);
	if (va.va_flags & OPAQUE)
		return (0);

#ifdef MAC
	if ((error = mac_vnode_check_open(cred, lvp, VEXEC | VREAD)) != 0)
		return (error);
#endif
	if ((error = VOP_ACCESS(lvp, VEXEC | VREAD, cred, td)) != 0)
		return (error);
	if ((error = VOP_OPEN(lvp, FREAD, cred, td, NULL)) != 0)
		return (error);
	if ((error = VOP_GETATTR(lvp, &va, cred)) != 0)
		return (error);

	dirbuflen = max(DEV_BSIZE, GENERIC_MAXDIRSIZ);
	if (dirbuflen < va.va_blocksize)
		dirbuflen = va.va_blocksize;
	dirbuf = malloc(dirbuflen, M_TEMP, M_WAITOK);

	len = 0;
	off = 0;
	eofflag = 0;

	for (;;) {
		error = vn_dir_next_dirent(lvp, td, dirbuf, dirbuflen,
		    &dp, &len, &off, &eofflag);
		if (error != 0)
			break;

		if (len == 0) {
			/* EOF */
			error = 0;
			break;
		}

		if (dp->d_type == DT_WHT)
			continue;

		/*
		 * Any file in the directory which is not '.' or '..' indicates
		 * the directory is not empty.
		 */
		switch (dp->d_namlen) {
		case 2:
			if (dp->d_name[1] != '.') {
				/* Can't be '..' (nor '.') */
				break;
			}
			/* FALLTHROUGH */
		case 1:
			if (dp->d_name[0] != '.') {
				/* Can't be '..' nor '.' */
				break;
			}
			continue;
		default:
			break;
		}

		cn.cn_namelen = dp->d_namlen;
		cn.cn_pnbuf = NULL;
		cn.cn_nameptr = dp->d_name;
		cn.cn_nameiop = LOOKUP;
		cn.cn_flags = LOCKPARENT | LOCKLEAF | RDONLY | ISLASTCN;
		cn.cn_lkflags = LK_EXCLUSIVE;
		cn.cn_cred = cred;

		error = VOP_LOOKUP(uvp, &tvp, &cn);
		if (tvp != NULLVP)
			vput(tvp);
		if (error != 0 && error != ENOENT && error != EJUSTRETURN)
			break;
		else if ((cn.cn_flags & ISWHITEOUT) == 0) {
			error = ENOTEMPTY;
			break;
		} else
			error = 0;
	}

	VOP_CLOSE(lvp, FREAD, cred, td);
	free(dirbuf, M_TEMP);
	return (error);
}
