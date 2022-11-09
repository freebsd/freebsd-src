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
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/tree.h>

#include <crypto/sha2/sha256.h>

#include "hammer2.h"
#include "hammer2_xxhash.h"

static hammer2_chain_t *hammer2_combined_find(hammer2_chain_t *,
    hammer2_blockref_t *, int, hammer2_key_t *, hammer2_key_t, hammer2_key_t,
    hammer2_blockref_t **);
static hammer2_chain_t *hammer2_chain_lastdrop(hammer2_chain_t *, int);
static void hammer2_chain_lru_flush(hammer2_pfs_t *);
static void hammer2_chain_load_data(hammer2_chain_t *);
static int hammer2_chain_testcheck(const hammer2_chain_t *, void *);

/*
 * Basic RBTree for chains.
 */
static int
hammer2_chain_cmp(const hammer2_chain_t *chain1, const hammer2_chain_t *chain2)
{
	hammer2_key_t c1_beg, c1_end, c2_beg, c2_end;

	/*
	 * Compare chains.  Overlaps are not supposed to happen and catch
	 * any software issues early we count overlaps as a match.
	 */
	c1_beg = chain1->bref.key;
	c1_end = c1_beg + ((hammer2_key_t)1 << chain1->bref.keybits) - 1;
	c2_beg = chain2->bref.key;
	c2_end = c2_beg + ((hammer2_key_t)1 << chain2->bref.keybits) - 1;

	if (c1_end < c2_beg)	/* fully to the left */
		return (-1);
	if (c1_beg > c2_end)	/* fully to the right */
		return (1);
	return (0);		/* overlap (must not cross edge boundary) */
}

RB_GENERATE_STATIC(hammer2_chain_tree, hammer2_chain, rbnode,
    hammer2_chain_cmp);
RB_SCAN_INFO(hammer2_chain_tree, hammer2_chain);
RB_GENERATE_SCAN_STATIC(hammer2_chain_tree, hammer2_chain, rbnode);

/*
 * Assert that a chain has no media data associated with it.
 */
static __inline void
hammer2_chain_assert_no_data(const hammer2_chain_t *chain)
{
	KKASSERT(chain->dio == NULL);

	if (chain->bref.type != HAMMER2_BREF_TYPE_VOLUME &&
	    chain->bref.type != HAMMER2_BREF_TYPE_FREEMAP &&
	    chain->data)
		hpanic("chain %p still has data", chain);
}

/*
 * Allocate a new disconnected chain element representing the specified
 * bref.  chain->refs is set to 1 and the passed bref is copied to
 * chain->bref.  chain->bytes is derived from the bref.
 *
 * Returns a referenced but unlocked (because there is no core) chain.
 */
static hammer2_chain_t *
hammer2_chain_alloc(hammer2_dev_t *hmp, hammer2_pfs_t *pmp,
    hammer2_blockref_t *bref)
{
	hammer2_chain_t *chain;
	unsigned int bytes;

	/*
	 * Special case - radix of 0 indicates a chain that does not
	 * need a data reference (context is completely embedded in the
	 * bref).
	 */
	if ((int)(bref->data_off & HAMMER2_OFF_MASK_RADIX))
		bytes = 1U << (int)(bref->data_off & HAMMER2_OFF_MASK_RADIX);
	else
		bytes = 0;

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_INODE:
	case HAMMER2_BREF_TYPE_INDIRECT:
	case HAMMER2_BREF_TYPE_DATA:
	case HAMMER2_BREF_TYPE_DIRENT:
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
	case HAMMER2_BREF_TYPE_FREEMAP:
	case HAMMER2_BREF_TYPE_VOLUME:
		chain = malloc(sizeof(*chain), M_HAMMER2, M_WAITOK | M_ZERO);
		atomic_add_long(&hammer2_chain_allocs, 1);
		break;
	case HAMMER2_BREF_TYPE_EMPTY:
	default:
		hpanic("bad blockref type %d", bref->type);
		break;
	}

	/*
	 * Initialize the new chain structure.  pmp must be set to NULL for
	 * chains belonging to the super-root topology of a device mount.
	 */
	if (pmp == hmp->spmp)
		chain->pmp = NULL;
	else
		chain->pmp = pmp;

	chain->hmp = hmp;
	chain->bref = *bref;
	chain->bytes = bytes;
	chain->refs = 1;
	chain->flags = HAMMER2_CHAIN_ALLOCATED;

	hammer2_chain_init(chain);

	return (chain);
}

/*
 * A common function to initialize chains including vchain.
 */
void
hammer2_chain_init(hammer2_chain_t *chain)
{
	RB_INIT(&chain->core.rbtree);
	hammer2_mtx_init_recurse(&chain->lock, "h2ch_lk");
	hammer2_mtx_init(&chain->inp_lock, "h2ch_inplk");
	hammer2_spin_init(&chain->core.spin, "h2ch_cosp");
}

/*
 * Add a reference to a chain element, preventing its destruction.
 * Can be called with spinlock held.
 */
void
hammer2_chain_ref(hammer2_chain_t *chain)
{
	if (atomic_fetchadd_int(&chain->refs, 1) == 0) {
		/*
		 * Just flag that the chain was used and should be recycled
		 * on the LRU if it encounters it later.
		 */
		if (chain->flags & HAMMER2_CHAIN_ONLRU)
			atomic_set_int(&chain->flags, HAMMER2_CHAIN_LRUHINT);
	}
}

/*
 * Ref a locked chain and force the data to be held across an unlock.
 * Chain must be currently locked.
 */
void
hammer2_chain_ref_hold(hammer2_chain_t *chain)
{
	hammer2_mtx_assert_locked(&chain->lock);

	atomic_add_int(&chain->lockcnt, 1);
	hammer2_chain_ref(chain);
}

/*
 * Insert the chain in the core rbtree.
 */
static int
hammer2_chain_insert(hammer2_chain_t *parent, hammer2_chain_t *chain,
    int generation)
{
	hammer2_chain_t *xchain __diagused;
	int error = 0;

	hammer2_spin_ex(&parent->core.spin);

	/* Interlocked by spinlock, check for race. */
	if (parent->core.generation != generation) {
		error = HAMMER2_ERROR_EAGAIN;
		goto failed;
	}

	/* Insert chain. */
	xchain = RB_INSERT(hammer2_chain_tree, &parent->core.rbtree, chain);
	KASSERT(xchain == NULL,
	    ("collision %p %p %016jx", chain, xchain, chain->bref.key));

	atomic_set_int(&chain->flags, HAMMER2_CHAIN_ONRBTREE);
	chain->parent = parent;
	++parent->core.chain_count;
	++parent->core.generation; /* XXX incs for _get() too */
failed:
	hammer2_spin_unex(&parent->core.spin);

	return (error);
}

/*
 * Drop the caller's reference to the chain.  When the ref count drops to
 * zero this function will try to disassociate the chain from its parent and
 * deallocate it, then recursely drop the parent using the implied ref
 * from the chain's chain->parent.
 *
 * Nobody should own chain's mutex on the 1->0 transition, unless this drop
 * races an acquisition by another cpu.  Therefore we can loop if we are
 * unable to acquire the mutex, and refs is unlikely to be 1 unless we again
 * race against another drop.
 */
void
hammer2_chain_drop(hammer2_chain_t *chain)
{
	unsigned int refs;

	KKASSERT(chain->refs > 0);

	while (chain) {
		refs = chain->refs;
		__compiler_membar();

		KKASSERT(refs > 0);
		if (refs == 1) {
			if (hammer2_mtx_ex_try(&chain->lock) == 0)
				chain = hammer2_chain_lastdrop(chain, 0);
			/* Retry the same chain, or chain from lastdrop. */
		} else {
			if (atomic_cmpset_int(&chain->refs, refs, refs - 1))
				break;
			/* Retry the same chain. */
		}
		cpu_spinwait();
	}
}

/*
 * Unhold a held and probably not-locked chain, ensure that the data is
 * dropped on the 1->0 transition of lockcnt by obtaining an exclusive
 * lock and then simply unlocking the chain.
 */
void
hammer2_chain_unhold(hammer2_chain_t *chain)
{
	unsigned int lockcnt;
	int iter = 0;

	for (;;) {
		lockcnt = chain->lockcnt;
		__compiler_membar();

		if (lockcnt > 1) {
			if (atomic_cmpset_int(&chain->lockcnt, lockcnt,
			    lockcnt - 1))
				break;
		} else if (hammer2_mtx_ex_try(&chain->lock) == 0) {
			hammer2_chain_unlock(chain);
			break;
		} else {
			/*
			 * This situation can easily occur on SMP due to
			 * the gap inbetween the 1->0 transition and the
			 * final unlock.  We cannot safely block on the
			 * mutex because lockcnt might go above 1.
			 */
			if (++iter > 1000) {
				if (iter > 1000 + hz) {
					hprintf("h2race1\n");
					iter = 1000;
				}
				pause("h2race1", 1);
			}
			cpu_spinwait();
		}
	}
}

void
hammer2_chain_drop_unhold(hammer2_chain_t *chain)
{
	hammer2_chain_unhold(chain);
	hammer2_chain_drop(chain);
}

void
hammer2_chain_rehold(hammer2_chain_t *chain)
{
	hammer2_chain_lock(chain, HAMMER2_RESOLVE_SHARED);
	atomic_add_int(&chain->lockcnt, 1);
	hammer2_chain_unlock(chain);
}

/*
 * Handles the (potential) last drop of chain->refs from 1->0.  Called with
 * the mutex exclusively locked, refs == 1, and lockcnt 0.  SMP races are
 * possible against refs and lockcnt.  We must dispose of the mutex on chain.
 *
 * This function returns an unlocked chain for recursive drop or NULL.  It
 * can return the same chain if it determines it has raced another ref.
 *
 * The chain cannot be freed if it has any children.
 * The core spinlock is allowed to nest child-to-parent (not parent-to-child).
 */
static hammer2_chain_t *
hammer2_chain_lastdrop(hammer2_chain_t *chain, int depth)
{
	hammer2_pfs_t *pmp;
	hammer2_chain_t *parent, *rdrop;

	hammer2_mtx_assert_ex(&chain->lock);

	/*
	 * We need chain's spinlock to interlock the sub-tree test.
	 * We already have chain's mutex, protecting chain->parent.
	 * Remember that chain->refs can be in flux.
	 */
	hammer2_spin_ex(&chain->core.spin);

	if (chain->parent != NULL) {
		/* Nothing to do for read-only mount. */
	} else if (chain->bref.type == HAMMER2_BREF_TYPE_VOLUME ||
		   chain->bref.type == HAMMER2_BREF_TYPE_FREEMAP) {
		/* Nothing to do for read-only mount. */
	} else {
		/*
		 * The chain has no parent and can be flagged for destruction.
		 * This can happen for e.g. via
		 *  hammer2_chain_lookup()
		 *    hammer2_chain_get()
		 *      hammer2_chain_insert() -> HAMMER2_ERROR_EAGAIN
		 *      hammer2_chain_drop() (chain->parent still NULL)
		 *        hammer2_chain_lastdrop()
		 */
		atomic_set_int(&chain->flags, HAMMER2_CHAIN_DESTROY);
	}

	/*
	 * If any children exist we must leave the chain intact with refs == 0.
	 * They exist because chains are retained below us which have refs.
	 *
	 * Retry (return chain) if we fail to transition the refs to 0, else
	 * return NULL indication nothing more to do.
	 *
	 * Chains with children are NOT put on the LRU list.
	 */
	if (chain->core.chain_count) {
		if (atomic_cmpset_int(&chain->refs, 1, 0)) {
			hammer2_spin_unex(&chain->core.spin);
			hammer2_chain_assert_no_data(chain);
			hammer2_mtx_unlock(&chain->lock);
			chain = NULL;
		} else {
			hammer2_spin_unex(&chain->core.spin);
			hammer2_mtx_unlock(&chain->lock);
		}
		return (chain);
	}
	/* Spinlock still held. */
	/* No chains left under us. */

	/*
	 * chain->core has no children left so no accessors can get to our
	 * chain from there.  Now we have to lock the parent core to interlock
	 * remaining possible accessors that might bump chain's refs before
	 * we can safely drop chain's refs with intent to free the chain.
	 */
	pmp = chain->pmp;	/* can be NULL */
	rdrop = NULL;
	parent = chain->parent;

	/*
	 * WARNING! chain's spin lock is still held here, and other spinlocks
	 *	    will be acquired and released in the code below.  We
	 *	    cannot be making fancy procedure calls!
	 */

	/*
	 * We can cache the chain if it is associated with a pmp
	 * and not flagged as being destroyed or requesting a full
	 * release.  In this situation the chain is not removed
	 * from its parent, i.e. it can still be looked up.
	 *
	 * We intentionally do not cache DATA chains because these
	 * were likely used to load data into the logical buffer cache
	 * and will not be accessed again for some time.
	 */
	if ((chain->flags &
	    (HAMMER2_CHAIN_DESTROY | HAMMER2_CHAIN_RELEASE)) == 0 &&
	    chain->pmp && chain->bref.type != HAMMER2_BREF_TYPE_DATA) {
		if (parent)
			hammer2_spin_ex(&parent->core.spin);
		if (atomic_cmpset_int(&chain->refs, 1, 0) == 0) {
			/*
			 * 1->0 transition failed, retry.  Do not drop
			 * the chain's data yet!
			 */
			if (parent)
				hammer2_spin_unex(&parent->core.spin);
			hammer2_spin_unex(&chain->core.spin);
			hammer2_mtx_unlock(&chain->lock);
			return (chain);
		}

		/* Success. */
		hammer2_chain_assert_no_data(chain);

		/*
		 * Make sure we are on the LRU list, clean up excessive
		 * LRU entries.  We can only really drop one but there might
		 * be other entries that we can remove from the lru_list
		 * without dropping.
		 *
		 * NOTE: HAMMER2_CHAIN_ONLRU may only be safely set when
		 *	 chain->core.spin AND pmp->lru_spin are held, but
		 *	 can be safely cleared only holding pmp->lru_spin.
		 */
		if ((chain->flags & HAMMER2_CHAIN_ONLRU) == 0) {
			hammer2_spin_ex(&pmp->lru_spin);
			if ((chain->flags & HAMMER2_CHAIN_ONLRU) == 0) {
				atomic_set_int(&chain->flags,
				    HAMMER2_CHAIN_ONLRU);
				TAILQ_INSERT_TAIL(&pmp->lru_list, chain, entry);
				atomic_add_int(&pmp->lru_count, 1);
			}
			if (pmp->lru_count < HAMMER2_LRU_LIMIT)
				depth = 1;	/* Disable lru_list flush. */
			hammer2_spin_unex(&pmp->lru_spin);
		} else {
			/* Disable lru_list flush. */
			depth = 1;
		}

		if (parent)
			hammer2_spin_unex(&parent->core.spin);
		hammer2_spin_unex(&chain->core.spin);
		hammer2_mtx_unlock(&chain->lock);

		/*
		 * lru_list hysteresis (see above for depth overrides).
		 * Note that depth also prevents excessive lastdrop recursion.
		 */
		if (depth == 0)
			hammer2_chain_lru_flush(pmp);
		return (NULL);
	}

	/* Make sure we are not on the LRU list. */
	if (chain->flags & HAMMER2_CHAIN_ONLRU) {
		hammer2_spin_ex(&pmp->lru_spin);
		if (chain->flags & HAMMER2_CHAIN_ONLRU) {
			atomic_add_int(&pmp->lru_count, -1);
			atomic_clear_int(&chain->flags, HAMMER2_CHAIN_ONLRU);
			TAILQ_REMOVE(&pmp->lru_list, chain, entry);
		}
		hammer2_spin_unex(&pmp->lru_spin);
	}

	/*
	 * Spinlock the parent and try to drop the last ref on chain.
	 * On success determine if we should dispose of the chain
	 * (remove the chain from its parent, etc).
	 *
	 * Normal core locks are top-down recursive but we define
	 * core spinlocks as bottom-up recursive, so this is safe.
	 */
	if (parent) {
		hammer2_spin_ex(&parent->core.spin);
		if (atomic_cmpset_int(&chain->refs, 1, 0) == 0) {
			/* 1->0 transition failed, retry. */
			hammer2_spin_unex(&parent->core.spin);
			hammer2_spin_unex(&chain->core.spin);
			hammer2_mtx_unlock(&chain->lock);
			return (chain);
		}

		/*
		 * 1->0 transition successful, parent spin held to prevent
		 * new lookups, chain spinlock held to protect parent field.
		 * Remove chain from the parent.
		 */
		if (chain->flags & HAMMER2_CHAIN_ONRBTREE) {
			RB_REMOVE(hammer2_chain_tree, &parent->core.rbtree,
			    chain);
			atomic_clear_int(&chain->flags, HAMMER2_CHAIN_ONRBTREE);
			--parent->core.chain_count;
			chain->parent = NULL;
		}

		/*
		 * If our chain was the last chain in the parent's core the
		 * core is now empty and its parent might have to be
		 * re-dropped if it has 0 refs.
		 */
		if (parent->core.chain_count == 0) {
			rdrop = parent;
			atomic_add_int(&rdrop->refs, 1);
			/*
			if (atomic_cmpset_int(&rdrop->refs, 0, 1) == 0)
				rdrop = NULL;
			*/
		}
		hammer2_spin_unex(&parent->core.spin);
	} else {
		/* No-parent case. */
		if (atomic_cmpset_int(&chain->refs, 1, 0) == 0) {
			/* 1->0 transition failed, retry. */
			hammer2_spin_unex(&parent->core.spin);
			hammer2_spin_unex(&chain->core.spin);
			hammer2_mtx_unlock(&chain->lock);
			return (chain);
		}
	}

	/*
	 * Successful 1->0 transition, no parent, no children... no way for
	 * anyone to ref this chain any more.  We can clean-up and free it.
	 *
	 * We still have the core spinlock, and core's chain_count is 0.
	 * Any parent spinlock is gone.
	 */
	hammer2_spin_unex(&chain->core.spin);
	hammer2_chain_assert_no_data(chain);
	hammer2_mtx_unlock(&chain->lock);
	KKASSERT(RB_EMPTY(&chain->core.rbtree) && chain->core.chain_count == 0);

	/*
	 * All locks are gone, no pointers remain to the chain, finish
	 * freeing it.
	 */
	if (chain->flags & HAMMER2_CHAIN_ALLOCATED) {
		atomic_clear_int(&chain->flags, HAMMER2_CHAIN_ALLOCATED);
		hammer2_mtx_destroy(&chain->lock);
		hammer2_mtx_destroy(&chain->inp_lock);
		hammer2_spin_destroy(&chain->core.spin);
		chain->hmp = NULL;
		free(chain, M_HAMMER2);
		atomic_add_long(&hammer2_chain_allocs, -1);
	}

	/* Possible chaining loop when parent re-drop needed. */
	return (rdrop);
}

/*
 * Heuristical flush of the LRU, try to reduce the number of entries
 * on the LRU to (HAMMER2_LRU_LIMIT * 2 / 3).  This procedure is called
 * only when lru_count exceeds HAMMER2_LRU_LIMIT.
 */
static void
hammer2_chain_lru_flush(hammer2_pfs_t *pmp)
{
	hammer2_chain_t *chain;
	unsigned int refs;
again:
	chain = NULL;
	hammer2_spin_ex(&pmp->lru_spin);
	while (pmp->lru_count > HAMMER2_LRU_LIMIT * 2 / 3) {
		/*
		 * Pick a chain off the lru_list, just recycle it quickly
		 * if LRUHINT is set (the chain was ref'd but left on
		 * the lru_list, so cycle to the end).
		 */
		chain = TAILQ_FIRST(&pmp->lru_list);
		TAILQ_REMOVE(&pmp->lru_list, chain, entry);

		if (chain->flags & HAMMER2_CHAIN_LRUHINT) {
			atomic_clear_int(&chain->flags, HAMMER2_CHAIN_LRUHINT);
			TAILQ_INSERT_TAIL(&pmp->lru_list, chain, entry);
			chain = NULL;
			continue;
		}

		/*
		 * Ok, we are off the LRU.  We must adjust refs before we
		 * can safely clear the ONLRU flag.
		 */
		atomic_add_int(&pmp->lru_count, -1);
		if (atomic_cmpset_int(&chain->refs, 0, 1)) {
			atomic_clear_int(&chain->flags, HAMMER2_CHAIN_ONLRU);
			atomic_set_int(&chain->flags, HAMMER2_CHAIN_RELEASE);
			break;
		}
		atomic_clear_int(&chain->flags, HAMMER2_CHAIN_ONLRU);
		chain = NULL;
	}
	hammer2_spin_unex(&pmp->lru_spin);
	if (chain == NULL)
		return;

	/*
	 * If we picked a chain off the lru list we may be able to lastdrop
	 * it.  Use a depth of 1 to prevent excessive lastdrop recursion.
	 */
	while (chain) {
		refs = chain->refs;
		__compiler_membar();
		KKASSERT(refs > 0);

		if (refs == 1) {
			if (hammer2_mtx_ex_try(&chain->lock) == 0)
				chain = hammer2_chain_lastdrop(chain, 1);
			/* Retry the same chain, or chain from lastdrop. */
		} else {
			if (atomic_cmpset_int(&chain->refs, refs, refs - 1))
				break;
			/* Retry the same chain. */
		}
		cpu_spinwait();
	}
	goto again;
}

/*
 * On last lock release.
 */
static hammer2_io_t *
hammer2_chain_drop_data(hammer2_chain_t *chain)
{
	hammer2_io_t *dio;

	if ((dio = chain->dio) != NULL) {
		chain->dio = NULL;
		chain->data = NULL;
	} else {
		switch (chain->bref.type) {
		case HAMMER2_BREF_TYPE_VOLUME:
		case HAMMER2_BREF_TYPE_FREEMAP:
			break;
		default:
			if (chain->data != NULL) {
				hammer2_spin_unex(&chain->core.spin);
				hpanic("chain data not NULL: "
				    "chain=%p refs=%d bref=%016jx.%02x "
				    "parent=%p dio=%p data=%p",
				    chain, chain->refs, chain->bref.data_off,
				    chain->bref.type, chain->parent, chain->dio,
				    chain->data);
			}
			KKASSERT(chain->data == NULL);
			break;
		}
	}
	return (dio);
}

/*
 * Lock a referenced chain element, acquiring its data with I/O if necessary,
 * and specify how you would like the data to be resolved.
 *
 * If an I/O or other fatal error occurs, chain->error will be set to non-zero.
 *
 * The lock is allowed to recurse, multiple locking ops will aggregate
 * the requested resolve types.  Once data is assigned it will not be
 * removed until the last unlock.
 *
 * HAMMER2_RESOLVE_MAYBE - Do not resolve data elements for DATA chains.
 *			   (typically used to avoid device/logical buffer
 *			    aliasing for data)
 *
 * HAMMER2_RESOLVE_ALWAYS- Always resolve the data element.
 *
 * HAMMER2_RESOLVE_SHARED- (flag) The chain is locked shared, otherwise
 *			   it will be locked exclusive.
 *
 * NOTE: Embedded elements (volume header, inodes) are always resolved
 *	 regardless.
 *
 * NOTE: (data) elements are normally locked RESOLVE_MAYBE
 *	 so as not to instantiate a device buffer, which could alias against
 *	 a logical file buffer.  However, if ALWAYS is specified the
 *	 device buffer will be instantiated anyway.
 *
 * NOTE: The return value is currently always 0.
 *
 * WARNING! This function blocks on I/O if data needs to be fetched.  This
 *	    blocking can run concurrent with other compatible lock holders
 *	    who do not need data returning.
 */
int
hammer2_chain_lock(hammer2_chain_t *chain, int how)
{
	KKASSERT(chain->refs > 0);

	/*
	 * Get the appropriate lock.  If LOCKAGAIN is flagged with
	 * SHARED the caller expects a shared lock to already be
	 * present and we are giving it another ref.  This case must
	 * importantly not block if there is a pending exclusive lock
	 * request.
	 */
	atomic_add_int(&chain->lockcnt, 1);
	if (how & HAMMER2_RESOLVE_SHARED) {
		if (how & HAMMER2_RESOLVE_LOCKAGAIN) {
			if (hammer2_mtx_owned(&chain->lock))
				hammer2_mtx_ex(&chain->lock);
			else
				hammer2_mtx_sh(&chain->lock);
		} else {
			hammer2_mtx_sh(&chain->lock);
		}
	} else {
		hammer2_mtx_ex(&chain->lock);
	}

	/*
	 * If we already have a valid data pointer no further action is
	 * necessary.
	 */
	if (chain->data)
		return (0);

	/*
	 * Do we have to resolve the data?  This is generally only
	 * applicable to HAMMER2_BREF_TYPE_DATA which is special-cased.
	 * Other bref types expects the data to be there.
	 */
	switch (how & HAMMER2_RESOLVE_MASK) {
	case HAMMER2_RESOLVE_MAYBE:
		if (chain->bref.type == HAMMER2_BREF_TYPE_DATA)
			return (0);
		/* fall through */
	case HAMMER2_RESOLVE_ALWAYS:
	default:
		break;
	}

	/* Caller requires data. */
	hammer2_chain_load_data(chain);

	return (0);
}

/*
 * Issue I/O and install chain->data.  Caller must hold a chain lock, lock
 * may be of any type.
 *
 * Once chain->data is set it cannot be disposed of until all locks are
 * released.
 */
static void
hammer2_chain_load_data(hammer2_chain_t *chain)
{
	hammer2_dev_t *hmp;
	hammer2_blockref_t *bref;
	char *bdata;
	int error;

	/*
	 * Degenerate case, data already present, or chain has no media
	 * reference to load.
	 */
	if (chain->data)
		return;
	if ((chain->bref.data_off & ~HAMMER2_OFF_MASK_RADIX) == 0)
		return;

	hmp = chain->hmp;
	KKASSERT(hmp != NULL);

	/*
	 * inp_lock protects HAMMER2_CHAIN_{IOINPROG,SIGNAL} bits.
	 * DragonFly uses tsleep_interlock(9) here without taking mutex.
	 */
	hammer2_mtx_ex(&chain->inp_lock);
again:
	if (chain->flags & HAMMER2_CHAIN_IOINPROG) {
		atomic_set_int(&chain->flags, HAMMER2_CHAIN_IOSIGNAL);
		hammer2_mtx_sleep(&chain->flags, &chain->inp_lock, "h2ch");
		goto again;
	}
	atomic_set_int(&chain->flags, HAMMER2_CHAIN_IOINPROG);
	hammer2_mtx_unlock(&chain->inp_lock);

	/*
	 * We own CHAIN_IOINPROG.
	 * Degenerate case if we raced another load.
	 */
	if (chain->data)
		goto done;

	/* We must resolve to a device buffer by issuing I/O. */
	bref = &chain->bref;
	error = hammer2_io_bread(hmp, bref->type, bref->data_off, chain->bytes,
	    &chain->dio);
	if (error) {
		chain->error = HAMMER2_ERROR_EIO;
		hprintf("bread error %d at data_off %016jx\n",
		    error, (intmax_t)bref->data_off);
		hammer2_io_bqrelse(&chain->dio);
		goto done;
	}
	chain->error = 0;

	bdata = hammer2_io_data(chain->dio, chain->bref.data_off);

	if ((chain->flags & HAMMER2_CHAIN_TESTEDGOOD) == 0) {
		if (hammer2_chain_testcheck(chain, bdata) == 0)
			chain->error = HAMMER2_ERROR_CHECK;
		else
			atomic_set_int(&chain->flags, HAMMER2_CHAIN_TESTEDGOOD);
	}

	switch (bref->type) {
	case HAMMER2_BREF_TYPE_VOLUME:
	case HAMMER2_BREF_TYPE_FREEMAP:
		hpanic("unresolved volume header");
		break;
	case HAMMER2_BREF_TYPE_DIRENT:
		KKASSERT(chain->bytes != 0);
		/* fall through */
	case HAMMER2_BREF_TYPE_INODE:
	case HAMMER2_BREF_TYPE_FREEMAP_LEAF:
	case HAMMER2_BREF_TYPE_INDIRECT:
	case HAMMER2_BREF_TYPE_DATA:
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
	default:
		/* Point data at the device buffer and leave dio intact. */
		chain->data = (void *)bdata;
		break;
	}
done:
	/* Release HAMMER2_CHAIN_IOINPROG and signal waiters if requested. */
	KKASSERT(chain->flags & HAMMER2_CHAIN_IOINPROG);
	hammer2_mtx_ex(&chain->inp_lock);
	atomic_clear_int(&chain->flags, HAMMER2_CHAIN_IOINPROG);
	if (chain->flags & HAMMER2_CHAIN_IOSIGNAL) {
		atomic_clear_int(&chain->flags, HAMMER2_CHAIN_IOSIGNAL);
		hammer2_mtx_wakeup(&chain->flags);
	}
	hammer2_mtx_unlock(&chain->inp_lock);
}

/*
 * Unlock and deref a chain element.
 *
 * Remember that the presence of children under chain prevent the chain's
 * destruction but do not add additional references, so the dio will still
 * be dropped.
 */
void
hammer2_chain_unlock(hammer2_chain_t *chain)
{
	hammer2_io_t *dio;
	unsigned int lockcnt;
	int iter = 0;

	/*
	 * If multiple locks are present (or being attempted) on this
	 * particular chain we can just unlock, drop refs, and return.
	 *
	 * Otherwise fall-through on the 1->0 transition.
	 */
	for (;;) {
		lockcnt = chain->lockcnt;
		KKASSERT(lockcnt > 0);
		__compiler_membar();

		if (lockcnt > 1) {
			if (atomic_cmpset_int(&chain->lockcnt, lockcnt,
			    lockcnt - 1)) {
				hammer2_mtx_unlock(&chain->lock);
				return;
			}
		} else if (hammer2_mtx_owned(&chain->lock) ||
		    hammer2_mtx_upgrade_try(&chain->lock) == 0) {
			/* While holding the mutex exclusively. */
			if (atomic_cmpset_int(&chain->lockcnt, 1, 0))
				break;
		} else {
			/*
			 * This situation can easily occur on SMP due to
			 * the gap inbetween the 1->0 transition and the
			 * final unlock.  We cannot safely block on the
			 * mutex because lockcnt might go above 1.
			 */
			if (++iter > 1000) {
				if (iter > 1000 + hz) {
					hprintf("h2race2\n");
					iter = 1000;
				}
				pause("h2race2", 1);
			}
			cpu_spinwait();
		}
	}

	/*
	 * Last unlock / mutex upgraded to exclusive.  Drop the data
	 * reference.
	 */
	dio = hammer2_chain_drop_data(chain);
	if (dio)
		hammer2_io_bqrelse(&dio);
	hammer2_mtx_unlock(&chain->lock);
}

/*
 * This calculates the point at which all remaining blockrefs are empty.
 * This routine can only be called on a live chain.
 *
 * Caller holds the chain locked, but possibly with a shared lock.  We
 * must use an exclusive spinlock to prevent corruption.
 *
 * NOTE: Flag is not set until after the count is complete, allowing
 *	 callers to test the flag without holding the spinlock.
 */
static void
hammer2_chain_countbrefs(hammer2_chain_t *chain, hammer2_blockref_t *base,
    int count)
{
	hammer2_mtx_assert_locked(&chain->lock);

	hammer2_spin_ex(&chain->core.spin);
	if ((chain->flags & HAMMER2_CHAIN_COUNTEDBREFS) == 0) {
		if (base) {
			while (--count >= 0)
				if (base[count].type != HAMMER2_BREF_TYPE_EMPTY)
					break;
			chain->core.live_zero = count + 1;
		} else {
			chain->core.live_zero = 0;
		}
		atomic_set_int(&chain->flags, HAMMER2_CHAIN_COUNTEDBREFS);
	}
	hammer2_spin_unex(&chain->core.spin);
}

/*
 * This function returns the chain at the nearest key within the specified
 * range.  The returned chain will be referenced but not locked.
 *
 * This function will recurse through chain->rbtree as necessary and will
 * return a *key_nextp suitable for iteration.  *key_nextp is only set if
 * the iteration value is less than the current value of *key_nextp.
 *
 * The caller should use (*key_nextp) to calculate the actual range of
 * the returned element, which will be (key_beg to *key_nextp - 1), because
 * there might be another element which is superior to the returned element
 * and overlaps it.
 *
 * (*key_nextp) can be passed as key_beg in an iteration only while non-NULL
 * chains continue to be returned.  On EOF (*key_nextp) may overflow since
 * it will wind up being (key_end + 1).
 *
 * WARNING!  Must be called with child's spinlock held.  Spinlock remains
 *	     held through the operation.
 */
struct hammer2_chain_find_info {
	hammer2_chain_t		*best;
	hammer2_key_t		key_beg;
	hammer2_key_t		key_end;
	hammer2_key_t		key_next;
};

static int hammer2_chain_find_cmp(hammer2_chain_t *, void *);
static int hammer2_chain_find_callback(hammer2_chain_t *, void *);

static hammer2_chain_t *
hammer2_chain_find(hammer2_chain_t *parent, hammer2_key_t *key_nextp,
    hammer2_key_t key_beg, hammer2_key_t key_end)
{
	struct hammer2_chain_find_info info;

	info.best = NULL;
	info.key_beg = key_beg;
	info.key_end = key_end;
	info.key_next = *key_nextp;

	RB_SCAN(hammer2_chain_tree, &parent->core.rbtree,
	    hammer2_chain_find_cmp, hammer2_chain_find_callback, &info);
	*key_nextp = info.key_next;

	return (info.best);
}

static int
hammer2_chain_find_cmp(hammer2_chain_t *child, void *data)
{
	struct hammer2_chain_find_info *info = data;
	hammer2_key_t child_beg, child_end;

	child_beg = child->bref.key;
	child_end = child_beg + ((hammer2_key_t)1 << child->bref.keybits) - 1;

	if (child_end < info->key_beg)
		return (-1);
	if (child_beg > info->key_end)
		return (1);
	return (0);
}

static int
hammer2_chain_find_callback(hammer2_chain_t *child, void *data)
{
	struct hammer2_chain_find_info *info = data;
	hammer2_chain_t *best;
	hammer2_key_t child_end;

	if ((best = info->best) == NULL) {
		/* No previous best.  Assign best. */
		info->best = child;
	} else if (best->bref.key <= info->key_beg &&
	    child->bref.key <= info->key_beg) {
		/* Illegal overlap. */
		KKASSERT(0);
	} else if (child->bref.key < best->bref.key) {
		/*
		 * Child has a nearer key and best is not flush with key_beg.
		 * Set best to child.  Truncate key_next to the old best key.
		 */
		info->best = child;
		if (info->key_next > best->bref.key || info->key_next == 0)
			info->key_next = best->bref.key;
	} else if (child->bref.key == best->bref.key) {
		/*
		 * If our current best is flush with the child then this
		 * is an illegal overlap.
		 *
		 * key_next will automatically be limited to the smaller of
		 * the two end-points.
		 */
		KKASSERT(0);
	} else {
		/*
		 * Keep the current best but truncate key_next to the child's
		 * base.
		 *
		 * key_next will also automatically be limited to the smaller
		 * of the two end-points (probably not necessary for this case
		 * but we do it anyway).
		 */
		if (info->key_next > child->bref.key || info->key_next == 0)
			info->key_next = child->bref.key;
	}

	/* Always truncate key_next based on child's end-of-range. */
	child_end = child->bref.key + ((hammer2_key_t)1 << child->bref.keybits);
	if (child_end && (info->key_next > child_end || info->key_next == 0))
		info->key_next = child_end;

	return (0);
}

/*
 * Retrieve the specified chain from a media blockref, creating the
 * in-memory chain structure which reflects it.  The returned chain is
 * held and locked according to (how) (HAMMER2_RESOLVE_*).  The caller must
 * handle crc-checks and so forth, and should check chain->error before
 * assuming that the data is good.
 *
 * To handle insertion races pass the INSERT_RACE flag along with the
 * generation number of the core.  NULL will be returned if the generation
 * number changes before we have a chance to insert the chain.  Insert
 * races can occur because the parent might be held shared.
 *
 * Caller must hold the parent locked shared or exclusive since we may
 * need the parent's bref array to find our block.
 *
 * WARNING! chain->pmp is always set to NULL for any chain representing
 *	    part of the super-root topology.
 */
static hammer2_chain_t *
hammer2_chain_get(hammer2_chain_t *parent, int generation,
    hammer2_blockref_t *bref, int how)
{
	hammer2_dev_t *hmp = parent->hmp;
	hammer2_chain_t *chain;
	int error;

	hammer2_mtx_assert_locked(&parent->lock);

	/*
	 * Allocate a chain structure representing the existing media
	 * entry.  Resulting chain has one ref and is not locked.
	 */
	if (bref->flags & HAMMER2_BREF_FLAG_PFSROOT)
		chain = hammer2_chain_alloc(hmp, NULL, bref);
	else
		chain = hammer2_chain_alloc(hmp, parent->pmp, bref);
	/* Ref'd chain returned. */

	/* Chain must be locked to avoid unexpected ripouts. */
	hammer2_chain_lock(chain, how);

	/*
	 * Link the chain into its parent.  A spinlock is required to safely
	 * access the RBTREE, and it is possible to collide with another
	 * hammer2_chain_get() operation because the caller might only hold
	 * a shared lock on the parent.
	 */
	KKASSERT(parent->refs > 0);
	error = hammer2_chain_insert(parent, chain, generation);
	if (error) {
		KKASSERT((chain->flags & HAMMER2_CHAIN_ONRBTREE) == 0);
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
		chain = NULL;
	} else {
		KKASSERT(chain->flags & HAMMER2_CHAIN_ONRBTREE);
	}

	/*
	 * Return our new chain referenced but not locked, or NULL if
	 * a race occurred.
	 */
	return (chain);
}

/*
 * Lookup initialization/completion API.
 */
hammer2_chain_t *
hammer2_chain_lookup_init(hammer2_chain_t *parent, int flags)
{
	hammer2_chain_ref(parent);

	if (flags & HAMMER2_LOOKUP_SHARED)
		hammer2_chain_lock(parent,
		    HAMMER2_RESOLVE_ALWAYS | HAMMER2_RESOLVE_SHARED);
	else
		hammer2_chain_lock(parent, HAMMER2_RESOLVE_ALWAYS);

	return (parent);
}

void
hammer2_chain_lookup_done(hammer2_chain_t *parent)
{
	if (parent) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
	}
}

/*
 * Take the locked chain and return a locked parent.  The chain is unlocked
 * and dropped.  *chainp is set to the returned parent as a convenience.
 * Pass HAMMER2_RESOLVE_* flags in flags.
 *
 * This will work even if the chain is errored, and the caller can check
 * parent->error on return if desired since the parent will be locked.
 */
static hammer2_chain_t *
hammer2_chain_repparent(hammer2_chain_t **chainp, int flags)
{
	hammer2_chain_t *chain, *parent;

	chain = *chainp;
	hammer2_mtx_assert_locked(&chain->lock);

	parent = chain->parent;
	KKASSERT(parent);

	hammer2_chain_ref(parent);
	/* DragonFly uses flags|HAMMER2_RESOLVE_NONBLOCK followed by reptrack */
	hammer2_chain_lock(parent, flags);

	hammer2_chain_unlock(chain);
	hammer2_chain_drop(chain);
	*chainp = parent;

	return (parent);
}

/*
 * Locate the first chain whos key range overlaps (key_beg, key_end) inclusive.
 * (*parentp) typically points to an inode but can also point to a related
 * indirect block and this function will recurse upwards and find the inode
 * or the nearest undeleted indirect block covering the key range.
 *
 * This function unconditionally sets *errorp, replacing any previous value.
 *
 * (*parentp) must be exclusive or shared locked (depending on flags) and
 * referenced and can be an inode or an existing indirect block within the
 * inode.
 *
 * If (*parent) is errored out, this function will not attempt to recurse
 * the radix tree and will return NULL along with an appropriate *errorp.
 * If NULL is returned and *errorp is 0, the requested lookup could not be
 * located.
 *
 * On return (*parentp) will be modified to point at the deepest parent chain
 * element encountered during the search, as a helper for an insertion or
 * deletion.
 *
 * The new (*parentp) will be locked shared or exclusive (depending on flags),
 * and referenced, and the old will be unlocked and dereferenced (no change
 * if they are both the same).  This is particularly important if the caller
 * wishes to insert a new chain, (*parentp) will be set properly even if NULL
 * is returned, as long as no error occurred.
 *
 * The matching chain will be returned locked according to flags.
 *
 * --
 * NULL is returned if no match was found, but (*parentp) will still
 * potentially be adjusted.
 *
 * On return (*key_nextp) will point to an iterative value for key_beg.
 * (If NULL is returned (*key_nextp) is set to (key_end + 1)).
 *
 * This function will also recurse up the chain if the key is not within the
 * current parent's range.  (*parentp) can never be set to NULL.  An iteration
 * can simply allow (*parentp) to float inside the loop.
 *
 * NOTE!  chain->data is not always resolved.  By default it will not be
 *	  resolved for BREF_TYPE_DATA, FREEMAP_NODE, or FREEMAP_LEAF.  Use
 *	  HAMMER2_LOOKUP_ALWAYS to force resolution (but be careful w/
 *	  BREF_TYPE_DATA as the device buffer can alias the logical file
 *	  buffer).
 */
hammer2_chain_t *
hammer2_chain_lookup(hammer2_chain_t **parentp, hammer2_key_t *key_nextp,
    hammer2_key_t key_beg, hammer2_key_t key_end, int *errorp, int flags)
{
	hammer2_chain_t *chain, *parent;
	hammer2_blockref_t bsave, *base, *bref;
	hammer2_key_t scan_beg, scan_end;
	int how_always = HAMMER2_RESOLVE_ALWAYS;
	int how_maybe = HAMMER2_RESOLVE_MAYBE;
	int how, count, generation, maxloops = 300000;

	if (flags & HAMMER2_LOOKUP_ALWAYS) {
		how_maybe = how_always;
		how = HAMMER2_RESOLVE_ALWAYS;
	} else {
		how = HAMMER2_RESOLVE_MAYBE;
	}
	if (flags & HAMMER2_LOOKUP_SHARED) {
		how_maybe |= HAMMER2_RESOLVE_SHARED;
		how_always |= HAMMER2_RESOLVE_SHARED;
		how |= HAMMER2_RESOLVE_SHARED;
	}

	/*
	 * Recurse (*parentp) upward if necessary until the parent completely
	 * encloses the key range or we hit the inode.
	 */
	parent = *parentp;
	hammer2_mtx_assert_locked(&parent->lock);
	*errorp = 0;

	while (parent->bref.type == HAMMER2_BREF_TYPE_INDIRECT ||
	    parent->bref.type == HAMMER2_BREF_TYPE_FREEMAP_NODE) {
		scan_beg = parent->bref.key;
		scan_end = scan_beg +
		    ((hammer2_key_t)1 << parent->bref.keybits) - 1;
		if (key_beg >= scan_beg && key_end <= scan_end)
			break;
		parent = hammer2_chain_repparent(parentp, how_maybe);
	}
again:
	if (--maxloops == 0)
		hpanic("maxloops");

	/*
	 * No lookup is possible if the parent is errored.  We delayed
	 * this check as long as we could to ensure that the parent backup,
	 * embedded data code could still execute.
	 */
	if (parent->error) {
		*errorp = parent->error;
		return (NULL);
	}

	/*
	 * Locate the blockref array.  Currently we do a fully associative
	 * search through the array.
	 */
	switch (parent->bref.type) {
	case HAMMER2_BREF_TYPE_INODE:
		/*
		 * Special shortcut for embedded data returns the inode
		 * itself.  Callers must detect this condition and access
		 * the embedded data (the strategy code does this for us).
		 *
		 * This is only applicable to regular files and softlinks.
		 *
		 * We need a second lock on parent.  Since we already have
		 * a lock we must pass LOCKAGAIN to prevent unexpected
		 * blocking (we don't want to block on a second shared
		 * ref if an exclusive lock is pending)
		 */
		if (parent->data->ipdata.meta.op_flags &
		    HAMMER2_OPFLAG_DIRECTDATA) {
			hammer2_chain_ref(parent);
			hammer2_chain_lock(parent,
			    how_always | HAMMER2_RESOLVE_LOCKAGAIN);
			*key_nextp = key_end + 1;
			return (parent);
		}
		base = &parent->data->ipdata.u.blockset.blockref[0];
		count = HAMMER2_SET_COUNT;
		break;
	case HAMMER2_BREF_TYPE_FREEMAP_NODE:
	case HAMMER2_BREF_TYPE_INDIRECT:
		KKASSERT(parent->data);
		base = &parent->data->npdata[0];
		count = parent->bytes / sizeof(hammer2_blockref_t);
		break;
	case HAMMER2_BREF_TYPE_VOLUME:
		base = &parent->data->voldata.sroot_blockset.blockref[0];
		count = HAMMER2_SET_COUNT;
		break;
	case HAMMER2_BREF_TYPE_FREEMAP:
		base = &parent->data->blkset.blockref[0];
		count = HAMMER2_SET_COUNT;
		break;
	default:
		hpanic("bad blockref type %d", parent->bref.type);
		break;
	}

	/*
	 * Merged scan to find next candidate.
	 *
	 * hammer2_base_*() functions require the parent->core.live_* fields
	 * to be synchronized.
	 *
	 * We need to hold the spinlock to access the block array and RB tree
	 * and to interlock chain creation.
	 */
	if ((parent->flags & HAMMER2_CHAIN_COUNTEDBREFS) == 0)
		hammer2_chain_countbrefs(parent, base, count);

	/* Combined search. */
	hammer2_spin_ex(&parent->core.spin);
	chain = hammer2_combined_find(parent, base, count, key_nextp, key_beg,
	    key_end, &bref);
	generation = parent->core.generation;

	/* Exhausted parent chain, iterate. */
	if (bref == NULL) {
		KKASSERT(chain == NULL);
		hammer2_spin_unex(&parent->core.spin);
		if (key_beg == key_end)	/* Short cut single-key case. */
			return (NULL);

		/* Stop if we reached the end of the iteration. */
		if (parent->bref.type != HAMMER2_BREF_TYPE_INDIRECT &&
		    parent->bref.type != HAMMER2_BREF_TYPE_FREEMAP_NODE)
			return (NULL);

		/*
		 * Calculate next key, stop if we reached the end of the
		 * iteration, otherwise go up one level and loop.
		 */
		key_beg = parent->bref.key +
		    ((hammer2_key_t)1 << parent->bref.keybits);
		if (key_beg == 0 || key_beg > key_end)
			return (NULL);
		parent = hammer2_chain_repparent(parentp, how_maybe);
		goto again;
	}

	/* Selected from blockref or in-memory chain. */
	bsave = *bref;
	if (chain == NULL) {
		hammer2_spin_unex(&parent->core.spin);
		if (bsave.type == HAMMER2_BREF_TYPE_INDIRECT ||
		    bsave.type == HAMMER2_BREF_TYPE_FREEMAP_NODE)
			chain = hammer2_chain_get(parent, generation, &bsave,
			    how_maybe);
		else
			chain = hammer2_chain_get(parent, generation, &bsave,
			    how);
		if (chain == NULL)
			goto again;
	} else {
		hammer2_chain_ref(chain);
		hammer2_spin_unex(&parent->core.spin);
		/*
		 * chain is referenced but not locked.  We must lock the
		 * chain to obtain definitive state.
		 */
		if (bsave.type == HAMMER2_BREF_TYPE_INDIRECT ||
		    bsave.type == HAMMER2_BREF_TYPE_FREEMAP_NODE)
			hammer2_chain_lock(chain, how_maybe);
		else
			hammer2_chain_lock(chain, how);
		KKASSERT(chain->parent == parent);
	}
	if (bcmp(&bsave, &chain->bref, sizeof(bsave)) ||
	    chain->parent != parent) {
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
		chain = NULL;
		goto again;
	}

	/*
	 * If the chain element is an indirect block it becomes the new
	 * parent and we loop on it.
	 *
	 * The parent always has to be locked with at least RESOLVE_MAYBE
	 * so we can access its data.  It might need a fixup if the caller
	 * passed incompatible flags.  Be careful not to cause a deadlock
	 * as a data-load requires an exclusive lock.
	 */
	if (chain->bref.type == HAMMER2_BREF_TYPE_INDIRECT ||
	    chain->bref.type == HAMMER2_BREF_TYPE_FREEMAP_NODE) {
		hammer2_chain_unlock(parent);
		hammer2_chain_drop(parent);
		*parentp = parent = chain;
		chain = NULL;
		goto again;
	}

	/*
	 * All done, return the locked chain.
	 *
	 * NOTE! A chain->error must be tested by the caller upon return.
	 *	 *errorp is only set based on issues which occur while
	 *	 trying to reach the chain.
	 */
	return (chain);
}

/*
 * After having issued a lookup we can iterate all matching keys.
 *
 * If chain is non-NULL we continue the iteration from just after it's index.
 * If chain is NULL we assume the parent was exhausted and continue the
 * iteration at the next parent.
 *
 * If a fatal error occurs (typically an I/O error), a dummy chain is
 * returned with chain->error and error-identifying information set.  This
 * chain will assert if you try to do anything fancy with it.
 *
 * XXX Depending on where the error occurs we should allow continued iteration.
 *
 * parent must be locked on entry and remains locked throughout.  chain's
 * lock status must match flags.  Chain is always at least referenced.
 */
hammer2_chain_t *
hammer2_chain_next(hammer2_chain_t **parentp, hammer2_chain_t *chain,
    hammer2_key_t *key_nextp, hammer2_key_t key_beg, hammer2_key_t key_end,
    int *errorp, int flags)
{
	hammer2_chain_t *parent;
	int how_maybe;

	/* Calculate locking flags for upward recursion. */
	how_maybe = HAMMER2_RESOLVE_MAYBE;
	if (flags & HAMMER2_LOOKUP_SHARED)
		how_maybe |= HAMMER2_RESOLVE_SHARED;

	parent = *parentp;
	hammer2_mtx_assert_locked(&parent->lock);
	*errorp = 0;

	/* Calculate the next index and recalculate the parent if necessary. */
	if (chain) {
		key_beg = chain->bref.key +
		    ((hammer2_key_t)1 << chain->bref.keybits);
		hammer2_chain_unlock(chain);
		hammer2_chain_drop(chain);
		/*
		 * chain invalid past this point, but we can still do a
		 * pointer comparison w/parent.
		 *
		 * Any scan where the lookup returned degenerate data embedded
		 * in the inode has an invalid index and must terminate.
		 */
		if (chain == parent)
			return (NULL);
		if (key_beg == 0 || key_beg > key_end)
			return (NULL);
		chain = NULL;
	} else if (parent->bref.type != HAMMER2_BREF_TYPE_INDIRECT &&
		   parent->bref.type != HAMMER2_BREF_TYPE_FREEMAP_NODE) {
		/* We reached the end of the iteration. */
		return (NULL);
	} else {
		/*
		 * Continue iteration with next parent unless the current
		 * parent covers the range.
		 */
		key_beg = parent->bref.key +
		    ((hammer2_key_t)1 << parent->bref.keybits);
		if (key_beg == 0 || key_beg > key_end)
			return (NULL);
		parent = hammer2_chain_repparent(parentp, how_maybe);
	}

	/* And execute. */
	return (hammer2_chain_lookup(parentp, key_nextp, key_beg, key_end,
	    errorp, flags));
}

/*
 * Returns the index of the nearest element in the blockref array >= elm.
 * Returns (count) if no element could be found.
 *
 * Sets *key_nextp to the next key for loop purposes but does not modify
 * it if the next key would be higher than the current value of *key_nextp.
 * Note that *key_nexp can overflow to 0, which should be tested by the
 * caller.
 *
 * WARNING!  Must be called with parent's spinlock held.  Spinlock remains
 *	     held through the operation.
 */
static int
hammer2_base_find(hammer2_chain_t *parent, hammer2_blockref_t *base, int count,
    hammer2_key_t *key_nextp, hammer2_key_t key_beg, hammer2_key_t key_end)
{
	hammer2_blockref_t *scan;
	hammer2_key_t scan_end;
	int i, limit;

	/*
	 * Require the live chain's already have their core's counted
	 * so we can optimize operations.
	 */
	KKASSERT(parent->flags & HAMMER2_CHAIN_COUNTEDBREFS);

	/* Degenerate case. */
	if (count == 0 || base == NULL)
		return (count);

	/*
	 * Sequential optimization using parent->cache_index.  This is
	 * the most likely scenario.
	 *
	 * We can avoid trailing empty entries on live chains, otherwise
	 * we might have to check the whole block array.
	 */
	i = parent->cache_index;	/* SMP RACE OK */
	__compiler_membar();
	limit = parent->core.live_zero;
	if (i >= limit)
		i = limit - 1;
	if (i < 0)
		i = 0;
	KKASSERT(i < count);

	/* Search backwards. */
	scan = &base[i];
	while (i > 0 && (scan->type == HAMMER2_BREF_TYPE_EMPTY ||
	    scan->key > key_beg)) {
		--scan;
		--i;
	}
	parent->cache_index = i;

	/*
	 * Search forwards, stop when we find a scan element which
	 * encloses the key or until we know that there are no further
	 * elements.
	 */
	while (i < count) {
		if (scan->type != HAMMER2_BREF_TYPE_EMPTY) {
			scan_end = scan->key +
			    ((hammer2_key_t)1 << scan->keybits) - 1;
			if (scan->key > key_beg || scan_end >= key_beg)
				break;
		}
		if (i >= limit)
			return (count);
		++scan;
		++i;
	}
	if (i != count) {
		parent->cache_index = i;
		if (i >= limit) {
			i = count;
		} else {
			scan_end = scan->key +
			    ((hammer2_key_t)1 << scan->keybits);
			if (scan_end && (*key_nextp > scan_end ||
			    *key_nextp == 0))
				*key_nextp = scan_end;
		}
	}
	return (i);
}

/*
 * Do a combined search and return the next match either from the blockref
 * array or from the in-memory chain.  Sets *brefp to the returned bref in
 * both cases, or sets it to NULL if the search exhausted.  Only returns
 * a non-NULL chain if the search matched from the in-memory chain.
 *
 * When no in-memory chain has been found and a non-NULL bref is returned
 * in *brefp.
 *
 * The returned chain is not locked or referenced.  Use the returned bref
 * to determine if the search exhausted or not.  Iterate if the base find
 * is chosen but matches a deleted chain.
 *
 * WARNING!  Must be called with parent's spinlock held.  Spinlock remains
 *	     held through the operation.
 */
static hammer2_chain_t *
hammer2_combined_find(hammer2_chain_t *parent, hammer2_blockref_t *base,
    int count, hammer2_key_t *key_nextp, hammer2_key_t key_beg,
    hammer2_key_t key_end, hammer2_blockref_t **brefp)
{
	hammer2_chain_t *chain;
	hammer2_blockref_t *bref;
	int i;

	/* Lookup in block array and in rbtree. */
	*key_nextp = key_end + 1;
	i = hammer2_base_find(parent, base, count, key_nextp, key_beg, key_end);
	chain = hammer2_chain_find(parent, key_nextp, key_beg, key_end);

	/* Neither matched. */
	if (i == count && chain == NULL) {
		*brefp = NULL;
		return (NULL);
	}

	/* Only chain matched. */
	if (i == count) {
		bref = &chain->bref;
		goto found;
	}

	/* Only blockref matched. */
	if (chain == NULL) {
		bref = &base[i];
		goto found;
	}

	/*
	 * Both in-memory and blockref matched, select the nearer element.
	 *
	 * If both are flush with the left-hand side or both are the
	 * same distance away, select the chain.  In this situation the
	 * chain must have been loaded from the matching blockmap.
	 */
	if ((chain->bref.key <= key_beg && base[i].key <= key_beg) ||
	    chain->bref.key == base[i].key) {
		KKASSERT(chain->bref.key == base[i].key);
		bref = &chain->bref;
		goto found;
	}

	/* Select the nearer key. */
	if (chain->bref.key < base[i].key) {
		bref = &chain->bref;
	} else {
		bref = &base[i];
		chain = NULL;
	}

	/* If the bref is out of bounds we've exhausted our search. */
found:
	if (bref->key > key_end) {
		*brefp = NULL;
		chain = NULL;
	} else {
		*brefp = bref;
	}
	return (chain);
}

/*
 * Returns non-zero on success, 0 on failure.
 */
static int
hammer2_chain_testcheck(const hammer2_chain_t *chain, void *bdata)
{
	static int count = 0;
	int r = 0;

	switch (HAMMER2_DEC_CHECK(chain->bref.methods)) {
	case HAMMER2_CHECK_NONE:
	case HAMMER2_CHECK_DISABLED:
		r = 1;
		break;
	case HAMMER2_CHECK_ISCSI32:
		r = chain->bref.check.iscsi32.value ==
		    hammer2_icrc32(bdata, chain->bytes);
		break;
	case HAMMER2_CHECK_XXHASH64:
		r = chain->bref.check.xxhash64.value ==
		    XXH64(bdata, chain->bytes, XXH_HAMMER2_SEED);
		break;
	case HAMMER2_CHECK_SHA192:
		{
		SHA256_CTX hash_ctx;
		union {
			uint8_t digest[SHA256_DIGEST_LENGTH];
			uint64_t digest64[SHA256_DIGEST_LENGTH/8];
		} u;

		SHA256_Init(&hash_ctx);
		SHA256_Update(&hash_ctx, bdata, chain->bytes);
		SHA256_Final(u.digest, &hash_ctx);
		u.digest64[2] ^= u.digest64[3];
		r = bcmp(u.digest, chain->bref.check.sha192.data,
		    sizeof(chain->bref.check.sha192.data)) == 0;
		}
		break;
	case HAMMER2_CHECK_FREEMAP:
		r = chain->bref.check.freemap.icrc32 ==
		    hammer2_icrc32(bdata, chain->bytes);
		break;
	default:
		hpanic("unknown check type %02x\n", chain->bref.methods);
		break;
	}

	if (r == 0 && count < 1000) {
		hprintf("failed: chain %s %016jx %016jx/%-2d meth=%02x "
		    "mir=%016jx mod=%016jx flags=%08x\n",
		    hammer2_breftype_to_str(chain->bref.type),
		    chain->bref.data_off, chain->bref.key, chain->bref.keybits,
		    chain->bref.methods, chain->bref.mirror_tid,
		    chain->bref.modify_tid, chain->flags);
		count++;
		if (count >= 1000)
			hprintf("gave up\n");
	}

	return (r);
}

/*
 * Acquire the chain and parent representing the specified inode for the
 * device at the specified cluster index.
 *
 * The flags passed in are LOOKUP flags, not RESOLVE flags.
 *
 * If we are unable to locate the inode, HAMMER2_ERROR_EIO or HAMMER2_ERROR_CHECK
 * is returned.  In case of error, *chainp and/or *parentp may still be returned
 * non-NULL.
 *
 * The caller may pass-in a locked *parentp and/or *chainp, or neither.
 * They will be unlocked and released by this function.  The *parentp and
 * *chainp representing the located inode are returned locked.
 *
 * The returned error includes any error on the returned chain in addition to
 * errors incurred while trying to lookup the inode.
 */
int
hammer2_chain_inode_find(hammer2_pfs_t *pmp, hammer2_key_t inum, int clindex,
    int flags, hammer2_chain_t **parentp, hammer2_chain_t **chainp)
{
	hammer2_inode_t *ip;
	hammer2_chain_t *parent, *rchain;
	hammer2_key_t key_dummy;
	int resolve_flags, error;

	resolve_flags = (flags & HAMMER2_LOOKUP_SHARED) ?
	    HAMMER2_RESOLVE_SHARED : 0;

	/* Caller expects us to replace these. */
	if (*chainp) {
		hammer2_chain_unlock(*chainp);
		hammer2_chain_drop(*chainp);
		*chainp = NULL;
	}
	if (*parentp) {
		hammer2_chain_unlock(*parentp);
		hammer2_chain_drop(*parentp);
		*parentp = NULL;
	}

	/*
	 * Be very careful, this is a backend function and we CANNOT
	 * lock any frontend inode structure we find.  But we have to
	 * look the inode up this way first in case it exists but is
	 * detached from the radix tree.
	 */
	ip = hammer2_inode_lookup(pmp, inum);
	if (ip) {
		*chainp = hammer2_inode_chain_and_parent(ip, clindex, parentp,
		    resolve_flags);
		hammer2_inode_drop(ip);
		if (*chainp)
			return ((*chainp)->error);
		hammer2_chain_unlock(*chainp);
		hammer2_chain_drop(*chainp);
		*chainp = NULL;
		if (*parentp) {
			hammer2_chain_unlock(*parentp);
			hammer2_chain_drop(*parentp);
			*parentp = NULL;
		}
	}

	/*
	 * Inodes hang off of the iroot (bit 63 is clear, differentiating
	 * inodes from root directory entries in the key lookup).
	 */
	parent = hammer2_inode_chain(pmp->iroot, clindex, resolve_flags);
	rchain = NULL;
	if (parent) {
		/*
		 * NOTE: rchain can be returned as NULL even if error == 0
		 *	 (i.e. not found)
		 */
		rchain = hammer2_chain_lookup(&parent, &key_dummy, inum, inum,
		    &error, flags);
		/*
		 * Propagate a chain-specific error to caller.
		 *
		 * If the chain is not errored, we must still validate that the inode
		 * number is correct, because all hell will break loose if it isn't
		 * correct.  It should always be correct so print to the console and
		 * simulate a CHECK error if it is not.
		 */
		if (error == 0 && rchain) {
			error = rchain->error;
			if (error == 0 && rchain->data)
				if (inum != rchain->data->ipdata.meta.inum) {
					hprintf("lookup inum %ju, got valid "
					    "inode but with inum %ju\n",
					    inum,
					    rchain->data->ipdata.meta.inum);
					error = HAMMER2_ERROR_CHECK;
					rchain->error = error;
				}
		}
	} else {
		error = HAMMER2_ERROR_EIO;
	}
	*parentp = parent;
	*chainp = rchain;

	return (error);
}

/*
 * Returns non-zero if the chain (INODE or DIRENT) matches the filename.
 */
int
hammer2_chain_dirent_test(const hammer2_chain_t *chain, const char *name,
    size_t name_len)
{
	const hammer2_inode_data_t *ripdata;

	if (chain->bref.type == HAMMER2_BREF_TYPE_INODE) {
		ripdata = &chain->data->ipdata;
		if (ripdata->meta.name_len == name_len &&
		    bcmp(ripdata->filename, name, name_len) == 0)
			return (1);
	}
	if (chain->bref.type == HAMMER2_BREF_TYPE_DIRENT &&
	    chain->bref.embed.dirent.namlen == name_len) {
		if (name_len > sizeof(chain->bref.check.buf) &&
		    bcmp(chain->data->buf, name, name_len) == 0)
			return (1);
		if (name_len <= sizeof(chain->bref.check.buf) &&
		    bcmp(chain->bref.check.buf, name, name_len) == 0)
			return (1);
	}

	return (0);
}

void
hammer2_dump_chain(hammer2_chain_t *chain, int tab, int bi, int *countp,
    char pfx, unsigned int flags)
{
	hammer2_chain_t *scan, *parent;
	int i;

	--*countp;
	if (*countp == 0) {
		printf("%*.*s...\n", tab, tab, "");
		return;
	}
	if (*countp < 0)
		return;

	printf("%*.*s%c-chain %p %s.%-3d %016jx %016jx/%-2d mir=%016jx "
	    "mod=%016jx\n",
	    tab, tab, "", pfx, chain,
	    hammer2_breftype_to_str(chain->bref.type), bi,
	    chain->bref.data_off, chain->bref.key, chain->bref.keybits,
	    chain->bref.mirror_tid, chain->bref.modify_tid);

	printf("%*.*s      [%08x] (%s) refs=%d",
	    tab, tab, "", chain->flags,
	    (chain->bref.type == HAMMER2_BREF_TYPE_INODE && chain->data) ?
	    (char *)chain->data->ipdata.filename : "?",
	    chain->refs);

	parent = chain->parent;
	if (parent)
		printf("\n%*.*s      p=%p [pflags %08x prefs %d]",
		    tab, tab, "", parent, parent->flags, parent->refs);

	if (RB_EMPTY(&chain->core.rbtree)) {
		printf("\n");
	} else {
		i = 0;
		printf(" {\n");
		RB_FOREACH(scan, hammer2_chain_tree, &chain->core.rbtree) {
			if ((scan->flags & flags) || flags == (unsigned int)-1)
				hammer2_dump_chain(scan, tab + 4, i, countp,
				    'a', flags);
			i++;
		}
		if (chain->bref.type == HAMMER2_BREF_TYPE_INODE && chain->data)
			printf("%*.*s}(%s)\n", tab, tab, "",
			    chain->data->ipdata.filename);
		else
			printf("%*.*s}\n", tab, tab, "");
	}
}
