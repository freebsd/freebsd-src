/*
 * linux/fs/hfs/brec.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the code to access records in a btree.
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
 * first()
 *
 * returns HFS_BPATH_FIRST if elem->record == 1, 0 otherwise
 */
static inline int first(const struct hfs_belem *elem)
{
	return (elem->record == 1) ? HFS_BPATH_FIRST : 0;
}

/*
 * overflow()
 *
 * return HFS_BPATH_OVERFLOW if the node has no room for an 
 * additional pointer record, 0 otherwise.
 */
static inline int overflow(const struct hfs_btree *tree,
			   const struct hfs_bnode *bnode)
{
	/* there is some algebra involved in getting this form */
	return ((HFS_SECTOR_SIZE - sizeof(hfs_u32)) <
		 (bnode_end(bnode) + (2+bnode->ndNRecs)*sizeof(hfs_u16) +
		  ROUND(tree->bthKeyLen+1))) ?  HFS_BPATH_OVERFLOW : 0;
}

/*
 * underflow()
 *
 * return HFS_BPATH_UNDERFLOW if the node will be less that 1/2 full
 * upon removal of a pointer record, 0 otherwise.
 */
static inline int underflow(const struct hfs_btree *tree,
			    const struct hfs_bnode *bnode)
{
	return ((bnode->ndNRecs * sizeof(hfs_u16) +
		 bnode_offset(bnode, bnode->ndNRecs)) <
		(HFS_SECTOR_SIZE - sizeof(struct NodeDescriptor))/2) ?
		HFS_BPATH_UNDERFLOW : 0;
}

/*================ Global functions ================*/

/*
 * hfs_brec_next()
 *
 * Description:
 *   Obtain access to a child of an internal node in a B-tree.
 * Input Variable(s):
 *   struct hfs_brec *brec: pointer to the (struct hfs_brec) to
 *    add an element to.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   struct hfs_belem *: pointer to the new path element or NULL
 * Preconditions:
 *   'brec' points to a "valid" (struct hfs_brec), the last element of
 *   which corresponds to a record in a bnode of type ndIndxNode and the
 *   'record' field indicates the index record for the desired child.
 * Postconditions:
 *   If the call to hfs_bnode_find() fails then 'brec' is released
 *   and a NULL is returned.
 *   Otherwise:
 *    Any ancestors in 'brec' that are not needed (as determined by the
 *     'keep_flags' field of 'brec) are released from 'brec'.
 *    A new element is added to 'brec' corresponding to the desired
 *     child.
 *    The child is obtained with the same 'lock_type' field as its
 *     parent.
 *    The 'record' field is initialized to the last record.
 *    A pointer to the new path element is returned.
 */
struct hfs_belem *hfs_brec_next(struct hfs_brec *brec)
{
	struct hfs_belem *elem = brec->bottom;
	hfs_u32 node;
	int lock_type;

	/* release unneeded ancestors */
	elem->flags = first(elem) |
		      overflow(brec->tree, elem->bnr.bn) |
		      underflow(brec->tree, elem->bnr.bn);
	if (!(brec->keep_flags & elem->flags)) {
		hfs_brec_relse(brec, brec->bottom-1);
	} else if ((brec->bottom-2 >= brec->top) &&
		   !(elem->flags & (elem-1)->flags)) {
		hfs_brec_relse(brec, brec->bottom-2);
	}

	node = hfs_get_hl(belem_record(elem));
	lock_type = elem->bnr.lock_type;

	if (!node || hfs_bnode_in_brec(node, brec)) {
		hfs_warn("hfs_bfind: corrupt btree\n");
		hfs_brec_relse(brec, NULL);
		return NULL;
	}

	++elem;
	++brec->bottom;

	elem->bnr = hfs_bnode_find(brec->tree, node, lock_type);
	if (!elem->bnr.bn) {
		hfs_brec_relse(brec, NULL);
		return NULL;
	}
	elem->record = elem->bnr.bn->ndNRecs;

	return elem;
}

/*
 * hfs_brec_lock()
 *
 * Description:
 *   This function obtains HFS_LOCK_WRITE access to the bnode
 *   containing this hfs_brec.	All descendents in the path from this
 *   record to the leaf are given HFS_LOCK_WRITE access and all
 *   ancestors in the path from the root to here are released.
 * Input Variable(s):
 *   struct hfs_brec *brec: pointer to the brec to obtain
 *    HFS_LOCK_WRITE access to some of the nodes of.
 *   struct hfs_belem *elem: the first node to lock or NULL for all
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'brec' points to a "valid" (struct hfs_brec)
 * Postconditions: 
 *   All nodes between the indicated node and the beginning of the path
 *    are released.  hfs_bnode_lock() is called in turn on each node
 *    from the indicated node to the leaf node of the path, with a
 *    lock_type argument of HFS_LOCK_WRITE.  If one of those calls
 *    results in deadlock, then this function will never return.
 */
void hfs_brec_lock(struct hfs_brec *brec, struct hfs_belem *elem) 
{
	if (!elem) {
		elem = brec->top;
	} else if (elem > brec->top) {
		hfs_brec_relse(brec, elem-1);
	}

	while (elem <= brec->bottom) {
		hfs_bnode_lock(&elem->bnr, HFS_LOCK_WRITE);
		++elem;
	}
}

/*
 * hfs_brec_init()
 *
 * Description:
 *   Obtain access to the root node of a B-tree.
 *   Note that this first must obtain access to the header node.
 * Input Variable(s):
 *   struct hfs_brec *brec: pointer to the (struct hfs_brec) to
 *    initialize
 *   struct hfs_btree *btree: pointer to the (struct hfs_btree)
 *   int lock_type: the type of access to get to the nodes.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   struct hfs_belem *: pointer to the root path element or NULL
 * Preconditions:
 *   'brec' points to a (struct hfs_brec).
 *   'tree' points to a valid (struct hfs_btree).
 * Postconditions:
 *   If the two calls to brec_bnode_find() succeed then the return value
 *   points to a (struct hfs_belem) which corresponds to the root node
 *   of 'brec->tree'.
 *   Both the root and header nodes are obtained with the type of lock
 *   given by (flags & HFS_LOCK_MASK).
 *   The fields 'record' field of the root is set to its last record.
 *   If the header node is not needed to complete the appropriate
 *   operation (as determined by the 'keep_flags' field of 'brec') then
 *   it is released before this function returns.
 *   If either call to brec_bnode_find() fails, NULL is returned and the
 *   (struct hfs_brec) pointed to by 'brec' is invalid.
 */
struct hfs_belem *hfs_brec_init(struct hfs_brec *brec, struct hfs_btree *tree,
				int flags)
{
	struct hfs_belem *head = &brec->elem[0];
	struct hfs_belem *root = &brec->elem[1];
	int lock_type = flags & HFS_LOCK_MASK;

	brec->tree = tree;

	head->bnr = hfs_bnode_find(tree, 0, lock_type);
	if (!head->bnr.bn) {
		return NULL;
	}

	root->bnr = hfs_bnode_find(tree, tree->bthRoot, lock_type);
	if (!root->bnr.bn) {
		hfs_bnode_relse(&head->bnr);
		return NULL;
	}

	root->record = root->bnr.bn->ndNRecs;
	
	brec->top = head;
	brec->bottom = root;
	
	brec->keep_flags = flags & HFS_BPATH_MASK;

	/* HFS_BPATH_FIRST not applicable for root */
	/* and HFS_BPATH_UNDERFLOW is different */
	root->flags = overflow(tree, root->bnr.bn);
	if (root->record < 3) {
		root->flags |= HFS_BPATH_UNDERFLOW;
	}

	if (!(root->flags & brec->keep_flags)) {
		hfs_brec_relse(brec, head);
	}

	return root;
}
