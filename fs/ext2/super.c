/*
 *  linux/fs/ext2/super.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>


static void ext2_sync_super(struct super_block *sb,
			    struct ext2_super_block *es);

static char error_buf[1024];

void ext2_error (struct super_block * sb, const char * function,
		 const char * fmt, ...)
{
	va_list args;
	struct ext2_super_block *es = EXT2_SB(sb)->s_es;

	if (!(sb->s_flags & MS_RDONLY)) {
		sb->u.ext2_sb.s_mount_state |= EXT2_ERROR_FS;
		es->s_state =
			cpu_to_le16(le16_to_cpu(es->s_state) | EXT2_ERROR_FS);
		ext2_sync_super(sb, es);
	}
	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);
	if (test_opt (sb, ERRORS_PANIC) ||
	    (le16_to_cpu(sb->u.ext2_sb.s_es->s_errors) == EXT2_ERRORS_PANIC &&
	     !test_opt (sb, ERRORS_CONT) && !test_opt (sb, ERRORS_RO)))
		panic ("EXT2-fs panic (device %s): %s: %s\n",
		       bdevname(sb->s_dev), function, error_buf);
	printk (KERN_CRIT "EXT2-fs error (device %s): %s: %s\n",
		bdevname(sb->s_dev), function, error_buf);
	if (test_opt (sb, ERRORS_RO) ||
	    (le16_to_cpu(sb->u.ext2_sb.s_es->s_errors) == EXT2_ERRORS_RO &&
	     !test_opt (sb, ERRORS_CONT) && !test_opt (sb, ERRORS_PANIC))) {
		printk ("Remounting filesystem read-only\n");
		sb->s_flags |= MS_RDONLY;
	}
}

NORET_TYPE void ext2_panic (struct super_block * sb, const char * function,
			    const char * fmt, ...)
{
	va_list args;

	if (!(sb->s_flags & MS_RDONLY)) {
		sb->u.ext2_sb.s_mount_state |= EXT2_ERROR_FS;
		sb->u.ext2_sb.s_es->s_state =
			cpu_to_le16(le16_to_cpu(sb->u.ext2_sb.s_es->s_state) | EXT2_ERROR_FS);
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh);
		sb->s_dirt = 1;
	}
	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);
	sb->s_flags |= MS_RDONLY;
	panic ("EXT2-fs panic (device %s): %s: %s\n",
	       bdevname(sb->s_dev), function, error_buf);
}

void ext2_warning (struct super_block * sb, const char * function,
		   const char * fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);
	printk (KERN_WARNING "EXT2-fs warning (device %s): %s: %s\n",
		bdevname(sb->s_dev), function, error_buf);
}

void ext2_update_dynamic_rev(struct super_block *sb)
{
	struct ext2_super_block *es = EXT2_SB(sb)->s_es;

	if (le32_to_cpu(es->s_rev_level) > EXT2_GOOD_OLD_REV)
		return;

	ext2_warning(sb, __FUNCTION__,
		     "updating to rev %d because of new feature flag, "
		     "running e2fsck is recommended",
		     EXT2_DYNAMIC_REV);

	es->s_first_ino = cpu_to_le32(EXT2_GOOD_OLD_FIRST_INO);
	es->s_inode_size = cpu_to_le16(EXT2_GOOD_OLD_INODE_SIZE);
	es->s_rev_level = cpu_to_le32(EXT2_DYNAMIC_REV);
	/* leave es->s_feature_*compat flags alone */
	/* es->s_uuid will be set by e2fsck if empty */

	/*
	 * The rest of the superblock fields should be zero, and if not it
	 * means they are likely already in use, so leave them alone.  We
	 * can leave it up to e2fsck to clean up any inconsistencies there.
	 */
}

void ext2_put_super (struct super_block * sb)
{
	int db_count;
	int i;

	if (!(sb->s_flags & MS_RDONLY)) {
		struct ext2_super_block *es = EXT2_SB(sb)->s_es;

		es->s_state = le16_to_cpu(EXT2_SB(sb)->s_mount_state);
		ext2_sync_super(sb, es);
	}
	db_count = EXT2_SB(sb)->s_gdb_count;
	for (i = 0; i < db_count; i++)
		if (sb->u.ext2_sb.s_group_desc[i])
			brelse (sb->u.ext2_sb.s_group_desc[i]);
	kfree(sb->u.ext2_sb.s_group_desc);
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
		if (sb->u.ext2_sb.s_inode_bitmap[i])
			brelse (sb->u.ext2_sb.s_inode_bitmap[i]);
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
		if (sb->u.ext2_sb.s_block_bitmap[i])
			brelse (sb->u.ext2_sb.s_block_bitmap[i]);
	brelse (sb->u.ext2_sb.s_sbh);

	return;
}

static struct super_operations ext2_sops = {
	read_inode:	ext2_read_inode,
	write_inode:	ext2_write_inode,
	put_inode:	ext2_put_inode,
	delete_inode:	ext2_delete_inode,
	put_super:	ext2_put_super,
	write_super:	ext2_write_super,
	statfs:		ext2_statfs,
	remount_fs:	ext2_remount,
};

/*
 * This function has been shamelessly adapted from the msdos fs
 */
static int parse_options (char * options, unsigned long * sb_block,
			  unsigned short *resuid, unsigned short * resgid,
			  unsigned long * mount_options)
{
	char * this_char;
	char * value;

	if (!options)
		return 1;
	for (this_char = strtok (options, ",");
	     this_char != NULL;
	     this_char = strtok (NULL, ",")) {
		if ((value = strchr (this_char, '=')) != NULL)
			*value++ = 0;
		if (!strcmp (this_char, "bsddf"))
			clear_opt (*mount_options, MINIX_DF);
		else if (!strcmp (this_char, "nouid32")) {
			set_opt (*mount_options, NO_UID32);
		}
		else if (!strcmp (this_char, "check")) {
			if (!value || !*value || !strcmp (value, "none"))
				clear_opt (*mount_options, CHECK);
			else
#ifdef CONFIG_EXT2_CHECK
				set_opt (*mount_options, CHECK);
#else
				printk("EXT2 Check option not supported\n");
#endif
		}
		else if (!strcmp (this_char, "debug"))
			set_opt (*mount_options, DEBUG);
		else if (!strcmp (this_char, "errors")) {
			if (!value || !*value) {
				printk ("EXT2-fs: the errors option requires "
					"an argument\n");
				return 0;
			}
			if (!strcmp (value, "continue")) {
				clear_opt (*mount_options, ERRORS_RO);
				clear_opt (*mount_options, ERRORS_PANIC);
				set_opt (*mount_options, ERRORS_CONT);
			}
			else if (!strcmp (value, "remount-ro")) {
				clear_opt (*mount_options, ERRORS_CONT);
				clear_opt (*mount_options, ERRORS_PANIC);
				set_opt (*mount_options, ERRORS_RO);
			}
			else if (!strcmp (value, "panic")) {
				clear_opt (*mount_options, ERRORS_CONT);
				clear_opt (*mount_options, ERRORS_RO);
				set_opt (*mount_options, ERRORS_PANIC);
			}
			else {
				printk ("EXT2-fs: Invalid errors option: %s\n",
					value);
				return 0;
			}
		}
		else if (!strcmp (this_char, "grpid") ||
			 !strcmp (this_char, "bsdgroups"))
			set_opt (*mount_options, GRPID);
		else if (!strcmp (this_char, "minixdf"))
			set_opt (*mount_options, MINIX_DF);
		else if (!strcmp (this_char, "nocheck"))
			clear_opt (*mount_options, CHECK);
		else if (!strcmp (this_char, "nogrpid") ||
			 !strcmp (this_char, "sysvgroups"))
			clear_opt (*mount_options, GRPID);
		else if (!strcmp (this_char, "resgid")) {
			if (!value || !*value) {
				printk ("EXT2-fs: the resgid option requires "
					"an argument\n");
				return 0;
			}
			*resgid = simple_strtoul (value, &value, 0);
			if (*value) {
				printk ("EXT2-fs: Invalid resgid option: %s\n",
					value);
				return 0;
			}
		}
		else if (!strcmp (this_char, "resuid")) {
			if (!value || !*value) {
				printk ("EXT2-fs: the resuid option requires "
					"an argument");
				return 0;
			}
			*resuid = simple_strtoul (value, &value, 0);
			if (*value) {
				printk ("EXT2-fs: Invalid resuid option: %s\n",
					value);
				return 0;
			}
		}
		else if (!strcmp (this_char, "sb")) {
			if (!value || !*value) {
				printk ("EXT2-fs: the sb option requires "
					"an argument");
				return 0;
			}
			*sb_block = simple_strtoul (value, &value, 0);
			if (*value) {
				printk ("EXT2-fs: Invalid sb option: %s\n",
					value);
				return 0;
			}
		}
		/* Silently ignore the quota options */
		else if (!strcmp (this_char, "grpquota")
		         || !strcmp (this_char, "noquota")
		         || !strcmp (this_char, "quota")
		         || !strcmp (this_char, "usrquota"))
			/* Don't do anything ;-) */ ;
		else {
			printk ("EXT2-fs: Unrecognized mount option %s\n", this_char);
			return 0;
		}
	}
	return 1;
}

static int ext2_setup_super (struct super_block * sb,
			      struct ext2_super_block * es,
			      int read_only)
{
	int res = 0;
	if (le32_to_cpu(es->s_rev_level) > EXT2_MAX_SUPP_REV) {
		printk ("EXT2-fs warning: revision level too high, "
			"forcing read-only mode\n");
		res = MS_RDONLY;
	}
	if (read_only)
		return res;
	if (!(sb->u.ext2_sb.s_mount_state & EXT2_VALID_FS))
		printk ("EXT2-fs warning: mounting unchecked fs, "
			"running e2fsck is recommended\n");
	else if ((sb->u.ext2_sb.s_mount_state & EXT2_ERROR_FS))
		printk ("EXT2-fs warning: mounting fs with errors, "
			"running e2fsck is recommended\n");
	else if ((__s16) le16_to_cpu(es->s_max_mnt_count) >= 0 &&
		 le16_to_cpu(es->s_mnt_count) >=
		 (unsigned short) (__s16) le16_to_cpu(es->s_max_mnt_count))
		printk ("EXT2-fs warning: maximal mount count reached, "
			"running e2fsck is recommended\n");
	else if (le32_to_cpu(es->s_checkinterval) &&
		(le32_to_cpu(es->s_lastcheck) + le32_to_cpu(es->s_checkinterval) <= CURRENT_TIME))
		printk ("EXT2-fs warning: checktime reached, "
			"running e2fsck is recommended\n");
	if (!(__s16) le16_to_cpu(es->s_max_mnt_count))
		es->s_max_mnt_count = (__s16) cpu_to_le16(EXT2_DFL_MAX_MNT_COUNT);
	es->s_mnt_count=cpu_to_le16(le16_to_cpu(es->s_mnt_count) + 1);
	ext2_write_super(sb);
	if (test_opt (sb, DEBUG))
		printk ("[EXT II FS %s, %s, bs=%lu, fs=%lu, gc=%lu, "
			"bpg=%lu, ipg=%lu, mo=%04lx]\n",
			EXT2FS_VERSION, EXT2FS_DATE, sb->s_blocksize,
			sb->u.ext2_sb.s_frag_size,
			sb->u.ext2_sb.s_groups_count,
			EXT2_BLOCKS_PER_GROUP(sb),
			EXT2_INODES_PER_GROUP(sb),
			sb->u.ext2_sb.s_mount_opt);
#ifdef CONFIG_EXT2_CHECK
	if (test_opt (sb, CHECK)) {
		ext2_check_blocks_bitmap (sb);
		ext2_check_inodes_bitmap (sb);
	}
#endif
	return res;
}

static int ext2_check_descriptors (struct super_block * sb)
{
	int i;
	int desc_block = 0;
	unsigned long block = le32_to_cpu(sb->u.ext2_sb.s_es->s_first_data_block);
	struct ext2_group_desc * gdp = NULL;

	ext2_debug ("Checking group descriptors");

	for (i = 0; i < sb->u.ext2_sb.s_groups_count; i++)
	{
		if ((i % EXT2_DESC_PER_BLOCK(sb)) == 0)
			gdp = (struct ext2_group_desc *) sb->u.ext2_sb.s_group_desc[desc_block++]->b_data;
		if (le32_to_cpu(gdp->bg_block_bitmap) < block ||
		    le32_to_cpu(gdp->bg_block_bitmap) >= block + EXT2_BLOCKS_PER_GROUP(sb))
		{
			ext2_error (sb, "ext2_check_descriptors",
				    "Block bitmap for group %d"
				    " not in group (block %lu)!",
				    i, (unsigned long) le32_to_cpu(gdp->bg_block_bitmap));
			return 0;
		}
		if (le32_to_cpu(gdp->bg_inode_bitmap) < block ||
		    le32_to_cpu(gdp->bg_inode_bitmap) >= block + EXT2_BLOCKS_PER_GROUP(sb))
		{
			ext2_error (sb, "ext2_check_descriptors",
				    "Inode bitmap for group %d"
				    " not in group (block %lu)!",
				    i, (unsigned long) le32_to_cpu(gdp->bg_inode_bitmap));
			return 0;
		}
		if (le32_to_cpu(gdp->bg_inode_table) < block ||
		    le32_to_cpu(gdp->bg_inode_table) + sb->u.ext2_sb.s_itb_per_group >=
		    block + EXT2_BLOCKS_PER_GROUP(sb))
		{
			ext2_error (sb, "ext2_check_descriptors",
				    "Inode table for group %d"
				    " not in group (block %lu)!",
				    i, (unsigned long) le32_to_cpu(gdp->bg_inode_table));
			return 0;
		}
		block += EXT2_BLOCKS_PER_GROUP(sb);
		gdp++;
	}
	return 1;
}

#define log2(n) ffz(~(n))
 
/*
 * Maximal file size.  There is a direct, and {,double-,triple-}indirect
 * block limit, and also a limit of (2^32 - 1) 512-byte sectors in i_blocks.
 * We need to be 1 filesystem block less than the 2^32 sector limit.
 */
static loff_t ext2_max_size(int bits)
{
	loff_t res = EXT2_NDIR_BLOCKS;
	res += 1LL << (bits-2);
	res += 1LL << (2*(bits-2));
	res += 1LL << (3*(bits-2));
	res <<= bits;
	if (res > (512LL << 32) - (1 << bits))
		res = (512LL << 32) - (1 << bits);
	return res;
}

static unsigned long descriptor_loc(struct super_block *sb,
				    unsigned long logic_sb_block,
				    int nr)
{
	struct ext2_sb_info *sbi = EXT2_SB(sb);
	unsigned long bg, first_data_block, first_meta_bg;
	int has_super = 0;
	
	first_data_block = le32_to_cpu(sbi->s_es->s_first_data_block);
	first_meta_bg = le32_to_cpu(sbi->s_es->s_first_meta_bg);

	if (!EXT2_HAS_INCOMPAT_FEATURE(sb, EXT2_FEATURE_INCOMPAT_META_BG) ||
	    nr < first_meta_bg)
		return (logic_sb_block + nr + 1);
	bg = sbi->s_desc_per_block * nr;
	if (ext2_bg_has_super(sb, bg))
		has_super = 1;
	return (first_data_block + has_super + (bg * sbi->s_blocks_per_group));
}

struct super_block * ext2_read_super (struct super_block * sb, void * data,
				      int silent)
{
	struct buffer_head * bh;
  	struct ext2_sb_info * sbi = EXT2_SB(sb);
	struct ext2_super_block * es;
	unsigned long sb_block = 1;
	unsigned short resuid = EXT2_DEF_RESUID;
	unsigned short resgid = EXT2_DEF_RESGID;
	unsigned long block;
	unsigned long logic_sb_block = 1;
	unsigned long offset = 0;
	kdev_t dev = sb->s_dev;
	int blocksize = BLOCK_SIZE;
	int db_count;
	int i, j;

	/*
	 * See what the current blocksize for the device is, and
	 * use that as the blocksize.  Otherwise (or if the blocksize
	 * is smaller than the default) use the default.
	 * This is important for devices that have a hardware
	 * sectorsize that is larger than the default.
	 */
	blocksize = get_hardsect_size(dev);
	if(blocksize < BLOCK_SIZE )
	    blocksize = BLOCK_SIZE;

	sb->u.ext2_sb.s_mount_opt = 0;
	if (!parse_options ((char *) data, &sb_block, &resuid, &resgid,
	    &sb->u.ext2_sb.s_mount_opt)) {
		return NULL;
	}

	if (set_blocksize(dev, blocksize) < 0) {
		printk ("EXT2-fs: unable to set blocksize %d\n", blocksize);
		return NULL;
	}
	sb->s_blocksize = blocksize;

	/*
	 * If the superblock doesn't start on a sector boundary,
	 * calculate the offset.  FIXME(eric) this doesn't make sense
	 * that we would have to do this.
	 */
	if (blocksize != BLOCK_SIZE) {
		logic_sb_block = (sb_block*BLOCK_SIZE) / blocksize;
		offset = (sb_block*BLOCK_SIZE) % blocksize;
	}

	if (!(bh = sb_bread(sb, logic_sb_block))) {
		printk ("EXT2-fs: unable to read superblock\n");
		return NULL;
	}
	/*
	 * Note: s_es must be initialized as soon as possible because
	 *       some ext2 macro-instructions depend on its value
	 */
	es = (struct ext2_super_block *) (((char *)bh->b_data) + offset);
	sb->u.ext2_sb.s_es = es;
	sb->s_magic = le16_to_cpu(es->s_magic);
	if (sb->s_magic != EXT2_SUPER_MAGIC) {
		if (!silent)
			printk ("VFS: Can't find ext2 filesystem on dev %s.\n",
				bdevname(dev));
		goto failed_mount;
	}
	if (le32_to_cpu(es->s_rev_level) == EXT2_GOOD_OLD_REV &&
	    (EXT2_HAS_COMPAT_FEATURE(sb, ~0U) ||
	     EXT2_HAS_RO_COMPAT_FEATURE(sb, ~0U) ||
	     EXT2_HAS_INCOMPAT_FEATURE(sb, ~0U)))
		printk("EXT2-fs warning: feature flags set on rev 0 fs, "
		       "running e2fsck is recommended\n");
	/*
	 * Check feature flags regardless of the revision level, since we
	 * previously didn't change the revision level when setting the flags,
	 * so there is a chance incompat flags are set on a rev 0 filesystem.
	 */
	if ((i = EXT2_HAS_INCOMPAT_FEATURE(sb, ~EXT2_FEATURE_INCOMPAT_SUPP))) {
		printk("EXT2-fs: %s: couldn't mount because of "
		       "unsupported optional features (%x).\n",
		       bdevname(dev), i);
		goto failed_mount;
	}
	if (!(sb->s_flags & MS_RDONLY) &&
	    (i = EXT2_HAS_RO_COMPAT_FEATURE(sb, ~EXT2_FEATURE_RO_COMPAT_SUPP))){
		printk("EXT2-fs: %s: couldn't mount RDWR because of "
		       "unsupported optional features (%x).\n",
		       bdevname(dev), i);
		goto failed_mount;
	}
	if (EXT2_HAS_COMPAT_FEATURE(sb, EXT3_FEATURE_COMPAT_HAS_JOURNAL))
		ext2_warning(sb, __FUNCTION__,
			"mounting ext3 filesystem as ext2\n");
	sb->s_blocksize_bits =
		le32_to_cpu(EXT2_SB(sb)->s_es->s_log_block_size) + 10;
	sb->s_blocksize = 1 << sb->s_blocksize_bits;

	sb->s_maxbytes = ext2_max_size(sb->s_blocksize_bits);

	/* If the blocksize doesn't match, re-read the thing.. */
	if (sb->s_blocksize != blocksize) {
		blocksize = sb->s_blocksize;
		brelse(bh);

		if (set_blocksize(dev, blocksize) < 0) {
			printk(KERN_ERR "EXT2-fs: blocksize too small for device.\n");
			return NULL;
		}

		logic_sb_block = (sb_block*BLOCK_SIZE) / blocksize;
		offset = (sb_block*BLOCK_SIZE) % blocksize;
		bh = sb_bread(sb, logic_sb_block);
		if(!bh) {
			printk("EXT2-fs: Couldn't read superblock on "
			       "2nd try.\n");
			goto failed_mount;
		}
		es = (struct ext2_super_block *) (((char *)bh->b_data) + offset);
		sb->u.ext2_sb.s_es = es;
		if (es->s_magic != le16_to_cpu(EXT2_SUPER_MAGIC)) {
			printk ("EXT2-fs: Magic mismatch, very weird !\n");
			goto failed_mount;
		}
	}

	if (le32_to_cpu(es->s_rev_level) == EXT2_GOOD_OLD_REV) {
		sbi->s_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
		sbi->s_first_ino = EXT2_GOOD_OLD_FIRST_INO;
	} else {
		sbi->s_inode_size = le16_to_cpu(es->s_inode_size);
		sbi->s_first_ino = le32_to_cpu(es->s_first_ino);
		if ((sbi->s_inode_size < EXT2_GOOD_OLD_INODE_SIZE) ||
		    (sbi->s_inode_size & (sbi->s_inode_size - 1)) ||
		    (sbi->s_inode_size > blocksize)) {
			printk ("EXT2-fs: unsupported inode size: %d\n",
				sbi->s_inode_size);
			goto failed_mount;
		}
	}
	sb->u.ext2_sb.s_frag_size = EXT2_MIN_FRAG_SIZE <<
				   le32_to_cpu(es->s_log_frag_size);
	if (sb->u.ext2_sb.s_frag_size)
		sb->u.ext2_sb.s_frags_per_block = sb->s_blocksize /
						  sb->u.ext2_sb.s_frag_size;
	else
		sb->s_magic = 0;
	sb->u.ext2_sb.s_blocks_per_group = le32_to_cpu(es->s_blocks_per_group);
	sb->u.ext2_sb.s_frags_per_group = le32_to_cpu(es->s_frags_per_group);
	sb->u.ext2_sb.s_inodes_per_group = le32_to_cpu(es->s_inodes_per_group);
	sb->u.ext2_sb.s_inodes_per_block = sb->s_blocksize /
					   EXT2_INODE_SIZE(sb);
	sb->u.ext2_sb.s_itb_per_group = sb->u.ext2_sb.s_inodes_per_group /
				        sb->u.ext2_sb.s_inodes_per_block;
	sb->u.ext2_sb.s_desc_per_block = sb->s_blocksize /
					 sizeof (struct ext2_group_desc);
	sb->u.ext2_sb.s_sbh = bh;
	if (resuid != EXT2_DEF_RESUID)
		sb->u.ext2_sb.s_resuid = resuid;
	else
		sb->u.ext2_sb.s_resuid = le16_to_cpu(es->s_def_resuid);
	if (resgid != EXT2_DEF_RESGID)
		sb->u.ext2_sb.s_resgid = resgid;
	else
		sb->u.ext2_sb.s_resgid = le16_to_cpu(es->s_def_resgid);
	sb->u.ext2_sb.s_mount_state = le16_to_cpu(es->s_state);
	sb->u.ext2_sb.s_addr_per_block_bits =
		log2 (EXT2_ADDR_PER_BLOCK(sb));
	sb->u.ext2_sb.s_desc_per_block_bits =
		log2 (EXT2_DESC_PER_BLOCK(sb));
	if (sb->s_magic != EXT2_SUPER_MAGIC) {
		if (!silent)
			printk ("VFS: Can't find an ext2 filesystem on dev "
				"%s.\n",
				bdevname(dev));
		goto failed_mount;
	}
	if (sb->s_blocksize != bh->b_size) {
		if (!silent)
			printk ("VFS: Unsupported blocksize on dev "
				"%s.\n", bdevname(dev));
		goto failed_mount;
	}

	if (sb->s_blocksize != sb->u.ext2_sb.s_frag_size) {
		printk ("EXT2-fs: fragsize %lu != blocksize %lu (not supported yet)\n",
			sb->u.ext2_sb.s_frag_size, sb->s_blocksize);
		goto failed_mount;
	}

	if (sb->u.ext2_sb.s_blocks_per_group > sb->s_blocksize * 8) {
		printk ("EXT2-fs: #blocks per group too big: %lu\n",
			sb->u.ext2_sb.s_blocks_per_group);
		goto failed_mount;
	}
	if (sb->u.ext2_sb.s_frags_per_group > sb->s_blocksize * 8) {
		printk ("EXT2-fs: #fragments per group too big: %lu\n",
			sb->u.ext2_sb.s_frags_per_group);
		goto failed_mount;
	}
	if (sb->u.ext2_sb.s_inodes_per_group > sb->s_blocksize * 8) {
		printk ("EXT2-fs: #inodes per group too big: %lu\n",
			sb->u.ext2_sb.s_inodes_per_group);
		goto failed_mount;
	}

	sb->u.ext2_sb.s_groups_count = (le32_to_cpu(es->s_blocks_count) -
				        le32_to_cpu(es->s_first_data_block) +
				       EXT2_BLOCKS_PER_GROUP(sb) - 1) /
				       EXT2_BLOCKS_PER_GROUP(sb);
	db_count = (sb->u.ext2_sb.s_groups_count + EXT2_DESC_PER_BLOCK(sb) - 1) /
		   EXT2_DESC_PER_BLOCK(sb);
	sb->u.ext2_sb.s_group_desc = kmalloc (db_count * sizeof (struct buffer_head *), GFP_KERNEL);
	if (sb->u.ext2_sb.s_group_desc == NULL) {
		printk ("EXT2-fs: not enough memory\n");
		goto failed_mount;
	}
	for (i = 0; i < db_count; i++) {
		block = descriptor_loc(sb, logic_sb_block, i);
		sbi->s_group_desc[i] = sb_bread(sb, block);
		if (!sbi->s_group_desc[i]) {
			for (j = 0; j < i; j++)
				brelse (sbi->s_group_desc[j]);
			kfree(sbi->s_group_desc);
			printk ("EXT2-fs: unable to read group descriptors\n");
			goto failed_mount;
		}
	}
	if (!ext2_check_descriptors (sb)) {
		printk ("EXT2-fs: group descriptors corrupted!\n");
		db_count = i;
		goto failed_mount2;
	}
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++) {
		sb->u.ext2_sb.s_inode_bitmap_number[i] = 0;
		sb->u.ext2_sb.s_inode_bitmap[i] = NULL;
		sb->u.ext2_sb.s_block_bitmap_number[i] = 0;
		sb->u.ext2_sb.s_block_bitmap[i] = NULL;
	}
	sb->u.ext2_sb.s_loaded_inode_bitmaps = 0;
	sb->u.ext2_sb.s_loaded_block_bitmaps = 0;
	sb->u.ext2_sb.s_gdb_count = db_count;
	/*
	 * set up enough so that it can read an inode
	 */
	sb->s_op = &ext2_sops;
	sb->s_root = d_alloc_root(iget(sb, EXT2_ROOT_INO));
	if (!sb->s_root || !S_ISDIR(sb->s_root->d_inode->i_mode) ||
	    !sb->s_root->d_inode->i_blocks || !sb->s_root->d_inode->i_size) {
		if (sb->s_root) {
			dput(sb->s_root);
			sb->s_root = NULL;
			printk(KERN_ERR "EXT2-fs: corrupt root inode, run e2fsck\n");
		} else
			printk(KERN_ERR "EXT2-fs: get root inode failed\n");
		goto failed_mount2;
	}
	ext2_setup_super (sb, es, sb->s_flags & MS_RDONLY);
	return sb;
failed_mount2:
	for (i = 0; i < db_count; i++)
		brelse(sb->u.ext2_sb.s_group_desc[i]);
	kfree(sb->u.ext2_sb.s_group_desc);
failed_mount:
	brelse(bh);
	return NULL;
}

static void ext2_commit_super (struct super_block * sb,
			       struct ext2_super_block * es)
{
	es->s_wtime = cpu_to_le32(CURRENT_TIME);
	mark_buffer_dirty(sb->u.ext2_sb.s_sbh);
	sb->s_dirt = 0;
}

static void ext2_sync_super(struct super_block *sb, struct ext2_super_block *es)
{
	es->s_wtime = cpu_to_le32(CURRENT_TIME);
	mark_buffer_dirty(EXT2_SB(sb)->s_sbh);
	ll_rw_block(WRITE, 1, &EXT2_SB(sb)->s_sbh);
	wait_on_buffer(EXT2_SB(sb)->s_sbh);
	sb->s_dirt = 0;
}

/*
 * In the second extended file system, it is not necessary to
 * write the super block since we use a mapping of the
 * disk super block in a buffer.
 *
 * However, this function is still used to set the fs valid
 * flags to 0.  We need to set this flag to 0 since the fs
 * may have been checked while mounted and e2fsck may have
 * set s_state to EXT2_VALID_FS after some corrections.
 */

void ext2_write_super (struct super_block * sb)
{
	struct ext2_super_block * es;

	if (!(sb->s_flags & MS_RDONLY)) {
		es = sb->u.ext2_sb.s_es;

		if (le16_to_cpu(es->s_state) & EXT2_VALID_FS) {
			ext2_debug ("setting valid to 0\n");
			es->s_state = cpu_to_le16(le16_to_cpu(es->s_state) &
						  ~EXT2_VALID_FS);
			es->s_mtime = cpu_to_le32(CURRENT_TIME);
			ext2_sync_super(sb, es);
		} else
			ext2_commit_super (sb, es);
	}
	sb->s_dirt = 0;
}

int ext2_remount (struct super_block * sb, int * flags, char * data)
{
	struct ext2_super_block * es;
	unsigned short resuid = sb->u.ext2_sb.s_resuid;
	unsigned short resgid = sb->u.ext2_sb.s_resgid;
	unsigned long new_mount_opt;
	unsigned long tmp;

	/*
	 * Allow the "check" option to be passed as a remount option.
	 */
	new_mount_opt = sb->u.ext2_sb.s_mount_opt;
	if (!parse_options (data, &tmp, &resuid, &resgid,
			    &new_mount_opt))
		return -EINVAL;

	sb->u.ext2_sb.s_mount_opt = new_mount_opt;
	sb->u.ext2_sb.s_resuid = resuid;
	sb->u.ext2_sb.s_resgid = resgid;
	es = sb->u.ext2_sb.s_es;
	if ((*flags & MS_RDONLY) == (sb->s_flags & MS_RDONLY))
		return 0;
	if (*flags & MS_RDONLY) {
		if (le16_to_cpu(es->s_state) & EXT2_VALID_FS ||
		    !(sb->u.ext2_sb.s_mount_state & EXT2_VALID_FS))
			return 0;
		/*
		 * OK, we are remounting a valid rw partition rdonly, so set
		 * the rdonly flag and then mark the partition as valid again.
		 */
		es->s_state = cpu_to_le16(sb->u.ext2_sb.s_mount_state);
		es->s_mtime = cpu_to_le32(CURRENT_TIME);
	} else {
		int ret;
		if ((ret = EXT2_HAS_RO_COMPAT_FEATURE(sb,
					       ~EXT2_FEATURE_RO_COMPAT_SUPP))) {
			printk("EXT2-fs: %s: couldn't remount RDWR because of "
			       "unsupported optional features (%x).\n",
			       bdevname(sb->s_dev), ret);
			return -EROFS;
		}
		/*
		 * Mounting a RDONLY partition read-write, so reread and
		 * store the current valid flag.  (It may have been changed
		 * by e2fsck since we originally mounted the partition.)
		 */
		sb->u.ext2_sb.s_mount_state = le16_to_cpu(es->s_state);
		if (!ext2_setup_super (sb, es, 0))
			sb->s_flags &= ~MS_RDONLY;
	}
	ext2_sync_super(sb, es);
	return 0;
}

int ext2_statfs (struct super_block * sb, struct statfs * buf)
{
	unsigned long overhead;
	int i;

	if (test_opt (sb, MINIX_DF))
		overhead = 0;
	else {
		/*
		 * Compute the overhead (FS structures)
		 */

		/*
		 * All of the blocks before first_data_block are
		 * overhead
		 */
		overhead = le32_to_cpu(sb->u.ext2_sb.s_es->s_first_data_block);

		/*
		 * Add the overhead attributed to the superblock and
		 * block group descriptors.  If the sparse superblocks
		 * feature is turned on, then not all groups have this.
		 */
		for (i = 0; i < EXT2_SB(sb)->s_groups_count; i++)
			overhead += ext2_bg_has_super(sb, i) +
				ext2_bg_num_gdb(sb, i);

		/*
		 * Every block group has an inode bitmap, a block
		 * bitmap, and an inode table.
		 */
		overhead += (sb->u.ext2_sb.s_groups_count *
			     (2 + sb->u.ext2_sb.s_itb_per_group));
	}

	buf->f_type = EXT2_SUPER_MAGIC;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = le32_to_cpu(sb->u.ext2_sb.s_es->s_blocks_count) - overhead;
	buf->f_bfree = ext2_count_free_blocks (sb);
	buf->f_bavail = buf->f_bfree - le32_to_cpu(sb->u.ext2_sb.s_es->s_r_blocks_count);
	if (buf->f_bfree < le32_to_cpu(sb->u.ext2_sb.s_es->s_r_blocks_count))
		buf->f_bavail = 0;
	buf->f_files = le32_to_cpu(sb->u.ext2_sb.s_es->s_inodes_count);
	buf->f_ffree = ext2_count_free_inodes (sb);
	buf->f_namelen = EXT2_NAME_LEN;
	return 0;
}

static DECLARE_FSTYPE_DEV(ext2_fs_type, "ext2", ext2_read_super);

static int __init init_ext2_fs(void)
{
        return register_filesystem(&ext2_fs_type);
}

static void __exit exit_ext2_fs(void)
{
	unregister_filesystem(&ext2_fs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_ext2_fs)
module_exit(exit_ext2_fs)
