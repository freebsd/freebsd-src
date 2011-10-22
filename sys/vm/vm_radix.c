/*
 * Copyright (c) 2008 Mayur Shardul <mayur.shardul@gmail.com>
 * Copyright (c) 2011 Jeffrey Roberson <jeff@freebsd.org>
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
#include <vm/vm.h>
#include <vm/vm_radix.h>
#include <vm/vm_object.h>

#include <sys/kdb.h>


SLIST_HEAD(, vm_radix_node) res_rnodes_head =
    SLIST_HEAD_INITIALIZER(res_rnodes_head);

struct mtx rnode_lock;
vm_offset_t rnode_start;
vm_offset_t rnode_end;

/*
 * Allocate a radix node.  Initializes all elements to 0.
 */
static struct vm_radix_node *
vm_radix_node_get(void)
{
	struct vm_radix_node *rnode;

	if (VM_OBJECT_LOCKED(kernel_object) || VM_OBJECT_LOCKED(kmem_object)){
		mtx_lock_spin(&rnode_lock);
		if (!SLIST_EMPTY(&res_rnodes_head)) {
			rnode = SLIST_FIRST(&res_rnodes_head);
			SLIST_REMOVE_HEAD(&res_rnodes_head, next);
			mtx_unlock_spin(&rnode_lock);
			bzero((void *)rnode, sizeof(*rnode));
			goto out;
		} 
		mtx_unlock_spin(&rnode_lock);
		panic("No memory for kernel_object. . .");
	}
	rnode = malloc(sizeof(struct vm_radix_node), M_TEMP, M_NOWAIT | M_ZERO);
	if (rnode == NULL) {
		panic("vm_radix_node_get: Can not allocate memory\n");
		return NULL;
	}
out:

	return rnode;
}

/*
 * Free radix node.
 */
static void
vm_radix_node_put(struct vm_radix_node *rnode)
{

	KASSERT(rnode->rn_count == 0,
	    ("vm_radix_node_put: Freeing a node with %d children\n",
	    rnode->rn_count));
	if ((vm_offset_t)rnode >= rnode_start &&
	    (vm_offset_t)rnode < rnode_end) {
		mtx_lock_spin(&rnode_lock);
		SLIST_INSERT_HEAD(&res_rnodes_head, rnode, next);
		mtx_unlock_spin(&rnode_lock);
	} else
		free(rnode,M_TEMP);
}

/*
 * Return the position in the array for a given level.
 */
static inline int
vm_radix_slot(vm_pindex_t index, int level)
{

	return ((index >> (level * VM_RADIX_WIDTH)) & VM_RADIX_MASK);
}

/*
 * Inserts the key-value pair in to the radix tree.  Returns errno.
 * Panics if the key already exists.
 */
int
vm_radix_insert(struct vm_radix *rtree, vm_pindex_t index, void *val)
{
	struct vm_radix_node *rnode;
	int slot;
	int level;

	CTR3(KTR_VM,
	    "insert: tree %p, index %p, val %p", rtree, (void *)index, val);
	if (index == -1)
		panic("vm_radix_insert: -1 is not a valid index.\n");
	/*
	 * Increase the height by adding nodes at the root until
	 * there is sufficient space.
	 */
	while (rtree->rt_height == 0 ||
	     index > VM_RADIX_MAX(rtree->rt_height)) {
		CTR3(KTR_VM, "insert: expanding %jd > %jd height %d",
		    index, VM_RADIX_MAX(rtree->rt_height), rtree->rt_height);
		/*
		 * Only allocate tree nodes if they are needed.
		 */
		if (rtree->rt_root == NULL || rtree->rt_root->rn_count != 0) {
			rnode = vm_radix_node_get();
			if (rnode == NULL)
				return (ENOMEM);
			if (rtree->rt_root) {
				rnode->rn_child[0] = rtree->rt_root;
				rnode->rn_count = 1;
			}
			rtree->rt_root = rnode;
		}
		rtree->rt_height++;
		KASSERT(rtree->rt_height <= VM_RADIX_LIMIT,
		    ("vm_radix_insert: Tree %p height %d too tall", rtree,
		    rtree->rt_height));
	}

	/* Now that the tree is tall enough, fill in the path to the index. */
	rnode = rtree->rt_root;
	for (level = rtree->rt_height - 1; level > 0; level--) {
		slot = vm_radix_slot(index, level);
		/* Add the required intermidiate nodes. */
		if (rnode->rn_child[slot] == NULL) {
			rnode->rn_child[slot] = vm_radix_node_get();
    			if (rnode->rn_child[slot] == NULL)
		    		return (ENOMEM);
			rnode->rn_count++;
	    	}
		CTR5(KTR_VM,
		    "insert: tree %p, index %p, level %d, slot %d, child %p",
		    rtree, (void *)index, level, slot, rnode->rn_child[slot]);
		rnode = rnode->rn_child[slot];
	}

	slot = vm_radix_slot(index, level);
	CTR5(KTR_VM, "insert: tree %p, index %p, level %d, slot %d, child %p",
	    rtree, (void *)index, level, slot, rnode->rn_child[slot]);
	KASSERT(rnode->rn_child[slot] == NULL,
	    ("vm_radix_insert: Duplicate value %p at index: %lu\n", 
	    rnode->rn_child[slot], (u_long)index));
	rnode->rn_child[slot] = val;
	rnode->rn_count++;

	return 0;
}

/*
 * Returns the value stored at the index.  If the index is not present
 * NULL is returned.
 */
void *
vm_radix_lookup(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *rnode;
	int slot;
	int level;

	if (index > VM_RADIX_MAX(rtree->rt_height))
		return NULL;
	level = rtree->rt_height - 1;
	rnode = rtree->rt_root;
	while (rnode) {
		slot = vm_radix_slot(index, level);
		CTR5(KTR_VM,
		    "lookup: tree %p, index %p, level %d, slot %d, child %p",
		    rtree, (void *)index, level, slot, rnode->rn_child[slot]);
		if (level == 0)
			return rnode->rn_child[slot];
		rnode = rnode->rn_child[slot];
		level--;
	}
	CTR2(KTR_VM, "lookup: tree %p, index %p failed", rtree, (void *)index);

	return NULL;
}

/*
 * Looks up as many as cnt values between start and end and stores them
 * in the caller allocated array out.  The next index can be used to
 * restart the scan.  This optimizes forward scans in the tree.
 */
int
vm_radix_lookupn(struct vm_radix *rtree, vm_pindex_t start,
    vm_pindex_t end, void **out, int cnt, vm_pindex_t *next)
{
	struct vm_radix_node *rnode;
	struct vm_radix_node *child;
	vm_pindex_t max;
	vm_pindex_t inc;
	int slot;
	int level;
	void *val;
	int outidx;
	int loops = 0;

	CTR3(KTR_VM, "lookupn: tree %p, start %p, end %p",
	    rtree, (void *)start, (void *)end);
	outidx = 0;
	max = VM_RADIX_MAX(rtree->rt_height);
	if (start > max)
		return 0;
	if (end > max + 1)
		end = max + 1;
	if (end == 0)
		end = -1;
restart:
	loops++;
	if (loops > 1000)
		panic("vm_radix_lookupn: looping %d\n", loops);
	/*
	 * Search the tree from the top for any leaf node holding an index
	 * between start and end.
	 */
	level = rtree->rt_height - 1;
	rnode = rtree->rt_root;
	while (rnode) {
		slot = vm_radix_slot(start, level);
		CTR5(KTR_VM,
		    "lookupn: tree %p, index %p, level %d, slot %d, child %p",
		    rtree, (void *)start, level, slot, rnode->rn_child[slot]);
		if (level == 0)
			break;
		/*
		 * If we don't have an exact match we must start our search
		 * from the next leaf and adjust our index appropriately.
		 */
		if ((child = rnode->rn_child[slot]) == NULL) {
			/*
			 * Calculate how much to increment our index by
			 * based on the tree level.  We must truncate the
			 * lower bits to start from the begnning of the next
			 * leaf.
		 	 */
			inc = 1LL << (level * VM_RADIX_WIDTH);
			start &= ~VM_RADIX_MAX(level);
			start += inc;
			slot++;
			CTR5(KTR_VM,
			    "lookupn: start %p end %p inc %d mask 0x%lX slot %d",
			    (void *)start, (void *)end, inc, ~VM_RADIX_MAX(level), slot);
			for (; slot < VM_RADIX_COUNT && start < end;
			    slot++, start += inc) {
				child = rnode->rn_child[slot];
				if (child)
					break;
			}
		}
		rnode = child;
		level--;
	}
	if (rnode == NULL) {
		/*
		 * If we still have another range to search, start looking
		 * from the next node.  Otherwise, return what we've already
		 * found.  The loop above has already adjusted start to the
		 * beginning of the next node.
		 *
		 * Detect start wrapping back to 0 and return in this case.
		 */
		if (start < end && start != 0)
			goto restart;
		goto out;
	}
	for (; outidx < cnt && slot < VM_RADIX_COUNT && start < end;
	    slot++, start++) {
		val = rnode->rn_child[slot];
		if (val == NULL)
			continue;
		out[outidx++] = val;
	}
	/*
	 * Go fetch the next page to fill the requested number of pages
	 * otherwise the caller will simply call us again to fulfill the
	 * same request after the structures are pushed out of cache.
	 */
	if (outidx < cnt && start < end)
		goto restart;

out:
	*next = start;

	return (outidx);
}

/*
 * Look up any entry at a position greater or equal to index.
 */
void *
vm_radix_lookup_ge(struct vm_radix *rtree, vm_pindex_t index)
{
	vm_pindex_t max;
	void *val;
	int n;

	max = VM_RADIX_MAX(rtree->rt_height);
	n = vm_radix_lookupn(rtree, index, max + 1, &val, 1, &max);
	if (n)
		return (val);
	return (NULL);
}

/*
 * Look up any entry at a position less than or equal to index.
 */
void *
vm_radix_lookup_le(struct vm_radix *rtree, vm_pindex_t index)
{
	return NULL;
}

/*
 * Remove the specified index from the tree.  If possible the height of the
 * tree is adjusted after deletion.  The value stored at index is returned
 * panics if the key is not present.
 */
void *
vm_radix_remove(struct vm_radix *rtree, vm_pindex_t index)
{
	struct vm_radix_node *stack[VM_RADIX_LIMIT];
	struct vm_radix_node *rnode;
	void *val;
	int level;
	int slot;

	KASSERT(index <= VM_RADIX_MAX(rtree->rt_height),
	    ("vm_radix_remove: %p index %jd out of range %jd.",
	    rtree, index, VM_RADIX_MAX(rtree->rt_height)));
	val = NULL;
	rnode = rtree->rt_root;
	level = rtree->rt_height - 1;
	/*
	 * Find the node and record the path in stack.
	 */
	while (level && rnode) {
		stack[level] = rnode;
		slot = vm_radix_slot(index, level);
		rnode = rnode->rn_child[slot];
		CTR5(KTR_VM,
		    "remove: tree %p, index %p, level %d, slot %d, child %p",
		    rtree, (void *)index, level, slot, rnode->rn_child[slot]);
		level--;
	}
	slot = vm_radix_slot(index, 0);
	KASSERT(rnode != NULL && rnode->rn_child[slot] != NULL,
	    ("vm_radix_remove: index %jd not present in the tree.\n", index));

	val = rnode->rn_child[slot];
	for (;;) {
		rnode->rn_child[slot] = NULL;
		rnode->rn_count--;
		if (rnode->rn_count > 0)
			break;
		vm_radix_node_put(rnode);
		if (rnode == rtree->rt_root) {
			rtree->rt_root = NULL;
			rtree->rt_height = 0;
			break;
		}
		rnode = stack[++level];
		slot = vm_radix_slot(index, level);
			
	}
	return (val);
}

/*
 * Attempts to reduce the height of the tree.
 */
void 
vm_radix_shrink(struct vm_radix *rtree)
{
	struct vm_radix_node *tmp;

	if (rtree->rt_root == NULL)
		return;

	/* Adjust the height of the tree. */
	while (rtree->rt_root->rn_count == 1 && 
	    rtree->rt_root->rn_child[0] != NULL) {
		tmp = rtree->rt_root;
		rtree->rt_root = tmp->rn_child[0];
		rtree->rt_height--; 
		tmp->rn_count--;
		vm_radix_node_put(tmp);
	}
	/* Finally see if we have an empty tree. */
	if (rtree->rt_root->rn_count == 0) {
		vm_radix_node_put(rtree->rt_root);
		rtree->rt_root = NULL;
		rtree->rt_height = 0;
	}
}
