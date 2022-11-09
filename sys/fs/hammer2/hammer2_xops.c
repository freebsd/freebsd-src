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
 * Backend for hammer2_readdir().
 */
void
hammer2_xop_readdir(hammer2_xop_t *arg, int clindex)
{
	hammer2_xop_readdir_t *xop = &arg->xop_readdir;
	hammer2_chain_t *chain, *parent;
	hammer2_key_t lkey, key_next;
	int error = 0;

	lkey = xop->lkey;

	/*
	 * The inode's chain is the iterator.  If we cannot acquire it our
	 * contribution ends here.
	 */
	parent = hammer2_inode_chain(xop->head.ip1, clindex,
	    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
	if (parent == NULL) {
		hprintf("NULL parent\n");
		goto done;
	}

	/*
	 * Directory scan [re]start and loop, the feed inherits the chain's
	 * lock so do not unlock it on the iteration.
	 */
	chain = hammer2_chain_lookup(&parent, &key_next, lkey, lkey, &error,
	    HAMMER2_LOOKUP_SHARED);
	if (chain == NULL)
		chain = hammer2_chain_lookup(&parent, &key_next, lkey,
		    HAMMER2_KEY_MAX, &error, HAMMER2_LOOKUP_SHARED);
	while (chain) {
		error = hammer2_xop_feed(&xop->head, chain, clindex, 0);
		if (error)
			goto break2;
		chain = hammer2_chain_next(&parent, chain, &key_next, key_next,
		    HAMMER2_KEY_MAX, &error, HAMMER2_LOOKUP_SHARED);
	}
break2:
	if (chain) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	}
	hammer2_chain_unlock(parent);
	hammer2_chain_drop(parent);
done:
	hammer2_xop_feed(&xop->head, NULL, clindex, error);
}

/*
 * Backend for hammer2_nresolve().
 */
void
hammer2_xop_nresolve(hammer2_xop_t *arg, int clindex)
{
	hammer2_xop_nresolve_t *xop = &arg->xop_nresolve;
	hammer2_chain_t *chain, *parent;
	hammer2_key_t lhc, key_next;
	const char *name;
	size_t name_len;
	int error;

	chain = NULL;
	parent = hammer2_inode_chain(xop->head.ip1, clindex,
	    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
	if (parent == NULL) {
		hprintf("NULL parent\n");
		error = HAMMER2_ERROR_EIO;
		goto done;
	}
	name = xop->head.name1;
	name_len = xop->head.name1_len;

	/* Lookup the directory entry. */
	lhc = hammer2_dirhash(name, name_len);
	chain = hammer2_chain_lookup(&parent, &key_next, lhc,
	    lhc + HAMMER2_DIRHASH_LOMASK, &error,
	    HAMMER2_LOOKUP_ALWAYS | HAMMER2_LOOKUP_SHARED);
	while (chain) {
		if (hammer2_chain_dirent_test(chain, name, name_len))
			break;
		chain = hammer2_chain_next(&parent, chain, &key_next, key_next,
		    lhc + HAMMER2_DIRHASH_LOMASK, &error,
		    HAMMER2_LOOKUP_ALWAYS | HAMMER2_LOOKUP_SHARED);
	}

	/* Locate the target inode for a directory entry. */
	if (chain && chain->error == 0) {
		if (chain->bref.type == HAMMER2_BREF_TYPE_DIRENT) {
			lhc = chain->bref.embed.dirent.inum;
			error = hammer2_chain_inode_find(chain->pmp, lhc,
			    clindex, HAMMER2_LOOKUP_SHARED, &parent, &chain);
		}
	} else if (chain && error == 0) {
		error = chain->error;
	}
done:
	error = hammer2_xop_feed(&xop->head, chain, clindex, error);
	if (chain) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
}

/*
 * Generic lookup of a specific key.
 */
void
hammer2_xop_lookup(hammer2_xop_t *arg, int clindex)
{
	hammer2_xop_lookup_t *xop = &arg->xop_lookup;
	hammer2_chain_t *chain, *parent;
	hammer2_key_t key_next;
	int error = 0;

	chain = NULL;
	parent = hammer2_inode_chain(xop->head.ip1, clindex,
	    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
	if (parent == NULL) {
		hprintf("NULL parent\n");
		error = HAMMER2_ERROR_EIO;
		goto done;
	}

	/*
	 * Lookup all possibly conflicting directory entries, the feed
	 * inherits the chain's lock so do not unlock it on the iteration.
	 */
	chain = hammer2_chain_lookup(&parent, &key_next, xop->lhc, xop->lhc,
	    &error, HAMMER2_LOOKUP_ALWAYS | HAMMER2_LOOKUP_SHARED);
	if (error == 0) {
		if (chain)
			error = chain->error;
		else
			error = HAMMER2_ERROR_ENOENT;
	}
	hammer2_xop_feed(&xop->head, chain, clindex, error);
done:
	if (chain) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
}

/*
 * Backend for hammer2_bmap().
 */
void
hammer2_xop_bmap(hammer2_xop_t *arg, int clindex)
{
	hammer2_xop_bmap_t *xop = &arg->xop_bmap;
	hammer2_inode_t *ip = xop->head.ip1;
	hammer2_chain_t *chain, *parent;
	hammer2_key_t lbase, key_dummy;
	int lblksize, error = 0;

	lblksize = hammer2_get_logical();
	lbase = (hammer2_key_t)xop->lbn * lblksize;
	KKASSERT(((int)lbase & HAMMER2_PBUFMASK) == 0);

	chain = NULL;
	parent = hammer2_inode_chain(ip, clindex,
	    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
	if (parent == NULL) {
		hprintf("NULL parent\n");
		error = HAMMER2_ERROR_EIO;
		goto done;
	}

	/*
	 * NULL chain isn't necessarily an error.
	 * It could be a zero filled data without physical block assigned.
	 */
	chain = hammer2_chain_lookup(&parent, &key_dummy, lbase, lbase,
	    &error, HAMMER2_LOOKUP_ALWAYS | HAMMER2_LOOKUP_SHARED);
	if (error == 0) {
		if (chain) {
			error = chain->error;
			if (error == 0)
				xop->pbn = (chain->bref.data_off &
				    ~HAMMER2_OFF_MASK_RADIX) / lblksize;
		} else {
			error = HAMMER2_ERROR_ENOENT;
		}
	}
done:
	error = hammer2_xop_feed(&xop->head, chain, clindex, error);
	if (chain) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
	}
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
}
