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
 */

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

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/locks.h>

#include <asm/bitops.h>

static struct ext2_group_desc * get_group_desc (struct super_block * sb,
						unsigned int block_group,
						struct buffer_head ** bh)
{
	unsigned long group_desc;
	unsigned long desc;
	struct ext2_group_desc * gdp;

	if (block_group >= sb->u.ext2_sb.s_groups_count)
		ext2_panic (sb, "get_group_desc",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sb->u.ext2_sb.s_groups_count);

	group_desc = block_group / EXT2_DESC_PER_BLOCK(sb);
	desc = block_group % EXT2_DESC_PER_BLOCK(sb);
	if (!sb->u.ext2_sb.s_group_desc[group_desc])
		ext2_panic (sb, "get_group_desc",
			    "Group descriptor not loaded - "
			    "block_group = %d, group_desc = %lu, desc = %lu",
			     block_group, group_desc, desc);
	gdp = (struct ext2_group_desc *) 
		sb->u.ext2_sb.s_group_desc[group_desc]->b_data;
	if (bh)
		*bh = sb->u.ext2_sb.s_group_desc[group_desc];
	return gdp + desc;
}

static void read_inode_bitmap (struct super_block * sb,
			       unsigned long block_group,
			       unsigned int bitmap_nr)
{
	struct ext2_group_desc * gdp;
	struct buffer_head * bh;

	gdp = get_group_desc (sb, block_group, NULL);
	bh = bread (sb->s_dev, gdp->bg_inode_bitmap, sb->s_blocksize);
	if (!bh)
		ext2_panic (sb, "read_inode_bitmap",
			    "Cannot read inode bitmap - "
			    "block_group = %lu, inode_bitmap = %lu",
			    block_group, (unsigned long) gdp->bg_inode_bitmap);
	sb->u.ext2_sb.s_inode_bitmap_number[bitmap_nr] = block_group;
	sb->u.ext2_sb.s_inode_bitmap[bitmap_nr] = bh;
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
 */
static int load_inode_bitmap (struct super_block * sb,
			      unsigned int block_group)
{
	int i, j;
	unsigned long inode_bitmap_number;
	struct buffer_head * inode_bitmap;

	if (block_group >= sb->u.ext2_sb.s_groups_count)
		ext2_panic (sb, "load_inode_bitmap",
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			     block_group, sb->u.ext2_sb.s_groups_count);
	if (sb->u.ext2_sb.s_loaded_inode_bitmaps > 0 &&
	    sb->u.ext2_sb.s_inode_bitmap_number[0] == block_group)
		return 0;
	if (sb->u.ext2_sb.s_groups_count <= EXT2_MAX_GROUP_LOADED) {
		if (sb->u.ext2_sb.s_inode_bitmap[block_group]) {
			if (sb->u.ext2_sb.s_inode_bitmap_number[block_group] != block_group)
				ext2_panic (sb, "load_inode_bitmap",
					    "block_group != inode_bitmap_number");
			else
				return block_group;
		} else {
			read_inode_bitmap (sb, block_group, block_group);
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
		read_inode_bitmap (sb, block_group, 0);
	}
	return 0;
}

/*
 * This function sets the deletion time for the inode
 *
 * This may be used one day by an 'undelete' program
 */
static void set_inode_dtime (struct inode * inode,
			     struct ext2_group_desc * gdp)
{
	unsigned long inode_block;
	struct buffer_head * bh;
	struct ext2_inode * raw_inode;

	inode_block = gdp->bg_inode_table + (((inode->i_ino - 1) %
			EXT2_INODES_PER_GROUP(inode->i_sb)) /
			EXT2_INODES_PER_BLOCK(inode->i_sb));
	bh = bread (inode->i_sb->s_dev, inode_block, inode->i_sb->s_blocksize);
	if (!bh)
		ext2_panic (inode->i_sb, "set_inode_dtime",
			    "Cannot load inode table block - "
			    "inode=%lu, inode_block=%lu",
			    inode->i_ino, inode_block);
	raw_inode = ((struct ext2_inode *) bh->b_data) +
			(((inode->i_ino - 1) %
			EXT2_INODES_PER_GROUP(inode->i_sb)) %
			EXT2_INODES_PER_BLOCK(inode->i_sb));
	raw_inode->i_links_count = 0;
	raw_inode->i_dtime = CURRENT_TIME;
	mark_buffer_dirty(bh, 1);
	if (IS_SYNC(inode)) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}
	brelse (bh);
}

void ext2_free_inode (struct inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	struct buffer_head * bh2;
	unsigned long block_group;
	unsigned long bit;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	struct ext2_super_block * es;

	if (!inode)
		return;
	if (!inode->i_dev) {
		printk ("ext2_free_inode: inode has no device\n");
		return;
	}
	if (inode->i_count > 1) {
		printk ("ext2_free_inode: inode has count=%d\n",
			inode->i_count);
		return;
	}
	if (inode->i_nlink) {
		printk ("ext2_free_inode: inode has nlink=%d\n",
			inode->i_nlink);
		return;
	}
	if (!inode->i_sb) {
		printk("ext2_free_inode: inode on nonexistent device\n");
		return;
	}

	ext2_debug ("freeing inode %lu\n", inode->i_ino);

	sb = inode->i_sb;
	lock_super (sb);
	if (inode->i_ino < EXT2_FIRST_INO ||
	    inode->i_ino > sb->u.ext2_sb.s_es->s_inodes_count) {
		ext2_error (sb, "free_inode",
			    "reserved inode or nonexistent inode");
		unlock_super (sb);
		return;
	}
	es = sb->u.ext2_sb.s_es;
	block_group = (inode->i_ino - 1) / EXT2_INODES_PER_GROUP(sb);
	bit = (inode->i_ino - 1) % EXT2_INODES_PER_GROUP(sb);
	bitmap_nr = load_inode_bitmap (sb, block_group);
	bh = sb->u.ext2_sb.s_inode_bitmap[bitmap_nr];
	if (!clear_bit (bit, bh->b_data))
		ext2_warning (sb, "ext2_free_inode",
			      "bit already cleared for inode %lu", inode->i_ino);
	else {
		gdp = get_group_desc (sb, block_group, &bh2);
		gdp->bg_free_inodes_count++;
		if (S_ISDIR(inode->i_mode))
			gdp->bg_used_dirs_count--;
		mark_buffer_dirty(bh2, 1);
		es->s_free_inodes_count++;
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
		set_inode_dtime (inode, gdp);
	}
	mark_buffer_dirty(bh, 1);
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer (bh);
	}

	sb->s_dirt = 1;
	clear_inode (inode);
	unlock_super (sb);
}

/*
 * This function increments the inode version number
 *
 * This may be used one day by the NFS server
 */
static void inc_inode_version (struct inode * inode,
			       struct ext2_group_desc *gdp,
			       int mode)
{
	unsigned long inode_block;
	struct buffer_head * bh;
	struct ext2_inode * raw_inode;

	inode_block = gdp->bg_inode_table + (((inode->i_ino - 1) %
			EXT2_INODES_PER_GROUP(inode->i_sb)) /
			EXT2_INODES_PER_BLOCK(inode->i_sb));
	bh = bread (inode->i_sb->s_dev, inode_block, inode->i_sb->s_blocksize);
	if (!bh) {
		ext2_error (inode->i_sb, "inc_inode_version",
			    "Cannot load inode table block - "
			    "inode=%lu, inode_block=%lu\n",
			    inode->i_ino, inode_block);
		inode->u.ext2_i.i_version = 1;
		return;
	}
	raw_inode = ((struct ext2_inode *) bh->b_data) +
			(((inode->i_ino - 1) %
			EXT2_INODES_PER_GROUP(inode->i_sb)) %
			EXT2_INODES_PER_BLOCK(inode->i_sb));
	raw_inode->i_version++;
	inode->u.ext2_i.i_version = raw_inode->i_version;
	mark_buffer_dirty(bh, 1);
	brelse (bh);
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
struct inode * ext2_new_inode (const struct inode * dir, int mode)
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

	if (!dir || !(inode = get_empty_inode ()))
		return NULL;
	sb = dir->i_sb;
	inode->i_sb = sb;
	inode->i_flags = sb->s_flags;
	lock_super (sb);
	es = sb->u.ext2_sb.s_es;
repeat:
	gdp = NULL; i=0;
	
	if (S_ISDIR(mode)) {
		avefreei = es->s_free_inodes_count /
			sb->u.ext2_sb.s_groups_count;
/* I am not yet convinced that this next bit is necessary.
		i = dir->u.ext2_i.i_block_group;
		for (j = 0; j < sb->u.ext2_sb.s_groups_count; j++) {
			tmp = get_group_desc (sb, i, &bh2);
			if ((tmp->bg_used_dirs_count << 8) < 
			    tmp->bg_free_inodes_count) {
				gdp = tmp;
				break;
			}
			else
			i = ++i % sb->u.ext2_sb.s_groups_count;
		}
*/
		if (!gdp) {
			for (j = 0; j < sb->u.ext2_sb.s_groups_count; j++) {
				tmp = get_group_desc (sb, j, &bh2);
				if (tmp->bg_free_inodes_count &&
					tmp->bg_free_inodes_count >= avefreei) {
					if (!gdp || 
					    (tmp->bg_free_blocks_count >
					     gdp->bg_free_blocks_count)) {
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
		tmp = get_group_desc (sb, i, &bh2);
		if (tmp->bg_free_inodes_count)
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
				tmp = get_group_desc (sb, i, &bh2);
				if (tmp->bg_free_inodes_count) {
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
				tmp = get_group_desc (sb, i, &bh2);
				if (tmp->bg_free_inodes_count) {
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
	bh = sb->u.ext2_sb.s_inode_bitmap[bitmap_nr];
	if ((j = find_first_zero_bit ((unsigned long *) bh->b_data,
				      EXT2_INODES_PER_GROUP(sb))) <
	    EXT2_INODES_PER_GROUP(sb)) {
		if (set_bit (j, bh->b_data)) {
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
		if (gdp->bg_free_inodes_count != 0) {
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
	if (j < EXT2_FIRST_INO || j > es->s_inodes_count) {
		ext2_error (sb, "ext2_new_inode",
			    "reserved inode or inode > inodes count - "
			    "block_group = %d,inode=%d", i, j);
		unlock_super (sb);
		iput (inode);
		return NULL;
	}
	gdp->bg_free_inodes_count--;
	if (S_ISDIR(mode))
		gdp->bg_used_dirs_count++;
	mark_buffer_dirty(bh2, 1);
	es->s_free_inodes_count--;
	mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
	sb->s_dirt = 1;
	inode->i_mode = mode;
	inode->i_sb = sb;
	inode->i_count = 1;
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
	inode->i_dirt = 1;
	inode->i_ino = j;
	inode->i_blksize = sb->s_blocksize;
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
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
	inc_inode_version (inode, gdp, mode);

	ext2_debug ("allocating inode %lu\n", inode->i_ino);

	unlock_super (sb);
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
		gdp = get_group_desc (sb, i, NULL);
		desc_count += gdp->bg_free_inodes_count;
		bitmap_nr = load_inode_bitmap (sb, i);
		x = ext2_count_free (sb->u.ext2_sb.s_inode_bitmap[bitmap_nr],
				     EXT2_INODES_PER_GROUP(sb) / 8);
		printk ("group %d: stored = %d, counted = %lu\n",
			i, gdp->bg_free_inodes_count, x);
		bitmap_count += x;
	}
	printk("ext2_count_free_inodes: stored = %lu, computed = %lu, %lu\n",
		es->s_free_inodes_count, desc_count, bitmap_count);
	unlock_super (sb);
	return desc_count;
#else
	return sb->u.ext2_sb.s_es->s_free_inodes_count;
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
		gdp = get_group_desc (sb, i, NULL);
		desc_count += gdp->bg_free_inodes_count;
		bitmap_nr = load_inode_bitmap (sb, i);
		x = ext2_count_free (sb->u.ext2_sb.s_inode_bitmap[bitmap_nr],
				     EXT2_INODES_PER_GROUP(sb) / 8);
		if (gdp->bg_free_inodes_count != x)
			ext2_error (sb, "ext2_check_inodes_bitmap",
				    "Wrong free inodes count in group %d, "
				    "stored = %d, counted = %lu", i,
				    gdp->bg_free_inodes_count, x);
		bitmap_count += x;
	}
	if (es->s_free_inodes_count != bitmap_count)
		ext2_error (sb, "ext2_check_inodes_bitmap",
			    "Wrong free inodes count in super block, "
			    "stored = %lu, counted = %lu",
			    (unsigned long) es->s_free_inodes_count, bitmap_count);
	unlock_super (sb);
}
