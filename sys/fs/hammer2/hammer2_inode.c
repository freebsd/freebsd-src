/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/tree.h>
#include <sys/vnode.h>

#include "hammer2.h"

static void hammer2_inode_repoint(hammer2_inode_t *, hammer2_cluster_t *);
static void hammer2_inode_repoint_one(hammer2_inode_t *, hammer2_cluster_t *,
    int);

static int
hammer2_inode_cmp(const hammer2_inode_t *ip1, const hammer2_inode_t *ip2)
{
	if (ip1->meta.inum < ip2->meta.inum)
		return (-1);
	if (ip1->meta.inum > ip2->meta.inum)
		return (1);
	return (0);
}

RB_GENERATE_STATIC(hammer2_inode_tree, hammer2_inode, rbnode,
    hammer2_inode_cmp);

/*
 * HAMMER2 offers shared and exclusive locks on inodes.
 * Pass a mask of flags for options:
 *	- pass HAMMER2_RESOLVE_SHARED if a shared lock is desired.
 *	- pass HAMMER2_RESOLVE_ALWAYS if you need the inode's meta-data.
 *	  Most front-end inode locks do.
 */
void
hammer2_inode_lock(hammer2_inode_t *ip, int how)
{
	hammer2_inode_ref(ip);

	if (how & HAMMER2_RESOLVE_SHARED)
		hammer2_mtx_sh(&ip->lock);
	else
		hammer2_mtx_ex(&ip->lock);
}

void
hammer2_inode_unlock(hammer2_inode_t *ip)
{
	hammer2_mtx_unlock(&ip->lock);
	hammer2_inode_drop(ip);
}

/*
 * Select a chain out of an inode's cluster and lock it.
 * The inode does not have to be locked.
 */
hammer2_chain_t *
hammer2_inode_chain(hammer2_inode_t *ip, int clindex, int how)
{
	hammer2_chain_t *chain;
	hammer2_cluster_t *cluster;

	hammer2_spin_sh(&ip->cluster_spin);
	cluster = &ip->cluster;
	if (clindex >= cluster->nchains)
		chain = NULL;
	else
		chain = cluster->array[clindex].chain;
	if (chain) {
		hammer2_chain_ref(chain);
		hammer2_spin_unsh(&ip->cluster_spin);
		hammer2_chain_lock(chain, how);
	} else {
		hammer2_spin_unsh(&ip->cluster_spin);
	}

	return (chain);
}

hammer2_chain_t *
hammer2_inode_chain_and_parent(hammer2_inode_t *ip, int clindex,
    hammer2_chain_t **parentp, int how)
{
	hammer2_chain_t *chain, *parent;

	for (;;) {
		hammer2_spin_sh(&ip->cluster_spin);
		if (clindex >= ip->cluster.nchains)
			chain = NULL;
		else
			chain = ip->cluster.array[clindex].chain;
		if (chain) {
			hammer2_chain_ref(chain);
			hammer2_spin_unsh(&ip->cluster_spin);
			hammer2_chain_lock(chain, how);
		} else {
			hammer2_spin_unsh(&ip->cluster_spin);
		}

		/* Get parent, lock order must be (parent, chain). */
		parent = chain->parent;
		if (parent) {
			hammer2_chain_ref(parent);
			hammer2_chain_unlock(chain);
			hammer2_chain_lock(parent, how);
			hammer2_chain_lock(chain, how);
		}
		if (ip->cluster.array[clindex].chain == chain &&
		    chain->parent == parent)
			break;

		/* Retry. */
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
		if (parent) {
			hammer2_chain_unlock(parent);
			hammer2_chain_drop(parent);
		}
	}
	*parentp = parent;

	return (chain);
}

/*
 * Lookup an inode by inode number.
 */
hammer2_inode_t *
hammer2_inode_lookup(hammer2_pfs_t *pmp, hammer2_tid_t inum)
{
	hammer2_inode_t *ip, find;

	KKASSERT(pmp);
	if (pmp->spmp_hmp) {
		ip = NULL;
	} else {
		hammer2_spin_ex(&pmp->inum_spin);
		bzero(&find, sizeof(find));
		find.meta.inum = inum;
		ip = RB_FIND(hammer2_inode_tree, &pmp->inum_tree, &find);
		if (ip)
			hammer2_inode_ref(ip);
		hammer2_spin_unex(&pmp->inum_spin);
	}

	return (ip);
}

/*
 * Adding a ref to an inode is only legal if the inode already has at least
 * one ref.
 * Can be called with spinlock held.
 */
void
hammer2_inode_ref(hammer2_inode_t *ip)
{
	atomic_add_int(&ip->refs, 1);
}

/*
 * Drop an inode reference, freeing the inode when the last reference goes
 * away.
 */
void
hammer2_inode_drop(hammer2_inode_t *ip)
{
	hammer2_pfs_t *pmp;
	unsigned int refs;

	while (ip) {
		refs = ip->refs;
		__compiler_membar();
		if (refs == 1) {
			/*
			 * Transition to zero, must interlock with
			 * the inode inumber lookup tree (if applicable).
			 * It should not be possible for anyone to race
			 * the transition to 0.
			 */
			pmp = ip->pmp;
			KKASSERT(pmp);
			hammer2_spin_ex(&pmp->inum_spin);

			if (atomic_cmpset_int(&ip->refs, 1, 0)) {
				if (ip->flags & HAMMER2_INODE_ONRBTREE) {
					atomic_clear_int(&ip->flags,
					    HAMMER2_INODE_ONRBTREE);
					RB_REMOVE(hammer2_inode_tree,
					    &pmp->inum_tree, ip);
				}
				hammer2_spin_unex(&pmp->inum_spin);
				ip->pmp = NULL;

				/*
				 * Cleaning out ip->cluster isn't entirely
				 * trivial.
				 */
				hammer2_inode_repoint(ip, NULL);
				hammer2_mtx_destroy(&ip->lock);
				hammer2_spin_destroy(&ip->cluster_spin);

				free(ip, M_HAMMER2);
				atomic_add_long(&hammer2_inode_allocs, -1);
				ip = NULL; /* Will terminate loop. */
			} else {
				hammer2_spin_unex(&ip->pmp->inum_spin);
			}
		} else {
			/* Non zero transition. */
			if (atomic_cmpset_int(&ip->refs, refs, refs - 1))
				break;
		}
	}
}

/*
 * Get the vnode associated with the given inode, allocating the vnode if
 * necessary.  The vnode will be returned exclusively locked.
 *
 * The caller must lock the inode (shared or exclusive).
 */
int
hammer2_igetv(hammer2_inode_t *ip, int flags, struct vnode **vpp)
{
	struct mount *mp;
	struct vnode *vp = NULL;
	struct thread *td = curthread;
	hammer2_tid_t inum;
	int error;

	hammer2_mtx_assert_locked(&ip->lock);

	KKASSERT(ip);
	KKASSERT(ip->pmp);
	KKASSERT(ip->pmp->mp);
	mp = ip->pmp->mp;
	inum = ip->meta.inum & HAMMER2_DIRHASH_USERMSK;

	error = vfs_hash_get(mp, inum, flags, td, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	error = getnewvnode("hammer2", mp, &hammer2_vnodeops, &vp);
	if (error) {
		*vpp = NULL;
		return (error);
	}
	KKASSERT(vp);

	lockmgr(vp->v_vnlock, LK_EXCLUSIVE, NULL);
	vp->v_data = ip;
	ip->vp = vp;
	hammer2_inode_ref(ip); /* vp association */

	error = insmntque(vp, mp);
	if (error) {
		*vpp = NULL;
		return (error);
	}

	error = vfs_hash_insert(vp, inum, flags, td, vpp, NULL, NULL);
	if (error || *vpp != NULL)
		return (error);

	KASSERT(ip->meta.mode, ("mode 0"));
	KASSERT(ip->meta.type, ("type 0"));
	vp->v_type = hammer2_get_vtype(ip->meta.type);
	KASSERT(vp->v_type != VBAD, ("VBAD"));
	KASSERT(vp->v_type != VNON, ("VNON"));

	if (vp->v_type == VFIFO)
		vp->v_op = &hammer2_fifoops;
	KASSERT(vp->v_op, ("NULL vnode ops"));

	if (inum == 1)
		vp->v_vflag |= VV_ROOT;

	*vpp = vp;
	return (0);
}

/*
 * Returns the inode associated with the arguments, allocating a new
 * hammer2_inode structure if necessary, then synchronizing it to the passed
 * xop cluster.  When synchronizing, if idx >= 0, only cluster index (idx)
 * is synchronized.  Otherwise the whole cluster is synchronized.  inum will
 * be extracted from the passed-in xop and the inum argument will be ignored.
 *
 * If xop is passed as NULL then a new hammer2_inode is allocated with the
 * specified inum, and returned.   For normal inodes, the inode will be
 * indexed in memory and if it already exists the existing ip will be
 * returned instead of allocating a new one.  The superroot and PFS inodes
 * are not indexed in memory.
 *
 * The returned inode will be locked and the caller may dispose of both
 * via hammer2_inode_unlock() + hammer2_inode_drop().
 *
 * The hammer2_inode structure regulates the interface between the high level
 * kernel VNOPS API and the filesystem backend (the chains).
 */
hammer2_inode_t *
hammer2_inode_get(hammer2_pfs_t *pmp, hammer2_xop_head_t *xop,
    hammer2_tid_t inum, int idx)
{
	hammer2_inode_t *nip;
	const hammer2_inode_data_t *iptmp, *nipdata;

	KKASSERT(xop == NULL ||
	    hammer2_cluster_type(&xop->cluster) == HAMMER2_BREF_TYPE_INODE);
	KKASSERT(pmp);

	if (xop) {
		iptmp = &hammer2_xop_gdata(xop)->ipdata;
		inum = iptmp->meta.inum;
		hammer2_xop_pdata(xop);
	}
again:
	nip = hammer2_inode_lookup(pmp, inum);
	if (nip) {
		/*
		 * We may have to unhold the cluster to avoid a deadlock
		 * against vnlru (and possibly other XOPs).
		 */
		if (xop) {
			if (hammer2_mtx_ex_try(&nip->lock) != 0) {
				hammer2_cluster_unhold(&xop->cluster);
				hammer2_mtx_ex(&nip->lock);
				hammer2_cluster_rehold(&xop->cluster);
			}
		} else {
			hammer2_mtx_ex(&nip->lock);
		}

		/*
		 * Handle SMP race (not applicable to the super-root spmp
		 * which can't index inodes due to duplicative inode numbers).
		 */
		if (pmp->spmp_hmp == NULL &&
		    (nip->flags & HAMMER2_INODE_ONRBTREE) == 0) {
			hammer2_mtx_unlock(&nip->lock);
			hammer2_inode_drop(nip);
			goto again;
		}
		if (xop) {
			if (idx >= 0)
				hammer2_inode_repoint_one(nip, &xop->cluster,
				    idx);
			else
				hammer2_inode_repoint(nip, &xop->cluster);
		}
		return (nip);
	}

	/*
	 * We couldn't find the inode number, create a new inode and try to
	 * insert it, handle insertion races.
	 */
	nip = malloc(sizeof(*nip), M_HAMMER2, M_WAITOK | M_ZERO);
	atomic_add_long(&hammer2_inode_allocs, 1);
	hammer2_spin_init(&nip->cluster_spin, "h2ip_clsp");

	nip->cluster.pmp = pmp;
	if (xop) {
		nipdata = &hammer2_xop_gdata(xop)->ipdata;
		nip->meta = nipdata->meta;
		hammer2_xop_pdata(xop);
		hammer2_inode_repoint(nip, &xop->cluster);
	} else {
		nip->meta.inum = inum;
	}

	nip->pmp = pmp;

	/*
	 * ref and lock on nip gives it state compatible to after a
	 * hammer2_inode_lock() call.
	 */
	nip->refs = 1;
	hammer2_mtx_init(&nip->lock, "h2ip_lk");
	hammer2_mtx_ex(&nip->lock);

	/*
	 * Attempt to add the inode.  If it fails we raced another inode
	 * get.  Undo all the work and try again.
	 */
	if (pmp->spmp_hmp == NULL) {
		hammer2_spin_ex(&pmp->inum_spin);
		if (RB_INSERT(hammer2_inode_tree, &pmp->inum_tree, nip)) {
			hammer2_spin_unex(&pmp->inum_spin);
			hammer2_mtx_unlock(&nip->lock);
			hammer2_inode_drop(nip);
			goto again;
		}
		atomic_set_int(&nip->flags, HAMMER2_INODE_ONRBTREE);
		hammer2_spin_unex(&pmp->inum_spin);
	}

	return (nip);
}

/*
 * Repoint ip->cluster's chains to cluster's chains and fixup the default
 * focus.  All items, valid or invalid, are repointed.
 *
 * Cluster may be NULL to clean out any chains in ip->cluster.
 */
static void
hammer2_inode_repoint(hammer2_inode_t *ip, hammer2_cluster_t *cluster)
{
	hammer2_chain_t *dropch[HAMMER2_MAXCLUSTER];
	hammer2_chain_t *ochain, *nchain;
	int i;

	bzero(dropch, sizeof(dropch));

	/*
	 * Replace chains in ip->cluster with chains from cluster and
	 * adjust the focus if necessary.
	 *
	 * NOTE: nchain and/or ochain can be NULL due to gaps
	 *	 in the cluster arrays.
	 */
	hammer2_spin_ex(&ip->cluster_spin);
	for (i = 0; cluster && i < cluster->nchains; ++i) {
		/* Do not replace elements which are the same. */
		nchain = cluster->array[i].chain;
		if (i < ip->cluster.nchains) {
			ochain = ip->cluster.array[i].chain;
			if (ochain == nchain)
				continue;
		} else {
			ochain = NULL;
		}

		/* Make adjustments. */
		ip->cluster.array[i].chain = nchain;
		if (nchain)
			hammer2_chain_ref(nchain);
		dropch[i] = ochain;
	}

	/* Release any left-over chains in ip->cluster. */
	while (i < ip->cluster.nchains) {
		nchain = ip->cluster.array[i].chain;
		if (nchain)
			ip->cluster.array[i].chain = NULL;
		dropch[i] = nchain;
		++i;
	}

	/*
	 * Fixup fields.  Note that the inode-embedded cluster is never
	 * directly locked.
	 */
	if (cluster) {
		ip->cluster.nchains = cluster->nchains;
		ip->cluster.focus = cluster->focus;
		hammer2_assert_cluster(&ip->cluster);
	} else {
		ip->cluster.nchains = 0;
		ip->cluster.focus = NULL;
	}

	hammer2_spin_unex(&ip->cluster_spin);

	/* Cleanup outside of spinlock. */
	while (--i >= 0)
		if (dropch[i])
			hammer2_chain_drop(dropch[i]);
}

/*
 * Repoint a single element from the cluster to the ip.  Does not change
 * focus and requires inode to be re-locked to clean-up flags.
 */
static void
hammer2_inode_repoint_one(hammer2_inode_t *ip, hammer2_cluster_t *cluster,
    int idx)
{
	hammer2_chain_t *ochain, *nchain;
	int i;

	hammer2_spin_ex(&ip->cluster_spin);
	KKASSERT(idx < cluster->nchains);
	if (idx < ip->cluster.nchains) {
		ochain = ip->cluster.array[idx].chain;
		nchain = cluster->array[idx].chain;
	} else {
		ochain = NULL;
		nchain = cluster->array[idx].chain;
		for (i = ip->cluster.nchains; i <= idx; ++i)
			bzero(&ip->cluster.array[i],
			    sizeof(ip->cluster.array[i]));
		ip->cluster.nchains = idx + 1;
		hammer2_assert_cluster(&ip->cluster);
	}
	if (ochain != nchain) {
		/* Make adjustments. */
		ip->cluster.array[idx].chain = nchain;
	}
	hammer2_spin_unex(&ip->cluster_spin);

	if (ochain != nchain) {
		if (nchain)
			hammer2_chain_ref(nchain);
		if (ochain)
			hammer2_chain_drop(ochain);
	}
}

hammer2_key_t
hammer2_inode_data_count(const hammer2_inode_t *ip)
{
	hammer2_chain_t *chain;
	hammer2_key_t count = 0;
	int i;

	for (i = 0; i < ip->cluster.nchains; ++i) {
		chain = ip->cluster.array[i].chain;
		if (chain == NULL)
			continue;
		if (count < chain->bref.embed.stats.data_count)
			count = chain->bref.embed.stats.data_count;
	}

	return (count);
}

hammer2_key_t
hammer2_inode_inode_count(const hammer2_inode_t *ip)
{
	hammer2_chain_t *chain;
	hammer2_key_t count = 0;
	int i;

	for (i = 0; i < ip->cluster.nchains; ++i) {
		chain = ip->cluster.array[i].chain;
		if (chain == NULL)
			continue;
		if (count < chain->bref.embed.stats.inode_count)
			count = chain->bref.embed.stats.inode_count;
	}

	return (count);
}
