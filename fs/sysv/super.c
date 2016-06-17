/*
 *  linux/fs/sysv/inode.c
 *
 *  minix/inode.c
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  xenix/inode.c
 *  Copyright (C) 1992  Doug Evans
 *
 *  coh/inode.c
 *  Copyright (C) 1993  Pascal Haible, Bruno Haible
 *
 *  sysv/inode.c
 *  Copyright (C) 1993  Paul B. Monday
 *
 *  sysv/inode.c
 *  Copyright (C) 1993  Bruno Haible
 *  Copyright (C) 1997, 1998  Krzysztof G. Baranowski
 *
 *  This file contains code for read/parsing the superblock.
 */

#include <linux/module.h>

#include <linux/fs.h>
#include <linux/sysv_fs.h>
#include <linux/init.h>

/*
 * The following functions try to recognize specific filesystems.
 *
 * We recognize:
 * - Xenix FS by its magic number.
 * - SystemV FS by its magic number.
 * - Coherent FS by its funny fname/fpack field.
 * - SCO AFS by s_nfree == 0xffff
 * - V7 FS has no distinguishing features.
 *
 * We discriminate among SystemV4 and SystemV2 FS by the assumption that
 * the time stamp is not < 01-01-1980.
 */

enum {
	JAN_1_1980 = (10*365 + 2) * 24 * 60 * 60
};

static void detected_xenix(struct super_block *sb)
{
	struct buffer_head *bh1 = sb->sv_bh1;
	struct buffer_head *bh2 = sb->sv_bh2;
	struct xenix_super_block * sbd1;
	struct xenix_super_block * sbd2;

	if (bh1 != bh2)
		sbd1 = sbd2 = (struct xenix_super_block *) bh1->b_data;
	else {
		/* block size = 512, so bh1 != bh2 */
		sbd1 = (struct xenix_super_block *) bh1->b_data;
		sbd2 = (struct xenix_super_block *) (bh2->b_data - 512);
	}

	sb->sv_link_max = XENIX_LINK_MAX;
	sb->sv_fic_size = XENIX_NICINOD;
	sb->sv_flc_size = XENIX_NICFREE;
	sb->sv_sbd1 = (char *) sbd1;
	sb->sv_sbd2 = (char *) sbd2;
	sb->sv_sb_fic_count = &sbd1->s_ninode;
	sb->sv_sb_fic_inodes = &sbd1->s_inode[0];
	sb->sv_sb_total_free_inodes = &sbd2->s_tinode;
	sb->sv_bcache_count = &sbd1->s_nfree;
	sb->sv_bcache = &sbd1->s_free[0];
	sb->sv_free_blocks = &sbd2->s_tfree;
	sb->sv_sb_time = &sbd2->s_time;
	sb->sv_firstdatazone = fs16_to_cpu(sb, sbd1->s_isize);
	sb->sv_nzones = fs32_to_cpu(sb, sbd1->s_fsize);
}

static void detected_sysv4(struct super_block *sb)
{
	struct sysv4_super_block * sbd;
	struct buffer_head *bh1 = sb->sv_bh1;
	struct buffer_head *bh2 = sb->sv_bh2;

	if (bh1 == bh2)
		sbd = (struct sysv4_super_block *) (bh1->b_data + BLOCK_SIZE/2);
	else
		sbd = (struct sysv4_super_block *) bh2->b_data;

	sb->sv_link_max = SYSV_LINK_MAX;
	sb->sv_fic_size = SYSV_NICINOD;
	sb->sv_flc_size = SYSV_NICFREE;
	sb->sv_sbd1 = (char *) sbd;
	sb->sv_sbd2 = (char *) sbd;
	sb->sv_sb_fic_count = &sbd->s_ninode;
	sb->sv_sb_fic_inodes = &sbd->s_inode[0];
	sb->sv_sb_total_free_inodes = &sbd->s_tinode;
	sb->sv_bcache_count = &sbd->s_nfree;
	sb->sv_bcache = &sbd->s_free[0];
	sb->sv_free_blocks = &sbd->s_tfree;
	sb->sv_sb_time = &sbd->s_time;
	sb->sv_sb_state = &sbd->s_state;
	sb->sv_firstdatazone = fs16_to_cpu(sb, sbd->s_isize);
	sb->sv_nzones = fs32_to_cpu(sb, sbd->s_fsize);
}

static void detected_sysv2(struct super_block *sb)
{
	struct sysv2_super_block * sbd;
	struct buffer_head *bh1 = sb->sv_bh1;
	struct buffer_head *bh2 = sb->sv_bh2;

	if (bh1 == bh2)
		sbd = (struct sysv2_super_block *) (bh1->b_data + BLOCK_SIZE/2);
	else
		sbd = (struct sysv2_super_block *) bh2->b_data;

	sb->sv_link_max = SYSV_LINK_MAX;
	sb->sv_fic_size = SYSV_NICINOD;
	sb->sv_flc_size = SYSV_NICFREE;
	sb->sv_sbd1 = (char *) sbd;
	sb->sv_sbd2 = (char *) sbd;
	sb->sv_sb_fic_count = &sbd->s_ninode;
	sb->sv_sb_fic_inodes = &sbd->s_inode[0];
	sb->sv_sb_total_free_inodes = &sbd->s_tinode;
	sb->sv_bcache_count = &sbd->s_nfree;
	sb->sv_bcache = &sbd->s_free[0];
	sb->sv_free_blocks = &sbd->s_tfree;
	sb->sv_sb_time = &sbd->s_time;
	sb->sv_sb_state = &sbd->s_state;
	sb->sv_firstdatazone = fs16_to_cpu(sb, sbd->s_isize);
	sb->sv_nzones = fs32_to_cpu(sb, sbd->s_fsize);
}

static void detected_coherent(struct super_block *sb)
{
	struct coh_super_block * sbd;
	struct buffer_head *bh1 = sb->sv_bh1;

	sbd = (struct coh_super_block *) bh1->b_data;

	sb->sv_link_max = COH_LINK_MAX;
	sb->sv_fic_size = COH_NICINOD;
	sb->sv_flc_size = COH_NICFREE;
	sb->sv_sbd1 = (char *) sbd;
	sb->sv_sbd2 = (char *) sbd;
	sb->sv_sb_fic_count = &sbd->s_ninode;
	sb->sv_sb_fic_inodes = &sbd->s_inode[0];
	sb->sv_sb_total_free_inodes = &sbd->s_tinode;
	sb->sv_bcache_count = &sbd->s_nfree;
	sb->sv_bcache = &sbd->s_free[0];
	sb->sv_free_blocks = &sbd->s_tfree;
	sb->sv_sb_time = &sbd->s_time;
	sb->sv_firstdatazone = fs16_to_cpu(sb, sbd->s_isize);
	sb->sv_nzones = fs32_to_cpu(sb, sbd->s_fsize);
}

static void detected_v7(struct super_block *sb)
{
	struct buffer_head *bh2 = sb->sv_bh2;
	struct v7_super_block *sbd = (struct v7_super_block *)bh2->b_data;

	sb->sv_link_max = V7_LINK_MAX;
	sb->sv_fic_size = V7_NICINOD;
	sb->sv_flc_size = V7_NICFREE;
	sb->sv_sbd1 = (char *)sbd;
	sb->sv_sbd2 = (char *)sbd;
	sb->sv_sb_fic_count = &sbd->s_ninode;
	sb->sv_sb_fic_inodes = &sbd->s_inode[0];
	sb->sv_sb_total_free_inodes = &sbd->s_tinode;
	sb->sv_bcache_count = &sbd->s_nfree;
	sb->sv_bcache = &sbd->s_free[0];
	sb->sv_free_blocks = &sbd->s_tfree;
	sb->sv_sb_time = &sbd->s_time;
	sb->sv_firstdatazone = fs16_to_cpu(sb, sbd->s_isize);
	sb->sv_nzones = fs32_to_cpu(sb, sbd->s_fsize);
}

static int detect_xenix (struct super_block *sb, struct buffer_head *bh)
{
	struct xenix_super_block * sbd = (struct xenix_super_block *)bh->b_data;
	if (sbd->s_magic == cpu_to_le32(0x2b5544))
		sb->sv_bytesex = BYTESEX_LE;
	else if (sbd->s_magic == cpu_to_be32(0x2b5544))
		sb->sv_bytesex = BYTESEX_BE;
	else
		return 0;
	if (sbd->s_type > 2 || sbd->s_type < 1)
		return 0;
	sb->sv_type = FSTYPE_XENIX;
	return sbd->s_type;
}

static int detect_sysv (struct super_block *sb, struct buffer_head *bh)
{
	/* All relevant fields are at the same offsets in R2 and R4 */
	struct sysv4_super_block * sbd;

	sbd = (struct sysv4_super_block *) (bh->b_data + BLOCK_SIZE/2);
	if (sbd->s_magic == cpu_to_le32(0xfd187e20))
		sb->sv_bytesex = BYTESEX_LE;
	else if (sbd->s_magic == cpu_to_be32(0xfd187e20))
		sb->sv_bytesex = BYTESEX_BE;
	else
		return 0;
 
 	if (fs16_to_cpu(sb, sbd->s_nfree) == 0xffff) {
 		sb->sv_type = FSTYPE_AFS;
 		if (!(sb->s_flags & MS_RDONLY)) {
 			printk("SysV FS: SCO EAFS on %s detected, " 
 				"forcing read-only mode.\n", 
 				bdevname(sb->s_dev));
 			sb->s_flags |= MS_RDONLY;
 		}
 		return sbd->s_type;
 	}
 
	if (fs32_to_cpu(sb, sbd->s_time) < JAN_1_1980) {
		/* this is likely to happen on SystemV2 FS */
		if (sbd->s_type > 3 || sbd->s_type < 1)
			return 0;
		sb->sv_type = FSTYPE_SYSV2;
		return sbd->s_type;
	}
	if ((sbd->s_type > 3 || sbd->s_type < 1) &&
	    (sbd->s_type > 0x30 || sbd->s_type < 0x10))
		return 0;

	/* On Interactive Unix (ISC) Version 4.0/3.x s_type field = 0x10,
	   0x20 or 0x30 indicates that symbolic links and the 14-character
	   filename limit is gone. Due to lack of information about this
           feature read-only mode seems to be a reasonable approach... -KGB */

	if (sbd->s_type >= 0x10) {
		printk("SysV FS: can't handle long file names on %s, "
		       "forcing read-only mode.\n", kdevname(sb->s_dev));
		sb->s_flags |= MS_RDONLY;
	}

	sb->sv_type = FSTYPE_SYSV4;
	return sbd->s_type >= 0x10 ? (sbd->s_type >> 4) : sbd->s_type;
}

static int detect_coherent (struct super_block *sb, struct buffer_head *bh)
{
	struct coh_super_block * sbd;

	sbd = (struct coh_super_block *) (bh->b_data + BLOCK_SIZE/2);
	if ((memcmp(sbd->s_fname,"noname",6) && memcmp(sbd->s_fname,"xxxxx ",6))
	    || (memcmp(sbd->s_fpack,"nopack",6) && memcmp(sbd->s_fpack,"xxxxx\n",6)))
		return 0;
	sb->sv_bytesex = BYTESEX_PDP;
	sb->sv_type = FSTYPE_COH;
	return 1;
}

static int detect_sysv_odd(struct super_block *sb, struct buffer_head *bh)
{
	int size = detect_sysv(sb, bh);

	return size>2 ? 0 : size;
}

static struct {
	int block;
	int (*test)(struct super_block *, struct buffer_head *);
} flavours[] = {
	{1, detect_xenix},
	{0, detect_sysv},
	{0, detect_coherent},
	{9, detect_sysv_odd},
	{15,detect_sysv_odd},
	{18,detect_sysv},
};

static char *flavour_names[] = {
	[FSTYPE_XENIX]	"Xenix",
	[FSTYPE_SYSV4]	"SystemV",
	[FSTYPE_SYSV2]	"SystemV Release 2",
	[FSTYPE_COH]	"Coherent",
	[FSTYPE_V7]	"V7",
	[FSTYPE_AFS]	"AFS",
};

static void (*flavour_setup[])(struct super_block *) = {
	[FSTYPE_XENIX]	detected_xenix,
	[FSTYPE_SYSV4]	detected_sysv4,
	[FSTYPE_SYSV2]	detected_sysv2,
	[FSTYPE_COH]	detected_coherent,
	[FSTYPE_V7]	detected_v7,
	[FSTYPE_AFS]	detected_sysv4,
};

static int complete_read_super(struct super_block *sb, int silent, int size)
{
	struct inode *root_inode;
	char *found = flavour_names[sb->sv_type];
	u_char n_bits = size+8;
	int bsize = 1 << n_bits;
	int bsize_4 = bsize >> 2;

	sb->sv_firstinodezone = 2;

	flavour_setup[sb->sv_type](sb);
	
	sb->sv_truncate = 1;
	sb->sv_ndatazones = sb->sv_nzones - sb->sv_firstdatazone;
	sb->sv_inodes_per_block = bsize >> 6;
	sb->sv_inodes_per_block_1 = (bsize >> 6)-1;
	sb->sv_inodes_per_block_bits = n_bits-6;
	sb->sv_ind_per_block = bsize_4;
	sb->sv_ind_per_block_2 = bsize_4*bsize_4;
	sb->sv_toobig_block = 10 + bsize_4 * (1 + bsize_4 * (1 + bsize_4));
	sb->sv_ind_per_block_bits = n_bits-2;

	sb->sv_ninodes = (sb->sv_firstdatazone - sb->sv_firstinodezone)
		<< sb->sv_inodes_per_block_bits;

	sb->s_blocksize = bsize;
	sb->s_blocksize_bits = n_bits;
	if (!silent)
		printk("VFS: Found a %s FS (block size = %ld) on device %s\n",
		       found, sb->s_blocksize, bdevname(sb->s_dev));

	sb->s_magic = SYSV_MAGIC_BASE + sb->sv_type;
	/* set up enough so that it can read an inode */
	sb->s_op = &sysv_sops;
	root_inode = iget(sb,SYSV_ROOT_INO);
	if (!root_inode || is_bad_inode(root_inode)) {
		printk("SysV FS: get root inode failed\n");
		return 0;
	}
	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root) {
		iput(root_inode);
		printk("SysV FS: get root dentry failed\n");
		return 0;
	}
	if (sb->sv_truncate)
		sb->s_root->d_op = &sysv_dentry_operations;
	sb->s_flags |= MS_RDONLY;
	sb->s_dirt = 1;
	return 1;
}

static struct super_block *sysv_read_super(struct super_block *sb,
					   void *data, int silent)
{
	struct buffer_head *bh1;
	struct buffer_head *bh = NULL;
	kdev_t dev = sb->s_dev;
	unsigned long blocknr;
	int size = 0;
	int i;
	
	if (1024 != sizeof (struct xenix_super_block))
		panic("Xenix FS: bad super-block size");
	if ((512 != sizeof (struct sysv4_super_block))
            || (512 != sizeof (struct sysv2_super_block)))
		panic("SystemV FS: bad super-block size");
	if (500 != sizeof (struct coh_super_block))
		panic("Coherent FS: bad super-block size");
	if (64 != sizeof (struct sysv_inode))
		panic("sysv fs: bad i-node size");
	set_blocksize(dev,BLOCK_SIZE);
	sb->s_blocksize = BLOCK_SIZE;
	sb->sv_block_base = 0;

	for (i = 0; i < sizeof(flavours)/sizeof(flavours[0]) && !size; i++) {
		brelse(bh);
		bh = sb_bread(sb, flavours[i].block);
		if (!bh)
			continue;
		size = flavours[i].test(sb, bh);
	}

	if (!size)
		goto Eunknown;

	switch (size) {
		case 1:
			blocknr = bh->b_blocknr << 1;
			brelse(bh);
			set_blocksize(dev, 512);
			sb->s_blocksize = 512;
			bh1 = sb_bread(sb, blocknr);
			bh = sb_bread(sb, blocknr + 1);
			break;
		case 2:
			bh1 = bh;
			break;
		case 3:
			blocknr = bh->b_blocknr >> 1;
			brelse(bh);
			set_blocksize(dev, 2048);
			sb->s_blocksize = 2048;
			bh1 = bh = sb_bread(sb, blocknr);
			break;
		default:
			goto Ebadsize;
	}

	if (bh && bh1) {
		sb->sv_bh1 = bh1;
		sb->sv_bh2 = bh;
		if (complete_read_super(sb, silent, size))
			return sb;
	}

	brelse(bh1);
	brelse(bh);
	set_blocksize(sb->s_dev,BLOCK_SIZE);
	printk("oldfs: cannot read superblock\n");
failed:
	return NULL;

Eunknown:
	brelse(bh);
	if (!silent)
		printk("VFS: unable to find oldfs superblock on device %s\n",
			bdevname(dev));
	goto failed;
Ebadsize:
	brelse(bh);
	if (!silent)
		printk("VFS: oldfs: unsupported block size (%dKb)\n",
			1<<(size-2));
	goto failed;
}

static struct super_block *v7_read_super(struct super_block *sb,void *data,
				  int silent)
{
	struct buffer_head *bh, *bh2 = NULL;
	kdev_t dev = sb->s_dev;
	struct v7_super_block *v7sb;
	struct sysv_inode *v7i;

	if (440 != sizeof (struct v7_super_block))
		panic("V7 FS: bad super-block size");
	if (64 != sizeof (struct sysv_inode))
		panic("sysv fs: bad i-node size");

	sb->sv_type = FSTYPE_V7;
	sb->sv_bytesex = BYTESEX_PDP;

	set_blocksize(dev, 512);
	sb->s_blocksize = 512;

	if ((bh = sb_bread(sb, 1)) == NULL) {
		if (!silent)
			printk("VFS: unable to read V7 FS superblock on "
			       "device %s.\n", bdevname(dev));
		goto failed;
	}

	/* plausibility check on superblock */
	v7sb = (struct v7_super_block *) bh->b_data;
	if (fs16_to_cpu(sb,v7sb->s_nfree) > V7_NICFREE ||
	    fs16_to_cpu(sb,v7sb->s_ninode) > V7_NICINOD ||
	    fs32_to_cpu(sb,v7sb->s_time) == 0)
		goto failed;

	/* plausibility check on root inode: it is a directory,
	   with a nonzero size that is a multiple of 16 */
	if ((bh2 = sb_bread(sb, 2)) == NULL)
		goto failed;
	v7i = (struct sysv_inode *)(bh2->b_data + 64);
	if ((fs16_to_cpu(sb,v7i->i_mode) & ~0777) != S_IFDIR ||
	    (fs32_to_cpu(sb,v7i->i_size) == 0) ||
	    (fs32_to_cpu(sb,v7i->i_size) & 017) != 0)
		goto failed;
	brelse(bh2);

	sb->sv_bh1 = bh;
	sb->sv_bh2 = bh;
	if (complete_read_super(sb, silent, 1))
		return sb;

failed:
	brelse(bh2);
	brelse(bh);
	return NULL;
}

/* Every kernel module contains stuff like this. */

static DECLARE_FSTYPE_DEV(sysv_fs_type, "sysv", sysv_read_super);
static DECLARE_FSTYPE_DEV(v7_fs_type, "v7", v7_read_super);

static int __init init_sysv_fs(void)
{
	int err = register_filesystem(&sysv_fs_type);
	if (!err)
		err = register_filesystem(&v7_fs_type);
	return err;
}

static void __exit exit_sysv_fs(void)
{
	unregister_filesystem(&sysv_fs_type);
	unregister_filesystem(&v7_fs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_sysv_fs)
module_exit(exit_sysv_fs)
MODULE_LICENSE("GPL");
