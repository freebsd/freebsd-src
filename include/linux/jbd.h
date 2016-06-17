/*
 * linux/include/linux/jbd.h
 * 
 * Written by Stephen C. Tweedie <sct@redhat.com>
 *
 * Copyright 1998-2000 Red Hat, Inc --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Definitions for transaction data structures for the buffer cache
 * filesystem journaling support.
 */

#ifndef _LINUX_JBD_H
#define _LINUX_JBD_H

#if defined(CONFIG_JBD) || defined(CONFIG_JBD_MODULE) || !defined(__KERNEL__)

/* Allow this file to be included directly into e2fsprogs */
#ifndef __KERNEL__
#include "jfs_compat.h"
#define JFS_DEBUG
#define jfs_debug jbd_debug
#else

#include <linux/journal-head.h>
#include <linux/stddef.h>
#include <asm/semaphore.h>
#endif

#define journal_oom_retry 1

/*
 * Define JBD_PARANOID_WRITES to cause a kernel BUG() check if ext3
 * finds a buffer unexpectedly dirty.  This is useful for debugging, but
 * can cause spurious kernel panics if there are applications such as
 * tune2fs modifying our buffer_heads behind our backs.
 */
#undef JBD_PARANOID_WRITES

/*
 * Define JBD_PARANIOD_IOFAIL to cause a kernel BUG() if ext3 finds
 * certain classes of error which can occur due to failed IOs.  Under
 * normal use we want ext3 to continue after such errors, because
 * hardware _can_ fail, but for debugging purposes when running tests on
 * known-good hardware we may want to trap these errors.
 */
#undef JBD_PARANOID_IOFAIL

#ifdef CONFIG_JBD_DEBUG
/*
 * Define JBD_EXPENSIVE_CHECKING to enable more expensive internal
 * consistency checks.  By default we don't do this unless
 * CONFIG_JBD_DEBUG is on.
 */
#define JBD_EXPENSIVE_CHECKING
extern int journal_enable_debug;

#define jbd_debug(n, f, a...)						\
	do {								\
		if ((n) <= journal_enable_debug) {			\
			printk (KERN_DEBUG "(%s, %d): %s: ",		\
				__FILE__, __LINE__, __FUNCTION__);	\
		  	printk (f, ## a);				\
		}							\
	} while (0)
#else
#define jbd_debug(f, a...)	/**/
#endif

extern void * __jbd_kmalloc (char *where, size_t size, int flags, int retry);
#define jbd_kmalloc(size, flags) \
	__jbd_kmalloc(__FUNCTION__, (size), (flags), journal_oom_retry)
#define jbd_rep_kmalloc(size, flags) \
	__jbd_kmalloc(__FUNCTION__, (size), (flags), 1)

#define JFS_MIN_JOURNAL_BLOCKS 1024

#ifdef __KERNEL__

/**
 * typedef handle_t - The handle_t type represents a single atomic update being performed by some process.
 *
 * All filesystem modifications made by the process go
 * through this handle.  Recursive operations (such as quota operations)
 * are gathered into a single update.
 *
 * The buffer credits field is used to account for journaled buffers
 * being modified by the running process.  To ensure that there is
 * enough log space for all outstanding operations, we need to limit the
 * number of outstanding buffers possible at any time.  When the
 * operation completes, any buffer credits not used are credited back to
 * the transaction, so that at all times we know how many buffers the
 * outstanding updates on a transaction might possibly touch. 
 * 
 * This is an opaque datatype.
 **/
typedef struct handle_s		handle_t;	/* Atomic operation type */


/**
 * typedef journal_t - The journal_t maintains all of the journaling state information for a single filesystem.
 *
 * journal_t is linked to from the fs superblock structure.
 * 
 * We use the journal_t to keep track of all outstanding transaction
 * activity on the filesystem, and to manage the state of the log
 * writing process.
 *
 * This is an opaque datatype.
 **/
typedef struct journal_s	journal_t;	/* Journal control structure */
#endif

/*
 * Internal structures used by the logging mechanism:
 */

#define JFS_MAGIC_NUMBER 0xc03b3998U /* The first 4 bytes of /dev/random! */

/*
 * On-disk structures
 */

/* 
 * Descriptor block types:
 */

#define JFS_DESCRIPTOR_BLOCK	1
#define JFS_COMMIT_BLOCK	2
#define JFS_SUPERBLOCK_V1	3
#define JFS_SUPERBLOCK_V2	4
#define JFS_REVOKE_BLOCK	5

/*
 * Standard header for all descriptor blocks:
 */
typedef struct journal_header_s
{
	__u32		h_magic;
	__u32		h_blocktype;
	__u32		h_sequence;
} journal_header_t;


/* 
 * The block tag: used to describe a single buffer in the journal 
 */
typedef struct journal_block_tag_s
{
	__u32		t_blocknr;	/* The on-disk block number */
	__u32		t_flags;	/* See below */
} journal_block_tag_t;

/* 
 * The revoke descriptor: used on disk to describe a series of blocks to
 * be revoked from the log 
 */
typedef struct journal_revoke_header_s
{
	journal_header_t r_header;
	int		 r_count;	/* Count of bytes used in the block */
} journal_revoke_header_t;


/* Definitions for the journal tag flags word: */
#define JFS_FLAG_ESCAPE		1	/* on-disk block is escaped */
#define JFS_FLAG_SAME_UUID	2	/* block has same uuid as previous */
#define JFS_FLAG_DELETED	4	/* block deleted by this transaction */
#define JFS_FLAG_LAST_TAG	8	/* last tag in this descriptor block */


/*
 * The journal superblock.  All fields are in big-endian byte order.
 */
typedef struct journal_superblock_s
{
/* 0x0000 */
	journal_header_t s_header;

/* 0x000C */
	/* Static information describing the journal */
	__u32	s_blocksize;		/* journal device blocksize */
	__u32	s_maxlen;		/* total blocks in journal file */
	__u32	s_first;		/* first block of log information */
	
/* 0x0018 */
	/* Dynamic information describing the current state of the log */
	__u32	s_sequence;		/* first commit ID expected in log */
	__u32	s_start;		/* blocknr of start of log */

/* 0x0020 */
	/* Error value, as set by journal_abort(). */
	__s32	s_errno;

/* 0x0024 */
	/* Remaining fields are only valid in a version-2 superblock */
	__u32	s_feature_compat; 	/* compatible feature set */
	__u32	s_feature_incompat; 	/* incompatible feature set */
	__u32	s_feature_ro_compat; 	/* readonly-compatible feature set */
/* 0x0030 */
	__u8	s_uuid[16];		/* 128-bit uuid for journal */

/* 0x0040 */
	__u32	s_nr_users;		/* Nr of filesystems sharing log */
	
	__u32	s_dynsuper;		/* Blocknr of dynamic superblock copy*/
	
/* 0x0048 */
	__u32	s_max_transaction;	/* Limit of journal blocks per trans.*/
	__u32	s_max_trans_data;	/* Limit of data blocks per trans. */

/* 0x0050 */
	__u32	s_padding[44];

/* 0x0100 */
	__u8	s_users[16*48];		/* ids of all fs'es sharing the log */
/* 0x0400 */
} journal_superblock_t;

#define JFS_HAS_COMPAT_FEATURE(j,mask)					\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_compat & cpu_to_be32((mask))))
#define JFS_HAS_RO_COMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_ro_compat & cpu_to_be32((mask))))
#define JFS_HAS_INCOMPAT_FEATURE(j,mask)				\
	((j)->j_format_version >= 2 &&					\
	 ((j)->j_superblock->s_feature_incompat & cpu_to_be32((mask))))

#define JFS_FEATURE_INCOMPAT_REVOKE	0x00000001

/* Features known to this kernel version: */
#define JFS_KNOWN_COMPAT_FEATURES	0
#define JFS_KNOWN_ROCOMPAT_FEATURES	0
#define JFS_KNOWN_INCOMPAT_FEATURES	JFS_FEATURE_INCOMPAT_REVOKE

#ifdef __KERNEL__

#include <linux/fs.h>
#include <linux/sched.h>

#define JBD_ASSERTIONS
#ifdef JBD_ASSERTIONS
#define J_ASSERT(assert)						\
do {									\
	if (!(assert)) {						\
		printk (KERN_EMERG					\
			"Assertion failure in %s() at %s:%d: \"%s\"\n",	\
			__FUNCTION__, __FILE__, __LINE__, # assert);	\
		BUG();							\
	}								\
} while (0)

#if defined(CONFIG_BUFFER_DEBUG)
void buffer_assertion_failure(struct buffer_head *bh);
#define J_ASSERT_BH(bh, expr)						\
	do {								\
		if (!(expr))						\
			buffer_assertion_failure(bh);			\
		J_ASSERT(expr);						\
	} while (0)
#define J_ASSERT_JH(jh, expr)	J_ASSERT_BH(jh2bh(jh), expr)
#else
#define J_ASSERT_BH(bh, expr)	J_ASSERT(expr)
#define J_ASSERT_JH(jh, expr)	J_ASSERT(expr)
#endif

#else
#define J_ASSERT(assert)	do { } while (0)
#endif		/* JBD_ASSERTIONS */

#if defined(JBD_PARANOID_IOFAIL)
#define J_EXPECT(expr, why...)		J_ASSERT(expr)
#define J_EXPECT_BH(bh, expr, why...)	J_ASSERT_BH(bh, expr)
#define J_EXPECT_JH(jh, expr, why...)	J_ASSERT_JH(jh, expr)
#else
#define __journal_expect(expr, why...)					     \
	do {								     \
		if (!(expr)) {						     \
			printk(KERN_ERR "EXT3-fs unexpected failure: %s;\n", # expr); \
			printk(KERN_ERR why);				     \
		}							     \
	} while (0)
#define J_EXPECT(expr, why...)		__journal_expect(expr, ## why)
#define J_EXPECT_BH(bh, expr, why...)	__journal_expect(expr, ## why)
#define J_EXPECT_JH(jh, expr, why...)	__journal_expect(expr, ## why)
#endif

enum jbd_state_bits {
	BH_JWrite
	  = BH_PrivateStart,	/* 1 if being written to log (@@@ DEBUGGING) */
	BH_Freed,		/* 1 if buffer has been freed (truncated) */
	BH_Revoked,		/* 1 if buffer has been revoked from the log */
	BH_RevokeValid,		/* 1 if buffer revoked flag is valid */
	BH_JBDDirty,		/* 1 if buffer is dirty but journaled */
};

/* Return true if the buffer is one which JBD is managing */
static inline int buffer_jbd(struct buffer_head *bh)
{
	return __buffer_state(bh, JBD);
}

static inline struct buffer_head *jh2bh(struct journal_head *jh)
{
	return jh->b_bh;
}

static inline struct journal_head *bh2jh(struct buffer_head *bh)
{
	return bh->b_private;
}

#define HAVE_JOURNAL_CALLBACK_STATUS
struct journal_callback {
	struct list_head jcb_list;
	void (*jcb_func)(struct journal_callback *jcb, int error);
	/* user data goes here */
};

struct jbd_revoke_table_s;

/**
 * The handle_t type represents a single atomic update being performed
 * by some process.  All filesystem modifications made by the process go
 * through this handle.  Recursive operations (such as quota operations)
 * are gathered into a single update.
 *
 * The buffer credits field is used to account for journaled buffers
 * being modified by the running process.  To ensure that there is
 * enough log space for all outstanding operations, we need to limit the
 * number of outstanding buffers possible at any time.  When the
 * operation completes, any buffer credits not used are credited back to
 * the transaction, so that at all times we know how many buffers the
 * outstanding updates on a transaction might possibly touch. 
 *
 * struct handle_s - The handle_s type is the concrete type associated with handle_t.
 * @h_transaction: Which compound transaction is this update a part of?
 * @h_buffer_credits: Number of remaining buffers we are allowed to dirty.
 * @h_ref: Reference count on this handle
 * @h_err: Field for caller's use to track errors through large fs operations
 * @h_sync: flag for sync-on-close
 * @h_jdata: flag to force data journaling
 * @h_aborted: flag indicating fatal error on handle
 **/

/* Docbook can't yet cope with the bit fields, but will leave the documentation
 * in so it can be fixed later. 
 */

struct handle_s 
{
	/* Which compound transaction is this update a part of? */
	transaction_t	      * h_transaction;

	/* Number of remaining buffers we are allowed to dirty: */
	int			h_buffer_credits;

	/* Reference count on this handle */
	int			h_ref;

	/* Field for caller's use to track errors through large fs */
	/* operations */
	int			h_err;

	/* List of application registered callbacks for this handle.
	 * The function(s) will be called after the transaction that
	 * this handle is part of has been committed to disk.
	 */
	struct list_head	h_jcb;

	/* Flags */
	unsigned int	h_sync:		1;	/* sync-on-close */
	unsigned int	h_jdata:	1;	/* force data journaling */
	unsigned int	h_aborted:	1;	/* fatal error on handle */
};


/* The transaction_t type is the guts of the journaling mechanism.  It
 * tracks a compound transaction through its various states:
 *
 * RUNNING:	accepting new updates
 * LOCKED:	Updates still running but we don't accept new ones
 * RUNDOWN:	Updates are tidying up but have finished requesting
 *		new buffers to modify (state not used for now)
 * FLUSH:       All updates complete, but we are still writing to disk
 * COMMIT:      All data on disk, writing commit record
 * FINISHED:	We still have to keep the transaction for checkpointing.
 *
 * The transaction keeps track of all of the buffers modified by a
 * running transaction, and all of the buffers committed but not yet
 * flushed to home for finished transactions.
 */

struct transaction_s 
{
	/* Pointer to the journal for this transaction. */
	journal_t *		t_journal;
	
	/* Sequence number for this transaction */
	tid_t			t_tid;
	
	/* Transaction's current state */
	enum {
		T_RUNNING,
		T_LOCKED,
		T_RUNDOWN,
		T_FLUSH,
		T_COMMIT,
		T_FINISHED 
	}			t_state;

	/* Where in the log does this transaction's commit start? */
	unsigned long		t_log_start;
	
	/* Doubly-linked circular list of all inodes owned by this
           transaction */	/* AKPM: unused */
	struct inode *		t_ilist;
	
	/* Number of buffers on the t_buffers list */
	int			t_nr_buffers;
	
	/* Doubly-linked circular list of all buffers reserved but not
           yet modified by this transaction */
	struct journal_head *	t_reserved_list;
	
	/* Doubly-linked circular list of all metadata buffers owned by this
           transaction */
	struct journal_head *	t_buffers;
	
	/*
	 * Doubly-linked circular list of all data buffers still to be
	 * flushed before this transaction can be committed.
	 * Protected by journal_datalist_lock.
	 */
	struct journal_head *	t_sync_datalist;
	
	/*
	 * Doubly-linked circular list of all writepage data buffers
	 * still to be written before this transaction can be committed.
	 * Protected by journal_datalist_lock.
	 */
	struct journal_head *	t_async_datalist;
	
	/* Doubly-linked circular list of all forget buffers (superceded
           buffers which we can un-checkpoint once this transaction
           commits) */
	struct journal_head *	t_forget;
	
	/*
	 * Doubly-linked circular list of all buffers still to be
	 * flushed before this transaction can be checkpointed.
	 */
	/* Protected by journal_datalist_lock */
	struct journal_head *	t_checkpoint_list;
	
	/* Doubly-linked circular list of temporary buffers currently
           undergoing IO in the log */
	struct journal_head *	t_iobuf_list;
	
	/* Doubly-linked circular list of metadata buffers being
           shadowed by log IO.  The IO buffers on the iobuf list and the
           shadow buffers on this list match each other one for one at
           all times. */
	struct journal_head *	t_shadow_list;
	
	/* Doubly-linked circular list of control buffers being written
           to the log. */
	struct journal_head *	t_log_list;
	
	/* Number of outstanding updates running on this transaction */
	int			t_updates;

	/* Number of buffers reserved for use by all handles in this
	 * transaction handle but not yet modified. */
	int			t_outstanding_credits;
	
	/*
	 * Forward and backward links for the circular list of all
	 * transactions awaiting checkpoint.
	 */
	/* Protected by journal_datalist_lock */
	transaction_t		*t_cpnext, *t_cpprev;

	/* When will the transaction expire (become due for commit), in
	 * jiffies ? */
	unsigned long		t_expires;

	/* How many handles used this transaction? */
	int t_handle_count;

	/* List of registered callback functions for this transaction.
	 * Called when the transaction is committed. */
	struct list_head	t_jcb;
};

/**
 * struct journal_s - The journal_s type is the concrete type associated with journal_t.
 * @j_flags:  General journaling state flags
 * @j_errno:  Is there an outstanding uncleared error on the journal (from a prior abort)? 
 * @j_sb_buffer: First part of superblock buffer
 * @j_superblock: Second part of superblock buffer
 * @j_format_version: Version of the superblock format
 * @j_barrier_count:  Number of processes waiting to create a barrier lock
 * @j_barrier: The barrier lock itself
 * @j_running_transaction: The current running transaction..
 * @j_committing_transaction: the transaction we are pushing to disk
 * @j_checkpoint_transactions: a linked circular list of all transactions waiting for checkpointing
 * @j_wait_transaction_locked: Wait queue for waiting for a locked transaction to start committing, or for a barrier lock to be released
 * @j_wait_logspace: Wait queue for waiting for checkpointing to complete
 * @j_wait_done_commit: Wait queue for waiting for commit to complete 
 * @j_wait_checkpoint:  Wait queue to trigger checkpointing
 * @j_wait_commit: Wait queue to trigger commit
 * @j_wait_updates: Wait queue to wait for updates to complete
 * @j_checkpoint_sem: Semaphore for locking against concurrent checkpoints
 * @j_sem: The main journal lock, used by lock_journal() 
 * @j_head: Journal head - identifies the first unused block in the journal
 * @j_tail: Journal tail - identifies the oldest still-used block in the journal.
 * @j_free: Journal free - how many free blocks are there in the journal?
 * @j_first: The block number of the first usable block 
 * @j_last: The block number one beyond the last usable block
 * @j_dev: Device where we store the journal
 * @j_blocksize: blocksize for the location where we store the journal.
 * @j_blk_offset: starting block offset for into the device where we store the journal
 * @j_fs_dev: Device which holds the client fs.  For internal journal this will be equal to j_dev
 * @j_maxlen: Total maximum capacity of the journal region on disk.
 * @j_inode: Optional inode where we store the journal.  If present, all  journal block numbers are mapped into this inode via bmap().
 * @j_tail_sequence:  Sequence number of the oldest transaction in the log 
 * @j_transaction_sequence: Sequence number of the next transaction to grant
 * @j_commit_sequence: Sequence number of the most recently committed transaction
 * @j_commit_request: Sequence number of the most recent transaction wanting commit 
 * @j_uuid: Uuid of client object.
 * @j_task: Pointer to the current commit thread for this journal
 * @j_max_transaction_buffers:  Maximum number of metadata buffers to allow in a single compound commit transaction
 * @j_commit_interval: What is the maximum transaction lifetime before we begin a commit?
 * @j_commit_timer:  The timer used to wakeup the commit thread
 * @j_commit_timer_active: Timer flag
 * @j_all_journals:  Link all journals together - system-wide 
 * @j_revoke: The revoke table - maintains the list of revoked blocks in the current transaction.
 **/

struct journal_s
{
	/* General journaling state flags */
	unsigned long		j_flags;

	/* Is there an outstanding uncleared error on the journal (from */
	/* a prior abort)? */
	int			j_errno;
	
	/* The superblock buffer */
	struct buffer_head *	j_sb_buffer;
	journal_superblock_t *	j_superblock;

	/* Version of the superblock format */
	int			j_format_version;

	/* Number of processes waiting to create a barrier lock */
	int			j_barrier_count;
	
	/* The barrier lock itself */
	struct semaphore	j_barrier;
	
	/* Transactions: The current running transaction... */
	transaction_t *		j_running_transaction;
	
	/* ... the transaction we are pushing to disk ... */
	transaction_t *		j_committing_transaction;
	
	/* ... and a linked circular list of all transactions waiting */
	/* for checkpointing. */
	/* Protected by journal_datalist_lock */
	transaction_t *		j_checkpoint_transactions;

	/* Wait queue for waiting for a locked transaction to start */
        /*  committing, or for a barrier lock to be released */
	wait_queue_head_t	j_wait_transaction_locked;
	
	/* Wait queue for waiting for checkpointing to complete */
	wait_queue_head_t	j_wait_logspace;
	
	/* Wait queue for waiting for commit to complete */
	wait_queue_head_t	j_wait_done_commit;
	
	/* Wait queue to trigger checkpointing */
	wait_queue_head_t	j_wait_checkpoint;
	
	/* Wait queue to trigger commit */
	wait_queue_head_t	j_wait_commit;
	
	/* Wait queue to wait for updates to complete */
	wait_queue_head_t	j_wait_updates;

	/* Semaphore for locking against concurrent checkpoints */
	struct semaphore 	j_checkpoint_sem;

	/* The main journal lock, used by lock_journal() */
	struct semaphore	j_sem;
		
	/* Journal head: identifies the first unused block in the journal. */
	unsigned long		j_head;
	
	/* Journal tail: identifies the oldest still-used block in the */
	/* journal. */
	unsigned long		j_tail;

	/* Journal free: how many free blocks are there in the journal? */
	unsigned long		j_free;

	/* Journal start and end: the block numbers of the first usable */
	/* block and one beyond the last usable block in the journal. */
	unsigned long		j_first, j_last;

	/* Device, blocksize and starting block offset for the location */
	/* where we store the journal. */
	kdev_t			j_dev;
	int			j_blocksize;
	unsigned int		j_blk_offset;

	/* Device which holds the client fs.  For internal journal this */
	/* will be equal to j_dev. */
	kdev_t			j_fs_dev;

	/* Total maximum capacity of the journal region on disk. */
	unsigned int		j_maxlen;

	/* Optional inode where we store the journal.  If present, all */
	/* journal block numbers are mapped into this inode via */
	/* bmap(). */
	struct inode *		j_inode;

	/* Sequence number of the oldest transaction in the log */
	tid_t			j_tail_sequence;
	/* Sequence number of the next transaction to grant */
	tid_t			j_transaction_sequence;
	/* Sequence number of the most recently committed transaction */
	tid_t			j_commit_sequence;
	/* Sequence number of the most recent transaction wanting commit */
	tid_t			j_commit_request;

	/* Journal uuid: identifies the object (filesystem, LVM volume   */
	/* etc) backed by this journal.  This will eventually be         */
	/* replaced by an array of uuids, allowing us to index multiple  */
	/* devices within a single journal and to perform atomic updates */
	/* across them.  */

	__u8			j_uuid[16];

	/* Pointer to the current commit thread for this journal */
	struct task_struct *	j_task;

	/* Maximum number of metadata buffers to allow in a single */
	/* compound commit transaction */
	int			j_max_transaction_buffers;

	/* What is the maximum transaction lifetime before we begin a */
	/* commit? */
	unsigned long		j_commit_interval;

	/* The timer used to wakeup the commit thread: */
	struct timer_list *	j_commit_timer;
	int			j_commit_timer_active;

	/* Link all journals together - system-wide */
	struct list_head	j_all_journals;

	/* The revoke table: maintains the list of revoked blocks in the */
        /*  current transaction. */
	struct jbd_revoke_table_s *j_revoke;
};

/* 
 * Journal flag definitions 
 */
#define JFS_UNMOUNT	0x001	/* Journal thread is being destroyed */
#define JFS_ABORT	0x002	/* Journaling has been aborted for errors. */
#define JFS_ACK_ERR	0x004	/* The errno in the sb has been acked */
#define JFS_FLUSHED	0x008	/* The journal superblock has been flushed */
#define JFS_LOADED	0x010	/* The journal superblock has been loaded */

/* 
 * Function declarations for the journaling transaction and buffer
 * management
 */

/* Filing buffers */
extern void __journal_unfile_buffer(struct journal_head *);
extern void journal_unfile_buffer(struct journal_head *);
extern void __journal_refile_buffer(struct journal_head *);
extern void journal_refile_buffer(struct journal_head *);
extern void __journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_free_buffer(struct journal_head *bh);
extern void journal_file_buffer(struct journal_head *, transaction_t *, int);
extern void __journal_clean_data_list(transaction_t *transaction);

/* Log buffer allocation */
extern struct journal_head * journal_get_descriptor_buffer(journal_t *);
int journal_next_log_block(journal_t *, unsigned long *);

/* Commit management */
void journal_end_buffer_io_sync(struct buffer_head *bh, int uptodate);
extern void journal_commit_transaction(journal_t *);

/* Checkpoint list management */
int __journal_clean_checkpoint_list(journal_t *journal);
extern void journal_remove_checkpoint(struct journal_head *);
extern void __journal_remove_checkpoint(struct journal_head *);
extern void journal_insert_checkpoint(struct journal_head *, transaction_t *);
extern void __journal_insert_checkpoint(struct journal_head *,transaction_t *);

/* Buffer IO */
extern int 
journal_write_metadata_buffer(transaction_t	  *transaction,
			      struct journal_head  *jh_in,
			      struct journal_head **jh_out,
			      int		   blocknr);

/* Transaction locking */
extern void		__wait_on_journal (journal_t *);

/*
 * Journal locking.
 *
 * We need to lock the journal during transaction state changes so that
 * nobody ever tries to take a handle on the running transaction while
 * we are in the middle of moving it to the commit phase.  
 *
 * Note that the locking is completely interrupt unsafe.  We never touch
 * journal structures from interrupts.
 *
 * In 2.2, the BKL was required for lock_journal.  This is no longer
 * the case.
 */

static inline void lock_journal(journal_t *journal)
{
	down(&journal->j_sem);
}

/* This returns zero if we acquired the semaphore */
static inline int try_lock_journal(journal_t * journal)
{
	return down_trylock(&journal->j_sem);
}

static inline void unlock_journal(journal_t * journal)
{
	up(&journal->j_sem);
}


static inline handle_t *journal_current_handle(void)
{
	return current->journal_info;
}

/* The journaling code user interface:
 *
 * Create and destroy handles
 * Register buffer modifications against the current transaction. 
 */

extern handle_t *journal_start(journal_t *, int nblocks);
extern handle_t *journal_try_start(journal_t *, int nblocks);
extern int	 journal_restart (handle_t *, int nblocks);
extern int	 journal_extend (handle_t *, int nblocks);
extern int	 journal_get_write_access (handle_t *, struct buffer_head *);
extern int	 journal_get_create_access (handle_t *, struct buffer_head *);
extern int	 journal_get_undo_access (handle_t *, struct buffer_head *);
extern int	 journal_dirty_data (handle_t *,
				struct buffer_head *, int async);
extern int	 journal_dirty_metadata (handle_t *, struct buffer_head *);
extern void	 journal_release_buffer (handle_t *, struct buffer_head *);
extern void	 journal_forget (handle_t *, struct buffer_head *);
extern void	 journal_sync_buffer (struct buffer_head *);
extern int	 journal_flushpage(journal_t *, struct page *, unsigned long);
extern int	 journal_try_to_free_buffers(journal_t *, struct page *, int);
extern int	 journal_stop(handle_t *);
extern int	 journal_flush (journal_t *);
extern void	 journal_callback_set(handle_t *handle,
				      void (*fn)(struct journal_callback *,int),
				      struct journal_callback *jcb);

extern void	 journal_lock_updates (journal_t *);
extern void	 journal_unlock_updates (journal_t *);

extern journal_t * journal_init_dev(kdev_t dev, kdev_t fs_dev,
				int start, int len, int bsize);
extern journal_t * journal_init_inode (struct inode *);
extern int	   journal_update_format (journal_t *);
extern int	   journal_check_used_features 
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_check_available_features 
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_set_features 
		   (journal_t *, unsigned long, unsigned long, unsigned long);
extern int	   journal_create     (journal_t *);
extern int	   journal_load       (journal_t *journal);
extern void	   journal_destroy    (journal_t *);
extern int	   journal_recover    (journal_t *journal);
extern int	   journal_wipe       (journal_t *, int);
extern int	   journal_skip_recovery	(journal_t *);
extern void	   journal_update_superblock	(journal_t *, int);
extern void	   __journal_abort_hard	(journal_t *);
extern void	   __journal_abort_soft	(journal_t *, int);
extern void	   journal_abort      (journal_t *, int);
extern int	   journal_errno      (journal_t *);
extern void	   journal_ack_err    (journal_t *);
extern int	   journal_clear_err  (journal_t *);
extern int	   journal_bmap(journal_t *, unsigned long, unsigned long *);
extern int	   journal_force_commit(journal_t *);

/*
 * journal_head management
 */
extern struct journal_head
		*journal_add_journal_head(struct buffer_head *bh);
extern void	journal_remove_journal_head(struct buffer_head *bh);
extern void	__journal_remove_journal_head(struct buffer_head *bh);
extern void	journal_unlock_journal_head(struct journal_head *jh);

/* Primary revoke support */
#define JOURNAL_REVOKE_DEFAULT_HASH 256
extern int	   journal_init_revoke(journal_t *, int);
extern void	   journal_destroy_revoke_caches(void);
extern int	   journal_init_revoke_caches(void);

extern void	   journal_destroy_revoke(journal_t *);
extern int	   journal_revoke (handle_t *,
				unsigned long, struct buffer_head *);
extern int	   journal_cancel_revoke(handle_t *, struct journal_head *);
extern void	   journal_write_revoke_records(journal_t *, transaction_t *);

/* Recovery revoke support */
extern int	   journal_set_revoke(journal_t *, unsigned long, tid_t);
extern int	   journal_test_revoke(journal_t *, unsigned long, tid_t);
extern void	   journal_clear_revoke(journal_t *);
extern void	   journal_brelse_array(struct buffer_head *b[], int n);

/* The log thread user interface:
 *
 * Request space in the current transaction, and force transaction commit
 * transitions on demand.
 */

extern int	log_space_left (journal_t *); /* Called with journal locked */
extern tid_t	log_start_commit (journal_t *, transaction_t *);
extern void	log_wait_commit (journal_t *, tid_t);
extern int	log_do_checkpoint (journal_t *, int);

extern void	log_wait_for_space(journal_t *, int nblocks);
extern void	__journal_drop_transaction(journal_t *, transaction_t *);
extern int	cleanup_journal_tail(journal_t *);

/* Reduce journal memory usage by flushing */
extern void shrink_journal_memory(void);

/* Debugging code only: */

#define jbd_ENOSYS() \
do {								      \
	printk (KERN_ERR "JBD unimplemented function " __FUNCTION__); \
	current->state = TASK_UNINTERRUPTIBLE;			      \
	schedule();						      \
} while (1)

extern void __jbd_unexpected_dirty_buffer(char *, int, struct journal_head *);
#define jbd_unexpected_dirty_buffer(jh) \
	__jbd_unexpected_dirty_buffer(__FUNCTION__, __LINE__, (jh))
	
/*
 * is_journal_abort
 *
 * Simple test wrapper function to test the JFS_ABORT state flag.  This
 * bit, when set, indicates that we have had a fatal error somewhere,
 * either inside the journaling layer or indicated to us by the client
 * (eg. ext3), and that we and should not commit any further
 * transactions.  
 */

static inline int is_journal_aborted(journal_t *journal)
{
	return journal->j_flags & JFS_ABORT;
}

static inline int is_handle_aborted(handle_t *handle)
{
	if (handle->h_aborted)
		return 1;
	return is_journal_aborted(handle->h_transaction->t_journal);
}

static inline void journal_abort_handle(handle_t *handle)
{
	handle->h_aborted = 1;
}

/* Not all architectures define BUG() */
#ifndef BUG
 #define BUG() do { \
        printk("kernel BUG at %s:%d!\n", __FILE__, __LINE__); \
	* ((char *) 0) = 0; \
 } while (0)
#endif /* BUG */

#endif /* __KERNEL__   */

/* Comparison functions for transaction IDs: perform comparisons using
 * modulo arithmetic so that they work over sequence number wraps. */

static inline int tid_gt(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference > 0);
}

static inline int tid_geq(tid_t x, tid_t y)
{
	int difference = (x - y);
	return (difference >= 0);
}

extern int journal_blocks_per_page(struct inode *inode);

/*
 * Definitions which augment the buffer_head layer
 */

/* journaling buffer types */
#define BJ_None		0	/* Not journaled */
#define BJ_SyncData	1	/* Normal data: flush before commit */
#define BJ_AsyncData	2	/* writepage data: wait on it before commit */
#define BJ_Metadata	3	/* Normal journaled metadata */
#define BJ_Forget	4	/* Buffer superceded by this transaction */
#define BJ_IO		5	/* Buffer is for temporary IO use */
#define BJ_Shadow	6	/* Buffer contents being shadowed to the log */
#define BJ_LogCtl	7	/* Buffer contains log descriptors */
#define BJ_Reserved	8	/* Buffer is reserved for access by journal */
#define BJ_Types	9
 
#ifdef __KERNEL__

extern spinlock_t jh_splice_lock;
/*
 * Once `expr1' has been found true, take jh_splice_lock
 * and then reevaluate everything.
 */
#define SPLICE_LOCK(expr1, expr2)				\
	({							\
		int ret = (expr1);				\
		if (ret) {					\
			spin_lock(&jh_splice_lock);		\
			ret = (expr1) && (expr2);		\
			spin_unlock(&jh_splice_lock);		\
		}						\
		ret;						\
	})

/*
 * A number of buffer state predicates.  They test for
 * buffer_jbd() because they are used in core kernel code.
 *
 * These will be racy on SMP unless we're *sure* that the
 * buffer won't be detached from the journalling system
 * in parallel.
 */

/* Return true if the buffer is on journal list `list' */
static inline int buffer_jlist_eq(struct buffer_head *bh, int list)
{
	return SPLICE_LOCK(buffer_jbd(bh), bh2jh(bh)->b_jlist == list);
}

/* Return true if this bufer is dirty wrt the journal */
static inline int buffer_jdirty(struct buffer_head *bh)
{
	return buffer_jbd(bh) && __buffer_state(bh, JBDDirty);
}

/* Return true if it's a data buffer which journalling is managing */
static inline int buffer_jbd_data(struct buffer_head *bh)
{
	return SPLICE_LOCK(buffer_jbd(bh),
			bh2jh(bh)->b_jlist == BJ_SyncData ||
			bh2jh(bh)->b_jlist == BJ_AsyncData);
}

#ifdef CONFIG_SMP
#define assert_spin_locked(lock)	J_ASSERT(spin_is_locked(lock))
#else
#define assert_spin_locked(lock)	do {} while(0)
#endif

#define buffer_trace_init(bh)	do {} while (0)
#define print_buffer_fields(bh)	do {} while (0)
#define print_buffer_trace(bh)	do {} while (0)
#define BUFFER_TRACE(bh, info)	do {} while (0)
#define BUFFER_TRACE2(bh, bh2, info)	do {} while (0)
#define JBUFFER_TRACE(jh, info)	do {} while (0)

#endif	/* __KERNEL__ */

#endif	/* CONFIG_JBD || CONFIG_JBD_MODULE || !__KERNEL__ */

/*
 * Compatibility no-ops which allow the kernel to compile without CONFIG_JBD
 * go here.
 */

#if defined(__KERNEL__) && !(defined(CONFIG_JBD) || defined(CONFIG_JBD_MODULE))

#define J_ASSERT(expr)			do {} while (0)
#define J_ASSERT_BH(bh, expr)		do {} while (0)
#define buffer_jbd(bh)			0
#define buffer_jlist_eq(bh, val)	0
#define journal_buffer_journal_lru(bh)	0

#endif	/* defined(__KERNEL__) && !defined(CONFIG_JBD) */
#endif	/* _LINUX_JBD_H */
