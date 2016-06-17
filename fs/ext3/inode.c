/*
 *  linux/fs/ext3/inode.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Goal-directed block allocation by Stephen Tweedie
 * 	(sct@redhat.com), 1993, 1998
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 * 	(jj@sunsite.ms.mff.cuni.cz)
 *
 *  Assorted race fixes, rewrite of ext3_get_block() by Al Viro, 2000
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/ext3_jbd.h>
#include <linux/jbd.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/highuid.h>
#include <linux/quotaops.h>
#include <linux/module.h>

/*
 * SEARCH_FROM_ZERO forces each block allocation to search from the start
 * of the filesystem.  This is to force rapid reallocation of recently-freed
 * blocks.  The file fragmentation is horrendous.
 */
#undef SEARCH_FROM_ZERO

/*
 * Test whether an inode is a fast symlink.
 */
static inline int ext3_inode_is_fast_symlink(struct inode *inode)
{
	int ea_blocks = EXT3_I(inode)->i_file_acl ?
		(inode->i_sb->s_blocksize >> 9) : 0;

	return (S_ISLNK(inode->i_mode) &&
		inode->i_blocks - ea_blocks == 0);
}

/* The ext3 forget function must perform a revoke if we are freeing data
 * which has been journaled.  Metadata (eg. indirect blocks) must be
 * revoked in all cases. 
 *
 * "bh" may be NULL: a metadata block may have been freed from memory
 * but there may still be a record of it in the journal, and that record
 * still needs to be revoked.
 */

static int ext3_forget(handle_t *handle, int is_metadata,
		       struct inode *inode, struct buffer_head *bh,
		       int blocknr)
{
	int err;

	BUFFER_TRACE(bh, "enter");

	jbd_debug(4, "forgetting bh %p: is_metadata = %d, mode %o, "
		  "data mode %lx\n",
		  bh, is_metadata, inode->i_mode,
		  test_opt(inode->i_sb, DATA_FLAGS));
	
	/* Never use the revoke function if we are doing full data
	 * journaling: there is no need to, and a V1 superblock won't
	 * support it.  Otherwise, only skip the revoke on un-journaled
	 * data blocks. */

	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_JOURNAL_DATA ||
	    (!is_metadata && !ext3_should_journal_data(inode))) {
		if (bh) {
			BUFFER_TRACE(bh, "call journal_forget");
			ext3_journal_forget(handle, bh);
		}
		return 0;
	}

	/*
	 * data!=journal && (is_metadata || should_journal_data(inode))
	 */
	BUFFER_TRACE(bh, "call ext3_journal_revoke");
	err = ext3_journal_revoke(handle, blocknr, bh);
	if (err)
		ext3_abort(inode->i_sb, __FUNCTION__,
			   "error %d when attempting revoke", err);
	BUFFER_TRACE(bh, "exit");
	return err;
}

/*
 * Work out how many blocks we need to progress with the next chunk of a
 * truncate transaction.
 */

static unsigned long blocks_for_truncate(struct inode *inode) 
{
	unsigned long needed;
	
	needed = inode->i_blocks >> (inode->i_sb->s_blocksize_bits - 9);

	/* Give ourselves just enough room to cope with inodes in which
	 * i_blocks is corrupt: we've seen disk corruptions in the past
	 * which resulted in random data in an inode which looked enough
	 * like a regular file for ext3 to try to delete it.  Things
	 * will go a bit crazy if that happens, but at least we should
	 * try not to panic the whole kernel. */
	if (needed < 2)
		needed = 2;

	/* But we need to bound the transaction so we don't overflow the
	 * journal. */
	if (needed > EXT3_MAX_TRANS_DATA) 
		needed = EXT3_MAX_TRANS_DATA;

	return EXT3_DATA_TRANS_BLOCKS + needed;
}
	
/* 
 * Truncate transactions can be complex and absolutely huge.  So we need to
 * be able to restart the transaction at a conventient checkpoint to make
 * sure we don't overflow the journal.
 *
 * start_transaction gets us a new handle for a truncate transaction,
 * and extend_transaction tries to extend the existing one a bit.  If
 * extend fails, we need to propagate the failure up and restart the
 * transaction in the top-level truncate loop. --sct 
 */

static handle_t *start_transaction(struct inode *inode) 
{
	handle_t *result;
	
	result = ext3_journal_start(inode, blocks_for_truncate(inode));
	if (!IS_ERR(result))
		return result;
	
	ext3_std_error(inode->i_sb, PTR_ERR(result));
	return result;
}

/*
 * Try to extend this transaction for the purposes of truncation.
 *
 * Returns 0 if we managed to create more room.  If we can't create more
 * room, and the transaction must be restarted we return 1.
 */
static int try_to_extend_transaction(handle_t *handle, struct inode *inode)
{
	if (handle->h_buffer_credits > EXT3_RESERVE_TRANS_BLOCKS)
		return 0;
	if (!ext3_journal_extend(handle, blocks_for_truncate(inode)))
		return 0;
	return 1;
}

/*
 * Restart the transaction associated with *handle.  This does a commit,
 * so before we call here everything must be consistently dirtied against
 * this transaction.
 */
static int ext3_journal_test_restart(handle_t *handle, struct inode *inode)
{
	jbd_debug(2, "restarting handle %p\n", handle);
	return ext3_journal_restart(handle, blocks_for_truncate(inode));
}

/*
 * Called at each iput()
 */
void ext3_put_inode (struct inode * inode)
{
	ext3_discard_prealloc (inode);
}

/*
 * Called at the last iput() if i_nlink is zero.
 */
void ext3_delete_inode (struct inode * inode)
{
	handle_t *handle;
	
	if (is_bad_inode(inode) ||
	    inode->i_ino == EXT3_ACL_IDX_INO ||
	    inode->i_ino == EXT3_ACL_DATA_INO)
		goto no_delete;

	lock_kernel();
	handle = start_transaction(inode);
	if (IS_ERR(handle)) {
		/* If we're going to skip the normal cleanup, we still
		 * need to make sure that the in-core orphan linked list
		 * is properly cleaned up. */
		ext3_orphan_del(NULL, inode);

		ext3_std_error(inode->i_sb, PTR_ERR(handle));
		unlock_kernel();
		goto no_delete;
	}
	
	if (IS_SYNC(inode))
		handle->h_sync = 1;
	inode->i_size = 0;
	if (inode->i_blocks)
		ext3_truncate(inode);
	/*
	 * Kill off the orphan record which ext3_truncate created.
	 * AKPM: I think this can be inside the above `if'.
	 * Note that ext3_orphan_del() has to be able to cope with the
	 * deletion of a non-existent orphan - this is because we don't
	 * know if ext3_truncate() actually created an orphan record.
	 * (Well, we could do this if we need to, but heck - it works)
	 */
	ext3_orphan_del(handle, inode);
	inode->u.ext3_i.i_dtime	= CURRENT_TIME;

	/* 
	 * One subtle ordering requirement: if anything has gone wrong
	 * (transaction abort, IO errors, whatever), then we can still
	 * do these next steps (the fs will already have been marked as
	 * having errors), but we can't free the inode if the mark_dirty
	 * fails.  
	 */
	if (ext3_mark_inode_dirty(handle, inode))
		/* If that failed, just do the required in-core inode clear. */
		clear_inode(inode);
	else
		ext3_free_inode(handle, inode);
	ext3_journal_stop(handle, inode);
	unlock_kernel();
	return;
no_delete:
	clear_inode(inode);	/* We must guarantee clearing of inode... */
}

void ext3_discard_prealloc (struct inode * inode)
{
#ifdef EXT3_PREALLOCATE
	lock_kernel();
	/* Writer: ->i_prealloc* */
	if (inode->u.ext3_i.i_prealloc_count) {
		unsigned short total = inode->u.ext3_i.i_prealloc_count;
		unsigned long block = inode->u.ext3_i.i_prealloc_block;
		inode->u.ext3_i.i_prealloc_count = 0;
		inode->u.ext3_i.i_prealloc_block = 0;
		/* Writer: end */
		ext3_free_blocks (inode, block, total);
	}
	unlock_kernel();
#endif
}

static int ext3_alloc_block (handle_t *handle,
			struct inode * inode, unsigned long goal, int *err)
{
#ifdef EXT3FS_DEBUG
	static unsigned long alloc_hits = 0, alloc_attempts = 0;
#endif
	unsigned long result;

#ifdef EXT3_PREALLOCATE
	/* Writer: ->i_prealloc* */
	if (inode->u.ext3_i.i_prealloc_count &&
	    (goal == inode->u.ext3_i.i_prealloc_block ||
	     goal + 1 == inode->u.ext3_i.i_prealloc_block))
	{
		result = inode->u.ext3_i.i_prealloc_block++;
		inode->u.ext3_i.i_prealloc_count--;
		/* Writer: end */
		ext3_debug ("preallocation hit (%lu/%lu).\n",
			    ++alloc_hits, ++alloc_attempts);
	} else {
		ext3_discard_prealloc (inode);
		ext3_debug ("preallocation miss (%lu/%lu).\n",
			    alloc_hits, ++alloc_attempts);
		if (S_ISREG(inode->i_mode))
			result = ext3_new_block (inode, goal, 
				 &inode->u.ext3_i.i_prealloc_count,
				 &inode->u.ext3_i.i_prealloc_block, err);
		else
			result = ext3_new_block (inode, goal, 0, 0, err);
		/*
		 * AKPM: this is somewhat sticky.  I'm not surprised it was
		 * disabled in 2.2's ext3.  Need to integrate b_committed_data
		 * guarding with preallocation, if indeed preallocation is
		 * effective.
		 */
	}
#else
	result = ext3_new_block (handle, inode, goal, 0, 0, err);
#endif
	return result;
}


typedef struct {
	u32	*p;
	u32	key;
	struct buffer_head *bh;
} Indirect;

static inline void add_chain(Indirect *p, struct buffer_head *bh, u32 *v)
{
	p->key = *(p->p = v);
	p->bh = bh;
}

static inline int verify_chain(Indirect *from, Indirect *to)
{
	while (from <= to && from->key == *from->p)
		from++;
	return (from > to);
}

/**
 *	ext3_block_to_path - parse the block number into array of offsets
 *	@inode: inode in question (we are only interested in its superblock)
 *	@i_block: block number to be parsed
 *	@offsets: array to store the offsets in
 *
 *	To store the locations of file's data ext3 uses a data structure common
 *	for UNIX filesystems - tree of pointers anchored in the inode, with
 *	data blocks at leaves and indirect blocks in intermediate nodes.
 *	This function translates the block number into path in that tree -
 *	return value is the path length and @offsets[n] is the offset of
 *	pointer to (n+1)th node in the nth one. If @block is out of range
 *	(negative or too large) warning is printed and zero returned.
 *
 *	Note: function doesn't find node addresses, so no IO is needed. All
 *	we need to know is the capacity of indirect blocks (taken from the
 *	inode->i_sb).
 */

/*
 * Portability note: the last comparison (check that we fit into triple
 * indirect block) is spelled differently, because otherwise on an
 * architecture with 32-bit longs and 8Kb pages we might get into trouble
 * if our filesystem had 8Kb blocks. We might use long long, but that would
 * kill us on x86. Oh, well, at least the sign propagation does not matter -
 * i_block would have to be negative in the very beginning, so we would not
 * get there at all.
 */

static int ext3_block_to_path(struct inode *inode, long i_block, int offsets[4])
{
	int ptrs = EXT3_ADDR_PER_BLOCK(inode->i_sb);
	int ptrs_bits = EXT3_ADDR_PER_BLOCK_BITS(inode->i_sb);
	const long direct_blocks = EXT3_NDIR_BLOCKS,
		indirect_blocks = ptrs,
		double_blocks = (1 << (ptrs_bits * 2));
	int n = 0;

	if (i_block < 0) {
		ext3_warning (inode->i_sb, "ext3_block_to_path", "block < 0");
	} else if (i_block < direct_blocks) {
		offsets[n++] = i_block;
	} else if ( (i_block -= direct_blocks) < indirect_blocks) {
		offsets[n++] = EXT3_IND_BLOCK;
		offsets[n++] = i_block;
	} else if ((i_block -= indirect_blocks) < double_blocks) {
		offsets[n++] = EXT3_DIND_BLOCK;
		offsets[n++] = i_block >> ptrs_bits;
		offsets[n++] = i_block & (ptrs - 1);
	} else if (((i_block -= double_blocks) >> (ptrs_bits * 2)) < ptrs) {
		offsets[n++] = EXT3_TIND_BLOCK;
		offsets[n++] = i_block >> (ptrs_bits * 2);
		offsets[n++] = (i_block >> ptrs_bits) & (ptrs - 1);
		offsets[n++] = i_block & (ptrs - 1);
	} else {
		ext3_warning (inode->i_sb, "ext3_block_to_path", "block > big");
	}
	return n;
}

/**
 *	ext3_get_branch - read the chain of indirect blocks leading to data
 *	@inode: inode in question
 *	@depth: depth of the chain (1 - direct pointer, etc.)
 *	@offsets: offsets of pointers in inode/indirect blocks
 *	@chain: place to store the result
 *	@err: here we store the error value
 *
 *	Function fills the array of triples <key, p, bh> and returns %NULL
 *	if everything went OK or the pointer to the last filled triple
 *	(incomplete one) otherwise. Upon the return chain[i].key contains
 *	the number of (i+1)-th block in the chain (as it is stored in memory,
 *	i.e. little-endian 32-bit), chain[i].p contains the address of that
 *	number (it points into struct inode for i==0 and into the bh->b_data
 *	for i>0) and chain[i].bh points to the buffer_head of i-th indirect
 *	block for i>0 and NULL for i==0. In other words, it holds the block
 *	numbers of the chain, addresses they were taken from (and where we can
 *	verify that chain did not change) and buffer_heads hosting these
 *	numbers.
 *
 *	Function stops when it stumbles upon zero pointer (absent block)
 *		(pointer to last triple returned, *@err == 0)
 *	or when it gets an IO error reading an indirect block
 *		(ditto, *@err == -EIO)
 *	or when it notices that chain had been changed while it was reading
 *		(ditto, *@err == -EAGAIN)
 *	or when it reads all @depth-1 indirect blocks successfully and finds
 *	the whole chain, all way to the data (returns %NULL, *err == 0).
 */
static Indirect *ext3_get_branch(struct inode *inode, int depth, int *offsets,
				 Indirect chain[4], int *err)
{
	struct super_block *sb = inode->i_sb;
	Indirect *p = chain;
	struct buffer_head *bh;

	*err = 0;
	/* i_data is not going away, no lock needed */
	add_chain (chain, NULL, inode->u.ext3_i.i_data + *offsets);
	if (!p->key)
		goto no_block;
	while (--depth) {
		bh = sb_bread(sb, le32_to_cpu(p->key));
		if (!bh)
			goto failure;
		/* Reader: pointers */
		if (!verify_chain(chain, p))
			goto changed;
		add_chain(++p, bh, (u32*)bh->b_data + *++offsets);
		/* Reader: end */
		if (!p->key)
			goto no_block;
	}
	return NULL;

changed:
	brelse(bh);
	*err = -EAGAIN;
	goto no_block;
failure:
	*err = -EIO;
no_block:
	return p;
}

/**
 *	ext3_find_near - find a place for allocation with sufficient locality
 *	@inode: owner
 *	@ind: descriptor of indirect block.
 *
 *	This function returns the prefered place for block allocation.
 *	It is used when heuristic for sequential allocation fails.
 *	Rules are:
 *	  + if there is a block to the left of our position - allocate near it.
 *	  + if pointer will live in indirect block - allocate near that block.
 *	  + if pointer will live in inode - allocate in the same
 *	    cylinder group. 
 *	Caller must make sure that @ind is valid and will stay that way.
 */

static inline unsigned long ext3_find_near(struct inode *inode, Indirect *ind)
{
	u32 *start = ind->bh ? (u32*) ind->bh->b_data : inode->u.ext3_i.i_data;
	u32 *p;

	/* Try to find previous block */
	for (p = ind->p - 1; p >= start; p--)
		if (*p)
			return le32_to_cpu(*p);

	/* No such thing, so let's try location of indirect block */
	if (ind->bh)
		return ind->bh->b_blocknr;

	/*
	 * It is going to be refered from inode itself? OK, just put it into
	 * the same cylinder group then.
	 */
	return (inode->u.ext3_i.i_block_group * 
		EXT3_BLOCKS_PER_GROUP(inode->i_sb)) +
	       le32_to_cpu(inode->i_sb->u.ext3_sb.s_es->s_first_data_block);
}

/**
 *	ext3_find_goal - find a prefered place for allocation.
 *	@inode: owner
 *	@block:  block we want
 *	@chain:  chain of indirect blocks
 *	@partial: pointer to the last triple within a chain
 *	@goal:	place to store the result.
 *
 *	Normally this function find the prefered place for block allocation,
 *	stores it in *@goal and returns zero. If the branch had been changed
 *	under us we return -EAGAIN.
 */

static int ext3_find_goal(struct inode *inode, long block, Indirect chain[4],
			  Indirect *partial, unsigned long *goal)
{
	/* Writer: ->i_next_alloc* */
	if (block == inode->u.ext3_i.i_next_alloc_block + 1) {
		inode->u.ext3_i.i_next_alloc_block++;
		inode->u.ext3_i.i_next_alloc_goal++;
	}
#ifdef SEARCH_FROM_ZERO
	inode->u.ext3_i.i_next_alloc_block = 0;
	inode->u.ext3_i.i_next_alloc_goal = 0;
#endif
	/* Writer: end */
	/* Reader: pointers, ->i_next_alloc* */
	if (verify_chain(chain, partial)) {
		/*
		 * try the heuristic for sequential allocation,
		 * failing that at least try to get decent locality.
		 */
		if (block == inode->u.ext3_i.i_next_alloc_block)
			*goal = inode->u.ext3_i.i_next_alloc_goal;
		if (!*goal)
			*goal = ext3_find_near(inode, partial);
#ifdef SEARCH_FROM_ZERO
		*goal = 0;
#endif
		return 0;
	}
	/* Reader: end */
	return -EAGAIN;
}

/**
 *	ext3_alloc_branch - allocate and set up a chain of blocks.
 *	@inode: owner
 *	@num: depth of the chain (number of blocks to allocate)
 *	@offsets: offsets (in the blocks) to store the pointers to next.
 *	@branch: place to store the chain in.
 *
 *	This function allocates @num blocks, zeroes out all but the last one,
 *	links them into chain and (if we are synchronous) writes them to disk.
 *	In other words, it prepares a branch that can be spliced onto the
 *	inode. It stores the information about that chain in the branch[], in
 *	the same format as ext3_get_branch() would do. We are calling it after
 *	we had read the existing part of chain and partial points to the last
 *	triple of that (one with zero ->key). Upon the exit we have the same
 *	picture as after the successful ext3_get_block(), excpet that in one
 *	place chain is disconnected - *branch->p is still zero (we did not
 *	set the last link), but branch->key contains the number that should
 *	be placed into *branch->p to fill that gap.
 *
 *	If allocation fails we free all blocks we've allocated (and forget
 *	their buffer_heads) and return the error value the from failed
 *	ext3_alloc_block() (normally -ENOSPC). Otherwise we set the chain
 *	as described above and return 0.
 */

static int ext3_alloc_branch(handle_t *handle, struct inode *inode,
			     int num,
			     unsigned long goal,
			     int *offsets,
			     Indirect *branch)
{
	int blocksize = inode->i_sb->s_blocksize;
	int n = 0, keys = 0;
	int err = 0;
	int i;
	int parent = ext3_alloc_block(handle, inode, goal, &err);

	branch[0].key = cpu_to_le32(parent);
	if (parent) {
		for (n = 1; n < num; n++) {
			struct buffer_head *bh;
			/* Allocate the next block */
			int nr = ext3_alloc_block(handle, inode, parent, &err);
			if (!nr)
				break;
			branch[n].key = cpu_to_le32(nr);
			keys = n+1;
			
			/*
			 * Get buffer_head for parent block, zero it out
			 * and set the pointer to new one, then send
			 * parent to disk.  
			 */
			bh = sb_getblk(inode->i_sb, parent);
			branch[n].bh = bh;
			lock_buffer(bh);
			BUFFER_TRACE(bh, "call get_create_access");
			err = ext3_journal_get_create_access(handle, bh);
			if (err) {
				unlock_buffer(bh);
				brelse(bh);
				break;
			}

			memset(bh->b_data, 0, blocksize);
			branch[n].p = (u32*) bh->b_data + offsets[n];
			*branch[n].p = branch[n].key;
			BUFFER_TRACE(bh, "marking uptodate");
			mark_buffer_uptodate(bh, 1);
			unlock_buffer(bh);

			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			err = ext3_journal_dirty_metadata(handle, bh);
			if (err)
				break;
			
			parent = nr;
		}
	}
	if (n == num)
		return 0;

	/* Allocation failed, free what we already allocated */
	for (i = 1; i < keys; i++) {
		BUFFER_TRACE(branch[i].bh, "call journal_forget");
		ext3_journal_forget(handle, branch[i].bh);
	}
	for (i = 0; i < keys; i++)
		ext3_free_blocks(handle, inode, le32_to_cpu(branch[i].key), 1);
	return err;
}

/**
 *	ext3_splice_branch - splice the allocated branch onto inode.
 *	@inode: owner
 *	@block: (logical) number of block we are adding
 *	@chain: chain of indirect blocks (with a missing link - see
 *		ext3_alloc_branch)
 *	@where: location of missing link
 *	@num:   number of blocks we are adding
 *
 *	This function verifies that chain (up to the missing link) had not
 *	changed, fills the missing link and does all housekeeping needed in
 *	inode (->i_blocks, etc.). In case of success we end up with the full
 *	chain to new block and return 0. Otherwise (== chain had been changed)
 *	we free the new blocks (forgetting their buffer_heads, indeed) and
 *	return -EAGAIN.
 */

static int ext3_splice_branch(handle_t *handle, struct inode *inode, long block,
			      Indirect chain[4], Indirect *where, int num)
{
	int i;
	int err = 0;

	/*
	 * If we're splicing into a [td]indirect block (as opposed to the
	 * inode) then we need to get write access to the [td]indirect block
	 * before the splice.
	 */
	if (where->bh) {
		BUFFER_TRACE(where->bh, "get_write_access");
		err = ext3_journal_get_write_access(handle, where->bh);
		if (err)
			goto err_out;
	}
	/* Verify that place we are splicing to is still there and vacant */

	/* Writer: pointers, ->i_next_alloc* */
	if (!verify_chain(chain, where-1) || *where->p)
		/* Writer: end */
		goto changed;

	/* That's it */

	*where->p = where->key;
	inode->u.ext3_i.i_next_alloc_block = block;
	inode->u.ext3_i.i_next_alloc_goal = le32_to_cpu(where[num-1].key);
#ifdef SEARCH_FROM_ZERO
	inode->u.ext3_i.i_next_alloc_block = 0;
	inode->u.ext3_i.i_next_alloc_goal = 0;
#endif
	/* Writer: end */

	/* We are done with atomic stuff, now do the rest of housekeeping */

	inode->i_ctime = CURRENT_TIME;
	ext3_mark_inode_dirty(handle, inode);

	/* had we spliced it onto indirect block? */
	if (where->bh) {
		/*
		 * akpm: If we spliced it onto an indirect block, we haven't
		 * altered the inode.  Note however that if it is being spliced
		 * onto an indirect block at the very end of the file (the
		 * file is growing) then we *will* alter the inode to reflect
		 * the new i_size.  But that is not done here - it is done in
		 * generic_commit_write->__mark_inode_dirty->ext3_dirty_inode.
		 */
		jbd_debug(5, "splicing indirect only\n");
		BUFFER_TRACE(where->bh, "call ext3_journal_dirty_metadata");
		err = ext3_journal_dirty_metadata(handle, where->bh);
		if (err) 
			goto err_out;
	} else {
		/*
		 * OK, we spliced it into the inode itself on a direct block.
		 * Inode was dirtied above.
		 */
		jbd_debug(5, "splicing direct\n");
	}
	return err;

changed:
	/*
	 * AKPM: if where[i].bh isn't part of the current updating
	 * transaction then we explode nastily.  Test this code path.
	 */
	jbd_debug(1, "the chain changed: try again\n");
	err = -EAGAIN;
	
err_out:
	for (i = 1; i < num; i++) {
		BUFFER_TRACE(where[i].bh, "call journal_forget");
		ext3_journal_forget(handle, where[i].bh);
	}
	/* For the normal collision cleanup case, we free up the blocks.
	 * On genuine filesystem errors we don't even think about doing
	 * that. */
	if (err == -EAGAIN)
		for (i = 0; i < num; i++)
			ext3_free_blocks(handle, inode, 
					 le32_to_cpu(where[i].key), 1);
	return err;
}

/*
 * Allocation strategy is simple: if we have to allocate something, we will
 * have to go the whole way to leaf. So let's do it before attaching anything
 * to tree, set linkage between the newborn blocks, write them if sync is
 * required, recheck the path, free and repeat if check fails, otherwise
 * set the last missing link (that will protect us from any truncate-generated
 * removals - all blocks on the path are immune now) and possibly force the
 * write on the parent block.
 * That has a nice additional property: no special recovery from the failed
 * allocations is needed - we simply release blocks and do not touch anything
 * reachable from inode.
 *
 * akpm: `handle' can be NULL if create == 0.
 *
 * The BKL may not be held on entry here.  Be sure to take it early.
 */

static int ext3_get_block_handle(handle_t *handle, struct inode *inode, 
				 long iblock,
				 struct buffer_head *bh_result, int create)
{
	int err = -EIO;
	int offsets[4];
	Indirect chain[4];
	Indirect *partial;
	unsigned long goal;
	int left;
	int depth = ext3_block_to_path(inode, iblock, offsets);
	loff_t new_size;

	J_ASSERT(handle != NULL || create == 0);

	if (depth == 0)
		goto out;

	lock_kernel();
reread:
	partial = ext3_get_branch(inode, depth, offsets, chain, &err);

	/* Simplest case - block found, no allocation needed */
	if (!partial) {
		bh_result->b_state &= ~(1UL << BH_New);
got_it:
		bh_result->b_dev = inode->i_dev;
		bh_result->b_blocknr = le32_to_cpu(chain[depth-1].key);
		bh_result->b_state |= (1UL << BH_Mapped);
		/* Clean up and exit */
		partial = chain+depth-1; /* the whole chain */
		goto cleanup;
	}

	/* Next simple case - plain lookup or failed read of indirect block */
	if (!create || err == -EIO) {
cleanup:
		while (partial > chain) {
			BUFFER_TRACE(partial->bh, "call brelse");
			brelse(partial->bh);
			partial--;
		}
		BUFFER_TRACE(bh_result, "returned");
		unlock_kernel();
out:
		return err;
	}

	/*
	 * Indirect block might be removed by truncate while we were
	 * reading it. Handling of that case (forget what we've got and
	 * reread) is taken out of the main path.
	 */
	if (err == -EAGAIN)
		goto changed;

	if (ext3_find_goal(inode, iblock, chain, partial, &goal) < 0)
		goto changed;

	left = (chain + depth) - partial;

	/*
	 * Block out ext3_truncate while we alter the tree
	 */
	down_read(&inode->u.ext3_i.truncate_sem);
	err = ext3_alloc_branch(handle, inode, left, goal,
					offsets+(partial-chain), partial);

	/* The ext3_splice_branch call will free and forget any buffers
	 * on the new chain if there is a failure, but that risks using
	 * up transaction credits, especially for bitmaps where the
	 * credits cannot be returned.  Can we handle this somehow?  We
	 * may need to return -EAGAIN upwards in the worst case.  --sct */
	if (!err)
		err = ext3_splice_branch(handle, inode, iblock, chain,
					 partial, left);
	up_read(&inode->u.ext3_i.truncate_sem);
	if (err == -EAGAIN)
		goto changed;
	if (err)
		goto cleanup;

	new_size = inode->i_size;
	/*
	 * This is not racy against ext3_truncate's modification of i_disksize
	 * because VM/VFS ensures that the file cannot be extended while
	 * truncate is in progress.  It is racy between multiple parallel
	 * instances of get_block, but we have the BKL.
	 */
	if (new_size > inode->u.ext3_i.i_disksize)
		inode->u.ext3_i.i_disksize = new_size;

	bh_result->b_state |= (1UL << BH_New);
	goto got_it;

changed:
	while (partial > chain) {
		jbd_debug(1, "buffer chain changed, retrying\n");
		BUFFER_TRACE(partial->bh, "brelsing");
		brelse(partial->bh);
		partial--;
	}
	goto reread;
}

/*
 * The BKL is not held on entry here.
 */
static int ext3_get_block(struct inode *inode, long iblock,
			struct buffer_head *bh_result, int create)
{
	handle_t *handle = 0;
	int ret;

	if (create) {
		handle = ext3_journal_current_handle();
		J_ASSERT(handle != 0);
	}
	ret = ext3_get_block_handle(handle, inode, iblock, bh_result, create);
	return ret;
}

/*
 * `handle' can be NULL if create is zero
 */
struct buffer_head *ext3_getblk(handle_t *handle, struct inode * inode,
				long block, int create, int * errp)
{
	struct buffer_head dummy;
	int fatal = 0, err;
	
	J_ASSERT(handle != NULL || create == 0);

	dummy.b_state = 0;
	dummy.b_blocknr = -1000;
	buffer_trace_init(&dummy.b_history);
	*errp = ext3_get_block_handle(handle, inode, block, &dummy, create);
	if (!*errp && buffer_mapped(&dummy)) {
		struct buffer_head *bh;
		bh = sb_getblk(inode->i_sb, dummy.b_blocknr);
		if (buffer_new(&dummy)) {
			J_ASSERT(create != 0);
			J_ASSERT(handle != 0);

			/* Now that we do not always journal data, we
			   should keep in mind whether this should
			   always journal the new buffer as metadata.
			   For now, regular file writes use
			   ext3_get_block instead, so it's not a
			   problem. */
			lock_kernel();
			lock_buffer(bh);
			BUFFER_TRACE(bh, "call get_create_access");
			fatal = ext3_journal_get_create_access(handle, bh);
			if (!fatal) {
				memset(bh->b_data, 0,
				       inode->i_sb->s_blocksize);
				mark_buffer_uptodate(bh, 1);
			}
			unlock_buffer(bh);
			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			err = ext3_journal_dirty_metadata(handle, bh);
			if (!fatal) fatal = err;
			unlock_kernel();
		} else {
			BUFFER_TRACE(bh, "not a new buffer");
		}
		if (fatal) {
			*errp = fatal;
			brelse(bh);
			bh = NULL;
		}
		return bh;
	}
	return NULL;
}

struct buffer_head *ext3_bread(handle_t *handle, struct inode * inode,
			       int block, int create, int *err)
{
	struct buffer_head * bh;
	int prev_blocks;

	prev_blocks = inode->i_blocks;

	bh = ext3_getblk (handle, inode, block, create, err);
	if (!bh)
		return bh;
#ifdef EXT3_PREALLOCATE
	/*
	 * If the inode has grown, and this is a directory, then use a few
	 * more of the preallocated blocks to keep directory fragmentation
	 * down.  The preallocated blocks are guaranteed to be contiguous.
	 */
	if (create &&
	    S_ISDIR(inode->i_mode) &&
	    inode->i_blocks > prev_blocks &&
	    EXT3_HAS_COMPAT_FEATURE(inode->i_sb,
				    EXT3_FEATURE_COMPAT_DIR_PREALLOC)) {
		int i;
		struct buffer_head *tmp_bh;

		for (i = 1;
		     inode->u.ext3_i.i_prealloc_count &&
		     i < EXT3_SB(inode->i_sb)->s_es->s_prealloc_dir_blocks;
		     i++) {
			/*
			 * ext3_getblk will zero out the contents of the
			 * directory for us
			 */
			tmp_bh = ext3_getblk(handle, inode,
						block+i, create, err);
			if (!tmp_bh) {
				brelse (bh);
				return 0;
			}
			brelse (tmp_bh);
		}
	}
#endif
	if (buffer_uptodate(bh))
		return bh;
	ll_rw_block (READ, 1, &bh);
	wait_on_buffer (bh);
	if (buffer_uptodate(bh))
		return bh;
	brelse (bh);
	*err = -EIO;
	return NULL;
}

static int walk_page_buffers(	handle_t *handle,
				struct inode *inode,
				struct buffer_head *head,
				unsigned from,
				unsigned to,
				int *partial,
				int (*fn)(	handle_t *handle,
						struct inode *inode,
						struct buffer_head *bh))
{
	struct buffer_head *bh;
	unsigned block_start, block_end;
	unsigned blocksize = head->b_size;
	int err, ret = 0;

	for (	bh = head, block_start = 0;
		ret == 0 && (bh != head || !block_start);
	    	block_start = block_end, bh = bh->b_this_page)
	{
		block_end = block_start + blocksize;
		if (block_end <= from || block_start >= to) {
			if (partial && !buffer_uptodate(bh))
				*partial = 1;
			continue;
		}
		err = (*fn)(handle, inode, bh);
		if (!ret)
			ret = err;
	}
	return ret;
}

/*
 * To preserve ordering, it is essential that the hole instantiation and
 * the data write be encapsulated in a single transaction.  We cannot
 * close off a transaction and start a new one between the ext3_get_block()
 * and the commit_write().  So doing the journal_start at the start of
 * prepare_write() is the right place.
 *
 * Also, this function can nest inside ext3_writepage() ->
 * block_write_full_page(). In that case, we *know* that ext3_writepage()
 * has generated enough buffer credits to do the whole page.  So we won't
 * block on the journal in that case, which is good, because the caller may
 * be PF_MEMALLOC.
 *
 * By accident, ext3 can be reentered when a transaction is open via
 * quota file writes.  If we were to commit the transaction while thus
 * reentered, there can be a deadlock - we would be holding a quota
 * lock, and the commit would never complete if another thread had a
 * transaction open and was blocking on the quota lock - a ranking
 * violation.
 *
 * So what we do is to rely on the fact that journal_stop/journal_start
 * will _not_ run commit under these circumstances because handle->h_ref
 * is elevated.  We'll still have enough credits for the tiny quotafile
 * write.  
 */

static int do_journal_get_write_access(handle_t *handle, struct inode *inode,
				       struct buffer_head *bh)
{
	return ext3_journal_get_write_access(handle, bh);
}

static int ext3_prepare_write(struct file *file, struct page *page,
			      unsigned from, unsigned to)
{
	struct inode *inode = page->mapping->host;
	int ret, needed_blocks = ext3_writepage_trans_blocks(inode);
	handle_t *handle;

	lock_kernel();
	handle = ext3_journal_start(inode, needed_blocks);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}
	unlock_kernel();
	ret = block_prepare_write(page, from, to, ext3_get_block);
	lock_kernel();
	if (ret != 0)
		goto prepare_write_failed;

	if (ext3_should_journal_data(inode)) {
		ret = walk_page_buffers(handle, inode, page->buffers,
				from, to, NULL, do_journal_get_write_access);
		if (ret) {
			/*
			 * We're going to fail this prepare_write(),
			 * so commit_write() will not be called.
			 * We need to undo block_prepare_write()'s kmap().
			 * AKPM: Do we need to clear PageUptodate?  I don't
			 * think so.
			 */
			kunmap(page);
		}
	}
prepare_write_failed:
	if (ret)
		ext3_journal_stop(handle, inode);
out:
	unlock_kernel();
	return ret;
}

static int journal_dirty_sync_data(handle_t *handle, struct inode *inode,
				   struct buffer_head *bh)
{
	int ret = ext3_journal_dirty_data(handle, bh, 0);
	buffer_insert_inode_data_queue(bh, inode);
	return ret;
}

/*
 * For ext3_writepage().  We also brelse() the buffer to account for
 * the bget() which ext3_writepage() performs.
 */
static int journal_dirty_async_data(handle_t *handle, struct inode *inode, 
				    struct buffer_head *bh)
{
	int ret = ext3_journal_dirty_data(handle, bh, 1);
	buffer_insert_inode_data_queue(bh, inode);
	__brelse(bh);
	return ret;
}

/* For commit_write() in data=journal mode */
static int commit_write_fn(handle_t *handle, struct inode *inode, 
			   struct buffer_head *bh)
{
	set_bit(BH_Uptodate, &bh->b_state);
	return ext3_journal_dirty_metadata(handle, bh);
}

/*
 * We need to pick up the new inode size which generic_commit_write gave us
 * `file' can be NULL - eg, when called from block_symlink().
 *
 * ext3 inode->i_dirty_buffers policy:  If we're journalling data we
 * definitely don't want them to appear on the inode at all - instead
 * we need to manage them at the JBD layer and we need to intercept
 * the relevant sync operations and translate them into journal operations.
 *
 * If we're not journalling data then we can just leave the buffers
 * on ->i_dirty_buffers.  If someone writes them out for us then thanks.
 * Otherwise we'll do it in commit, if we're using ordered data.
 */

static int ext3_commit_write(struct file *file, struct page *page,
			     unsigned from, unsigned to)
{
	handle_t *handle = ext3_journal_current_handle();
	struct inode *inode = page->mapping->host;
	int ret = 0, ret2;

	lock_kernel();
	if (ext3_should_journal_data(inode)) {
		/*
		 * Here we duplicate the generic_commit_write() functionality
		 */
		int partial = 0;
		loff_t pos = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;

		ret = walk_page_buffers(handle, inode, page->buffers,
			from, to, &partial, commit_write_fn);
		if (!partial)
			SetPageUptodate(page);
		kunmap(page);
		if (pos > inode->i_size)
			inode->i_size = pos;
		EXT3_I(inode)->i_state |= EXT3_STATE_JDATA;
	} else {
		if (ext3_should_order_data(inode)) {
			ret = walk_page_buffers(handle, inode, page->buffers,
				from, to, NULL, journal_dirty_sync_data);
		}
		/* Be careful here if generic_commit_write becomes a
		 * required invocation after block_prepare_write. */
		if (ret == 0) {
			ret = generic_commit_write(file, page, from, to);
		} else {
			/*
			 * block_prepare_write() was called, but we're not
			 * going to call generic_commit_write().  So we
			 * need to perform generic_commit_write()'s kunmap
			 * by hand.
			 */
			kunmap(page);
		}
	}
	if (inode->i_size > inode->u.ext3_i.i_disksize) {
		inode->u.ext3_i.i_disksize = inode->i_size;
		ret2 = ext3_mark_inode_dirty(handle, inode);
		if (!ret) 
			ret = ret2;
	}
	ret2 = ext3_journal_stop(handle, inode);
	unlock_kernel();
	if (!ret)
		ret = ret2;
	return ret;
}

/* 
 * bmap() is special.  It gets used by applications such as lilo and by
 * the swapper to find the on-disk block of a specific piece of data.
 *
 * Naturally, this is dangerous if the block concerned is still in the
 * journal.  If somebody makes a swapfile on an ext3 data-journaling
 * filesystem and enables swap, then they may get a nasty shock when the
 * data getting swapped to that swapfile suddenly gets overwritten by
 * the original zero's written out previously to the journal and
 * awaiting writeback in the kernel's buffer cache. 
 *
 * So, if we see any bmap calls here on a modified, data-journaled file,
 * take extra steps to flush any blocks which might be in the cache. 
 */
static int ext3_bmap(struct address_space *mapping, long block)
{
	struct inode *inode = mapping->host;
	journal_t *journal;
	int err;
	
	if (EXT3_I(inode)->i_state & EXT3_STATE_JDATA) {
		/* 
		 * This is a REALLY heavyweight approach, but the use of
		 * bmap on dirty files is expected to be extremely rare:
		 * only if we run lilo or swapon on a freshly made file
		 * do we expect this to happen. 
		 *
		 * (bmap requires CAP_SYS_RAWIO so this does not
		 * represent an unprivileged user DOS attack --- we'd be
		 * in trouble if mortal users could trigger this path at
		 * will.) 
		 *
		 * NB. EXT3_STATE_JDATA is not set on files other than
		 * regular files.  If somebody wants to bmap a directory
		 * or symlink and gets confused because the buffer
		 * hasn't yet been flushed to disk, they deserve
		 * everything they get.
		 */
		
		EXT3_I(inode)->i_state &= ~EXT3_STATE_JDATA;
		journal = EXT3_JOURNAL(inode);
		journal_lock_updates(journal);
		err = journal_flush(journal);
		journal_unlock_updates(journal);
		
		if (err)
			return 0;
	}
	
	return generic_block_bmap(mapping,block,ext3_get_block);
}

static int bget_one(handle_t *handle, struct inode *inode, 
		    struct buffer_head *bh)
{
	atomic_inc(&bh->b_count);
	return 0;
}

/*
 * Note that we always start a transaction even if we're not journalling
 * data.  This is to preserve ordering: any hole instantiation within
 * __block_write_full_page -> ext3_get_block() should be journalled
 * along with the data so we don't crash and then get metadata which
 * refers to old data.
 *
 * In all journalling modes block_write_full_page() will start the I/O.
 *
 * Problem:
 *
 *	ext3_writepage() -> kmalloc() -> __alloc_pages() -> page_launder() ->
 *		ext3_writepage()
 *
 * Similar for:
 *
 *	ext3_file_write() -> generic_file_write() -> __alloc_pages() -> ...
 *
 * Same applies to ext3_get_block().  We will deadlock on various things like
 * lock_journal and i_truncate_sem.
 *
 * Setting PF_MEMALLOC here doesn't work - too many internal memory
 * allocations fail.
 *
 * 16May01: If we're reentered then journal_current_handle() will be
 *	    non-zero. We simply *return*.
 *
 * 1 July 2001: @@@ FIXME:
 *   In journalled data mode, a data buffer may be metadata against the
 *   current transaction.  But the same file is part of a shared mapping
 *   and someone does a writepage() on it.
 *
 *   We will move the buffer onto the async_data list, but *after* it has
 *   been dirtied. So there's a small window where we have dirty data on
 *   BJ_Metadata.
 *
 *   Note that this only applies to the last partial page in the file.  The
 *   bit which block_write_full_page() uses prepare/commit for.  (That's
 *   broken code anyway: it's wrong for msync()).
 *
 *   It's a rare case: affects the final partial page, for journalled data
 *   where the file is subject to bith write() and writepage() in the same
 *   transction.  To fix it we'll need a custom block_write_full_page().
 *   We'll probably need that anyway for journalling writepage() output.
 *
 * We don't honour synchronous mounts for writepage().  That would be
 * disastrous.  Any write() or metadata operation will sync the fs for
 * us.
 */
static int ext3_writepage(struct page *page)
{
	struct inode *inode = page->mapping->host;
	struct buffer_head *page_buffers;
	handle_t *handle = NULL;
	int ret = 0, err;
	int needed;
	int order_data;

	J_ASSERT(PageLocked(page));
	
	/*
	 * We give up here if we're reentered, because it might be
	 * for a different filesystem.  One *could* look for a
	 * nested transaction opportunity.
	 */
	lock_kernel();
	if (ext3_journal_current_handle())
		goto out_fail;

	needed = ext3_writepage_trans_blocks(inode);
	if (current->flags & PF_MEMALLOC)
		handle = ext3_journal_try_start(inode, needed);
	else
		handle = ext3_journal_start(inode, needed);
				
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out_fail;
	}

	order_data = ext3_should_order_data(inode) ||
			ext3_should_journal_data(inode);

	unlock_kernel();

	page_buffers = NULL;	/* Purely to prevent compiler warning */

	/* bget() all the buffers */
	if (order_data) {
		if (!page->buffers)
			create_empty_buffers(page,
				inode->i_dev, inode->i_sb->s_blocksize);
		page_buffers = page->buffers;
		walk_page_buffers(handle, inode, page_buffers, 0,
				PAGE_CACHE_SIZE, NULL, bget_one);
	}

	ret = block_write_full_page(page, ext3_get_block);

	/*
	 * The page can become unlocked at any point now, and
	 * truncate can then come in and change things.  So we
	 * can't touch *page from now on.  But *page_buffers is
	 * safe due to elevated refcount.
	 */

	handle = ext3_journal_current_handle();
	lock_kernel();

	/* And attach them to the current transaction */
	if (order_data) {
		err = walk_page_buffers(handle, inode, page_buffers,
			0, PAGE_CACHE_SIZE, NULL, journal_dirty_async_data);
		if (!ret)
			ret = err;
	}

	err = ext3_journal_stop(handle, inode);
	if (!ret)
		ret = err;
	unlock_kernel();
	return ret;

out_fail:
	
	unlock_kernel();
	SetPageDirty(page);
	UnlockPage(page);
	return ret;
}

static int ext3_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page,ext3_get_block);
}


static int ext3_flushpage(struct page *page, unsigned long offset)
{
	journal_t *journal = EXT3_JOURNAL(page->mapping->host);
	return journal_flushpage(journal, page, offset);
}

static int ext3_releasepage(struct page *page, int wait)
{
	journal_t *journal = EXT3_JOURNAL(page->mapping->host);
	return journal_try_to_free_buffers(journal, page, wait);
}


struct address_space_operations ext3_aops = {
	readpage:	ext3_readpage,		/* BKL not held.  Don't need */
	writepage:	ext3_writepage,		/* BKL not held.  We take it */
	sync_page:	block_sync_page,
	prepare_write:	ext3_prepare_write,	/* BKL not held.  We take it */
	commit_write:	ext3_commit_write,	/* BKL not held.  We take it */
	bmap:		ext3_bmap,		/* BKL held */
	flushpage:	ext3_flushpage,		/* BKL not held.  Don't need */
	releasepage:	ext3_releasepage,	/* BKL not held.  Don't need */
};

/*
 * ext3_block_truncate_page() zeroes out a mapping from file offset `from'
 * up to the end of the block which corresponds to `from'.
 * This required during truncate. We need to physically zero the tail end
 * of that block so it doesn't yield old data if the file is later grown.
 */
static int ext3_block_truncate_page(handle_t *handle,
		struct address_space *mapping, loff_t from)
{
	unsigned long index = from >> PAGE_CACHE_SHIFT;
	unsigned offset = from & (PAGE_CACHE_SIZE-1);
	unsigned blocksize, iblock, length, pos;
	struct inode *inode = mapping->host;
	struct page *page;
	struct buffer_head *bh;
	int err;

	blocksize = inode->i_sb->s_blocksize;
	length = offset & (blocksize - 1);

	/* Block boundary? Nothing to do */
	if (!length)
		return 0;

	length = blocksize - length;
	iblock = index << (PAGE_CACHE_SHIFT - inode->i_sb->s_blocksize_bits);

	page = find_or_create_page(mapping, index, GFP_NOFS);
	err = -ENOMEM;
	if (!page)
		goto out;

	if (!page->buffers)
		create_empty_buffers(page, inode->i_dev, blocksize);

	/* Find the buffer that contains "offset" */
	bh = page->buffers;
	pos = blocksize;
	while (offset >= pos) {
		bh = bh->b_this_page;
		iblock++;
		pos += blocksize;
	}

	err = 0;
	if (!buffer_mapped(bh)) {
		/* Hole? Nothing to do */
		if (buffer_uptodate(bh))
			goto unlock;
		ext3_get_block(inode, iblock, bh, 0);
		/* Still unmapped? Nothing to do */
		if (!buffer_mapped(bh))
			goto unlock;
	}

	/* Ok, it's mapped. Make sure it's up-to-date */
	if (Page_Uptodate(page))
		set_bit(BH_Uptodate, &bh->b_state);

	if (!buffer_uptodate(bh)) {
		err = -EIO;
		ll_rw_block(READ, 1, &bh);
		wait_on_buffer(bh);
		/* Uhhuh. Read error. Complain and punt. */
		if (!buffer_uptodate(bh))
			goto unlock;
	}

	if (ext3_should_journal_data(inode)) {
		BUFFER_TRACE(bh, "get write access");
		err = ext3_journal_get_write_access(handle, bh);
		if (err)
			goto unlock;
	}
	
	memset(kmap(page) + offset, 0, length);
	flush_dcache_page(page);
	kunmap(page);

	BUFFER_TRACE(bh, "zeroed end of block");

	err = 0;
	if (ext3_should_journal_data(inode)) {
		err = ext3_journal_dirty_metadata(handle, bh);
	} else {
		if (ext3_should_order_data(inode))
			err = ext3_journal_dirty_data(handle, bh, 0);
		__mark_buffer_dirty(bh);
	}

unlock:
	UnlockPage(page);
	page_cache_release(page);
out:
	return err;
}

/*
 * Probably it should be a library function... search for first non-zero word
 * or memcmp with zero_page, whatever is better for particular architecture.
 * Linus?
 */
static inline int all_zeroes(u32 *p, u32 *q)
{
	while (p < q)
		if (*p++)
			return 0;
	return 1;
}

/**
 *	ext3_find_shared - find the indirect blocks for partial truncation.
 *	@inode:	  inode in question
 *	@depth:	  depth of the affected branch
 *	@offsets: offsets of pointers in that branch (see ext3_block_to_path)
 *	@chain:	  place to store the pointers to partial indirect blocks
 *	@top:	  place to the (detached) top of branch
 *
 *	This is a helper function used by ext3_truncate().
 *
 *	When we do truncate() we may have to clean the ends of several
 *	indirect blocks but leave the blocks themselves alive. Block is
 *	partially truncated if some data below the new i_size is refered
 *	from it (and it is on the path to the first completely truncated
 *	data block, indeed).  We have to free the top of that path along
 *	with everything to the right of the path. Since no allocation
 *	past the truncation point is possible until ext3_truncate()
 *	finishes, we may safely do the latter, but top of branch may
 *	require special attention - pageout below the truncation point
 *	might try to populate it.
 *
 *	We atomically detach the top of branch from the tree, store the
 *	block number of its root in *@top, pointers to buffer_heads of
 *	partially truncated blocks - in @chain[].bh and pointers to
 *	their last elements that should not be removed - in
 *	@chain[].p. Return value is the pointer to last filled element
 *	of @chain.
 *
 *	The work left to caller to do the actual freeing of subtrees:
 *		a) free the subtree starting from *@top
 *		b) free the subtrees whose roots are stored in
 *			(@chain[i].p+1 .. end of @chain[i].bh->b_data)
 *		c) free the subtrees growing from the inode past the @chain[0].
 *			(no partially truncated stuff there).  */

static Indirect *ext3_find_shared(struct inode *inode,
				int depth,
				int offsets[4],
				Indirect chain[4],
				u32 *top)
{
	Indirect *partial, *p;
	int k, err;

	*top = 0;
	/* Make k index the deepest non-null offest + 1 */
	for (k = depth; k > 1 && !offsets[k-1]; k--)
		;
	partial = ext3_get_branch(inode, k, offsets, chain, &err);
	/* Writer: pointers */
	if (!partial)
		partial = chain + k-1;
	/*
	 * If the branch acquired continuation since we've looked at it -
	 * fine, it should all survive and (new) top doesn't belong to us.
	 */
	if (!partial->key && *partial->p)
		/* Writer: end */
		goto no_top;
	for (p=partial; p>chain && all_zeroes((u32*)p->bh->b_data,p->p); p--)
		;
	/*
	 * OK, we've found the last block that must survive. The rest of our
	 * branch should be detached before unlocking. However, if that rest
	 * of branch is all ours and does not grow immediately from the inode
	 * it's easier to cheat and just decrement partial->p.
	 */
	if (p == chain + k - 1 && p > chain) {
		p->p--;
	} else {
		*top = *p->p;
		/* Nope, don't do this in ext3.  Must leave the tree intact */
#if 0
		*p->p = 0;
#endif
	}
	/* Writer: end */

	while(partial > p)
	{
		brelse(partial->bh);
		partial--;
	}
no_top:
	return partial;
}

/*
 * Zero a number of block pointers in either an inode or an indirect block.
 * If we restart the transaction we must again get write access to the
 * indirect block for further modification.
 *
 * We release `count' blocks on disk, but (last - first) may be greater
 * than `count' because there can be holes in there.
 */
static void
ext3_clear_blocks(handle_t *handle, struct inode *inode, struct buffer_head *bh,
		unsigned long block_to_free, unsigned long count,
		u32 *first, u32 *last)
{
	u32 *p;
	if (try_to_extend_transaction(handle, inode)) {
		if (bh) {
			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			ext3_journal_dirty_metadata(handle, bh);
		}
		ext3_mark_inode_dirty(handle, inode);
		ext3_journal_test_restart(handle, inode);
		if (bh) {
			BUFFER_TRACE(bh, "retaking write access");
			ext3_journal_get_write_access(handle, bh);
		}
	}

	/*
	 * Any buffers which are on the journal will be in memory. We find
	 * them on the hash table so journal_revoke() will run journal_forget()
	 * on them.  We've already detached each block from the file, so
	 * bforget() in journal_forget() should be safe.
	 *
	 * AKPM: turn on bforget in journal_forget()!!!
	 */
	for (p = first; p < last; p++) {
		u32 nr = le32_to_cpu(*p);
		if (nr) {
			struct buffer_head *bh;

			*p = 0;
			bh = sb_get_hash_table(inode->i_sb, nr);
			ext3_forget(handle, 0, inode, bh, nr);
		}
	}

	ext3_free_blocks(handle, inode, block_to_free, count);
}

/**
 * ext3_free_data - free a list of data blocks
 * @handle:	handle for this transaction
 * @inode:	inode we are dealing with
 * @this_bh:	indirect buffer_head which contains *@first and *@last
 * @first:	array of block numbers
 * @last:	points immediately past the end of array
 *
 * We are freeing all blocks refered from that array (numbers are stored as
 * little-endian 32-bit) and updating @inode->i_blocks appropriately.
 *
 * We accumulate contiguous runs of blocks to free.  Conveniently, if these
 * blocks are contiguous then releasing them at one time will only affect one
 * or two bitmap blocks (+ group descriptor(s) and superblock) and we won't
 * actually use a lot of journal space.
 *
 * @this_bh will be %NULL if @first and @last point into the inode's direct
 * block pointers.
 */
static void ext3_free_data(handle_t *handle, struct inode *inode,
			   struct buffer_head *this_bh, u32 *first, u32 *last)
{
	unsigned long block_to_free = 0;    /* Starting block # of a run */
	unsigned long count = 0;	    /* Number of blocks in the run */ 
	u32 *block_to_free_p = NULL;	    /* Pointer into inode/ind
					       corresponding to
					       block_to_free */
	unsigned long nr;		    /* Current block # */
	u32 *p;				    /* Pointer into inode/ind
					       for current block */
	int err;

	if (this_bh) {				/* For indirect block */
		BUFFER_TRACE(this_bh, "get_write_access");
		err = ext3_journal_get_write_access(handle, this_bh);
		/* Important: if we can't update the indirect pointers
		 * to the blocks, we can't free them. */
		if (err)
			return;
	}

	for (p = first; p < last; p++) {
		nr = le32_to_cpu(*p);
		if (nr) {
			/* accumulate blocks to free if they're contiguous */
			if (count == 0) {
				block_to_free = nr;
				block_to_free_p = p;
				count = 1;
			} else if (nr == block_to_free + count) {
				count++;
			} else {
				ext3_clear_blocks(handle, inode, this_bh, 
						  block_to_free,
						  count, block_to_free_p, p);
				block_to_free = nr;
				block_to_free_p = p;
				count = 1;
			}
		}
	}

	if (count > 0)
		ext3_clear_blocks(handle, inode, this_bh, block_to_free,
				  count, block_to_free_p, p);

	if (this_bh) {
		BUFFER_TRACE(this_bh, "call ext3_journal_dirty_metadata");
		ext3_journal_dirty_metadata(handle, this_bh);
	}
}

/**
 *	ext3_free_branches - free an array of branches
 *	@handle: JBD handle for this transaction
 *	@inode:	inode we are dealing with
 *	@parent_bh: the buffer_head which contains *@first and *@last
 *	@first:	array of block numbers
 *	@last:	pointer immediately past the end of array
 *	@depth:	depth of the branches to free
 *
 *	We are freeing all blocks refered from these branches (numbers are
 *	stored as little-endian 32-bit) and updating @inode->i_blocks
 *	appropriately.
 */
static void ext3_free_branches(handle_t *handle, struct inode *inode,
			       struct buffer_head *parent_bh,
			       u32 *first, u32 *last, int depth)
{
	unsigned long nr;
	u32 *p;

	if (is_handle_aborted(handle))
		return;
	
	if (depth--) {
		struct buffer_head *bh;
		int addr_per_block = EXT3_ADDR_PER_BLOCK(inode->i_sb);
		p = last;
		while (--p >= first) {
			nr = le32_to_cpu(*p);
			if (!nr)
				continue;		/* A hole */

			/* Go read the buffer for the next level down */
			bh = sb_bread(inode->i_sb, nr);

			/*
			 * A read failure? Report error and clear slot
			 * (should be rare).
			 */
			if (!bh) {
				ext3_error(inode->i_sb, "ext3_free_branches",
					   "Read failure, inode=%ld, block=%ld",
					   inode->i_ino, nr);
				continue;
			}

			/* This zaps the entire block.  Bottom up. */
			BUFFER_TRACE(bh, "free child branches");
			ext3_free_branches(handle, inode, bh, (u32*)bh->b_data,
					   (u32*)bh->b_data + addr_per_block,
					   depth);

			/*
			 * We've probably journalled the indirect block several
			 * times during the truncate.  But it's no longer
			 * needed and we now drop it from the transaction via
			 * journal_revoke().
			 *
			 * That's easy if it's exclusively part of this
			 * transaction.  But if it's part of the committing
			 * transaction then journal_forget() will simply
			 * brelse() it.  That means that if the underlying
			 * block is reallocated in ext3_get_block(),
			 * unmap_underlying_metadata() will find this block
			 * and will try to get rid of it.  damn, damn.
			 *
			 * If this block has already been committed to the
			 * journal, a revoke record will be written.  And
			 * revoke records must be emitted *before* clearing
			 * this block's bit in the bitmaps.
			 */
			ext3_forget(handle, 1, inode, bh, bh->b_blocknr);

			/*
			 * Everything below this this pointer has been
			 * released.  Now let this top-of-subtree go.
			 *
			 * We want the freeing of this indirect block to be
			 * atomic in the journal with the updating of the
			 * bitmap block which owns it.  So make some room in
			 * the journal.
			 *
			 * We zero the parent pointer *after* freeing its
			 * pointee in the bitmaps, so if extend_transaction()
			 * for some reason fails to put the bitmap changes and
			 * the release into the same transaction, recovery
			 * will merely complain about releasing a free block,
			 * rather than leaking blocks.
			 */
			if (is_handle_aborted(handle))
				return;
			if (try_to_extend_transaction(handle, inode)) {
				ext3_mark_inode_dirty(handle, inode);
				ext3_journal_test_restart(handle, inode);
			}

			ext3_free_blocks(handle, inode, nr, 1);

			if (parent_bh) {
				/*
				 * The block which we have just freed is
				 * pointed to by an indirect block: journal it
				 */
				BUFFER_TRACE(parent_bh, "get_write_access");
				if (!ext3_journal_get_write_access(handle,
								   parent_bh)){
					*p = 0;
					BUFFER_TRACE(parent_bh,
					"call ext3_journal_dirty_metadata");
					ext3_journal_dirty_metadata(handle, 
								    parent_bh);
				}
			}
		}
	} else {
		/* We have reached the bottom of the tree. */
		BUFFER_TRACE(parent_bh, "free data blocks");
		ext3_free_data(handle, inode, parent_bh, first, last);
	}
}

/*
 * ext3_truncate()
 *
 * We block out ext3_get_block() block instantiations across the entire
 * transaction, and VFS/VM ensures that ext3_truncate() cannot run
 * simultaneously on behalf of the same inode.
 *
 * As we work through the truncate and commmit bits of it to the journal there
 * is one core, guiding principle: the file's tree must always be consistent on
 * disk.  We must be able to restart the truncate after a crash.
 *
 * The file's tree may be transiently inconsistent in memory (although it
 * probably isn't), but whenever we close off and commit a journal transaction,
 * the contents of (the filesystem + the journal) must be consistent and
 * restartable.  It's pretty simple, really: bottom up, right to left (although
 * left-to-right works OK too).
 *
 * Note that at recovery time, journal replay occurs *before* the restart of
 * truncate against the orphan inode list.
 *
 * The committed inode has the new, desired i_size (which is the same as
 * i_disksize in this case).  After a crash, ext3_orphan_cleanup() will see
 * that this inode's truncate did not complete and it will again call
 * ext3_truncate() to have another go.  So there will be instantiated blocks
 * to the right of the truncation point in a crashed ext3 filesystem.  But
 * that's fine - as long as they are linked from the inode, the post-crash
 * ext3_truncate() run will find them and release them.
 */

void ext3_truncate(struct inode * inode)
{
	handle_t *handle;
	u32 *i_data = inode->u.ext3_i.i_data;
	int addr_per_block = EXT3_ADDR_PER_BLOCK(inode->i_sb);
	int offsets[4];
	Indirect chain[4];
	Indirect *partial;
	int nr = 0;
	int n;
	long last_block;
	unsigned blocksize;

	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode) ||
	    S_ISLNK(inode->i_mode)))
		return;
	if (ext3_inode_is_fast_symlink(inode))
		return;
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return;

	ext3_discard_prealloc(inode);

	handle = start_transaction(inode);
	if (IS_ERR(handle))
		return;		/* AKPM: return what? */

	blocksize = inode->i_sb->s_blocksize;
	last_block = (inode->i_size + blocksize-1)
					>> EXT3_BLOCK_SIZE_BITS(inode->i_sb);

	ext3_block_truncate_page(handle, inode->i_mapping, inode->i_size);
		

	n = ext3_block_to_path(inode, last_block, offsets);
	if (n == 0)
		goto out_stop;	/* error */

	/*
	 * OK.  This truncate is going to happen.  We add the inode to the
	 * orphan list, so that if this truncate spans multiple transactions,
	 * and we crash, we will resume the truncate when the filesystem
	 * recovers.  It also marks the inode dirty, to catch the new size.
	 *
	 * Implication: the file must always be in a sane, consistent
	 * truncatable state while each transaction commits.
	 */
	if (ext3_orphan_add(handle, inode))
		goto out_stop;

	/*
	 * The orphan list entry will now protect us from any crash which
	 * occurs before the truncate completes, so it is now safe to propagate
	 * the new, shorter inode size (held for now in i_size) into the
	 * on-disk inode. We do this via i_disksize, which is the value which
	 * ext3 *really* writes onto the disk inode.
	 */
	inode->u.ext3_i.i_disksize = inode->i_size;

	/*
	 * From here we block out all ext3_get_block() callers who want to
	 * modify the block allocation tree.
	 */
	down_write(&inode->u.ext3_i.truncate_sem);

	if (n == 1) {		/* direct blocks */
		ext3_free_data(handle, inode, NULL, i_data+offsets[0],
			       i_data + EXT3_NDIR_BLOCKS);
		goto do_indirects;
	}

	partial = ext3_find_shared(inode, n, offsets, chain, &nr);
	/* Kill the top of shared branch (not detached) */
	if (nr) {
		if (partial == chain) {
			/* Shared branch grows from the inode */
			ext3_free_branches(handle, inode, NULL,
					   &nr, &nr+1, (chain+n-1) - partial);
			*partial->p = 0;
			/*
			 * We mark the inode dirty prior to restart,
			 * and prior to stop.  No need for it here.
			 */
		} else {
			/* Shared branch grows from an indirect block */
			BUFFER_TRACE(partial->bh, "get_write_access");
			ext3_free_branches(handle, inode, partial->bh,
					partial->p,
					partial->p+1, (chain+n-1) - partial);
		}
	}
	/* Clear the ends of indirect blocks on the shared branch */
	while (partial > chain) {
		ext3_free_branches(handle, inode, partial->bh, partial->p + 1,
				   (u32*)partial->bh->b_data + addr_per_block,
				   (chain+n-1) - partial);
		BUFFER_TRACE(partial->bh, "call brelse");
		brelse (partial->bh);
		partial--;
	}
do_indirects:
	/* Kill the remaining (whole) subtrees */
	switch (offsets[0]) {
		default:
			nr = i_data[EXT3_IND_BLOCK];
			if (nr) {
				ext3_free_branches(handle, inode, NULL,
						   &nr, &nr+1, 1);
				i_data[EXT3_IND_BLOCK] = 0;
			}
		case EXT3_IND_BLOCK:
			nr = i_data[EXT3_DIND_BLOCK];
			if (nr) {
				ext3_free_branches(handle, inode, NULL,
						   &nr, &nr+1, 2);
				i_data[EXT3_DIND_BLOCK] = 0;
			}
		case EXT3_DIND_BLOCK:
			nr = i_data[EXT3_TIND_BLOCK];
			if (nr) {
				ext3_free_branches(handle, inode, NULL,
						   &nr, &nr+1, 3);
				i_data[EXT3_TIND_BLOCK] = 0;
			}
		case EXT3_TIND_BLOCK:
			;
	}
	up_write(&inode->u.ext3_i.truncate_sem);
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	ext3_mark_inode_dirty(handle, inode);

	/* In a multi-transaction truncate, we only make the final
	 * transaction synchronous */
	if (IS_SYNC(inode))
		handle->h_sync = 1;
out_stop:
	/*
	 * If this was a simple ftruncate(), and the file will remain alive
	 * then we need to clear up the orphan record which we created above.
	 * However, if this was a real unlink then we were called by
	 * ext3_delete_inode(), and we allow that function to clean up the
	 * orphan info for us.
	 */
	if (inode->i_nlink)
		ext3_orphan_del(handle, inode);

	ext3_journal_stop(handle, inode);
}

/* 
 * ext3_get_inode_loc returns with an extra refcount against the
 * inode's underlying buffer_head on success. 
 */

int ext3_get_inode_loc (struct inode *inode, struct ext3_iloc *iloc)
{
	struct buffer_head *bh = 0;
	unsigned long block;
	unsigned long block_group;
	unsigned long group_desc;
	unsigned long desc;
	unsigned long offset;
	struct ext3_group_desc * gdp;
		
	if ((inode->i_ino != EXT3_ROOT_INO &&
		inode->i_ino != EXT3_ACL_IDX_INO &&
		inode->i_ino != EXT3_ACL_DATA_INO &&
		inode->i_ino != EXT3_JOURNAL_INO &&
		inode->i_ino < EXT3_FIRST_INO(inode->i_sb)) ||
		inode->i_ino > le32_to_cpu(
			inode->i_sb->u.ext3_sb.s_es->s_inodes_count)) {
		ext3_error (inode->i_sb, "ext3_get_inode_loc",
			    "bad inode number: %lu", inode->i_ino);
		goto bad_inode;
	}
	block_group = (inode->i_ino - 1) / EXT3_INODES_PER_GROUP(inode->i_sb);
	if (block_group >= inode->i_sb->u.ext3_sb.s_groups_count) {
		ext3_error (inode->i_sb, "ext3_get_inode_loc",
			    "group >= groups count");
		goto bad_inode;
	}
	group_desc = block_group >> EXT3_DESC_PER_BLOCK_BITS(inode->i_sb);
	desc = block_group & (EXT3_DESC_PER_BLOCK(inode->i_sb) - 1);
	bh = inode->i_sb->u.ext3_sb.s_group_desc[group_desc];
	if (!bh) {
		ext3_error (inode->i_sb, "ext3_get_inode_loc",
			    "Descriptor not loaded");
		goto bad_inode;
	}

	gdp = (struct ext3_group_desc *) bh->b_data;
	/*
	 * Figure out the offset within the block group inode table
	 */
	offset = ((inode->i_ino - 1) % EXT3_INODES_PER_GROUP(inode->i_sb)) *
		EXT3_INODE_SIZE(inode->i_sb);
	block = le32_to_cpu(gdp[desc].bg_inode_table) +
		(offset >> EXT3_BLOCK_SIZE_BITS(inode->i_sb));
	if (!(bh = sb_bread(inode->i_sb, block))) {
		ext3_error (inode->i_sb, "ext3_get_inode_loc",
			    "unable to read inode block - "
			    "inode=%lu, block=%lu", inode->i_ino, block);
		goto bad_inode;
	}
	offset &= (EXT3_BLOCK_SIZE(inode->i_sb) - 1);

	iloc->bh = bh;
	iloc->raw_inode = (struct ext3_inode *) (bh->b_data + offset);
	iloc->block_group = block_group;
	
	return 0;
	
 bad_inode:
	return -EIO;
}

void ext3_set_inode_flags(struct inode *inode)
{
	unsigned int flags = inode->u.ext3_i.i_flags;

	inode->i_flags &= ~(S_SYNC|S_APPEND|S_IMMUTABLE|S_NOATIME);
	if (flags & EXT3_SYNC_FL)
		inode->i_flags |= S_SYNC;
	if (flags & EXT3_APPEND_FL)
		inode->i_flags |= S_APPEND;
	if (flags & EXT3_IMMUTABLE_FL)
		inode->i_flags |= S_IMMUTABLE;
	if (flags & EXT3_NOATIME_FL)
		inode->i_flags |= S_NOATIME;
}


void ext3_read_inode(struct inode * inode)
{
	struct ext3_iloc iloc;
	struct ext3_inode *raw_inode;
	struct buffer_head *bh;
	int block;
	
	if(ext3_get_inode_loc(inode, &iloc))
		goto bad_inode;
	bh = iloc.bh;
	raw_inode = iloc.raw_inode;
	init_rwsem(&inode->u.ext3_i.truncate_sem);
	inode->i_mode = le16_to_cpu(raw_inode->i_mode);
	inode->i_uid = (uid_t)le16_to_cpu(raw_inode->i_uid_low);
	inode->i_gid = (gid_t)le16_to_cpu(raw_inode->i_gid_low);
	if(!(test_opt (inode->i_sb, NO_UID32))) {
		inode->i_uid |= le16_to_cpu(raw_inode->i_uid_high) << 16;
		inode->i_gid |= le16_to_cpu(raw_inode->i_gid_high) << 16;
	}
	inode->i_nlink = le16_to_cpu(raw_inode->i_links_count);
	inode->i_size = le32_to_cpu(raw_inode->i_size);
	inode->i_atime = le32_to_cpu(raw_inode->i_atime);
	inode->i_ctime = le32_to_cpu(raw_inode->i_ctime);
	inode->i_mtime = le32_to_cpu(raw_inode->i_mtime);
	inode->u.ext3_i.i_dtime = le32_to_cpu(raw_inode->i_dtime);
	/* We now have enough fields to check if the inode was active or not.
	 * This is needed because nfsd might try to access dead inodes
	 * the test is that same one that e2fsck uses
	 * NeilBrown 1999oct15
	 */
	if (inode->i_nlink == 0) {
		if (inode->i_mode == 0 ||
		    !(inode->i_sb->u.ext3_sb.s_mount_state & EXT3_ORPHAN_FS)) {
			/* this inode is deleted */
			brelse (bh);
			goto bad_inode;
		}
		/* The only unlinked inodes we let through here have
		 * valid i_mode and are being read by the orphan
		 * recovery code: that's fine, we're about to complete
		 * the process of deleting those. */
	}
	inode->i_blksize = PAGE_SIZE;	/* This is the optimal IO size
					 * (for stat), not the fs block
					 * size */  
	inode->i_blocks = le32_to_cpu(raw_inode->i_blocks);
	inode->i_version = ++event;
	inode->u.ext3_i.i_flags = le32_to_cpu(raw_inode->i_flags);
#ifdef EXT3_FRAGMENTS
	inode->u.ext3_i.i_faddr = le32_to_cpu(raw_inode->i_faddr);
	inode->u.ext3_i.i_frag_no = raw_inode->i_frag;
	inode->u.ext3_i.i_frag_size = raw_inode->i_fsize;
#endif
	inode->u.ext3_i.i_file_acl = le32_to_cpu(raw_inode->i_file_acl);
	if (!S_ISREG(inode->i_mode)) {
		inode->u.ext3_i.i_dir_acl = le32_to_cpu(raw_inode->i_dir_acl);
	} else {
		inode->i_size |=
			((__u64)le32_to_cpu(raw_inode->i_size_high)) << 32;
	}
	inode->u.ext3_i.i_disksize = inode->i_size;
	inode->i_generation = le32_to_cpu(raw_inode->i_generation);
#ifdef EXT3_PREALLOCATE
	inode->u.ext3_i.i_prealloc_count = 0;
#endif
	inode->u.ext3_i.i_block_group = iloc.block_group;

	/*
	 * NOTE! The in-memory inode i_data array is in little-endian order
	 * even on big-endian machines: we do NOT byteswap the block numbers!
	 */
	for (block = 0; block < EXT3_N_BLOCKS; block++)
		inode->u.ext3_i.i_data[block] = iloc.raw_inode->i_block[block];
	INIT_LIST_HEAD(&inode->u.ext3_i.i_orphan);

	if (inode->i_ino == EXT3_ACL_IDX_INO ||
	    inode->i_ino == EXT3_ACL_DATA_INO)
		/* Nothing to do */ ;
	else if (S_ISREG(inode->i_mode)) {
		inode->i_op = &ext3_file_inode_operations;
		inode->i_fop = &ext3_file_operations;
		inode->i_mapping->a_ops = &ext3_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &ext3_dir_inode_operations;
		inode->i_fop = &ext3_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		if (ext3_inode_is_fast_symlink(inode))
			inode->i_op = &ext3_fast_symlink_inode_operations;
		else {
			inode->i_op = &page_symlink_inode_operations;
			inode->i_mapping->a_ops = &ext3_aops;
		}
	} else 
		init_special_inode(inode, inode->i_mode,
				   le32_to_cpu(iloc.raw_inode->i_block[0]));
	brelse(iloc.bh);
	ext3_set_inode_flags(inode);
	return;
	
bad_inode:
	make_bad_inode(inode);
	return;
}

/*
 * Post the struct inode info into an on-disk inode location in the
 * buffer-cache.  This gobbles the caller's reference to the
 * buffer_head in the inode location struct.  
 */

static int ext3_do_update_inode(handle_t *handle, 
				struct inode *inode, 
				struct ext3_iloc *iloc)
{
	struct ext3_inode *raw_inode = iloc->raw_inode;
	struct buffer_head *bh = iloc->bh;
	int err = 0, rc, block;

	if (handle) {
		BUFFER_TRACE(bh, "get_write_access");
		err = ext3_journal_get_write_access(handle, bh);
		if (err)
			goto out_brelse;
	}
	/* For fields not not tracking in the in-memory inode,
	 * initialise them to zero for new inodes. */
	if (EXT3_I(inode)->i_state & EXT3_STATE_NEW)
		memset(raw_inode, 0, EXT3_SB(inode->i_sb)->s_inode_size);

	raw_inode->i_mode = cpu_to_le16(inode->i_mode);
	if(!(test_opt(inode->i_sb, NO_UID32))) {
		raw_inode->i_uid_low = cpu_to_le16(low_16_bits(inode->i_uid));
		raw_inode->i_gid_low = cpu_to_le16(low_16_bits(inode->i_gid));
/*
 * Fix up interoperability with old kernels. Otherwise, old inodes get
 * re-used with the upper 16 bits of the uid/gid intact
 */
		if(!inode->u.ext3_i.i_dtime) {
			raw_inode->i_uid_high =
				cpu_to_le16(high_16_bits(inode->i_uid));
			raw_inode->i_gid_high =
				cpu_to_le16(high_16_bits(inode->i_gid));
		} else {
			raw_inode->i_uid_high = 0;
			raw_inode->i_gid_high = 0;
		}
	} else {
		raw_inode->i_uid_low =
			cpu_to_le16(fs_high2lowuid(inode->i_uid));
		raw_inode->i_gid_low =
			cpu_to_le16(fs_high2lowgid(inode->i_gid));
		raw_inode->i_uid_high = 0;
		raw_inode->i_gid_high = 0;
	}
	raw_inode->i_links_count = cpu_to_le16(inode->i_nlink);
	raw_inode->i_size = cpu_to_le32(inode->u.ext3_i.i_disksize);
	raw_inode->i_atime = cpu_to_le32(inode->i_atime);
	raw_inode->i_ctime = cpu_to_le32(inode->i_ctime);
	raw_inode->i_mtime = cpu_to_le32(inode->i_mtime);
	raw_inode->i_blocks = cpu_to_le32(inode->i_blocks);
	raw_inode->i_dtime = cpu_to_le32(inode->u.ext3_i.i_dtime);
	raw_inode->i_flags = cpu_to_le32(inode->u.ext3_i.i_flags);
#ifdef EXT3_FRAGMENTS
	raw_inode->i_faddr = cpu_to_le32(inode->u.ext3_i.i_faddr);
	raw_inode->i_frag = inode->u.ext3_i.i_frag_no;
	raw_inode->i_fsize = inode->u.ext3_i.i_frag_size;
#endif
	raw_inode->i_file_acl = cpu_to_le32(inode->u.ext3_i.i_file_acl);
	if (!S_ISREG(inode->i_mode)) {
		raw_inode->i_dir_acl = cpu_to_le32(inode->u.ext3_i.i_dir_acl);
	} else {
		raw_inode->i_size_high =
			cpu_to_le32(inode->u.ext3_i.i_disksize >> 32);
		if (inode->u.ext3_i.i_disksize > 0x7fffffffULL) {
			struct super_block *sb = inode->i_sb;
			if (!EXT3_HAS_RO_COMPAT_FEATURE(sb,
					EXT3_FEATURE_RO_COMPAT_LARGE_FILE) ||
			    EXT3_SB(sb)->s_es->s_rev_level ==
					cpu_to_le32(EXT3_GOOD_OLD_REV)) {
			       /* If this is the first large file
				* created, add a flag to the superblock.
				*/
				err = ext3_journal_get_write_access(handle,
						sb->u.ext3_sb.s_sbh);
				if (err)
					goto out_brelse;
				ext3_update_dynamic_rev(sb);
				EXT3_SET_RO_COMPAT_FEATURE(sb,
					EXT3_FEATURE_RO_COMPAT_LARGE_FILE);
				sb->s_dirt = 1;
				handle->h_sync = 1;
				err = ext3_journal_dirty_metadata(handle,
						sb->u.ext3_sb.s_sbh);
			}
		}
	}
	raw_inode->i_generation = cpu_to_le32(inode->i_generation);
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		raw_inode->i_block[0] =
			cpu_to_le32(kdev_t_to_nr(inode->i_rdev));
	else for (block = 0; block < EXT3_N_BLOCKS; block++)
		raw_inode->i_block[block] = inode->u.ext3_i.i_data[block];

	BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
	rc = ext3_journal_dirty_metadata(handle, bh);
	if (!err)
		err = rc;
	EXT3_I(inode)->i_state &= ~EXT3_STATE_NEW;

out_brelse:
	brelse (bh);
	ext3_std_error(inode->i_sb, err);
	return err;
}

/*
 * ext3_write_inode()
 *
 * We are called from a few places:
 *
 * - Within generic_file_write() for O_SYNC files.
 *   Here, there will be no transaction running. We wait for any running
 *   trasnaction to commit.
 *
 * - Within sys_sync(), kupdate and such.
 *   We wait on commit, if tol to.
 *
 * - Within prune_icache() (PF_MEMALLOC == true)
 *   Here we simply return.  We can't afford to block kswapd on the
 *   journal commit.
 *
 * In all cases it is actually safe for us to return without doing anything,
 * because the inode has been copied into a raw inode buffer in
 * ext3_mark_inode_dirty().  This is a correctness thing for O_SYNC and for
 * knfsd.
 *
 * Note that we are absolutely dependent upon all inode dirtiers doing the
 * right thing: they *must* call mark_inode_dirty() after dirtying info in
 * which we are interested.
 *
 * It would be a bug for them to not do this.  The code:
 *
 *	mark_inode_dirty(inode)
 *	stuff();
 *	inode->i_size = expr;
 *
 * is in error because a kswapd-driven write_inode() could occur while
 * `stuff()' is running, and the new i_size will be lost.  Plus the inode
 * will no longer be on the superblock's dirty inode list.
 */
void ext3_write_inode(struct inode *inode, int wait)
{
	if (current->flags & PF_MEMALLOC)
		return;

	if (ext3_journal_current_handle()) {
		jbd_debug(0, "called recursively, non-PF_MEMALLOC!\n");
		return;
	}

	if (!wait)
		return;

	ext3_force_commit(inode->i_sb);	
}

/*
 * ext3_setattr()
 *
 * Called from notify_change.
 *
 * We want to trap VFS attempts to truncate the file as soon as
 * possible.  In particular, we want to make sure that when the VFS
 * shrinks i_size, we put the inode on the orphan list and modify
 * i_disksize immediately, so that during the subsequent flushing of
 * dirty pages and freeing of disk blocks, we can guarantee that any
 * commit will leave the blocks being flushed in an unused state on
 * disk.  (On recovery, the inode will get truncated and the blocks will
 * be freed, so we have a strong guarantee that no future commit will
 * leave these blocks visible to the user.)  
 *
 * This is only needed for regular files.  rmdir() has its own path, and
 * we can never truncate a direcory except on final unlink (at which
 * point i_nlink is zero so recovery is easy.)
 *
 * Called with the BKL.  
 */

int ext3_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	int error, rc = 0;
	const unsigned int ia_valid = attr->ia_valid;

	error = inode_change_ok(inode, attr);
	if (error)
		return error;

	if ((ia_valid & ATTR_UID && attr->ia_uid != inode->i_uid) ||
		(ia_valid & ATTR_GID && attr->ia_gid != inode->i_gid)) {
		error = DQUOT_TRANSFER(inode, attr) ? -EDQUOT : 0;
		if (error)
			return error;
	}

	if (attr->ia_valid & ATTR_SIZE && attr->ia_size < inode->i_size) {
		handle_t *handle;

		handle = ext3_journal_start(inode, 3);
		if (IS_ERR(handle)) {
			error = PTR_ERR(handle);
			goto err_out;
		}
		
		error = ext3_orphan_add(handle, inode);
		inode->u.ext3_i.i_disksize = attr->ia_size;
		rc = ext3_mark_inode_dirty(handle, inode);
		if (!error)
			error = rc;
		ext3_journal_stop(handle, inode);
	}
	
	rc = inode_setattr(inode, attr);

	/* If inode_setattr's call to ext3_truncate failed to get a
	 * transaction handle at all, we need to clean up the in-core
	 * orphan list manually. */
	if (inode->i_nlink)
		ext3_orphan_del(NULL, inode);

err_out:
	ext3_std_error(inode->i_sb, error);
	if (!error)
		error = rc;
	return error;
}


/*
 * akpm: how many blocks doth make a writepage()?
 *
 * With N blocks per page, it may be:
 * N data blocks
 * 2 indirect block
 * 2 dindirect
 * 1 tindirect
 * N+5 bitmap blocks (from the above)
 * N+5 group descriptor summary blocks
 * 1 inode block
 * 1 superblock.
 * 2 * EXT3_SINGLEDATA_TRANS_BLOCKS for the quote files
 *
 * 3 * (N + 5) + 2 + 2 * EXT3_SINGLEDATA_TRANS_BLOCKS
 *
 * With ordered or writeback data it's the same, less the N data blocks.
 *
 * If the inode's direct blocks can hold an integral number of pages then a
 * page cannot straddle two indirect blocks, and we can only touch one indirect
 * and dindirect block, and the "5" above becomes "3".
 *
 * This still overestimates under most circumstances.  If we were to pass the
 * start and end offsets in here as well we could do block_to_path() on each
 * block and work out the exact number of indirects which are touched.  Pah.
 */

int ext3_writepage_trans_blocks(struct inode *inode)
{
	int bpp = ext3_journal_blocks_per_page(inode);
	int indirects = (EXT3_NDIR_BLOCKS % bpp) ? 5 : 3;
	int ret;
	
	if (ext3_should_journal_data(inode))
		ret = 3 * (bpp + indirects) + 2;
	else
		ret = 2 * (bpp + indirects) + 2;

#ifdef CONFIG_QUOTA
	ret += 2 * EXT3_SINGLEDATA_TRANS_BLOCKS;
#endif

	return ret;
}

int
ext3_mark_iloc_dirty(handle_t *handle, 
		     struct inode *inode,
		     struct ext3_iloc *iloc)
{
	int err = 0;

	if (handle) {
		/* the do_update_inode consumes one bh->b_count */
		atomic_inc(&iloc->bh->b_count);
		err = ext3_do_update_inode(handle, inode, iloc);
		/* ext3_do_update_inode() does journal_dirty_metadata */
		brelse(iloc->bh);
	} else {
		printk(KERN_EMERG "%s: called with no handle!\n", __FUNCTION__);
	}
	return err;
}

/* 
 * On success, We end up with an outstanding reference count against
 * iloc->bh.  This _must_ be cleaned up later. 
 */

int
ext3_reserve_inode_write(handle_t *handle, struct inode *inode, 
			 struct ext3_iloc *iloc)
{
	int err = 0;
	if (handle) {
		err = ext3_get_inode_loc(inode, iloc);
		if (!err) {
			BUFFER_TRACE(iloc->bh, "get_write_access");
			err = ext3_journal_get_write_access(handle, iloc->bh);
			if (err) {
				brelse(iloc->bh);
				iloc->bh = NULL;
			}
		}
	}
	ext3_std_error(inode->i_sb, err);
	return err;
}

/*
 * akpm: What we do here is to mark the in-core inode as clean
 * with respect to inode dirtiness (it may still be data-dirty).
 * This means that the in-core inode may be reaped by prune_icache
 * without having to perform any I/O.  This is a very good thing,
 * because *any* task may call prune_icache - even ones which
 * have a transaction open against a different journal.
 *
 * Is this cheating?  Not really.  Sure, we haven't written the
 * inode out, but prune_icache isn't a user-visible syncing function.
 * Whenever the user wants stuff synced (sys_sync, sys_msync, sys_fsync)
 * we start and wait on commits.
 *
 * Is this efficient/effective?  Well, we're being nice to the system
 * by cleaning up our inodes proactively so they can be reaped
 * without I/O.  But we are potentially leaving up to five seconds'
 * worth of inodes floating about which prune_icache wants us to
 * write out.  One way to fix that would be to get prune_icache()
 * to do a write_super() to free up some memory.  It has the desired
 * effect.
 */
int ext3_mark_inode_dirty(handle_t *handle, struct inode *inode)
{
	struct ext3_iloc iloc;
	int err;

	err = ext3_reserve_inode_write(handle, inode, &iloc);
	if (!err)
		err = ext3_mark_iloc_dirty(handle, inode, &iloc);
	return err;
}

/*
 * akpm: ext3_dirty_inode() is called from __mark_inode_dirty()
 *
 * We're really interested in the case where a file is being extended.
 * i_size has been changed by generic_commit_write() and we thus need
 * to include the updated inode in the current transaction.
 *
 * Also, DQUOT_ALLOC_SPACE() will always dirty the inode when blocks
 * are allocated to the file.
 *
 * If the inode is marked synchronous, we don't honour that here - doing
 * so would cause a commit on atime updates, which we don't bother doing.
 * We handle synchronous inodes at the highest possible level.
 */
void ext3_dirty_inode(struct inode *inode)
{
	handle_t *current_handle = ext3_journal_current_handle();
	handle_t *handle;

	lock_kernel();
	handle = ext3_journal_start(inode, 2);
	if (IS_ERR(handle))
		goto out;
	if (current_handle &&
		current_handle->h_transaction != handle->h_transaction) {
		/* This task has a transaction open against a different fs */
		printk(KERN_EMERG "%s: transactions do not match!\n",
			__FUNCTION__);
	} else {
		jbd_debug(5, "marking dirty.  outer handle=%p\n",
				current_handle);
		ext3_mark_inode_dirty(handle, inode);
	}
	ext3_journal_stop(handle, inode);
out:
	unlock_kernel();
}

#ifdef AKPM
/* 
 * Bind an inode's backing buffer_head into this transaction, to prevent
 * it from being flushed to disk early.  Unlike
 * ext3_reserve_inode_write, this leaves behind no bh reference and
 * returns no iloc structure, so the caller needs to repeat the iloc
 * lookup to mark the inode dirty later.
 */
static inline int
ext3_pin_inode(handle_t *handle, struct inode *inode)
{
	struct ext3_iloc iloc;
	
	int err = 0;
	if (handle) {
		err = ext3_get_inode_loc(inode, &iloc);
		if (!err) {
			BUFFER_TRACE(iloc.bh, "get_write_access");
			err = journal_get_write_access(handle, iloc.bh);
			if (!err)
				err = ext3_journal_dirty_metadata(handle, 
								  iloc.bh);
			brelse(iloc.bh);
		}
	}
	ext3_std_error(inode->i_sb, err);
	return err;
}
#endif

int ext3_change_inode_journal_flag(struct inode *inode, int val)
{
	journal_t *journal;
	handle_t *handle;
	int err;

	/*
	 * We have to be very careful here: changing a data block's
	 * journaling status dynamically is dangerous.  If we write a
	 * data block to the journal, change the status and then delete
	 * that block, we risk forgetting to revoke the old log record
	 * from the journal and so a subsequent replay can corrupt data.
	 * So, first we make sure that the journal is empty and that
	 * nobody is changing anything.
	 */

	journal = EXT3_JOURNAL(inode);
	if (is_journal_aborted(journal) || IS_RDONLY(inode))
		return -EROFS;
	
	journal_lock_updates(journal);
	journal_flush(journal);

	/*
	 * OK, there are no updates running now, and all cached data is
	 * synced to disk.  We are now in a completely consistent state
	 * which doesn't have anything in the journal, and we know that
	 * no filesystem updates are running, so it is safe to modify
	 * the inode's in-core data-journaling state flag now.
	 */

	if (val)
		inode->u.ext3_i.i_flags |= EXT3_JOURNAL_DATA_FL;
	else
		inode->u.ext3_i.i_flags &= ~EXT3_JOURNAL_DATA_FL;

	journal_unlock_updates(journal);

	/* Finally we can mark the inode as dirty. */

	handle = ext3_journal_start(inode, 1);
	if (IS_ERR(handle))
		return PTR_ERR(handle);

	err = ext3_mark_inode_dirty(handle, inode);
	handle->h_sync = 1;
	ext3_journal_stop(handle, inode);
	ext3_std_error(inode->i_sb, err);
	
	return err;
}


/*
 * ext3_aops_journal_start().
 *
 * <This function died, but the comment lives on>
 *
 * We need to take the inode semaphore *outside* the
 * journal_start/journal_stop.  Otherwise, a different task could do a
 * wait_for_commit() while holding ->i_sem, which deadlocks.  The rule
 * is: transaction open/closes are considered to be a locking operation
 * and they nest *inside* ->i_sem.
 * ----------------------------------------------------------------------------
 * Possible problem:
 *	ext3_file_write()
 *	-> generic_file_write()
 *	   -> __alloc_pages()
 *	      -> page_launder()
 *		 -> ext3_writepage()
 *
 * And the writepage can be on a different fs while we have a
 * transaction open against this one!  Bad.
 *
 * I tried making the task PF_MEMALLOC here, but that simply results in
 * 0-order allocation failures passed back to generic_file_write().
 * Instead, we rely on the reentrancy protection in ext3_writepage().
 * ----------------------------------------------------------------------------
 * When we do the journal_start() here we don't really need to reserve
 * any blocks - we won't need any until we hit ext3_prepare_write(),
 * which does all the needed journal extending.  However!  There is a
 * problem with quotas:
 *
 * Thread 1:
 * sys_sync
 * ->sync_dquots
 *   ->commit_dquot
 *     ->lock_dquot
 *     ->write_dquot
 *       ->ext3_file_write
 *         ->journal_start
 *         ->ext3_prepare_write
 *           ->journal_extend
 *           ->journal_start
 * Thread 2:
 * ext3_create		(for example)
 * ->ext3_new_inode
 *   ->dquot_initialize
 *     ->lock_dquot
 *
 * Deadlock.  Thread 1's journal_start blocks because thread 2 has a
 * transaction open.  Thread 2's transaction will never close because
 * thread 2 is stuck waiting for the dquot lock.
 *
 * So.  We must ensure that thread 1 *never* needs to extend the journal
 * for quota writes.  We do that by reserving enough journal blocks
 * here, in ext3_aops_journal_start() to ensure that the forthcoming "see if we
 * need to extend" test in ext3_prepare_write() succeeds.  
 */
