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
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <vm/uma.h>

#include "hammer2.h"

#define H2XOPDESCRIPTOR(label)					\
	hammer2_xop_desc_t hammer2_##label##_desc = {		\
		.storage_func = hammer2_xop_##label,		\
		.id = #label					\
	}

H2XOPDESCRIPTOR(readdir);
H2XOPDESCRIPTOR(nresolve);
H2XOPDESCRIPTOR(lookup);
H2XOPDESCRIPTOR(bmap);
H2XOPDESCRIPTOR(strategy_read);

/*
 * Allocate or reallocate XOP FIFO.  This doesn't exist in DragonFly
 * where XOP is handled by dedicated kernel threads and when FIFO stalls
 * threads wait for frontend to collect results.
 */
static void
hammer2_xop_fifo_alloc(hammer2_xop_fifo_t *fifo, int fifo_size)
{
	struct malloc_type *type = M_HAMMER2;
	int flags = M_WAITOK | M_ZERO;
	size_t size;

	KKASSERT((fifo_size & (fifo_size - 1)) == 0);
	KKASSERT(fifo_size >= HAMMER2_XOPFIFO);
	KKASSERT(fifo_size <= INT_MAX);

	size = sizeof(hammer2_chain_t*) * fifo_size;
	if (!fifo->array)
		fifo->array = malloc(size, type, flags);
	else
		fifo->array = realloc(fifo->array, size, type, flags);
	KKASSERT(fifo->array);

	size = sizeof(int) * fifo_size;
	if (!fifo->errors)
		fifo->errors = malloc(size, type, flags);
	else
		fifo->errors = realloc(fifo->errors, size, type, flags);
	KKASSERT(fifo->errors);
}

/*
 * Allocate a XOP request.
 * Once allocated a XOP request can be started, collected, and retired,
 * and can be retired early if desired.
 */
void *
hammer2_xop_alloc(hammer2_inode_t *ip)
{
	hammer2_xop_t *xop;

	xop = uma_zalloc(zone_xops, M_WAITOK | M_ZERO);
	KKASSERT(xop->head.cluster.array[0].chain == NULL);

	xop->head.ip1 = ip;
	xop->head.cluster.nchains = ip->cluster.nchains;
	xop->head.cluster.pmp = ip->pmp;
	hammer2_assert_cluster(&ip->cluster);

	/* run_mask - Frontend associated with XOP. */
	xop->head.run_mask = HAMMER2_XOPMASK_VOP;

	hammer2_xop_fifo_t *fifo = &xop->head.collect[0];
	xop->head.fifo_size = HAMMER2_XOPFIFO;
	hammer2_xop_fifo_alloc(fifo, xop->head.fifo_size);

	hammer2_inode_ref(ip);

	return (xop);
}

void
hammer2_xop_setname(hammer2_xop_head_t *xop, const char *name, size_t name_len)
{
	xop->name1 = malloc(name_len + 1, M_HAMMER2, M_WAITOK | M_ZERO);
	xop->name1_len = name_len;
	bcopy(name, xop->name1, name_len);
}

/*
 * (Backend) Returns non-zero if the frontend is still attached.
 */
static __inline int
hammer2_xop_active(const hammer2_xop_head_t *xop)
{
	if (xop->run_mask & HAMMER2_XOPMASK_VOP)
		return (1);
	else
		return (0);
}

/*
 * hashinit(9) based hash to track inode dependencies.
 */
static __inline int
xop_ipdep_value(const hammer2_inode_t *ip)
{
	int idx;

	hammer2_mtx_assert_ex(&ip->pmp->xop_lock);

	KKASSERT(ip != NULL);
	idx = ip->meta.inum % HAMMER2_IHASH_SIZE;
	KKASSERT(idx >= 0 && idx < HAMMER2_IHASH_SIZE);

	return (idx);
}

static int
xop_testset_ipdep(hammer2_inode_t *ip)
{
	hammer2_ipdep_list_t *ipdep;
	hammer2_inode_t *iptmp;

	hammer2_mtx_assert_ex(&ip->pmp->xop_lock);

	ipdep = &ip->pmp->ipdep_lists[xop_ipdep_value(ip)];
	LIST_FOREACH(iptmp, ipdep, entry)
		if (iptmp == ip)
			return (1); /* collision */

	LIST_INSERT_HEAD(ipdep, ip, entry);
	return (0);
}

static void
xop_unset_ipdep(hammer2_inode_t *ip)
{
	hammer2_ipdep_list_t *ipdep;
	hammer2_inode_t *iptmp;

	hammer2_mtx_assert_ex(&ip->pmp->xop_lock);

	ipdep = &ip->pmp->ipdep_lists[xop_ipdep_value(ip)];
	LIST_FOREACH(iptmp, ipdep, entry)
		if (iptmp == ip) {
			LIST_REMOVE(ip, entry);
			return;
		}
}

/*
 * Start a XOP request, queueing it to all nodes in the cluster to
 * execute the cluster op.
 */
void
hammer2_xop_start(hammer2_xop_head_t *xop, hammer2_xop_desc_t *desc)
{
	hammer2_inode_t *ip = xop->ip1;
	hammer2_pfs_t *pmp = ip->pmp;
	uint32_t mask;
	int i;

	hammer2_assert_cluster(&ip->cluster);
	xop->desc = desc;

	for (i = 0; i < ip->cluster.nchains; ++i) {
		if (ip->cluster.array[i].chain) {
			atomic_set_32(&xop->run_mask, 1LLU << i);
			atomic_set_32(&xop->chk_mask, 1LLU << i);
		}
	}

	for (i = 0; i < ip->cluster.nchains; ++i) {
		mask = 1LLU << i;
		if (hammer2_xop_active(xop)) {
			hammer2_mtx_ex(&pmp->xop_lock);
again:
			if (xop_testset_ipdep(ip)) {
				pmp->flags |= HAMMER2_PMPF_WAITING;
				hammer2_mtx_sleep(pmp, &pmp->xop_lock, "h2pmp");
				goto again;
			}
			hammer2_mtx_unlock(&pmp->xop_lock);

			xop->desc->storage_func((hammer2_xop_t *)xop, i);
			hammer2_xop_retire(xop, mask);
		} else {
			hammer2_xop_feed(xop, NULL, i, ECONNABORTED);
			hammer2_xop_retire(xop, mask);
		}
	}
}

/*
 * Retire a XOP.  Used by both the VOP frontend and by the XOP backend.
 */
void
hammer2_xop_retire(hammer2_xop_head_t *xop, uint32_t mask)
{
	hammer2_pfs_t *pmp;
	hammer2_chain_t *chain;
	hammer2_xop_fifo_t *fifo;

	uint32_t omask;
	int i;

	/* Remove the frontend collector or remove a backend feeder. */
	KASSERT(xop->run_mask & mask, ("%x vs %x", xop->run_mask, mask));
	omask = atomic_fetchadd_32(&xop->run_mask, -mask);

	/* More than one entity left. */
	if ((omask & HAMMER2_XOPMASK_ALLDONE) != mask)
		return;

	/*
	 * All collectors are gone, we can cleanup and dispose of the XOP.
	 * Cleanup the collection cluster.
	 */
	for (i = 0; i < xop->cluster.nchains; ++i) {
		xop->cluster.array[i].flags = 0;
		chain = xop->cluster.array[i].chain;
		if (chain) {
			xop->cluster.array[i].chain = NULL;
			hammer2_chain_drop_unhold(chain);
		}
	}

	/*
	 * Cleanup the fifos.  Since we are the only entity left on this
	 * xop we don't have to worry about fifo flow control.
	 */
	mask = xop->chk_mask;
	for (i = 0; mask && i < HAMMER2_MAXCLUSTER; ++i) {
		fifo = &xop->collect[i];
		while (fifo->ri != fifo->wi) {
			chain = fifo->array[fifo->ri & fifo_mask(xop)];
			if (chain)
				hammer2_chain_drop_unhold(chain);
			++fifo->ri;
		}
		mask &= ~(1U << i);
	}

	/* The inode is only held at this point, simply drop it. */
	if (xop->ip1) {
		pmp = xop->ip1->pmp;
		hammer2_mtx_ex(&pmp->xop_lock);
		xop_unset_ipdep(xop->ip1);
		if (pmp->flags & HAMMER2_PMPF_WAITING) {
			pmp->flags &= ~HAMMER2_PMPF_WAITING;
			hammer2_mtx_wakeup(pmp);
		}
		hammer2_mtx_unlock(&pmp->xop_lock);

		hammer2_inode_drop(xop->ip1);
		xop->ip1 = NULL;
	}

	if (xop->name1) {
		free(xop->name1, M_HAMMER2);
		xop->name1 = NULL;
		xop->name1_len = 0;
	}

	for (i = 0; i < xop->cluster.nchains; ++i) {
		fifo = &xop->collect[i];
		free(fifo->array, M_HAMMER2);
		free(fifo->errors, M_HAMMER2);
	}

	uma_zfree(zone_xops, xop);
}

/*
 * (Backend) Feed chain data.
 * The chain must be locked (either shared or exclusive).  The caller may
 * unlock and drop the chain on return.  This function will add an extra
 * ref and hold the chain's data for the pass-back.
 *
 * No xop lock is needed because we are only manipulating fields under
 * our direct control.
 *
 * Returns 0 on success and a HAMMER2 error code if sync is permanently
 * lost.  The caller retains a ref on the chain but by convention
 * the lock is typically inherited by the xop (caller loses lock).
 *
 * Returns non-zero on error.  In this situation the caller retains a
 * ref on the chain but loses the lock (we unlock here).
 */
int
hammer2_xop_feed(hammer2_xop_head_t *xop, hammer2_chain_t *chain, int clindex,
    int error)
{
	hammer2_xop_fifo_t *fifo;

	/* Early termination (typically of xop_readir). */
	if (hammer2_xop_active(xop) == 0) {
		error = HAMMER2_ERROR_ABORTED;
		goto done;
	}

	/*
	 * Entry into the XOP collector.
	 * We own the fifo->wi for our clindex.
	 */
	fifo = &xop->collect[clindex];
	while (fifo->ri == fifo->wi - xop->fifo_size) {
		if ((xop->run_mask & HAMMER2_XOPMASK_VOP) == 0) {
			error = HAMMER2_ERROR_ABORTED;
			goto done;
		}
		xop->fifo_size *= 2;
		hammer2_xop_fifo_alloc(fifo, xop->fifo_size);
	}

	if (chain)
		hammer2_chain_ref_hold(chain);
	if (error == 0 && chain)
		error = chain->error;
	fifo->errors[fifo->wi & fifo_mask(xop)] = error;
	fifo->array[fifo->wi & fifo_mask(xop)] = chain;
	++fifo->wi;

	error = 0;
done:
	return (error);
}

/*
 * (Frontend) collect a response from a running cluster op.
 * Responses are collected into a cohesive response >= collect_key.
 *
 * Returns 0 on success plus a filled out xop->cluster structure.
 * Return ENOENT on normal termination.
 * Otherwise return an error.
 */
int
hammer2_xop_collect(hammer2_xop_head_t *xop, int flags)
{
	hammer2_xop_fifo_t *fifo;
	hammer2_chain_t *chain;
	hammer2_key_t lokey;
	int i, keynull, adv, error;

	/*
	 * First loop tries to advance pieces of the cluster which
	 * are out of sync.
	 */
	lokey = HAMMER2_KEY_MAX;
	keynull = HAMMER2_CHECK_NULL;

	for (i = 0; i < xop->cluster.nchains; ++i) {
		chain = xop->cluster.array[i].chain;
		if (chain == NULL) {
			adv = 1;
		} else if (chain->bref.key < xop->collect_key) {
			adv = 1;
		} else {
			keynull &= ~HAMMER2_CHECK_NULL;
			if (lokey > chain->bref.key)
				lokey = chain->bref.key;
			adv = 0;
		}
		if (adv == 0)
			continue;

		/* Advance element if possible, advanced element may be NULL. */
		if (chain)
			hammer2_chain_drop_unhold(chain);

		fifo = &xop->collect[i];
		if (fifo->ri != fifo->wi) {
			chain = fifo->array[fifo->ri & fifo_mask(xop)];
			error = fifo->errors[fifo->ri & fifo_mask(xop)];
			++fifo->ri;
			xop->cluster.array[i].chain = chain;
			xop->cluster.array[i].error = error;
			if (chain == NULL)
				xop->cluster.array[i].flags |=
				    HAMMER2_CITEM_NULL;
			--i; /* Loop on same index. */
		} else {
			/*
			 * Retain CITEM_NULL flag.  If set just repeat EOF.
			 * If not, the NULL,0 combination indicates an
			 * operation in-progress.
			 */
			xop->cluster.array[i].chain = NULL;
			/* Retain any CITEM_NULL setting. */
		}
	}

	/*
	 * Determine whether the lowest collected key meets clustering
	 * requirements.  Returns HAMMER2_ERROR_*:
	 *
	 * 0	  - key valid, cluster can be returned.
	 * ENOENT - normal end of scan, return ENOENT.
	 * EIO	  - IO error or CRC check error from hammer2_cluster_check().
	 */
	error = hammer2_cluster_check(&xop->cluster, lokey, keynull);

	if (lokey == HAMMER2_KEY_MAX)
		xop->collect_key = lokey;
	else
		xop->collect_key = lokey + 1;

	return (error);
}
