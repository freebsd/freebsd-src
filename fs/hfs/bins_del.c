/*
 * linux/fs/hfs/bins_del.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the code common to inserting and deleting records
 * in a B-tree.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs_btree.h"

/*================ File-local functions ================*/

/*
 * hfs_bnode_update_key()
 *
 * Description:
 *   Updates the key for a bnode in its parent.
 *   The key change is propagated up the tree as necessary.
 * Input Variable(s):
 *   struct hfs_brec *brec: the search path to update keys in
 *   struct hfs_belem *belem: the search path element with the changed key
 *   struct hfs_bnode *bnode: the bnode with the changed key
 *   int offset: the "distance" from 'belem->bn' to 'bnode':
 *    0 if the change is in 'belem->bn',
 *    1 if the change is in its right sibling, etc.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'brec' points to a valid (struct hfs_brec)
 *   'belem' points to a valid (struct hfs_belem) in 'brec'.
 *   'bnode' points to a valid (struct hfs_bnode) which is non-empty
 *    and is 'belem->bn' or one of its siblings.
 *   'offset' is as described above.
 * Postconditions:
 *   The key change is propagated up the tree as necessary.
 */
void hfs_bnode_update_key(struct hfs_brec *brec, struct hfs_belem *belem,
			  struct hfs_bnode *bnode, int offset)
{
	int record = (--belem)->record + offset;
	void *key = bnode_datastart(bnode) + 1;
	int keysize = brec->tree->bthKeyLen;
	struct hfs_belem *limit;

  	memcpy(1+bnode_key(belem->bnr.bn, record), key, keysize);

	/* don't trash the header */
	if (brec->top > &brec->elem[1]) {
		limit = brec->top;
	} else {
		limit = &brec->elem[1];
	}

	while ((belem > limit) && (record == 1)) {
		record = (--belem)->record;
		memcpy(1+belem_key(belem), key, keysize);
	}
}

/*
 * hfs_bnode_shift_right()
 *
 * Description:
 *   Shifts some records from a node to its right neighbor.
 * Input Variable(s):
 *   struct hfs_bnode* left: the node to shift records from
 *   struct hfs_bnode* right: the node to shift records to
 *   hfs_u16 first: the number of the first record in 'left' to move to 'right'
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'left' and 'right' point to valid (struct hfs_bnode)s.
 *   'left' contains at least 'first' records.
 *   'right' has enough free space to hold the records to be moved from 'left'
 * Postconditions:
 *   The record numbered 'first' and all records after it in 'left' are
 *   placed at the beginning of 'right'.
 *   The key corresponding to 'right' in its parent is NOT updated.
 */
void hfs_bnode_shift_right(struct hfs_bnode *left, struct hfs_bnode *right,
			   int first)
{
	int i, adjust, nrecs;
	unsigned size;
	hfs_u16 *to, *from;

	if ((first <= 0) || (first > left->ndNRecs)) {
		hfs_warn("bad argument to shift_right: first=%d, nrecs=%d\n",
		       first, left->ndNRecs);
		return;
	}

	/* initialize variables */
	nrecs = left->ndNRecs + 1 - first;
	size = bnode_end(left) - bnode_offset(left, first);

	/* move (possibly empty) contents of right node forward */
	memmove(bnode_datastart(right) + size,
		bnode_datastart(right), 
		bnode_end(right) - sizeof(struct NodeDescriptor));

	/* copy in new records */
	memcpy(bnode_datastart(right), bnode_key(left,first), size);

	/* fix up offsets in right node */
	i = right->ndNRecs + 1;
	from = RECTBL(right, i);
	to = from - nrecs;
	while (i--) {
		hfs_put_hs(hfs_get_hs(from++) + size, to++);
	}
	adjust = sizeof(struct NodeDescriptor) - bnode_offset(left, first);
	i = nrecs-1;
	from = RECTBL(left, first+i);
	while (i--) {
		hfs_put_hs(hfs_get_hs(from++) + adjust, to++);
	}

	/* fix record counts */
	left->ndNRecs -= nrecs;
	right->ndNRecs += nrecs;
}

/*
 * hfs_bnode_shift_left()
 *
 * Description:
 *   Shifts some records from a node to its left neighbor.
 * Input Variable(s):
 *   struct hfs_bnode* left: the node to shift records to
 *   struct hfs_bnode* right: the node to shift records from
 *   hfs_u16 last: the number of the last record in 'right' to move to 'left'
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'left' and 'right' point to valid (struct hfs_bnode)s.
 *   'right' contains at least 'last' records.
 *   'left' has enough free space to hold the records to be moved from 'right'
 * Postconditions:
 *   The record numbered 'last' and all records before it in 'right' are
 *   placed at the end of 'left'.
 *   The key corresponding to 'right' in its parent is NOT updated.
 */
void hfs_bnode_shift_left(struct hfs_bnode *left, struct hfs_bnode *right,
			  int last)
{
	int i, adjust, nrecs;
	unsigned size;
	hfs_u16 *to, *from;

	if ((last <= 0) || (last > right->ndNRecs)) {
		hfs_warn("bad argument to shift_left: last=%d, nrecs=%d\n",
		       last, right->ndNRecs);
		return;
	}

	/* initialize variables */
	size = bnode_offset(right, last + 1) - sizeof(struct NodeDescriptor);

	/* copy records to left node */
	memcpy(bnode_dataend(left), bnode_datastart(right), size);

	/* move (possibly empty) remainder of right node backward */
	memmove(bnode_datastart(right), bnode_datastart(right) + size, 
			bnode_end(right) - bnode_offset(right, last + 1));

	/* fix up offsets */
	nrecs = left->ndNRecs;
	i = last;
	from = RECTBL(right, 2);
	to = RECTBL(left, nrecs + 2);
	adjust = bnode_offset(left, nrecs + 1) - sizeof(struct NodeDescriptor);
	while (i--) {
		hfs_put_hs(hfs_get_hs(from--) + adjust, to--);
	}
	i = right->ndNRecs + 1 - last;
	++from;
	to = RECTBL(right, 1);
	while (i--) {
		hfs_put_hs(hfs_get_hs(from--) - size, to--);
	}

	/* fix record counts */
	left->ndNRecs += last;
	right->ndNRecs -= last;
}

/*
 * hfs_bnode_in_brec()
 *
 * Description:
 *   Determines whethet a given bnode is part of a given brec.
 *   This is used to avoid deadlock in the case of a corrupted b-tree.
 * Input Variable(s):
 *   hfs_u32 node: the number of the node to check for.
 *   struct hfs_brec* brec: the brec to check in.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   int: 1 it found, 0 if not
 * Preconditions:
 *   'brec' points to a valid struct hfs_brec.
 * Postconditions:
 *   'brec' is unchanged.
 */
int hfs_bnode_in_brec(hfs_u32 node, const struct hfs_brec *brec)
{
	const struct hfs_belem *belem = brec->bottom;

	while (belem && (belem >= brec->top)) {
		if (belem->bnr.bn && (belem->bnr.bn->node == node)) {
			return 1;
		}
		--belem;
	}
	return 0;
}
