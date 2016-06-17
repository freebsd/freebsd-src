/*
 *  linux/include/linux/ext3_fs_sb.h
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

#ifndef _LINUX_EXT3_FS_SB
#define _LINUX_EXT3_FS_SB

#ifdef __KERNEL__
#include <linux/timer.h>
#include <linux/wait.h>
#endif

/*
 * The following is not needed anymore since the descriptors buffer
 * heads are now dynamically allocated
 */
/* #define EXT3_MAX_GROUP_DESC	8 */

#define EXT3_MAX_GROUP_LOADED	8

/*
 * third extended-fs super-block data in memory
 */
struct ext3_sb_info {
	unsigned long s_frag_size;	/* Size of a fragment in bytes */
	unsigned long s_frags_per_block;/* Number of fragments per block */
	unsigned long s_inodes_per_block;/* Number of inodes per block */
	unsigned long s_frags_per_group;/* Number of fragments in a group */
	unsigned long s_blocks_per_group;/* Number of blocks in a group */
	unsigned long s_inodes_per_group;/* Number of inodes in a group */
	unsigned long s_itb_per_group;	/* Number of inode table blocks per group */
	unsigned long s_gdb_count;	/* Number of group descriptor blocks */
	unsigned long s_desc_per_block;	/* Number of group descriptors per block */
	unsigned long s_groups_count;	/* Number of groups in the fs */
	struct buffer_head * s_sbh;	/* Buffer containing the super block */
	struct ext3_super_block * s_es;	/* Pointer to the super block in the buffer */
	struct buffer_head ** s_group_desc;
	unsigned short s_loaded_inode_bitmaps;
	unsigned short s_loaded_block_bitmaps;
	unsigned long s_inode_bitmap_number[EXT3_MAX_GROUP_LOADED];
	struct buffer_head * s_inode_bitmap[EXT3_MAX_GROUP_LOADED];
	unsigned long s_block_bitmap_number[EXT3_MAX_GROUP_LOADED];
	struct buffer_head * s_block_bitmap[EXT3_MAX_GROUP_LOADED];
	unsigned long  s_mount_opt;
	uid_t s_resuid;
	gid_t s_resgid;
	unsigned short s_mount_state;
	unsigned short s_pad;
	int s_addr_per_block_bits;
	int s_desc_per_block_bits;
	int s_inode_size;
	int s_first_ino;
	u32 s_next_generation;

	/* Journaling */
	struct inode * s_journal_inode;
	struct journal_s * s_journal;
	struct list_head s_orphan;
	unsigned long s_commit_interval;
	struct block_device *journal_bdev;
#ifdef CONFIG_JBD_DEBUG
	struct timer_list turn_ro_timer;	/* For turning read-only (crash simulation) */
	wait_queue_head_t ro_wait_queue;	/* For people waiting for the fs to go read-only */
#endif
};

#endif	/* _LINUX_EXT3_FS_SB */
