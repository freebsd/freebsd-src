/*
 * linux/include/linux/ext3_jbd.h
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 *
 * Copyright 1998--1999 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Ext3-specific journaling extensions.
 */

#ifndef _LINUX_EXT3_JBD_H
#define _LINUX_EXT3_JBD_H

#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/ext3_fs.h>

#define EXT3_JOURNAL(inode)	(EXT3_SB((inode)->i_sb)->s_journal)

/* Define the number of blocks we need to account to a transaction to
 * modify one block of data.
 * 
 * We may have to touch one inode, one bitmap buffer, up to three
 * indirection blocks, the group and superblock summaries, and the data
 * block to complete the transaction.  */

#define EXT3_SINGLEDATA_TRANS_BLOCKS	8U

/* Define the minimum size for a transaction which modifies data.  This
 * needs to take into account the fact that we may end up modifying two
 * quota files too (one for the group, one for the user quota).  The
 * superblock only gets updated once, of course, so don't bother
 * counting that again for the quota updates. */

#define EXT3_DATA_TRANS_BLOCKS		(3 * EXT3_SINGLEDATA_TRANS_BLOCKS - 2)

extern int ext3_writepage_trans_blocks(struct inode *inode);

/* Delete operations potentially hit one directory's namespace plus an
 * entire inode, plus arbitrary amounts of bitmap/indirection data.  Be
 * generous.  We can grow the delete transaction later if necessary. */

#define EXT3_DELETE_TRANS_BLOCKS	(2 * EXT3_DATA_TRANS_BLOCKS + 64)

/* Define an arbitrary limit for the amount of data we will anticipate
 * writing to any given transaction.  For unbounded transactions such as
 * write(2) and truncate(2) we can write more than this, but we always
 * start off at the maximum transaction size and grow the transaction
 * optimistically as we go. */

#define EXT3_MAX_TRANS_DATA		64U

/* We break up a large truncate or write transaction once the handle's
 * buffer credits gets this low, we need either to extend the
 * transaction or to start a new one.  Reserve enough space here for
 * inode, bitmap, superblock, group and indirection updates for at least
 * one block, plus two quota updates.  Quota allocations are not
 * needed. */

#define EXT3_RESERVE_TRANS_BLOCKS	12U

int
ext3_mark_iloc_dirty(handle_t *handle, 
		     struct inode *inode,
		     struct ext3_iloc *iloc);

/* 
 * On success, We end up with an outstanding reference count against
 * iloc->bh.  This _must_ be cleaned up later. 
 */

int ext3_reserve_inode_write(handle_t *handle, struct inode *inode, 
			struct ext3_iloc *iloc);

int ext3_mark_inode_dirty(handle_t *handle, struct inode *inode);

/*
 * Wrapper functions with which ext3 calls into JBD.  The intent here is
 * to allow these to be turned into appropriate stubs so ext3 can control
 * ext2 filesystems, so ext2+ext3 systems only nee one fs.  This work hasn't
 * been done yet.
 */

static inline void ext3_journal_abort_handle(const char *caller, 
					     const char *err_fn,
					     struct buffer_head *bh,
					     handle_t *handle,
					     int err)
{
	char nbuf[16];
	const char *errstr = ext3_decode_error(NULL, err, nbuf);
	
	printk(KERN_ERR "%s: aborting transaction: %s in %s", 
	       caller, errstr, err_fn);

	if (bh)
		BUFFER_TRACE(bh, "abort");
	journal_abort_handle(handle);
	if (!handle->h_err)
		handle->h_err = err;
}

static inline int
__ext3_journal_get_undo_access(const char *where,
			       handle_t *handle, struct buffer_head *bh)
{
	int err = journal_get_undo_access(handle, bh);
	if (err)
		ext3_journal_abort_handle(where, __FUNCTION__, bh, handle,err);
	return err;
}

static inline int
__ext3_journal_get_write_access(const char *where,
				handle_t *handle, struct buffer_head *bh)
{
	int err = journal_get_write_access(handle, bh);
	if (err)
		ext3_journal_abort_handle(where, __FUNCTION__, bh, handle,err);
	return err;
}

static inline int
__ext3_journal_dirty_data(const char *where,
			  handle_t *handle, struct buffer_head *bh, int async)
{
	int err = journal_dirty_data(handle, bh, async);
	if (err)
		ext3_journal_abort_handle(where, __FUNCTION__, bh, handle,err);
	return err;
}

static inline void
ext3_journal_forget(handle_t *handle, struct buffer_head *bh)
{
	journal_forget(handle, bh);
}

static inline int
__ext3_journal_revoke(const char *where, handle_t *handle,
		      unsigned long blocknr, struct buffer_head *bh)
{
	int err = journal_revoke(handle, blocknr, bh);
	if (err)
		ext3_journal_abort_handle(where, __FUNCTION__, bh, handle,err);
	return err;
}

static inline int
__ext3_journal_get_create_access(const char *where,
				 handle_t *handle, struct buffer_head *bh)
{
	int err = journal_get_create_access(handle, bh);
	if (err)
		ext3_journal_abort_handle(where, __FUNCTION__, bh, handle,err);
	return err;
}

static inline int
__ext3_journal_dirty_metadata(const char *where,
			      handle_t *handle, struct buffer_head *bh)
{
	int err = journal_dirty_metadata(handle, bh);
	if (err)
		ext3_journal_abort_handle(where, __FUNCTION__, bh, handle,err);
	return err;
}


#define ext3_journal_get_undo_access(handle, bh) \
	__ext3_journal_get_undo_access(__FUNCTION__, (handle), (bh))
#define ext3_journal_get_write_access(handle, bh) \
	__ext3_journal_get_write_access(__FUNCTION__, (handle), (bh))
#define ext3_journal_dirty_data(handle, bh, async) \
	__ext3_journal_dirty_data(__FUNCTION__, (handle), (bh), (async))
#define ext3_journal_revoke(handle, blocknr, bh) \
	__ext3_journal_revoke(__FUNCTION__, (handle), (blocknr), (bh))
#define ext3_journal_get_create_access(handle, bh) \
	__ext3_journal_get_create_access(__FUNCTION__, (handle), (bh))
#define ext3_journal_dirty_metadata(handle, bh) \
	__ext3_journal_dirty_metadata(__FUNCTION__, (handle), (bh))



/* 
 * Wrappers for journal_start/end.
 *
 * The only special thing we need to do here is to make sure that all
 * journal_end calls result in the superblock being marked dirty, so
 * that sync() will call the filesystem's write_super callback if
 * appropriate. 
 */
static inline handle_t *ext3_journal_start(struct inode *inode, int nblocks)
{
	journal_t *journal;
	
	if (inode->i_sb->s_flags & MS_RDONLY)
		return ERR_PTR(-EROFS);

	/* Special case here: if the journal has aborted behind our
	 * backs (eg. EIO in the commit thread), then we still need to
	 * take the FS itself readonly cleanly. */
	journal = EXT3_JOURNAL(inode);
	if (is_journal_aborted(journal)) {
		ext3_abort(inode->i_sb, __FUNCTION__,
			   "Detected aborted journal");
		return ERR_PTR(-EROFS);
	}
	
	return journal_start(journal, nblocks);
}

static inline handle_t *
ext3_journal_try_start(struct inode *inode, int nblocks)
{
	if (inode->i_sb->s_flags & MS_RDONLY)
		return ERR_PTR(-EROFS);
	return journal_try_start(EXT3_JOURNAL(inode), nblocks);
}

/* 
 * The only special thing we need to do here is to make sure that all
 * journal_stop calls result in the superblock being marked dirty, so
 * that sync() will call the filesystem's write_super callback if
 * appropriate. 
 */
static inline int __ext3_journal_stop(const char *where,
				      handle_t *handle, struct inode *inode)
{
	int err = handle->h_err;
	int rc = journal_stop(handle);

	inode->i_sb->s_dirt = 1;
	if (!err)
		err = rc;
	if (err)
		__ext3_std_error(inode->i_sb, where, err);
	return err;
}
#define ext3_journal_stop(handle, inode) \
	__ext3_journal_stop(__FUNCTION__, (handle), (inode))

static inline handle_t *ext3_journal_current_handle(void)
{
	return journal_current_handle();
}

static inline void
ext3_log_start_commit(journal_t *journal, transaction_t *transaction)
{
	log_start_commit(journal, transaction);
}

static inline void ext3_log_wait_commit(journal_t *journal, tid_t tid)
{
	log_wait_commit(journal, tid);
}

static inline int ext3_journal_extend(handle_t *handle, int nblocks)
{
	return journal_extend(handle, nblocks);
}

static inline int ext3_journal_restart(handle_t *handle, int nblocks)
{
	return journal_restart(handle, nblocks);
}

static inline int ext3_journal_blocks_per_page(struct inode *inode)
{
	return journal_blocks_per_page(inode);
}

static inline int ext3_journal_force_commit(journal_t *journal)
{
	return journal_force_commit(journal);
}

/* super.c */
int ext3_force_commit(struct super_block *sb);

static inline int ext3_should_journal_data(struct inode *inode)
{
	if (!S_ISREG(inode->i_mode))
		return 1;
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_JOURNAL_DATA)
		return 1;
	if (inode->u.ext3_i.i_flags & EXT3_JOURNAL_DATA_FL)
		return 1;
	return 0;
}

static inline int ext3_should_order_data(struct inode *inode)
{
	return (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_ORDERED_DATA);
}


#endif	/* _LINUX_EXT3_JBD_H */
