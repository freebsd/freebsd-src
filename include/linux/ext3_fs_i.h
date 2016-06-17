/*
 *  linux/include/linux/ext3_fs_i.h
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs_i.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#ifndef _LINUX_EXT3_FS_I
#define _LINUX_EXT3_FS_I

#include <linux/rwsem.h>

/*
 * second extended file system inode data in memory
 */
struct ext3_inode_info {
	__u32	i_data[15];
	__u32	i_flags;
#ifdef EXT3_FRAGMENTS
	__u32	i_faddr;
	__u8	i_frag_no;
	__u8	i_frag_size;
	__u16	unused;			/* formerly i_osync */
#endif
	__u32	i_file_acl;
	__u32	i_dir_acl;
	__u32	i_dtime;
	__u32	i_block_group;
	__u32	i_state;		/* Dynamic state flags for ext3 */
	__u32	i_next_alloc_block;
	__u32	i_next_alloc_goal;
#ifdef EXT3_PREALLOCATE
	__u32	i_prealloc_block;
	__u32	i_prealloc_count;
#endif
	__u32	i_dir_start_lookup;
	
	struct list_head i_orphan;	/* unlinked but open inodes */

	/*
	 * i_disksize keeps track of what the inode size is ON DISK, not
	 * in memory.  During truncate, i_size is set to the new size by
	 * the VFS prior to calling ext3_truncate(), but the filesystem won't
	 * set i_disksize to 0 until the truncate is actually under way.
	 *
	 * The intent is that i_disksize always represents the blocks which
	 * are used by this file.  This allows recovery to restart truncate
	 * on orphans if we crash during truncate.  We actually write i_disksize
	 * into the on-disk inode when writing inodes out, instead of i_size.
	 *
	 * The only time when i_disksize and i_size may be different is when
	 * a truncate is in progress.  The only things which change i_disksize
	 * are ext3_get_block (growth) and ext3_truncate (shrinkth).
	 */
	loff_t	i_disksize;

	/*
	 * truncate_sem is for serialising ext3_truncate() against
	 * ext3_getblock().  In the 2.4 ext2 design, great chunks of inode's
	 * data tree are chopped off during truncate. We can't do that in
	 * ext3 because whenever we perform intermediate commits during
	 * truncate, the inode and all the metadata blocks *must* be in a
	 * consistent state which allows truncation of the orphans to restart
	 * during recovery.  Hence we must fix the get_block-vs-truncate race
	 * by other means, so we have truncate_sem.
	 */
	struct rw_semaphore truncate_sem;
};

#endif	/* _LINUX_EXT3_FS_I */
