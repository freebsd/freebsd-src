/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 *  linux/fs/ext2/balloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  Enhanced block allocation by Stephen Tweedie (sct@dcs.ed.ac.uk), 1993
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include <ufs/ufs/quota.h>
#include <ufs/ufs/ufsmount.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>

#ifdef __i386__
#include <gnu/ext2fs/i386-bitops.h>
#else
#error Provide an bitops.h file, please !
#endif

#define in_range(b, first, len)		((b) >= (first) && (b) <= (first) + (len) - 1)

/* got rid of get_group_desc since it can already be found in 
 * ext2_linux_ialloc.c
 */

static void read_block_bitmap (struct mount * mp,
			       unsigned int block_group,
			       unsigned long bitmap_nr)
{
	struct ext2_sb_info *sb = VFSTOUFS(mp)->um_e2fs;
	struct ext2_group_desc * gdp;
	struct buffer_head * bh;
	int    error;
	
	gdp = get_group_desc (mp, block_group, NULL);
	if(error = bread (VFSTOUFS(mp)->um_devvp, 
		fsbtodb(sb, gdp->bg_block_bitmap),sb->s_blocksize, NOCRED, &bh))
		panic ( "read_block_bitmap: "
			    "Cannot read block bitmap - "
			    "block_group = %d, block_bitmap = %lu",
			    block_group, (unsigned long) gdp->bg_block_bitmap);
	sb->s_block_bitmap_number[bitmap_nr] = block_group;
	sb->s_block_bitmap[bitmap_nr] = bh;
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
 */
static int load__block_bitmap (struct mount * mp,
			       unsigned int block_group)
{
	int i, j;
	struct ext2_sb_info *sb = VFSTOUFS(mp)->um_e2fs;
	unsigned long block_bitmap_number;
	struct buffer_head * block_bitmap;

	if (block_group >= sb->s_groups_count)
		panic ( "load_block_bitmap: "
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sb->s_groups_count);

	if (sb->s_groups_count <= EXT2_MAX_GROUP_LOADED) {
		if (sb->s_block_bitmap[block_group]) {
			if (sb->s_block_bitmap_number[block_group] !=
			    block_group)
				panic ( "load_block_bitmap: "
					    "block_group != block_bitmap_number");
			else
				return block_group;
		} else {
			read_block_bitmap (mp, block_group, block_group);
			return block_group;
		}
	}

	for (i = 0; i < sb->s_loaded_block_bitmaps &&
		    sb->s_block_bitmap_number[i] != block_group; i++)
		;
	if (i < sb->s_loaded_block_bitmaps &&
  	    sb->s_block_bitmap_number[i] == block_group) {
		block_bitmap_number = sb->s_block_bitmap_number[i];
		block_bitmap = sb->s_block_bitmap[i];
		for (j = i; j > 0; j--) {
			sb->s_block_bitmap_number[j] =
				sb->s_block_bitmap_number[j - 1];
			sb->s_block_bitmap[j] =
				sb->s_block_bitmap[j - 1];
		}
		sb->s_block_bitmap_number[0] = block_bitmap_number;
		sb->s_block_bitmap[0] = block_bitmap;
	} else {
		if (sb->s_loaded_block_bitmaps < EXT2_MAX_GROUP_LOADED)
			sb->s_loaded_block_bitmaps++;
		else
			brelse (sb->s_block_bitmap[EXT2_MAX_GROUP_LOADED - 1]);
		for (j = sb->s_loaded_block_bitmaps - 1; j > 0;  j--) {
			sb->s_block_bitmap_number[j] =
				sb->s_block_bitmap_number[j - 1];
			sb->s_block_bitmap[j] =
				sb->s_block_bitmap[j - 1];
		}
		read_block_bitmap (mp, block_group, 0);
	}
	return 0;
}

static inline int load_block_bitmap (struct mount * mp,
				     unsigned int block_group)
{
	struct ext2_sb_info *sb = VFSTOUFS(mp)->um_e2fs;
	if (sb->s_loaded_block_bitmaps > 0 &&
	    sb->s_block_bitmap_number[0] == block_group)
		return 0;
	
	if (sb->s_groups_count <= EXT2_MAX_GROUP_LOADED && 
	    sb->s_block_bitmap_number[block_group] == block_group &&
	    sb->s_block_bitmap[block_group]) 
		return block_group;

	return load__block_bitmap (mp, block_group);
}

void ext2_free_blocks (struct mount * mp, unsigned long block,
		       unsigned long count)
{
	struct ext2_sb_info *sb = VFSTOUFS(mp)->um_e2fs;
	struct buffer_head * bh;
	struct buffer_head * bh2;
	unsigned long block_group;
	unsigned long bit;
	unsigned long i;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	struct ext2_super_block * es = sb->s_es;

	if (!sb) {
		printf ("ext2_free_blocks: nonexistent device");
		return;
	}
	lock_super (VFSTOUFS(mp)->um_devvp);
	if (block < es->s_first_data_block || 
	    (block + count) > es->s_blocks_count) {
		printf ( "ext2_free_blocks: "
			    "Freeing blocks not in datazone - "
			    "block = %lu, count = %lu", block, count);
		unlock_super (VFSTOUFS(mp)->um_devvp);
		return;
	}

	ext2_debug ("freeing blocks %lu to %lu\n", block, block+count-1);

	block_group = (block - es->s_first_data_block) /
		      EXT2_BLOCKS_PER_GROUP(sb);
	bit = (block - es->s_first_data_block) % EXT2_BLOCKS_PER_GROUP(sb);
	if (bit + count > EXT2_BLOCKS_PER_GROUP(sb))
		panic ( "ext2_free_blocks: "
			    "Freeing blocks across group boundary - "
			    "Block = %lu, count = %lu",
			    block, count);
	bitmap_nr = load_block_bitmap (mp, block_group);
	bh = sb->s_block_bitmap[bitmap_nr];
	gdp = get_group_desc (mp, block_group, &bh2);

	if (/* test_opt (sb, CHECK_STRICT) && 	assume always strict ! */
	    (in_range (gdp->bg_block_bitmap, block, count) ||
	     in_range (gdp->bg_inode_bitmap, block, count) ||
	     in_range (block, gdp->bg_inode_table,
		       sb->s_itb_per_group) ||
	     in_range (block + count - 1, gdp->bg_inode_table,
		       sb->s_itb_per_group)))
		panic ( "ext2_free_blocks: "
			    "Freeing blocks in system zones - "
			    "Block = %lu, count = %lu",
			    block, count);

	for (i = 0; i < count; i++) {
		if (!clear_bit (bit + i, bh->b_data))
			printf ("ext2_free_blocks: "
				      "bit already cleared for block %lu", 
				      block);
		else {
			gdp->bg_free_blocks_count++;
			es->s_free_blocks_count++;
		}
	}

	mark_buffer_dirty(bh2);
	mark_buffer_dirty(bh);
/****
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}
****/
	sb->s_dirt = 1;
	unlock_super (VFSTOUFS(mp)->um_devvp);
	return;
}

/*
 * ext2_new_block uses a goal block to assist allocation.  If the goal is
 * free, or there is a free block within 32 blocks of the goal, that block
 * is allocated.  Otherwise a forward search is made for a free block; within 
 * each block group the search first looks for an entire free byte in the block
 * bitmap, and then for any free bit if that fails.
 */
int ext2_new_block (struct mount * mp, unsigned long goal,
		    int * prealloc_count,
		    int * prealloc_block)
{
	struct ext2_sb_info *sb = VFSTOUFS(mp)->um_e2fs;
	struct buffer_head * bh;
	struct buffer_head * bh2;
	char * p, * r;
	int i, j, k, tmp;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	struct ext2_super_block * es = sb->s_es;

#ifdef EXT2FS_DEBUG
	static int goal_hits = 0, goal_attempts = 0;
#endif
	if (!sb) {
		printf ("ext2_new_block: nonexistent device");
		return 0;
	}
	lock_super (VFSTOUFS(mp)->um_devvp);

        ext2_debug ("goal=%lu.\n", goal);

repeat:
	/*
	 * First, test whether the goal block is free.
	 */
	if (goal < es->s_first_data_block || goal >= es->s_blocks_count)
		goal = es->s_first_data_block;
	i = (goal - es->s_first_data_block) / EXT2_BLOCKS_PER_GROUP(sb);
	gdp = get_group_desc (mp, i, &bh2);
	if (gdp->bg_free_blocks_count > 0) {
		j = ((goal - es->s_first_data_block) % EXT2_BLOCKS_PER_GROUP(sb));
#ifdef EXT2FS_DEBUG
		if (j)
			goal_attempts++;
#endif
		bitmap_nr = load_block_bitmap (mp, i);
		bh = sb->s_block_bitmap[bitmap_nr];

		ext2_debug ("goal is at %d:%d.\n", i, j); 

		if (!test_bit(j, bh->b_data)) {
#ifdef EXT2FS_DEBUG
			goal_hits++;
			ext2_debug ("goal bit allocated.\n");
#endif
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
			j = find_next_zero_bit(bh->b_data, end_goal, j);
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
		k = find_next_zero_bit ((unsigned long *) bh->b_data, 
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
	for (k = 0; k < sb->s_groups_count; k++) {
		i++;
		if (i >= sb->s_groups_count)
			i = 0;
		gdp = get_group_desc (mp, i, &bh2);
		if (gdp->bg_free_blocks_count > 0)
			break;
	}
	if (k >= sb->s_groups_count) {
		unlock_super (VFSTOUFS(mp)->um_devvp);
		return 0;
	}
	bitmap_nr = load_block_bitmap (mp, i);
	bh = sb->s_block_bitmap[bitmap_nr];
	r = memscan(bh->b_data, 0, EXT2_BLOCKS_PER_GROUP(sb) >> 3);
	j = (r - bh->b_data) << 3;

	if (j < EXT2_BLOCKS_PER_GROUP(sb))
		goto search_back;
	else
		j = find_first_zero_bit ((unsigned long *) bh->b_data,
					 EXT2_BLOCKS_PER_GROUP(sb));
	if (j >= EXT2_BLOCKS_PER_GROUP(sb)) {
		printf ( "ext2_new_block: "
			 "Free blocks count corrupted for block group %d", i);
		unlock_super (VFSTOUFS(mp)->um_devvp);
		return 0;
	}

search_back:
	/* 
	 * We have succeeded in finding a free byte in the block
	 * bitmap.  Now search backwards up to 7 bits to find the
	 * start of this group of free blocks.
	 */
	for (k = 0; k < 7 && j > 0 && !test_bit (j - 1, bh->b_data); k++, j--);
	
got_block:

	ext2_debug ("using block group %d(%d)\n", i, gdp->bg_free_blocks_count);

	tmp = j + i * EXT2_BLOCKS_PER_GROUP(sb) + es->s_first_data_block;

	if (/* test_opt (sb, CHECK_STRICT) && we are always strict. */
	    (tmp == gdp->bg_block_bitmap ||
	     tmp == gdp->bg_inode_bitmap ||
	     in_range (tmp, gdp->bg_inode_table, sb->s_itb_per_group)))
		panic ( "ext2_new_block: "
			    "Allocating block in system zone - "
			    "%dth block = %u in group %u", j, tmp, i);

	if (set_bit (j, bh->b_data)) {
		printf ( "ext2_new_block: "
			 "bit already set for block %d", j);
		goto repeat;
	}

	ext2_debug ("found bit %d\n", j);

	/*
	 * Do block preallocation now if required.
	 */
#ifdef EXT2_PREALLOCATE
	if (prealloc_block) {
		*prealloc_count = 0;
		*prealloc_block = tmp + 1;
		for (k = 1;
		     k < 8 && (j + k) < EXT2_BLOCKS_PER_GROUP(sb); k++) {
			if (set_bit (j + k, bh->b_data))
				break;
			(*prealloc_count)++;
		}	
		gdp->bg_free_blocks_count -= *prealloc_count;
		es->s_free_blocks_count -= *prealloc_count;
		ext2_debug ("Preallocated a further %lu bits.\n",
			    *prealloc_count); 
	}
#endif

	j = tmp;

	mark_buffer_dirty(bh);
/****
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}
****/
	if (j >= es->s_blocks_count) {
		printf ( "ext2_new_block: "
			    "block >= blocks count - "
			    "block_group = %d, block=%d", i, j);
		unlock_super (VFSTOUFS(mp)->um_devvp);
		return 0;
	}

	ext2_debug ("allocating block %d. "
		    "Goal hits %d of %d.\n", j, goal_hits, goal_attempts);

	gdp->bg_free_blocks_count--;
	mark_buffer_dirty(bh2);
	es->s_free_blocks_count--;
	sb->s_dirt = 1;
	unlock_super (VFSTOUFS(mp)->um_devvp);
	return j;
}

#ifdef unused
static unsigned long ext2_count_free_blocks (struct mount * mp)
{
	struct ext2_sb_info *sb = VFSTOUFS(mp)->um_e2fs;
#ifdef EXT2FS_DEBUG
	struct ext2_super_block * es;
	unsigned long desc_count, bitmap_count, x;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	int i;
	
	lock_super (VFSTOUFS(mp)->um_devvp);
	es = sb->s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	for (i = 0; i < sb->s_groups_count; i++) {
		gdp = get_group_desc (mp, i, NULL);
		desc_count += gdp->bg_free_blocks_count;
		bitmap_nr = load_block_bitmap (mp, i);
		x = ext2_count_free (sb->s_block_bitmap[bitmap_nr],
				     sb->s_blocksize);
		ext2_debug ("group %d: stored = %d, counted = %lu\n",
			i, gdp->bg_free_blocks_count, x);
		bitmap_count += x;
	}
	ext2_debug( "stored = %lu, computed = %lu, %lu\n",
	       es->s_free_blocks_count, desc_count, bitmap_count);
	unlock_super (VFSTOUFS(mp)->um_devvp);
	return bitmap_count;
#else
	return sb->s_es->s_free_blocks_count;
#endif
}
#endif /* unused */

static inline int block_in_use (unsigned long block,
				struct ext2_sb_info * sb,
				unsigned char * map)
{
	return test_bit ((block - sb->s_es->s_first_data_block) %
			 EXT2_BLOCKS_PER_GROUP(sb), map);
}

#ifdef unused
static void ext2_check_blocks_bitmap (struct mount * mp)
{
	struct ext2_sb_info *sb = VFSTOUFS(mp)->um_e2fs;
	struct buffer_head * bh;
	struct ext2_super_block * es;
	unsigned long desc_count, bitmap_count, x;
	unsigned long desc_blocks;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	int i, j;

	lock_super (VFSTOUFS(mp)->um_devvp);
	es = sb->s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	desc_blocks = (sb->s_groups_count + EXT2_DESC_PER_BLOCK(sb) - 1) /
		      EXT2_DESC_PER_BLOCK(sb);
	for (i = 0; i < sb->s_groups_count; i++) {
		gdp = get_group_desc (mp, i, NULL);
		desc_count += gdp->bg_free_blocks_count;
		bitmap_nr = load_block_bitmap (mp, i);
		bh = sb->s_block_bitmap[bitmap_nr];

		if (!test_bit (0, bh->b_data))
			printf ( "ext2_check_blocks_bitmap: "
				    "Superblock in group %d is marked free", i);

		for (j = 0; j < desc_blocks; j++)
			if (!test_bit (j + 1, bh->b_data))
				printf ("ext2_check_blocks_bitmap: "
					    "Descriptor block #%d in group "
					    "%d is marked free", j, i);

		if (!block_in_use (gdp->bg_block_bitmap, sb, bh->b_data))
			printf ("ext2_check_blocks_bitmap: "
				    "Block bitmap for group %d is marked free",
				    i);

		if (!block_in_use (gdp->bg_inode_bitmap, sb, bh->b_data))
			printf ("ext2_check_blocks_bitmap: "
				    "Inode bitmap for group %d is marked free",
				    i);

		for (j = 0; j < sb->s_itb_per_group; j++)
			if (!block_in_use (gdp->bg_inode_table + j, sb, bh->b_data))
				printf ("ext2_check_blocks_bitmap: "
					    "Block #%d of the inode table in "
					    "group %d is marked free", j, i);

		x = ext2_count_free (bh, sb->s_blocksize);
		if (gdp->bg_free_blocks_count != x)
			printf ("ext2_check_blocks_bitmap: "
				    "Wrong free blocks count for group %d, "
				    "stored = %d, counted = %lu", i,
				    gdp->bg_free_blocks_count, x);
		bitmap_count += x;
	}
	if (es->s_free_blocks_count != bitmap_count)
		printf ("ext2_check_blocks_bitmap: "
			    "Wrong free blocks count in super block, "
			    "stored = %lu, counted = %lu",
			    (unsigned long) es->s_free_blocks_count, bitmap_count);
	unlock_super (VFSTOUFS(mp)->um_devvp);
}
#endif /* unused */

/*
 *  this function is taken from 
 *  linux/fs/ext2/bitmap.c
 */

static int nibblemap[] = {4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};

unsigned long ext2_count_free (struct buffer_head * map, unsigned int numchars)
{
        unsigned int i;
        unsigned long sum = 0;

        if (!map)
                return (0);
        for (i = 0; i < numchars; i++)
                sum += nibblemap[map->b_data[i] & 0xf] +
                        nibblemap[(map->b_data[i] >> 4) & 0xf];
        return (sum);
}
