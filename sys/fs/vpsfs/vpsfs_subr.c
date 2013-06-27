/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)vpsfs_subr.c	8.7 (Berkeley) 5/14/95
 *
 * $FreeBSD: head/sys/fs/vpsfs/vpsfs_subr.c 250505 2013-05-11 11:17:44Z kib $
 */

#ifdef VPS

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/taskqueue.h>

#include <vps/vps_account.h>

#include <fs/vpsfs/vpsfs.h>

/*
 * Null layer cache:
 * Each cache entry holds a reference to the lower vnode
 * along with a pointer to the alias vnode.  When an
 * entry is added the lower vnode is VREF'd.  When the
 * alias is removed the lower vnode is vrele'd.
 */

#define	VPSFS_NHASH(vp) \
    (&vpsfs_node_hashtbl[vfs_hash_index(vp) & vpsfs_hash_mask])

static LIST_HEAD(vpsfs_node_hashhead, vpsfs_node) *vpsfs_node_hashtbl;
static struct mtx vpsfs_hashmtx;
static u_long vpsfs_hash_mask;

static MALLOC_DEFINE(M_VPSFSHASH, "vpsfs_hash", "VPSFS hash table");
MALLOC_DEFINE(M_VPSFSNODE, "vpsfs_node", "VPSFS vnode private part");

static struct vnode * vpsfs_hashins(struct mount *, struct vpsfs_node *);

static const char *vpsfs_tag = "vpsfs";

/*
 * Initialise cache headers
 */
int
vpsfs_init(vfsp)
	struct vfsconf *vfsp;
{

	vpsfs_node_hashtbl = hashinit(desiredvnodes, M_VPSFSHASH,
	    &vpsfs_hash_mask);
	mtx_init(&vpsfs_hashmtx, "vpsfshs", NULL, MTX_DEF);

	vps_func->vpsfs_calcusage_path = vpsfs_calcusage_path;
	vps_func->vpsfs_nodeget = vpsfs_nodeget;
	vps_func->vpsfs_tag = vpsfs_tag;

	return (0);
}

int
vpsfs_uninit(vfsp)
	struct vfsconf *vfsp;
{

	vps_func->vpsfs_calcusage_path = NULL;
	vps_func->vpsfs_nodeget = NULL;
	vps_func->vpsfs_tag = NULL;

	mtx_destroy(&vpsfs_hashmtx);
	hashdestroy(vpsfs_node_hashtbl, M_VPSFSHASH, vpsfs_hash_mask);
	return (0);
}

/*
 * Return a VREF'ed alias for lower vnode if already exists, else 0.
 * Lower vnode should be locked on entry and will be left locked on exit.
 */
struct vnode *
vpsfs_hashget(mp, lowervp)
	struct mount *mp;
	struct vnode *lowervp;
{
	struct vpsfs_node_hashhead *hd;
	struct vpsfs_node *a;
	struct vnode *vp;

	ASSERT_VOP_LOCKED(lowervp, "vpsfs_hashget");

	/*
	 * Find hash base, and then search the (two-way) linked
	 * list looking for a vpsfs_node structure which is referencing
	 * the lower vnode.  If found, the increment the vpsfs_node
	 * reference count (but NOT the lower vnode's VREF counter).
	 */
	hd = VPSFS_NHASH(lowervp);
	mtx_lock(&vpsfs_hashmtx);
	LIST_FOREACH(a, hd, vpsfs_hash) {
		if (a->vpsfs_lowervp == lowervp &&
		    VPSFSTOV(a)->v_mount == mp) {
			/*
			 * Since we have the lower node locked the vpsfs
			 * node can not be in the process of recycling.  If
			 * it had been recycled before we grabed the lower
			 * lock it would not have been found on the hash.
			 */
			vp = VPSFSTOV(a);
			vref(vp);
			mtx_unlock(&vpsfs_hashmtx);
			return (vp);
		}
	}
	mtx_unlock(&vpsfs_hashmtx);
	return (NULLVP);
}

/*
 * Act like vpsfs_hashget, but add passed vpsfs_node to hash if no existing
 * node found.
 */
static struct vnode *
vpsfs_hashins(mp, xp)
	struct mount *mp;
	struct vpsfs_node *xp;
{
	struct vpsfs_node_hashhead *hd;
	struct vpsfs_node *oxp;
	struct vnode *ovp;

	hd = VPSFS_NHASH(xp->vpsfs_lowervp);
	mtx_lock(&vpsfs_hashmtx);
	LIST_FOREACH(oxp, hd, vpsfs_hash) {
		if (oxp->vpsfs_lowervp == xp->vpsfs_lowervp &&
		    VPSFSTOV(oxp)->v_mount == mp) {
			/*
			 * See vpsfs_hashget for a description of this
			 * operation.
			 */
			ovp = VPSFSTOV(oxp);
			vref(ovp);
			mtx_unlock(&vpsfs_hashmtx);
			return (ovp);
		}
	}
	LIST_INSERT_HEAD(hd, xp, vpsfs_hash);
	mtx_unlock(&vpsfs_hashmtx);
	return (NULLVP);
}

static void
vpsfs_destroy_proto(struct vnode *vp, void *xp)
{

	lockmgr(&vp->v_lock, LK_EXCLUSIVE, NULL);
	VI_LOCK(vp);
	vp->v_data = NULL;
	vp->v_vnlock = &vp->v_lock;
	vp->v_op = &dead_vnodeops;
	VI_UNLOCK(vp);
	vgone(vp);
	vput(vp);
	free(xp, M_VPSFSNODE);
}

static void
vpsfs_insmntque_dtr(struct vnode *vp, void *xp)
{

	vput(((struct vpsfs_node *)xp)->vpsfs_lowervp);
	vpsfs_destroy_proto(vp, xp);
}

/*
 * Make a new or get existing vpsfs node.
 * Vp is the alias vnode, lowervp is the lower vnode.
 * 
 * The lowervp assumed to be locked and having "spare" reference.
 * This routine * vrele lowervp if vpsfs node was taken from hash.
 * Otherwise it "transfers" * the caller's "spare" reference to
 * created vpsfs vnode.
 */
int
vpsfs_nodeget(mp, lowervp, vpp)
	struct mount *mp;
	struct vnode *lowervp;
	struct vnode **vpp;
{
	struct vpsfs_node *xp;
	struct vnode *vp;
	int error;

	ASSERT_VOP_LOCKED(lowervp, "lowervp");
	KASSERT(lowervp->v_usecount >= 1,
	    ("Unreferenced vnode %p", lowervp));

	/* Lookup the hash firstly. */
	*vpp = vpsfs_hashget(mp, lowervp);
	if (*vpp != NULL) {
		vrele(lowervp);
		return (0);
	}

	/*
	 * The insmntque1() call below requires the exclusive lock on
	 * the vpsfs vnode.  Upgrade the lock now if hash failed to
	 * provide ready to use vnode.
	 */
	if (VOP_ISLOCKED(lowervp) != LK_EXCLUSIVE) {
		KASSERT((MOUNTTOVPSFSMOUNT(mp)->vpsfsm_flags &
		    VPSFSM_CACHE) != 0,
		    ("lowervp %p is not excl locked and cache is disabled",
		    lowervp));
		vn_lock(lowervp, LK_UPGRADE | LK_RETRY);
		if ((lowervp->v_iflag & VI_DOOMED) != 0) {
			vput(lowervp);
			return (ENOENT);
		}
	}

	/*
	 * We do not serialize vnode creation, instead we will check for
	 * duplicates later, when adding new vnode to hash.
	 * Note that duplicate can only appear in hash if the lowervp is
	 * locked LK_SHARED.
	 */
	xp = malloc(sizeof(struct vpsfs_node), M_VPSFSNODE, M_WAITOK);

	error = getnewvnode(vpsfs_tag, mp, &vpsfs_vnodeops, &vp);
	if (error) {
		vput(lowervp);
		free(xp, M_VPSFSNODE);
		return (error);
	}

	xp->vpsfs_vnode = vp;
	xp->vpsfs_lowervp = lowervp;
	xp->vpsfs_flags = 0;
	vp->v_type = lowervp->v_type;
	vp->v_data = xp;
	vp->v_vnlock = lowervp->v_vnlock;
	error = insmntque1(vp, mp, vpsfs_insmntque_dtr, xp);
	if (error != 0)
		return (error);
	/*
	 * Atomically insert our new node into the hash or vget existing 
	 * if someone else has beaten us to it.
	 */
	*vpp = vpsfs_hashins(mp, xp);
	if (*vpp != NULL) {
		vrele(lowervp);
		vpsfs_destroy_proto(vp, xp);
		return (0);
	}
	*vpp = vp;

	return (0);
}

/*
 * Remove node from hash.
 */
void
vpsfs_hashrem(xp)
	struct vpsfs_node *xp;
{

	mtx_lock(&vpsfs_hashmtx);
	LIST_REMOVE(xp, vpsfs_hash);
	mtx_unlock(&vpsfs_hashmtx);
}

#ifdef DIAGNOSTIC

struct vnode *
vpsfs_checkvp(vp, fil, lno)
	struct vnode *vp;
	char *fil;
	int lno;
{
	struct vpsfs_node *a = VTOVPSFS(vp);

#ifdef notyet
	/*
	 * Can't do this check because vop_reclaim runs
	 * with a funny vop vector.
	 */
	if (vp->v_op != vpsfs_vnodeop_p) {
		printf ("vpsfs_checkvp: on non-vpsfs-node\n");
		panic("vpsfs_checkvp");
	}
#endif
	if (a->vpsfs_lowervp == NULLVP) {
		/* Should never happen */
		panic("vpsfs_checkvp %p", vp);
	}
	VI_LOCK_FLAGS(a->vpsfs_lowervp, MTX_DUPOK);
	if (a->vpsfs_lowervp->v_usecount < 1)
		panic ("vpsfs with unref'ed lowervp, vp %p lvp %p",
		    vp, a->vpsfs_lowervp);
	VI_UNLOCK(a->vpsfs_lowervp);
#ifdef notyet
	printf("vpsfs %x/%d -> %x/%d [%s, %d]\n",
		VPSFSTOV(a), vrefcnt(VPSFSTOV(a)),
		a->vpsfs_lowervp, vrefcnt(a->vpsfs_lowervp),
		fil, lno);
#endif
	return (a->vpsfs_lowervp);
}
#endif

#endif /* VPS */

