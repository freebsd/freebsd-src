/*
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


/*
 * Radix tree implementation.
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/ktr.h>
#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_radix.h>
#include <vm/vm_object.h>

#include <sys/kdb.h>

CTASSERT(sizeof(struct vm_radix_node) < PAGE_SIZE);

static uma_zone_t vm_radix_node_zone;

#ifndef UMA_MD_SMALL_ALLOC
static void *
vm_radix_node_zone_allocf(uma_zone_t zone, int size, uint8_t *flags, int wait)
{
	vm_offset_t addr;
	vm_page_t m;
	int pflags;

	/* Inform UMA that this allocator uses kernel_map. */
	*flags = UMA_SLAB_KERNEL;

	pflags = VM_ALLOC_WIRED | VM_ALLOC_NOOBJ;

	/*
	 * As kmem_alloc_nofault() can however fail, let just assume that
	 * M_NOWAIT is on and act accordingly.
	 */
	pflags |= ((wait & M_USE_RESERVE) != 0) ? VM_ALLOC_INTERRUPT :
	    VM_ALLOC_SYSTEM;
	if ((wait & M_ZERO) != 0)
		pflags |= VM_ALLOC_ZERO; 
	addr = kmem_alloc_nofault(kernel_map, size);
	if (addr == 0)
		return (NULL);

	/* Just one page allocation is assumed here. */
	m = vm_page_alloc(NULL, OFF_TO_IDX(addr - VM_MIN_KERNEL_ADDRESS),
	    pflags);
	if (m == NULL) {
		kmem_free(kernel_map, addr, size);
		return (NULL);
	}
	if ((wait & M_ZERO) != 0 && (m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);
	pmap_qenter(addr, &m, 1);
	return ((void *)addr);
}

static void
vm_radix_node_zone_freef(void *item, int size, uint8_t flags)
{
	vm_page_t m;
	vm_offset_t voitem;

	MPASS((flags & UMA_SLAB_KERNEL) != 0);

	/* Just one page allocation is assumed here. */
	voitem = (vm_offset_t)item;
	m = PHYS_TO_VM_PAGE(pmap_kextract(voitem));
	pmap_qremove(voitem, 1);
	vm_page_free(m);
	kmem_free(kernel_map, voitem, size);
}

static void
init_vm_radix_alloc(void *dummy __unused)
{

	uma_zone_set_allocf(vm_radix_node_zone, vm_radix_node_zone_allocf);
	uma_zone_set_freef(vm_radix_node_zone, vm_radix_node_zone_freef);
}
SYSINIT(vm_radix, SI_SUB_KMEM, SI_ORDER_SECOND, init_vm_radix_alloc, NULL);
#endif

/*
 * Radix node zone destructor.
 */
#ifdef INVARIANTS
static void
vm_radix_node_zone_dtor(void *mem, int size, void *arg)
{
	struct vm_radix_node *rnode;

	rnode = mem;
	KASSERT(rnode->rn_count == 0,
	    ("vm_radix_node_put: Freeing a node with %d children\n",
	    rnode->rn_count));
}
#endif

/*
 * Allocate a radix node.  Initializes all elements to 0.
 */
static __inline struct vm_radix_node *
vm_radix_node_get(void)
{

	return (uma_zalloc(vm_radix_node_zone, M_NOWAIT | M_ZERO));
}

/*
 * Free radix node.
 */
static __inline void
vm_radix_node_put(struct vm_radix_node *rnode)
{

	uma_zfree(vm_radix_node_zone, rnode);
}

/*
 * Return the position in the array for a given level.
 */
static __inline int
vm_radix_slot(vm_pindex_t index, int level)
{

	return ((index >> (level * VM_RADIX_WIDTH)) & VM_RADIX_MASK);
}

void
vm_radix_init(void)
{

	vm_radix_node_zone = uma_zcreate("RADIX NODE",
	    sizeof(struct vm_radix_node), NULL,
#ifdef INVARIANTS
	    vm_radix_node_zone_dtor,
#else
	    NULL,
#endif
	    NULL, NULL, VM_RADIX_HEIGHT, UMA_ZONE_VM);
}

/*
 * Extract the root node and height from a radix tree with a single load.
 */
static __inline int
vm_radix_height(struct vm_radix *rtree, struct vm_radix_node **rnode)
{
	uintptr_t root;
	int height;

	root = rtree->rt_root;
	height = root & VM_RADIX_HEIGHT;
	*rnode = (struct vm_radix_node *)(root - height);
	return (height);
}


/*
 * Set the root node and height for a radix tree.
 */
static inline void
vm_radix_setroot(struct vm_radix *rtree, struct vm_radix_node *rnode,
    int height)
{
	uintptr_t root;

	root = (uintptr_t)rnode | height;
	rtree->rt_root = root;
}

static inline void *
vm_radix_match(void *child, int color)
{
	uintptr_t c;

	c = (uintptr_t)child;

	if ((c & color) == 0)
		return (NULL);
	return ((void *)(c & ~VM_RADIX_FLAGS));
}

static void
vm_radix_reclaim_allnodes_internal(struct vm_radix_node *rnode, int level)
{
	int slot;

	MPASS(rnode != NULL && level >= 0);

	/*
	 * Level 0 just contains pages as children, thus make it a special
	 * case, free the node and return.
	 */
	if (level == 0) {
		CTR2(KTR_VM, "reclaiming: node %p, level %d", rnode, level);
		rnode->rn_count = 0;
		vm_radix_node_put(rnode);
		return;
	}
	for (slot = 0; slot < VM_RADIX_COUNT && rnode->rn_count != 0; slot++) {
		if (rnode->rn_child[slot] == NULL)
			continue;
		CTR3(KTR_VM,
		    "reclaiming: node %p, level %d recursing in slot %d",
		    rnode, level, slot);
		vm_radix_reclaim_allnodes_internal(rnode->rn_child[slot],
		    level - 1);
		rnode->rn_count--;
	}
	MPASS(rnode->rn_count == 0);
	CTR2(KTR_VM, "reclaiming: node %p, level %d", rnode, level);
	vm_radix_node_put(rnode);
}

/*
 * Inserts the key-value pair in to the radix tree.  Returns errno.
 * Panics if the key already exists.
 */
int
vm_radix_insert(struct vm_radix *rtree, vm_pindex_t index, void *val)
{
	struct vm_radix_node *rnode;
	struct vm_radix_node *root;
	int level;
	int slot;

	CTR3(KTR_VM,
	    "insert: tree %p, index %ju, val %p", rtree, (uintmax_t)index, val);
	if (index == -1)
		panic("vm_radix_insert: -1 is not a valid index.\n");
	level = vm_radix_height(rtree, &root);
	/*
	 * Increase the height by adding nodes at the root until
	 * there is sufficient space.
	 */
	while (level == 0 || index > VM_RADIX_MAX(level)) {
		CTR3(KTR_VM, "insert: expanding %jd > %jd height %d",
		    index, VM_RADIX_MAX(level), level);
		level++;
		KASSERT(level <= VM_RADIX_LIMIT,
		    ("vm_radix_insert: Tree %p height %d too tall",
		    rtree, level));
		/*
		 * Only allocate tree nodes if they are needed.
		 */
		if (root == NULL || root->rn_count != 0) {
			rnode = vm_radix_node_get();
			if (rnode == NULL) {
				CTR4(KTR_VM,
		"insert: tree %p, root %p, index: %ju, level: %d ENOMEM",
				    rtree, root, (uintmax_t)index, level);
				return (ENOMEM);
			}
			/*
			 * Store the new pointer with a memory barrier so
			 * that it is visible before the new root.
			 */
			if (root) {
				atomic_store_rel_ptr((volatile uintptr_t *)
				    &rnode->rn_child[0], (uintptr_t)root);
				rnode->rn_count = 1;
			}
			root = rnode;
		}
		vm_radix_setroot(rtree, root, level);
	}

	/* Now that the tree is tall enough, fill in the path to the index. */
	rnode = root;
	for (level = level - 1; level > 0; level--) {
		slot = vm_radix_slot(index, level);
		/* Add the required intermidiate nodes. */
		if (rnode->rn_child[slot] == NULL) {
			rnode->rn_child[slot] = vm_radix_node_get();
    			if (rnode->rn_child[slot] == NULL) {
				CTR5(KTR_VM,
	"insert: tree %p, index %ju, level %d, slot %d, rnode %p ENOMEM",
				     rtree, (uintmax_t)index, level, slot,
				     rnode);
				CTR4(KTR_VM,
			"insert: tree %p, rnode %p, child %p, count %u ENOMEM",
		    		    rtree, rnode, rnode->rn_child[slot],
				    rnode->rn_count);
		    		return (ENOMEM);
			}
			rnode->rn_count++;
	    	}
		CTR5(KTR_VM,
		    "insert: tree %p, index %ju, level %d, slot %d, rnode %p",
		    rtree, (uintmax_t)index, level, slot, rnode);
		CTR4(KTR_VM,
		    "insert: tree %p, rnode %p, child %p, count %u",
		    rtree, rnode, rnode->rn_child[slot], rnode->rn_count);
		rnode = rnode->rn_child[slot];
	}

	slot = vm_radix_slot(index, 0);
	MPASS(rnode != NULL);
	KASSERT(rnode->rn_child[slot] == NULL,
	    ("vm_radix_insert: Duplicate value %p at index: %lu\n", 
	    rnode->rn_child[slot], (u_long)index));
	val = (void *)((uintptr_t)val | VM_RADIX_BLACK);
	rnode->rn_child[slot] = val;
	rnode->rn_count++;
	CTR6(KTR_VM,
	    "insert: tree %p, index %ju, level %d, slot %d, rnode %p, count %u",
	    rtree, (uintmax_t)index, level, slot, rnode, rnode->rn_count);

	return 0;
}

/*
 * Returns the value stored at the index.  If the index is not present
 * NULL is returned.
 */
void *
vm_radix_lookup(struct vm_radix *rtree, vm_pindex_t index, int color)
{
	struct vm_radix_node *rnode;
	int slot;
	int level;

	level = vm_radix_height(rtree, &rnode);
	if (index > VM_RADIX_MAX(level))
		return NULL;
	level--;
	while (rnode) {
		slot = vm_radix_slot(index, level);
		CTR6(KTR_VM,
	"lookup: tree %p, index %ju, level %d, slot %d, rnode %p, child %p",
		    rtree, (uintmax_t)index, level, slot, rnode,
		    rnode->rn_child[slot]);
		if (level == 0)
			return vm_radix_match(rnode->rn_child[slot], color);
		rnode = rnode->rn_child[slot];
		level--;
	}
	CTR2(KTR_VM, "lookup: tree %p, index %ju failed", rtree,
	     (uintmax_t)index);

	return NULL;
}

void *
vm_radix_color(struct vm_radix *rtree, vm_pindex_t index, int color)
{
	struct vm_radix_node *rnode;
	uintptr_t child;
	int slot;
	int level;

	level = vm_radix_height(rtree, &rnode);
	if (index > VM_RADIX_MAX(level))
		return NULL;
	level--;
	while (rnode) {
		slot = vm_radix_slot(index, level);
		CTR6(KTR_VM,
	"color: tree %p, index %ju, level %d, slot %d, rnode %p, child %p",
		    rtree, (uintmax_t)index, level, slot, rnode,
		    rnode->rn_child[slot]);
		if (level == 0)
			break;
		rnode = rnode->rn_child[slot];
		level--;
	}
	if (rnode == NULL || rnode->rn_child[slot] == NULL)
		return (NULL);
	child = (uintptr_t)rnode->rn_child[slot];
	child &= ~VM_RADIX_FLAGS;
	rnode->rn_child[slot] = (void *)(child | color);

	return (void *)child;
}

/*
 * Find the first leaf with a valid node between *startp and end.  Return
 * the index of the first valid item in the leaf in *startp.
 */
static struct vm_radix_node *
vm_radix_leaf(struct vm_radix *rtree, vm_pindex_t *startp, vm_pindex_t end)
{
	struct vm_radix_node *rnode;
	vm_pindex_t start;
	vm_pindex_t inc;
	int slot;
	int level;

	start = *startp;
restart:
	level = vm_radix_height(rtree, &rnode);
	if (start > VM_RADIX_MAX(level) || (end && start >= end)) {
		rnode = NULL;
		goto out;
	}
	/*
	 * Search the tree from the top for any leaf node holding an index
	 * between start and end.
	 */
	for (level--; level; level--) {
		slot = vm_radix_slot(start, level);
		CTR6(KTR_VM,
	"leaf: tree %p, index %ju, level %d, slot %d, rnode %p, child %p",
		    rtree, (uintmax_t)start, level, slot, rnode,
		    (rnode != NULL) ? rnode->rn_child[slot] : NULL);
		if (rnode->rn_child[slot] != NULL) {
			rnode = rnode->rn_child[slot];
			continue;
		}
		/*
		 * Calculate how much to increment our index by
		 * based on the tree level.  We must truncate the
		 * lower bits to start from the begnning of the
		 * next leaf.
	 	 */
		inc = 1LL << (level * VM_RADIX_WIDTH);
		start &= ~VM_RADIX_MAX(level);
		start += inc;
		slot++;
		CTR5(KTR_VM,
		    "leaf: start %ju end %ju inc %d mask 0x%lX slot %d",
		    (uintmax_t)start, (uintmax_t)end, inc,
		    ~VM_RADIX_MAX(level), slot);
		for (; slot < VM_RADIX_COUNT; slot++, start += inc) {
			if (end != 0 && start >= end) {
				rnode = NULL;
				goto out;
			}
			if (rnode->rn_child[slot]) {
				rnode = rnode->rn_child[slot];
				break;
			}
		}
		if (slot == VM_RADIX_COUNT)
			goto restart;
	}

out:
	*startp = start;
	return (rnode);
}

    

/*
 * Looks up as many as cnt values between start and end, and stores
 * them in the caller allocated array out.  The next index can be used
 * to restart the scan.  This optimizes forward scans in the tree.
 */
int
vm_radix_lookupn(struct vm_radix *rtree, vm_pindex_t start,
    vm_pindex_t end, int color, void **out, int cnt, vm_pindex_t *next)
{
	struct vm_radix_node *rnode;
	void *val;
	int slot;
	int outidx;

	CTR3(KTR_VM, "lookupn: tree %p, start %ju, end %ju",
	    rtree, (uintmax_t)start, (uintmax_t)end);
	if (rtree->rt_root == 0)
		return (0);
	outidx = 0;
	while ((rnode = vm_radix_leaf(rtree, &start, end)) != NULL) {
		slot = vm_radix_slot(start, 0);
		for (; slot < VM_RADIX_COUNT; slot++, start++) {
			if (end != 0 && start >= end)
				goto out;
			val = vm_radix_match(rnode->rn_child[slot], color);
			if (val == NULL)
				continue;
			CTR4(KTR_VM,
			    "lookupn: tree %p index %ju slot %d found child %p",
			    rtree, (uintmax_t)start, slot, val);
			out[outidx] = val;
			if (++outidx == cnt)
				goto out;
		}
		if (end != 0 && start >= end)
			break;
	}
out:
	*next = start;
	return (outidx);
}

void
vm_radix_foreach(struct vm_radix *rtree, vm_pindex_t start, vm_pindex_t end,
    int color, void (*iter)(void *))
{
	struct vm_radix_node *rnode;
	void *val;
	int slot;

	if (rtree->rt_root == 0)
		return;
	while ((rnode = vm_radix_leaf(rtree, &start, end)) != NULL) {
		slot = vm_radix_slot(start, 0);
		for (; slot < VM_RADIX_COUNT; slot++, start++) {
			if (end != 0 && start >= end)
				return;
			val = vm_radix_match(rnode->rn_child[slot], color);
			if (val)
				iter(val);
		}
		if (end != 0 && start >= end)
			return;
	}
}


/*
 * Look up any entry at a position less than or equal to index.
 */
void *
vm_radix_lookup_le(struct vm_radix *rtree, vm_pindex_t index, int color)
{
	struct vm_radix_node *rnode;
	struct vm_radix_node *child;
	vm_pindex_t max;
	vm_pindex_t inc;
	void *val;
	int slot;
	int level;

	CTR2(KTR_VM,
	    "lookup_le: tree %p, index %ju", rtree, (uintmax_t)index);
restart:
	level = vm_radix_height(rtree, &rnode);
	if (rnode == NULL)
		return (NULL);
	max = VM_RADIX_MAX(level);
	if (index > max || index == 0)
		index = max;
	/*
	 * Search the tree from the top for any leaf node holding an index
	 * lower than 'index'.
	 */
	level--;
	while (rnode) {
		slot = vm_radix_slot(index, level);
		CTR6(KTR_VM,
	"lookup_le: tree %p, index %ju, level %d, slot %d, rnode %p, child %p",
		    rtree, (uintmax_t)index, level, slot, rnode,
		    rnode->rn_child[slot]);
		if (level == 0)
			break;
		/*
		 * If we don't have an exact match we must start our search
		 * from the next leaf and adjust our index appropriately.
		 */
		if ((child = rnode->rn_child[slot]) == NULL) {
			/*
			 * Calculate how much to decrement our index by
			 * based on the tree level.  We must set the
			 * lower bits to start from the end of the next
			 * leaf.
		 	 */
			inc = 1LL << (level * VM_RADIX_WIDTH);
			index |= VM_RADIX_MAX(level);
			index -= inc;
			slot--;
			CTR4(KTR_VM,
			    "lookup_le: start %ju inc %ld mask 0x%lX slot %d",
			    (uintmax_t)index, inc, VM_RADIX_MAX(level), slot);
			for (; slot >= 0; slot--, index -= inc) {
				child = rnode->rn_child[slot];
				if (child)
					break;
			}
		}
		rnode = child;
		level--;
	}
	if (rnode) {
		for (; slot >= 0; slot--, index--) {
			val = vm_radix_match(rnode->rn_child[slot], color);
			if (val)
				return (val);
		}
	}
	if (index != -1)
		goto restart;
	return (NULL);
}

/*
 * Remove the specified index from the tree.  If possible the height of the
 * tree is adjusted after deletion.  The value stored at index is returned
 * panics if the key is not present.
 */
void *
vm_radix_remove(struct vm_radix *rtree, vm_pindex_t index, int color)
{
	struct vm_radix_node *stack[VM_RADIX_LIMIT];
	struct vm_radix_node *rnode, *root;
	void *val;
	int level;
	int slot;

	level = vm_radix_height(rtree, &root);
	KASSERT(index <= VM_RADIX_MAX(level),
	    ("vm_radix_remove: %p index %ju out of range %jd.",
	     rtree, (uintmax_t)index, VM_RADIX_MAX(level)));
	rnode = root;
	val = NULL;
	level--;
	/*
	 * Find the node and record the path in stack.
	 */
	while (level && rnode) {
		stack[level] = rnode;
		slot = vm_radix_slot(index, level);
		CTR5(KTR_VM,
		    "remove: tree %p, index %ju, level %d, slot %d, rnode %p",
		    rtree, (uintmax_t)index, level, slot, rnode);
		CTR4(KTR_VM, "remove: tree %p, rnode %p, child %p, count %u",
		    rtree, rnode, rnode->rn_child[slot], rnode->rn_count);
		rnode = rnode->rn_child[slot];
		level--;
	}
	KASSERT(rnode != NULL,
		("vm_radix_remove: index %ju not present in the tree.\n",
		 (uintmax_t)index));
	slot = vm_radix_slot(index, 0);
	val = vm_radix_match(rnode->rn_child[slot], color);
	KASSERT(val != NULL,
		("vm_radix_remove: index %ju not present in the tree.\n",
		 (uintmax_t)index));

	for (;;) {
		CTR5(KTR_VM,
	"remove: resetting tree %p, index %ju, level %d, slot %d, rnode %p",
		    rtree, (uintmax_t)index, level, slot, rnode);
		CTR4(KTR_VM,
		    "remove: resetting tree %p, rnode %p, child %p, count %u",
		    rtree, rnode,
		    (rnode != NULL) ? rnode->rn_child[slot] : NULL,
		    (rnode != NULL) ? rnode->rn_count : 0);
		rnode->rn_child[slot] = NULL;
		rnode->rn_count--;
		/*
		 * Only allow black removes to prune the tree.
		 */
		if ((color & VM_RADIX_BLACK) == 0 || rnode->rn_count > 0)
			break;
		vm_radix_node_put(rnode);
		if (rnode == root) {
			vm_radix_setroot(rtree, NULL, 0);
			break;
		}
		rnode = stack[++level];
		slot = vm_radix_slot(index, level);
			
	}
	return (val);
}

/*
 * Remove and free all the nodes from the radix tree.
 * This function is recrusive but there is a tight control on it as the
 * maximum depth of the tree is fixed.
 */
void
vm_radix_reclaim_allnodes(struct vm_radix *rtree)
{
	struct vm_radix_node *root;
	int level;

	if (rtree->rt_root == 0)
		return;
	level = vm_radix_height(rtree, &root);
	vm_radix_reclaim_allnodes_internal(root, level - 1);
	rtree->rt_root = 0;
}

/*
 * Attempts to reduce the height of the tree.
 */
void 
vm_radix_shrink(struct vm_radix *rtree)
{
	struct vm_radix_node *tmp, *root;
	int level;

	if (rtree->rt_root == 0)
		return;
	level = vm_radix_height(rtree, &root);

	/* Adjust the height of the tree. */
	while (root->rn_count == 1 && root->rn_child[0] != NULL) {
		tmp = root;
		root->rn_count--;
		root = root->rn_child[0];
		level--;
		vm_radix_node_put(tmp);
	}
	/* Finally see if we have an empty tree. */
	if (root->rn_count == 0) {
		vm_radix_node_put(root);
		root = NULL;
		level--;
	}
	vm_radix_setroot(rtree, root, level);
}
