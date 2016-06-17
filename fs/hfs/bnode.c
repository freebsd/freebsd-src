/*
 * linux/fs/hfs/bnode.c
 *
 * Copyright (C) 1995-1997  Paul H. Hargrove
 * This file may be distributed under the terms of the GNU General Public License.
 *
 * This file contains the code to access nodes in the B-tree structure.
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

/*================ File-local variables ================*/
 
/* debugging statistics */
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
int bnode_count = 0;
#endif

/*================ Global functions ================*/

/*
 * hfs_bnode_delete()
 *
 * Description:
 *   This function is called to remove a bnode from the cache and
 *   release its resources.
 * Input Variable(s):
 *   struct hfs_bnode *bn: Pointer to the (struct hfs_bnode) to be
 *   removed from the cache.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'bn' points to a "valid" (struct hfs_bnode).
 * Postconditions:
 *   The node 'bn' is removed from the cache, its memory freed and its
 *   buffer (if any) released.
 */
void hfs_bnode_delete(struct hfs_bnode *bn)
{
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	--bnode_count;
#endif
	/* join neighbors */
	if (bn->next) {
		bn->next->prev = bn->prev;
	}
	if (bn->prev) {
		bn->prev->next = bn->next;
	}
	/* fix cache slot if necessary */
	if (bhash(bn->tree, bn->node) == bn) {
		bhash(bn->tree, bn->node) = bn->next;
	}
	/* release resources */
	hfs_buffer_put(bn->buf); /* safe: checks for NULL argument */
	HFS_DELETE(bn);
}


/*
 * hfs_bnode_read()
 *
 * Description: 
 *   This function creates a (struct hfs_bnode) and, if appropriate,
 *   inserts it in the cache.
 * Input Variable(s):
 *   struct hfs_bnode *bnode: pointer to the new bnode.
 *   struct hfs_btree *tree: pointer to the (struct hfs_btree)
 *    containing the desired node
 *   hfs_u32 node: the number of the desired node.
 *   int sticky: the value to assign to the 'sticky' field.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   (struct hfs_bnode *) pointing to the newly created bnode or NULL.
 * Preconditions:
 *   'bnode' points to a "valid" (struct hfs_bnode).
 *   'tree' points to a "valid" (struct hfs_btree).
 *   'node' is an existing node number in the B-tree.
 * Postconditions:
 *   The following are true of 'bnode' upon return:
 *    The 'magic' field is set to indicate a valid (struct hfs_bnode). 
 *    The 'sticky', 'tree' and 'node' fields are initialized to the
 *    values of the of the corresponding arguments.
 *    If the 'sticky' argument is zero then the fields 'prev' and
 *    'next' are initialized by inserting the (struct hfs_bnode) in the
 *    linked list of the appropriate cache slot; otherwise they are
 *    initialized to NULL.
 *    The data is read from disk (or buffer cache) and the 'buf' field
 *    points to the buffer for that data.
 *    If no other processes tried to access this node while this
 *    process was waiting on disk I/O (if necessary) then the
 *    remaining fields are zero ('count', 'resrv', 'lock') or NULL
 *    ('wqueue', 'rqueue') corresponding to no accesses.
 *    If there were access attempts during I/O then they were blocked
 *    until the I/O was complete, and the fields 'count', 'resrv',
 *    'lock', 'wqueue' and 'rqueue' reflect the results of unblocking
 *    those processes when the I/O was completed.
 */
void hfs_bnode_read(struct hfs_bnode *bnode, struct hfs_btree *tree,
		    hfs_u32 node, int sticky)
{
	struct NodeDescriptor *nd;
	int block, lcv;
	hfs_u16 curr, prev, limit;

	/* Initialize the structure */
	memset(bnode, 0, sizeof(*bnode));
	bnode->magic = HFS_BNODE_MAGIC;
	bnode->tree = tree;
	bnode->node = node;
	bnode->sticky = sticky;
	hfs_init_waitqueue(&bnode->rqueue);
	hfs_init_waitqueue(&bnode->wqueue);

	if (sticky == HFS_NOT_STICKY) {
		/* Insert it in the cache if appropriate */
		if ((bnode->next = bhash(tree, node))) {
			bnode->next->prev = bnode;
		}
		bhash(tree, node) = bnode;
	}

	/* Make the bnode look like it is being
	   modified so other processes will wait for
	   the I/O to complete */
	bnode->count = bnode->resrv = bnode->lock = 1;

	/* Read in the node, possibly causing a schedule()
	   call.  If the I/O fails then emit a warning.	 Each
	   process that was waiting on the bnode (including
	   the current one) will notice the failure and
	   hfs_bnode_relse() the node.	The last hfs_bnode_relse()
	   will call hfs_bnode_delete() and discard the bnode.	*/

	block = hfs_extent_map(&tree->entry.u.file.data_fork, node, 0);
	if (!block) {
		hfs_warn("hfs_bnode_read: bad node number 0x%08x\n", node);
	} else if (hfs_buffer_ok(bnode->buf =
				 hfs_buffer_get(tree->sys_mdb, block, 1))) {
		/* read in the NodeDescriptor */
		nd = (struct NodeDescriptor *)hfs_buffer_data(bnode->buf);
		bnode->ndFLink    = hfs_get_hl(nd->ndFLink);
		bnode->ndBLink    = hfs_get_hl(nd->ndBLink);
		bnode->ndType     = nd->ndType;
		bnode->ndNHeight  = nd->ndNHeight;
		bnode->ndNRecs    = hfs_get_hs(nd->ndNRecs);

		/* verify the integrity of the node */
		prev = sizeof(struct NodeDescriptor);
		limit = HFS_SECTOR_SIZE - sizeof(hfs_u16)*(bnode->ndNRecs + 1);
		for (lcv=1; lcv <= (bnode->ndNRecs + 1); ++lcv) {
			curr = hfs_get_hs(RECTBL(bnode, lcv));
			if ((curr < prev) || (curr > limit)) {
				hfs_warn("hfs_bnode_read: corrupt node "
					 "number 0x%08x\n", node);
				hfs_buffer_put(bnode->buf);
				bnode->buf = NULL;
				break;
			}
			prev = curr;
		}
	}

	/* Undo our fakery with the lock state and
	   hfs_wake_up() anyone who we managed to trick */
	--bnode->count;
	bnode->resrv = bnode->lock = 0;
	hfs_wake_up(&bnode->rqueue);
}

/*
 * hfs_bnode_lock()
 *
 * Description:
 *   This function does the locking of a bnode.
 * Input Variable(s):
 *   struct hfs_bnode *bn: pointer to the (struct hfs_bnode) to lock
 *   int lock_type: the type of lock desired
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'bn' points to a "valid" (struct hfs_bnode).
 *   'lock_type' is a valid hfs_lock_t
 * Postconditions:
 *   The 'count' field of 'bn' is incremented by one.  If 'lock_type'
 *   is HFS_LOCK_RESRV the 'resrv' field is also incremented.
 */
void hfs_bnode_lock(struct hfs_bnode_ref *bnr, int lock_type)
{
	struct hfs_bnode *bn = bnr->bn;

	if ((lock_type == bnr->lock_type) || !bn) {
		return;
	}

	if (bnr->lock_type == HFS_LOCK_WRITE) {
		hfs_bnode_commit(bnr->bn);
	}

	switch (lock_type) {
	default:
		goto bail;
		break;

	case HFS_LOCK_READ:
		/* We may not obtain read access if any process is
		   currently modifying or waiting to modify this node.
		   If we can't obtain access we wait on the rqueue
		   wait queue to be woken up by the modifying process
		   when it relinquishes its lock. */
		switch (bnr->lock_type) {
		default:
			goto bail;
			break;

		case HFS_LOCK_NONE:
			while (bn->lock || waitqueue_active(&bn->wqueue)) {
				hfs_sleep_on(&bn->rqueue);
			}
			++bn->count;
			break;
		}
		break;
			
	case HFS_LOCK_RESRV:
		/* We may not obtain a reservation (read access with
		   an option to write later), if any process currently
		   holds a reservation on this node.  That includes
		   any process which is currently modifying this node.
		   If we can't obtain access, then we wait on the
		   rqueue wait queue to e woken up by the
		   reservation-holder when it calls hfs_bnode_relse. */
		switch (bnr->lock_type) {
		default:
			goto bail;
			break;

		case HFS_LOCK_NONE:
			while (bn->resrv) {
				hfs_sleep_on(&bn->rqueue);
			}
			bn->resrv = 1;
			++bn->count;
			break;

		case HFS_LOCK_WRITE:
			bn->lock = 0;
			hfs_wake_up(&bn->rqueue);
			break;
		}
		break;
		
	case HFS_LOCK_WRITE:
		switch (bnr->lock_type) {
		default:
			goto bail;
			break;

		case HFS_LOCK_NONE:
			while (bn->resrv) {
				hfs_sleep_on(&bn->rqueue);
			}
			bn->resrv = 1;
			++bn->count;
		case HFS_LOCK_RESRV:
			while (bn->count > 1) {
				hfs_sleep_on(&bn->wqueue);
			}
			bn->lock = 1;
			break;
		}
		break;

	case HFS_LOCK_NONE:
		switch (bnr->lock_type) {
		default:
			goto bail;
			break;

		case HFS_LOCK_READ:
			/* This process was reading this node.	If
			   there is now exactly one other process using
			   the node then hfs_wake_up() a (potentially
			   nonexistent) waiting process.  Note that I
			   refer to "a" process since the reservation
			   system ensures that only one process can
			   get itself on the wait queue.  */
			if (bn->count == 2) {
				hfs_wake_up(&bn->wqueue);
			}
			break;

		case HFS_LOCK_WRITE:
			/* This process was modifying this node.
			   Unlock the node and fall-through to the
			   HFS_LOCK_RESRV case, since a 'reservation'
			   is a prerequisite for HFS_LOCK_WRITE.  */
			bn->lock = 0;
		case HFS_LOCK_RESRV:
			/* This process had placed a 'reservation' on
			   this node, indicating an intention to
			   possibly modify the node.  We can get to
			   this spot directly (if the 'reservation'
			   not converted to a HFS_LOCK_WRITE), or by
			   falling through from the above case if the
			   reservation was converted.
			   Since HFS_LOCK_RESRV and HFS_LOCK_WRITE
			   both block processes that want access
			   (HFS_LOCK_RESRV blocks other processes that
			   want reservations but allow HFS_LOCK_READ
			   accesses, while HFS_LOCK_WRITE must have
			   exclusive access and thus blocks both
			   types) we hfs_wake_up() any processes that
			   might be waiting for access.	 If multiple
			   processes are waiting for a reservation
			   then the magic of process scheduling will
			   settle the dispute. */
			bn->resrv = 0;
			hfs_wake_up(&bn->rqueue);
			break;
		}
		--bn->count;
		break;
	}
	bnr->lock_type = lock_type;
	return;

bail:
	hfs_warn("hfs_bnode_lock: invalid lock change: %d->%d.\n",
		bnr->lock_type, lock_type);
	return;
}

/*
 * hfs_bnode_relse()
 *
 * Description:
 *   This function is called when a process is done using a bnode.  If
 *   the proper conditions are met then we call hfs_bnode_delete() to remove
 *   it from the cache.	 If it is not deleted then we update its state
 *   to reflect one less process using it.
 * Input Variable(s):
 *   struct hfs_bnode *bn: pointer to the (struct hfs_bnode) to release.
 *   int lock_type: The type of lock held by the process releasing this node.
 * Output Variable(s):
 *   NONE
 * Returns:
 *   void
 * Preconditions:
 *   'bn' is NULL or points to a "valid" (struct hfs_bnode).
 * Postconditions:
 *   If 'bn' meets the appropriate conditions (see below) then it is
 *   kept in the cache and all fields are set to consistent values
 *   which reflect one less process using the node than upon entry.
 *   If 'bn' does not meet the conditions then it is deleted (see
 *   hfs_bnode_delete() for postconditions).
 *   In either case, if 'lock_type' is HFS_LOCK_WRITE
 *   then the corresponding buffer is dirtied.
 */
void hfs_bnode_relse(struct hfs_bnode_ref *bnr)
{
	struct hfs_bnode *bn;

	if (!bnr || !(bn = bnr->bn)) {
		return;
	}

	/* We update the lock state of the node if it is still in use
	   or if it is "sticky" (such as the B-tree head and root).
	   Otherwise we just delete it.	 */
	if ((bn->count > 1) || (waitqueue_active(&bn->rqueue)) || (bn->sticky != HFS_NOT_STICKY)) {
		hfs_bnode_lock(bnr, HFS_LOCK_NONE);
	} else {
		/* dirty buffer if we (might) have modified it */
		if (bnr->lock_type == HFS_LOCK_WRITE) {
			hfs_bnode_commit(bn);
		}
		hfs_bnode_delete(bn);
		bnr->lock_type = HFS_LOCK_NONE;
	}
	bnr->bn = NULL;
}

/*
 * hfs_bnode_find()
 *
 * Description:
 *   This function is called to obtain a bnode.  The cache is
 *   searched for the node.  If it not found there it is added to
 *   the cache by hfs_bnode_read().  There are two special cases node=0
 *   (the header node) and node='tree'->bthRoot (the root node), in
 *   which the nodes are obtained from fields of 'tree' without
 *   consulting or modifying the cache.
 * Input Variable(s):
 *   struct hfs_tree *tree: pointer to the (struct hfs_btree) from
 *    which to get a node.
 *   int node: the node number to get from 'tree'.
 *   int lock_type: The kind of access (HFS_LOCK_READ, or
 *    HFS_LOCK_RESRV) to obtain to the node
 * Output Variable(s):
 *   NONE
 * Returns:
 *   (struct hfs_bnode_ref) Reference to the requested node.
 * Preconditions:
 *   'tree' points to a "valid" (struct hfs_btree).
 * Postconditions:
 *   If 'node' refers to a valid node in 'tree' and 'lock_type' has
 *   one of the values listed above and no I/O errors occur then the
 *   value returned refers to a valid (struct hfs_bnode) corresponding
 *   to the requested node with the requested access type.  The node
 *   is also added to the cache if not previously present and not the
 *   root or header.
 *   If the conditions given above are not met, the bnode in the
 *   returned reference is NULL.
 */
struct hfs_bnode_ref hfs_bnode_find(struct hfs_btree *tree,
				    hfs_u32 node, int lock_type)
{
	struct hfs_bnode *bn;
	struct hfs_bnode *empty = NULL;
	struct hfs_bnode_ref bnr;

	bnr.lock_type = HFS_LOCK_NONE;
	bnr.bn = NULL;

#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	hfs_warn("hfs_bnode_find: %c %d:%d\n",
		 lock_type==HFS_LOCK_READ?'R':
			(lock_type==HFS_LOCK_RESRV?'V':'W'),
		 (int)ntohl(tree->entry.cnid), node);
#endif

	/* check special cases */
	if (!node) {
		bn = &tree->head;
		goto return_it;
	} else if (node == tree->bthRoot) {
		bn = tree->root;
		goto return_it;
	} 

restart:
	/* look for the node in the cache. */
	bn = bhash(tree, node);
	while (bn && (bn->magic == HFS_BNODE_MAGIC)) {
		if (bn->node == node) {
			goto found_it;
		}
		bn = bn->next;
	}

	if (!empty) {
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
		++bnode_count;
#endif
		if (HFS_NEW(empty)) {
			goto restart;
		}
		return bnr;
	}
	bn = empty;
	hfs_bnode_read(bn, tree, node, HFS_NOT_STICKY);
	goto return_it;

found_it:
	/* check validity */
	if (bn->magic != HFS_BNODE_MAGIC) {
		/* If we find a corrupt bnode then we return
		   NULL.  However, we don't try to remove it
		   from the cache or release its resources
		   since we have no idea what kind of trouble
		   we could get into that way. */
		hfs_warn("hfs_bnode_find: bnode cache is corrupt.\n");
		return bnr;
	} 
	if (empty) {
#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
		--bnode_count;
#endif
		HFS_DELETE(empty);
	}
	
return_it:
	/* Wait our turn */
	bnr.bn = bn;
	hfs_bnode_lock(&bnr, lock_type);

	/* Check for failure to read the node from disk */
	if (!hfs_buffer_ok(bn->buf)) {
		hfs_bnode_relse(&bnr);
	}

#if defined(DEBUG_BNODES) || defined(DEBUG_ALL)
	if (!bnr.bn) {
		hfs_warn("hfs_bnode_find: failed\n");
	} else {
		hfs_warn("hfs_bnode_find: use %d(%d) lvl %d [%d]\n", bn->count,
			 bn->buf->b_count, bn->ndNHeight, bnode_count);
		hfs_warn("hfs_bnode_find: blnk %u flnk %u recs %u\n", 
			 bn->ndBLink, bn->ndFLink, bn->ndNRecs);
	}
#endif

	return bnr;
}

/*
 * hfs_bnode_commit()
 *
 * Called to write a possibly dirty bnode back to disk.
 */
void hfs_bnode_commit(struct hfs_bnode *bn)
{
	if (hfs_buffer_ok(bn->buf)) {
		struct NodeDescriptor *nd;
		nd = (struct NodeDescriptor *)hfs_buffer_data(bn->buf);

		hfs_put_hl(bn->ndFLink, nd->ndFLink);
		hfs_put_hl(bn->ndBLink, nd->ndBLink);
		nd->ndType    = bn->ndType;
		nd->ndNHeight = bn->ndNHeight;
		hfs_put_hs(bn->ndNRecs, nd->ndNRecs);
		hfs_buffer_dirty(bn->buf);

		/* increment write count */
		hfs_mdb_dirty(bn->tree->sys_mdb);
	}
}
