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

#include "hammer2.h"

/*
 * Returns the bref type of the cluster's foucs.
 *
 * If the cluster is errored, returns HAMMER2_BREF_TYPE_EMPTY (0).
 * The cluster must be locked.
 */
uint8_t
hammer2_cluster_type(const hammer2_cluster_t *cluster)
{
	if (cluster->error == 0) {
		KKASSERT(cluster->focus != NULL);
		return (cluster->focus->bref.type);
	}

	return (0);
}

/*
 * Returns the bref of the cluster's focus, sans any data-offset information
 * (since offset information is per-node and wouldn't be useful).
 *
 * If the cluster is errored, returns an empty bref.
 * The cluster must be locked.
 */
void
hammer2_cluster_bref(const hammer2_cluster_t *cluster, hammer2_blockref_t *bref)
{
	if (cluster->error == 0) {
		KKASSERT(cluster->focus != NULL);
		*bref = cluster->focus->bref;
		bref->data_off = 0;
	} else {
		bzero(bref, sizeof(*bref));
	}
}

/*
 * Create a degenerate cluster with one ref from a single locked chain.
 * The returned cluster will be focused on the chain and inherit its
 * error state.
 *
 * The chain's lock and reference are transfered to the new cluster, so
 * the caller should not try to unlock the chain separately.
 */
void
hammer2_dummy_xop_from_chain(hammer2_xop_head_t *xop, hammer2_chain_t *chain)
{
	hammer2_cluster_t *cluster = &xop->cluster;

	bzero(xop, sizeof(*xop));

	cluster->array[0].chain = chain;
	cluster->nchains = 1;
	cluster->focus = chain;
	cluster->pmp = chain->pmp;
	cluster->error = chain->error;

	hammer2_assert_cluster(cluster);
}

void
hammer2_cluster_unhold(hammer2_cluster_t *cluster)
{
	hammer2_chain_t *chain;
	int i;

	for (i = 0; i < cluster->nchains; ++i) {
		chain = cluster->array[i].chain;
		if (chain)
			hammer2_chain_unhold(chain);
	}
}

void
hammer2_cluster_rehold(hammer2_cluster_t *cluster)
{
	hammer2_chain_t *chain;
	int i;

	for (i = 0; i < cluster->nchains; ++i) {
		chain = cluster->array[i].chain;
		if (chain)
			hammer2_chain_rehold(chain);
	}
}

/*
 * This is used by the XOPS subsystem to calculate the state of
 * the collection and tell hammer2_xop_collect() what to do with it.
 */
int
hammer2_cluster_check(hammer2_cluster_t *cluster, hammer2_key_t key, int flags)
{
	hammer2_pfs_t *pmp;
	hammer2_chain_t *chain;
	int i, error;

	cluster->focus = NULL;
	cluster->error = 0;
	hammer2_assert_cluster(cluster);

	pmp = cluster->pmp;
	KKASSERT(pmp != NULL);

	/*
	 * NOTE: A NULL chain is not necessarily an error, it could be
	 *	 e.g. a lookup failure or the end of an iteration.
	 *	 Process normally.
	 */
	for (i = 0; i < cluster->nchains; ++i) {
		chain = cluster->array[i].chain;
		error = cluster->array[i].error;

		switch (pmp->pfs_types[i]) {
		case HAMMER2_PFSTYPE_MASTER:
		case HAMMER2_PFSTYPE_SUPROOT:
			cluster->focus = chain;
			cluster->error = error;
			break;
		default:
			hpanic("invalid PFS type %d", pmp->pfs_types[i]);
			break;
		}
	}

	if (flags & HAMMER2_CHECK_NULL) {
		if (cluster->error == 0)
			cluster->error = HAMMER2_ERROR_ENOENT;
		return (cluster->error);
	}

	if (cluster->focus == NULL)
		return (HAMMER2_ERROR_EIO);

	for (i = 0; i < cluster->nchains; ++i) {
		chain = cluster->array[i].chain;
		if (i == 0) {
			KKASSERT(chain != NULL);
			KKASSERT(chain == cluster->focus);
			KKASSERT(chain->bref.key == key);
		} else {
			KKASSERT(chain == NULL);
		}
	}

	return (cluster->error);
}
