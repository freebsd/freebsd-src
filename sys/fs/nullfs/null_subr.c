/*
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
 *	@(#)null_subr.c	8.7 (Berkeley) 5/14/95
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <fs/nullfs/null.h>

#define LOG2_SIZEVNODE 7		/* log2(sizeof struct vnode) */
#define	NNULLNODECACHE 16

/*
 * Null layer cache:
 * Each cache entry holds a reference to the lower vnode
 * along with a pointer to the alias vnode.  When an
 * entry is added the lower vnode is VREF'd.  When the
 * alias is removed the lower vnode is vrele'd.
 */

#define	NULL_NHASH(vp) \
	(&null_node_hashtbl[(((uintptr_t)vp)>>LOG2_SIZEVNODE) & null_node_hash])

static LIST_HEAD(null_node_hashhead, null_node) *null_node_hashtbl;
static u_long null_node_hash;
struct mtx null_hashmtx;

static MALLOC_DEFINE(M_NULLFSHASH, "NULLFS hash", "NULLFS hash table");
MALLOC_DEFINE(M_NULLFSNODE, "NULLFS node", "NULLFS vnode private part");

static struct vnode * null_hashget(struct vnode *);
static struct vnode * null_hashins(struct null_node *);

/*
 * Initialise cache headers
 */
int
nullfs_init(vfsp)
	struct vfsconf *vfsp;
{

	NULLFSDEBUG("nullfs_init\n");		/* printed during system boot */
	null_node_hashtbl = hashinit(NNULLNODECACHE, M_NULLFSHASH, &null_node_hash);
	mtx_init(&null_hashmtx, "nullhs", NULL, MTX_DEF);
	return (0);
}

int
nullfs_uninit(vfsp)
	struct vfsconf *vfsp;
{

	mtx_destroy(&null_hashmtx);
	free(null_node_hashtbl, M_NULLFSHASH);
	return (0);
}

/*
 * Return a VREF'ed alias for lower vnode if already exists, else 0.
 * Lower vnode should be locked on entry and will be left locked on exit.
 */
static struct vnode *
null_hashget(lowervp)
	struct vnode *lowervp;
{
	struct thread *td = curthread;	/* XXX */
	struct null_node_hashhead *hd;
	struct null_node *a;
	struct vnode *vp;

	/*
	 * Find hash base, and then search the (two-way) linked
	 * list looking for a null_node structure which is referencing
	 * the lower vnode.  If found, the increment the null_node
	 * reference count (but NOT the lower vnode's VREF counter).
	 */
	hd = NULL_NHASH(lowervp);
loop:
	mtx_lock(&null_hashmtx);
	LIST_FOREACH(a, hd, null_hash) {
		if (a->null_lowervp == lowervp) {
			vp = NULLTOV(a);
			mtx_lock(&vp->v_interlock);
			mtx_unlock(&null_hashmtx);
			/*
			 * We need vget for the VXLOCK
			 * stuff, but we don't want to lock
			 * the lower node.
			 */
			if (vget(vp, LK_EXCLUSIVE | LK_THISLAYER | LK_INTERLOCK, td))
				goto loop;

			return (vp);
		}
	}
	mtx_unlock(&null_hashmtx);
	return (NULLVP);
}

/*
 * Act like null_hashget, but add passed null_node to hash if no existing
 * node found.
 */
static struct vnode *
null_hashins(xp)
	struct null_node *xp;
{
	struct thread *td = curthread;	/* XXX */
	struct null_node_hashhead *hd;
	struct null_node *oxp;
	struct vnode *ovp;

	hd = NULL_NHASH(xp->null_lowervp);
loop:
	mtx_lock(&null_hashmtx);
	LIST_FOREACH(oxp, hd, null_hash) {
		if (oxp->null_lowervp == xp->null_lowervp) {
			ovp = NULLTOV(oxp);
			mtx_lock(&ovp->v_interlock);
			mtx_unlock(&null_hashmtx);
			if (vget(ovp, LK_EXCLUSIVE | LK_THISLAYER | LK_INTERLOCK, td))
				goto loop;

			return (ovp);
		}
	}
	LIST_INSERT_HEAD(hd, xp, null_hash);
	mtx_unlock(&null_hashmtx);
	return (NULLVP);
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
null_nodeget(mp, lowervp, vpp)
	struct mount *mp;
	struct vnode *lowervp;
	struct vnode **vpp;
{
	struct thread *td = curthread;	/* XXX */
	struct null_node *xp;
	struct vnode *vp;
	int error;

	/* Lookup the hash firstly */
	*vpp = null_hashget(lowervp);
	if (*vpp != NULL) {
		vrele(lowervp);
		return (0);
	}

	/*
	 * We do not serialize vnode creation, instead we will check for
	 * duplicates later, when adding new vnode to hash.
	 *
	 * Note that duplicate can only appear in hash if the lowervp is
	 * locked LK_SHARED.
	 */

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(xp, struct null_node *, sizeof(struct null_node),
	    M_NULLFSNODE, M_WAITOK);

	error = getnewvnode("null", mp, null_vnodeop_p, &vp);
	if (error) {
		FREE(xp, M_NULLFSNODE);
		return (error);
	}

	xp->null_vnode = vp;
	xp->null_lowervp = lowervp;

	vp->v_type = lowervp->v_type;
	vp->v_data = xp;

	/* Though v_lock is inited by getnewvnode(), we want our own wmesg */
	lockinit(&vp->v_lock, PVFS, "nunode", VLKTIMEOUT, LK_NOPAUSE);

	/*
	 * From NetBSD:
	 * Now lock the new node. We rely on the fact that we were passed
	 * a locked vnode. If the lower node is exporting a struct lock
	 * (v_vnlock != NULL) then we just set the upper v_vnlock to the
	 * lower one, and both are now locked. If the lower node is exporting
	 * NULL, then we copy that up and manually lock the new vnode.
	 */

	vp->v_vnlock = lowervp->v_vnlock;
	error = VOP_LOCK(vp, LK_EXCLUSIVE | LK_THISLAYER, td);
	if (error)
		panic("null_nodeget: can't lock new vnode\n");

	/*
	 * Atomically insert our new node into the hash or vget existing 
	 * if someone else has beaten us to it.
	 */
	*vpp = null_hashins(xp);
	if (*vpp != NULL) {
		vrele(lowervp);
		VOP_UNLOCK(vp, LK_THISLAYER, td);
		vp->v_vnlock = NULL;
		xp->null_lowervp = NULL;
		vrele(vp);
		return (0);
	}

	/*
	 * XXX We take extra vref just to workaround UFS's XXX:
	 * UFS can vrele() vnode in VOP_CLOSE() in some cases. Luckily, this
	 * can only happen if v_usecount == 1. To workaround, we just don't
	 * let v_usecount be 1, it will be 2 or more.
	 */
	VREF(lowervp);

	*vpp = vp;

	return (0);
}

/*
 * Remove node from hash.
 */
void
null_hashrem(xp)
	struct null_node *xp;
{

	mtx_lock(&null_hashmtx);
	LIST_REMOVE(xp, null_hash);
	mtx_unlock(&null_hashmtx);
}

#ifdef DIAGNOSTIC
#include "opt_ddb.h"

#ifdef DDB
#define	null_checkvp_barrier	1
#else
#define	null_checkvp_barrier	0
#endif

struct vnode *
null_checkvp(vp, fil, lno)
	struct vnode *vp;
	char *fil;
	int lno;
{
	struct null_node *a = VTONULL(vp);
#ifdef notyet
	/*
	 * Can't do this check because vop_reclaim runs
	 * with a funny vop vector.
	 */
	if (vp->v_op != null_vnodeop_p) {
		printf ("null_checkvp: on non-null-node\n");
		while (null_checkvp_barrier) /*WAIT*/ ;
		panic("null_checkvp");
	};
#endif
	if (a->null_lowervp == NULLVP) {
		/* Should never happen */
		int i; u_long *p;
		printf("vp = %p, ZERO ptr\n", (void *)vp);
		for (p = (u_long *) a, i = 0; i < 8; i++)
			printf(" %lx", p[i]);
		printf("\n");
		/* wait for debugger */
		while (null_checkvp_barrier) /*WAIT*/ ;
		panic("null_checkvp");
	}
	if (vrefcnt(a->null_lowervp) < 1) {
		int i; u_long *p;
		printf("vp = %p, unref'ed lowervp\n", (void *)vp);
		for (p = (u_long *) a, i = 0; i < 8; i++)
			printf(" %lx", p[i]);
		printf("\n");
		/* wait for debugger */
		while (null_checkvp_barrier) /*WAIT*/ ;
		panic ("null with unref'ed lowervp");
	};
#ifdef notyet
	printf("null %x/%d -> %x/%d [%s, %d]\n",
	        NULLTOV(a), vrefcnt(NULLTOV(a)),
		a->null_lowervp, vrefcnt(a->null_lowervp),
		fil, lno);
#endif
	return a->null_lowervp;
}
#endif
