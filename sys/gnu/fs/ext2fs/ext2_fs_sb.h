/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 *  linux/include/linux/ext2_fs_sb.h
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs_sb.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#ifndef _LINUX_EXT2_FS_SB
#define _LINUX_EXT2_FS_SB

/*
 * The following is not needed anymore since the descriptors buffer
 * heads are now dynamically allocated
 */
/* #define EXT2_MAX_GROUP_DESC	8 */

#define EXT2_MAX_GROUP_LOADED	8

#define buffer_head buf
#define MAXMNTLEN	512

/*
 * second extended-fs super-block data in memory
 */
struct ext2_sb_info {
	unsigned long s_frag_size;	/* Size of a fragment in bytes */
	unsigned long s_frags_per_block;/* Number of fragments per block */
	unsigned long s_inodes_per_block;/* Number of inodes per block */
	unsigned long s_frags_per_group;/* Number of fragments in a group */
	unsigned long s_blocks_per_group;/* Number of blocks in a group */
	unsigned long s_inodes_per_group;/* Number of inodes in a group */
	unsigned long s_itb_per_group;	/* Number of inode table blocks per group */
	unsigned long s_db_per_group;	/* Number of descriptor blocks per group */
	unsigned long s_desc_per_block;	/* Number of group descriptors per block */
	unsigned long s_groups_count;	/* Number of groups in the fs */
	struct buffer_head * s_sbh;	/* Buffer containing the super block */
	struct ext2_super_block * s_es;	/* Pointer to the super block in the buffer */
	struct buffer_head ** s_group_desc;
	unsigned short s_loaded_inode_bitmaps;
	unsigned short s_loaded_block_bitmaps;
	unsigned long s_inode_bitmap_number[EXT2_MAX_GROUP_LOADED];
	struct buffer_head * s_inode_bitmap[EXT2_MAX_GROUP_LOADED];
	unsigned long s_block_bitmap_number[EXT2_MAX_GROUP_LOADED];
	struct buffer_head * s_block_bitmap[EXT2_MAX_GROUP_LOADED];
	int s_rename_lock;
	unsigned long  s_mount_opt;
	unsigned short s_resuid;
	unsigned short s_resgid;
	unsigned short s_mount_state;
	/* 
	   stuff that FFS keeps in its super block or that linux
	   has in its non-ext2 specific super block and which is
	   generally considered useful 
	*/
	unsigned long s_blocksize;
	unsigned long s_blocksize_bits;
	unsigned int  s_bshift;			/* = log2(s_blocksize) */
	quad_t	 s_qbmask;			/* = s_blocksize - 1 */
	unsigned int  s_fsbtodb;		/* shift to get disk block */
	char    s_rd_only;                      /* read-only 		*/
	char    s_dirt;                         /* fs modified flag */

	char    fs_fsmnt[MAXMNTLEN];            /* name mounted on */
};

#endif	/* _LINUX_EXT2_FS_SB */
