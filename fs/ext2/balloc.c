/*
 *  linux/fs/ext2/balloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  Enhanced block allocation by Stephen Tweedie (sct@dcs.ed.ac.uk), 1993
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
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
 * when a file system is mounted (see ext2_read_super).
 */


#define in_range(b, first, len)		((b) >= (first) && (b) <= (first) + (len) - 1)

struct ext2_group_desc * ext2_get_group_desc(struct super_block * sb,
					     unsigned int block_group,
					     struct buffer_head ** bh)
{
	unsigned long group_desc;
	unsigned long desc;
	struct ext2_group_desc * gdp;

	if (block_group >= sb->u.ext2_sb.s_groups_count) {
		ext2_error (sb, "ext2_get_group_desc",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sb->u.ext2_sb.s_groups_count);

		return NULL;
	}
	
	group_desc = block_group / EXT2_DESC_PER_BLOCK(sb);
	desc = block_group % EXT2_DESC_PER_BLOCK(sb);
	if (!sb->u.ext2_sb.s_group_desc[group_desc]) {
		ext2_error (sb, "ext2_get_group_desc",
			    "Group descriptor not loaded - "
			    "block_group = %d, group_desc = %lu, desc = %lu",
			     block_group, group_desc, desc);
		return NULL;
	}
	
	gdp = (struct ext2_group_desc *) 
	      sb->u.ext2_sb.s_group_desc[group_desc]->b_data;
	if (bh)
		*bh = sb->u.ext2_sb.s_group_desc[group_desc];
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
	struct ext2_group_desc * gdp;
	struct buffer_head * bh = NULL;
	int retval = -EIO;
	
	gdp = ext2_get_group_desc (sb, block_group, NULL);
	if (!gdp)
		goto error_out;
	retval = 0;
	bh = sb_bread(sb, le32_to_cpu(gdp->bg_block_bitmap));
	if (!bh) {
		ext2_error (sb, "read_block_bitmap",
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
	sb->u.ext2_sb.s_block_bitmap_number[bitmap_nr] = block_group;
	sb->u.ext2_sb.s_block_bitmap[bitmap_nr] = bh;
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
 * 2/ If the file system contains less than EXT2_MAX_GROUP_LOADED groups,
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

	if (block_group >= sb->u.ext2_sb.s_groups_count)
		ext2_panic (sb, "load_block_bitmap",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sb->u.ext2_sb.s_groups_count);

	if (sb->u.ext2_sb.s_groups_count <= EXT2_MAX_GROUP_LOADED) {
		if (sb->u.ext2_sb.s_block_bitmap[block_group]) {
			if (sb->u.ext2_sb.s_block_bitmap_number[block_group] ==
			    block_group)
				return block_group;
			ext2_error (sb, "__load_block_bitmap",
				    "block_group != block_bitmap_number");
		}
		retval = read_block_bitmap (sb, block_group, block_group);
		if (retval < 0)
			return retval;
		return block_group;
	}

	for (i = 0; i < sb->u.ext2_sb.s_loaded_block_bitmaps &&
		    sb->u.ext2_sb.s_block_bitmap_number[i] != block_group; i++)
		;
	if (i < sb->u.ext2_sb.s_loaded_block_bitmaps &&
  	    sb->u.ext2_sb.s_block_bitmap_number[i] == block_group) {
		block_bitmap_number = sb->u.ext2_sb.s_block_bitmap_number[i];
		block_bitmap = sb->u.ext2_sb.s_block_bitmap[i];
		for (j = i; j > 0; j--) {
			sb->u.ext2_sb.s_block_bitmap_number[j] =
				sb->u.ext2_sb.s_block_bitmap_number[j - 1];
			sb->u.ext2_sb.s_block_bitmap[j] =
				sb->u.ext2_sb.s_block_bitmap[j - 1];
		}
		sb->u.ext2_sb.s_block_bitmap_number[0] = block_bitmap_number;
		sb->u.ext2_sb.s_block_bitmap[0] = block_bitmap;

		/*
		 * There's still one special case here --- if block_bitmap == 0
		 * then our last attempt to read the bitmap failed and we have
		 * just ended up caching that failure.  Try again to read it.
		 */
		if (!block_bitmap)
			retval = read_block_bitmap (sb, block_group, 0);
	} else {
		if (sb->u.ext2_sb.s_loaded_block_bitmaps < EXT2_MAX_GROUP_LOADED)
			sb->u.ext2_sb.s_loaded_block_bitmaps++;
		else
			brelse (sb->u.ext2_sb.s_block_bitmap[EXT2_MAX_GROUP_LOADED - 1]);
		for (j = sb->u.ext2_sb.s_loaded_block_bitmaps - 1; j > 0; j--) {
			sb->u.ext2_sb.s_block_bitmap_number[j] =
				sb->u.ext2_sb.s_block_bitmap_number[j - 1];
			sb->u.ext2_sb.s_block_bitmap[j] =
				sb->u.ext2_sb.s_block_bitmap[j - 1];
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
 * filesystems is <= EXT2_MAX_GROUP_LOADED, then we have no way of 
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
	if (sb->u.ext2_sb.s_loaded_block_bitmaps > 0 &&
	    sb->u.ext2_sb.s_block_bitmap_number[0] == block_group &&
	    sb->u.ext2_sb.s_block_bitmap[0]) {
		return 0;
	}
	/*
	 * Or can we do a fast lookup based on a loaded group on a filesystem
	 * small enough to be mapped directly into the superblock?
	 */
	else if (sb->u.ext2_sb.s_groups_count <= EXT2_MAX_GROUP_LOADED && 
		 sb->u.ext2_sb.s_block_bitmap_number[block_group] == block_group &&
		 sb->u.ext2_sb.s_block_bitmap[block_group]) {
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
	if (!sb->u.ext2_sb.s_block_bitmap[slot])
		return -EIO;
	
	/*
	 * Must have been read in OK to get this far.
	 */
	return slot;
}

/* Free given blocks, update quota and i_blocks field */
void ext2_free_blocks (struct inode * inode, unsigned long block,
		       unsigned long count)
{
	struct buffer_head * bh;
	struct buffer_head * bh2;
	unsigned long block_group;
	unsigned long bit;
	unsigned long i;
	int bitmap_nr;
	unsigned long overflow;
	struct super_block * sb;
	struct ext2_group_desc * gdp;
	struct ext2_super_block * es;

	sb = inode->i_sb;
	if (!sb) {
		printk ("ext2_free_blocks: nonexistent device");
		return;
	}
	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
	if (block < le32_to_cpu(es->s_first_data_block) ||
	    block + count < block ||
	    (block + count) > le32_to_cpu(es->s_blocks_count)) {
		ext2_error (sb, "ext2_free_blocks",
			    "Freeing blocks not in datazone - "
			    "block = %lu, count = %lu", block, count);
		goto error_return;
	}

	ext2_debug ("freeing block(s) %lu-%lu\n", block, block + count - 1);

do_more:
	overflow = 0;
	block_group = (block - le32_to_cpu(es->s_first_data_block)) /
		      EXT2_BLOCKS_PER_GROUP(sb);
	bit = (block - le32_to_cpu(es->s_first_data_block)) %
		      EXT2_BLOCKS_PER_GROUP(sb);
	/*
	 * Check to see if we are freeing blocks across a group
	 * boundary.
	 */
	if (bit + count > EXT2_BLOCKS_PER_GROUP(sb)) {
		overflow = bit + count - EXT2_BLOCKS_PER_GROUP(sb);
		count -= overflow;
	}
	bitmap_nr = load_block_bitmap (sb, block_group);
	if (bitmap_nr < 0)
		goto error_return;
	
	bh = sb->u.ext2_sb.s_block_bitmap[bitmap_nr];
	gdp = ext2_get_group_desc (sb, block_group, &bh2);
	if (!gdp)
		goto error_return;

	for (i = 0; i < count; i++, block++) {
		if (block == le32_to_cpu(gdp->bg_block_bitmap) ||
		    block == le32_to_cpu(gdp->bg_inode_bitmap) ||
		    in_range(block, le32_to_cpu(gdp->bg_inode_table),
			     EXT2_SB(sb)->s_itb_per_group)) {
			ext2_error(sb, __FUNCTION__,
				   "Freeing block in system zone - block = %lu",
				   block);
			continue;
		}

		if (!ext2_clear_bit (bit + i, bh->b_data))
			ext2_error(sb, __FUNCTION__,
				   "bit already cleared for block %lu", block);
		else {
			DQUOT_FREE_BLOCK(inode, 1);
			gdp->bg_free_blocks_count =
				cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count)+1);
			es->s_free_blocks_count =
				cpu_to_le32(le32_to_cpu(es->s_free_blocks_count)+1);
		}
	}
	
	mark_buffer_dirty(bh2);
	mark_buffer_dirty(sb->u.ext2_sb.s_sbh);

	mark_buffer_dirty(bh);
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}
	if (overflow) {
		count = overflow;
		goto do_more;
	}
	sb->s_dirt = 1;
error_return:
	unlock_super (sb);
	return;
}

/*
 * ext2_new_block uses a goal block to assist allocation.  If the goal is
 * free, or there is a free block within 32 blocks of the goal, that block
 * is allocated.  Otherwise a forward search is made for a free block; within 
 * each block group the search first looks for an entire free byte in the block
 * bitmap, and then for any free bit if that fails.
 * This function also updates quota and i_blocks field.
 */
int ext2_new_block (struct inode * inode, unsigned long goal,
    u32 * prealloc_count, u32 * prealloc_block, int * err)
{
	struct buffer_head * bh;
	struct buffer_head * bh2;
	char * p, * r;
	int i, j, k, tmp;
	int bitmap_nr;
	struct super_block * sb;
	struct ext2_group_desc * gdp;
	struct ext2_super_block * es;
#ifdef EXT2FS_DEBUG
	static int goal_hits = 0, goal_attempts = 0;
#endif
	*err = -ENOSPC;
	sb = inode->i_sb;
	if (!sb) {
		printk ("ext2_new_block: nonexistent device");
		return 0;
	}

	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
	if (le32_to_cpu(es->s_free_blocks_count) <= le32_to_cpu(es->s_r_blocks_count) &&
	    ((sb->u.ext2_sb.s_resuid != current->fsuid) &&
	     (sb->u.ext2_sb.s_resgid == 0 ||
	      !in_group_p (sb->u.ext2_sb.s_resgid)) && 
	     !capable(CAP_SYS_RESOURCE)))
		goto out;

	ext2_debug ("goal=%lu.\n", goal);

repeat:
	/*
	 * First, test whether the goal block is free.
	 */
	if (goal < le32_to_cpu(es->s_first_data_block) ||
	    goal >= le32_to_cpu(es->s_blocks_count))
		goal = le32_to_cpu(es->s_first_data_block);
	i = (goal - le32_to_cpu(es->s_first_data_block)) / EXT2_BLOCKS_PER_GROUP(sb);
	gdp = ext2_get_group_desc (sb, i, &bh2);
	if (!gdp)
		goto io_error;

	if (le16_to_cpu(gdp->bg_free_blocks_count) > 0) {
		j = ((goal - le32_to_cpu(es->s_first_data_block)) % EXT2_BLOCKS_PER_GROUP(sb));
#ifdef EXT2FS_DEBUG
		if (j)
			goal_attempts++;
#endif
		bitmap_nr = load_block_bitmap (sb, i);
		if (bitmap_nr < 0)
			goto io_error;
		
		bh = sb->u.ext2_sb.s_block_bitmap[bitmap_nr];

		ext2_debug ("goal is at %d:%d.\n", i, j);

		if (!ext2_test_bit(j, bh->b_data)) {
			ext2_debug("goal bit allocated, %d hits\n",++goal_hits);
			goto got_block;
		}
		if (j) {
			/*
			 * The goal was occupied; search forward for a free 
			 * block within the next XX blocks.
			 *
			 * end_goal is more or less random, but it has to be
			 * less than EXT2_BLOCKS_PER_GROUP. Aligning up to the
			 * next 64-bit boundary is simple..
			 */
			int end_goal = (j + 63) & ~63;
			j = ext2_find_next_zero_bit(bh->b_data, end_goal, j);
			if (j < end_goal)
				goto got_block;
		}
	
		ext2_debug ("Bit not found near goal\n");

		/*
		 * There has been no free block found in the near vicinity
		 * of the goal: do a search forward through the block groups,
		 * searching in each group first for an entire free byte in
		 * the bitmap and then for any free bit.
		 * 
		 * Search first in the remainder of the current group; then,
		 * cyclicly search through the rest of the groups.
		 */
		p = ((char *) bh->b_data) + (j >> 3);
		r = memscan(p, 0, (EXT2_BLOCKS_PER_GROUP(sb) - j + 7) >> 3);
		k = (r - ((char *) bh->b_data)) << 3;
		if (k < EXT2_BLOCKS_PER_GROUP(sb)) {
			j = k;
			goto search_back;
		}

		k = ext2_find_next_zero_bit ((unsigned long *) bh->b_data, 
					EXT2_BLOCKS_PER_GROUP(sb),
					j);
		if (k < EXT2_BLOCKS_PER_GROUP(sb)) {
			j = k;
			goto got_block;
		}
	}

	ext2_debug ("Bit not found in block group %d.\n", i);

	/*
	 * Now search the rest of the groups.  We assume that 
	 * i and gdp correctly point to the last group visited.
	 */
	for (k = 0; k < sb->u.ext2_sb.s_groups_count; k++) {
		i++;
		if (i >= sb->u.ext2_sb.s_groups_count)
			i = 0;
		gdp = ext2_get_group_desc (sb, i, &bh2);
		if (!gdp)
			goto io_error;
		if (le16_to_cpu(gdp->bg_free_blocks_count) > 0)
			break;
	}
	if (k >= sb->u.ext2_sb.s_groups_count)
		goto out;
	bitmap_nr = load_block_bitmap (sb, i);
	if (bitmap_nr < 0)
		goto io_error;
	
	bh = sb->u.ext2_sb.s_block_bitmap[bitmap_nr];
	r = memscan(bh->b_data, 0, EXT2_BLOCKS_PER_GROUP(sb) >> 3);
	j = (r - bh->b_data) << 3;
	if (j < EXT2_BLOCKS_PER_GROUP(sb))
		goto search_back;
	else
		j = ext2_find_first_zero_bit ((unsigned long *) bh->b_data,
					 EXT2_BLOCKS_PER_GROUP(sb));
	if (j >= EXT2_BLOCKS_PER_GROUP(sb)) {
		ext2_error (sb, "ext2_new_block",
			    "Free blocks count corrupted for block group %d", i);
		goto out;
	}

search_back:
	/* 
	 * We have succeeded in finding a free byte in the block
	 * bitmap.  Now search backwards up to 7 bits to find the
	 * start of this group of free blocks.
	 */
	for (k = 0; k < 7 && j > 0 && !ext2_test_bit (j - 1, bh->b_data); k++, j--);
	
got_block:

	ext2_debug ("using block group %d(%d)\n", i, gdp->bg_free_blocks_count);

	/*
	 * Check quota for allocation of this block.
	 */
	if(DQUOT_ALLOC_BLOCK(inode, 1)) {
		*err = -EDQUOT;
		goto out;
	}

	tmp = j + i * EXT2_BLOCKS_PER_GROUP(sb) + le32_to_cpu(es->s_first_data_block);

	if (tmp == le32_to_cpu(gdp->bg_block_bitmap) ||
	    tmp == le32_to_cpu(gdp->bg_inode_bitmap) ||
	    in_range (tmp, le32_to_cpu(gdp->bg_inode_table),
		      EXT2_SB(sb)->s_itb_per_group)) {
		ext2_error (sb, "ext2_new_block",
			    "Allocating block in system zone - block = %u",
			    tmp);
		ext2_set_bit(j, bh->b_data);
		DQUOT_FREE_BLOCK(inode, 1);
		goto repeat;
	}

	if (ext2_set_bit (j, bh->b_data)) {
		ext2_warning (sb, "ext2_new_block",
			      "bit already set for block %d", j);
		DQUOT_FREE_BLOCK(inode, 1);
		goto repeat;
	}

	ext2_debug ("found bit %d\n", j);

	/*
	 * Do block preallocation now if required.
	 */
#ifdef EXT2_PREALLOCATE
	/* Writer: ->i_prealloc* */
	if (prealloc_count && !*prealloc_count) {
		int	prealloc_goal;
		unsigned long next_block = tmp + 1;

		prealloc_goal = es->s_prealloc_blocks ?
			es->s_prealloc_blocks : EXT2_DEFAULT_PREALLOC_BLOCKS;

		*prealloc_block = next_block;
		/* Writer: end */
		for (k = 1;
		     k < prealloc_goal && (j + k) < EXT2_BLOCKS_PER_GROUP(sb);
		     k++, next_block++) {
			if (DQUOT_PREALLOC_BLOCK(inode, 1))
				break;
			/* Writer: ->i_prealloc* */
			if (*prealloc_block + *prealloc_count != next_block ||
			    ext2_set_bit (j + k, bh->b_data)) {
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
		ext2_debug ("Preallocated a further %lu bits.\n",
			       (k - 1));
	}
#endif

	j = tmp;

	mark_buffer_dirty(bh);
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}

	if (j >= le32_to_cpu(es->s_blocks_count)) {
		ext2_error (sb, "ext2_new_block",
			    "block(%d) >= blocks count(%d) - "
			    "block_group = %d, es == %p ",j,
			le32_to_cpu(es->s_blocks_count), i, es);
		goto out;
	}

	ext2_debug ("allocating block %d. "
		    "Goal hits %d of %d.\n", j, goal_hits, goal_attempts);

	gdp->bg_free_blocks_count = cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count) - 1);
	mark_buffer_dirty(bh2);
	es->s_free_blocks_count = cpu_to_le32(le32_to_cpu(es->s_free_blocks_count) - 1);
	mark_buffer_dirty(sb->u.ext2_sb.s_sbh);
	sb->s_dirt = 1;
	unlock_super (sb);
	*err = 0;
	return j;
	
io_error:
	*err = -EIO;
out:
	unlock_super (sb);
	return 0;
	
}

unsigned long ext2_count_free_blocks (struct super_block * sb)
{
#ifdef EXT2FS_DEBUG
	struct ext2_super_block * es;
	unsigned long desc_count, bitmap_count, x;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	int i;
	
	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	for (i = 0; i < sb->u.ext2_sb.s_groups_count; i++) {
		gdp = ext2_get_group_desc (sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
		bitmap_nr = load_block_bitmap (sb, i);
		if (bitmap_nr < 0)
			continue;
		
		x = ext2_count_free (sb->u.ext2_sb.s_block_bitmap[bitmap_nr],
				     sb->s_blocksize);
		printk ("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(gdp->bg_free_blocks_count), x);
		bitmap_count += x;
	}
	printk("ext2_count_free_blocks: stored = %lu, computed = %lu, %lu\n",
	       le32_to_cpu(es->s_free_blocks_count), desc_count, bitmap_count);
	unlock_super (sb);
	return bitmap_count;
#else
	return le32_to_cpu(sb->u.ext2_sb.s_es->s_free_blocks_count);
#endif
}

static inline int block_in_use (unsigned long block,
				struct super_block * sb,
				unsigned char * map)
{
	return ext2_test_bit ((block - le32_to_cpu(sb->u.ext2_sb.s_es->s_first_data_block)) %
			 EXT2_BLOCKS_PER_GROUP(sb), map);
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

int ext2_group_sparse(int group)
{
	return (test_root(group, 3) || test_root(group, 5) ||
		test_root(group, 7));
}

/**
 *	ext2_bg_has_super - number of blocks used by the superblock in group
 *	@sb: superblock for filesystem
 *	@group: group number to check
 *
 *	Return the number of blocks used by the superblock (primary or backup)
 *	in this group.  Currently this will be only 0 or 1.
 */
int ext2_bg_has_super(struct super_block *sb, int group)
{
	if (EXT2_HAS_RO_COMPAT_FEATURE(sb,EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)&&
	    !ext2_group_sparse(group))
		return 0;
	return 1;
}

/**
 *	ext2_bg_num_gdb - number of blocks used by the group table in group
 *	@sb: superblock for filesystem
 *	@group: group number to check
 *
 *	Return the number of blocks used by the group descriptor table
 *	(primary or backup) in this group.  In the future there may be a
 *	different number of descriptor blocks in each group.
 */
unsigned long ext2_bg_num_gdb(struct super_block *sb, int group)
{
	if (EXT2_HAS_RO_COMPAT_FEATURE(sb,EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER)&&
	    !ext2_group_sparse(group))
		return 0;
	return EXT2_SB(sb)->s_gdb_count;
}

#ifdef CONFIG_EXT2_CHECK
/* Called at mount-time, super-block is locked */
void ext2_check_blocks_bitmap (struct super_block * sb)
{
	struct buffer_head * bh;
	struct ext2_super_block * es;
	unsigned long desc_count, bitmap_count, x, j;
	unsigned long desc_blocks;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	int i;

	es = sb->u.ext2_sb.s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	for (i = 0; i < sb->u.ext2_sb.s_groups_count; i++) {
		gdp = ext2_get_group_desc (sb, i, NULL);
		if (!gdp)
			continue;
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
		bitmap_nr = load_block_bitmap (sb, i);
		if (bitmap_nr < 0)
			continue;

		bh = EXT2_SB(sb)->s_block_bitmap[bitmap_nr];

		if (ext2_bg_has_super(sb, i) && !ext2_test_bit(0, bh->b_data))
			ext2_error(sb, __FUNCTION__,
				   "Superblock in group %d is marked free", i);

		desc_blocks = ext2_bg_num_gdb(sb, i);
		for (j = 0; j < desc_blocks; j++)
			if (!ext2_test_bit(j + 1, bh->b_data))
				ext2_error(sb, __FUNCTION__,
					   "Descriptor block #%ld in group "
					   "%d is marked free", j, i);

		if (!block_in_use (le32_to_cpu(gdp->bg_block_bitmap), sb, bh->b_data))
			ext2_error (sb, "ext2_check_blocks_bitmap",
				    "Block bitmap for group %d is marked free",
				    i);

		if (!block_in_use (le32_to_cpu(gdp->bg_inode_bitmap), sb, bh->b_data))
			ext2_error (sb, "ext2_check_blocks_bitmap",
				    "Inode bitmap for group %d is marked free",
				    i);

		for (j = 0; j < sb->u.ext2_sb.s_itb_per_group; j++)
			if (!block_in_use (le32_to_cpu(gdp->bg_inode_table) + j, sb, bh->b_data))
				ext2_error (sb, "ext2_check_blocks_bitmap",
					    "Block #%ld of the inode table in "
					    "group %d is marked free", j, i);

		x = ext2_count_free (bh, sb->s_blocksize);
		if (le16_to_cpu(gdp->bg_free_blocks_count) != x)
			ext2_error (sb, "ext2_check_blocks_bitmap",
				    "Wrong free blocks count for group %d, "
				    "stored = %d, counted = %lu", i,
				    le16_to_cpu(gdp->bg_free_blocks_count), x);
		bitmap_count += x;
	}
	if (le32_to_cpu(es->s_free_blocks_count) != bitmap_count)
		ext2_error (sb, "ext2_check_blocks_bitmap",
			    "Wrong free blocks count in super block, "
			    "stored = %lu, counted = %lu",
			    (unsigned long) le32_to_cpu(es->s_free_blocks_count), bitmap_count);
}
#endif
