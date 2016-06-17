/*
 *  linux/fs/ext2/ialloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  BSD ufs-inspired inode and directory allocation by 
 *  Stephen Tweedie (sct@dcs.ed.ac.uk), 1993
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/locks.h>
#include <linux/quotaops.h>


/*
 * ialloc.c contains the inodes allocation and deallocation routines
 */

/*
 * The free inodes are managed by bitmaps.  A file system contains several
 * blocks groups.  Each group contains 1 bitmap block for blocks, 1 bitmap
 * block for inodes, N blocks for the inode table and data blocks.
 *
 * The file system contains group descriptors which are located after the
 * super block.  Each descriptor contains the number of the bitmap block and
 * the free blocks count in the block.  The descriptors are loaded in memory
 * when a file system is mounted (see ext2_read_super).
 */


/*
 * Read the inode allocation bitmap for a given block_group, reading
 * into the specified slot in the superblock's bitmap cache.
 *
 * Return buffer_head of bitmap on success or NULL.
 */
static struct buffer_head *read_inode_bitmap (struct super_block * sb,
					       unsigned long block_group)
{
	struct ext2_group_desc *desc;
	struct buffer_head *bh = NULL;

	desc = ext2_get_group_desc(sb, block_group, NULL);
	if (!desc)
		goto error_out;

	bh = sb_bread(sb, le32_to_cpu(desc->bg_inode_bitmap));
	if (!bh)
		ext2_error (sb, "read_inode_bitmap",
			    "Cannot read inode bitmap - "
			    "block_group = %lu, inode_bitmap = %lu",
			    block_group, (unsigned long) desc->bg_inode_bitmap);
error_out:
	return bh;
}

/*
 * load_inode_bitmap loads the inode bitmap for a blocks group
 *
 * It maintains a cache for the last bitmaps loaded.  This cache is managed
 * with a LRU algorithm.
 *
 * Notes:
 * 1/ There is one cache per mounted file system.
 * 2/ If the file system contains less than EXT2_MAX_GROUP_LOADED groups,
 *    this function reads the bitmap without maintaining a LRU cache.
 * 
 * Return the buffer_head of the bitmap or the ERR_PTR(error)
 */
static struct buffer_head *load_inode_bitmap (struct super_block * sb,
					      unsigned int block_group)
{
	int i, slot = 0;
	struct ext2_sb_info *sbi = &sb->u.ext2_sb;
	struct buffer_head *bh = sbi->s_inode_bitmap[0];

	if (block_group >= sbi->s_groups_count)
		ext2_panic (sb, "load_inode_bitmap",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			     block_group, sbi->s_groups_count);

	if (sbi->s_loaded_inode_bitmaps > 0 &&
	    sbi->s_inode_bitmap_number[0] == block_group && bh)
		goto found;

	if (sbi->s_groups_count <= EXT2_MAX_GROUP_LOADED) {
		slot = block_group;
		bh = sbi->s_inode_bitmap[slot];
		if (!bh)
			goto read_it;
		if (sbi->s_inode_bitmap_number[slot] == slot)
			goto found;
		ext2_panic (sb, "load_inode_bitmap",
			    "block_group != inode_bitmap_number");
	}

	bh = NULL;
	for (i = 0; i < sbi->s_loaded_inode_bitmaps &&
		    sbi->s_inode_bitmap_number[i] != block_group;
	     i++)
		;
	if (i < sbi->s_loaded_inode_bitmaps)
		bh = sbi->s_inode_bitmap[i];
	else if (sbi->s_loaded_inode_bitmaps < EXT2_MAX_GROUP_LOADED)
		sbi->s_loaded_inode_bitmaps++;
	else
		brelse (sbi->s_inode_bitmap[--i]);

	while (i--) {
		sbi->s_inode_bitmap_number[i+1] = sbi->s_inode_bitmap_number[i];
		sbi->s_inode_bitmap[i+1] = sbi->s_inode_bitmap[i];
	}

read_it:
	if (!bh)
		bh = read_inode_bitmap (sb, block_group);
	sbi->s_inode_bitmap_number[slot] = block_group;
	sbi->s_inode_bitmap[slot] = bh;
	if (!bh)
		return ERR_PTR(-EIO);
found:
	return bh;
}

/*
 * NOTE! When we get the inode, we're the only people
 * that have access to it, and as such there are no
 * race conditions we have to worry about. The inode
 * is not on the hash-lists, and it cannot be reached
 * through the filesystem because the directory entry
 * has been deleted earlier.
 *
 * HOWEVER: we must make sure that we get no aliases,
 * which means that we have to call "clear_inode()"
 * _before_ we mark the inode not in use in the inode
 * bitmaps. Otherwise a newly created file might use
 * the same inode number (not actually the same pointer
 * though), and then we'd have two inodes sharing the
 * same inode number and space on the harddisk.
 */
void ext2_free_inode (struct inode * inode)
{
	struct super_block * sb = inode->i_sb;
	int is_directory;
	unsigned long ino;
	struct buffer_head * bh;
	struct buffer_head * bh2;
	unsigned long block_group;
	unsigned long bit;
	struct ext2_group_desc * desc;
	struct ext2_super_block * es;

	ino = inode->i_ino;
	ext2_debug ("freeing inode %lu\n", ino);

	/*
	 * Note: we must free any quota before locking the superblock,
	 * as writing the quota to disk may need the lock as well.
	 */
	if (!is_bad_inode(inode)) {
		/* Quota is already initialized in iput() */
	    	DQUOT_FREE_INODE(inode);
		DQUOT_DROP(inode);
	}

	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
	is_directory = S_ISDIR(inode->i_mode);

	/* Do this BEFORE marking the inode not in use or returning an error */
	clear_inode (inode);

	if (ino < EXT2_FIRST_INO(sb) ||
	    ino > le32_to_cpu(es->s_inodes_count)) {
		ext2_error (sb, "ext2_free_inode",
			    "reserved or nonexistent inode %lu", ino);
		goto error_return;
	}
	block_group = (ino - 1) / EXT2_INODES_PER_GROUP(sb);
	bit = (ino - 1) % EXT2_INODES_PER_GROUP(sb);
	bh = load_inode_bitmap (sb, block_group);
	if (IS_ERR(bh))
		goto error_return;

	/* Ok, now we can actually update the inode bitmaps.. */
	if (!ext2_clear_bit (bit, bh->b_data))
		ext2_error (sb, "ext2_free_inode",
			      "bit already cleared for inode %lu", ino);
	else {
		desc = ext2_get_group_desc (sb, block_group, &bh2);
		if (desc) {
			desc->bg_free_inodes_count =
				cpu_to_le16(le16_to_cpu(desc->bg_free_inodes_count) + 1);
			if (is_directory)
				desc->bg_used_dirs_count =
					cpu_to_le16(le16_to_cpu(desc->bg_used_dirs_count) - 1);
		}
		mark_buffer_dirty(bh2);
		es->s_free_inodes_count =
			cpu_to_le32(le32_to_cpu(es->s_free_inodes_count) + 1);
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh);
	}
	mark_buffer_dirty(bh);
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}
	sb->s_dirt = 1;
error_return:
	unlock_super (sb);
}

/*
 * There are two policies for allocating an inode.  If the new inode is
 * a directory, then a forward search is made for a block group with both
 * free space and a low directory-to-inode ratio; if that fails, then of
 * the groups with above-average free space, that group with the fewest
 * directories already is chosen.
 *
 * For other inodes, search forward from the parent directory\'s block
 * group to find a free inode.
 */

static int find_group_dir(struct super_block *sb, int parent_group)
{
	struct ext2_super_block * es = sb->u.ext2_sb.s_es;
	int ngroups = sb->u.ext2_sb.s_groups_count;
	int avefreei = le32_to_cpu(es->s_free_inodes_count) / ngroups;
	struct ext2_group_desc *desc, *best_desc = NULL;
	struct buffer_head *bh, *best_bh = NULL;
	int group, best_group = -1;

	for (group = 0; group < ngroups; group++) {
		desc = ext2_get_group_desc (sb, group, &bh);
		if (!desc || !desc->bg_free_inodes_count)
			continue;
		if (le16_to_cpu(desc->bg_free_inodes_count) < avefreei)
			continue;
		if (!best_desc || 
		    (le16_to_cpu(desc->bg_free_blocks_count) >
		     le16_to_cpu(best_desc->bg_free_blocks_count))) {
			best_group = group;
			best_desc = desc;
			best_bh = bh;
		}
	}
	if (!best_desc)
		return -1;
	best_desc->bg_free_inodes_count =
		cpu_to_le16(le16_to_cpu(best_desc->bg_free_inodes_count) - 1);
	best_desc->bg_used_dirs_count =
		cpu_to_le16(le16_to_cpu(best_desc->bg_used_dirs_count) + 1);
	mark_buffer_dirty(best_bh);
	return best_group;
}

static int find_group_other(struct super_block *sb, int parent_group)
{
	int ngroups = sb->u.ext2_sb.s_groups_count;
	struct ext2_group_desc *desc;
	struct buffer_head *bh;
	int group, i;

	/*
	 * Try to place the inode in its parent directory
	 */
	group = parent_group;
	desc = ext2_get_group_desc (sb, group, &bh);
	if (desc && le16_to_cpu(desc->bg_free_inodes_count))
		goto found;

	/*
	 * Use a quadratic hash to find a group with a
	 * free inode
	 */
	for (i = 1; i < ngroups; i <<= 1) {
		group += i;
		if (group >= ngroups)
			group -= ngroups;
		desc = ext2_get_group_desc (sb, group, &bh);
		if (desc && le16_to_cpu(desc->bg_free_inodes_count))
			goto found;
	}

	/*
	 * That failed: try linear search for a free inode
	 */
	group = parent_group + 1;
	for (i = 2; i < ngroups; i++) {
		if (++group >= ngroups)
			group = 0;
		desc = ext2_get_group_desc (sb, group, &bh);
		if (desc && le16_to_cpu(desc->bg_free_inodes_count))
			goto found;
	}

	return -1;

found:
	desc->bg_free_inodes_count =
		cpu_to_le16(le16_to_cpu(desc->bg_free_inodes_count) - 1);
	mark_buffer_dirty(bh);
	return group;
}

struct inode * ext2_new_inode (const struct inode * dir, int mode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	struct buffer_head * bh2;
	int group, i;
	ino_t ino;
	struct inode * inode;
	struct ext2_group_desc * desc;
	struct ext2_super_block * es;
	int err;

	sb = dir->i_sb;
	inode = new_inode(sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
repeat:
	if (S_ISDIR(mode))
		group = find_group_dir(sb, dir->u.ext2_i.i_block_group);
	else 
		group = find_group_other(sb, dir->u.ext2_i.i_block_group);

	err = -ENOSPC;
	if (group == -1)
		goto fail;

	err = -EIO;
	bh = load_inode_bitmap (sb, group);
	if (IS_ERR(bh))
		goto fail2;

	i = ext2_find_first_zero_bit ((unsigned long *) bh->b_data,
				      EXT2_INODES_PER_GROUP(sb));
	if (i >= EXT2_INODES_PER_GROUP(sb))
		goto bad_count;
	ext2_set_bit (i, bh->b_data);

	mark_buffer_dirty(bh);
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}

	ino = group * EXT2_INODES_PER_GROUP(sb) + i + 1;
	if (ino < EXT2_FIRST_INO(sb) || ino > le32_to_cpu(es->s_inodes_count)) {
		ext2_error (sb, "ext2_new_inode",
			    "reserved inode or inode > inodes count - "
			    "block_group = %d,inode=%ld", group, ino);
		err = -EIO;
		goto fail2;
	}

	es->s_free_inodes_count =
		cpu_to_le32(le32_to_cpu(es->s_free_inodes_count) - 1);
	mark_buffer_dirty(sb->u.ext2_sb.s_sbh);
	sb->s_dirt = 1;
	inode->i_uid = current->fsuid;
	if (test_opt (sb, GRPID))
		inode->i_gid = dir->i_gid;
	else if (dir->i_mode & S_ISGID) {
		inode->i_gid = dir->i_gid;
		if (S_ISDIR(mode))
			mode |= S_ISGID;
	} else
		inode->i_gid = current->fsgid;
	inode->i_mode = mode;

	inode->i_ino = ino;
	inode->i_blksize = PAGE_SIZE;	/* This is the optimal IO size (for stat), not the fs block size */
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->u.ext2_i.i_state = EXT2_STATE_NEW;
	inode->u.ext2_i.i_flags = dir->u.ext2_i.i_flags & ~EXT2_BTREE_FL;
	if (S_ISLNK(mode))
		inode->u.ext2_i.i_flags &= ~(EXT2_IMMUTABLE_FL|EXT2_APPEND_FL);
	inode->u.ext2_i.i_block_group = group;
	ext2_set_inode_flags(inode);
	insert_inode_hash(inode);
	inode->i_generation = event++;
	mark_inode_dirty(inode);

	unlock_super (sb);
	if(DQUOT_ALLOC_INODE(inode)) {
		DQUOT_DROP(inode);
		inode->i_flags |= S_NOQUOTA;
		inode->i_nlink = 0;
		iput(inode);
		return ERR_PTR(-EDQUOT);
	}
	ext2_debug ("allocating inode %lu\n", inode->i_ino);
	return inode;

fail2:
	desc = ext2_get_group_desc (sb, group, &bh2);
	desc->bg_free_inodes_count =
		cpu_to_le16(le16_to_cpu(desc->bg_free_inodes_count) + 1);
	if (S_ISDIR(mode))
		desc->bg_used_dirs_count =
			cpu_to_le16(le16_to_cpu(desc->bg_used_dirs_count) - 1);
	mark_buffer_dirty(bh2);
fail:
	unlock_super(sb);
	make_bad_inode(inode);
	iput(inode);
	return ERR_PTR(err);

bad_count:
	ext2_error (sb, "ext2_new_inode",
		    "Free inodes count corrupted in group %d",
		    group);
	/* Is it really ENOSPC? */
	err = -ENOSPC;
	if (sb->s_flags & MS_RDONLY)
		goto fail;

	desc = ext2_get_group_desc (sb, group, &bh2);
	desc->bg_free_inodes_count = 0;
	mark_buffer_dirty(bh2);
	goto repeat;
}

unsigned long ext2_count_free_inodes (struct super_block * sb)
{
#ifdef EXT2FS_DEBUG
	struct ext2_super_block * es;
	unsigned long desc_count = 0, bitmap_count = 0;
	int i;

	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
	for (i = 0; i < sb->u.ext2_sb.s_groups_count; i++) {
		struct ext2_group_desc *desc = ext2_get_group_desc (sb, i, NULL);
		struct buffer_head *bh;
		unsigned x;

		if (!desc)
			continue;
		desc_count += le16_to_cpu(desc->bg_free_inodes_count);
		bh = load_inode_bitmap (sb, i);
		if (IS_ERR(bh))
			continue;

		x = ext2_count_free (bh, EXT2_INODES_PER_GROUP(sb) / 8);
		printk ("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(desc->bg_free_inodes_count), x);
		bitmap_count += x;
	}
	printk("ext2_count_free_inodes: stored = %lu, computed = %lu, %lu\n",
		le32_to_cpu(es->s_free_inodes_count), desc_count, bitmap_count);
	unlock_super (sb);
	return desc_count;
#else
	return le32_to_cpu(sb->u.ext2_sb.s_es->s_free_inodes_count);
#endif
}

#ifdef CONFIG_EXT2_CHECK
/* Called at mount-time, super-block is locked */
void ext2_check_inodes_bitmap (struct super_block * sb)
{
	struct ext2_super_block * es = sb->u.ext2_sb.s_es;
	unsigned long desc_count = 0, bitmap_count = 0;
	int i;

	for (i = 0; i < sb->u.ext2_sb.s_groups_count; i++) {
		struct ext2_group_desc *desc = ext2_get_group_desc(sb, i, NULL);
		struct buffer_head *bh;
		unsigned x;

		if (!desc)
			continue;
		desc_count += le16_to_cpu(desc->bg_free_inodes_count);
		bh = load_inode_bitmap (sb, i);
		if (IS_ERR(bh))
			continue;
		
		x = ext2_count_free (bh, EXT2_INODES_PER_GROUP(sb) / 8);
		if (le16_to_cpu(desc->bg_free_inodes_count) != x)
			ext2_error (sb, "ext2_check_inodes_bitmap",
				    "Wrong free inodes count in group %d, "
				    "stored = %d, counted = %lu", i,
				    le16_to_cpu(desc->bg_free_inodes_count), x);
		bitmap_count += x;
	}
	if (le32_to_cpu(es->s_free_inodes_count) != bitmap_count)
		ext2_error (sb, "ext2_check_inodes_bitmap",
			    "Wrong free inodes count in super block, "
			    "stored = %lu, counted = %lu",
			    (unsigned long)le32_to_cpu(es->s_free_inodes_count),
			    bitmap_count);
}
#endif
