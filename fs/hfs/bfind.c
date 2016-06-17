/*
 * linux/fs/hfs/bfind.c
 *
 * Copyright (C) 1995, 1996  Paul H. Hargrove
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

/*================ Global functions ================*/

/*
 * hfs_brec_relse()
 *
 * Description:
 *   This function releases some of the nodes associated with a brec.
 * Input Variable(s):
 *   struct hfs_brec *brec: pointer to the brec to release some nodes from.
 *   struct hfs_belem *elem: the last node to release or NULL for all
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'brec' points to a "valid" (struct hfs_brec)
 * Postconditions: 
 *   All nodes between the indicated node and the beginning of the path
 *    are released.
 */
void hfs_brec_relse(struct hfs_brec *brec, struct hfs_belem *elem)
{
	if (!elem) {
		elem = brec->bottom;
	}

	while (brec->top <= elem) {
		hfs_bnode_relse(&brec->top->bnr);
		++brec->top;
	}
}

/*
 * hfs_bfind()
 *
 * Description:
 *   This function has sole responsibility for locating existing
 *   records in a B-tree.  Given a B-tree and a key it locates the
 *   "greatest" record "less than or equal to" the given key.  The
 *   exact behavior is determined by the bits of the flags variable as
 *   follows:
 *     ('flags' & HFS_LOCK_MASK):
 *      The lock_type argument to be used when calling hfs_bnode_find().
 *     HFS_BFIND_EXACT: only accept an exact match, otherwise take the
 *	"largest" record less than 'target' as a "match"
 *     HFS_BFIND_LOCK: request HFS_LOCK_WRITE access to the node containing
 *	the "matching" record when it is located
 *     HFS_BPATH_FIRST: keep access to internal nodes when accessing their
 *      first child.
 *     HFS_BPATH_OVERFLOW: keep access to internal nodes when the accessed
 *      child is too full to insert another pointer record.
 *     HFS_BPATH_UNDERFLOW: keep access to internal nodes when the accessed
 *      child is would be less than half full upon removing a pointer record.
 * Input Variable(s):
 *   struct hfs_brec *brec: pointer to the (struct hfs_brec) to hold
 *    the search results.
 *   struct hfs_bkey *target: pointer to the (struct hfs_bkey)
 *    to search for
 *   int flags: bitwise OR of flags which determine the function's behavior
 * Output Variable(s):
 *   'brec' contains the results of the search on success or is invalid
 *    on failure.
 * Returns:
 *   int: 0 or 1 on success or an error code on failure:
 *     -EINVAL: one of the input variables was NULL.
 *     -ENOENT: tree is valid but empty or no "matching" record was located.
 *	 If the HFS_BFIND_EXACT bit of 'flags' is not set then the case of no
 *	 matching record will give a 'brec' with a 'record' field of zero
 *	 rather than returning this error.
 *     -EIO: an I/O operation or an assertion about the structure of a
 *       valid B-tree failed indicating corruption of either the B-tree
 *       structure on the disk or one of the in-core structures representing
 *       the B-tree.
 *	 (This could also be returned if a kmalloc() call failed in a
 *	 subordinate routine that is intended to get the data from the
 *	 disk or the buffer cache.)
 * Preconditions:
 *   'brec' is NULL or points to a (struct hfs_brec) with a 'tree' field
 *    which points to a valid (struct hfs_btree).
 *   'target' is NULL or points to a "valid" (struct hfs_bkey)
 * Postconditions:
 *   If 'brec', 'brec->tree' or 'target' is NULL then -EINVAL is returned.
 *   If 'brec', 'brec->tree' and 'target' are non-NULL but the tree
 *   is empty then -ENOENT is returned.
 *   If 'brec', 'brec->tree' and 'target' are non-NULL but the call to
 *   hfs_brec_init() fails then '*brec' is NULL and -EIO is returned.
 *   If 'brec', 'brec->tree' and 'target' are non-NULL and the tree is
 *   non-empty then the tree is searched as follows:
 *    If any call to hfs_brec_next() fails or returns a node that is
 *     neither an index node nor a leaf node then -EIO is returned to
 *     indicate that the B-tree or buffer-cache are corrupted.
 *    If every record in the tree is "greater than" the given key
 *     and the HFS_BFIND_EXACT bit of 'flags' is set then -ENOENT is returned.
 *    If every record in the tree is "greater than" the given key
 *     and the HFS_BFIND_EXACT bit of 'flags' is clear then 'brec' refers
 *     to the first leaf node in the tree and has a 'record' field of
 *     zero, and 1 is returned.
 *    If a "matching" record is located with key "equal to" 'target'
 *     then the return value is 0 and 'brec' indicates the record.
 *    If a "matching" record is located with key "greater than" 'target'
 *     then the behavior is determined as follows:
 *	If the HFS_BFIND_EXACT bit of 'flags' is not set then 1 is returned
 *       and 'brec' refers to the "matching" record.
 *	If the HFS_BFIND_EXACT bit of 'flags' is set then -ENOENT is returned.
 *    If the return value is non-negative and the HFS_BFIND_LOCK bit of
 *     'flags' is set then hfs_brec_lock() is called on the bottom element
 *     of 'brec' before returning.
 */
int hfs_bfind(struct hfs_brec *brec, struct hfs_btree *tree,
	      const struct hfs_bkey *target, int flags)
{
	struct hfs_belem *curr;
	struct hfs_bkey *key;
	struct hfs_bnode *bn;
	int result, ntype;

	/* check for invalid arguments */
	if (!brec || (tree->magic != HFS_BTREE_MAGIC) || !target) {
		return -EINVAL;
	}

	/* check for empty tree */
	if (!tree->root || !tree->bthNRecs) {
		return -ENOENT;
	}

	/* start search at root of tree */
	if (!(curr = hfs_brec_init(brec, tree, flags))) {
		return -EIO;
	}

	/* traverse the tree */
	do {
		bn = curr->bnr.bn;

		if (!curr->record) {
			hfs_warn("hfs_bfind: empty bnode\n");
			hfs_brec_relse(brec, NULL);
			return -EIO;
		}

		/* reverse linear search yielding largest key "less
		   than or equal to" 'target'.
		   It is questionable whether a binary search would be
		   significantly faster */
		do {
			key = belem_key(curr);
			if (!key->KeyLen) {
				hfs_warn("hfs_bfind: empty key\n");
				hfs_brec_relse(brec, NULL);
				return -EIO;
			}
			result = (tree->compare)(target, key);
		} while ((result<0) && (--curr->record));

		ntype = bn->ndType;

		/* see if all keys > target */
		if (!curr->record) {
			if (bn->ndBLink) {
				/* at a node other than the left-most at a
				   given level it means the parent had an
				   incorrect key for this child */
				hfs_brec_relse(brec, NULL);
				hfs_warn("hfs_bfind: corrupted b-tree %d.\n",
					 (int)ntohl(tree->entry.cnid));
				return -EIO;
			}
			if (flags & HFS_BFIND_EXACT) {
				/* we're not going to find it */
				hfs_brec_relse(brec, NULL);
				return -ENOENT;
			}
			if (ntype == ndIndxNode) {
				/* since we are at the left-most node at
				   the current level and looking for the
				   predecessor of 'target' keep going down */
				curr->record = 1;
			} else {
				/* we're at first leaf so fall through */
			}
		}

		/* get next node if necessary */
		if ((ntype == ndIndxNode) && !(curr = hfs_brec_next(brec))) {
			return -EIO;
		}
	} while (ntype == ndIndxNode);

	if (key->KeyLen > tree->bthKeyLen) {
		hfs_warn("hfs_bfind: oversized key\n");
		hfs_brec_relse(brec, NULL);
		return -EIO;
	}

	if (ntype != ndLeafNode) {
		hfs_warn("hfs_bfind: invalid node type %02x in node %d of "
		         "btree %d\n", bn->ndType, bn->node,
		         (int)ntohl(tree->entry.cnid));
		hfs_brec_relse(brec, NULL);
		return -EIO;
	}

	if ((flags & HFS_BFIND_EXACT) && result) {
		hfs_brec_relse(brec, NULL);
		return -ENOENT;
	}

	if (!(flags & HFS_BPATH_MASK)) {
		hfs_brec_relse(brec, brec->bottom-1);
	}

	if (flags & HFS_BFIND_LOCK) {
		hfs_brec_lock(brec, brec->bottom);
	}

	brec->key  = brec_key(brec);
	brec->data = bkey_record(brec->key);

	return result ? 1 : 0;
}

/*
 * hfs_bsucc()
 *
 * Description:
 *   This function overwrites '*brec' with its successor in the B-tree,
 *   obtaining the same type of access.
 * Input Variable(s):
 *   struct hfs_brec *brec: address of the (struct hfs_brec) to overwrite
 *    with its successor
 * Output Variable(s):
 *   struct hfs_brec *brec: address of the successor of the original
 *    '*brec' or to invalid data
 * Returns:
 *   int: 0 on success, or one of -EINVAL, -EIO, or -EINVAL on failure
 * Preconditions:
 *   'brec' pointers to a "valid" (struct hfs_brec)
 * Postconditions:
 *   If the given '*brec' is not "valid" -EINVAL is returned and
 *    '*brec' is unchanged.
 *   If the given 'brec' is "valid" but has no successor then -ENOENT
 *    is returned and '*brec' is invalid.
 *   If a call to hfs_bnode_find() is necessary to find the successor,
 *    but fails then -EIO is returned and '*brec' is invalid.
 *   If none of the three previous conditions prevents finding the
 *    successor of '*brec', then 0 is returned, and '*brec' is overwritten
 *    with the (struct hfs_brec) for its successor.
 *   In the cases when '*brec' is invalid, the old records is freed.
 */
int hfs_bsucc(struct hfs_brec *brec, int count)
{
	struct hfs_belem *belem;
	struct hfs_bnode *bn;

	if (!brec || !(belem = brec->bottom) || (belem != brec->top) ||
	    !(bn = belem->bnr.bn) || (bn->magic != HFS_BNODE_MAGIC) ||
	    !bn->tree || (bn->tree->magic != HFS_BTREE_MAGIC) ||
	    !hfs_buffer_ok(bn->buf)) {
		hfs_warn("hfs_bsucc: invalid/corrupt arguments.\n");
		return -EINVAL;
	}

	while (count) {
		int left = bn->ndNRecs - belem->record;

		if (left < count) {
			struct hfs_bnode_ref old;
			hfs_u32 node;

			/* Advance to next node */
			if (!(node = bn->ndFLink)) {
				hfs_brec_relse(brec, belem);
				return -ENOENT;
			}
			if (node == bn->node) {
				hfs_warn("hfs_bsucc: corrupt btree\n");
				hfs_brec_relse(brec, belem);
				return -EIO;
			}
			old = belem->bnr;
			belem->bnr = hfs_bnode_find(brec->tree, node,
						    belem->bnr.lock_type);
			hfs_bnode_relse(&old);
			if (!(bn = belem->bnr.bn)) {
				return -EIO;
			}
			belem->record = 1;
			count -= (left + 1);
		} else {
			belem->record += count;
			break;
		}
	}
	brec->key  = belem_key(belem);
	brec->data = bkey_record(brec->key);

	if (brec->key->KeyLen > brec->tree->bthKeyLen) {
		hfs_warn("hfs_bsucc: oversized key\n");
		hfs_brec_relse(brec, NULL);
		return -EIO;
	}

	return 0;
}
