/*
 * linux/fs/hfs/bdelete.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the code to delete records in a B-tree.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs_btree.h"

/*================ Variable-like macros ================*/

#define FULL (HFS_SECTOR_SIZE - sizeof(struct NodeDescriptor))
#define NO_SPACE (HFS_SECTOR_SIZE+1)

/*================ File-local functions ================*/

/*
 * bdelete_nonempty()
 *
 * Description:
 *   Deletes a record from a given bnode without regard to it becoming empty.
 * Input Variable(s):
 *   struct hfs_brec* brec: pointer to the brec for the deletion
 *   struct hfs_belem* belem: which node in 'brec' to delete from
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'brec' points to a valid (struct hfs_brec).
 *   'belem' points to a valid (struct hfs_belem) in 'brec'.
 * Postconditions:
 *   The record has been inserted in the position indicated by 'brec'.
 */
static void bdelete_nonempty(struct hfs_brec *brec, struct hfs_belem *belem)
{
	int i, rec, nrecs, tomove;
	hfs_u16 size;
	hfs_u8 *start;
	struct hfs_bnode *bnode = belem->bnr.bn;

	rec = belem->record;
	nrecs = bnode->ndNRecs;
	size = bnode_rsize(bnode, rec);
	tomove = bnode_offset(bnode, nrecs+1) - bnode_offset(bnode, rec+1);
	
	/* adjust the record table */
	for (i = rec+1; i <= nrecs; ++i) {
		hfs_put_hs(bnode_offset(bnode,i+1) - size, RECTBL(bnode,i));
	}

	/* move it down */
	start = bnode_key(bnode, rec);
	memmove(start, start + size, tomove);

	/* update record count */
	--bnode->ndNRecs;
}

/*
 * del_root()
 *
 * Description:
 *   Delete the current root bnode.
 * Input Variable(s):
 *   struct hfs_bnode_ref *root: reference to the root bnode
 * Output Variable(s):
 *   NONE
 * Returns:
 *   int: 0 on success, error code on failure
 * Preconditions:
 *   'root' refers to the root bnode with HFS_LOCK_WRITE access.
 *   None of 'root's children are held with HFS_LOCK_WRITE access.
 * Postconditions:
 *   The current 'root' node is removed from the tree and the depth
 *    of the tree is reduced by one.
 *   If 'root' is an index node with exactly one child, then that
 *    child becomes the new root of the tree.
 *   If 'root' is an empty leaf node the tree becomes empty.
 *   Upon return access to 'root' is relinquished.
 */
static int del_root(struct hfs_bnode_ref *root)
{
	struct hfs_btree *tree = root->bn->tree;
	struct hfs_bnode_ref child;
	hfs_u32 node;

	if (root->bn->ndNRecs > 1) {
		return 0;
	} else if (root->bn->ndNRecs == 0) {
		/* tree is empty */
		tree->bthRoot = 0;
		tree->root = NULL;
		tree->bthRoot = 0;
		tree->bthFNode = 0;
		tree->bthLNode = 0;
		--tree->bthDepth;
		tree->dirt = 1;
		if (tree->bthDepth) {
			hfs_warn("hfs_bdelete: empty tree with bthDepth=%d\n",
				 tree->bthDepth);
			goto bail;
		}
		return hfs_bnode_free(root);
	} else if (root->bn->ndType == ndIndxNode) {
		/* tree is non-empty */
		node = hfs_get_hl(bkey_record(bnode_datastart(root->bn)));

		child = hfs_bnode_find(tree, node, HFS_LOCK_READ);
		if (!child.bn) {
			hfs_warn("hfs_bdelete: can't read child node.\n");
			goto bail;
		}
			
		child.bn->sticky = HFS_STICKY;
        	if (child.bn->next) {
                	child.bn->next->prev = child.bn->prev;
        	}
        	if (child.bn->prev) {
                	child.bn->prev->next = child.bn->next;
        	}
        	if (bhash(tree, child.bn->node) == child.bn) {
                	bhash(tree, child.bn->node) = child.bn->next;
        	}
		child.bn->next = NULL;
		child.bn->prev = NULL;

		tree->bthRoot = child.bn->node;
		tree->root = child.bn;

		/* re-assign bthFNode and bthLNode if the new root is
                   a leaf node. */
		if (child.bn->ndType == ndLeafNode) {
			tree->bthFNode = node;
			tree->bthLNode = node;
		}
		hfs_bnode_relse(&child);

		tree->bthRoot = node;
		--tree->bthDepth;
		tree->dirt = 1;
		if (!tree->bthDepth) {
			hfs_warn("hfs_bdelete: non-empty tree with "
				 "bthDepth == 0\n");
			goto bail;
		}
		return hfs_bnode_free(root);	/* marks tree dirty */
	}
	hfs_bnode_relse(root);
	return 0;

bail:
	hfs_bnode_relse(root);
	return -EIO;
}


/*
 * delete_empty_bnode()
 *
 * Description:
 *   Removes an empty non-root bnode from between 'left' and 'right'
 * Input Variable(s):
 *   hfs_u32 left_node: node number of 'left' or zero if 'left' is invalid
 *   struct hfs_bnode_ref *left: reference to the left neighbor of the
 *    bnode to remove, or invalid if no such neighbor exists.
 *   struct hfs_bnode_ref *center: reference to the bnode to remove
 *   hfs_u32 right_node: node number of 'right' or zero if 'right' is invalid
 *   struct hfs_bnode_ref *right: reference to the right neighbor of the
 *    bnode to remove, or invalid if no such neighbor exists.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'left_node' is as described above.
 *   'left' points to a valid (struct hfs_bnode_ref) having HFS_LOCK_WRITE
 *    access and referring to the left neighbor of 'center' if such a
 *    neighbor exists, or invalid if no such neighbor exists.
 *   'center' points to a valid (struct hfs_bnode_ref) having HFS_LOCK_WRITE
 *    access and referring to the bnode to delete.
 *   'right_node' is as described above.
 *   'right' points to a valid (struct hfs_bnode_ref) having HFS_LOCK_WRITE
 *    access and referring to the right neighbor of 'center' if such a
 *    neighbor exists, or invalid if no such neighbor exists.
 * Postconditions:
 *   If 'left' is valid its 'ndFLink' field becomes 'right_node'.
 *   If 'right' is valid its 'ndBLink' field becomes 'left_node'.
 *   If 'center' was the first leaf node then the tree's 'bthFNode'
 *    field becomes 'right_node' 
 *   If 'center' was the last leaf node then the tree's 'bthLNode'
 *    field becomes 'left_node' 
 *   'center' is NOT freed and access to the nodes is NOT relinquished.
 */
static void delete_empty_bnode(hfs_u32 left_node, struct hfs_bnode_ref *left,
			       struct hfs_bnode_ref *center,
			       hfs_u32 right_node, struct hfs_bnode_ref *right)
{
	struct hfs_bnode *bnode = center->bn;

	if (left_node) {
		left->bn->ndFLink = right_node;
	} else if (bnode->ndType == ndLeafNode) {
		bnode->tree->bthFNode = right_node;
		bnode->tree->dirt = 1;
	}

	if (right_node) {
		right->bn->ndBLink = left_node;
	} else if (bnode->ndType == ndLeafNode) {
		bnode->tree->bthLNode = left_node;
		bnode->tree->dirt = 1;
	}
}

/*
 * balance()
 *
 * Description:
 *   Attempt to equalize space usage in neighboring bnodes.
 * Input Variable(s):
 *   struct hfs_bnode *left: the left bnode.
 *   struct hfs_bnode *right: the right bnode.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'left' and 'right' point to valid (struct hfs_bnode)s obtained
 *    with HFS_LOCK_WRITE access, and are neighbors.
 * Postconditions:
 *   Records are shifted either left or right to make the space usage
 *   nearly equal.  When exact equality is not possible the break
 *   point is chosen to reduce data movement.
 *   The key corresponding to 'right' in its parent is NOT updated.
 */
static void balance(struct hfs_bnode *left, struct hfs_bnode *right)
{
	int index, left_free, right_free, half;

	left_free = bnode_freespace(left);
	right_free = bnode_freespace(right);
	half = (left_free + right_free)/2;

	if (left_free < right_free) {
		/* shift right to balance */
		index = left->ndNRecs + 1;
		while (right_free >= half) {
			--index;
			right_free -= bnode_rsize(left,index)+sizeof(hfs_u16);
		}
		if (index < left->ndNRecs) {
#if defined(DEBUG_ALL) || defined(DEBUG_BALANCE)
			hfs_warn("shifting %d of %d recs right to balance: ",
			       left->ndNRecs - index, left->ndNRecs);
#endif
			hfs_bnode_shift_right(left, right, index+1);
#if defined(DEBUG_ALL) || defined(DEBUG_BALANCE)
			hfs_warn("%d,%d\n", left->ndNRecs, right->ndNRecs);
#endif
		}
	} else {
		/* shift left to balance */
		index = 0;
		while (left_free >= half) {
			++index;
			left_free -= bnode_rsize(right,index)+sizeof(hfs_u16);
		}
		if (index > 1) {
#if defined(DEBUG_ALL) || defined(DEBUG_BALANCE)
			hfs_warn("shifting %d of %d recs left to balance: ",
			       index-1, right->ndNRecs);
#endif
			hfs_bnode_shift_left(left, right, index-1);
#if defined(DEBUG_ALL) || defined(DEBUG_BALANCE)
			hfs_warn("%d,%d\n", left->ndNRecs, right->ndNRecs);
#endif
		}
	}
}

/*
 * bdelete()
 *
 * Delete the given record from a B-tree.
 */
static int bdelete(struct hfs_brec *brec)
{
	struct hfs_btree *tree = brec->tree;
	struct hfs_belem *belem = brec->bottom;
	struct hfs_belem *parent = (belem-1);
	struct hfs_bnode *bnode;
	hfs_u32 left_node, right_node;
	struct hfs_bnode_ref left, right;
	int left_space, right_space, min_space;
	int fix_right_key;
	int fix_key;
	
	while ((belem > brec->top) &&
	       (belem->flags & (HFS_BPATH_UNDERFLOW | HFS_BPATH_FIRST))) {
		bnode = belem->bnr.bn;
		fix_key = belem->flags & HFS_BPATH_FIRST;
		fix_right_key = 0;

		bdelete_nonempty(brec, belem);

		if (bnode->node == tree->root->node) {
			del_root(&belem->bnr);
			--brec->bottom;
			goto done;
		}

		/* check for btree corruption which could lead to deadlock */
		left_node = bnode->ndBLink;
		right_node = bnode->ndFLink;
		if ((left_node && hfs_bnode_in_brec(left_node, brec)) ||
		    (right_node && hfs_bnode_in_brec(right_node, brec)) ||
		    (left_node == right_node)) {
			hfs_warn("hfs_bdelete: corrupt btree\n");
			hfs_brec_relse(brec, NULL);
			return -EIO;
		}

		/* grab the left neighbor if it exists */
		if (left_node) {
			hfs_bnode_lock(&belem->bnr, HFS_LOCK_RESRV);
			left = hfs_bnode_find(tree,left_node,HFS_LOCK_WRITE);
			if (!left.bn) {
				hfs_warn("hfs_bdelete: unable to read left "
					 "neighbor.\n");
				hfs_brec_relse(brec, NULL);
				return -EIO;
			}
			hfs_bnode_lock(&belem->bnr, HFS_LOCK_WRITE);
			if (parent->record != 1) {
				left_space = bnode_freespace(left.bn);
			} else {
				left_space = NO_SPACE;
			}
		} else {
			left.bn = NULL;
			left_space = NO_SPACE;
		}

		/* grab the right neighbor if it exists */
		if (right_node) {
			right = hfs_bnode_find(tree,right_node,HFS_LOCK_WRITE);
			if (!right.bn) {
				hfs_warn("hfs_bdelete: unable to read right "
					 "neighbor.\n");
				hfs_bnode_relse(&left);
				hfs_brec_relse(brec, NULL);
				return -EIO;
			}
			if (parent->record < parent->bnr.bn->ndNRecs) {
				right_space = bnode_freespace(right.bn);
			} else {
				right_space = NO_SPACE;
			}
		} else {
			right.bn = NULL;
			right_space = NO_SPACE;
		}

		if (left_space < right_space) {
			min_space = left_space;
		} else {
			min_space = right_space;
		}

		if (min_space == NO_SPACE) {
			hfs_warn("hfs_bdelete: no siblings?\n");
			hfs_brec_relse(brec, NULL);
			return -EIO;
		}

		if (bnode->ndNRecs == 0) {
			delete_empty_bnode(left_node, &left, &belem->bnr,
					   right_node, &right);
		} else if (min_space + bnode_freespace(bnode) >= FULL) {
			if ((right_space == NO_SPACE) ||
			    ((right_space == min_space) &&
			     (left_space != NO_SPACE))) {
				hfs_bnode_shift_left(left.bn, bnode,
						     bnode->ndNRecs);
			} else {
				hfs_bnode_shift_right(bnode, right.bn, 1);
				fix_right_key = 1;
			}
			delete_empty_bnode(left_node, &left, &belem->bnr,
					   right_node, &right);
		} else if (min_space == right_space) {
			balance(bnode, right.bn);
			fix_right_key = 1;
		} else {
			balance(left.bn, bnode);
			fix_key = 1;
		}

		if (fix_right_key) {
			hfs_bnode_update_key(brec, belem, right.bn, 1);
		}

		hfs_bnode_relse(&left);
		hfs_bnode_relse(&right);

		if (bnode->ndNRecs) {
			if (fix_key) {
				hfs_bnode_update_key(brec, belem, bnode, 0);
			}
			goto done;
		}

		hfs_bnode_free(&belem->bnr);
		--brec->bottom;
		belem = parent;
		--parent;
	}

	if (belem < brec->top) {
		hfs_warn("hfs_bdelete: Missing parent.\n");
		hfs_brec_relse(brec, NULL);
		return -EIO;
	}

	bdelete_nonempty(brec, belem);

done:
	hfs_brec_relse(brec, NULL);
	return 0;
}

/*================ Global functions ================*/

/*
 * hfs_bdelete()
 *
 * Delete the requested record from a B-tree.
 */
int hfs_bdelete(struct hfs_btree *tree, const struct hfs_bkey *key)
{ 
	struct hfs_belem *belem;
	struct hfs_bnode *bnode;
	struct hfs_brec brec;
	int retval;

	if (!tree || (tree->magic != HFS_BTREE_MAGIC) || !key) {
		hfs_warn("hfs_bdelete: invalid arguments.\n");
		return -EINVAL;
	}

	retval = hfs_bfind(&brec, tree, key, HFS_BFIND_DELETE);
	if (!retval) {
		belem = brec.bottom;
		bnode = belem->bnr.bn;

		belem->flags = 0;
        	if ((bnode->ndNRecs * sizeof(hfs_u16) + bnode_end(bnode) -
		     bnode_rsize(bnode, belem->record)) < FULL/2) {
			belem->flags |= HFS_BPATH_UNDERFLOW;
		}
		if (belem->record == 1) {
			belem->flags |= HFS_BPATH_FIRST;
		}

		if (!belem->flags) {
			hfs_brec_lock(&brec, brec.bottom);
		} else {
			hfs_brec_lock(&brec, NULL);
		}

		retval = bdelete(&brec);
		if (!retval) {
			--brec.tree->bthNRecs;
			brec.tree->dirt = 1;
		}
		hfs_brec_relse(&brec, NULL);
	}
	return retval;
}
