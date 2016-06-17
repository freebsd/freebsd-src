/*
 * linux/fs/befs/io.c
 *
 * Copyright (C) 2001 Will Dyson <will_dyson@pobox.com
 *
 * Based on portions of file.c and inode.c 
 * by Makoto Kato (m_kato@ga2.so-net.ne.jp)
 *
 * Many thanks to Dominic Giampaolo, author of Practical File System
 * Design with the Be File System, for such a helpful book.
 *
 */

#include <linux/fs.h>

#include "befs.h"
#include "io.h"

/*
 * Converts befs notion of disk addr to a disk offset and uses
 * linux kernel function bread() to get the buffer containing
 * the offset. -Will Dyson
 *
 */

struct buffer_head *
befs_bread_iaddr(struct super_block *sb, befs_inode_addr iaddr)
{
	struct buffer_head *bh = NULL;
	befs_blocknr_t block = 0;
	vfs_blocknr_t vfs_block = 0;
	befs_sb_info *befs_sb = BEFS_SB(sb);

	befs_debug(sb, "---> Enter befs_read_iaddr() "
		   "[%u, %hu, %hu]",
		   iaddr.allocation_group, iaddr.start, iaddr.len);

	if (iaddr.allocation_group > befs_sb->num_ags) {
		befs_error(sb, "BEFS: Invalid allocation group %u, max is %u",
			   iaddr.allocation_group, befs_sb->num_ags);
		goto error;
	}

	block = iaddr2blockno(sb, &iaddr);
	vfs_block = (vfs_blocknr_t) block;

	if (vfs_block != block) {
		befs_error(sb, "Error converting to host blocknr_t. %Lu "
			   "is larger than the host can use", block);
		goto error;
	}

	befs_debug(sb, "befs_read_iaddr: offset = %lu", block);

	bh = bread(sb->s_dev, vfs_block, befs_sb->block_size);

	if (bh == NULL) {
		befs_error(sb, "Failed to read block %lu", block);
		goto error;
	}

	befs_debug(sb, "<--- befs_read_iaddr()");
	return bh;

      error:
	befs_debug(sb, "<--- befs_read_iaddr() ERROR");
	return NULL;
}

struct buffer_head *
befs_bread(struct super_block *sb, befs_blocknr_t block)
{
	struct buffer_head *bh = NULL;
	befs_sb_info *befs_sb = BEFS_SB(sb);
	vfs_blocknr_t vfs_block = (vfs_blocknr_t) block;

	befs_debug(sb, "---> Enter befs_read() %Lu", block);

	if (vfs_block != block) {
		befs_error(sb, "Error converting to host blocknr_t. %Lu "
			   "is larger than the host can use", block);
		goto error;
	}

	bh = bread(sb->s_dev, vfs_block, befs_sb->block_size);

	if (bh == NULL) {
		befs_error(sb, "Failed to read block %lu", vfs_block);
		goto error;
	}

	befs_debug(sb, "<--- befs_read()");

	return bh;

      error:
	befs_debug(sb, "<--- befs_read() ERROR");
	return NULL;
}
