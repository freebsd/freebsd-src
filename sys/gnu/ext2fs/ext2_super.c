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

#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <asm/uaccess.h>



static char error_buf[1024];

void ext2_error (struct super_block * sb, const char * function,
		 const char * fmt, ...)
{
	va_list args;

	if (!(sb->s_flags & MS_RDONLY)) {
		sb->u.ext2_sb.s_mount_state |= EXT2_ERROR_FS;
		sb->u.ext2_sb.s_es->s_state =
			cpu_to_le16(le16_to_cpu(sb->u.ext2_sb.s_es->s_state) | EXT2_ERROR_FS);
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
		sb->s_dirt = 1;
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
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
		sb->s_dirt = 1;
	}
	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);
	/* this is to prevent panic from syncing this filesystem */
	if (sb->s_lock)
		sb->s_lock=0;
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

void ext2_put_super (struct super_block * sb)
{
	int db_count;
	int i;

	if (!(sb->s_flags & MS_RDONLY)) {
		sb->u.ext2_sb.s_es->s_state = le16_to_cpu(sb->u.ext2_sb.s_mount_state);
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
	}
	db_count = sb->u.ext2_sb.s_db_per_group;
	for (i = 0; i < db_count; i++)
		if (sb->u.ext2_sb.s_group_desc[i])
			brelse (sb->u.ext2_sb.s_group_desc[i]);
	kfree_s (sb->u.ext2_sb.s_group_desc,
		 db_count * sizeof (struct buffer_head *));
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
		if (sb->u.ext2_sb.s_inode_bitmap[i])
			brelse (sb->u.ext2_sb.s_inode_bitmap[i]);
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
		if (sb->u.ext2_sb.s_block_bitmap[i])
			brelse (sb->u.ext2_sb.s_block_bitmap[i]);
	brelse (sb->u.ext2_sb.s_sbh);

	MOD_DEC_USE_COUNT;
	return;
}

static struct super_operations ext2_sops = {
	ext2_read_inode,
	ext2_write_inode,
	ext2_put_inode,
	ext2_delete_inode,
	NULL,
	ext2_put_super,
	ext2_write_super,
	ext2_statfs,
	ext2_remount
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
		else if (!strcmp (this_char, "check")) {
			if (!value || !*value)
				set_opt (*mount_options, CHECK_NORMAL);
			else if (!strcmp (value, "none")) {
				clear_opt (*mount_options, CHECK_NORMAL);
				clear_opt (*mount_options, CHECK_STRICT);
			}
			else if (!strcmp (value, "normal"))
				set_opt (*mount_options, CHECK_NORMAL);
			else if (!strcmp (value, "strict")) {
				set_opt (*mount_options, CHECK_NORMAL);
				set_opt (*mount_options, CHECK_STRICT);
			}
			else {
				printk ("EXT2-fs: Invalid check option: %s\n",
					value);
				return 0;
			}
		}
		else if (!strcmp (this_char, "debug"))
			set_opt (*mount_options, DEBUG);
		else if (!strcmp (this_char, "errors")) {
			if (!value || !*value) {
				printk ("EXT2-fs: the errors option requires "
					"an argument");
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
		else if (!strcmp (this_char, "nocheck")) {
			clear_opt (*mount_options, CHECK_NORMAL);
			clear_opt (*mount_options, CHECK_STRICT);
		}
		else if (!strcmp (this_char, "nogrpid") ||
			 !strcmp (this_char, "sysvgroups"))
			clear_opt (*mount_options, GRPID);
		else if (!strcmp (this_char, "resgid")) {
			if (!value || !*value) {
				printk ("EXT2-fs: the resgid option requires "
					"an argument");
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

static void ext2_setup_super (struct super_block * sb,
			      struct ext2_super_block * es)
{
	if (le32_to_cpu(es->s_rev_level) > EXT2_MAX_SUPP_REV) {
			printk ("EXT2-fs warning: revision level too high, "
				"forcing read/only mode\n");
			sb->s_flags |= MS_RDONLY;
	}
	if (!(sb->s_flags & MS_RDONLY)) {
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
		es->s_state = cpu_to_le16(le16_to_cpu(es->s_state) & ~EXT2_VALID_FS);
		if (!(__s16) le16_to_cpu(es->s_max_mnt_count))
			es->s_max_mnt_count = (__s16) cpu_to_le16(EXT2_DFL_MAX_MNT_COUNT);
		es->s_mnt_count=cpu_to_le16(le16_to_cpu(es->s_mnt_count) + 1);
		es->s_mtime = cpu_to_le32(CURRENT_TIME);
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
		sb->s_dirt = 1;
		if (test_opt (sb, DEBUG))
			printk ("[EXT II FS %s, %s, bs=%lu, fs=%lu, gc=%lu, "
				"bpg=%lu, ipg=%lu, mo=%04lx]\n",
				EXT2FS_VERSION, EXT2FS_DATE, sb->s_blocksize,
				sb->u.ext2_sb.s_frag_size,
				sb->u.ext2_sb.s_groups_count,
				EXT2_BLOCKS_PER_GROUP(sb),
				EXT2_INODES_PER_GROUP(sb),
				sb->u.ext2_sb.s_mount_opt);
		if (test_opt (sb, CHECK)) {
			ext2_check_blocks_bitmap (sb);
			ext2_check_inodes_bitmap (sb);
		}
	}
#if 0 /* ibasket's still have unresolved bugs... -DaveM */

	/* [T. Schoebel-Theuer] This limit should be maintained on disk.
	 * This is just provisionary.
	 */
	sb->s_ibasket_max = 100;
#endif
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

struct super_block * ext2_read_super (struct super_block * sb, void * data,
				      int silent)
{
	struct buffer_head * bh;
	struct ext2_super_block * es;
	unsigned long sb_block = 1;
	unsigned short resuid = EXT2_DEF_RESUID;
	unsigned short resgid = EXT2_DEF_RESGID;
	unsigned long logic_sb_block = 1;
	unsigned long offset = 0;
	kdev_t dev = sb->s_dev;
	int blocksize = BLOCK_SIZE;
	int hblock;
	int db_count;
	int i, j;

	/*
	 * See what the current blocksize for the device is, and
	 * use that as the blocksize.  Otherwise (or if the blocksize
	 * is smaller than the default) use the default.
	 * This is important for devices that have a hardware
	 * sectorsize that is larger than the default.
	 */
	blocksize = get_hardblocksize(dev);
	if( blocksize == 0 || blocksize < BLOCK_SIZE )
	  {
	    blocksize = BLOCK_SIZE;
	  }

	sb->u.ext2_sb.s_mount_opt = 0;
	set_opt (sb->u.ext2_sb.s_mount_opt, CHECK_NORMAL);
	if (!parse_options ((char *) data, &sb_block, &resuid, &resgid,
	    &sb->u.ext2_sb.s_mount_opt)) {
		sb->s_dev = 0;
		return NULL;
	}

	MOD_INC_USE_COUNT;
	lock_super (sb);
	set_blocksize (dev, blocksize);

	/*
	 * If the superblock doesn't start on a sector boundary,
	 * calculate the offset.  FIXME(eric) this doesn't make sense
	 * that we would have to do this.
	 */
	if (blocksize != BLOCK_SIZE) {
		logic_sb_block = (sb_block*BLOCK_SIZE) / blocksize;
		offset = (sb_block*BLOCK_SIZE) % blocksize;
	}

	if (!(bh = bread (dev, logic_sb_block, blocksize))) {
		sb->s_dev = 0;
		unlock_super (sb);
		printk ("EXT2-fs: unable to read superblock\n");
		MOD_DEC_USE_COUNT;
		return NULL;
	}
	/*
	 * Note: s_es must be initialized s_es as soon as possible because
	 * some ext2 macro-instructions depend on its value
	 */
	es = (struct ext2_super_block *) (((char *)bh->b_data) + offset);
	sb->u.ext2_sb.s_es = es;
	sb->s_magic = le16_to_cpu(es->s_magic);
	if (sb->s_magic != EXT2_SUPER_MAGIC) {
		if (!silent)
			printk ("VFS: Can't find an ext2 filesystem on dev "
				"%s.\n", bdevname(dev));
	failed_mount:
		sb->s_dev = 0;
		unlock_super (sb);
		if (bh)
			brelse(bh);
		MOD_DEC_USE_COUNT;
		return NULL;
	}
	if (le32_to_cpu(es->s_rev_level) > EXT2_GOOD_OLD_REV) {
		if (le32_to_cpu(es->s_feature_incompat) & ~EXT2_FEATURE_INCOMPAT_SUPP) {
			printk("EXT2-fs: %s: couldn't mount because of "
			       "unsupported optional features.\n",
			       bdevname(dev));
			goto failed_mount;
		}
		if (!(sb->s_flags & MS_RDONLY) &&
		    (le32_to_cpu(es->s_feature_ro_compat) & ~EXT2_FEATURE_RO_COMPAT_SUPP)) {
			printk("EXT2-fs: %s: couldn't mount RDWR because of "
			       "unsupported optional features.\n",
			       bdevname(dev));
			goto failed_mount;
		}
	}
	sb->s_blocksize_bits = le32_to_cpu(sb->u.ext2_sb.s_es->s_log_block_size) + 10;
	sb->s_blocksize = 1 << sb->s_blocksize_bits;
	if (sb->s_blocksize != BLOCK_SIZE &&
	    (sb->s_blocksize == 1024 || sb->s_blocksize == 2048 ||
	     sb->s_blocksize == 4096)) {
		/*
		 * Make sure the blocksize for the filesystem is larger
		 * than the hardware sectorsize for the machine.
		 */
		hblock = get_hardblocksize(dev);
		if(    (hblock != 0)
		    && (sb->s_blocksize < hblock) )
		{
			printk("EXT2-fs: blocksize too small for device.\n");
			goto failed_mount;
		}

		brelse (bh);
		set_blocksize (dev, sb->s_blocksize);
		logic_sb_block = (sb_block*BLOCK_SIZE) / sb->s_blocksize;
		offset = (sb_block*BLOCK_SIZE) % sb->s_blocksize;
		bh = bread (dev, logic_sb_block, sb->s_blocksize);
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
		sb->u.ext2_sb.s_inode_size = EXT2_GOOD_OLD_INODE_SIZE;
		sb->u.ext2_sb.s_first_ino = EXT2_GOOD_OLD_FIRST_INO;
	} else {
		sb->u.ext2_sb.s_inode_size = le16_to_cpu(es->s_inode_size);
		sb->u.ext2_sb.s_first_ino = le32_to_cpu(es->s_first_ino);
		if (sb->u.ext2_sb.s_inode_size != EXT2_GOOD_OLD_INODE_SIZE) {
			printk ("EXT2-fs: unsupported inode size: %d\n",
				sb->u.ext2_sb.s_inode_size);
			goto failed_mount;
		}
	}
	sb->u.ext2_sb.s_feature_compat = le32_to_cpu(es->s_feature_compat);
	sb->u.ext2_sb.s_feature_incompat = le32_to_cpu(es->s_feature_incompat);
	sb->u.ext2_sb.s_feature_ro_compat = le32_to_cpu(es->s_feature_ro_compat);
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
		sb->u.ext2_sb.s_group_desc[i] = bread (dev, logic_sb_block + i + 1,
						       sb->s_blocksize);
		if (!sb->u.ext2_sb.s_group_desc[i]) {
			for (j = 0; j < i; j++)
				brelse (sb->u.ext2_sb.s_group_desc[j]);
			kfree_s (sb->u.ext2_sb.s_group_desc,
				 db_count * sizeof (struct buffer_head *));
			printk ("EXT2-fs: unable to read group descriptors\n");
			goto failed_mount;
		}
	}
	if (!ext2_check_descriptors (sb)) {
		for (j = 0; j < db_count; j++)
			brelse (sb->u.ext2_sb.s_group_desc[j]);
		kfree_s (sb->u.ext2_sb.s_group_desc,
			 db_count * sizeof (struct buffer_head *));
		printk ("EXT2-fs: group descriptors corrupted !\n");
		goto failed_mount;
	}
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++) {
		sb->u.ext2_sb.s_inode_bitmap_number[i] = 0;
		sb->u.ext2_sb.s_inode_bitmap[i] = NULL;
		sb->u.ext2_sb.s_block_bitmap_number[i] = 0;
		sb->u.ext2_sb.s_block_bitmap[i] = NULL;
	}
	sb->u.ext2_sb.s_loaded_inode_bitmaps = 0;
	sb->u.ext2_sb.s_loaded_block_bitmaps = 0;
	sb->u.ext2_sb.s_db_per_group = db_count;
	unlock_super (sb);
	/*
	 * set up enough so that it can read an inode
	 */
	sb->s_dev = dev;
	sb->s_op = &ext2_sops;
	sb->s_root = d_alloc_root(iget(sb, EXT2_ROOT_INO));
	if (!sb->s_root) {
		sb->s_dev = 0;
		for (i = 0; i < db_count; i++)
			if (sb->u.ext2_sb.s_group_desc[i])
				brelse (sb->u.ext2_sb.s_group_desc[i]);
		kfree_s (sb->u.ext2_sb.s_group_desc,
			 db_count * sizeof (struct buffer_head *));
		brelse (bh);
		printk ("EXT2-fs: get root inode failed\n");
		MOD_DEC_USE_COUNT;
		return NULL;
	}
	ext2_setup_super (sb, es);
	return sb;
}

static void ext2_commit_super (struct super_block * sb,
			       struct ext2_super_block * es)
{
	es->s_wtime = cpu_to_le32(CURRENT_TIME);
	mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
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

		ext2_debug ("setting valid to 0\n");

		if (le16_to_cpu(es->s_state) & EXT2_VALID_FS) {
			es->s_state = cpu_to_le16(le16_to_cpu(es->s_state) & ~EXT2_VALID_FS);
			es->s_mtime = cpu_to_le32(CURRENT_TIME);
		}
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
	new_mount_opt = EXT2_MOUNT_CHECK_NORMAL;
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
		mark_buffer_dirty(sb->u.ext2_sb.s_sbh, 1);
		sb->s_dirt = 1;
		ext2_commit_super (sb, es);
	}
	else {
		/*
		 * Mounting a RDONLY partition read-write, so reread and
		 * store the current valid flag.  (It may have been changed
		 * by e2fsck since we originally mounted the partition.)
		 */
		sb->u.ext2_sb.s_mount_state = le16_to_cpu(es->s_state);
		sb->s_flags &= ~MS_RDONLY;
		ext2_setup_super (sb, es);
	}
	return 0;
}

int ext2_statfs (struct super_block * sb, struct statfs * buf, int bufsiz)
{
	unsigned long overhead;
	struct statfs tmp;
	int	ngroups, i;

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
		 * block group descriptors.  If this is sparse
		 * superblocks is turned on, then not all groups have
		 * this.
		 */
		if (sb->u.ext2_sb.s_feature_ro_compat &
		    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) {
			ngroups = 0;
			for (i=0 ; i < sb->u.ext2_sb.s_groups_count; i++)
				if (ext2_group_sparse(i))
					ngroups++;
		} else
			ngroups = sb->u.ext2_sb.s_groups_count;
		overhead += ngroups * (1 + sb->u.ext2_sb.s_db_per_group);

		/*
		 * Every block group has an inode bitmap, a block
		 * bitmap, and an inode table.
		 */
		overhead += (sb->u.ext2_sb.s_groups_count *
			     (2 + sb->u.ext2_sb.s_itb_per_group));
	}

	tmp.f_type = EXT2_SUPER_MAGIC;
	tmp.f_bsize = sb->s_blocksize;
	tmp.f_blocks = le32_to_cpu(sb->u.ext2_sb.s_es->s_blocks_count) - overhead;
	tmp.f_bfree = ext2_count_free_blocks (sb);
	tmp.f_bavail = tmp.f_bfree - le32_to_cpu(sb->u.ext2_sb.s_es->s_r_blocks_count);
	if (tmp.f_bfree < le32_to_cpu(sb->u.ext2_sb.s_es->s_r_blocks_count))
		tmp.f_bavail = 0;
	tmp.f_files = le32_to_cpu(sb->u.ext2_sb.s_es->s_inodes_count);
	tmp.f_ffree = ext2_count_free_inodes (sb);
	tmp.f_namelen = EXT2_NAME_LEN;
	return copy_to_user(buf, &tmp, bufsiz) ? -EFAULT : 0;
}

static struct file_system_type ext2_fs_type = {
	"ext2", 
	FS_REQUIRES_DEV /* | FS_IBASKET */,	/* ibaskets have unresolved bugs */
        ext2_read_super, 
	NULL
};

static int __init init_ext2_fs(void)
{
        return register_filesystem(&ext2_fs_type);
}

static int __exit exit_ext2_fs(void)
{
	unregister_filesystem(&ext2_fs_type);
}

EXPORT_NO_SYMBOLS;

module_init(init_ext2_fs)
module_exit(exit_ext2_fs)
