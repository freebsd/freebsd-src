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

#include <linux/fs.h>
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
 * Return >=0 on success or a -ve error code.
 */
static int read_inode_bitmap (struct super_block * sb,
			       unsigned long block_group,
			       unsigned int bitmap_nr)
{
	struct ext2_group_desc * gdp;
	struct buffer_head * bh = NULL;
	int retval = 0;

	gdp = ext2_get_group_desc (sb, block_group, NULL);
	if (!gdp) {
		retval = -EIO;
		goto error_out;
	}
	bh = bread (sb->s_dev, le32_to_cpu(gdp->bg_inode_bitmap), sb->s_blocksize);
	if (!bh) {
		ext2_error (sb, "read_inode_bitmap",
			    "Cannot read inode bitmap - "
			    "block_group = %lu, inode_bitmap = %lu",
			    block_group, (unsigned long) gdp->bg_inode_bitmap);
		retval = -EIO;
	}
	/*
	 * On IO error, just leave a zero in the superblock's block pointer for
	 * this group.  The IO will be retried next time.
	 */
error_out:
	sb->u.ext2_sb.s_inode_bitmap_number[bitmap_nr] = block_group;
	sb->u.ext2_sb.s_inode_bitmap[bitmap_nr] = bh;
	return retval;
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
 * Return the slot used to store the bitmap, or a -ve error code.
 */
static int load_inode_bitmap (struct super_block * sb,
			      unsigned int block_group)
{
	int i, j, retval = 0;
	unsigned long inode_bitmap_number;
	struct buffer_head * inode_bitmap;

	if (block_group >= sb->u.ext2_sb.s_groups_count)
		ext2_panic (sb, "load_inode_bitmap",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			     block_group, sb->u.ext2_sb.s_groups_count);
	if (sb->u.ext2_sb.s_loaded_inode_bitmaps > 0 &&
	    sb->u.ext2_sb.s_inode_bitmap_number[0] == block_group &&
	    sb->u.ext2_sb.s_inode_bitmap[0] != NULL)
		return 0;
	if (sb->u.ext2_sb.s_groups_count <= EXT2_MAX_GROUP_LOADED) {
		if (sb->u.ext2_sb.s_inode_bitmap[block_group]) {
			if (sb->u.ext2_sb.s_inode_bitmap_number[block_group] != block_group)
				ext2_panic (sb, "load_inode_bitmap",
					    "block_group != inode_bitmap_number");
			else
				return block_group;
		} else {
			retval = read_inode_bitmap (sb, block_group,
						    block_group);
			if (retval < 0)
				return retval;
			return block_group;
		}
	}

	for (i = 0; i < sb->u.ext2_sb.s_loaded_inode_bitmaps &&
		    sb->u.ext2_sb.s_inode_bitmap_number[i] != block_group;
	     i++)
		;
	if (i < sb->u.ext2_sb.s_loaded_inode_bitmaps &&
  	    sb->u.ext2_sb.s_inode_bitmap_number[i] == block_group) {
		inode_bitmap_number = sb->u.ext2_sb.s_inode_bitmap_number[i];
		inode_bitmap = sb->u.ext2_sb.s_inode_bitmap[i];
		for (j = i; j > 0; j--) {
			sb->u.ext2_sb.s_inode_bitmap_number[j] =
				sb->u.ext2_sb.s_inode_bitmap_number[j - 1];
			sb->u.ext2_sb.s_inode_bitmap[j] =
				sb->u.ext2_sb.s_inode_bitmap[j - 1];
		}
		sb->u.ext2_sb.s_inode_bitmap_number[0] = inode_bitmap_number;
		sb->u.ext2_sb.s_inode_bitmap[0] = inode_bitmap;

		/*
		 * There's still one special case here --- if inode_bitmap == 0
		 * then our last attempt to read the bitmap failed and we have
		 * just ended up caching that failure.  Try again to read it.
		 */
		if (!inode_bitmap)
			retval = read_inode_bitmap (sb, block_group, 0);
		
	} else {
		if (sb->u.ext2_sb.s_loaded_inode_bitmaps < EXT2_MAX_GROUP_LOADED)
			sb->u.ext2_sb.s_loaded_inode_bitmaps++;
		else
			brelse (sb->u.ext2_sb.s_inode_bitmap[EXT2_MAX_GROUP_LOADED - 1]);
		for (j = sb->u.ext2_sb.s_loaded_inode_bitmaps - 1; j > 0; j--) {
			sb->u.ext2_sb.s_inode_bitmap_number[j] =
				sb->u.ext2_sb.s_inode_bitmap_number[j - 1];
			sb->u.ext2_sb.s_inode_bitmap[j] =
				sb->u.ext2_sb.s_inode_bitmap[j - 1];
		}
		retval = read_inode_bitmap (sb, block_group, 0);
	}
	return retval;
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
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	struct ext2_super_block * es;

	if (!inode->i_dev) {
		printk ("ext2_free_inode: inode has no device\n");
		return;
	}
	if (inode->i_count > 1) {
		printk ("ext2_free_inode: inode has count=%d\n", inode->i_count);
		return;
	}
	if (inode->i_nlink) {
		printk ("ext2_free_inode: inode has nlink=%d\n",
			inode->i_nlink);
		return;
	}
	if (!sb) {
		printk("ext2_free_inode: inode on nonexistent device\n");
		return;
	}

	ino = inode->i_ino;
	ext2_debug ("freeing inode %lu\n", ino);

	/*
	 * Note: we must free any quota before locking the superblock,
	 * as writing the quota to disk may need the lock as well.
	 */
	DQUOT_FREE_INODE(sb, inode);
	DQUOT_DROP(inode);

	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
	if (ino < EXT2_FIRST_INO(sb) || 
	    ino > le32_to_cpu(es->s_inodes_count)) {
		ext2_error (sb, "free_inode",
			    "reserved inode or nonexistent inode");
		goto error_return;
	}
	block_group = (ino - 1) / EXT2_INODES_PER_GROUP(sb);
	bit = (ino - 1) % EXT2_INODES_PER_GROUP(sb);
	bitmap_nr = load_inode_bitmap (sb, block_group);
	if (bitmap_nr < 0)
		goto error_return;
	
	bh = sb->u.ext2_sb.s_inode_bitmap[bitmap_nr];

	is_directory = S_ISDIR(inode->i_mode);

	/* Do this BEFORE marking the inode not in use */
	clear_inode (inode);

	/* Ok, now we can actually update the inode bitmaps.. */
	if (!ext2_clear_bit (bit, bh->b_data))
		ext2_warning (sb, "ext2_free_inode",
			      "bit already cleared for inode %lu", ino);
	else {
		gdp = ext2_get_group_desc (sb, block_group, &bh2);
		if (gdp) {
			gdp->bg_free_inodes_count =
				cpu_to_le16(le16_to_cpu(gdp->bg_free_inodes_count) + 1);
			if (is_directory)
				gdp->bg_used_dirs_count =
					cpu_to_le16(le16_to_cpu(gdp->bg_used_dirs_count) - 1);
		}
		mark_buffer_dirty(bh2, 1);
		es->s_free_inodes_count =
			cpu_to_le32(le32_to_cpu(es->s_free_inodes_count) + 1);
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
	}
	mark_buffer_dirty(bh, 1);
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
struct inode * ext2_new_inode (const struct inode * dir, int mode, int * err)
{
	struct super_block * sb;
	struct buffer_head * bh;
	struct buffer_head * bh2;
	int i, j, avefreei;
	struct inode * inode;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	struct ext2_group_desc * tmp;
	struct ext2_super_block * es;

	/* Cannot create files in a deleted directory */
	if (!dir || !dir->i_nlink) {
		*err = -EPERM;
		return NULL;
	}

	inode = get_empty_inode ();
	if (!inode) {
		*err = -ENOMEM;
		return NULL;
	}

	sb = dir->i_sb;
	inode->i_sb = sb;
	inode->i_flags = 0;
	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
repeat:
	gdp = NULL; i=0;
	
	*err = -ENOSPC;
	if (S_ISDIR(mode)) {
		avefreei = le32_to_cpu(es->s_free_inodes_count) /
			sb->u.ext2_sb.s_groups_count;
/* I am not yet convinced that this next bit is necessary.
		i = dir->u.ext2_i.i_block_group;
		for (j = 0; j < sb->u.ext2_sb.s_groups_count; j++) {
			tmp = ext2_get_group_desc (sb, i, &bh2);
			if (tmp &&
			    (le16_to_cpu(tmp->bg_used_dirs_count) << 8) < 
			     le16_to_cpu(tmp->bg_free_inodes_count)) {
				gdp = tmp;
				break;
			}
			else
			i = ++i % sb->u.ext2_sb.s_groups_count;
		}
*/
		if (!gdp) {
			for (j = 0; j < sb->u.ext2_sb.s_groups_count; j++) {
				tmp = ext2_get_group_desc (sb, j, &bh2);
				if (tmp &&
				    le16_to_cpu(tmp->bg_free_inodes_count) &&
				    le16_to_cpu(tmp->bg_free_inodes_count) >= avefreei) {
					if (!gdp || 
					    (le16_to_cpu(tmp->bg_free_blocks_count) >
					     le16_to_cpu(gdp->bg_free_blocks_count))) {
						i = j;
						gdp = tmp;
					}
				}
			}
		}
	}
	else 
	{
		/*
		 * Try to place the inode in its parent directory
		 */
		i = dir->u.ext2_i.i_block_group;
		tmp = ext2_get_group_desc (sb, i, &bh2);
		if (tmp && le16_to_cpu(tmp->bg_free_inodes_count))
			gdp = tmp;
		else
		{
			/*
			 * Use a quadratic hash to find a group with a
			 * free inode
			 */
			for (j = 1; j < sb->u.ext2_sb.s_groups_count; j <<= 1) {
				i += j;
				if (i >= sb->u.ext2_sb.s_groups_count)
					i -= sb->u.ext2_sb.s_groups_count;
				tmp = ext2_get_group_desc (sb, i, &bh2);
				if (tmp &&
				    le16_to_cpu(tmp->bg_free_inodes_count)) {
					gdp = tmp;
					break;
				}
			}
		}
		if (!gdp) {
			/*
			 * That failed: try linear search for a free inode
			 */
			i = dir->u.ext2_i.i_block_group + 1;
			for (j = 2; j < sb->u.ext2_sb.s_groups_count; j++) {
				if (++i >= sb->u.ext2_sb.s_groups_count)
					i = 0;
				tmp = ext2_get_group_desc (sb, i, &bh2);
				if (tmp &&
				    le16_to_cpu(tmp->bg_free_inodes_count)) {
					gdp = tmp;
					break;
				}
			}
		}
	}

	if (!gdp) {
		unlock_super (sb);
		iput(inode);
		return NULL;
	}
	bitmap_nr = load_inode_bitmap (sb, i);
	if (bitmap_nr < 0) {
		unlock_super (sb);
		iput(inode);
		*err = -EIO;
		return NULL;
	}

	bh = sb->u.ext2_sb.s_inode_bitmap[bitmap_nr];
	if ((j = ext2_find_first_zero_bit ((unsigned long *) bh->b_data,
				      EXT2_INODES_PER_GROUP(sb))) <
	    EXT2_INODES_PER_GROUP(sb)) {
		if (ext2_set_bit (j, bh->b_data)) {
			ext2_warning (sb, "ext2_new_inode",
				      "bit already set for inode %d", j);
			goto repeat;
		}
		mark_buffer_dirty(bh, 1);
		if (sb->s_flags & MS_SYNCHRONOUS) {
			ll_rw_block (WRITE, 1, &bh);
			wait_on_buffer (bh);
		}
	} else {
		if (le16_to_cpu(gdp->bg_free_inodes_count) != 0) {
			ext2_error (sb, "ext2_new_inode",
				    "Free inodes count corrupted in group %d",
				    i);
			unlock_super (sb);
			iput (inode);
			return NULL;
		}
		goto repeat;
	}
	j += i * EXT2_INODES_PER_GROUP(sb) + 1;
	if (j < EXT2_FIRST_INO(sb) || j > le32_to_cpu(es->s_inodes_count)) {
		ext2_error (sb, "ext2_new_inode",
			    "reserved inode or inode > inodes count - "
			    "block_group = %d,inode=%d", i, j);
		unlock_super (sb);
		iput (inode);
		return NULL;
	}
	gdp->bg_free_inodes_count =
		cpu_to_le16(le16_to_cpu(gdp->bg_free_inodes_count) - 1);
	if (S_ISDIR(mode))
		gdp->bg_used_dirs_count =
			cpu_to_le16(le16_to_cpu(gdp->bg_used_dirs_count) + 1);
	mark_buffer_dirty(bh2, 1);
	es->s_free_inodes_count =
		cpu_to_le32(le32_to_cpu(es->s_free_inodes_count) - 1);
	mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
	sb->s_dirt = 1;
	inode->i_mode = mode;
	inode->i_sb = sb;
	inode->i_nlink = 1;
	inode->i_dev = sb->s_dev;
	inode->i_uid = current->fsuid;
	if (test_opt (sb, GRPID))
		inode->i_gid = dir->i_gid;
	else if (dir->i_mode & S_ISGID) {
		inode->i_gid = dir->i_gid;
		if (S_ISDIR(mode))
			mode |= S_ISGID;
	} else
		inode->i_gid = current->fsgid;

	inode->i_ino = j;
	inode->i_blksize = PAGE_SIZE;	/* This is the optimal IO size (for stat), not the fs block size */
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->u.ext2_i.i_new_inode = 1;
	inode->u.ext2_i.i_flags = dir->u.ext2_i.i_flags;
	if (S_ISLNK(mode))
		inode->u.ext2_i.i_flags &= ~(EXT2_IMMUTABLE_FL | EXT2_APPEND_FL);
	inode->u.ext2_i.i_faddr = 0;
	inode->u.ext2_i.i_frag_no = 0;
	inode->u.ext2_i.i_frag_size = 0;
	inode->u.ext2_i.i_file_acl = 0;
	inode->u.ext2_i.i_dir_acl = 0;
	inode->u.ext2_i.i_dtime = 0;
	inode->u.ext2_i.i_block_group = i;
	inode->i_op = NULL;
	if (inode->u.ext2_i.i_flags & EXT2_SYNC_FL)
		inode->i_flags |= MS_SYNCHRONOUS;
	insert_inode_hash(inode);
	inode->i_generation = event++;
	mark_inode_dirty(inode);

	unlock_super (sb);
	if(DQUOT_ALLOC_INODE(sb, inode)) {
		sb->dq_op->drop(inode);
		inode->i_nlink = 0;
		iput(inode);
		*err = -EDQUOT;
		return NULL;
	}
	ext2_debug ("allocating inode %lu\n", inode->i_ino);

	*err = 0;
	return inode;
}

unsigned long ext2_count_free_inodes (struct super_block * sb)
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
		desc_count += le16_to_cpu(gdp->bg_free_inodes_count);
		bitmap_nr = load_inode_bitmap (sb, i);
		if (bitmap_nr < 0)
			continue;

		x = ext2_count_free (sb->u.ext2_sb.s_inode_bitmap[bitmap_nr],
				     EXT2_INODES_PER_GROUP(sb) / 8);
		printk ("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(gdp->bg_free_inodes_count), x);
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

void ext2_check_inodes_bitmap (struct super_block * sb)
{
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
		desc_count += le16_to_cpu(gdp->bg_free_inodes_count);
		bitmap_nr = load_inode_bitmap (sb, i);
		if (bitmap_nr < 0)
			continue;
		
		x = ext2_count_free (sb->u.ext2_sb.s_inode_bitmap[bitmap_nr],
				     EXT2_INODES_PER_GROUP(sb) / 8);
		if (le16_to_cpu(gdp->bg_free_inodes_count) != x)
			ext2_error (sb, "ext2_check_inodes_bitmap",
				    "Wrong free inodes count in group %d, "
				    "stored = %d, counted = %lu", i,
				    le16_to_cpu(gdp->bg_free_inodes_count), x);
		bitmap_count += x;
	}
	if (le32_to_cpu(es->s_free_inodes_count) != bitmap_count)
		ext2_error (sb, "ext2_check_inodes_bitmap",
			    "Wrong free inodes count in super block, "
			    "stored = %lu, counted = %lu",
			    (unsigned long) le32_to_cpu(es->s_free_inodes_count),
			    bitmap_count);
	unlock_super (sb);
}
