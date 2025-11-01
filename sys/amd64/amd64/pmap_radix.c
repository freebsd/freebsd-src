/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 EMC Corp.
 * Copyright (c) 2011 Jeffrey Roberson <jeff@freebsd.org>
 * Copyright (c) 2008 Mayur Shardul <mayur.shardul@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/_pctrie.h>

#include <sys/pctrie.h>

void		ptpage_radix_wait(void);
void		ptpage_radix_zinit(void);
void		*ptpage_radix_node_alloc(struct pctrie *ptree);
void		ptpage_radix_node_free(struct pctrie *ptree, void *node);
smr_t		ptpage_radix_smr;

static __inline void
ptpage_radix_init(struct ptpage_radix *rtree)
{
	pctrie_init(&rtree->rt_trie);
}

static __inline bool
ptpage_radix_is_empty(struct ptpage_radix *rtree)
{
	return (pctrie_is_empty(&rtree->rt_trie));
}

PCTRIE_DEFINE_SMR(PTPAGE_RADIX, ptpage, pindex, ptpage_radix_node_alloc,
    ptpage_radix_node_free, ptpage_radix_smr);

/*
 * Inserts the key-value pair into the trie, starting search from root.
 * Panics if the key already exists.
 */
static __noinline int
ptpage_radix_insert(struct ptpage_radix *rtree, ptpage_t page)
{
	return (PTPAGE_RADIX_PCTRIE_INSERT(&rtree->rt_trie, page));
}

/*
 * Inserts the key-value pair into the trie, starting search from iterator.
 * Panics if the key already exists.
 */
static __inline int
ptpage_radix_iter_insert(struct pctrie_iter *pages, ptpage_t page)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_INSERT(pages, page));
}

/*
 * Insert the page into the ptpage_radix tree with its pindex as the key.  Panic if
 * the pindex already exists.  Return zero on success or a non-zero error on
 * memory allocation failure.  Set the out parameter mpred to the previous page
 * in the tree as if found by a previous call to ptpage_radix_lookup_le with the
 * new page pindex.
 */
static __inline int
ptpage_radix_insert_lookup_lt(struct ptpage_radix *rtree, ptpage_t page,
    ptpage_t *mpred)
{
	int error;

	error = PTPAGE_RADIX_PCTRIE_INSERT_LOOKUP_LE(&rtree->rt_trie, page, mpred);
	if (__predict_false(error == EEXIST))
		panic("ptpage_radix_insert_lookup_lt: page already present, %p",
		    *mpred);
	return (error);
}

/*
 * Returns the value stored at the index assuming there is an external lock.
 *
 * If the index is not present, NULL is returned.
 */
static __inline ptpage_t
ptpage_radix_lookup(struct ptpage_radix *rtree, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_LOOKUP(&rtree->rt_trie, index));
}

/*
 * Returns the value stored at the index without requiring an external lock.
 *
 * If the index is not present, NULL is returned.
 */
static __inline ptpage_t
ptpage_radix_lookup_unlocked(struct ptpage_radix *rtree, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_LOOKUP_UNLOCKED(&rtree->rt_trie, index));
}

/*
 * Initialize an iterator for ptpage_radix.
 */
static __inline void
ptpage_radix_iter_init(struct pctrie_iter *pages, struct ptpage_radix *rtree)
{
	pctrie_iter_init(pages, &rtree->rt_trie);
}

/*
 * Initialize an iterator for ptpage_radix.
 */
static __inline void
ptpage_radix_iter_limit_init(struct pctrie_iter *pages, struct ptpage_radix *rtree,
    vm_pindex_t limit)
{
	pctrie_iter_limit_init(pages, &rtree->rt_trie, limit);
}

/*
 * Returns the value stored at the index.
 * Requires that access be externally synchronized by a lock.
 *
 * If the index is not present, NULL is returned.
 */
static __inline ptpage_t
ptpage_radix_iter_lookup(struct pctrie_iter *pages, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_LOOKUP(pages, index));
}

/*
 * Returns the value stored 'stride' steps beyond the current position.
 * Requires that access be externally synchronized by a lock.
 *
 * If the index is not present, NULL is returned.
 */
static __inline ptpage_t
ptpage_radix_iter_stride(struct pctrie_iter *pages, int stride)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_STRIDE(pages, stride));
}

/*
 * Returns the page with the least pindex that is greater than or equal to the
 * specified pindex, or NULL if there are no such pages.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_lookup_ge(struct ptpage_radix *rtree, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_LOOKUP_GE(&rtree->rt_trie, index));
}

/*
 * Returns the page with the greatest pindex that is less than or equal to the
 * specified pindex, or NULL if there are no such pages.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_lookup_le(struct ptpage_radix *rtree, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_LOOKUP_LE(&rtree->rt_trie, index));
}

/*
 * Remove the specified index from the trie, and return the value stored at
 * that index.  If the index is not present, return NULL.
 */
static __inline ptpage_t
ptpage_radix_remove(struct ptpage_radix *rtree, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_REMOVE_LOOKUP(&rtree->rt_trie, index));
}

/*
 * Remove the current page from the trie.
 */
static __inline void
ptpage_radix_iter_remove(struct pctrie_iter *pages)
{
	PTPAGE_RADIX_PCTRIE_ITER_REMOVE(pages);
}
 
/*
 * Reclaim all the interior nodes of the trie, and invoke the callback
 * on all the pages, in order.
 */
static __inline void
ptpage_radix_reclaim_callback(struct ptpage_radix *rtree,
    void (*page_cb)(ptpage_t, void *), void *arg)
{
	PTPAGE_RADIX_PCTRIE_RECLAIM_CALLBACK(&rtree->rt_trie, page_cb, arg);
}

/*
 * Initialize an iterator pointing to the page with the least pindex that is
 * greater than or equal to the specified pindex, or NULL if there are no such
 * pages.  Return the page.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_iter_lookup_ge(struct pctrie_iter *pages, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_LOOKUP_GE(pages, index));
}

/*
 * Update the iterator to point to the page with the least pindex that is 'jump'
 * or more greater than or equal to the current pindex, or NULL if there are no
 * such pages.  Return the page.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_iter_jump(struct pctrie_iter *pages, vm_pindex_t jump)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_JUMP_GE(pages, jump));
}

/*
 * Update the iterator to point to the page with the least pindex that is one or
 * more greater than the current pindex, or NULL if there are no such pages.
 * Return the page.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_iter_step(struct pctrie_iter *pages)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_STEP_GE(pages));
}

/*
 * Initialize an iterator pointing to the page with the greatest pindex that is
 * less than or equal to the specified pindex, or NULL if there are no such
 * pages.  Return the page.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_iter_lookup_le(struct pctrie_iter *pages, vm_pindex_t index)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_LOOKUP_LE(pages, index));
}

/*
 * Update the iterator to point to the page with the pindex that is one greater
 * than the current pindex, or NULL if there is no such page.  Return the page.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_iter_next(struct pctrie_iter *pages)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_NEXT(pages));
}

/*
 * Update the iterator to point to the page with the pindex that is one less
 * than the current pindex, or NULL if there is no such page.  Return the page.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_iter_prev(struct pctrie_iter *pages)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_PREV(pages));
}

/*
 * Return the current page.
 *
 * Requires that access be externally synchronized by a lock.
 */
static __inline ptpage_t
ptpage_radix_iter_page(struct pctrie_iter *pages)
{
	return (PTPAGE_RADIX_PCTRIE_ITER_VALUE(pages));
}

/*
 * Replace an existing page in the trie with another one.
 * Panics if there is not an old page in the trie at the new page's index.
 */
static __inline ptpage_t
ptpage_radix_replace(struct ptpage_radix *rtree, ptpage_t newpage)
{
	return (PTPAGE_RADIX_PCTRIE_REPLACE(&rtree->rt_trie, newpage));
}


static uma_zone_t ptpage_radix_node_zone;

void *
ptpage_radix_node_alloc(struct pctrie *ptree)
{
	return (uma_zalloc_smr(ptpage_radix_node_zone, M_NOWAIT));
}

void
ptpage_radix_node_free(struct pctrie *ptree, void *node)
{
	uma_zfree_smr(ptpage_radix_node_zone, node);
}

void
ptpage_radix_zinit(void)
{

	ptpage_radix_node_zone = uma_zcreate("RADIX NODE", pctrie_node_size(),
	    NULL, NULL, pctrie_zone_init, NULL,
	    PCTRIE_PAD, UMA_ZONE_VM | UMA_ZONE_SMR);
	ptpage_radix_smr = uma_zone_get_smr(ptpage_radix_node_zone);
}

void
ptpage_radix_wait(void)
{
	uma_zwait(ptpage_radix_node_zone);
}
