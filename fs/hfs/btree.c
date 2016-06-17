/*
 * linux/fs/hfs/btree.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the code to manipulate the B-tree structure.
 * The catalog and extents files are both B-trees.
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
 * hfs_bnode_ditch() 
 *
 * Description:
 *   This function deletes an entire linked list of bnodes, so it
 *   does not need to keep the linked list consistent as
 *   hfs_bnode_delete() does.
 *   Called by hfs_btree_init() for error cleanup and by hfs_btree_free().
 * Input Variable(s):
 *   struct hfs_bnode *bn: pointer to the first (struct hfs_bnode) in
 *    the linked list to be deleted.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'bn' is NULL or points to a "valid" (struct hfs_bnode) with a 'prev'
 *    field of NULL.
 * Postconditions:
 *   'bn' and all (struct hfs_bnode)s in the chain of 'next' pointers
 *   are deleted, freeing the associated memory and hfs_buffer_put()ing
 *   the associated buffer.
 */
static void hfs_bnode_ditch(struct hfs_bnode *bn) {
	struct hfs_bnode *tmp;
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	extern int bnode_count;
#endif

	while (bn != NULL) {
		tmp = bn->next;
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
		hfs_warn("deleting node %d from tree %d with count %d\n",
		         bn->node, (int)ntohl(bn->tree->entry.cnid), bn->count);
		--bnode_count;
#endif
		hfs_buffer_put(bn->buf); /* safe: checks for NULL argument */

		/* free all but the header */
		if (bn->node) {
			HFS_DELETE(bn);
		}
		bn = tmp;
	}
}

/*================ Global functions ================*/

/*
 * hfs_btree_free()
 *
 * Description:
 *   This function frees a (struct hfs_btree) obtained from hfs_btree_init().
 *   Called by hfs_put_super().
 * Input Variable(s):
 *   struct hfs_btree *bt: pointer to the (struct hfs_btree) to free
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'bt' is NULL or points to a "valid" (struct hfs_btree)
 * Postconditions:
 *   If 'bt' points to a "valid" (struct hfs_btree) then all (struct
 *    hfs_bnode)s associated with 'bt' are freed by calling
 *    hfs_bnode_ditch() and the memory associated with the (struct
 *    hfs_btree) is freed.
 *   If 'bt' is NULL or not "valid" an error is printed and nothing
 *    is changed.
 */
void hfs_btree_free(struct hfs_btree *bt)
{
	int lcv;

	if (bt && (bt->magic == HFS_BTREE_MAGIC)) {
		hfs_extent_free(&bt->entry.u.file.data_fork);

		for (lcv=0; lcv<HFS_CACHELEN; ++lcv) {
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
			hfs_warn("deleting nodes from bucket %d:\n", lcv);
#endif
			hfs_bnode_ditch(bt->cache[lcv]);
		}

#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
		hfs_warn("deleting header and bitmap nodes\n");
#endif
		hfs_bnode_ditch(&bt->head);

#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
		hfs_warn("deleting root node\n");
#endif
		hfs_bnode_ditch(bt->root);

		HFS_DELETE(bt);
	} else if (bt) {
		hfs_warn("hfs_btree_free: corrupted hfs_btree.\n");
	}
}

/*
 * hfs_btree_init()
 *
 * Description:
 *   Given some vital information from the MDB (HFS superblock),
 *   initializes the fields of a (struct hfs_btree).
 * Input Variable(s):
 *   struct hfs_mdb *mdb: pointer to the MDB
 *   ino_t cnid: the CNID (HFS_CAT_CNID or HFS_EXT_CNID) of the B-tree
 *   hfs_u32 tsize: the size, in bytes, of the B-tree
 *   hfs_u32 csize: the size, in bytes, of the clump size for the B-tree
 * Output Variable(s):
 *   NONE
 * Returns:
 *   (struct hfs_btree *): pointer to the initialized hfs_btree on success,
 *    or NULL on failure
 * Preconditions:
 *   'mdb' points to a "valid" (struct hfs_mdb)
 * Postconditions:
 *   Assuming the inputs are what they claim to be, no errors occur
 *   reading from disk, and no inconsistencies are noticed in the data
 *   read from disk, the return value is a pointer to a "valid"
 *   (struct hfs_btree).  If there are errors reading from disk or
 *   inconsistencies are noticed in the data read from disk, then and
 *   all resources that were allocated are released and NULL is
 *   returned.	If the inputs are not what they claim to be or if they
 *   are unnoticed inconsistencies in the data read from disk then the
 *   returned hfs_btree is probably going to lead to errors when it is
 *   used in a non-trivial way.
 */
struct hfs_btree * hfs_btree_init(struct hfs_mdb *mdb, ino_t cnid,
				  hfs_byte_t ext[12],
				  hfs_u32 tsize, hfs_u32 csize)
{
	struct hfs_btree * bt;
	struct BTHdrRec * th;
	struct hfs_bnode * tmp;
	unsigned int next;
#if defined(DEBUG_HEADER) || defined(DEBUG_ALL)
	unsigned char *p, *q;
#endif

	if (!mdb || !ext || !HFS_NEW(bt)) {
		goto bail3;
	}

	bt->magic = HFS_BTREE_MAGIC;
	bt->sys_mdb = mdb->sys_mdb;
	bt->reserved = 0;
	bt->lock = 0;
	hfs_init_waitqueue(&bt->wait);
	bt->dirt = 0;
	memset(bt->cache, 0, sizeof(bt->cache));

#if 0   /* this is a fake entry. so we don't need to initialize it. */
	memset(&bt->entry, 0, sizeof(bt->entry));
	hfs_init_waitqueue(&bt->entry.wait);
	INIT_LIST_HEAD(&bt->entry.hash);
	INIT_LIST_HEAD(&bt->entry.list);
#endif

	bt->entry.mdb = mdb;
	bt->entry.cnid = cnid;
	bt->entry.type = HFS_CDR_FIL;
	bt->entry.u.file.magic = HFS_FILE_MAGIC;
	bt->entry.u.file.clumpablks = (csize / mdb->alloc_blksz)
						>> HFS_SECTOR_SIZE_BITS;
	bt->entry.u.file.data_fork.entry = &bt->entry;
	bt->entry.u.file.data_fork.lsize = tsize;
	bt->entry.u.file.data_fork.psize = tsize >> HFS_SECTOR_SIZE_BITS;
	bt->entry.u.file.data_fork.fork = HFS_FK_DATA;
	hfs_extent_in(&bt->entry.u.file.data_fork, ext);

	hfs_bnode_read(&bt->head, bt, 0, HFS_STICKY);
	if (!hfs_buffer_ok(bt->head.buf)) {
		goto bail2;
	}
	th = (struct BTHdrRec *)((char *)hfs_buffer_data(bt->head.buf) +
						sizeof(struct NodeDescriptor));

	/* read in the bitmap nodes (if any) */
	tmp = &bt->head;
	while ((next = tmp->ndFLink)) {
		if (!HFS_NEW(tmp->next)) {
			goto bail2;
		}
		hfs_bnode_read(tmp->next, bt, next, HFS_STICKY);
		if (!hfs_buffer_ok(tmp->next->buf)) {
			goto bail2;
		}
		tmp->next->prev = tmp;
		tmp = tmp->next;
	}

	if (hfs_get_ns(th->bthNodeSize) != htons(HFS_SECTOR_SIZE)) {
		hfs_warn("hfs_btree_init: bthNodeSize!=512 not supported\n");
		goto bail2;
	}

	if (cnid == htonl(HFS_CAT_CNID)) {
		bt->compare = (hfs_cmpfn)hfs_cat_compare;
	} else if (cnid == htonl(HFS_EXT_CNID)) {
		bt->compare = (hfs_cmpfn)hfs_ext_compare;
	} else {
		goto bail2;
	}
	bt->bthDepth  = hfs_get_hs(th->bthDepth);
	bt->bthRoot   = hfs_get_hl(th->bthRoot);
	bt->bthNRecs  = hfs_get_hl(th->bthNRecs);
	bt->bthFNode  = hfs_get_hl(th->bthFNode);
	bt->bthLNode  = hfs_get_hl(th->bthLNode);
	bt->bthNNodes = hfs_get_hl(th->bthNNodes);
	bt->bthFree   = hfs_get_hl(th->bthFree);
	bt->bthKeyLen = hfs_get_hs(th->bthKeyLen);

#if defined(DEBUG_HEADER) || defined(DEBUG_ALL)
	hfs_warn("bthDepth %d\n", bt->bthDepth);
	hfs_warn("bthRoot %d\n", bt->bthRoot);
	hfs_warn("bthNRecs %d\n", bt->bthNRecs);
	hfs_warn("bthFNode %d\n", bt->bthFNode);
	hfs_warn("bthLNode %d\n", bt->bthLNode);
	hfs_warn("bthKeyLen %d\n", bt->bthKeyLen);
	hfs_warn("bthNNodes %d\n", bt->bthNNodes);
	hfs_warn("bthFree %d\n", bt->bthFree);
	p = (unsigned char *)hfs_buffer_data(bt->head.buf);
	q = p + HFS_SECTOR_SIZE;
	while (p < q) {
		hfs_warn("%02x %02x %02x %02x %02x %02x %02x %02x "
		         "%02x %02x %02x %02x %02x %02x %02x %02x\n",
			 *p++, *p++, *p++, *p++, *p++, *p++, *p++, *p++,
			 *p++, *p++, *p++, *p++, *p++, *p++, *p++, *p++);
	}
#endif

	/* Read in the root if it exists.
	   The header always exists, but the root exists only if the
	   tree is non-empty */
	if (bt->bthDepth && bt->bthRoot) {
		if (!HFS_NEW(bt->root)) {
			goto bail2;
		}
		hfs_bnode_read(bt->root, bt, bt->bthRoot, HFS_STICKY);
		if (!hfs_buffer_ok(bt->root->buf)) {
			goto bail1;
		}
	} else {
		bt->root = NULL;
	}

	return bt;

 bail1:
	hfs_bnode_ditch(bt->root);
 bail2:
	hfs_bnode_ditch(&bt->head);
	HFS_DELETE(bt);
 bail3:
	return NULL;
}

/*
 * hfs_btree_commit()
 *
 * Called to write a possibly dirty btree back to disk.
 */
void hfs_btree_commit(struct hfs_btree *bt, hfs_byte_t ext[12], hfs_lword_t size)
{
	if (bt->dirt) {
		struct BTHdrRec *th;
		th = (struct BTHdrRec *)((char *)hfs_buffer_data(bt->head.buf) +
						 sizeof(struct NodeDescriptor));

		hfs_put_hs(bt->bthDepth,  th->bthDepth);
		hfs_put_hl(bt->bthRoot,   th->bthRoot);
		hfs_put_hl(bt->bthNRecs,  th->bthNRecs);
		hfs_put_hl(bt->bthFNode,  th->bthFNode);
		hfs_put_hl(bt->bthLNode,  th->bthLNode);
		hfs_put_hl(bt->bthNNodes, th->bthNNodes);
		hfs_put_hl(bt->bthFree,   th->bthFree);
		hfs_buffer_dirty(bt->head.buf);

		/*
		 * Commit the bnodes which are not cached.
		 * The map nodes don't need to be committed here because
		 * they are committed every time they are changed.
		 */
		hfs_bnode_commit(&bt->head);
		if (bt->root) {
			hfs_bnode_commit(bt->root);
		}

	
		hfs_put_hl(bt->bthNNodes << HFS_SECTOR_SIZE_BITS, size);
		hfs_extent_out(&bt->entry.u.file.data_fork, ext);
		/* hfs_buffer_dirty(mdb->buf); (Done by caller) */

		bt->dirt = 0;
	}
}
