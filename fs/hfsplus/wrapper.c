/*
 *  linux/fs/hfsplus/wrapper.c
 *
 * Copyright (C) 2001
 * Brad Boyer (flar@allandria.com)
 * (C) 2003 Ardis Technologies <roman@ardistech.com>
 *
 * Handling of HFS wrappers around HFS+ volumes
 */

#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/buffer_head.h>
#endif
#include <asm/unaligned.h>

#include "hfsplus_fs.h"
#include "hfsplus_raw.h"

struct hfsplus_wd {
	u32 ablk_size;
	u16 ablk_start;
	u16 embed_start;
	u16 embed_count;
};

static int hfsplus_read_mdb(unsigned char *bufptr, struct hfsplus_wd *wd)
{
	u32 extent;
	u16 attrib;

	if (be16_to_cpu(*(u16 *)(bufptr + HFSP_WRAPOFF_EMBEDSIG)) != HFSPLUS_VOLHEAD_SIG)
		return 0;

	attrib = be16_to_cpu(*(u16 *)(bufptr + HFSP_WRAPOFF_ATTRIB));
	if (!(attrib & HFSP_WRAP_ATTRIB_SLOCK) ||
	   !(attrib & HFSP_WRAP_ATTRIB_SPARED))
		return 0;

	wd->ablk_size = be32_to_cpu(*(u32 *)(bufptr + HFSP_WRAPOFF_ABLKSIZE));
	if (wd->ablk_size < HFSPLUS_SECTOR_SIZE)
		return 0;
	if (wd->ablk_size % HFSPLUS_SECTOR_SIZE)
		return 0;
	wd->ablk_start = be16_to_cpu(*(u16 *)(bufptr + HFSP_WRAPOFF_ABLKSTART));

	extent = be32_to_cpu(get_unaligned((u32 *)(bufptr + HFSP_WRAPOFF_EMBEDEXT)));
	wd->embed_start = (extent >> 16) & 0xFFFF;
	wd->embed_count = extent & 0xFFFF;

	return 1;
}

/* Find the volume header and fill in some minimum bits in superblock */
/* Takes in super block, returns true if good data read */
int hfsplus_read_wrapper(struct super_block *sb)
{
	struct buffer_head *bh;
	struct hfsplus_vh *vhdr;
	char *bufptr;
	unsigned long block, offset, vhsect;
	struct hfsplus_wd wd;
	u32 blocksize, blockoffset;
	u16 sig;

	blocksize = sb_min_blocksize(sb, HFSPLUS_SECTOR_SIZE);
	if (!blocksize) {
		printk("HFS+-fs: unable to configure block size\n");
		return -EINVAL;
	}

	block = (HFSPLUS_VOLHEAD_SECTOR * HFSPLUS_SECTOR_SIZE) / blocksize;
	offset = (HFSPLUS_VOLHEAD_SECTOR * HFSPLUS_SECTOR_SIZE) % blocksize;

	bh = sb_bread(sb, block);
	if (!bh) {
		printk("HFS+-fs: unable to read VHDR or MDB\n");
		return -EIO;
	}

	bufptr = bh->b_data + offset;
	sig = be16_to_cpu(*(u16 *)(bufptr + HFSP_WRAPOFF_SIG));
	if (sig == HFSP_WRAP_MAGIC) {
		if (!hfsplus_read_mdb(bufptr, &wd))
			goto error;
		vhsect = (wd.ablk_start + wd.embed_start * (wd.ablk_size >> 9))
			+ HFSPLUS_VOLHEAD_SECTOR;
		block = (vhsect * HFSPLUS_SECTOR_SIZE) / blocksize;
		offset = (vhsect * HFSPLUS_SECTOR_SIZE) % blocksize;
		brelse(bh);
		bh = sb_bread(sb, block);
		if (!bh) {
			printk("HFS+-fs: unable to read VHDR\n");
			return -EIO;
		}
		HFSPLUS_SB(sb).sect_count = wd.embed_count * (wd.ablk_size >> 9);
	} else {
		wd.ablk_start = 0;
		wd.ablk_size = blocksize;
		wd.embed_start = 0;
		HFSPLUS_SB(sb).sect_count = sb->s_bdev->bd_inode->i_size >> 9;
	}
	vhdr = (struct hfsplus_vh *)(bh->b_data + offset);
	if (be16_to_cpu(vhdr->signature) != HFSPLUS_VOLHEAD_SIG)
		goto error;
	blocksize = be32_to_cpu(vhdr->blocksize);
	brelse(bh);

	/* block size must be at least as large as a sector
	 * and a multiple of 2
	 */
	if (blocksize < HFSPLUS_SECTOR_SIZE ||
	    ((blocksize - 1) & blocksize))
		return -EINVAL;

	/* block offset must be a multiple of the block size */
	blockoffset = wd.ablk_start + wd.embed_start * (wd.ablk_size >> 9);
	if (blockoffset % (blocksize / HFSPLUS_SECTOR_SIZE)) {
		printk("HFS+-fs: embedded blocks not aligned with wrapper\n");
		return -EINVAL;
	}
	blockoffset /= blocksize / HFSPLUS_SECTOR_SIZE;
	HFSPLUS_SB(sb).blockoffset = blockoffset;

	if (sb_set_blocksize(sb, blocksize) != blocksize)
		return -EINVAL;

	block = blockoffset + HFSPLUS_VOLHEAD_SECTOR /
		(blocksize / HFSPLUS_SECTOR_SIZE);
	offset = (HFSPLUS_VOLHEAD_SECTOR * HFSPLUS_SECTOR_SIZE) % blocksize;
	bh = sb_bread(sb, block);
	if (!bh) {
		printk("HFS+-fs: unable to read VHDR or MDB\n");
		return -EIO;
	}
	vhdr = (struct hfsplus_vh *)(bh->b_data + offset);
	/* should still be the same... */
	if (be16_to_cpu(vhdr->signature) != HFSPLUS_VOLHEAD_SIG)
		goto error;
	HFSPLUS_SB(sb).s_vhbh = bh;
	HFSPLUS_SB(sb).s_vhdr = vhdr;

	return 0;
 error:
	brelse(bh);
	return -EINVAL;
}
