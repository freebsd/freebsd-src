/*
 *  linux/fs/ext3/balloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  Enhanced block allocation by Stephen Tweedie (sct@redhat.com), 1993
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/locks.h>
#include <linux/quotaops.h>

/*
 * balloc.c contains the blocks allocation and deallocation routines
 */

/*
 * The free blocks are managed by bitmaps.  A file system contains several
 * blocks groups.  Each group contains 1 bitmap block for blocks, 1 bitmap
 * block for inodes, N blocks for the inode table and data blocks.
 *
 * The file system contains group descriptors which are located after the
 * super block.  Each descriptor contains the number of the bitmap block and
 * the free blocks count in the block.  The descriptors are loaded in memory
 * when a file system is mounted (see ext3_read_super).
 */


#define in_range(b, first, len)	((b) >= (first) && (b) <= (first) + (len) - 1)

struct ext3_group_desc * ext3_get_group_desc(struct super_block * sb,
					     unsigned int block_group,
					     struct buffer_head ** bh)
{
	unsigned long group_desc;
	unsigned long desc;
	struct ext3_group_desc * gdp;

	if (block_group >= sb->u.ext3_sb.s_groups_count) {
		ext3_error (sb, "ext3_get_group_desc",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sb->u.ext3_sb.s_groups_count);

		return NULL;
	}
	
	group_desc = block_group / EXT3_DESC_PER_BLOCK(sb);
	desc = block_group % EXT3_DESC_PER_BLOCK(sb);
	if (!sb->u.ext3_sb.s_group_desc[group_desc]) {
		ext3_error (sb, "ext3_get_group_desc",
			    "Group descriptor not loaded - "
			    "block_group = %d, group_desc = %lu, desc = %lu",
			     block_group, group_desc, desc);
		return NULL;
	}
	
	gdp = (struct ext3_group_desc *) 
	      sb->u.ext3_sb.s_group_desc[group_desc]->b_data;
	if (bh)
		*bh = sb->u.ext3_sb.s_group_desc[group_desc];
	return gdp + desc;
}

/*
 * Read the bitmap for a given block_group, reading into the specified 
 * slot in the superblock's bitmap cache.
 *
 * Return >=0 on success or a -ve error code.
 */

static int read_block_bitmap (struct super_block * sb,
			       unsigned int block_group,
			       unsigned long bitmap_nr)
{
	struct ext3_group_desc * gdp;
	struct buffer_head * bh = NULL;
	int retval = -EIO;
	
	gdp = ext3_get_group_desc (sb, block_group, NULL);
	if (!gdp)
		goto error_out;
	retval = 0;
	bh = sb_bread(sb, le32_to_cpu(gdp->bg_block_bitmap));
	if (!bh) {
		ext3_error (sb, "read_block_bitmap",
			    "Cannot read block bitmap - "
			    "block_group = %d, block_bitmap = %lu",
			    block_group, (unsigned long) gdp->bg_block_bitmap);
		retval = -EIO;
	}
	/*
	 * On IO error, just leave a zero in the superblock's block pointer for
	 * this group.  The IO will be retried next time.
	 */
error_out:
	sb->u.ext3_sb.s_block_bitmap_number[bitmap_nr] = block_group;
	sb->u.ext3_sb.s_block_bitmap[bitmap_nr] = bh;
	return retval;
}

/*
 * load_block_bitmap loads the block bitmap for a blocks group
 *
 * It maintains a cache for the last bitmaps loaded.  This cache is managed
 * with a LRU algorithm.
 *
 * Notes:
 * 1/ There is one cache per mounted file system.
 * 2/ If the file system contains less than EXT3_MAX_GROUP_LOADED groups,
 *    this function reads the bitmap without maintaining a LRU cache.
 * 
 * Return the slot used to store the bitmap, or a -ve error code.
 */
static int __load_block_bitmap (struct super_block * sb,
			        unsigned int block_group)
{
	int i, j, retval = 0;
	unsigned long block_bitmap_number;
	struct buffer_head * block_bitmap;

	if (block_group >= sb->u.ext3_sb.s_groups_count)
		ext3_panic (sb, "load_block_bitmap",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sb->u.ext3_sb.s_groups_count);

	if (sb->u.ext3_sb.s_groups_count <= EXT3_MAX_GROUP_LOADED) {
		if (sb->u.ext3_sb.s_block_bitmap[block_group]) {
			if (sb->u.ext3_sb.s_block_bitmap_number[block_group] ==
			    block_group)
				return block_group;
			ext3_error (sb, "__load_block_bitmap",
				    "block_group != block_bitmap_number");
		}
		retval = read_block_bitmap (sb, block_group, block_group);
		if (retval < 0)
			return retval;
		return block_group;
	}

	for (i = 0; i < sb->u.ext3_sb.s_loaded_block_bitmaps &&
		    sb->u.ext3_sb.s_block_bitmap_number[i] != block_group; i++)
		;
	if (i < sb->u.ext3_sb.s_loaded_block_bitmaps &&
  	    sb->u.ext3_sb.s_block_bitmap_number[i] == block_group) {
		block_bitmap_number = sb->u.ext3_sb.s_block_bitmap_number[i];
		block_bitmap = sb->u.ext3_sb.s_block_bitmap[i];
		for (j = i; j > 0; j--) {
			sb->u.ext3_sb.s_block_bitmap_number[j] =
				sb->u.ext3_sb.s_block_bitmap_number[j - 1];
			sb->u.ext3_sb.s_block_bitmap[j] =
				sb->u.ext3_sb.s_block_bitmap[j - 1];
		}
		sb->u.ext3_sb.s_block_bitmap_number[0] = block_bitmap_number;
		sb->u.ext3_sb.s_block_bitmap[0] = block_bitmap;

		/*
		 * There's still one special case here --- if block_bitmap == 0
		 * then our last attempt to read the bitmap failed and we have
		 * just ended up caching that failure.  Try again to read it.
		 */
		if (!block_bitmap)
			retval = read_block_bitmap (sb, block_group, 0);
	} else {
		if (sb->u.ext3_sb.s_loaded_block_bitmaps<EXT3_MAX_GROUP_LOADED)
			sb->u.ext3_sb.s_loaded_block_bitmaps++;
		else
			brelse (sb->u.ext3_sb.s_block_bitmap
					[EXT3_MAX_GROUP_LOADED - 1]);
		for (j = sb->u.ext3_sb.s_loaded_block_bitmaps - 1;
					j > 0;  j--) {
			sb->u.ext3_sb.s_block_bitmap_number[j] =
				sb->u.ext3_sb.s_block_bitmap_number[j - 1];
			sb->u.ext3_sb.s_block_bitmap[j] =
				sb->u.ext3_sb.s_block_bitmap[j - 1];
		}
		retval = read_block_bitmap (sb, block_group, 0);
	}
	return retval;
}

/*
 * Load the block bitmap for a given block group.  First of all do a couple
 * of fast lookups for common cases and then pass the request onto the guts
 * of the bitmap loader.
 *
 * Return the slot number of the group in the superblock bitmap cache's on
 * success, or a -ve error code.
 *
 * There is still one inconsistency here --- if the number of groups in this
 * filesystems is <= EXT3_MAX_GROUP_LOADED, then we have no way of 
 * differentiating between a group for which we have never performed a bitmap
 * IO request, and a group for which the last bitmap read request failed.
 */
static inline int load_block_bitmap (struct super_block * sb,
				     unsigned int block_group)
{
	int slot;
	
	/*
	 * Do the lookup for the slot.  First of all, check if we're asking
	 * for the same slot as last time, and did we succeed that last time?
	 */
	if (sb->u.ext3_sb.s_loaded_block_bitmaps > 0 &&
	    sb->u.ext3_sb.s_block_bitmap_number[0] == block_group &&
	    sb->u.ext3_sb.s_block_bitmap[0]) {
		return 0;
	}
	/*
	 * Or can we do a fast lookup based on a loaded group on a filesystem
	 * small enough to be mapped directly into the superblock?
	 */
	else if (sb->u.ext3_sb.s_groups_count <= EXT3_MAX_GROUP_LOADED && 
		 sb->u.ext3_sb.s_block_bitmap_number[block_group]==block_group
			&& sb->u.ext3_sb.s_block_bitmap[block_group]) {
		slot = block_group;
	}
	/*
	 * If not, then do a full lookup for this block group.
	 */
	else {
		slot = __load_block_bitmap (sb, block_group);
	}

	/*
	 * <0 means we just got an error
	 */
	if (slot < 0)
		return slot;
	
	/*
	 * If it's a valid slot, we may still have cached a previous IO error,
	 * in which case the bh in the superblock cache will be zero.
	 */
	if (!sb->u.ext3_sb.s_block_bitmap[slot])
		return -EIO;
	
	/*
	 * Must have been read in OK to get this far.
	 */
	return slot;
}

/* Free given blocks, update quota and i_blocks field */
void ext3_free_blocks (handle_t *handle, struct inode * inode,
			unsigned long block, unsigned long count)
{
	struct buffer_head *bitmap_bh;
	struct buffer_head *gd_bh;
	unsigned long block_group;
	unsigned long bit;
	unsigned long i;
	int bitmap_nr;
	unsigned long overflow;
	struct super_block * sb;
	struct ext3_group_desc * gdp;
	struct ext3_super_block * es;
	int err = 0, ret;
	int dquot_freed_blocks = 0;

	sb = inode->i_sb;
	if (!sb) {
		printk ("ext3_free_blocks: nonexistent device");
		return;
	}
	lock_super (sb);
	es = sb->u.ext3_sb.s_es;
	if (block < le32_to_cpu(es->s_first_data_block) ||
	    block + count < block ||
	    (block + count) > le32_to_cpu(es->s_blocks_count)) {
		ext3_error (sb, "ext3_free_blocks",
			    "Freeing blocks not in datazone - "
			    "block = %lu, count = %lu", block, count);
		goto error_return;
	}

	ext3_debug ("freeing block %lu\n", block);

do_more:
	overflow = 0;
	block_group = (block - le32_to_cpu(es->s_first_data_block)) /
		      EXT3_BLOCKS_PER_GROUP(sb);
	bit = (block - le32_to_cpu(es->s_first_data_block)) %
		      EXT3_BLOCKS_PER_GROUP(sb);
	/*
	 * Check to see if we are freeing blocks across a group
	 * boundary.
	 */
	if (bit + count > EXT3_BLOCKS_PER_GROUP(sb)) {
		overflow = bit + count - EXT3_BLOCKS_PER_GROUP(sb);
		count -= overflow;
	}
	bitmap_nr = load_block_bitmap (sb, block_group);
	if (bitmap_nr < 0)
		goto error_return;
	
	bitmap_bh = sb->u.ext3_sb.s_block_bitmap[bitmap_nr];
	gdp = ext3_get_group_desc (sb, block_group, &gd_bh);
	if (!gdp)
		goto error_return;

	/*
	 * We are about to start releasing blocks in the bitmap,
	 * so we need undo access.
	 */
	/* @@@ check errors */
	BUFFER_TRACE(bitmap_bh, "getting undo access");
	err = ext3_journal_get_undo_access(handle, bitmap_bh);
	if (err)
		goto error_return;
	
	/*
	 * We are about to modify some metadata.  Call the journal APIs
	 * to unshare ->b_data if a currently-committing transaction is
	 * using it
	 */
	BUFFER_TRACE(gd_bh, "get_write_access");
	err = ext3_journal_get_write_access(handle, gd_bh);	
	if (err)
		goto error_return;

	BUFFER_TRACE(sb->u.ext3_sb.s_sbh, "get_write_access");
	err = ext3_journal_get_write_access(handle, sb->u.ext3_sb.s_sbh);
	if (err)
		goto error_return;

	for (i = 0; i < count; i++, block++) {
		if (block == le32_to_cpu(gdp->bg_block_bitmap) ||
		    block == le32_to_cpu(gdp->bg_inode_bitmap) ||
		    in_range(block, le32_to_cpu(gdp->bg_inode_table),
			     EXT3_SB(sb)->s_itb_per_group)) {
			ext3_error(sb, __FUNCTION__,
				   "Freeing block in system zone - block = %lu",
				   block);
			continue;
		}

		/*
		 * An HJ special.  This is expensive...
		 */
#ifdef CONFIG_JBD_DEBUG
		{
			struct buffer_head *debug_bh;
			debug_bh = sb_get_hash_table(sb, block);
			if (debug_bh) {
				BUFFER_TRACE(debug_bh, "Deleted!");
				if (!bh2jh(bitmap_bh)->b_committed_data)
					BUFFER_TRACE(debug_bh,
						"No commited data in bitmap");
				BUFFER_TRACE2(debug_bh, bitmap_bh, "bitmap");
				__brelse(debug_bh);
			}
		}
#endif
		BUFFER_TRACE(bitmap_bh, "clear bit");
		if (!ext3_clear_bit (bit + i, bitmap_bh->b_data)) {
			ext3_error(sb, __FUNCTION__,
				   "bit already cleared for block %lu", block);
			BUFFER_TRACE(bitmap_bh, "bit already cleared");
		} else {
			dquot_freed_blocks++;
			gdp->bg_free_blocks_count =
			  cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count)+1);
			es->s_free_blocks_count =
			  cpu_to_le32(le32_to_cpu(es->s_free_blocks_count)+1);
		}
		/* @@@ This prevents newly-allocated data from being
		 * freed and then reallocated within the same
		 * transaction. 
		 * 
		 * Ideally we would want to allow that to happen, but to
		 * do so requires making journal_forget() capable of
		 * revoking the queued write of a data block, which
		 * implies blocking on the journal lock.  *forget()
		 * cannot block due to truncate races.
		 *
		 * Eventually we can fix this by making journal_forget()
		 * return a status indicating whether or not it was able
		 * to revoke the buffer.  On successful revoke, it is
		 * safe not to set the allocation bit in the committed
		 * bitmap, because we know that there is no outstanding
		 * activity on the buffer any more and so it is safe to
		 * reallocate it.  
		 */
		BUFFER_TRACE(bitmap_bh, "clear in b_committed_data");
		J_ASSERT_BH(bitmap_bh,
				bh2jh(bitmap_bh)->b_committed_data != NULL);
		ext3_set_bit(bit + i, bh2jh(bitmap_bh)->b_committed_data);
	}

	/* We dirtied the bitmap block */
	BUFFER_TRACE(bitmap_bh, "dirtied bitmap block");
	err = ext3_journal_dirty_metadata(handle, bitmap_bh);

	/* And the group descriptor block */
	BUFFER_TRACE(gd_bh, "dirtied group descriptor block");
	ret = ext3_journal_dirty_metadata(handle, gd_bh);
	if (!err) err = ret;

	/* And the superblock */
	BUFFER_TRACE(sb->u.ext3_sb.s_sbh, "dirtied superblock");
	ret = ext3_journal_dirty_metadata(handle, sb->u.ext3_sb.s_sbh);
	if (!err) err = ret;

	if (overflow && !err) {
		count = overflow;
		goto do_more;
	}
	sb->s_dirt = 1;
error_return:
	ext3_std_error(sb, err);
	unlock_super(sb);
	if (dquot_freed_blocks)
		DQUOT_FREE_BLOCK(inode, dquot_freed_blocks);
	return;
}

/* For ext3 allocations, we must not reuse any blocks which are
 * allocated in the bitmap buffer's "last committed data" copy.  This
 * prevents deletes from freeing up the page for reuse until we have
 * committed the delete transaction.
 *
 * If we didn't do this, then deleting something and reallocating it as
 * data would allow the old block to be overwritten before the
 * transaction committed (because we force data to disk before commit).
 * This would lead to corruption if we crashed between overwriting the
 * data and committing the delete. 
 *
 * @@@ We may want to make this allocation behaviour conditional on
 * data-writes at some point, and disable it for metadata allocations or
 * sync-data inodes.
 */
static int ext3_test_allocatable(int nr, struct buffer_head *bh)
{
	if (ext3_test_bit(nr, bh->b_data))
		return 0;
	if (!buffer_jbd(bh) || !bh2jh(bh)->b_committed_data)
		return 1;
	return !ext3_test_bit(nr, bh2jh(bh)->b_committed_data);
}

/*
 * Find an allocatable block in a bitmap.  We honour both the bitmap and
 * its last-committed copy (if that exists), and perform the "most
 * appropriate allocation" algorithm of looking for a free block near
 * the initial goal; then for a free byte somewhere in the bitmap; then
 * for any free bit in the bitmap.
 */
static int find_next_usable_block(int start,
			struct buffer_head *bh, int maxblocks)
{
	int here, next;
	char *p, *r;
	
	if (start > 0) {
		/*
		 * The goal was occupied; search forward for a free 
		 * block within the next XX blocks.
		 *
		 * end_goal is more or less random, but it has to be
		 * less than EXT3_BLOCKS_PER_GROUP. Aligning up to the
		 * next 64-bit boundary is simple..
		 */
		int end_goal = (start + 63) & ~63;
		here = ext3_find_next_zero_bit(bh->b_data, end_goal, start);
		if (here < end_goal && ext3_test_allocatable(here, bh))
			return here;
		
		ext3_debug ("Bit not found near goal\n");
		
	}
	
	here = start;
	if (here < 0)
		here = 0;
	
	/*
	 * There has been no free block found in the near vicinity of
	 * the goal: do a search forward through the block groups,
	 * searching in each group first for an entire free byte in the
	 * bitmap and then for any free bit.
	 * 
	 * Search first in the remainder of the current group 
	 */
	p = ((char *) bh->b_data) + (here >> 3);
	r = memscan(p, 0, (maxblocks - here + 7) >> 3);
	next = (r - ((char *) bh->b_data)) << 3;
	
	if (next < maxblocks && ext3_test_allocatable(next, bh))
		return next;
	
	/* The bitmap search --- search forward alternately
	 * through the actual bitmap and the last-committed copy
	 * until we find a bit free in both. */

	while (here < maxblocks) {
		next  = ext3_find_next_zero_bit ((unsigned long *) bh->b_data, 
						 maxblocks, here);
		if (next >= maxblocks)
			return -1;
		if (ext3_test_allocatable(next, bh))
			return next;

		J_ASSERT_BH(bh, bh2jh(bh)->b_committed_data);
		here = ext3_find_next_zero_bit
			((unsigned long *) bh2jh(bh)->b_committed_data, 
			 maxblocks, next);
	}
	return -1;
}

/*
 * ext3_new_block uses a goal block to assist allocation.  If the goal is
 * free, or there is a free block within 32 blocks of the goal, that block
 * is allocated.  Otherwise a forward search is made for a free block; within 
 * each block group the search first looks for an entire free byte in the block
 * bitmap, and then for any free bit if that fails.
 * This function also updates quota and i_blocks field.
 */
int ext3_new_block (handle_t *handle, struct inode * inode,
		unsigned long goal, u32 * prealloc_count,
		u32 * prealloc_block, int * errp)
{
	struct buffer_head * bh, *bhtmp;
	struct buffer_head * bh2;
#if 0
	char * p, * r;
#endif
	int i, j, k, tmp, alloctmp;
	int bitmap_nr;
	int fatal = 0, err;
	int performed_allocation = 0;
	struct super_block * sb;
	struct ext3_group_desc * gdp;
	struct ext3_super_block * es;
#ifdef EXT3FS_DEBUG
	static int goal_hits = 0, goal_attempts = 0;
#endif
	*errp = -ENOSPC;
	sb = inode->i_sb;
	if (!sb) {
		printk ("ext3_new_block: nonexistent device");
		return 0;
	}

	/*
	 * Check quota for allocation of this block.
	 */
	if (DQUOT_ALLOC_BLOCK(inode, 1)) {
		*errp = -EDQUOT;
		return 0;
	}

	lock_super (sb);
	es = sb->u.ext3_sb.s_es;
	if (le32_to_cpu(es->s_free_blocks_count) <=
			le32_to_cpu(es->s_r_blocks_count) &&
	    ((sb->u.ext3_sb.s_resuid != current->fsuid) &&
	     (sb->u.ext3_sb.s_resgid == 0 ||
	      !in_group_p (sb->u.ext3_sb.s_resgid)) && 
	     !capable(CAP_SYS_RESOURCE)))
		goto out;

	ext3_debug ("goal=%lu.\n", goal);

repeat:
	/*
	 * First, test whether the goal block is free.
	 */
	if (goal < le32_to_cpu(es->s_first_data_block) ||
	    goal >= le32_to_cpu(es->s_blocks_count))
		goal = le32_to_cpu(es->s_first_data_block);
	i = (goal - le32_to_cpu(es->s_first_data_block)) /
			EXT3_BLOCKS_PER_GROUP(sb);
	gdp = ext3_get_group_desc (sb, i, &bh2);
	if (!gdp)
		goto io_error;

	if (le16_to_cpu(gdp->bg_free_blocks_count) > 0) {
		j = ((goal - le32_to_cpu(es->s_first_data_block)) %
				EXT3_BLOCKS_PER_GROUP(sb));
#ifdef EXT3FS_DEBUG
		if (j)
			goal_attempts++;
#endif
		bitmap_nr = load_block_bitmap (sb, i);
		if (bitmap_nr < 0)
			goto io_error;
		
		bh = sb->u.ext3_sb.s_block_bitmap[bitmap_nr];

		ext3_debug ("goal is at %d:%d.\n", i, j);

		if (ext3_test_allocatable(j, bh)) {
#ifdef EXT3FS_DEBUG
			goal_hits++;
			ext3_debug ("goal bit allocated.\n");
#endif
			goto got_block;
		}

		j = find_next_usable_block(j, bh, EXT3_BLOCKS_PER_GROUP(sb));
		if (j >= 0)
			goto search_back;
	}

	ext3_debug ("Bit not found in block group %d.\n", i);

	/*
	 * Now search the rest of the groups.  We assume that 
	 * i and gdp correctly point to the last group visited.
	 */
	for (k = 0; k < sb->u.ext3_sb.s_groups_count; k++) {
		i++;
		if (i >= sb->u.ext3_sb.s_groups_count)
			i = 0;
		gdp = ext3_get_group_desc (sb, i, &bh2);
		if (!gdp) {
			*errp = -EIO;
			goto out;
		}
		if (le16_to_cpu(gdp->bg_free_blocks_count) > 0) {
			bitmap_nr = load_block_bitmap (sb, i);
			if (bitmap_nr < 0)
				goto io_error;
	
			bh = sb->u.ext3_sb.s_block_bitmap[bitmap_nr];
			j = find_next_usable_block(-1, bh, 
						   EXT3_BLOCKS_PER_GROUP(sb));
			if (j >= 0) 
				goto search_back;
		}
	}

	/* No space left on the device */
	goto out;

search_back:
	/* 
	 * We have succeeded in finding a free byte in the block
	 * bitmap.  Now search backwards up to 7 bits to find the
	 * start of this group of free blocks.
	 */
	for (	k = 0;
		k < 7 && j > 0 && ext3_test_allocatable(j - 1, bh);
		k++, j--)
		;
	
got_block:

	ext3_debug ("using block group %d(%d)\n", i, gdp->bg_free_blocks_count);

	/* Make sure we use undo access for the bitmap, because it is
           critical that we do the frozen_data COW on bitmap buffers in
           all cases even if the buffer is in BJ_Forget state in the
           committing transaction.  */
	BUFFER_TRACE(bh, "get undo access for marking new block");
	fatal = ext3_journal_get_undo_access(handle, bh);
	if (fatal) goto out;
	
	BUFFER_TRACE(bh2, "get_write_access");
	fatal = ext3_journal_get_write_access(handle, bh2);
	if (fatal) goto out;

	BUFFER_TRACE(sb->u.ext3_sb.s_sbh, "get_write_access");
	fatal = ext3_journal_get_write_access(handle, sb->u.ext3_sb.s_sbh);
	if (fatal) goto out;

	tmp = j + i * EXT3_BLOCKS_PER_GROUP(sb)
				+ le32_to_cpu(es->s_first_data_block);

	if (tmp == le32_to_cpu(gdp->bg_block_bitmap) ||
	    tmp == le32_to_cpu(gdp->bg_inode_bitmap) ||
	    in_range (tmp, le32_to_cpu(gdp->bg_inode_table),
		      EXT3_SB(sb)->s_itb_per_group)) {
		ext3_error(sb, __FUNCTION__,
			   "Allocating block in system zone - block = %u", tmp);

		/* Note: This will potentially use up one of the handle's
		 * buffer credits.  Normally we have way too many credits,
		 * so that is OK.  In _very_ rare cases it might not be OK.
		 * We will trigger an assertion if we run out of credits,
		 * and we will have to do a full fsck of the filesystem -
		 * better than randomly corrupting filesystem metadata.
		 */
		ext3_set_bit(j, bh->b_data);
		goto repeat;
	}


	/* The superblock lock should guard against anybody else beating
	 * us to this point! */
	J_ASSERT_BH(bh, !ext3_test_bit(j, bh->b_data));
	BUFFER_TRACE(bh, "setting bitmap bit");
	ext3_set_bit(j, bh->b_data);
	performed_allocation = 1;

#ifdef CONFIG_JBD_DEBUG
	{
		struct buffer_head *debug_bh;

		/* Record bitmap buffer state in the newly allocated block */
		debug_bh = sb_get_hash_table(sb, tmp);
		if (debug_bh) {
			BUFFER_TRACE(debug_bh, "state when allocated");
			BUFFER_TRACE2(debug_bh, bh, "bitmap state");
			brelse(debug_bh);
		}
	}
#endif
	if (buffer_jbd(bh) && bh2jh(bh)->b_committed_data)
		J_ASSERT_BH(bh, !ext3_test_bit(j, bh2jh(bh)->b_committed_data));
	bhtmp = bh;
	alloctmp = j;

	ext3_debug ("found bit %d\n", j);

	/*
	 * Do block preallocation now if required.
	 */
#ifdef EXT3_PREALLOCATE
	/*
	 * akpm: this is not enabled for ext3.  Need to use
	 * ext3_test_allocatable()
	 */
	/* Writer: ->i_prealloc* */
	if (prealloc_count && !*prealloc_count) {
		int	prealloc_goal;
		unsigned long next_block = tmp + 1;

		prealloc_goal = es->s_prealloc_blocks ?
			es->s_prealloc_blocks : EXT3_DEFAULT_PREALLOC_BLOCKS;

		*prealloc_block = next_block;
		/* Writer: end */
		for (k = 1;
		     k < prealloc_goal && (j + k) < EXT3_BLOCKS_PER_GROUP(sb);
		     k++, next_block++) {
			if (DQUOT_PREALLOC_BLOCK(inode, 1))
				break;
			/* Writer: ->i_prealloc* */
			if (*prealloc_block + *prealloc_count != next_block ||
			    ext3_set_bit (j + k, bh->b_data)) {
				/* Writer: end */
				DQUOT_FREE_BLOCK(inode, 1);
 				break;
			}
			(*prealloc_count)++;
			/* Writer: end */
		}	
		/*
		 * As soon as we go for per-group spinlocks we'll need these
		 * done inside the loop above.
		 */
		gdp->bg_free_blocks_count =
			cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count) -
			       (k - 1));
		es->s_free_blocks_count =
			cpu_to_le32(le32_to_cpu(es->s_free_blocks_count) -
			       (k - 1));
		ext3_debug ("Preallocated a further %lu bits.\n",
			       (k - 1));
	}
#endif

	j = tmp;

	BUFFER_TRACE(bh, "journal_dirty_metadata for bitmap block");
	err = ext3_journal_dirty_metadata(handle, bh);
	if (!fatal) fatal = err;
	
	if (j >= le32_to_cpu(es->s_blocks_count)) {
		ext3_error (sb, "ext3_new_block",
			    "block(%d) >= blocks count(%d) - "
			    "block_group = %d, es == %p ",j,
			le32_to_cpu(es->s_blocks_count), i, es);
		goto out;
	}

	/*
	 * It is up to the caller to add the new buffer to a journal
	 * list of some description.  We don't know in advance whether
	 * the caller wants to use it as metadata or data.
	 */

	ext3_debug ("allocating block %d. "
		    "Goal hits %d of %d.\n", j, goal_hits, goal_attempts);

	gdp->bg_free_blocks_count =
			cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count) - 1);
	es->s_free_blocks_count =
			cpu_to_le32(le32_to_cpu(es->s_free_blocks_count) - 1);

	BUFFER_TRACE(bh2, "journal_dirty_metadata for group descriptor");
	err = ext3_journal_dirty_metadata(handle, bh2);
	if (!fatal) fatal = err;
	
	BUFFER_TRACE(bh, "journal_dirty_metadata for superblock");
	err = ext3_journal_dirty_metadata(handle, sb->u.ext3_sb.s_sbh);
	if (!fatal) fatal = err;

	sb->s_dirt = 1;
	if (fatal)
		goto out;

	unlock_super (sb);
	*errp = 0;
	return j;
	
io_error:
	*errp = -EIO;
out:
	if (fatal) {
		*errp = fatal;
		ext3_std_error(sb, fatal);
	}
	unlock_super (sb);
	/*
	 * Undo the block allocation
	 */
	if (!performed_allocation)
		DQUOT_FREE_BLOCK(inode, 1);
	return 0;
	
}

unsigned long ext3_count_free_blocks (struct super_block * sb)
{
#ifdef EXT3FS_DEBUG
	struct ext3_super_block * es;
	unsigned long desc_count, bitmap_count, x;
	int bitmap_nr;
	struct ext3_group_desc * gdp;
	int i;
	
	lock_super (sb);
	es = sb->u.ext3_sb.s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	for (i = 0; i < sb->u.ext3_sb.s_groups_count; i++) {
		gdp = ext3_get_group_desc (sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
		bitmap_nr = load_block_bitmap (sb, i);
		if (bitmap_nr < 0)
			continue;
		
		x = ext3_count_free (sb->u.ext3_sb.s_block_bitmap[bitmap_nr],
				     sb->s_blocksize);
		printk ("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(gdp->bg_free_blocks_count), x);
		bitmap_count += x;
	}
	printk("ext3_count_free_blocks: stored = %lu, computed = %lu, %lu\n",
	       le32_to_cpu(es->s_free_blocks_count), desc_count, bitmap_count);
	unlock_super (sb);
	return bitmap_count;
#else
	return le32_to_cpu(sb->u.ext3_sb.s_es->s_free_blocks_count);
#endif
}

static inline int block_in_use (unsigned long block,
				struct super_block * sb,
				unsigned char * map)
{
	return ext3_test_bit ((block -
		le32_to_cpu(sb->u.ext3_sb.s_es->s_first_data_block)) %
			 EXT3_BLOCKS_PER_GROUP(sb), map);
}

static inline int test_root(int a, int b)
{
	if (a == 0)
		return 1;
	while (1) {
		if (a == 1)
			return 1;
		if (a % b)
			return 0;
		a = a / b;
	}
}

int ext3_group_sparse(int group)
{
	return (test_root(group, 3) || test_root(group, 5) ||
		test_root(group, 7));
}

/**
 *	ext3_bg_has_super - number of blocks used by the superblock in group
 *	@sb: superblock for filesystem
 *	@group: group number to check
 *
 *	Return the number of blocks used by the superblock (primary or backup)
 *	in this group.  Currently this will be only 0 or 1.
 */
int ext3_bg_has_super(struct super_block *sb, int group)
{
	if (EXT3_HAS_RO_COMPAT_FEATURE(sb,EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER)&&
	    !ext3_group_sparse(group))
		return 0;
	return 1;
}

/**
 *	ext3_bg_num_gdb - number of blocks used by the group table in group
 *	@sb: superblock for filesystem
 *	@group: group number to check
 *
 *	Return the number of blocks used by the group descriptor table
 *	(primary or backup) in this group.  In the future there may be a
 *	different number of descriptor blocks in each group.
 */
unsigned long ext3_bg_num_gdb(struct super_block *sb, int group)
{
	if (EXT3_HAS_RO_COMPAT_FEATURE(sb,EXT3_FEATURE_RO_COMPAT_SPARSE_SUPER)&&
	    !ext3_group_sparse(group))
		return 0;
	return EXT3_SB(sb)->s_gdb_count;
}

#ifdef CONFIG_EXT3_CHECK
/* Called at mount-time, super-block is locked */
void ext3_check_blocks_bitmap (struct super_block * sb)
{
	struct buffer_head * bh;
	struct ext3_super_block * es;
	unsigned long desc_count, bitmap_count, x, j;
	unsigned long desc_blocks;
	int bitmap_nr;
	struct ext3_group_desc * gdp;
	int i;

	es = sb->u.ext3_sb.s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	for (i = 0; i < sb->u.ext3_sb.s_groups_count; i++) {
		gdp = ext3_get_group_desc (sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
		bitmap_nr = load_block_bitmap (sb, i);
		if (bitmap_nr < 0)
			continue;

		bh = EXT3_SB(sb)->s_block_bitmap[bitmap_nr];

		if (ext3_bg_has_super(sb, i) && !ext3_test_bit(0, bh->b_data))
			ext3_error(sb, __FUNCTION__,
				   "Superblock in group %d is marked free", i);

		desc_blocks = ext3_bg_num_gdb(sb, i);
		for (j = 0; j < desc_blocks; j++)
			if (!ext3_test_bit(j + 1, bh->b_data))
				ext3_error(sb, __FUNCTION__,
					   "Descriptor block #%ld in group "
					   "%d is marked free", j, i);

		if (!block_in_use (le32_to_cpu(gdp->bg_block_bitmap),
						sb, bh->b_data))
			ext3_error (sb, "ext3_check_blocks_bitmap",
				    "Block bitmap for group %d is marked free",
				    i);

		if (!block_in_use (le32_to_cpu(gdp->bg_inode_bitmap),
						sb, bh->b_data))
			ext3_error (sb, "ext3_check_blocks_bitmap",
				    "Inode bitmap for group %d is marked free",
				    i);

		for (j = 0; j < sb->u.ext3_sb.s_itb_per_group; j++)
			if (!block_in_use (le32_to_cpu(gdp->bg_inode_table) + j,
							sb, bh->b_data))
				ext3_error (sb, "ext3_check_blocks_bitmap",
					    "Block #%d of the inode table in "
					    "group %d is marked free", j, i);

		x = ext3_count_free (bh, sb->s_blocksize);
		if (le16_to_cpu(gdp->bg_free_blocks_count) != x)
			ext3_error (sb, "ext3_check_blocks_bitmap",
				    "Wrong free blocks count for group %d, "
				    "stored = %d, counted = %lu", i,
				    le16_to_cpu(gdp->bg_free_blocks_count), x);
		bitmap_count += x;
	}
	if (le32_to_cpu(es->s_free_blocks_count) != bitmap_count)
		ext3_error (sb, "ext3_check_blocks_bitmap",
			"Wrong free blocks count in super block, "
			"stored = %lu, counted = %lu",
			(unsigned long)le32_to_cpu(es->s_free_blocks_count),
			bitmap_count);
}
#endif
