/*
 *  linux/fs/ext3/fsync.c
 *
 *  Copyright (C) 1993  Stephen Tweedie (sct@redhat.com)
 *  from
 *  Copyright (C) 1992  Remy Card (card@masi.ibp.fr)
 *                      Laboratoire MASI - Institut Blaise Pascal
 *                      Universite Pierre et Marie Curie (Paris VI)
 *  from
 *  linux/fs/minix/truncate.c   Copyright (C) 1991, 1992  Linus Torvalds
 * 
 *  ext3fs fsync primitive
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 * 
 *  Removed unnecessary code duplication for little endian machines
 *  and excessive __inline__s. 
 *        Andi Kleen, 1997
 *
 * Major simplications and cleanup - we only need to do the metadata, because
 * we can depend on generic_block_fdatasync() to sync the data blocks.
 */

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/ext3_fs.h>
#include <linux/ext3_jbd.h>
#include <linux/jbd.h>
#include <linux/smp_lock.h>

/*
 * akpm: A new design for ext3_sync_file().
 *
 * This is only called from sys_fsync(), sys_fdatasync() and sys_msync().
 * There cannot be a transaction open by this task. (AKPM: quotas?)
 * Another task could have dirtied this inode.  Its data can be in any
 * state in the journalling system.
 *
 * What we do is just kick off a commit and wait on it.  This will snapshot the
 * inode to disk.
 *
 * Note that there is a serious optimisation we can make here: if the current
 * inode is not part of j_running_transaction or j_committing_transaction
 * then we have nothing to do.  That would require implementation of t_ilist,
 * which isn't too hard.
 */

int ext3_sync_file(struct file * file, struct dentry *dentry, int datasync)
{
	struct inode *inode = dentry->d_inode;
	int ret;

	J_ASSERT(ext3_journal_current_handle() == 0);

	/*
	 * fsync_inode_buffers() just walks i_dirty_buffers and waits
	 * on them.  It's a no-op for full data journalling because
	 * i_dirty_buffers will be ampty.
	 * Really, we only need to start I/O on the dirty buffers -
	 * we'll end up waiting on them in commit.
	 */
	ret = fsync_inode_buffers(inode);

	/* In writeback mode, we need to force out data buffers too.  In
	 * the other modes, ext3_force_commit takes care of forcing out
	 * just the right data blocks. */
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_WRITEBACK_DATA)
		ret |= fsync_inode_data_buffers(inode);

	ext3_force_commit(inode->i_sb);

	return ret;
}
