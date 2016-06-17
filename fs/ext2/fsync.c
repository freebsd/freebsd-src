/*
 *  linux/fs/ext2/fsync.c
 *
 *  Copyright (C) 1993  Stephen Tweedie (sct@dcs.ed.ac.uk)
 *  from
 *  Copyright (C) 1992  Remy Card (card@masi.ibp.fr)
 *                      Laboratoire MASI - Institut Blaise Pascal
 *                      Universite Pierre et Marie Curie (Paris VI)
 *  from
 *  linux/fs/minix/truncate.c   Copyright (C) 1991, 1992  Linus Torvalds
 * 
 *  ext2fs fsync primitive
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

#include <linux/fs.h>
#include <linux/ext2_fs.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>


/*
 *	File may be NULL when we are called. Perhaps we shouldn't
 *	even pass file to fsync ?
 */

int ext2_sync_file(struct file * file, struct dentry *dentry, int datasync)
{
	struct inode *inode = dentry->d_inode;
	return ext2_fsync_inode(inode, datasync);
}

int ext2_fsync_inode(struct inode *inode, int datasync)
{
	int err;
	
	err  = fsync_inode_buffers(inode);
	err |= fsync_inode_data_buffers(inode);
	if (!(inode->i_state & I_DIRTY))
		return err;
	if (datasync && !(inode->i_state & I_DIRTY_DATASYNC))
		return err;
	
	err |= ext2_sync_inode(inode);
	return err ? -EIO : 0;
}
