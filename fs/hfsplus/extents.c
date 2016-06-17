/*
 *  linux/fs/hfsplus/extents.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Handling of Extents both in catalog and extents overflow trees
 */

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/buffer_head.h>
#endif

#include "hfsplus_fs.h"
#include "hfsplus_raw.h"

/* Compare two extents keys, returns 0 on same, pos/neg for difference */
int hfsplus_cmp_ext_key(hfsplus_btree_key *k1, hfsplus_btree_key *k2)
{
	u32 k1id, k2id;
	u32 k1s, k2s;

	k1id = k1->ext.cnid;
	k2id = k2->ext.cnid;
	if (k1id != k2id)
		return be32_to_cpu(k1id) < be32_to_cpu(k2id) ? -1 : 1;

	if (k1->ext.fork_type != k2->ext.fork_type)
		return k1->ext.fork_type < k2->ext.fork_type ? -1 : 1;

	k1s = k1->ext.start_block;
	k2s = k2->ext.start_block;
	if (k1s == k2s)
		return 0;
	return be32_to_cpu(k1s) < be32_to_cpu(k2s) ? -1 : 1;
}

void hfsplus_fill_ext_key(hfsplus_btree_key *key, u32 cnid,
			  u32 block, u8 type)
{
	key->key_len = cpu_to_be16(HFSPLUS_EXT_KEYLEN - 2);
	key->ext.cnid = cpu_to_be32(cnid);
	key->ext.start_block = cpu_to_be32(block);
	key->ext.fork_type = type;
	key->ext.pad = 0;
}

static u32 hfsplus_find_extent(hfsplus_extent *extent, u32 off)
{
	int i;
	u32 count;

	for (i = 0; i < 8; extent++, i++) {
		count = be32_to_cpu(extent->block_count);
		if (off < count)
			return be32_to_cpu(extent->start_block) + off;
		off -= count;
	}
	/* panic? */
	return 0;
}

static int hfsplus_find_extentry(struct hfsplus_find_data *fd,
				 hfsplus_extent *extent)
{
	int res;

	fd->key->ext.cnid = 0;
	res = hfsplus_btree_find(fd);
	if (res && res != -ENOENT)
		return res;
	if (fd->key->ext.cnid != fd->search_key->ext.cnid ||
	    fd->key->ext.fork_type != fd->search_key->ext.fork_type)
		return -ENOENT;
	//if (fd->entrylength != ext_key_size)...
	hfsplus_bnode_readbytes(fd->bnode, extent, fd->entryoffset, fd->entrylength);
	return 0;
}

/* Get a block at iblock for inode, possibly allocating if create */
int hfsplus_get_block(struct inode *inode, sector_t iblock,
		      struct buffer_head *bh_result, int create)
{
	struct super_block *sb;
	hfsplus_extent_rec ext_entry;
	struct hfsplus_find_data fd;
	int err = -EIO;
	u32 ablock, dblock = 0;

	sb = inode->i_sb;

	/* Convert inode block to disk allocation block */
	ablock = iblock;

	if (ablock >= HFSPLUS_I(inode).total_blocks) {
		if (ablock > HFSPLUS_I(inode).total_blocks || !create)
			return -EIO;
		if (ablock >= HFSPLUS_I(inode).alloc_blocks) {
			err = hfsplus_extend_file(inode);
			if (err)
				return err;
		}
		HFSPLUS_I(inode).mmu_private += sb->s_blocksize;
		HFSPLUS_I(inode).total_blocks++;
		mark_inode_dirty(inode);
	} else
		create = 0;

	if (ablock < HFSPLUS_I(inode).extent_blocks) {
		dblock = hfsplus_find_extent(HFSPLUS_I(inode).extents, ablock);
	} else {
		hfsplus_find_init(HFSPLUS_SB(sb).ext_tree, &fd);
		hfsplus_fill_ext_key(fd.search_key, inode->i_ino, ablock, HFSPLUS_IS_RSRC(inode) ?
				     HFSPLUS_TYPE_RSRC : HFSPLUS_TYPE_DATA);
		err = hfsplus_find_extentry(&fd, ext_entry);
		if (!err)
			dblock = hfsplus_find_extent(ext_entry, ablock -
						    be32_to_cpu(fd.key->ext.start_block));
		hfsplus_find_exit(&fd);
		if (err)
			return err;
	}

	if (!dblock)
		return -EIO;
	dprint(DBG_EXTENT, "get_block(%lu): %lu - %u\n", inode->i_ino, iblock, dblock);

	map_bh(bh_result, sb, dblock + HFSPLUS_SB(sb).blockoffset);
	if (create)
		set_buffer_new(bh_result);
	return 0;
}

static void hfsplus_dump_extent(hfsplus_extent *extent)
{
	int i;

	dprint(DBG_EXTENT, "   ");
	for (i = 0; i < 8; i++)
		dprint(DBG_EXTENT, " %u:%u", be32_to_cpu(extent[i].start_block),
				 be32_to_cpu(extent[i].block_count));
	dprint(DBG_EXTENT, "\n");
}

static int hfsplus_add_extent(hfsplus_extent *extent, u32 offset,
			      u32 alloc_block, u32 block_count)
{
	u32 count, start;
	int i;

	hfsplus_dump_extent(extent);
	for (i = 0; i < 8; extent++, i++) {
		count = be32_to_cpu(extent->block_count);
		if (offset == count) {
			start = be32_to_cpu(extent->start_block);
			if (alloc_block != start + count) {
				if (++i >= 8)
					return -ENOSPC;
				extent++;
				extent->start_block = cpu_to_be32(alloc_block);
			} else
				block_count += count;
			extent->block_count = cpu_to_be32(block_count);
			return 0;
		} else if (offset < count)
			break;
		offset -= count;
	}
	/* panic? */
	return -EIO;
}

void hfsplus_free_blocks(struct super_block *sb, u32 start, u32 count)
{
	struct inode *anode;
	struct buffer_head *bh;
	int size, blk, off;
	unsigned long *data, word, m;

	anode = HFSPLUS_SB(sb).alloc_file;
	size = sb->s_blocksize / sizeof(unsigned long);
	blk = (start >> sb->s_blocksize_bits) / 8;
	off = (start & (sb->s_blocksize * 8 - 1)) / 8 / sizeof(unsigned long);
	m = 1 << (~start & (8 * sizeof(unsigned long) - 1));

	HFSPLUS_SB(sb).free_blocks += count;
	sb->s_dirt = 1;

	while (count) {
		bh = hfsplus_getblk(anode, blk);
		data = (unsigned long *)bh->b_data;
		do {
			word = ~be32_to_cpu(data[off]);
			for (;;) {
				if (word & m)
					printk("freeing free block %u\n", start);
				word |= m;
				start++;
				if (!--count) {
					data[off] = cpu_to_be32(~word);
					goto done;
				}
				if (!(m >>= 1))
					break;
			}
			data[off] = cpu_to_be32(~word);
			m = 1UL << (8 * sizeof(unsigned long) - 1);
		} while (++off < size);
	done:
		mark_buffer_dirty_inode(bh, anode);
		brelse(bh);
		if (++blk >= anode->i_blocks)
			break;
		off = 0;
	}
	if (count)
		printk("%u block left to free\n", count);
}

int hfsplus_free_extents(struct super_block *sb, hfsplus_extent *extent,
			 u32 offset, u32 block_nr)
{
	u32 count, start;
	int i;

	hfsplus_dump_extent(extent);
	for (i = 0; i < 8; extent++, i++) {
		count = be32_to_cpu(extent->block_count);
		if (offset == count)
			goto found;
		else if (offset < count)
			break;
		offset -= count;
	}
	/* panic? */
	return -EIO;
found:
	for (;;) {
		start = be32_to_cpu(extent->start_block);
		if (count <= block_nr) {
			hfsplus_free_blocks(sb, start, count);
			extent->block_count = 0;
			extent->start_block = 0;
			block_nr -= count;
		} else {
			count -= block_nr;
			hfsplus_free_blocks(sb, start + count, block_nr);
			extent->block_count = cpu_to_be32(count);
			block_nr = 0;
		}
		if (!block_nr || !i)
			return 0;
		i--;
		extent--;
		count = be32_to_cpu(extent->block_count);
	}
}

int hfsplus_free_fork(struct super_block *sb, u32 cnid, hfsplus_fork_raw *fork, int type)
{
	struct hfsplus_find_data fd;
	hfsplus_extent_rec ext_entry;
	u32 total_blocks, blocks, start;
	int res, i;

	total_blocks = be32_to_cpu(fork->total_blocks);
	if (!total_blocks)
		return 0;

	blocks = 0;
	for (i = 0; i < 8; i++)
		blocks += be32_to_cpu(fork->extents[i].block_count);

	res = hfsplus_free_extents(sb, fork->extents, blocks, blocks);
	if (res)
		return res;
	if (total_blocks == blocks)
		return 0;

	hfsplus_find_init(HFSPLUS_SB(sb).ext_tree, &fd);
	do {
		hfsplus_fill_ext_key(fd.search_key, cnid,
				     total_blocks, type);
		res = hfsplus_find_extentry(&fd, ext_entry);
		if (res)
			break;
		start = be32_to_cpu(fd.key->ext.start_block);
		hfsplus_free_extents(sb, ext_entry,
				     total_blocks - start,
				     total_blocks);
		hfsplus_bnode_remove_rec(&fd);
		total_blocks = start;
	} while (total_blocks > blocks);
	hfsplus_find_exit(&fd);

	return res;
}

int hfsplus_extend_file(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct inode *anode = HFSPLUS_SB(sb).alloc_file;
	struct hfsplus_find_data fd;
	hfsplus_extent_rec ext_entry;
	struct buffer_head *bh;
	u32 blk, blk1, ablock, block_count;
	unsigned long *data, word, m;
	int res, size, off, i;

	if (anode->i_size * 8 < HFSPLUS_SB(sb).total_blocks - HFSPLUS_SB(sb).free_blocks + 8) {
		// extend alloc file
		BUG();
	}
	blk = blk1 = (HFSPLUS_SB(sb).next_alloc >> sb->s_blocksize_bits) / 8;
	off = (HFSPLUS_SB(sb).next_alloc & (sb->s_blocksize * 8 - 1)) / sizeof(unsigned long) / 8;

	for (;;) {
		// ignore first off for now...
		off = 0;
		bh = hfsplus_getblk(anode, blk);
		if (blk == anode->i_blocks - 1)
			size = (HFSPLUS_SB(sb).total_blocks &
				(sb->s_blocksize * 8 - 1)) /
				8 / sizeof(unsigned long);
		else
			size = sb->s_blocksize / sizeof(unsigned long);
		data = (unsigned long *)bh->b_data;
		do {
			word = be32_to_cpu(data[off]);
			if (!~word)
				continue;
			m = 1UL << (sizeof(unsigned long) * 8 - 1);
			for (i = 0; m; i++, m >>= 1) {
				if (word & m)
					continue;
				ablock = (blk << sb->s_blocksize_bits) * 8 +
					off * sizeof(unsigned long) * 8 + i;
				block_count = 1;
				word |= m;
#if 0
				while ((m >>= 1) && !(word & m)) {
					block_count++;
					word |= m;
				}
#endif
				data[off] = cpu_to_be32(word);
				mark_buffer_dirty_inode(bh, anode);
				brelse(bh);
				goto found;
			}
		} while (++off < size);
		brelse(bh);
		if (++blk >= anode->i_blocks)
			blk = 0;
		if (blk == blk1)
			return -ENOSPC;
	}

found:
	dprint(DBG_EXTENT, "extend %lu: %u,%u\n", inode->i_ino, ablock, block_count);
	if (HFSPLUS_I(inode).alloc_blocks <= HFSPLUS_I(inode).extent_blocks) {
		if (!HFSPLUS_I(inode).extent_blocks) {
			dprint(DBG_EXTENT, "first extents\n");
			/* no extents yet */
			HFSPLUS_I(inode).extents[0].start_block = cpu_to_be32(ablock);
			HFSPLUS_I(inode).extents[0].block_count = cpu_to_be32(block_count);
			res = 0;
		} else
			/* try to append to extents in inode */
			res = hfsplus_add_extent(HFSPLUS_I(inode).extents,
						 HFSPLUS_I(inode).alloc_blocks,
						 ablock, block_count);
		if (!res) {
			hfsplus_dump_extent(HFSPLUS_I(inode).extents);
			HFSPLUS_I(inode).extent_blocks += block_count;
		} else if (res == -ENOSPC) {
			/* create new extent, so find place to insert it */
			hfsplus_find_init(HFSPLUS_SB(sb).ext_tree, &fd);
			hfsplus_fill_ext_key(fd.search_key, inode->i_ino,
					     HFSPLUS_I(inode).alloc_blocks,
					     HFSPLUS_IS_RSRC(inode) ?
					     HFSPLUS_TYPE_RSRC : HFSPLUS_TYPE_DATA);
			res = hfsplus_find_extentry(&fd, ext_entry);
			if (res && res != -ENOENT) {
				hfsplus_find_exit(&fd);
				goto out;
			}
			goto insert_extent;
		}
	} else {
		hfsplus_find_init(HFSPLUS_SB(sb).ext_tree, &fd);
		hfsplus_fill_ext_key(fd.search_key, inode->i_ino,
				     HFSPLUS_I(inode).alloc_blocks,
				     HFSPLUS_IS_RSRC(inode) ?
				     HFSPLUS_TYPE_RSRC : HFSPLUS_TYPE_DATA);
		res = hfsplus_find_extentry(&fd, ext_entry);
		if (res) {
			hfsplus_find_exit(&fd);
			goto out;
		}

		res = hfsplus_add_extent(ext_entry, HFSPLUS_I(inode).alloc_blocks -
					 be32_to_cpu(fd.key->ext.start_block),
					 ablock, block_count);
		if (!res) {
			hfsplus_dump_extent(ext_entry);
			hfsplus_bnode_writebytes(fd.bnode, &ext_entry,
						 fd.entryoffset,
						 sizeof(ext_entry));
		} else if (res == -ENOSPC)
			goto insert_extent;
		hfsplus_find_exit(&fd);
	}
out:
	if (!res) {
		HFSPLUS_I(inode).alloc_blocks += block_count;
		mark_inode_dirty(inode);
		HFSPLUS_SB(sb).free_blocks -= block_count;
		sb->s_dirt = 1;
	}
	return res;

insert_extent:
	dprint(DBG_EXTENT, "insert new extent\n");
	memset(ext_entry, 0, sizeof(ext_entry));
	ext_entry[0].start_block = cpu_to_be32(ablock);
	ext_entry[0].block_count = cpu_to_be32(block_count);

	hfsplus_bnode_insert_rec(&fd, ext_entry, sizeof(ext_entry));
	hfsplus_find_exit(&fd);
	res = 0;
	goto out;
}

void hfsplus_truncate(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	struct hfsplus_find_data fd;
	hfsplus_extent_rec ext_entry;
	u32 blk_cnt, start;
	int res;

	dprint(DBG_INODE, "truncate: %lu, %Lu -> %Lu\n", inode->i_ino,
	       (long long)HFSPLUS_I(inode).mmu_private, inode->i_size);
	if (inode->i_size == HFSPLUS_I(inode).mmu_private)
		return;
	if (inode->i_size > HFSPLUS_I(inode).mmu_private) {
		struct address_space *mapping = inode->i_mapping;
		struct page *page;
		u32 size = inode->i_size - 1;
		int res;

		page = grab_cache_page(mapping, size >> PAGE_CACHE_SHIFT);
		if (!page)
			return;
		size &= PAGE_CACHE_SIZE - 1;
		size++;
		res = mapping->a_ops->prepare_write(NULL, page, size, size);
		if (!res)
			res = mapping->a_ops->commit_write(NULL, page, size, size);
		if (res)
			inode->i_size = HFSPLUS_I(inode).mmu_private;
		unlock_page(page);
		page_cache_release(page);
		mark_inode_dirty(inode);
		return;
	}
	blk_cnt = (inode->i_size + sb->s_blocksize - 1) >> sb->s_blocksize_bits;

	while (1) {
		if (HFSPLUS_I(inode).alloc_blocks <= HFSPLUS_I(inode).extent_blocks) {
			hfsplus_free_extents(sb, HFSPLUS_I(inode).extents,
					     HFSPLUS_I(inode).alloc_blocks,
					     HFSPLUS_I(inode).alloc_blocks - blk_cnt);
			hfsplus_dump_extent(HFSPLUS_I(inode).extents);
			HFSPLUS_I(inode).extent_blocks = blk_cnt;
			break;
		}
		hfsplus_find_init(HFSPLUS_SB(sb).ext_tree, &fd);
		hfsplus_fill_ext_key(fd.search_key, inode->i_ino,
				     HFSPLUS_I(inode).alloc_blocks,
				     HFSPLUS_IS_RSRC(inode) ?
				     HFSPLUS_TYPE_RSRC : HFSPLUS_TYPE_DATA);
		res = hfsplus_find_extentry(&fd, ext_entry);
		if (res) {
			hfsplus_find_exit(&fd);
			break;
		}
		start = be32_to_cpu(fd.key->ext.start_block);
		hfsplus_free_extents(sb, ext_entry,
				     HFSPLUS_I(inode).alloc_blocks - start,
				     HFSPLUS_I(inode).alloc_blocks - blk_cnt);
		hfsplus_dump_extent(ext_entry);
		if (blk_cnt > start) {
			hfsplus_bnode_writebytes(fd.bnode, &ext_entry,
						 fd.entryoffset,
						 sizeof(ext_entry));
			hfsplus_find_exit(&fd);
			break;
		}
		HFSPLUS_I(inode).alloc_blocks = start;
		hfsplus_bnode_remove_rec(&fd);
		hfsplus_find_exit(&fd);
	}
	HFSPLUS_I(inode).mmu_private = inode->i_size;
	HFSPLUS_I(inode).alloc_blocks = HFSPLUS_I(inode).total_blocks = blk_cnt;
}
