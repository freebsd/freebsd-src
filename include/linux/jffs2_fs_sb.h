/* $Id: jffs2_fs_sb.h,v 1.16.2.1 2002/02/23 14:13:34 dwmw2 Exp $ */

#ifndef _JFFS2_FS_SB
#define _JFFS2_FS_SB

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/completion.h>
#include <asm/semaphore.h>
#include <linux/list.h>

#define INOCACHE_HASHSIZE 1

#define JFFS2_SB_FLAG_RO 1
#define JFFS2_SB_FLAG_MOUNTING 2

/* A struct for the overall file system control.  Pointers to
   jffs2_sb_info structs are named `c' in the source code.  
   Nee jffs_control
*/
struct jffs2_sb_info {
	struct mtd_info *mtd;

	__u32 highest_ino;
	unsigned int flags;
	spinlock_t nodelist_lock;

	//	pid_t thread_pid;		/* GC thread's PID */
	struct task_struct *gc_task;	/* GC task struct */
	struct semaphore gc_thread_start; /* GC thread start mutex */
	struct completion gc_thread_exit; /* GC thread exit completion port */
	//	__u32 gc_minfree_threshold;	/* GC trigger thresholds */
	//	__u32 gc_maxdirty_threshold;

	struct semaphore alloc_sem;	/* Used to protect all the following 
					   fields, and also to protect against
					   out-of-order writing of nodes.
					   And GC.
					*/
	__u32 flash_size;
	__u32 used_size;
	__u32 dirty_size;
	__u32 free_size;
	__u32 erasing_size;
	__u32 bad_size;
	__u32 sector_size;
	//	__u32 min_free_size;
	//	__u32 max_chunk_size;

	__u32 nr_free_blocks;
	__u32 nr_erasing_blocks;

	__u32 nr_blocks;
	struct jffs2_eraseblock *blocks;	/* The whole array of blocks. Used for getting blocks 
						 * from the offset (blocks[ofs / sector_size]) */
	struct jffs2_eraseblock *nextblock;	/* The block we're currently filling */

	struct jffs2_eraseblock *gcblock;	/* The block we're currently garbage-collecting */

	struct list_head clean_list;		/* Blocks 100% full of clean data */
	struct list_head dirty_list;		/* Blocks with some dirty space */
	struct list_head erasing_list;		/* Blocks which are currently erasing */
	struct list_head erase_pending_list;	/* Blocks which need erasing */
	struct list_head erase_complete_list;	/* Blocks which are erased and need the clean marker written to them */
	struct list_head free_list;		/* Blocks which are free and ready to be used */
	struct list_head bad_list;		/* Bad blocks. */
	struct list_head bad_used_list;		/* Bad blocks with valid data in. */

	spinlock_t erase_completion_lock;	/* Protect free_list and erasing_list 
						   against erase completion handler */
	wait_queue_head_t erase_wait;		/* For waiting for erases to complete */
	struct jffs2_inode_cache *inocache_list[INOCACHE_HASHSIZE];
	spinlock_t inocache_lock;
};

#ifdef JFFS2_OUT_OF_KERNEL
#define JFFS2_SB_INFO(sb) ((struct jffs2_sb_info *) &(sb)->u)
#else
#define JFFS2_SB_INFO(sb) (&sb->u.jffs2_sb)
#endif

#define OFNI_BS_2SFFJ(c)  ((struct super_block *) ( ((char *)c) - ((char *)(&((struct super_block *)NULL)->u)) ) )

#endif /* _JFFS2_FB_SB */
