/*
 *  linux/fs/ext3/file.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/file.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext3 fs regular file handling primitives
 *
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 *	(jj@sunsite.ms.mff.cuni.cz)
 */

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/locks.h>
#include <linux/jbd.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/smp_lock.h>

/*
 * Called when an inode is released. Note that this is different
 * from ext3_file_open: open gets called at every open, but release
 * gets called only when /all/ the files are closed.
 */
static int ext3_release_file (struct inode * inode, struct file * filp)
{
	if (filp->f_mode & FMODE_WRITE)
		ext3_discard_prealloc (inode);
	return 0;
}

/*
 * Called when an inode is about to be opened.
 * We use this to disallow opening RW large files on 32bit systems if
 * the caller didn't specify O_LARGEFILE.  On 64bit systems we force
 * on this flag in sys_open.
 */
static int ext3_open_file (struct inode * inode, struct file * filp)
{
	if (!(filp->f_flags & O_LARGEFILE) &&
	    inode->i_size > 0x7FFFFFFFLL)
		return -EFBIG;
	return 0;
}

/*
 * ext3_file_write().
 *
 * Most things are done in ext3_prepare_write() and ext3_commit_write().
 */

static ssize_t
ext3_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	ssize_t ret;
	int err;
	struct inode *inode = file->f_dentry->d_inode;

	ret = generic_file_write(file, buf, count, ppos);

	/* Skip file flushing code if there was an error, or if nothing
	   was written. */
	if (ret <= 0)
		return ret;
	
	/* If the inode is IS_SYNC, or is O_SYNC and we are doing
           data-journaling, then we need to make sure that we force the
           transaction to disk to keep all metadata uptodate
           synchronously. */

	if (file->f_flags & O_SYNC) {
		/* If we are non-data-journaled, then the dirty data has
                   already been flushed to backing store by
                   generic_osync_inode, and the inode has been flushed
                   too if there have been any modifications other than
                   mere timestamp updates.
		   
		   Open question --- do we care about flushing
		   timestamps too if the inode is IS_SYNC? */
		if (!ext3_should_journal_data(inode))
			return ret;

		goto force_commit;
	}

	/* So we know that there has been no forced data flush.  If the
           inode is marked IS_SYNC, we need to force one ourselves. */
	if (!IS_SYNC(inode))
		return ret;
	
	/* Open question #2 --- should we force data to disk here too?
           If we don't, the only impact is that data=writeback
           filesystems won't flush data to disk automatically on
           IS_SYNC, only metadata (but historically, that is what ext2
           has done.) */
	
force_commit:
	err = ext3_force_commit(inode->i_sb);
	if (err) 
		return err;
	return ret;
}

struct file_operations ext3_file_operations = {
	llseek:		generic_file_llseek,	/* BKL held */
	read:		generic_file_read,	/* BKL not held.  Don't need */
	write:		ext3_file_write,	/* BKL not held.  Don't need */
	ioctl:		ext3_ioctl,		/* BKL held */
	mmap:		generic_file_mmap,
	open:		ext3_open_file,		/* BKL not held.  Don't need */
	release:	ext3_release_file,	/* BKL not held.  Don't need */
	fsync:		ext3_sync_file,		/* BKL held */
};

struct inode_operations ext3_file_inode_operations = {
	truncate:	ext3_truncate,		/* BKL held */
	setattr:	ext3_setattr,		/* BKL held */
};

