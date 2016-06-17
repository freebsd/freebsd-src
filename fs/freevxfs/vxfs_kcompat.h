#ifndef _VXFS_KCOMPAT_H
#define _VXFS_KCOMPAT_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))

#include <linux/blkdev.h>

typedef long sector_t;

/* Dito.  */
static inline void map_bh(struct buffer_head *bh, struct super_block *sb, int block)
{
	bh->b_state |= 1 << BH_Mapped;
	bh->b_dev = sb->s_dev;
	bh->b_blocknr = block;
}

#endif /* Kernel 2.4 */
#endif /* _VXFS_KCOMPAT_H */
