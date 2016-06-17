/*
 * linux/fs/hfs/balloc.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * hfs_bnode_alloc() and hfs_bnode_bitop() are based on GPLed code
 * Copyright (C) 1995  Michael Dreher
 *
 * This file contains the code to create and destroy nodes
 * in the B-tree structure.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 *
 * The code in this file initializes some structures which contain
 * pointers by calling memset(&foo, 0, sizeof(foo)).
 * This produces the desired behavior only due to the non-ANSI
 * assumption that the machine representation of NULL is all zeros.
 */

#include "hfs_btree.h"

/*================ File-local functions ================*/

/*
 * get_new_node()
 *
 * Get a buffer for a new node with out reading it from disk.
 */
static hfs_buffer get_new_node(struct hfs_btree *tree, hfs_u32 node)
{
	int tmp;
	hfs_buffer retval = HFS_BAD_BUFFER;

  	tmp = hfs_extent_map(&tree->entry.u.file.data_fork, node, 0);
	if (tmp) {
		retval = hfs_buffer_get(tree->sys_mdb, tmp, 0);
	}
	return retval;
}

/*
 * hfs_bnode_init()
 *
 * Description:
 *   Initialize a newly allocated bnode.
 * Input Variable(s):
 *   struct hfs_btree *tree: Pointer to a B-tree
 *   hfs_u32 node: the node number to allocate
 * Output Variable(s):
 *   NONE
 * Returns:
 *   struct hfs_bnode_ref for the new node
 * Preconditions:
 *   'tree' points to a "valid" (struct hfs_btree)
 *   'node' exists and has been allocated in the bitmap of bnodes.
 * Postconditions:
 *   On success:
 *    The node is not read from disk, nor added to the bnode cache.
 *    The 'sticky' and locking-related fields are all zero/NULL.
 *    The bnode's nd{[FB]Link, Type, NHeight} fields are uninitialized.
 *    The bnode's ndNRecs field and offsets table indicate an empty bnode.
 *   On failure:
 *    The node is deallocated.
 */
static struct hfs_bnode_ref hfs_bnode_init(struct hfs_btree * tree,
					   hfs_u32 node)
{
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	extern int bnode_count;
#endif
	struct hfs_bnode_ref retval;

	retval.lock_type = HFS_LOCK_NONE;
	if (!HFS_NEW(retval.bn)) {
		hfs_warn("hfs_bnode_init: out of memory.\n");
		goto bail2;
	}

	/* Partially initialize the in-core structure */
	memset(retval.bn, 0, sizeof(*retval.bn));
	retval.bn->magic = HFS_BNODE_MAGIC;
	retval.bn->tree = tree;
	retval.bn->node = node;
	hfs_init_waitqueue(&retval.bn->wqueue);
	hfs_init_waitqueue(&retval.bn->rqueue);
	hfs_bnode_lock(&retval, HFS_LOCK_WRITE);

	retval.bn->buf = get_new_node(tree, node);
	if (!hfs_buffer_ok(retval.bn->buf)) {
		goto bail1;
	}

#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	++bnode_count;
#endif

	/* Partially initialize the on-disk structure */
	memset(hfs_buffer_data(retval.bn->buf), 0, HFS_SECTOR_SIZE);
	hfs_put_hs(sizeof(struct NodeDescriptor), RECTBL(retval.bn, 1));

	return retval;

bail1:
	HFS_DELETE(retval.bn);
bail2:
	/* clear the bit in the bitmap */
	hfs_bnode_bitop(tree, node, 0);
	return retval;
}

/*
 * init_mapnode()
 *
 * Description:
 *   Initializes a given node as a mapnode in the given tree.
 * Input Variable(s):
 *   struct hfs_bnode *bn: the node to add the mapnode after.
 *   hfs_u32: the node to use as a mapnode.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   struct hfs_bnode *: the new mapnode or NULL
 * Preconditions:
 *   'tree' is a valid (struct hfs_btree).
 *   'node' is the number of the first node in 'tree' that is not
 *    represented by a bit in the existing mapnodes.
 * Postconditions:
 *   On failure 'tree' is unchanged and NULL is returned.
 *   On success the node given by 'node' has been added to the linked
 *    list of mapnodes attached to 'tree', and has been initialized as
 *    a valid mapnode with its first bit set to indicate itself as
 *    allocated.
 */
static struct hfs_bnode *init_mapnode(struct hfs_bnode *bn, hfs_u32 node)
{
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	extern int bnode_count;
#endif
	struct hfs_bnode *retval;

	if (!HFS_NEW(retval)) {
		hfs_warn("hfs_bnode_add: out of memory.\n");
		return NULL;
	}

	memset(retval, 0, sizeof(*retval));
	retval->magic = HFS_BNODE_MAGIC;
	retval->tree = bn->tree;
	retval->node = node;
	retval->sticky = HFS_STICKY;
	retval->buf = get_new_node(bn->tree, node);
	if (!hfs_buffer_ok(retval->buf)) {
		HFS_DELETE(retval);
		return NULL;
	}

#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	++bnode_count;
#endif

	/* Initialize the bnode data structure */
	memset(hfs_buffer_data(retval->buf), 0, HFS_SECTOR_SIZE);
	retval->ndFLink = 0;
	retval->ndBLink = bn->node;
	retval->ndType = ndMapNode;
	retval->ndNHeight = 0;
	retval->ndNRecs = 1;
	hfs_put_hs(sizeof(struct NodeDescriptor), RECTBL(retval, 1));
	hfs_put_hs(0x1fa,                         RECTBL(retval, 2));
	*((hfs_u8 *)bnode_key(retval, 1)) = 0x80; /* set first bit of bitmap */
	retval->prev = bn;
	hfs_bnode_commit(retval);

	bn->ndFLink = node;
	bn->next = retval;
	hfs_bnode_commit(bn);

	return retval;
}

/*================ Global functions ================*/

/*
 * hfs_bnode_bitop()
 *
 * Description:
 *   Allocate/free the requested node of a B-tree of the hfs filesystem
 *   by setting/clearing the corresponding bit in the B-tree bitmap.
 *   The size of the B-tree will not be changed.
 * Input Variable(s):
 *   struct hfs_btree *tree: Pointer to a B-tree
 *   hfs_u32 bitnr: The node number to free
 *   int set: 0 to clear the bit, non-zero to set it.
 * Output Variable(s):
 *   None
 * Returns:
 *    0: no error
 *   -1: The node was already allocated/free, nothing has been done.
 *   -2: The node is out of range of the B-tree.
 *   -4: not enough map nodes to hold all the bits
 * Preconditions:
 *   'tree' points to a "valid" (struct hfs_btree)
 *   'bitnr' is a node number within the range of the btree, which is
 *   currently free/allocated.
 * Postconditions:
 *   The bit number 'bitnr' of the node bitmap is set/cleared and the
 *   number of free nodes in the btree is decremented/incremented by one.
 */
int hfs_bnode_bitop(struct hfs_btree *tree, hfs_u32 bitnr, int set)
{
	struct hfs_bnode *bn;   /* the current bnode */
	hfs_u16 start;		/* the start (in bits) of the bitmap in node */
	hfs_u16 len;		/* the len (in bits) of the bitmap in node */
	hfs_u32 *u32;		/* address of the u32 containing the bit */

	if (bitnr >= tree->bthNNodes) {
		hfs_warn("hfs_bnode_bitop: node number out of range.\n");
		return -2;
	}

	bn = &tree->head;
	for (;;) {
		start = bnode_offset(bn, bn->ndNRecs) << 3;
		len = (bnode_offset(bn, bn->ndNRecs + 1) << 3) - start;

		if (bitnr < len) {
			break;
		}

		/* continue on to next map node if available */
		if (!(bn = bn->next)) {
			hfs_warn("hfs_bnode_bitop: too few map nodes.\n");
			return -4;
		}
		bitnr -= len;
	}

	/* Change the correct bit */
	bitnr += start;
	u32 = (hfs_u32 *)hfs_buffer_data(bn->buf) + (bitnr >> 5);
	bitnr %= 32;
	if ((set && hfs_set_bit(bitnr, u32)) ||
	    (!set && !hfs_clear_bit(bitnr, u32))) {
		hfs_warn("hfs_bnode_bitop: bitmap corruption.\n");
		return -1;
	}
	hfs_buffer_dirty(bn->buf);

	/* adjust the free count */
	tree->bthFree += (set ? -1 : 1);
	tree->dirt = 1;

	return 0;
}

/*
 * hfs_bnode_alloc()
 *
 * Description:
 *   Find a cleared bit in the B-tree node bitmap of the hfs filesystem,
 *   set it and return the corresponding bnode, with its contents zeroed.
 *   When there is no free bnode in the tree, an error is returned, no
 *   new nodes will be added by this function!
 * Input Variable(s):
 *   struct hfs_btree *tree: Pointer to a B-tree
 * Output Variable(s):
 *   NONE
 * Returns:
 *   struct hfs_bnode_ref for the new bnode
 * Preconditions:
 *   'tree' points to a "valid" (struct hfs_btree)
 *   There is at least one free bnode.
 * Postconditions:
 *   On success:
 *     The corresponding bit in the btree bitmap is set.
 *     The number of free nodes in the btree is decremented by one.
 *   The node is not read from disk, nor added to the bnode cache.
 *   The 'sticky' field is uninitialized.
 */
struct hfs_bnode_ref hfs_bnode_alloc(struct hfs_btree *tree)
{
	struct hfs_bnode *bn;   /* the current bnode */
	hfs_u32 bitnr = 0;	/* which bit are we examining */
	hfs_u16 first;		/* the first clear bit in this bnode */
	hfs_u16 start;		/* the start (in bits) of the bitmap in node */
	hfs_u16 end;		/* the end (in bits) of the bitmap in node */
	hfs_u32 *data;		/* address of the data in this bnode */
	
	bn = &tree->head;
	for (;;) {
		start = bnode_offset(bn, bn->ndNRecs) << 3;
		end = bnode_offset(bn, bn->ndNRecs + 1) << 3;
		data =  (hfs_u32 *)hfs_buffer_data(bn->buf);
		
		/* search the current node */
		first = hfs_find_zero_bit(data, end, start);
		if (first < end) {
			break;
		}

		/* continue search in next map node */
		bn = bn->next;

		if (!bn) {
			hfs_warn("hfs_bnode_alloc: too few map nodes.\n");
			goto bail;
		}
		bitnr += (end - start);
	}

	if ((bitnr += (first - start)) >= tree->bthNNodes) {
		hfs_warn("hfs_bnode_alloc: no free nodes found, "
			 "count wrong?\n");
		goto bail;
	}

	if (hfs_set_bit(first % 32, data + (first>>5))) {
		hfs_warn("hfs_bnode_alloc: bitmap corruption.\n");
		goto bail;
	}
	hfs_buffer_dirty(bn->buf);

	/* decrement the free count */
	--tree->bthFree;
	tree->dirt = 1;

	return hfs_bnode_init(tree, bitnr);

bail:
	return (struct hfs_bnode_ref){NULL, HFS_LOCK_NONE};
}

/*
 * hfs_btree_extend()
 *
 * Description:
 *   Adds nodes to a B*-tree if possible.
 * Input Variable(s):
 *   struct hfs_btree *tree: the btree to add nodes to.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'tree' is a valid (struct hfs_btree *).
 * Postconditions:
 *   If possible the number of nodes indicated by the tree's clumpsize
 *    have been added to the tree, updating all in-core and on-disk
 *    allocation information.
 *   If insufficient disk-space was available then fewer nodes may have
 *    been added than would be expected based on the clumpsize.
 *   In the case of the extents B*-tree this function will add fewer
 *    nodes than expected if adding more would result in an extent
 *    record for the extents tree being added to the extents tree.
 *    The situation could be dealt with, but doing so confuses Macs.
 */
void hfs_btree_extend(struct hfs_btree *tree)
{
	struct hfs_bnode_ref head;
	struct hfs_bnode *bn, *tmp;
	struct hfs_cat_entry *entry = &tree->entry;
	struct hfs_mdb *mdb = entry->mdb;
	hfs_u32 old_nodes, new_nodes, total_nodes, new_mapnodes, seen;

	old_nodes = entry->u.file.data_fork.psize;

	entry->u.file.data_fork.lsize += 1; /* rounded up to clumpsize */
	hfs_extent_adj(&entry->u.file.data_fork);

	total_nodes = entry->u.file.data_fork.psize;
	entry->u.file.data_fork.lsize = total_nodes << HFS_SECTOR_SIZE_BITS;
	new_nodes = total_nodes - old_nodes;
	if (!new_nodes) {
		return;
	}

	head = hfs_bnode_find(tree, 0, HFS_LOCK_WRITE);
	if (!(bn = head.bn)) {
		hfs_warn("hfs_btree_extend: header node not found.\n");
		return;
	}

	seen = 0;
	new_mapnodes = 0;
	for (;;) {
		seen += bnode_rsize(bn, bn->ndNRecs) << 3;

		if (seen >= total_nodes) {
			break;
		}

		if (!bn->next) {
			tmp = init_mapnode(bn, seen);
			if (!tmp) {
				hfs_warn("hfs_btree_extend: "
					 "can't build mapnode.\n");
				hfs_bnode_relse(&head);
				return;
			}
			++new_mapnodes;
		}
		bn = bn->next;
	}
	hfs_bnode_relse(&head);

	tree->bthNNodes = total_nodes;
	tree->bthFree += (new_nodes - new_mapnodes);
	tree->dirt = 1;

	/* write the backup MDB, not returning until it is written */
	hfs_mdb_commit(mdb, 1);

	return;
}

/*
 * hfs_bnode_free()
 *
 * Remove a node from the cache and mark it free in the bitmap.
 */
int hfs_bnode_free(struct hfs_bnode_ref *bnr)
{
	hfs_u32 node = bnr->bn->node;
	struct hfs_btree *tree = bnr->bn->tree;

	if (bnr->bn->count != 1) {
		hfs_warn("hfs_bnode_free: count != 1.\n");
		return -EIO;
	}

	hfs_bnode_relse(bnr);
	hfs_bnode_bitop(tree, node, 0);
	return 0;
}
