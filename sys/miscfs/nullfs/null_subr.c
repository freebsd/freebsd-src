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
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <miscfs/nullfs/null.h>

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
struct lock null_hashlock;

static MALLOC_DEFINE(M_NULLFSHASH, "NULLFS hash", "NULLFS hash table");
MALLOC_DEFINE(M_NULLFSNODE, "NULLFS node", "NULLFS vnode private part");

static int	null_node_alloc(struct mount *mp, struct vnode *lowervp,
				     struct vnode **vpp);
static struct vnode *
		null_node_find(struct mount *mp, struct vnode *lowervp);

/*
 * Initialise cache headers
 */
int
nullfs_init(vfsp)
	struct vfsconf *vfsp;
{

	NULLFSDEBUG("nullfs_init\n");		/* printed during system boot */
	null_node_hashtbl = hashinit(NNULLNODECACHE, M_NULLFSHASH, &null_node_hash);
	lockinit(&null_hashlock, PVFS, "nullhs", 0, 0);
	return (0);
}

int
nullfs_uninit(vfsp)
	struct vfsconf *vfsp;
{

        if (null_node_hashtbl) {
		lockdestroy(&null_hashlock);
		free(null_node_hashtbl, M_NULLFSHASH);
	}
	return (0);
}

/*
 * Return a VREF'ed alias for lower vnode if already exists, else 0.
 * Lower vnode should be locked on entry and will be left locked on exit.
 */
static struct vnode *
null_node_find(mp, lowervp)
	struct mount *mp;
	struct vnode *lowervp;
{
	struct proc *p = curproc;	/* XXX */
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
	lockmgr(&null_hashlock, LK_EXCLUSIVE, NULL, p);
	for (a = hd->lh_first; a != 0; a = a->null_hash.le_next) {
		if (a->null_lowervp == lowervp && NULLTOV(a)->v_mount == mp) {
			vp = NULLTOV(a);
			lockmgr(&null_hashlock, LK_RELEASE, NULL, p);
			/*
			 * We need vget for the VXLOCK
			 * stuff, but we don't want to lock
			 * the lower node.
			 */
			if (vget(vp, LK_EXCLUSIVE | LK_CANRECURSE, p)) {
				printf ("null_node_find: vget failed.\n");
				goto loop;
			};
			/*
			 * Now we got both vnodes locked, so release the
			 * lower one.
			 */
			VOP_UNLOCK(lowervp, 0, p);
			return (vp);
		}
	}
	lockmgr(&null_hashlock, LK_RELEASE, NULL, p);

	return NULLVP;
}


/*
 * Make a new null_node node.
 * Vp is the alias vnode, lofsvp is the lower vnode.
 * Maintain a reference to (lowervp).
 */
static int
null_node_alloc(mp, lowervp, vpp)
	struct mount *mp;
	struct vnode *lowervp;
	struct vnode **vpp;
{
	struct proc *p = curproc;	/* XXX */
	struct null_node_hashhead *hd;
	struct null_node *xp;
	struct vnode *othervp, *vp;
	int error;

	/*
	 * Do the MALLOC before the getnewvnode since doing so afterward
	 * might cause a bogus v_data pointer to get dereferenced
	 * elsewhere if MALLOC should block.
	 */
	MALLOC(xp, struct null_node *, sizeof(struct null_node),
	    M_NULLFSNODE, M_WAITOK);

	error = getnewvnode(VT_NULL, mp, null_vnodeop_p, vpp);
	if (error) {
		FREE(xp, M_NULLFSNODE);
		return (error);
	}
	vp = *vpp;

	vp->v_type = lowervp->v_type;
	xp->null_vnode = vp;
	vp->v_data = xp;
	xp->null_lowervp = lowervp;
	/*
	 * Before we insert our new node onto the hash chains,
	 * check to see if someone else has beaten us to it.
	 * (We could have slept in MALLOC.)
	 */
	othervp = null_node_find(mp, lowervp);
	if (othervp) {
		vp->v_data = NULL;
		FREE(xp, M_NULLFSNODE);
		vp->v_type = VBAD;	/* node is discarded */
		vrele(vp);
		*vpp = othervp;
		return 0;
	};

	/*
	 * From NetBSD:
	 * Now lock the new node. We rely on the fact that we were passed
	 * a locked vnode. If the lower node is exporting a struct lock
	 * (v_vnlock != NULL) then we just set the upper v_vnlock to the
	 * lower one, and both are now locked. If the lower node is exporting
	 * NULL, then we copy that up and manually lock the new vnode.
	 */

	lockmgr(&null_hashlock, LK_EXCLUSIVE, NULL, p);
	vp->v_vnlock = lowervp->v_vnlock;
	error = VOP_LOCK(vp, LK_EXCLUSIVE | LK_THISLAYER, p);
	if (error)
		panic("null_node_alloc: can't lock new vnode\n");

	VREF(lowervp);
	hd = NULL_NHASH(lowervp);
	LIST_INSERT_HEAD(hd, xp, null_hash);
	lockmgr(&null_hashlock, LK_RELEASE, NULL, p);
	return 0;
}


/*
 * Try to find an existing null_node vnode refering to the given underlying
 * vnode (which should be locked). If no vnode found, create a new null_node
 * vnode which contains a reference to the lower vnode.
 */
int
null_node_create(mp, lowervp, newvpp)
	struct mount *mp;
	struct vnode *lowervp;
	struct vnode **newvpp;
{
	struct vnode *aliasvp;

	aliasvp = null_node_find(mp, lowervp);
	if (aliasvp) {
		/*
		 * null_node_find has taken another reference
		 * to the alias vnode.
		 */
		 vrele(lowervp);
#ifdef NULLFS_DEBUG
		vprint("null_node_create: exists", aliasvp);
#endif
	} else {
		int error;

		/*
		 * Get new vnode.
		 */
		NULLFSDEBUG("null_node_create: create new alias vnode\n");

		/*
		 * Make new vnode reference the null_node.
		 */
		error = null_node_alloc(mp, lowervp, &aliasvp);
		if (error)
			return error;

		/*
		 * aliasvp is already VREF'd by getnewvnode()
		 */
	}

#ifdef DIAGNOSTIC
	if (lowervp->v_usecount < 1) {
		/* Should never happen... */
		vprint ("null_node_create: alias ", aliasvp);
		vprint ("null_node_create: lower ", lowervp);
		panic ("null_node_create: lower has 0 usecount.");
	};
#endif

#ifdef NULLFS_DEBUG
	vprint("null_node_create: alias", aliasvp);
	vprint("null_node_create: lower", lowervp);
#endif

	*newvpp = aliasvp;
	return (0);
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
	if (a->null_lowervp->v_usecount < 1) {
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
	        NULLTOV(a), NULLTOV(a)->v_usecount,
		a->null_lowervp, a->null_lowervp->v_usecount,
		fil, lno);
#endif
	return a->null_lowervp;
}
#endif
