/*
 * linux/fs/hfs/binsert.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the code to insert records in a B-tree.
 *
 * "XXX" in a comment is a note to myself to consider changing something.
 *
 * In function preconditions the term "valid" applied to a pointer to
 * a structure means that the pointer is non-NULL and the structure it
 * points to has all fields initialized to consistent values.
 */

#include "hfs_btree.h"

/*================ File-local functions ================*/

/* btree locking functions */
static inline void hfs_btree_lock(struct hfs_btree *tree)
{
  while (tree->lock) 
    hfs_sleep_on(&tree->wait);
  tree->lock = 1;
}

static inline void hfs_btree_unlock(struct hfs_btree *tree)
{
  tree->lock = 0;
  hfs_wake_up(&tree->wait);
}

/*
 * binsert_nonfull()
 *
 * Description:
 *   Inserts a record in a given bnode known to have sufficient space.
 * Input Variable(s):
 *   struct hfs_brec* brec: pointer to the brec for the insertion
 *   struct hfs_belem* belem: the element in the search path to insert in
 *   struct hfs_bkey* key: pointer to the key for the record to insert
 *   void* data: pointer to the record to insert
 *   hfs_u16 keysize: size of the key to insert
 *   hfs_u16 datasize: size of the record to insert
 * Output Variable(s):
 *   NONE
 * Returns:
 *   NONE
 * Preconditions:
 *   'brec' points to a valid (struct hfs_brec).
 *   'belem' points to a valid (struct hfs_belem) in 'brec', the node
 *    of which has enough free space to insert 'key' and 'data'.
 *   'key' is a pointer to a valid (struct hfs_bkey) of length 'keysize'
 *    which, in sorted order, belongs at the location indicated by 'brec'.
 *   'data' is non-NULL an points to appropriate data of length 'datasize'
 * Postconditions:
 *   The record has been inserted in the position indicated by 'brec'.
 */
static void binsert_nonfull(struct hfs_brec *brec, struct hfs_belem *belem,
			    const struct hfs_bkey *key, const void *data,
			    hfs_u8 keysize, hfs_u16 datasize)
{
	int i, rec, nrecs, size, tomove;
	hfs_u8 *start;
	struct hfs_bnode *bnode = belem->bnr.bn;

	rec = ++(belem->record);
	size = ROUND(keysize+1) + datasize;
	nrecs = bnode->ndNRecs + 1;
	tomove = bnode_offset(bnode, nrecs) - bnode_offset(bnode, rec);
	
	/* adjust the record table */
	for (i = nrecs; i >= rec; --i) {
		hfs_put_hs(bnode_offset(bnode,i) + size, RECTBL(bnode,i+1));
	}

	/* make room */
	start = bnode_key(bnode, rec);
	memmove(start + size, start, tomove);

	/* copy in the key and the data*/
	*start = keysize;
	keysize = ROUND(keysize+1);
	memcpy(start + 1, (hfs_u8 *)key + 1, keysize-1);
	memcpy(start + keysize, data, datasize);

	/* update record count */
	++bnode->ndNRecs;
}

/*
 * add_root()
 *
 * Description:
 *   Adds a new root to a B*-tree, increasing its height.
 * Input Variable(s):
 *   struct hfs_btree *tree: the tree to add a new root to
 *   struct hfs_bnode *left: the new root's first child or NULL
 *   struct hfs_bnode *right: the new root's second child or NULL
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'tree' points to a valid (struct hfs_btree).
 *   'left' and 'right' point to valid (struct hfs_bnode)s, which
 *    resulted from splitting the old root node, or are both NULL
 *    if there was no root node before.
 * Postconditions:
 *   Upon success a new root node is added to 'tree' with either
 *    two children ('left' and 'right') or none.
 */
static void add_root(struct hfs_btree *tree,
		     struct hfs_bnode *left,
		     struct hfs_bnode *right)
{
	struct hfs_bnode_ref bnr;
	struct hfs_bnode *root;
	struct hfs_bkey *key;
	int keylen = tree->bthKeyLen;

	if (left && !right) {
		hfs_warn("add_root: LEFT but no RIGHT\n");
		return;
	}

	bnr = hfs_bnode_alloc(tree);
	if (!(root = bnr.bn)) {
		return;
	}

	root->sticky = HFS_STICKY;
	tree->root = root;
	tree->bthRoot = root->node;
	++tree->bthDepth;

	root->ndNHeight = tree->bthDepth;
	root->ndFLink = 0;
	root->ndBLink = 0;

	if (!left) {
		/* tree was empty */
		root->ndType  = ndLeafNode;
		root->ndNRecs = 0;

		tree->bthFNode = root->node;
		tree->bthLNode = root->node;
	} else {
		root->ndType  = ndIndxNode;
		root->ndNRecs = 2;

		hfs_put_hs(sizeof(struct NodeDescriptor) + ROUND(1+keylen) +
			   sizeof(hfs_u32), RECTBL(root, 2));
		key = bnode_key(root, 1);
		key->KeyLen = keylen;
		memcpy(key->value,
		       ((struct hfs_bkey *)bnode_key(left, 1))->value, keylen);
		hfs_put_hl(left->node, bkey_record(key));
		
		hfs_put_hs(sizeof(struct NodeDescriptor) + 2*ROUND(1+keylen) +
			   2*sizeof(hfs_u32), RECTBL(root, 3));
		key = bnode_key(root, 2);
		key->KeyLen = keylen;
		memcpy(key->value,
		       ((struct hfs_bkey *)bnode_key(right, 1))->value, keylen);
		hfs_put_hl(right->node, bkey_record(key));

		/* the former root (left) is now just a normal node */
		left->sticky = HFS_NOT_STICKY;
		if ((left->next = bhash(tree, left->node))) {
			left->next->prev = left;
		}
		bhash(tree, left->node) = left;
	}
	hfs_bnode_relse(&bnr);
	tree->dirt = 1;
}

/*
 * insert_empty_bnode()
 *
 * Description:
 *   Adds an empty node to the right of 'left'.
 * Input Variable(s):
 *   struct hfs_btree *tree: the tree to add a node to
 *   struct hfs_bnode *left: the node to add a node after
 * Output Variable(s):
 *   NONE
 * Returns:
 *   struct hfs_bnode_ref *: reference to the new bnode.
 * Preconditions:
 *   'tree' points to a valid (struct hfs_btree) with at least 1 free node.
 *   'left' points to a valid (struct hfs_bnode) belonging to 'tree'.
 * Postconditions:
 *   If NULL is returned then 'tree' and 'left' are unchanged.
 *   Otherwise a node with 0 records is inserted in the tree to the right
 *   of the node 'left'.  The 'ndFLink' of 'left' and the 'ndBLink' of
 *   the former right-neighbor of 'left' (if one existed) point to the
 *   new node.	If 'left' had no right neighbor and is a leaf node the
 *   the 'bthLNode' of 'tree' points to the new node.  The free-count and
 *   bitmap for 'tree' are kept current by hfs_bnode_alloc() which supplies
 *   the required node.
 */
static struct hfs_bnode_ref insert_empty_bnode(struct hfs_btree *tree,
					       struct hfs_bnode *left)
{
	struct hfs_bnode_ref retval;
	struct hfs_bnode_ref right;

	retval = hfs_bnode_alloc(tree);
	if (!retval.bn) {
		hfs_warn("hfs_binsert: out of bnodes?.\n");
		goto done;
	}
	retval.bn->sticky = HFS_NOT_STICKY;
	if ((retval.bn->next = bhash(tree, retval.bn->node))) {
		retval.bn->next->prev = retval.bn;
	}
	bhash(tree, retval.bn->node) = retval.bn;

	if (left->ndFLink) {
		right = hfs_bnode_find(tree, left->ndFLink, HFS_LOCK_WRITE);
		if (!right.bn) {
			hfs_warn("hfs_binsert: corrupt btree.\n");
			hfs_bnode_bitop(tree, retval.bn->node, 0);
			hfs_bnode_relse(&retval);
			goto done;
		}
		right.bn->ndBLink = retval.bn->node;
		hfs_bnode_relse(&right);
	} else if (left->ndType == ndLeafNode) {
		tree->bthLNode = retval.bn->node;
		tree->dirt = 1;
	}

	retval.bn->ndFLink   = left->ndFLink;
	retval.bn->ndBLink   = left->node;
	retval.bn->ndType    = left->ndType;
	retval.bn->ndNHeight = left->ndNHeight;
	retval.bn->ndNRecs   = 0;

	left->ndFLink = retval.bn->node;

 done:
	return retval;
}

/*
 * split()
 *
 * Description:
 *   Splits an over full node during insertion.
 *   Picks the split point that results in the most-nearly equal
 *   space usage in the new and old nodes.
 * Input Variable(s):
 *   struct hfs_belem *elem: the over full node.
 *   int size: the number of bytes to be used by the new record and its key.
 * Output Variable(s):
 *   struct hfs_belem *elem: changed to indicate where the new record
 *    should be inserted.
 * Returns:
 *   struct hfs_bnode_ref: reference to the new bnode.
 * Preconditions:
 *   'elem' points to a valid path element corresponding to the over full node.
 *   'size' is positive.
 * Postconditions:
 *   The records in the node corresponding to 'elem' are redistributed across
 *   the old and new nodes so that after inserting the new record, the space
 *   usage in these two nodes is as equal as possible.
 *   'elem' is updated so that a call to binsert_nonfull() will insert the
 *   new record in the correct location.
 */
static inline struct hfs_bnode_ref split(struct hfs_belem *elem, int size)
{
	struct hfs_bnode *bnode = elem->bnr.bn;
	int nrecs, cutoff, index, tmp, used, in_right;
	struct hfs_bnode_ref right;

	right = insert_empty_bnode(bnode->tree, bnode);
	if (right.bn) {
		nrecs = bnode->ndNRecs;
		cutoff = (size + bnode_end(bnode) -
			      sizeof(struct NodeDescriptor) +
			      (nrecs+1)*sizeof(hfs_u16))/2;
		used = 0;
		in_right = 1;
		/* note that this only works because records sizes are even */
		for (index=1; index <= elem->record; ++index) {
			tmp = (sizeof(hfs_u16) + bnode_rsize(bnode, index))/2;
			used += tmp;
			if (used > cutoff) {
				goto found;
			}
			used += tmp;
		}
		tmp = (size + sizeof(hfs_u16))/2;
		used += tmp;
		if (used > cutoff) {
			goto found;
		}
		in_right = 0;
		used += tmp;
		for (; index <= nrecs; ++index) {
			tmp = (sizeof(hfs_u16) + bnode_rsize(bnode, index))/2;
			used += tmp;
			if (used > cutoff) {
				goto found;
			}
			used += tmp;
		}
		/* couldn't find the split point! */
		hfs_bnode_relse(&right);
	}
	return right;

found:
	if (in_right) {
		elem->bnr = right;
		elem->record -= index-1;
	}
	hfs_bnode_shift_right(bnode, right.bn, index);

	return right;
}

/*
 * binsert()
 *
 * Description:
 *   Inserts a record in a tree known to have enough room, even if the
 *   insertion requires the splitting of nodes.
 * Input Variable(s):
 *    struct hfs_brec *brec: partial path to the node to insert in
 *    const struct hfs_bkey *key: key for the new record
 *    const void *data: data for the new record
 *    hfs_u8 keysize: size of the key
 *    hfs_u16 datasize: size of the data
 *    int reserve: number of nodes reserved in case of splits
 * Output Variable(s):
 *    *brec = NULL
 * Returns:
 *    int: 0 on success, error code on failure
 * Preconditions:
 *    'brec' points to a valid (struct hfs_brec) corresponding to a
 *     record in a leaf node, after which a record is to be inserted,
 *     or to "record 0" of the leaf node if the record is to be inserted
 *     before all existing records in the node.	 The (struct hfs_brec)
 *     includes all ancestors of the leaf node that are needed to
 *     complete the insertion including the parents of any nodes that
 *     will be split.
 *    'key' points to a valid (struct hfs_bkey) which is appropriate
 *     to this tree, and which belongs at the insertion point.
 *    'data' points data appropriate for the indicated node.
 *    'keysize' gives the size in bytes of the key.
 *    'datasize' gives the size in bytes of the data.
 *    'reserve' gives the number of nodes that have been reserved in the
 *     tree to allow for splitting of nodes.
 * Postconditions:
 *    All 'reserve'd nodes have been either used or released.
 *    *brec = NULL
 *    On success the key and data have been inserted at the indicated
 *    location in the tree, all appropriate fields of the in-core data
 *    structures have been changed and updated versions of the on-disk
 *    data structures have been scheduled for write-back to disk.
 *    On failure the B*-tree is probably invalid both on disk and in-core.
 *
 *    XXX: Some attempt at repair might be made in the event of failure,
 *    or the fs should be remounted read-only so things don't get worse.
 */
static int binsert(struct hfs_brec *brec, const struct hfs_bkey *key,
		   const void *data, hfs_u8 keysize, hfs_u16 datasize,
		   int reserve)
{
	struct hfs_bnode_ref left, right, other;
	struct hfs_btree *tree = brec->tree;
	struct hfs_belem *belem = brec->bottom;
	int tmpsize = 1 + tree->bthKeyLen;
	struct hfs_bkey *tmpkey = hfs_malloc(tmpsize);
	hfs_u32 node;
	
	while ((belem >= brec->top) && (belem->flags & HFS_BPATH_OVERFLOW)) {
		left = belem->bnr;
		if (left.bn->ndFLink &&
                    hfs_bnode_in_brec(left.bn->ndFLink, brec)) {
			hfs_warn("hfs_binsert: corrupt btree\n");
			tree->reserved -= reserve;
			hfs_free(tmpkey, tmpsize);
			return -EIO;
		}
			
		right = split(belem, ROUND(keysize+1) + ROUND(datasize));
		--reserve;
		--tree->reserved;
		if (!right.bn) {
			hfs_warn("hfs_binsert: unable to split node!\n");
			tree->reserved -= reserve;
			hfs_free(tmpkey, tmpsize);
			return -ENOSPC;
		}
		binsert_nonfull(brec, belem, key, data, keysize, datasize);
	
		if (belem->bnr.bn == left.bn) {
			other = right;
			if (belem->record == 1) {
				hfs_bnode_update_key(brec, belem, left.bn, 0);
			}
		} else {
			other = left;
		}

		if (left.bn->node == tree->root->node) {
			add_root(tree, left.bn, right.bn);
			hfs_bnode_relse(&other);
			goto done;
		}

		data = &node;
		datasize = sizeof(node);
		node = htonl(right.bn->node);
		key = tmpkey;
		keysize = tree->bthKeyLen;
		memcpy(tmpkey, bnode_key(right.bn, 1), keysize+1);
		hfs_bnode_relse(&other);
		
		--belem;
	}

	if (belem < brec->top) {
		hfs_warn("hfs_binsert: Missing parent.\n");
		tree->reserved -= reserve;
		hfs_free(tmpkey, tmpsize);
		return -EIO;
	}

	binsert_nonfull(brec, belem, key, data, keysize, datasize);

done:
	tree->reserved -= reserve;
	hfs_free(tmpkey, tmpsize);
	return 0;
}

/*================ Global functions ================*/

/*
 * hfs_binsert()
 *
 * Description:
 *   This function inserts a new record into a b-tree.
 * Input Variable(s):
 *   struct hfs_btree *tree: pointer to the (struct hfs_btree) to insert in
 *   struct hfs_bkey *key: pointer to the (struct hfs_bkey) to insert
 *   void *data: pointer to the data to associate with 'key' in the b-tree
 *   unsigned int datasize: the size of the data
 * Output Variable(s):
 *   NONE
 * Returns:
 *   int: 0 on success, error code on failure
 * Preconditions:
 *   'tree' points to a valid (struct hfs_btree)
 *   'key' points to a valid (struct hfs_bkey)
 *   'data' points to valid memory of length 'datasize'
 * Postconditions:
 *   If zero is returned then the record has been inserted in the
 *    indicated location updating all in-core data structures and
 *    scheduling all on-disk data structures for write-back.
 */
int hfs_binsert(struct hfs_btree *tree, const struct hfs_bkey *key,
		const void *data, hfs_u16 datasize)
{ 
	struct hfs_brec brec;
	struct hfs_belem *belem;
	int err, reserve, retval;
	hfs_u8 keysize;

	if (!tree || (tree->magic != HFS_BTREE_MAGIC) || !key || !data) {
		hfs_warn("hfs_binsert: invalid arguments.\n");
		return -EINVAL;
	}

	if (key->KeyLen > tree->bthKeyLen) {
		hfs_warn("hfs_binsert: oversized key\n");
		return -EINVAL;
	}

restart:
	if (!tree->bthNRecs) {
		/* create the root bnode */
		add_root(tree, NULL, NULL);
		if (!hfs_brec_init(&brec, tree, HFS_BFIND_INSERT)) {
			hfs_warn("hfs_binsert: failed to create root.\n");
			return -ENOSPC;
		}
	} else {
		err = hfs_bfind(&brec, tree, key, HFS_BFIND_INSERT);
		if (err < 0) {
			hfs_warn("hfs_binsert: hfs_brec_find failed.\n");
			return err;
		} else if (err == 0) {
			hfs_brec_relse(&brec, NULL);
			return -EEXIST;
		}
	}

	keysize = key->KeyLen;
	datasize = ROUND(datasize);
	belem = brec.bottom;
	belem->flags = 0;
	if (bnode_freespace(belem->bnr.bn) <
			    (sizeof(hfs_u16) + ROUND(keysize+1) + datasize)) {
		belem->flags |= HFS_BPATH_OVERFLOW;
	}
	if (belem->record == 0) {
		belem->flags |= HFS_BPATH_FIRST;
	}

	if (!belem->flags) {
		hfs_brec_lock(&brec, brec.bottom);
		reserve = 0;
	} else {
		reserve = brec.bottom - brec.top;
		if (brec.top == 0) {
			++reserve;
		}
		/* make certain we have enough nodes to proceed */
		if ((tree->bthFree - tree->reserved) < reserve) {
			hfs_brec_relse(&brec, NULL);
			hfs_btree_lock(tree);
			if ((tree->bthFree - tree->reserved) < reserve) {
				hfs_btree_extend(tree);
			}
			hfs_btree_unlock(tree);
			if ((tree->bthFree - tree->reserved) < reserve) {
				return -ENOSPC;
			} else {
				goto restart;
			}
		}
		tree->reserved += reserve;
		hfs_brec_lock(&brec, NULL);
	}

	retval = binsert(&brec, key, data, keysize, datasize, reserve);
	hfs_brec_relse(&brec, NULL);
	if (!retval) {
		++tree->bthNRecs;
		tree->dirt = 1;
	}
	return retval;
}
