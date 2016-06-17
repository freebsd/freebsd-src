/*
 *	fs/bfs/file.c
 *	BFS file operations.
 *	Copyright (C) 1999,2000 Tigran Aivazian <tigran@veritas.com>
 */

#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/bfs_fs.h>
#include <linux/smp_lock.h>
#include "bfs_defs.h"

#undef DEBUG

#ifdef DEBUG
#define dprintf(x...)	printf(x)
#else
#define dprintf(x...)
#endif

struct file_operations bfs_file_operations = {
	llseek: generic_file_llseek,
	read:	generic_file_read,
	write:	generic_file_write,
	mmap:	generic_file_mmap,
};

static int bfs_move_block(unsigned long from, unsigned long to, kdev_t dev)
{
	struct buffer_head *bh, *new;

	bh = bread(dev, from, BFS_BSIZE);
	if (!bh)
		return -EIO;
	new = getblk(dev, to, BFS_BSIZE);
	memcpy(new->b_data, bh->b_data, bh->b_size);
	mark_buffer_dirty(new);
	bforget(bh);
	brelse(new);
	return 0;
}

static int bfs_move_blocks(kdev_t dev, unsigned long start, unsigned long end, 
				unsigned long where)
{
	unsigned long i;

	dprintf("%08lx-%08lx->%08lx\n", start, end, where);
	for (i = start; i <= end; i++)
		if(bfs_move_block(i, where + i, dev)) {
			dprintf("failed to move block %08lx -> %08lx\n", i, where + i);
			return -EIO;
		}
	return 0;
}

static int bfs_get_block(struct inode * inode, long block, 
	struct buffer_head * bh_result, int create)
{
	long phys;
	int err;
	struct super_block *sb = inode->i_sb;
	struct buffer_head *sbh = sb->su_sbh;

	if (block < 0 || block > sb->su_blocks)
		return -EIO;

	phys = inode->iu_sblock + block;
	if (!create) {
		if (phys <= inode->iu_eblock) {
			dprintf("c=%d, b=%08lx, phys=%08lx (granted)\n", create, block, phys);
			bh_result->b_dev = inode->i_dev;
			bh_result->b_blocknr = phys;
			bh_result->b_state |= (1UL << BH_Mapped);
		}
		return 0;
	}

	/* if the file is not empty and the requested block is within the range
	   of blocks allocated for this file, we can grant it */
	if (inode->i_size && phys <= inode->iu_eblock) {
		dprintf("c=%d, b=%08lx, phys=%08lx (interim block granted)\n", 
				create, block, phys);
		bh_result->b_dev = inode->i_dev;
		bh_result->b_blocknr = phys;
		bh_result->b_state |= (1UL << BH_Mapped);
		return 0;
	}

	/* the rest has to be protected against itself */
	lock_kernel();

	/* if the last data block for this file is the last allocated block, we can
	   extend the file trivially, without moving it anywhere */
	if (inode->iu_eblock == sb->su_lf_eblk) {
		dprintf("c=%d, b=%08lx, phys=%08lx (simple extension)\n", 
				create, block, phys);
		bh_result->b_dev = inode->i_dev;
		bh_result->b_blocknr = phys;
		bh_result->b_state |= (1UL << BH_Mapped);
		sb->su_freeb -= phys - inode->iu_eblock;
		sb->su_lf_eblk = inode->iu_eblock = phys;
		mark_inode_dirty(inode);
		mark_buffer_dirty(sbh);
		err = 0;
		goto out;
	}

	/* Ok, we have to move this entire file to the next free block */
	phys = sb->su_lf_eblk + 1;
	if (inode->iu_sblock) { /* if data starts on block 0 then there is no data */
		err = bfs_move_blocks(inode->i_dev, inode->iu_sblock, 
				inode->iu_eblock, phys);
		if (err) {
			dprintf("failed to move ino=%08lx -> fs corruption\n", inode->i_ino);
			goto out;
		}
	} else
		err = 0;

	dprintf("c=%d, b=%08lx, phys=%08lx (moved)\n", create, block, phys);
	inode->iu_sblock = phys;
	phys += block;
	sb->su_lf_eblk = inode->iu_eblock = phys;

	/* this assumes nothing can write the inode back while we are here
	 * and thus update inode->i_blocks! (XXX)*/
	sb->su_freeb -= inode->iu_eblock - inode->iu_sblock + 1 - inode->i_blocks;
	mark_inode_dirty(inode);
	mark_buffer_dirty(sbh);
	bh_result->b_dev = inode->i_dev;
	bh_result->b_blocknr = phys;
	bh_result->b_state |= (1UL << BH_Mapped);
out:
	unlock_kernel();
	return err;
}

static int bfs_writepage(struct page *page)
{
	return block_write_full_page(page, bfs_get_block);
}

static int bfs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, bfs_get_block);
}

static int bfs_prepare_write(struct file *file, struct page *page, unsigned from, unsigned to)
{
	return block_prepare_write(page, from, to, bfs_get_block);
}

static int bfs_bmap(struct address_space *mapping, long block)
{
	return generic_block_bmap(mapping, block, bfs_get_block);
}

struct address_space_operations bfs_aops = {
	readpage:	bfs_readpage,
	writepage:	bfs_writepage,
	sync_page:	block_sync_page,
	prepare_write:	bfs_prepare_write,
	commit_write:	generic_commit_write,
	bmap:		bfs_bmap,
};

struct inode_operations bfs_file_inops;
