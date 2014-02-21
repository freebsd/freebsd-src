/**************************************************************************
 *
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 **************************************************************************/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Generic simple memory manager implementation. Intended to be used as a base
 * class implementation for more advanced memory managers.
 *
 * Note that the algorithm used is quite simple and there might be substantial
 * performance gains if a smarter free list is implemented. Currently it is just an
 * unordered stack of free regions. This could easily be improved if an RB-tree
 * is used instead. At least if we expect heavy fragmentation.
 *
 * Aligned allocations can also see improvement.
 *
 * Authors:
 * Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include <dev/drm2/drmP.h>
#include <dev/drm2/drm_mm.h>

#define MM_UNUSED_TARGET 4

static struct drm_mm_node *drm_mm_kmalloc(struct drm_mm *mm, int atomic)
{
	struct drm_mm_node *child;

	child = malloc(sizeof(*child), DRM_MEM_MM, M_ZERO |
	    (atomic ? M_NOWAIT : M_WAITOK));

	if (unlikely(child == NULL)) {
		mtx_lock(&mm->unused_lock);
		if (list_empty(&mm->unused_nodes))
			child = NULL;
		else {
			child =
			    list_entry(mm->unused_nodes.next,
				       struct drm_mm_node, node_list);
			list_del(&child->node_list);
			--mm->num_unused;
		}
		mtx_unlock(&mm->unused_lock);
	}
	return child;
}

int drm_mm_pre_get(struct drm_mm *mm)
{
	struct drm_mm_node *node;

	mtx_lock(&mm->unused_lock);
	while (mm->num_unused < MM_UNUSED_TARGET) {
		mtx_unlock(&mm->unused_lock);
		node = malloc(sizeof(*node), DRM_MEM_MM, M_WAITOK);
		mtx_lock(&mm->unused_lock);

		if (unlikely(node == NULL)) {
			int ret = (mm->num_unused < 2) ? -ENOMEM : 0;
			mtx_unlock(&mm->unused_lock);
			return ret;
		}
		++mm->num_unused;
		list_add_tail(&node->node_list, &mm->unused_nodes);
	}
	mtx_unlock(&mm->unused_lock);
	return 0;
}

static inline unsigned long drm_mm_hole_node_start(struct drm_mm_node *hole_node)
{
	return hole_node->start + hole_node->size;
}

static inline unsigned long drm_mm_hole_node_end(struct drm_mm_node *hole_node)
{
	struct drm_mm_node *next_node =
		list_entry(hole_node->node_list.next, struct drm_mm_node,
			   node_list);

	return next_node->start;
}

static void drm_mm_insert_helper(struct drm_mm_node *hole_node,
				 struct drm_mm_node *node,
				 unsigned long size, unsigned alignment)
{
	struct drm_mm *mm = hole_node->mm;
	unsigned long tmp = 0, wasted = 0;
	unsigned long hole_start = drm_mm_hole_node_start(hole_node);
	unsigned long hole_end = drm_mm_hole_node_end(hole_node);

	KASSERT(hole_node->hole_follows && !node->allocated, ("hole_node"));

	if (alignment)
		tmp = hole_start % alignment;

	if (!tmp) {
		hole_node->hole_follows = 0;
		list_del_init(&hole_node->hole_stack);
	} else
		wasted = alignment - tmp;

	node->start = hole_start + wasted;
	node->size = size;
	node->mm = mm;
	node->allocated = 1;

	INIT_LIST_HEAD(&node->hole_stack);
	list_add(&node->node_list, &hole_node->node_list);

	KASSERT(node->start + node->size <= hole_end, ("hole pos"));

	if (node->start + node->size < hole_end) {
		list_add(&node->hole_stack, &mm->hole_stack);
		node->hole_follows = 1;
	} else {
		node->hole_follows = 0;
	}
}

struct drm_mm_node *drm_mm_get_block_generic(struct drm_mm_node *hole_node,
					     unsigned long size,
					     unsigned alignment,
					     int atomic)
{
	struct drm_mm_node *node;

	node = drm_mm_kmalloc(hole_node->mm, atomic);
	if (unlikely(node == NULL))
		return NULL;

	drm_mm_insert_helper(hole_node, node, size, alignment);

	return node;
}

int drm_mm_insert_node(struct drm_mm *mm, struct drm_mm_node *node,
		       unsigned long size, unsigned alignment)
{
	struct drm_mm_node *hole_node;

	hole_node = drm_mm_search_free(mm, size, alignment, 0);
	if (!hole_node)
		return -ENOSPC;

	drm_mm_insert_helper(hole_node, node, size, alignment);

	return 0;
}

static void drm_mm_insert_helper_range(struct drm_mm_node *hole_node,
				       struct drm_mm_node *node,
				       unsigned long size, unsigned alignment,
				       unsigned long start, unsigned long end)
{
	struct drm_mm *mm = hole_node->mm;
	unsigned long tmp = 0, wasted = 0;
	unsigned long hole_start = drm_mm_hole_node_start(hole_node);
	unsigned long hole_end = drm_mm_hole_node_end(hole_node);

	KASSERT(hole_node->hole_follows && !node->allocated, ("hole_node"));

	if (hole_start < start)
		wasted += start - hole_start;
	if (alignment)
		tmp = (hole_start + wasted) % alignment;

	if (tmp)
		wasted += alignment - tmp;

	if (!wasted) {
		hole_node->hole_follows = 0;
		list_del_init(&hole_node->hole_stack);
	}

	node->start = hole_start + wasted;
	node->size = size;
	node->mm = mm;
	node->allocated = 1;

	INIT_LIST_HEAD(&node->hole_stack);
	list_add(&node->node_list, &hole_node->node_list);

	KASSERT(node->start + node->size <= hole_end, ("hole_end"));
	KASSERT(node->start + node->size <= end, ("end"));

	if (node->start + node->size < hole_end) {
		list_add(&node->hole_stack, &mm->hole_stack);
		node->hole_follows = 1;
	} else {
		node->hole_follows = 0;
	}
}

struct drm_mm_node *drm_mm_get_block_range_generic(struct drm_mm_node *hole_node,
						unsigned long size,
						unsigned alignment,
						unsigned long start,
						unsigned long end,
						int atomic)
{
	struct drm_mm_node *node;

	node = drm_mm_kmalloc(hole_node->mm, atomic);
	if (unlikely(node == NULL))
		return NULL;

	drm_mm_insert_helper_range(hole_node, node, size, alignment,
				   start, end);

	return node;
}

int drm_mm_insert_node_in_range(struct drm_mm *mm, struct drm_mm_node *node,
				unsigned long size, unsigned alignment,
				unsigned long start, unsigned long end)
{
	struct drm_mm_node *hole_node;

	hole_node = drm_mm_search_free_in_range(mm, size, alignment,
						start, end, 0);
	if (!hole_node)
		return -ENOSPC;

	drm_mm_insert_helper_range(hole_node, node, size, alignment,
				   start, end);

	return 0;
}

void drm_mm_remove_node(struct drm_mm_node *node)
{
	struct drm_mm *mm = node->mm;
	struct drm_mm_node *prev_node;

	KASSERT(!node->scanned_block && !node->scanned_prev_free
	    && !node->scanned_next_free, ("node"));

	prev_node =
	    list_entry(node->node_list.prev, struct drm_mm_node, node_list);

	if (node->hole_follows) {
		KASSERT(drm_mm_hole_node_start(node)
			!= drm_mm_hole_node_end(node), ("hole_follows"));
		list_del(&node->hole_stack);
	} else
		KASSERT(drm_mm_hole_node_start(node)
		       == drm_mm_hole_node_end(node), ("!hole_follows"));

	if (!prev_node->hole_follows) {
		prev_node->hole_follows = 1;
		list_add(&prev_node->hole_stack, &mm->hole_stack);
	} else
		list_move(&prev_node->hole_stack, &mm->hole_stack);

	list_del(&node->node_list);
	node->allocated = 0;
}

/*
 * Put a block. Merge with the previous and / or next block if they are free.
 * Otherwise add to the free stack.
 */

void drm_mm_put_block(struct drm_mm_node *node)
{
	struct drm_mm *mm = node->mm;

	drm_mm_remove_node(node);

	mtx_lock(&mm->unused_lock);
	if (mm->num_unused < MM_UNUSED_TARGET) {
		list_add(&node->node_list, &mm->unused_nodes);
		++mm->num_unused;
	} else
		free(node, DRM_MEM_MM);
	mtx_unlock(&mm->unused_lock);
}

static int check_free_hole(unsigned long start, unsigned long end,
			   unsigned long size, unsigned alignment)
{
	unsigned wasted = 0;

	if (end - start < size)
		return 0;

	if (alignment) {
		unsigned tmp = start % alignment;
		if (tmp)
			wasted = alignment - tmp;
	}

	if (end >= start + size + wasted) {
		return 1;
	}

	return 0;
}


struct drm_mm_node *drm_mm_search_free(const struct drm_mm *mm,
				       unsigned long size,
				       unsigned alignment, int best_match)
{
	struct drm_mm_node *entry;
	struct drm_mm_node *best;
	unsigned long best_size;

	best = NULL;
	best_size = ~0UL;

	list_for_each_entry(entry, &mm->hole_stack, hole_stack) {
		KASSERT(entry->hole_follows, ("hole_follows"));
		if (!check_free_hole(drm_mm_hole_node_start(entry),
				     drm_mm_hole_node_end(entry),
				     size, alignment))
			continue;

		if (!best_match)
			return entry;

		if (entry->size < best_size) {
			best = entry;
			best_size = entry->size;
		}
	}

	return best;
}

struct drm_mm_node *drm_mm_search_free_in_range(const struct drm_mm *mm,
						unsigned long size,
						unsigned alignment,
						unsigned long start,
						unsigned long end,
						int best_match)
{
	struct drm_mm_node *entry;
	struct drm_mm_node *best;
	unsigned long best_size;

	KASSERT(!mm->scanned_blocks, ("scanned"));

	best = NULL;
	best_size = ~0UL;

	list_for_each_entry(entry, &mm->hole_stack, hole_stack) {
		unsigned long adj_start = drm_mm_hole_node_start(entry) < start ?
			start : drm_mm_hole_node_start(entry);
		unsigned long adj_end = drm_mm_hole_node_end(entry) > end ?
			end : drm_mm_hole_node_end(entry);

		KASSERT(entry->hole_follows, ("hole_follows"));
		if (!check_free_hole(adj_start, adj_end, size, alignment))
			continue;

		if (!best_match)
			return entry;

		if (entry->size < best_size) {
			best = entry;
			best_size = entry->size;
		}
	}

	return best;
}

void drm_mm_replace_node(struct drm_mm_node *old, struct drm_mm_node *new)
{
	list_replace(&old->node_list, &new->node_list);
	list_replace(&old->hole_stack, &new->hole_stack);
	new->hole_follows = old->hole_follows;
	new->mm = old->mm;
	new->start = old->start;
	new->size = old->size;

	old->allocated = 0;
	new->allocated = 1;
}

void drm_mm_init_scan(struct drm_mm *mm, unsigned long size,
		      unsigned alignment)
{
	mm->scan_alignment = alignment;
	mm->scan_size = size;
	mm->scanned_blocks = 0;
	mm->scan_hit_start = 0;
	mm->scan_hit_size = 0;
	mm->scan_check_range = 0;
	mm->prev_scanned_node = NULL;
}

void drm_mm_init_scan_with_range(struct drm_mm *mm, unsigned long size,
				 unsigned alignment,
				 unsigned long start,
				 unsigned long end)
{
	mm->scan_alignment = alignment;
	mm->scan_size = size;
	mm->scanned_blocks = 0;
	mm->scan_hit_start = 0;
	mm->scan_hit_size = 0;
	mm->scan_start = start;
	mm->scan_end = end;
	mm->scan_check_range = 1;
	mm->prev_scanned_node = NULL;
}

int drm_mm_scan_add_block(struct drm_mm_node *node)
{
	struct drm_mm *mm = node->mm;
	struct drm_mm_node *prev_node;
	unsigned long hole_start, hole_end;
	unsigned long adj_start;
	unsigned long adj_end;

	mm->scanned_blocks++;

	KASSERT(!node->scanned_block, ("node->scanned_block"));
	node->scanned_block = 1;

	prev_node = list_entry(node->node_list.prev, struct drm_mm_node,
			       node_list);

	node->scanned_preceeds_hole = prev_node->hole_follows;
	prev_node->hole_follows = 1;
	list_del(&node->node_list);
	node->node_list.prev = &prev_node->node_list;
	node->node_list.next = &mm->prev_scanned_node->node_list;
	mm->prev_scanned_node = node;

	hole_start = drm_mm_hole_node_start(prev_node);
	hole_end = drm_mm_hole_node_end(prev_node);
	if (mm->scan_check_range) {
		adj_start = hole_start < mm->scan_start ?
			mm->scan_start : hole_start;
		adj_end = hole_end > mm->scan_end ?
			mm->scan_end : hole_end;
	} else {
		adj_start = hole_start;
		adj_end = hole_end;
	}

	if (check_free_hole(adj_start , adj_end,
			    mm->scan_size, mm->scan_alignment)) {
		mm->scan_hit_start = hole_start;
		mm->scan_hit_size = hole_end;

		return 1;
	}

	return 0;
}

int drm_mm_scan_remove_block(struct drm_mm_node *node)
{
	struct drm_mm *mm = node->mm;
	struct drm_mm_node *prev_node;

	mm->scanned_blocks--;

	KASSERT(node->scanned_block, ("scanned_block"));
	node->scanned_block = 0;

	prev_node = list_entry(node->node_list.prev, struct drm_mm_node,
			       node_list);

	prev_node->hole_follows = node->scanned_preceeds_hole;
	INIT_LIST_HEAD(&node->node_list);
	list_add(&node->node_list, &prev_node->node_list);

	/* Only need to check for containement because start&size for the
	 * complete resulting free block (not just the desired part) is
	 * stored. */
	if (node->start >= mm->scan_hit_start &&
	    node->start + node->size
	    		<= mm->scan_hit_start + mm->scan_hit_size) {
		return 1;
	}

	return 0;
}

int drm_mm_clean(struct drm_mm * mm)
{
	struct list_head *head = &mm->head_node.node_list;

	return (head->next->next == head);
}

int drm_mm_init(struct drm_mm * mm, unsigned long start, unsigned long size)
{
	INIT_LIST_HEAD(&mm->hole_stack);
	INIT_LIST_HEAD(&mm->unused_nodes);
	mm->num_unused = 0;
	mm->scanned_blocks = 0;
	mtx_init(&mm->unused_lock, "drm_unused", NULL, MTX_DEF);

	INIT_LIST_HEAD(&mm->head_node.node_list);
	INIT_LIST_HEAD(&mm->head_node.hole_stack);
	mm->head_node.hole_follows = 1;
	mm->head_node.scanned_block = 0;
	mm->head_node.scanned_prev_free = 0;
	mm->head_node.scanned_next_free = 0;
	mm->head_node.mm = mm;
	mm->head_node.start = start + size;
	mm->head_node.size = start - mm->head_node.start;
	list_add_tail(&mm->head_node.hole_stack, &mm->hole_stack);

	return 0;
}

void drm_mm_takedown(struct drm_mm * mm)
{
	struct drm_mm_node *entry, *next;

	if (!list_empty(&mm->head_node.node_list)) {
		DRM_ERROR("Memory manager not clean. Delaying takedown\n");
		return;
	}

	mtx_lock(&mm->unused_lock);
	list_for_each_entry_safe(entry, next, &mm->unused_nodes, node_list) {
		list_del(&entry->node_list);
		free(entry, DRM_MEM_MM);
		--mm->num_unused;
	}
	mtx_unlock(&mm->unused_lock);

	mtx_destroy(&mm->unused_lock);

	KASSERT(mm->num_unused == 0, ("num_unused != 0"));
}

void drm_mm_debug_table(struct drm_mm *mm, const char *prefix)
{
	struct drm_mm_node *entry;
	unsigned long total_used = 0, total_free = 0, total = 0;
	unsigned long hole_start, hole_end, hole_size;

	hole_start = drm_mm_hole_node_start(&mm->head_node);
	hole_end = drm_mm_hole_node_end(&mm->head_node);
	hole_size = hole_end - hole_start;
	if (hole_size)
		printf("%s 0x%08lx-0x%08lx: %8lu: free\n",
			prefix, hole_start, hole_end,
			hole_size);
	total_free += hole_size;

	drm_mm_for_each_node(entry, mm) {
		printf("%s 0x%08lx-0x%08lx: %8lu: used\n",
			prefix, entry->start, entry->start + entry->size,
			entry->size);
		total_used += entry->size;

		if (entry->hole_follows) {
			hole_start = drm_mm_hole_node_start(entry);
			hole_end = drm_mm_hole_node_end(entry);
			hole_size = hole_end - hole_start;
			printf("%s 0x%08lx-0x%08lx: %8lu: free\n",
				prefix, hole_start, hole_end,
				hole_size);
			total_free += hole_size;
		}
	}
	total = total_free + total_used;

	printf("%s total: %lu, used %lu free %lu\n", prefix, total,
		total_used, total_free);
}
