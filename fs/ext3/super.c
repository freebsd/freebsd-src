/*
 *  linux/fs/ext3/super.c
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
#include <linux/sched.h>
#include <linux/jbd.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/locks.h>
#include <linux/blkdev.h>
#include <linux/smp_lock.h>
#include <linux/random.h>
#include <asm/uaccess.h>

#ifdef CONFIG_JBD_DEBUG
static int ext3_ro_after; /* Make fs read-only after this many jiffies */
#endif

static int ext3_load_journal(struct super_block *, struct ext3_super_block *);
static int ext3_create_journal(struct super_block *, struct ext3_super_block *,
			       int);
static void ext3_commit_super (struct super_block * sb,
			       struct ext3_super_block * es,
			       int sync);
static void ext3_mark_recovery_complete(struct super_block * sb,
					struct ext3_super_block * es);
static void ext3_clear_journal_err(struct super_block * sb,
				   struct ext3_super_block * es);

static int ext3_sync_fs(struct super_block * sb);

#ifdef CONFIG_JBD_DEBUG
int journal_no_write[2];

/*
 * Debug code for turning filesystems "read-only" after a specified
 * amount of time.  This is for crash/recovery testing.
 */

static void make_rdonly(kdev_t dev, int *no_write)
{
	if (dev) {
		printk(KERN_WARNING "Turning device %s read-only\n", 
		       bdevname(dev));
		*no_write = 0xdead0000 + dev;
	}
}

static void turn_fs_readonly(unsigned long arg)
{
	struct super_block *sb = (struct super_block *)arg;

	make_rdonly(sb->s_dev, &journal_no_write[0]);
	make_rdonly(EXT3_SB(sb)->s_journal->j_dev, &journal_no_write[1]);
	wake_up(&EXT3_SB(sb)->ro_wait_queue);
}

static void setup_ro_after(struct super_block *sb)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	init_timer(&sbi->turn_ro_timer);
	if (ext3_ro_after) {
		printk(KERN_DEBUG "fs will go read-only in %d jiffies\n",
		       ext3_ro_after);
		init_waitqueue_head(&sbi->ro_wait_queue);
		journal_no_write[0] = 0;
		journal_no_write[1] = 0;
		sbi->turn_ro_timer.function = turn_fs_readonly;
		sbi->turn_ro_timer.data = (unsigned long)sb;
		sbi->turn_ro_timer.expires = jiffies + ext3_ro_after;
		ext3_ro_after = 0;
		add_timer(&sbi->turn_ro_timer);
	}
}

static void clear_ro_after(struct super_block *sb)
{
	del_timer_sync(&EXT3_SB(sb)->turn_ro_timer);
	journal_no_write[0] = 0;
	journal_no_write[1] = 0;
	ext3_ro_after = 0;
}
#else
#define setup_ro_after(sb)	do {} while (0)
#define clear_ro_after(sb)	do {} while (0)
#endif


static char error_buf[1024];

/* Determine the appropriate response to ext3_error on a given filesystem */

static int ext3_error_behaviour(struct super_block *sb)
{
	/* First check for mount-time options */
	if (test_opt (sb, ERRORS_PANIC))
		return EXT3_ERRORS_PANIC;
	if (test_opt (sb, ERRORS_RO))
		return EXT3_ERRORS_RO;
	if (test_opt (sb, ERRORS_CONT))
		return EXT3_ERRORS_CONTINUE;
	
	/* If no overrides were specified on the mount, then fall back
	 * to the default behaviour set in the filesystem's superblock
	 * on disk. */
	switch (le16_to_cpu(sb->u.ext3_sb.s_es->s_errors)) {
	case EXT3_ERRORS_PANIC:
		return EXT3_ERRORS_PANIC;
	case EXT3_ERRORS_RO:
		return EXT3_ERRORS_RO;
	default:
		break;
	}
	return EXT3_ERRORS_CONTINUE;
}

/* Deal with the reporting of failure conditions on a filesystem such as
 * inconsistencies detected or read IO failures.
 *
 * On ext2, we can store the error state of the filesystem in the
 * superblock.  That is not possible on ext3, because we may have other
 * write ordering constraints on the superblock which prevent us from
 * writing it out straight away; and given that the journal is about to
 * be aborted, we can't rely on the current, or future, transactions to
 * write out the superblock safely.
 *
 * We'll just use the journal_abort() error code to record an error in
 * the journal instead.  On recovery, the journal will compain about
 * that error until we've noted it down and cleared it.
 */

static void ext3_handle_error(struct super_block *sb)
{
	struct ext3_super_block *es = EXT3_SB(sb)->s_es;

	EXT3_SB(sb)->s_mount_state |= EXT3_ERROR_FS;
	es->s_state |= cpu_to_le32(EXT3_ERROR_FS);

	if (sb->s_flags & MS_RDONLY)
		return;

	if (ext3_error_behaviour(sb) != EXT3_ERRORS_CONTINUE) {
		EXT3_SB(sb)->s_mount_opt |= EXT3_MOUNT_ABORT;
		journal_abort(EXT3_SB(sb)->s_journal, -EIO);
	}

	if (ext3_error_behaviour(sb) == EXT3_ERRORS_PANIC) 
		panic ("EXT3-fs (device %s): panic forced after error\n",
		       bdevname(sb->s_dev));

	if (ext3_error_behaviour(sb) == EXT3_ERRORS_RO) {
		printk (KERN_CRIT "Remounting filesystem read-only\n");
		sb->s_flags |= MS_RDONLY;
	}

	ext3_commit_super(sb, es, 1);
}

void ext3_error (struct super_block * sb, const char * function,
		 const char * fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);

	printk (KERN_CRIT "EXT3-fs error (device %s): %s: %s\n",
		bdevname(sb->s_dev), function, error_buf);

	ext3_handle_error(sb);
}

const char *ext3_decode_error(struct super_block * sb, int errno, char nbuf[16])
{
	char *errstr = NULL;
	
	switch (errno) {
	case -EIO:
		errstr = "IO failure";
		break;
	case -ENOMEM:
		errstr = "Out of memory";
		break;
	case -EROFS:
		if (!sb || EXT3_SB(sb)->s_journal->j_flags & JFS_ABORT)
			errstr = "Journal has aborted";
		else
			errstr = "Readonly filesystem";
		break;
	default:
		/* If the caller passed in an extra buffer for unknown
		 * errors, textualise them now.  Else we just return
		 * NULL. */
		if (nbuf) {
			/* Check for truncated error codes... */
			if (snprintf(nbuf, 16, "error %d", -errno) >= 0)
				errstr = nbuf;
		}
		
		break;
	}

	return errstr;
}

/* __ext3_std_error decodes expected errors from journaling functions
 * automatically and invokes the appropriate error response.  */

void __ext3_std_error (struct super_block * sb, const char * function,
		       int errno)
{
	char nbuf[16];
	const char *errstr = ext3_decode_error(sb, errno, nbuf);

	printk (KERN_CRIT "EXT3-fs error (device %s) in %s: %s\n",
		bdevname(sb->s_dev), function, errstr);
	
	ext3_handle_error(sb);
}

/*
 * ext3_abort is a much stronger failure handler than ext3_error.  The
 * abort function may be used to deal with unrecoverable failures such
 * as journal IO errors or ENOMEM at a critical moment in log management.
 *
 * We unconditionally force the filesystem into an ABORT|READONLY state,
 * unless the error response on the fs has been set to panic in which
 * case we take the easy way out and panic immediately.
 */

void ext3_abort (struct super_block * sb, const char * function,
		 const char * fmt, ...)
{
	va_list args;

	printk (KERN_CRIT "ext3_abort called.\n");

	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);

	if (ext3_error_behaviour(sb) == EXT3_ERRORS_PANIC)
		panic ("EXT3-fs panic (device %s): %s: %s\n",
		       bdevname(sb->s_dev), function, error_buf);

	printk (KERN_CRIT "EXT3-fs abort (device %s): %s: %s\n",
		bdevname(sb->s_dev), function, error_buf);

	if (sb->s_flags & MS_RDONLY)
		return;
	
	printk (KERN_CRIT "Remounting filesystem read-only\n");
	sb->u.ext3_sb.s_mount_state |= EXT3_ERROR_FS;
	sb->s_flags |= MS_RDONLY;
	sb->u.ext3_sb.s_mount_opt |= EXT3_MOUNT_ABORT;
	journal_abort(EXT3_SB(sb)->s_journal, -EIO);
}

/* Deal with the reporting of failure conditions while running, such as
 * inconsistencies in operation or invalid system states.
 *
 * Use ext3_error() for cases of invalid filesystem states, as that will
 * record an error on disk and force a filesystem check on the next boot.
 */
NORET_TYPE void ext3_panic (struct super_block * sb, const char * function,
			    const char * fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);

	/* this is to prevent panic from syncing this filesystem */
	/* AKPM: is this sufficient? */
	sb->s_flags |= MS_RDONLY;
	panic ("EXT3-fs panic (device %s): %s: %s\n",
	       bdevname(sb->s_dev), function, error_buf);
}

void ext3_warning (struct super_block * sb, const char * function,
		   const char * fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	vsprintf (error_buf, fmt, args);
	va_end (args);
	printk (KERN_WARNING "EXT3-fs warning (device %s): %s: %s\n",
		bdevname(sb->s_dev), function, error_buf);
}

void ext3_update_dynamic_rev(struct super_block *sb)
{
	struct ext3_super_block *es = EXT3_SB(sb)->s_es;

	if (le32_to_cpu(es->s_rev_level) > EXT3_GOOD_OLD_REV)
		return;

	ext3_warning(sb, __FUNCTION__,
		     "updating to rev %d because of new feature flag, "
		     "running e2fsck is recommended",
		     EXT3_DYNAMIC_REV);

	es->s_first_ino = cpu_to_le32(EXT3_GOOD_OLD_FIRST_INO);
	es->s_inode_size = cpu_to_le16(EXT3_GOOD_OLD_INODE_SIZE);
	es->s_rev_level = cpu_to_le32(EXT3_DYNAMIC_REV);
	/* leave es->s_feature_*compat flags alone */
	/* es->s_uuid will be set by e2fsck if empty */

	/*
	 * The rest of the superblock fields should be zero, and if not it
	 * means they are likely already in use, so leave them alone.  We
	 * can leave it up to e2fsck to clean up any inconsistencies there.
	 */
}

/*
 * Open the external journal device
 */
static struct block_device *ext3_blkdev_get(kdev_t dev)
{
	struct block_device *bdev;
	int err = -ENODEV;

	bdev = bdget(kdev_t_to_nr(dev));
	if (bdev == NULL)
		goto fail;
	err = blkdev_get(bdev, FMODE_READ|FMODE_WRITE, 0, BDEV_FS);
	if (err < 0)
		goto fail;
	return bdev;

fail:
	printk(KERN_ERR "EXT3: failed to open journal device %s: %d\n",
			bdevname(dev), err);
	return NULL;
}

/*
 * Release the journal device
 */
static int ext3_blkdev_put(struct block_device *bdev)
{
	return blkdev_put(bdev, BDEV_FS);
}

static int ext3_blkdev_remove(struct ext3_sb_info *sbi)
{
	struct block_device *bdev;
	int ret = -ENODEV;

	bdev = sbi->journal_bdev;
	if (bdev) {
		ret = ext3_blkdev_put(bdev);
		sbi->journal_bdev = 0;
	}
	return ret;
}

#define orphan_list_entry(l) list_entry((l), struct inode, u.ext3_i.i_orphan)

static void dump_orphan_list(struct super_block *sb, struct ext3_sb_info *sbi)
{
	struct list_head *l;
	
	printk(KERN_ERR "sb orphan head is %d\n", 
	       le32_to_cpu(sbi->s_es->s_last_orphan));
	
	printk(KERN_ERR "sb_info orphan list:\n");
	list_for_each(l, &sbi->s_orphan) {
		struct inode *inode = orphan_list_entry(l);
		printk(KERN_ERR "  "
		       "inode 0x%04x:%ld at %p: mode %o, nlink %d, next %d\n",
		       inode->i_dev, inode->i_ino, inode,
		       inode->i_mode, inode->i_nlink, 
		       le32_to_cpu(NEXT_ORPHAN(inode)));
	}
}

void ext3_put_super (struct super_block * sb)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	struct ext3_super_block *es = sbi->s_es;
	kdev_t j_dev = sbi->s_journal->j_dev;
	int i;

	journal_destroy(sbi->s_journal);
	if (!(sb->s_flags & MS_RDONLY)) {
		EXT3_CLEAR_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER);
		es->s_state = le16_to_cpu(sbi->s_mount_state);
		BUFFER_TRACE(sbi->s_sbh, "marking dirty");
		mark_buffer_dirty(sbi->s_sbh);
		ext3_commit_super(sb, es, 1);
	}

	for (i = 0; i < sbi->s_gdb_count; i++)
		brelse(sbi->s_group_desc[i]);
	kfree(sbi->s_group_desc);
	for (i = 0; i < EXT3_MAX_GROUP_LOADED; i++)
		brelse(sbi->s_inode_bitmap[i]);
	for (i = 0; i < EXT3_MAX_GROUP_LOADED; i++)
		brelse(sbi->s_block_bitmap[i]);
	brelse(sbi->s_sbh);

	/* Debugging code just in case the in-memory inode orphan list
	 * isn't empty.  The on-disk one can be non-empty if we've
	 * detected an error and taken the fs readonly, but the
	 * in-memory list had better be clean by this point. */
	if (!list_empty(&sbi->s_orphan))
		dump_orphan_list(sb, sbi);
	J_ASSERT(list_empty(&sbi->s_orphan));

	invalidate_buffers(sb->s_dev);
	if (j_dev != sb->s_dev) {
		/*
		 * Invalidate the journal device's buffers.  We don't want them
		 * floating about in memory - the physical journal device may
		 * hotswapped, and it breaks the `ro-after' testing code.
		 */
		fsync_no_super(j_dev);
		invalidate_buffers(j_dev);
		ext3_blkdev_remove(sbi);
	}
	clear_ro_after(sb);

	return;
}

static struct dquot_operations ext3_qops;

static struct super_operations ext3_sops = {
	read_inode:	ext3_read_inode,	/* BKL held */
	write_inode:	ext3_write_inode,	/* BKL not held.  Don't need */
	dirty_inode:	ext3_dirty_inode,	/* BKL not held.  We take it */
	put_inode:	ext3_put_inode,		/* BKL not held.  Don't need */
	delete_inode:	ext3_delete_inode,	/* BKL not held.  We take it */
	put_super:	ext3_put_super,		/* BKL held */
	write_super:	ext3_write_super,	/* BKL held */
	sync_fs:	ext3_sync_fs,
	write_super_lockfs: ext3_write_super_lockfs, /* BKL not held. Take it */
	unlockfs:	ext3_unlockfs,		/* BKL not held.  We take it */
	statfs:		ext3_statfs,		/* BKL held */
	remount_fs:	ext3_remount,		/* BKL held */
};

static int want_value(char *value, char *option)
{
	if (!value || !*value) {
		printk(KERN_NOTICE "EXT3-fs: the %s option needs an argument\n",
		       option);
		return -1;
	}
	return 0;
}

static int want_null_value(char *value, char *option)
{
	if (*value) {
		printk(KERN_NOTICE "EXT3-fs: Invalid %s argument: %s\n",
		       option, value);
		return -1;
	}
	return 0;
}

static int want_numeric(char *value, char *option, unsigned long *number)
{
	if (want_value(value, option))
		return -1;
	*number = simple_strtoul(value, &value, 0);
	if (want_null_value(value, option))
		return -1;
	return 0;
}

/*
 * This function has been shamelessly adapted from the msdos fs
 */
static int parse_options (char * options, unsigned long * sb_block,
			  struct ext3_sb_info *sbi,
			  unsigned long * inum,
			  int is_remount)
{
	unsigned long *mount_options = &sbi->s_mount_opt;
	uid_t *resuid = &sbi->s_resuid;
	gid_t *resgid = &sbi->s_resgid;
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
		else if (!strcmp (this_char, "abort"))
			set_opt (*mount_options, ABORT);
		else if (!strcmp (this_char, "check")) {
			if (!value || !*value || !strcmp (value, "none"))
				clear_opt (*mount_options, CHECK);
			else
#ifdef CONFIG_EXT3_CHECK
				set_opt (*mount_options, CHECK);
#else
				printk(KERN_ERR 
				       "EXT3 Check option not supported\n");
#endif
		}
		else if (!strcmp (this_char, "debug"))
			set_opt (*mount_options, DEBUG);
		else if (!strcmp (this_char, "errors")) {
			if (want_value(value, "errors"))
				return 0;
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
				printk (KERN_ERR
					"EXT3-fs: Invalid errors option: %s\n",
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
			unsigned long v;
			if (want_numeric(value, "resgid", &v))
				return 0;
			*resgid = v;
		}
		else if (!strcmp (this_char, "resuid")) {
			unsigned long v;
			if (want_numeric(value, "resuid", &v))
				return 0;
			*resuid = v;
		}
		else if (!strcmp (this_char, "sb")) {
			if (want_numeric(value, "sb", sb_block))
				return 0;
		}
#ifdef CONFIG_JBD_DEBUG
		else if (!strcmp (this_char, "ro-after")) {
			unsigned long v;
			if (want_numeric(value, "ro-after", &v))
				return 0;
			ext3_ro_after = v;
		}
#endif
		/* Silently ignore the quota options */
		else if (!strcmp (this_char, "grpquota")
		         || !strcmp (this_char, "noquota")
		         || !strcmp (this_char, "quota")
		         || !strcmp (this_char, "usrquota"))
			/* Don't do anything ;-) */ ;
		else if (!strcmp (this_char, "journal")) {
			/* @@@ FIXME */
			/* Eventually we will want to be able to create
                           a journal file here.  For now, only allow the
                           user to specify an existing inode to be the
                           journal file. */
			if (is_remount) {
				printk(KERN_ERR "EXT3-fs: cannot specify "
				       "journal on remount\n");
				return 0;
			}

			if (want_value(value, "journal"))
				return 0;
			if (!strcmp (value, "update"))
				set_opt (*mount_options, UPDATE_JOURNAL);
			else if (want_numeric(value, "journal", inum))
				return 0;
		}
		else if (!strcmp (this_char, "noload"))
			set_opt (*mount_options, NOLOAD);
		else if (!strcmp (this_char, "data")) {
			int data_opt = 0;

			if (want_value(value, "data"))
				return 0;
			if (!strcmp (value, "journal"))
				data_opt = EXT3_MOUNT_JOURNAL_DATA;
			else if (!strcmp (value, "ordered"))
				data_opt = EXT3_MOUNT_ORDERED_DATA;
			else if (!strcmp (value, "writeback"))
				data_opt = EXT3_MOUNT_WRITEBACK_DATA;
			else {
				printk (KERN_ERR 
					"EXT3-fs: Invalid data option: %s\n",
					value);
				return 0;
			}
			if (is_remount) {
				if ((*mount_options & EXT3_MOUNT_DATA_FLAGS) !=
							data_opt) {
					printk(KERN_ERR
					       "EXT3-fs: cannot change data "
					       "mode on remount\n");
					return 0;
				}
			} else {
				*mount_options &= ~EXT3_MOUNT_DATA_FLAGS;
				*mount_options |= data_opt;
			}
		} else if (!strcmp (this_char, "commit")) {
			unsigned long v;
			if (want_numeric(value, "commit", &v))
				return 0;
			sbi->s_commit_interval = (HZ * v);
		} else {
			printk (KERN_ERR 
				"EXT3-fs: Unrecognized mount option %s\n",
				this_char);
			return 0;
		}
	}
	return 1;
}

static int ext3_setup_super(struct super_block *sb, struct ext3_super_block *es,
			    int read_only)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	int res = 0;

	if (le32_to_cpu(es->s_rev_level) > EXT3_MAX_SUPP_REV) {
		printk (KERN_ERR "EXT3-fs warning: revision level too high, "
			"forcing read-only mode\n");
		res = MS_RDONLY;
	}
	if (read_only)
		return res;
	if (!(sbi->s_mount_state & EXT3_VALID_FS))
		printk (KERN_WARNING "EXT3-fs warning: mounting unchecked fs, "
			"running e2fsck is recommended\n");
	else if ((sbi->s_mount_state & EXT3_ERROR_FS))
		printk (KERN_WARNING
			"EXT3-fs warning: mounting fs with errors, "
			"running e2fsck is recommended\n");
	else if ((__s16) le16_to_cpu(es->s_max_mnt_count) >= 0 &&
		 le16_to_cpu(es->s_mnt_count) >=
		 (unsigned short) (__s16) le16_to_cpu(es->s_max_mnt_count))
		printk (KERN_WARNING
			"EXT3-fs warning: maximal mount count reached, "
			"running e2fsck is recommended\n");
	else if (le32_to_cpu(es->s_checkinterval) &&
		(le32_to_cpu(es->s_lastcheck) +
			le32_to_cpu(es->s_checkinterval) <= CURRENT_TIME))
		printk (KERN_WARNING
			"EXT3-fs warning: checktime reached, "
			"running e2fsck is recommended\n");
#if 0
		/* @@@ We _will_ want to clear the valid bit if we find
                   inconsistencies, to force a fsck at reboot.  But for
                   a plain journaled filesystem we can keep it set as
                   valid forever! :) */
	es->s_state = cpu_to_le16(le16_to_cpu(es->s_state) & ~EXT3_VALID_FS);
#endif
	if (!(__s16) le16_to_cpu(es->s_max_mnt_count))
		es->s_max_mnt_count =
			(__s16) cpu_to_le16(EXT3_DFL_MAX_MNT_COUNT);
	es->s_mnt_count=cpu_to_le16(le16_to_cpu(es->s_mnt_count) + 1);
	es->s_mtime = cpu_to_le32(CURRENT_TIME);
	ext3_update_dynamic_rev(sb);
	EXT3_SET_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER);
	ext3_commit_super (sb, es, 1);
	if (test_opt (sb, DEBUG))
		printk (KERN_INFO
			"[EXT3 FS %s, %s, bs=%lu, gc=%lu, "
			"bpg=%lu, ipg=%lu, mo=%04lx]\n",
			EXT3FS_VERSION, EXT3FS_DATE, sb->s_blocksize,
			sbi->s_groups_count,
			EXT3_BLOCKS_PER_GROUP(sb),
			EXT3_INODES_PER_GROUP(sb),
			sbi->s_mount_opt);
	printk(KERN_INFO "EXT3 FS " EXT3FS_VERSION ", " EXT3FS_DATE " on %s, ",
				bdevname(sb->s_dev));
	if (EXT3_SB(sb)->s_journal->j_inode == NULL) {
		printk("external journal on %s\n",
				bdevname(EXT3_SB(sb)->s_journal->j_dev));
	} else {
		printk("internal journal\n");
	}
#ifdef CONFIG_EXT3_CHECK
	if (test_opt (sb, CHECK)) {
		ext3_check_blocks_bitmap (sb);
		ext3_check_inodes_bitmap (sb);
	}
#endif
	setup_ro_after(sb);
	return res;
}

static int ext3_check_descriptors (struct super_block * sb)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	unsigned long block = le32_to_cpu(sbi->s_es->s_first_data_block);
	struct ext3_group_desc * gdp = NULL;
	int desc_block = 0;
	int i;

	ext3_debug ("Checking group descriptors");

	for (i = 0; i < sbi->s_groups_count; i++)
	{
		if ((i % EXT3_DESC_PER_BLOCK(sb)) == 0)
			gdp = (struct ext3_group_desc *)
					sbi->s_group_desc[desc_block++]->b_data;
		if (le32_to_cpu(gdp->bg_block_bitmap) < block ||
		    le32_to_cpu(gdp->bg_block_bitmap) >=
				block + EXT3_BLOCKS_PER_GROUP(sb))
		{
			ext3_error (sb, "ext3_check_descriptors",
				    "Block bitmap for group %d"
				    " not in group (block %lu)!",
				    i, (unsigned long)
					le32_to_cpu(gdp->bg_block_bitmap));
			return 0;
		}
		if (le32_to_cpu(gdp->bg_inode_bitmap) < block ||
		    le32_to_cpu(gdp->bg_inode_bitmap) >=
				block + EXT3_BLOCKS_PER_GROUP(sb))
		{
			ext3_error (sb, "ext3_check_descriptors",
				    "Inode bitmap for group %d"
				    " not in group (block %lu)!",
				    i, (unsigned long)
					le32_to_cpu(gdp->bg_inode_bitmap));
			return 0;
		}
		if (le32_to_cpu(gdp->bg_inode_table) < block ||
		    le32_to_cpu(gdp->bg_inode_table) + sbi->s_itb_per_group >=
		    block + EXT3_BLOCKS_PER_GROUP(sb))
		{
			ext3_error (sb, "ext3_check_descriptors",
				    "Inode table for group %d"
				    " not in group (block %lu)!",
				    i, (unsigned long)
					le32_to_cpu(gdp->bg_inode_table));
			return 0;
		}
		block += EXT3_BLOCKS_PER_GROUP(sb);
		gdp++;
	}
	return 1;
}

static unsigned long descriptor_loc(struct super_block *sb,
				    unsigned long logic_sb_block,
				    int nr)
{
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	unsigned long bg, first_data_block, first_meta_bg;
	int has_super = 0;
	
	first_data_block = le32_to_cpu(sbi->s_es->s_first_data_block);
	first_meta_bg = le32_to_cpu(sbi->s_es->s_first_meta_bg);

	if (!EXT3_HAS_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_META_BG) ||
	    nr < first_meta_bg)
		return (logic_sb_block + nr + 1);
	bg = sbi->s_desc_per_block * nr;
	if (ext3_bg_has_super(sb, bg))
		has_super = 1;
	return (first_data_block + has_super + (bg * sbi->s_blocks_per_group));
}


/* ext3_orphan_cleanup() walks a singly-linked list of inodes (starting at
 * the superblock) which were deleted from all directories, but held open by
 * a process at the time of a crash.  We walk the list and try to delete these
 * inodes at recovery time (only with a read-write filesystem).
 *
 * In order to keep the orphan inode chain consistent during traversal (in
 * case of crash during recovery), we link each inode into the superblock
 * orphan list_head and handle it the same way as an inode deletion during
 * normal operation (which journals the operations for us).
 *
 * We only do an iget() and an iput() on each inode, which is very safe if we
 * accidentally point at an in-use or already deleted inode.  The worst that
 * can happen in this case is that we get a "bit already cleared" message from
 * ext3_free_inode().  The only reason we would point at a wrong inode is if
 * e2fsck was run on this filesystem, and it must have already done the orphan
 * inode cleanup for us, so we can safely abort without any further action.
 */
static void ext3_orphan_cleanup (struct super_block * sb,
				 struct ext3_super_block * es)
{
	unsigned int s_flags = sb->s_flags;
	int nr_orphans = 0, nr_truncates = 0;
	if (!es->s_last_orphan) {
		jbd_debug(4, "no orphan inodes to clean up\n");
		return;
	}

	if (sb->u.ext3_sb.s_mount_state & EXT3_ERROR_FS) {
		if (es->s_last_orphan)
			jbd_debug(1, "Errors on filesystem, "
				  "clearing orphan list.\n");
		es->s_last_orphan = 0;
		jbd_debug(1, "Skipping orphan recovery on fs with errors.\n");
		return;
	}

	if (s_flags & MS_RDONLY) {
		printk(KERN_INFO "EXT3-fs: %s: orphan cleanup on readonly fs\n",
		       bdevname(sb->s_dev));
		sb->s_flags &= ~MS_RDONLY;
	}

	while (es->s_last_orphan) {
		struct inode *inode;

		if (!(inode =
		      ext3_orphan_get(sb, le32_to_cpu(es->s_last_orphan)))) {
			es->s_last_orphan = 0;
			break;
		}

		list_add(&EXT3_I(inode)->i_orphan, &EXT3_SB(sb)->s_orphan);
		if (inode->i_nlink) {
			printk(KERN_DEBUG "%s: truncating inode %ld to %Ld "
				"bytes\n", __FUNCTION__, inode->i_ino,
				inode->i_size);
			jbd_debug(2, "truncating inode %ld to %Ld bytes\n",
				  inode->i_ino, inode->i_size);
			ext3_truncate(inode);
			nr_truncates++;
		} else {
			printk(KERN_DEBUG "%s: deleting unreferenced "
				"inode %ld\n", __FUNCTION__, inode->i_ino);
			jbd_debug(2, "deleting unreferenced inode %ld\n",
				  inode->i_ino);
			nr_orphans++;
		}
		iput(inode);  /* The delete magic happens here! */
	}

#define PLURAL(x) (x), ((x)==1) ? "" : "s"

	if (nr_orphans)
		printk(KERN_INFO "EXT3-fs: %s: %d orphan inode%s deleted\n",
		       bdevname(sb->s_dev), PLURAL(nr_orphans));
	if (nr_truncates)
		printk(KERN_INFO "EXT3-fs: %s: %d truncate%s cleaned up\n",
		       bdevname(sb->s_dev), PLURAL(nr_truncates));
	sb->s_flags = s_flags; /* Restore MS_RDONLY status */
}

#define log2(n) ffz(~(n))

/*
 * Maximal file size.  There is a direct, and {,double-,triple-}indirect
 * block limit, and also a limit of (2^32 - 1) 512-byte sectors in i_blocks.
 * We need to be 1 filesystem block less than the 2^32 sector limit.
 */
static loff_t ext3_max_size(int bits)
{
	loff_t res = EXT3_NDIR_BLOCKS;
	res += 1LL << (bits-2);
	res += 1LL << (2*(bits-2));
	res += 1LL << (3*(bits-2));
	res <<= bits;
	if (res > (512LL << 32) - (1 << bits))
		res = (512LL << 32) - (1 << bits);
	return res;
}

struct super_block * ext3_read_super (struct super_block * sb, void * data,
				      int silent)
{
	struct buffer_head * bh;
	struct ext3_super_block *es = 0;
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	unsigned long block;
	unsigned long sb_block = 1;
	unsigned long logic_sb_block = 1;
	unsigned long offset = 0;
	unsigned long journal_inum = 0;
	kdev_t dev = sb->s_dev;
	int blocksize;
	int hblock;
	int db_count;
	int i;
	int needs_recovery;

#ifdef CONFIG_JBD_DEBUG
	ext3_ro_after = 0;
#endif
	/*
	 * See what the current blocksize for the device is, and
	 * use that as the blocksize.  Otherwise (or if the blocksize
	 * is smaller than the default) use the default.
	 * This is important for devices that have a hardware
	 * sectorsize that is larger than the default.
	 */
	blocksize = EXT3_MIN_BLOCK_SIZE;
	hblock = get_hardsect_size(dev);
	if (blocksize < hblock)
		blocksize = hblock;

	sbi->s_mount_opt = 0;
	sbi->s_resuid = EXT3_DEF_RESUID;
	sbi->s_resgid = EXT3_DEF_RESGID;
	if (!parse_options ((char *) data, &sb_block, sbi, &journal_inum, 0)) {
		sb->s_dev = 0;
		goto out_fail;
	}

	sb->s_blocksize = blocksize;
	set_blocksize (dev, blocksize);

	/*
	 * The ext3 superblock will not be buffer aligned for other than 1kB
	 * block sizes.  We need to calculate the offset from buffer start.
	 */
	if (blocksize != EXT3_MIN_BLOCK_SIZE) {
		logic_sb_block = (sb_block * EXT3_MIN_BLOCK_SIZE) / blocksize;
		offset = (sb_block * EXT3_MIN_BLOCK_SIZE) % blocksize;
	}

	if (!(bh = sb_bread(sb, logic_sb_block))) {
		printk (KERN_ERR "EXT3-fs: unable to read superblock\n");
		goto out_fail;
	}
	/*
	 * Note: s_es must be initialized as soon as possible because
	 *       some ext3 macro-instructions depend on its value
	 */
	es = (struct ext3_super_block *) (((char *)bh->b_data) + offset);
	sbi->s_es = es;
	sb->s_magic = le16_to_cpu(es->s_magic);
	if (sb->s_magic != EXT3_SUPER_MAGIC) {
		if (!silent)
			printk(KERN_ERR 
			       "VFS: Can't find ext3 filesystem on dev %s.\n",
			       bdevname(dev));
		goto failed_mount;
	}
	if (le32_to_cpu(es->s_rev_level) == EXT3_GOOD_OLD_REV &&
	    (EXT3_HAS_COMPAT_FEATURE(sb, ~0U) ||
	     EXT3_HAS_RO_COMPAT_FEATURE(sb, ~0U) ||
	     EXT3_HAS_INCOMPAT_FEATURE(sb, ~0U)))
		printk(KERN_WARNING 
		       "EXT3-fs warning: feature flags set on rev 0 fs, "
		       "running e2fsck is recommended\n");
	/*
	 * Check feature flags regardless of the revision level, since we
	 * previously didn't change the revision level when setting the flags,
	 * so there is a chance incompat flags are set on a rev 0 filesystem.
	 */
	if ((i = EXT3_HAS_INCOMPAT_FEATURE(sb, ~EXT3_FEATURE_INCOMPAT_SUPP))) {
		printk(KERN_ERR "EXT3-fs: %s: couldn't mount because of "
		       "unsupported optional features (%x).\n",
		       bdevname(dev), i);
		goto failed_mount;
	}
	if (!(sb->s_flags & MS_RDONLY) &&
	    (i = EXT3_HAS_RO_COMPAT_FEATURE(sb, ~EXT3_FEATURE_RO_COMPAT_SUPP))){
		printk(KERN_ERR "EXT3-fs: %s: couldn't mount RDWR because of "
		       "unsupported optional features (%x).\n",
		       bdevname(dev), i);
		goto failed_mount;
	}
	sb->s_blocksize_bits = le32_to_cpu(es->s_log_block_size) + 10;
	sb->s_blocksize = 1 << sb->s_blocksize_bits;

	if (sb->s_blocksize < EXT3_MIN_BLOCK_SIZE ||
	    sb->s_blocksize > EXT3_MAX_BLOCK_SIZE) {
		printk(KERN_ERR 
		       "EXT3-fs: Unsupported filesystem blocksize %d on %s.\n",
		       blocksize, bdevname(dev));
		goto failed_mount;
	}

	sb->s_maxbytes = ext3_max_size(sb->s_blocksize_bits);

	if (sb->s_blocksize != blocksize) {
		blocksize = sb->s_blocksize;

		/*
		 * Make sure the blocksize for the filesystem is larger
		 * than the hardware sectorsize for the machine.
		 */
		if (sb->s_blocksize < hblock) {
			printk(KERN_ERR "EXT3-fs: blocksize %d too small for "
			       "device blocksize %d.\n", blocksize, hblock);
			goto failed_mount;
		}

		brelse (bh);
		set_blocksize (dev, sb->s_blocksize);
		logic_sb_block = (sb_block * EXT3_MIN_BLOCK_SIZE) / blocksize;
		offset = (sb_block * EXT3_MIN_BLOCK_SIZE) % blocksize;
		bh = sb_bread(sb, logic_sb_block);
		if (!bh) {
			printk(KERN_ERR 
			       "EXT3-fs: Can't read superblock on 2nd try.\n");
			return NULL;
		}
		es = (struct ext3_super_block *)(((char *)bh->b_data) + offset);
		sbi->s_es = es;
		if (es->s_magic != le16_to_cpu(EXT3_SUPER_MAGIC)) {
			printk (KERN_ERR 
				"EXT3-fs: Magic mismatch, very weird !\n");
			goto failed_mount;
		}
	}

	if (le32_to_cpu(es->s_rev_level) == EXT3_GOOD_OLD_REV) {
		sbi->s_inode_size = EXT3_GOOD_OLD_INODE_SIZE;
		sbi->s_first_ino = EXT3_GOOD_OLD_FIRST_INO;
	} else {
		sbi->s_inode_size = le16_to_cpu(es->s_inode_size);
		sbi->s_first_ino = le32_to_cpu(es->s_first_ino);
		if ((sbi->s_inode_size < EXT3_GOOD_OLD_INODE_SIZE) ||
		    (sbi->s_inode_size & (sbi->s_inode_size - 1)) ||
		    (sbi->s_inode_size > blocksize)) {
			printk (KERN_ERR
				"EXT3-fs: unsupported inode size: %d\n",
				sbi->s_inode_size);
			goto failed_mount;
		}
	}
	sbi->s_frag_size = EXT3_MIN_FRAG_SIZE <<
				   le32_to_cpu(es->s_log_frag_size);
	if (blocksize != sbi->s_frag_size) {
		printk(KERN_ERR
		       "EXT3-fs: fragsize %lu != blocksize %u (unsupported)\n",
		       sbi->s_frag_size, blocksize);
		goto failed_mount;
	}
	sbi->s_frags_per_block = 1;
	sbi->s_blocks_per_group = le32_to_cpu(es->s_blocks_per_group);
	sbi->s_frags_per_group = le32_to_cpu(es->s_frags_per_group);
	sbi->s_inodes_per_group = le32_to_cpu(es->s_inodes_per_group);
	sbi->s_inodes_per_block = blocksize / EXT3_INODE_SIZE(sb);
	sbi->s_itb_per_group = sbi->s_inodes_per_group /sbi->s_inodes_per_block;
	sbi->s_desc_per_block = blocksize / sizeof(struct ext3_group_desc);
	sbi->s_sbh = bh;
	if (sbi->s_resuid == EXT3_DEF_RESUID)
		sbi->s_resuid = le16_to_cpu(es->s_def_resuid);
	if (sbi->s_resgid == EXT3_DEF_RESGID)
		sbi->s_resgid = le16_to_cpu(es->s_def_resgid);
	sbi->s_mount_state = le16_to_cpu(es->s_state);
	sbi->s_addr_per_block_bits = log2(EXT3_ADDR_PER_BLOCK(sb));
	sbi->s_desc_per_block_bits = log2(EXT3_DESC_PER_BLOCK(sb));

	if (sbi->s_blocks_per_group > blocksize * 8) {
		printk (KERN_ERR
			"EXT3-fs: #blocks per group too big: %lu\n",
			sbi->s_blocks_per_group);
		goto failed_mount;
	}
	if (sbi->s_frags_per_group > blocksize * 8) {
		printk (KERN_ERR
			"EXT3-fs: #fragments per group too big: %lu\n",
			sbi->s_frags_per_group);
		goto failed_mount;
	}
	if (sbi->s_inodes_per_group > blocksize * 8) {
		printk (KERN_ERR
			"EXT3-fs: #inodes per group too big: %lu\n",
			sbi->s_inodes_per_group);
		goto failed_mount;
	}

	sbi->s_groups_count = (le32_to_cpu(es->s_blocks_count) -
			       le32_to_cpu(es->s_first_data_block) +
			       EXT3_BLOCKS_PER_GROUP(sb) - 1) /
			      EXT3_BLOCKS_PER_GROUP(sb);
	db_count = (sbi->s_groups_count + EXT3_DESC_PER_BLOCK(sb) - 1) /
		   EXT3_DESC_PER_BLOCK(sb);
	sbi->s_group_desc = kmalloc(db_count * sizeof (struct buffer_head *),
				    GFP_KERNEL);
	if (sbi->s_group_desc == NULL) {
		printk (KERN_ERR "EXT3-fs: not enough memory\n");
		goto failed_mount;
	}
	for (i = 0; i < db_count; i++) {
		block = descriptor_loc(sb, logic_sb_block, i);
		sbi->s_group_desc[i] = sb_bread(sb, block);
		if (!sbi->s_group_desc[i]) {
			printk (KERN_ERR "EXT3-fs: "
				"can't read group descriptor %d\n", i);
			db_count = i;
			goto failed_mount2;
		}
	}
	if (!ext3_check_descriptors (sb)) {
		printk (KERN_ERR "EXT3-fs: group descriptors corrupted !\n");
		goto failed_mount2;
	}
	for (i = 0; i < EXT3_MAX_GROUP_LOADED; i++) {
		sbi->s_inode_bitmap_number[i] = 0;
		sbi->s_inode_bitmap[i] = NULL;
		sbi->s_block_bitmap_number[i] = 0;
		sbi->s_block_bitmap[i] = NULL;
	}
	sbi->s_loaded_inode_bitmaps = 0;
	sbi->s_loaded_block_bitmaps = 0;
	sbi->s_gdb_count = db_count;
	get_random_bytes(&sbi->s_next_generation, sizeof(u32));
	/*
	 * set up enough so that it can read an inode
	 */
	sb->s_op = &ext3_sops;
	sb->dq_op = &ext3_qops;
	INIT_LIST_HEAD(&sbi->s_orphan); /* unlinked but open files */

	sb->s_root = 0;

	needs_recovery = (es->s_last_orphan != 0 ||
			  EXT3_HAS_INCOMPAT_FEATURE(sb,
				    EXT3_FEATURE_INCOMPAT_RECOVER));

	/*
	 * The first inode we look at is the journal inode.  Don't try
	 * root first: it may be modified in the journal!
	 */
	if (!test_opt(sb, NOLOAD) &&
	    EXT3_HAS_COMPAT_FEATURE(sb, EXT3_FEATURE_COMPAT_HAS_JOURNAL)) {
		if (ext3_load_journal(sb, es))
			goto failed_mount2;
	} else if (journal_inum) {
		if (ext3_create_journal(sb, es, journal_inum))
			goto failed_mount2;
	} else {
		if (!silent)
			printk (KERN_ERR
				"ext3: No journal on filesystem on %s\n",
				bdevname(dev));
		goto failed_mount2;
	}

	/* We have now updated the journal if required, so we can
	 * validate the data journaling mode. */
	switch (test_opt(sb, DATA_FLAGS)) {
	case 0:
		/* No mode set, assume a default based on the journal
                   capabilities: ORDERED_DATA if the journal can
                   cope, else JOURNAL_DATA */
		if (journal_check_available_features
		    (sbi->s_journal, 0, 0, JFS_FEATURE_INCOMPAT_REVOKE))
			set_opt(sbi->s_mount_opt, ORDERED_DATA);
		else
			set_opt(sbi->s_mount_opt, JOURNAL_DATA);
		break;

	case EXT3_MOUNT_ORDERED_DATA:
	case EXT3_MOUNT_WRITEBACK_DATA:
		if (!journal_check_available_features
		    (sbi->s_journal, 0, 0, JFS_FEATURE_INCOMPAT_REVOKE)) {
			printk(KERN_ERR "EXT3-fs: Journal does not support "
			       "requested data journaling mode\n");
			goto failed_mount3;
		}
	default:
		break;
	}

	/*
	 * The journal_load will have done any necessary log recovery,
	 * so we can safely mount the rest of the filesystem now.
	 */

	sb->s_root = d_alloc_root(iget(sb, EXT3_ROOT_INO));
	if (!sb->s_root || !S_ISDIR(sb->s_root->d_inode->i_mode) ||
	    !sb->s_root->d_inode->i_blocks || !sb->s_root->d_inode->i_size) {
		if (sb->s_root) {
			dput(sb->s_root);
			sb->s_root = NULL;
			printk(KERN_ERR
			       "EXT3-fs: corrupt root inode, run e2fsck\n");
		} else
			printk(KERN_ERR "EXT3-fs: get root inode failed\n");
		goto failed_mount3;
	}

	ext3_setup_super (sb, es, sb->s_flags & MS_RDONLY);
	/*
	 * akpm: core read_super() calls in here with the superblock locked.
	 * That deadlocks, because orphan cleanup needs to lock the superblock
	 * in numerous places.  Here we just pop the lock - it's relatively
	 * harmless, because we are now ready to accept write_super() requests,
	 * and aviro says that's the only reason for hanging onto the
	 * superblock lock.
	 */
	EXT3_SB(sb)->s_mount_state |= EXT3_ORPHAN_FS;
	unlock_super(sb);	/* akpm: sigh */
	ext3_orphan_cleanup(sb, es);
	lock_super(sb);
	EXT3_SB(sb)->s_mount_state &= ~EXT3_ORPHAN_FS;
	if (needs_recovery)
		printk (KERN_INFO "EXT3-fs: recovery complete.\n");
	ext3_mark_recovery_complete(sb, es);
	printk (KERN_INFO "EXT3-fs: mounted filesystem with %s data mode.\n",
		test_opt(sb,DATA_FLAGS) == EXT3_MOUNT_JOURNAL_DATA ? "journal":
		test_opt(sb,DATA_FLAGS) == EXT3_MOUNT_ORDERED_DATA ? "ordered":
		"writeback");

	return sb;

failed_mount3:
	journal_destroy(sbi->s_journal);
failed_mount2:
	for (i = 0; i < db_count; i++)
		brelse(sbi->s_group_desc[i]);
	kfree(sbi->s_group_desc);
failed_mount:
	ext3_blkdev_remove(sbi);
	brelse(bh);
out_fail:
	return NULL;
}

/*
 * Setup any per-fs journal parameters now.  We'll do this both on
 * initial mount, once the journal has been initialised but before we've
 * done any recovery; and again on any subsequent remount. 
 */
static void ext3_init_journal_params(struct ext3_sb_info *sbi, 
				     journal_t *journal)
{
	if (sbi->s_commit_interval)
		journal->j_commit_interval = sbi->s_commit_interval;
	/* We could also set up an ext3-specific default for the commit
	 * interval here, but for now we'll just fall back to the jbd
	 * default. */
}


static journal_t *ext3_get_journal(struct super_block *sb, int journal_inum)
{
	struct inode *journal_inode;
	journal_t *journal;

	/* First, test for the existence of a valid inode on disk.  Bad
	 * things happen if we iget() an unused inode, as the subsequent
	 * iput() will try to delete it. */

	journal_inode = iget(sb, journal_inum);
	if (!journal_inode) {
		printk(KERN_ERR "EXT3-fs: no journal found.\n");
		return NULL;
	}
	if (!journal_inode->i_nlink) {
		make_bad_inode(journal_inode);
		iput(journal_inode);
		printk(KERN_ERR "EXT3-fs: journal inode is deleted.\n");
		return NULL;
	}

	jbd_debug(2, "Journal inode found at %p: %Ld bytes\n",
		  journal_inode, journal_inode->i_size);
	if (is_bad_inode(journal_inode) || !S_ISREG(journal_inode->i_mode)) {
		printk(KERN_ERR "EXT3-fs: invalid journal inode.\n");
		iput(journal_inode);
		return NULL;
	}

	journal = journal_init_inode(journal_inode);
	if (!journal) {
		printk(KERN_ERR "EXT3-fs: Could not load journal inode\n");
		iput(journal_inode);
	}
	ext3_init_journal_params(EXT3_SB(sb), journal);
	return journal;
}

static journal_t *ext3_get_dev_journal(struct super_block *sb,
				       int dev)
{
	struct buffer_head * bh;
	journal_t *journal;
	int start;
	int len;
	int hblock, blocksize;
	unsigned long sb_block;
	unsigned long offset;
	kdev_t journal_dev = to_kdev_t(dev);
	struct ext3_super_block * es;
	struct block_device *bdev;

	bdev = ext3_blkdev_get(journal_dev);
	if (bdev == NULL)
		return NULL;

	blocksize = sb->s_blocksize;
	hblock = get_hardsect_size(journal_dev);
	if (blocksize < hblock) {
		printk(KERN_ERR
			"EXT3-fs: blocksize too small for journal device.\n");
		goto out_bdev;
	}
	
	sb_block = EXT3_MIN_BLOCK_SIZE / blocksize;
	offset = EXT3_MIN_BLOCK_SIZE % blocksize;
	set_blocksize(dev, blocksize);
	if (!(bh = bread(dev, sb_block, blocksize))) {
		printk(KERN_ERR "EXT3-fs: couldn't read superblock of "
		       "external journal\n");
		goto out_bdev;
	}

	es = (struct ext3_super_block *) (((char *)bh->b_data) + offset);
	if ((le16_to_cpu(es->s_magic) != EXT3_SUPER_MAGIC) ||
	    !(le32_to_cpu(es->s_feature_incompat) &
	      EXT3_FEATURE_INCOMPAT_JOURNAL_DEV)) {
		printk(KERN_ERR "EXT3-fs: external journal has "
					"bad superblock\n");
		brelse(bh);
		goto out_bdev;
	}

	if (memcmp(EXT3_SB(sb)->s_es->s_journal_uuid, es->s_uuid, 16)) {
		printk(KERN_ERR "EXT3-fs: journal UUID does not match\n");
		brelse(bh);
		goto out_bdev;
	}

	len = le32_to_cpu(es->s_blocks_count);
	start = sb_block + 1;
	brelse(bh);	/* we're done with the superblock */

	journal = journal_init_dev(journal_dev, sb->s_dev, 
					start, len, blocksize);
	if (!journal) {
		printk(KERN_ERR "EXT3-fs: failed to create device journal\n");
		goto out_bdev;
	}
	ll_rw_block(READ, 1, &journal->j_sb_buffer);
	wait_on_buffer(journal->j_sb_buffer);
	if (!buffer_uptodate(journal->j_sb_buffer)) {
		printk(KERN_ERR "EXT3-fs: I/O error on journal device\n");
		goto out_journal;
	}
	if (ntohl(journal->j_superblock->s_nr_users) != 1) {
		printk(KERN_ERR "EXT3-fs: External journal has more than one "
					"user (unsupported) - %d\n",
			ntohl(journal->j_superblock->s_nr_users));
		goto out_journal;
	}
	EXT3_SB(sb)->journal_bdev = bdev;
	ext3_init_journal_params(EXT3_SB(sb), journal);
	return journal;
out_journal:
	journal_destroy(journal);
out_bdev:
	ext3_blkdev_put(bdev);
	return NULL;
}

static int ext3_load_journal(struct super_block * sb,
			     struct ext3_super_block * es)
{
	journal_t *journal;
	int journal_inum = le32_to_cpu(es->s_journal_inum);
	int journal_dev = le32_to_cpu(es->s_journal_dev);
	int err = 0;
	int really_read_only;

	really_read_only = is_read_only(sb->s_dev);

	/*
	 * Are we loading a blank journal or performing recovery after a
	 * crash?  For recovery, we need to check in advance whether we
	 * can get read-write access to the device.
	 */

	if (EXT3_HAS_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER)) {
		if (sb->s_flags & MS_RDONLY) {
			printk(KERN_INFO "EXT3-fs: INFO: recovery "
					"required on readonly filesystem.\n");
			if (really_read_only) {
				printk(KERN_ERR "EXT3-fs: write access "
					"unavailable, cannot proceed.\n");
				return -EROFS;
			}
			printk (KERN_INFO "EXT3-fs: write access will "
					"be enabled during recovery.\n");
		}
	}

	if (journal_inum && journal_dev) {
		printk(KERN_ERR "EXT3-fs: filesystem has both journal "
		       "and inode journals!\n");
		return -EINVAL;
	}

	if (journal_inum) {
		if (!(journal = ext3_get_journal(sb, journal_inum)))
			return -EINVAL;
	} else {
		if (!(journal = ext3_get_dev_journal(sb, journal_dev)))
			return -EINVAL;
	}
	

	if (!really_read_only && test_opt(sb, UPDATE_JOURNAL)) {
		err = journal_update_format(journal);
		if (err)  {
			printk(KERN_ERR "EXT3-fs: error updating journal.\n");
			journal_destroy(journal);
			return err;
		}
	}

	if (!EXT3_HAS_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER))
		err = journal_wipe(journal, !really_read_only);
	if (!err)
		err = journal_load(journal);

	if (err) {
		printk(KERN_ERR "EXT3-fs: error loading journal.\n");
		journal_destroy(journal);
		return err;
	}

	EXT3_SB(sb)->s_journal = journal;
	ext3_clear_journal_err(sb, es);
	return 0;
}

static int ext3_create_journal(struct super_block * sb,
			       struct ext3_super_block * es,
			       int journal_inum)
{
	journal_t *journal;

	if (sb->s_flags & MS_RDONLY) {
		printk(KERN_ERR "EXT3-fs: readonly filesystem when trying to "
				"create journal.\n");
		return -EROFS;
	}

	if (!(journal = ext3_get_journal(sb, journal_inum)))
		return -EINVAL;

	printk(KERN_INFO "EXT3-fs: creating new journal on inode %d\n",
	       journal_inum);

	if (journal_create(journal)) {
		printk(KERN_ERR "EXT3-fs: error creating journal.\n");
		journal_destroy(journal);
		return -EIO;
	}

	EXT3_SB(sb)->s_journal = journal;

	ext3_update_dynamic_rev(sb);
	EXT3_SET_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER);
	EXT3_SET_COMPAT_FEATURE(sb, EXT3_FEATURE_COMPAT_HAS_JOURNAL);

	es->s_journal_inum = cpu_to_le32(journal_inum);
	sb->s_dirt = 1;

	/* Make sure we flush the recovery flag to disk. */
	ext3_commit_super(sb, es, 1);

	return 0;
}

static void ext3_commit_super (struct super_block * sb,
			       struct ext3_super_block * es,
			       int sync)
{
	es->s_wtime = cpu_to_le32(CURRENT_TIME);
	BUFFER_TRACE(sb->u.ext3_sb.s_sbh, "marking dirty");
	mark_buffer_dirty(sb->u.ext3_sb.s_sbh);
	if (sync) {
		ll_rw_block(WRITE, 1, &sb->u.ext3_sb.s_sbh);
		wait_on_buffer(sb->u.ext3_sb.s_sbh);
	}
}


/*
 * Have we just finished recovery?  If so, and if we are mounting (or
 * remounting) the filesystem readonly, then we will end up with a
 * consistent fs on disk.  Record that fact.
 */
static void ext3_mark_recovery_complete(struct super_block * sb,
					struct ext3_super_block * es)
{
	journal_flush(EXT3_SB(sb)->s_journal);
	if (EXT3_HAS_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER) &&
	    sb->s_flags & MS_RDONLY) {
		EXT3_CLEAR_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER);
		sb->s_dirt = 0;
		ext3_commit_super(sb, es, 1);
	}
}

/*
 * If we are mounting (or read-write remounting) a filesystem whose journal
 * has recorded an error from a previous lifetime, move that error to the
 * main filesystem now.
 */
static void ext3_clear_journal_err(struct super_block * sb,
				   struct ext3_super_block * es)
{
	journal_t *journal;
	int j_errno;
	const char *errstr;
	
	journal = EXT3_SB(sb)->s_journal;

	/*
	 * Now check for any error status which may have been recorded in the
	 * journal by a prior ext3_error() or ext3_abort()
	 */

	j_errno = journal_errno(journal);
	if (j_errno) {
		char nbuf[16];
		
		errstr = ext3_decode_error(sb, j_errno, nbuf);
		ext3_warning(sb, __FUNCTION__, "Filesystem error recorded "
			     "from previous mount: %s", errstr);
		ext3_warning(sb, __FUNCTION__, "Marking fs in need of "
			     "filesystem check.");
		
		sb->u.ext3_sb.s_mount_state |= EXT3_ERROR_FS;
		es->s_state |= cpu_to_le16(EXT3_ERROR_FS);
		ext3_commit_super (sb, es, 1);

		journal_clear_err(journal);
	}
}

/*
 * Force the running and committing transactions to commit,
 * and wait on the commit.
 */
int ext3_force_commit(struct super_block *sb)
{
	journal_t *journal;
	int ret;

	if (sb->s_flags & MS_RDONLY)
		return 0;

	journal = EXT3_SB(sb)->s_journal;
	sb->s_dirt = 0;
	lock_kernel();	/* important: lock down j_running_transaction */
	ret = ext3_journal_force_commit(journal);
	unlock_kernel();
	return ret;
}

/*
 * Ext3 always journals updates to the superblock itself, so we don't
 * have to propagate any other updates to the superblock on disk at this
 * point.  Just start an async writeback to get the buffers on their way
 * to the disk.
 *
 * This implicitly triggers the writebehind on sync().
 */

void ext3_write_super (struct super_block * sb)
{
	if (down_trylock(&sb->s_lock) == 0)
		BUG();
	sb->s_dirt = 0;
	log_start_commit(EXT3_SB(sb)->s_journal, NULL);
}

static int ext3_sync_fs(struct super_block *sb)
{
	tid_t target;
	
	sb->s_dirt = 0;
	target = log_start_commit(EXT3_SB(sb)->s_journal, NULL);
	log_wait_commit(EXT3_SB(sb)->s_journal, target);
	return 0;
}

/*
 * LVM calls this function before a (read-only) snapshot is created.  This
 * gives us a chance to flush the journal completely and mark the fs clean.
 */
void ext3_write_super_lockfs(struct super_block *sb)
{
	sb->s_dirt = 0;

	lock_kernel();		/* 2.4.5 forgot to do this for us */
	if (!(sb->s_flags & MS_RDONLY)) {
		journal_t *journal = EXT3_SB(sb)->s_journal;

		/* Now we set up the journal barrier. */
		unlock_super(sb);
		journal_lock_updates(journal);
		journal_flush(journal);
		lock_super(sb);

		/* Journal blocked and flushed, clear needs_recovery flag. */
		EXT3_CLEAR_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER);
		ext3_commit_super(sb, EXT3_SB(sb)->s_es, 1);
	}
	unlock_kernel();
}

/*
 * Called by LVM after the snapshot is done.  We need to reset the RECOVER
 * flag here, even though the filesystem is not technically dirty yet.
 */
void ext3_unlockfs(struct super_block *sb)
{
	if (!(sb->s_flags & MS_RDONLY)) {
		lock_kernel();
		lock_super(sb);
		/* Reser the needs_recovery flag before the fs is unlocked. */
		EXT3_SET_INCOMPAT_FEATURE(sb, EXT3_FEATURE_INCOMPAT_RECOVER);
		ext3_commit_super(sb, EXT3_SB(sb)->s_es, 1);
		unlock_super(sb);
		journal_unlock_updates(EXT3_SB(sb)->s_journal);
		unlock_kernel();
	}
}

int ext3_remount (struct super_block * sb, int * flags, char * data)
{
	struct ext3_super_block * es;
	struct ext3_sb_info *sbi = EXT3_SB(sb);
	unsigned long tmp;

	clear_ro_after(sb);

	/*
	 * Allow the "check" option to be passed as a remount option.
	 */
	if (!parse_options(data, &tmp, sbi, &tmp, 1))
		return -EINVAL;

	if (sbi->s_mount_opt & EXT3_MOUNT_ABORT)
		ext3_abort(sb, __FUNCTION__, "Abort forced by user");

	es = sbi->s_es;

	ext3_init_journal_params(sbi, sbi->s_journal);
	
	if ((*flags & MS_RDONLY) != (sb->s_flags & MS_RDONLY)) {
		if (sbi->s_mount_opt & EXT3_MOUNT_ABORT)
			return -EROFS;

		if (*flags & MS_RDONLY) {
			/*
			 * First of all, the unconditional stuff we have to do
			 * to disable replay of the journal when we next remount
			 */
			sb->s_flags |= MS_RDONLY;

			/*
			 * OK, test if we are remounting a valid rw partition
			 * readonly, and if so set the rdonly flag and then
			 * mark the partition as valid again.
			 */
			if (!(es->s_state & cpu_to_le16(EXT3_VALID_FS)) &&
			    (sbi->s_mount_state & EXT3_VALID_FS))
				es->s_state = cpu_to_le16(sbi->s_mount_state);

			ext3_mark_recovery_complete(sb, es);
		} else {
			int ret;
			if ((ret = EXT3_HAS_RO_COMPAT_FEATURE(sb,
					~EXT3_FEATURE_RO_COMPAT_SUPP))) {
				printk(KERN_WARNING "EXT3-fs: %s: couldn't "
				       "remount RDWR because of unsupported "
				       "optional features (%x).\n",
				       bdevname(sb->s_dev), ret);
				return -EROFS;
			}
			/*
			 * Mounting a RDONLY partition read-write, so reread
			 * and store the current valid flag.  (It may have
			 * been changed by e2fsck since we originally mounted
			 * the partition.)
			 */
			ext3_clear_journal_err(sb, es);
			sbi->s_mount_state = le16_to_cpu(es->s_state);
			if (!ext3_setup_super (sb, es, 0))
				sb->s_flags &= ~MS_RDONLY;
		}
	}
	setup_ro_after(sb);
	return 0;
}

int ext3_statfs (struct super_block * sb, struct statfs * buf)
{
	struct ext3_super_block *es = EXT3_SB(sb)->s_es;
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
		overhead = le32_to_cpu(es->s_first_data_block);

		/*
		 * Add the overhead attributed to the superblock and
		 * block group descriptors.  If the sparse superblocks
		 * feature is turned on, then not all groups have this.
		 */
		for (i = 0; i < EXT3_SB(sb)->s_groups_count; i++)
			overhead += ext3_bg_has_super(sb, i) +
				ext3_bg_num_gdb(sb, i);

		/*
		 * Every block group has an inode bitmap, a block
		 * bitmap, and an inode table.
		 */
		overhead += (EXT3_SB(sb)->s_groups_count *
			     (2 + EXT3_SB(sb)->s_itb_per_group));
	}

	buf->f_type = EXT3_SUPER_MAGIC;
	buf->f_bsize = sb->s_blocksize;
	buf->f_blocks = le32_to_cpu(es->s_blocks_count) - overhead;
	buf->f_bfree = ext3_count_free_blocks (sb);
	buf->f_bavail = buf->f_bfree - le32_to_cpu(es->s_r_blocks_count);
	if (buf->f_bfree < le32_to_cpu(es->s_r_blocks_count))
		buf->f_bavail = 0;
	buf->f_files = le32_to_cpu(es->s_inodes_count);
	buf->f_ffree = ext3_count_free_inodes (sb);
	buf->f_namelen = EXT3_NAME_LEN;
	return 0;
}

/* Helper function for writing quotas on sync - we need to start transaction before quota file
 * is locked for write. Otherwise the are possible deadlocks:
 * Process 1                         Process 2
 * ext3_create()                     quota_sync()
 *   journal_start()                   write_dquot()
 *   DQUOT_INIT()                        down(dqio_sem)
 *     down(dqio_sem)                    journal_start()
 *
 */

#ifdef CONFIG_QUOTA

static int (*old_write_dquot)(struct dquot *dquot);

/* Blocks: (2 data blocks) * (3 indirect + 1 descriptor + 1 bitmap) + superblock */
#define EXT3_OLD_QFMT_BLOCKS 11
/* Blocks: quota info + (4 pointer blocks + 1 entry block) * (3 indirect + 1 descriptor + 1 bitmap) + superblock */
#define EXT3_V0_QFMT_BLOCKS 27

static int ext3_write_dquot(struct dquot *dquot)
{
	int nblocks, ret;
	handle_t *handle;
	struct quota_info *dqops = sb_dqopt(dquot->dq_sb);
	struct inode *qinode;

	switch (dqops->info[dquot->dq_type].dqi_format->qf_fmt_id) {
		case QFMT_VFS_OLD:
			nblocks = EXT3_OLD_QFMT_BLOCKS;
			break;
		case QFMT_VFS_V0:
			nblocks = EXT3_V0_QFMT_BLOCKS;
			break;
		default:
			nblocks = EXT3_MAX_TRANS_DATA;
	}
	lock_kernel();
	qinode = dqops->files[dquot->dq_type]->f_dentry->d_inode;
	handle = ext3_journal_start(qinode, nblocks);
	if (IS_ERR(handle)) {
		unlock_kernel();
		return PTR_ERR(handle);
	}
	unlock_kernel();
	ret = old_write_dquot(dquot);
	lock_kernel();
	ret = ext3_journal_stop(handle, qinode);
	unlock_kernel();
	return ret;
}
#endif

static DECLARE_FSTYPE_DEV(ext3_fs_type, "ext3", ext3_read_super);

static int __init init_ext3_fs(void)
{
#ifdef CONFIG_QUOTA
	init_dquot_operations(&ext3_qops);
	old_write_dquot = ext3_qops.write_dquot;
	ext3_qops.write_dquot = ext3_write_dquot;
#endif
        return register_filesystem(&ext3_fs_type);
}

static void __exit exit_ext3_fs(void)
{
	unregister_filesystem(&ext3_fs_type);
}

EXPORT_NO_SYMBOLS;

MODULE_AUTHOR("Remy Card, Stephen Tweedie, Andrew Morton, Andreas Dilger, Theodore Ts'o and others");
MODULE_DESCRIPTION("Second Extended Filesystem with journaling extensions");
MODULE_LICENSE("GPL");
module_init(init_ext3_fs)
module_exit(exit_ext3_fs)
