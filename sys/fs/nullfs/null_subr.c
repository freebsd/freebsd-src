/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <fs/nullfs/null.h>

/*
 * Null layer cache:
 * Each cache entry holds a reference to the lower vnode
 * along with a pointer to the alias vnode.  When an
 * entry is added the lower vnode is VREF'd.  When the
 * alias is removed the lower vnode is vrele'd.
 */

#define	NULL_NHASH(vp) (&null_node_hashtbl[vfs_hash_index(vp) & null_hash_mask])

static LIST_HEAD(null_node_hashhead, null_node) *null_node_hashtbl;
static struct rwlock null_hash_lock;
static u_long null_hash_mask;

static MALLOC_DEFINE(M_NULLFSHASH, "nullfs_hash", "NULLFS hash table");
MALLOC_DEFINE(M_NULLFSNODE, "nullfs_node", "NULLFS vnode private part");

static void null_hashins(struct mount *, struct null_node *);

/*
 * Initialise cache headers
 */
int
nullfs_init(struct vfsconf *vfsp)
{

	null_node_hashtbl = hashinit(desiredvnodes, M_NULLFSHASH,
	    &null_hash_mask);
	rw_init(&null_hash_lock, "nullhs");
	return (0);
}

int
nullfs_uninit(struct vfsconf *vfsp)
{

	rw_destroy(&null_hash_lock);
	hashdestroy(null_node_hashtbl, M_NULLFSHASH, null_hash_mask);
	return (0);
}

/*
 * Return a VREF'ed alias for lower vnode if already exists, else 0.
 * Lower vnode should be locked on entry and will be left locked on exit.
 */
static struct vnode *
null_hashget_locked(struct mount *mp, struct vnode *lowervp)
{
	struct null_node_hashhead *hd;
	struct null_node *a;
	struct vnode *vp;

	ASSERT_VOP_LOCKED(lowervp, "null_hashget");
	rw_assert(&null_hash_lock, RA_LOCKED);

	/*
	 * Find hash base, and then search the (two-way) linked
	 * list looking for a null_node structure which is referencing
	 * the lower vnode.  If found, the increment the null_node
	 * reference count (but NOT the lower vnode's VREF counter).
	 */
	hd = NULL_NHASH(lowervp);
	LIST_FOREACH(a, hd, null_hash) {
		if (a->null_lowervp == lowervp && NULLTOV(a)->v_mount == mp) {
			/*
			 * Since we have the lower node locked the nullfs
			 * node can not be in the process of recycling.  If
			 * it had been recycled before we grabed the lower
			 * lock it would not have been found on the hash.
			 */
			vp = NULLTOV(a);
			vref(vp);
			return (vp);
		}
	}
	return (NULLVP);
}

struct vnode *
null_hashget(struct mount *mp, struct vnode *lowervp)
{
	struct null_node_hashhead *hd;
	struct vnode *vp;

	hd = NULL_NHASH(lowervp);
	if (LIST_EMPTY(hd))
		return (NULLVP);

	rw_rlock(&null_hash_lock);
	vp = null_hashget_locked(mp, lowervp);
	rw_runlock(&null_hash_lock);

	return (vp);
}

static void
null_hashins(struct mount *mp, struct null_node *xp)
{
	struct null_node_hashhead *hd;
#ifdef INVARIANTS
	struct null_node *oxp;
#endif

	rw_assert(&null_hash_lock, RA_WLOCKED);

	hd = NULL_NHASH(xp->null_lowervp);
#ifdef INVARIANTS
	LIST_FOREACH(oxp, hd, null_hash) {
		if (oxp->null_lowervp == xp->null_lowervp &&
		    NULLTOV(oxp)->v_mount == mp) {
			VNASSERT(0, NULLTOV(oxp),
			    ("vnode already in hash"));
		}
	}
#endif
	LIST_INSERT_HEAD(hd, xp, null_hash);
}

static void
null_destroy_proto(struct vnode *vp, void *xp)
{

	lockmgr(&vp->v_lock, LK_EXCLUSIVE, NULL);
	VI_LOCK(vp);
	vp->v_data = NULL;
	vp->v_vnlock = &vp->v_lock;
	vp->v_op = &dead_vnodeops;
	VI_UNLOCK(vp);
	vgone(vp);
	vput(vp);
	free(xp, M_NULLFSNODE);
}

/*
 * Make a new or get existing nullfs node.
 * Vp is the alias vnode, lowervp is the lower vnode.
 * 
 * The lowervp assumed to be locked and having "spare" reference. This routine
 * vrele lowervp if nullfs node was taken from hash. Otherwise it "transfers"
 * the caller's "spare" reference to created nullfs vnode.
 */
int
null_nodeget(struct mount *mp, struct vnode *lowervp, struct vnode **vpp)
{
	struct null_node *xp;
	struct vnode *vp;
	int error;

	ASSERT_VOP_LOCKED(lowervp, "lowervp");
	VNPASS(lowervp->v_usecount > 0, lowervp);

	/* Lookup the hash firstly. */
	*vpp = null_hashget(mp, lowervp);
	if (*vpp != NULL) {
		vrele(lowervp);
		return (0);
	}

	/*
	 * We do not serialize vnode creation, instead we will check for
	 * duplicates later, when adding new vnode to hash.
	 * Note that duplicate can only appear in hash if the lowervp is
	 * locked LK_SHARED.
	 */
	xp = malloc(sizeof(struct null_node), M_NULLFSNODE, M_WAITOK);

	error = getnewvnode("nullfs", mp, &null_vnodeops, &vp);
	if (error) {
		vput(lowervp);
		free(xp, M_NULLFSNODE);
		return (error);
	}

	VNPASS(vp->v_object == NULL, vp);
	VNPASS((vn_irflag_read(vp) & VIRF_PGREAD) == 0, vp);

	rw_wlock(&null_hash_lock);
	xp->null_vnode = vp;
	xp->null_lowervp = lowervp;
	xp->null_flags = 0;
	vp->v_type = lowervp->v_type;
	vp->v_data = xp;
	vp->v_vnlock = lowervp->v_vnlock;
	*vpp = null_hashget_locked(mp, lowervp);
	if (*vpp != NULL) {
		rw_wunlock(&null_hash_lock);
		vrele(lowervp);
		null_destroy_proto(vp, xp);
		return (0);
	}

	/*
	 * We might miss the case where lower vnode sets VIRF_PGREAD
	 * some time after construction, which is typical case.
	 * null_open rechecks.
	 */
	if ((vn_irflag_read(lowervp) & VIRF_PGREAD) != 0) {
		MPASS(lowervp->v_object != NULL);
		vp->v_object = lowervp->v_object;
		vn_irflag_set(vp, VIRF_PGREAD);
	}
	if ((vn_irflag_read(lowervp) & VIRF_INOTIFY) != 0)
		vn_irflag_set(vp, VIRF_INOTIFY);
	if ((vn_irflag_read(lowervp) & VIRF_INOTIFY_PARENT) != 0)
		vn_irflag_set(vp, VIRF_INOTIFY_PARENT);
	if (lowervp == MOUNTTONULLMOUNT(mp)->nullm_lowerrootvp)
		vp->v_vflag |= VV_ROOT;

	error = insmntque1(vp, mp);
	if (error != 0) {
		rw_wunlock(&null_hash_lock);
		vput(lowervp);
		vp->v_object = NULL;
		null_destroy_proto(vp, xp);
		return (error);
	}

	null_hashins(mp, xp);
	vn_set_state(vp, VSTATE_CONSTRUCTED);
	rw_wunlock(&null_hash_lock);
	*vpp = vp;

	return (0);
}

/*
 * Remove node from hash.
 */
void
null_hashrem(struct null_node *xp)
{

	rw_wlock(&null_hash_lock);
	LIST_REMOVE(xp, null_hash);
	rw_wunlock(&null_hash_lock);
}

#ifdef DIAGNOSTIC

struct vnode *
null_checkvp(struct vnode *vp, char *fil, int lno)
{
	struct null_node *a = VTONULL(vp);

#ifdef notyet
	/*
	 * Can't do this check because vop_reclaim runs
	 * with a funny vop vector.
	 */
	if (vp->v_op != null_vnodeop_p) {
		printf ("null_checkvp: on non-null-node\n");
		panic("null_checkvp");
	}
#endif
	if (a->null_lowervp == NULLVP) {
		/* Should never happen */
		panic("null_checkvp %p", vp);
	}
	VI_LOCK_FLAGS(a->null_lowervp, MTX_DUPOK);
	if (a->null_lowervp->v_usecount < 1)
		panic ("null with unref'ed lowervp, vp %p lvp %p",
		    vp, a->null_lowervp);
	VI_UNLOCK(a->null_lowervp);
#ifdef notyet
	printf("null %x/%d -> %x/%d [%s, %d]\n",
	        NULLTOV(a), vrefcnt(NULLTOV(a)),
		a->null_lowervp, vrefcnt(a->null_lowervp),
		fil, lno);
#endif
	return (a->null_lowervp);
}
#endif
