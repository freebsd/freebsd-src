/*
 * linux/fs/journal.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1998
 *
 * Copyright 1998 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Generic filesystem journal-writing code; part of the ext2fs
 * journaling system.
 *
 * This file manages journals: areas of disk reserved for logging
 * transactional updates.  This includes the kernel journaling thread
 * which is responsible for scheduling updates to the log.
 *
 * We do not actually manage the physical storage of the journal in this
 * file: that is left to a per-journal policy function, which allows us
 * to store the journal within a filesystem-specified area for ext2
 * journaling (ext2 can use a reserved inode for storing the log).
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>

EXPORT_SYMBOL(journal_start);
EXPORT_SYMBOL(journal_try_start);
EXPORT_SYMBOL(journal_restart);
EXPORT_SYMBOL(journal_extend);
EXPORT_SYMBOL(journal_stop);
EXPORT_SYMBOL(journal_lock_updates);
EXPORT_SYMBOL(journal_unlock_updates);
EXPORT_SYMBOL(journal_get_write_access);
EXPORT_SYMBOL(journal_get_create_access);
EXPORT_SYMBOL(journal_get_undo_access);
EXPORT_SYMBOL(journal_dirty_data);
EXPORT_SYMBOL(journal_dirty_metadata);
#if 0
EXPORT_SYMBOL(journal_release_buffer);
#endif
EXPORT_SYMBOL(journal_forget);
#if 0
EXPORT_SYMBOL(journal_sync_buffer);
#endif
EXPORT_SYMBOL(journal_flush);
EXPORT_SYMBOL(journal_revoke);
EXPORT_SYMBOL(journal_callback_set);

EXPORT_SYMBOL(journal_init_dev);
EXPORT_SYMBOL(journal_init_inode);
EXPORT_SYMBOL(journal_update_format);
EXPORT_SYMBOL(journal_check_used_features);
EXPORT_SYMBOL(journal_check_available_features);
EXPORT_SYMBOL(journal_set_features);
EXPORT_SYMBOL(journal_create);
EXPORT_SYMBOL(journal_load);
EXPORT_SYMBOL(journal_destroy);
EXPORT_SYMBOL(journal_recover);
EXPORT_SYMBOL(journal_update_superblock);
EXPORT_SYMBOL(journal_abort);
EXPORT_SYMBOL(journal_errno);
EXPORT_SYMBOL(journal_ack_err);
EXPORT_SYMBOL(journal_clear_err);
EXPORT_SYMBOL(log_wait_commit);
EXPORT_SYMBOL(log_start_commit);
EXPORT_SYMBOL(journal_wipe);
EXPORT_SYMBOL(journal_blocks_per_page);
EXPORT_SYMBOL(journal_flushpage);
EXPORT_SYMBOL(journal_try_to_free_buffers);
EXPORT_SYMBOL(journal_bmap);
EXPORT_SYMBOL(journal_force_commit);

static int journal_convert_superblock_v1(journal_t *, journal_superblock_t *);

/*
 * journal_datalist_lock is used to protect data buffers:
 *
 *	bh->b_transaction
 *	bh->b_tprev
 *	bh->b_tnext
 *
 * journal_free_buffer() is called from journal_try_to_free_buffer(), and is
 * async wrt everything else.
 *
 * It is also used for checkpoint data, also to protect against
 * journal_try_to_free_buffer():
 *
 *	bh->b_cp_transaction
 *	bh->b_cpnext
 *	bh->b_cpprev
 *	transaction->t_checkpoint_list
 *	transaction->t_cpnext
 *	transaction->t_cpprev
 *	journal->j_checkpoint_transactions
 *
 * It is global at this time rather than per-journal because it's
 * impossible for __journal_free_buffer to go from a buffer_head
 * back to a journal_t unracily (well, not true.  Fix later)
 *
 *
 * The `datalist' and `checkpoint list' functions are quite
 * separate and we could use two spinlocks here.
 *
 * lru_list_lock nests inside journal_datalist_lock.
 */
spinlock_t journal_datalist_lock = SPIN_LOCK_UNLOCKED;

/*
 * jh_splice_lock needs explantion.
 *
 * In a number of places we want to do things like:
 *
 *	if (buffer_jbd(bh) && bh2jh(bh)->foo)
 *
 * This is racy on SMP, because another CPU could remove the journal_head
 * in the middle of this expression.  We need locking.
 *
 * But we can greatly optimise the locking cost by testing BH_JBD
 * outside the lock.  So, effectively:
 *
 *	ret = 0;
 *	if (buffer_jbd(bh)) {
 *		spin_lock(&jh_splice_lock);
 *		if (buffer_jbd(bh)) {	 (* Still there? *)
 *			ret = bh2jh(bh)->foo;
 *		}
 *		spin_unlock(&jh_splice_lock);
 *	}
 *	return ret;
 *
 * Now, that protects us from races where another CPU can remove the
 * journal_head.  But it doesn't defend us from the situation where another
 * CPU can *add* a journal_head.  This is a correctness issue.  But it's not
 * a problem because a) the calling code was *already* racy and b) it often
 * can't happen at the call site and c) the places where we add journal_heads
 * tend to be under external locking.
 */
spinlock_t jh_splice_lock = SPIN_LOCK_UNLOCKED;

/*
 * List of all journals in the system.  Protected by the BKL.
 */
static LIST_HEAD(all_journals);

/*
 * Helper function used to manage commit timeouts
 */

static void commit_timeout(unsigned long __data)
{
	struct task_struct * p = (struct task_struct *) __data;

	wake_up_process(p);
}

/* Static check for data structure consistency.  There's no code
 * invoked --- we'll just get a linker failure if things aren't right.
 */
void __journal_internal_check(void)
{
	extern void journal_bad_superblock_size(void);
	if (sizeof(struct journal_superblock_s) != 1024)
		journal_bad_superblock_size();
}

/*
 * kjournald: The main thread function used to manage a logging device
 * journal.
 *
 * This kernel thread is responsible for two things:
 *
 * 1) COMMIT:  Every so often we need to commit the current state of the
 *    filesystem to disk.  The journal thread is responsible for writing
 *    all of the metadata buffers to disk.
 *
 * 2) CHECKPOINT: We cannot reuse a used section of the log file until all
 *    of the data in that part of the log has been rewritten elsewhere on
 *    the disk.  Flushing these old buffers to reclaim space in the log is
 *    known as checkpointing, and this thread is responsible for that job.
 */

journal_t *current_journal;		// AKPM: debug

int kjournald(void *arg)
{
	journal_t *journal = (journal_t *) arg;
	transaction_t *transaction;
	struct timer_list timer;

	current_journal = journal;

	lock_kernel();
	daemonize();
	reparent_to_init();
	spin_lock_irq(&current->sigmask_lock);
	sigfillset(&current->blocked);
	recalc_sigpending(current);
	spin_unlock_irq(&current->sigmask_lock);

	sprintf(current->comm, "kjournald");

	/* Set up an interval timer which can be used to trigger a
           commit wakeup after the commit interval expires */
	init_timer(&timer);
	timer.data = (unsigned long) current;
	timer.function = commit_timeout;
	journal->j_commit_timer = &timer;

	/* Record that the journal thread is running */
	journal->j_task = current;
	wake_up(&journal->j_wait_done_commit);

	printk(KERN_INFO "kjournald starting.  Commit interval %ld seconds\n",
			journal->j_commit_interval / HZ);
	list_add(&journal->j_all_journals, &all_journals);

	/* And now, wait forever for commit wakeup events. */
	while (1) {
		if (journal->j_flags & JFS_UNMOUNT)
			break;

		jbd_debug(1, "commit_sequence=%d, commit_request=%d\n",
			journal->j_commit_sequence, journal->j_commit_request);

		if (journal->j_commit_sequence != journal->j_commit_request) {
			jbd_debug(1, "OK, requests differ\n");
			if (journal->j_commit_timer_active) {
				journal->j_commit_timer_active = 0;
				del_timer(journal->j_commit_timer);
			}

			journal_commit_transaction(journal);
			continue;
		}

		wake_up(&journal->j_wait_done_commit);
		interruptible_sleep_on(&journal->j_wait_commit);

		jbd_debug(1, "kjournald wakes\n");

		/* Were we woken up by a commit wakeup event? */
		if ((transaction = journal->j_running_transaction) != NULL &&
		    journal->j_commit_interval &&
		    time_after_eq(jiffies, transaction->t_expires)) {
			journal->j_commit_request = transaction->t_tid;
			jbd_debug(1, "woke because of timeout\n");
		}
	}

	if (journal->j_commit_timer_active) {
		journal->j_commit_timer_active = 0;
		del_timer_sync(journal->j_commit_timer);
	}

	list_del(&journal->j_all_journals);

	journal->j_task = NULL;
	wake_up(&journal->j_wait_done_commit);
	unlock_kernel();
	jbd_debug(1, "Journal thread exiting.\n");
	return 0;
}

static void journal_start_thread(journal_t *journal)
{
	kernel_thread(kjournald, (void *) journal,
		      CLONE_VM | CLONE_FS | CLONE_FILES);
	while (!journal->j_task)
		sleep_on(&journal->j_wait_done_commit);
}

static void journal_kill_thread(journal_t *journal)
{
	journal->j_flags |= JFS_UNMOUNT;

	while (journal->j_task) {
		wake_up(&journal->j_wait_commit);
		sleep_on(&journal->j_wait_done_commit);
	}
}

#if 0

This is no longer needed - we do it in commit quite efficiently.
Note that if this function is resurrected, the loop needs to
be reorganised into the next_jh/last_jh algorithm.

/*
 * journal_clean_data_list: cleanup after data IO.
 *
 * Once the IO system has finished writing the buffers on the transaction's
 * data list, we can remove those buffers from the list.  This function
 * scans the list for such buffers and removes them cleanly.
 *
 * We assume that the journal is already locked.
 * We are called with journal_datalist_lock held.
 *
 * AKPM: This function looks inefficient.  Approximately O(n^2)
 * for potentially thousands of buffers.  It no longer shows on profiles
 * because these buffers are mainly dropped in journal_commit_transaction().
 */

void __journal_clean_data_list(transaction_t *transaction)
{
	struct journal_head *jh, *next;

	assert_spin_locked(&journal_datalist_lock);

restart:
	jh = transaction->t_sync_datalist;
	if (!jh)
		goto out;
	do {
		next = jh->b_tnext;
		if (!buffer_locked(jh2bh(jh)) && !buffer_dirty(jh2bh(jh))) {
			struct buffer_head *bh = jh2bh(jh);
			BUFFER_TRACE(bh, "data writeout complete: unfile");
			__journal_unfile_buffer(jh);
			jh->b_transaction = NULL;
			__journal_remove_journal_head(bh);
			refile_buffer(bh);
			__brelse(bh);
			goto restart;
		}
		jh = next;
	} while (transaction->t_sync_datalist &&
			jh != transaction->t_sync_datalist);
out:
	return;
}
#endif

/*
 * journal_write_metadata_buffer: write a metadata buffer to the journal.
 *
 * Writes a metadata buffer to a given disk block.  The actual IO is not
 * performed but a new buffer_head is constructed which labels the data
 * to be written with the correct destination disk block.
 *
 * Any magic-number escaping which needs to be done will cause a
 * copy-out here.  If the buffer happens to start with the
 * JFS_MAGIC_NUMBER, then we can't write it to the log directly: the
 * magic number is only written to the log for descripter blocks.  In
 * this case, we copy the data and replace the first word with 0, and we
 * return a result code which indicates that this buffer needs to be
 * marked as an escaped buffer in the corresponding log descriptor
 * block.  The missing word can then be restored when the block is read
 * during recovery.
 *
 * If the source buffer has already been modified by a new transaction
 * since we took the last commit snapshot, we use the frozen copy of
 * that data for IO.  If we end up using the existing buffer_head's data
 * for the write, then we *have* to lock the buffer to prevent anyone
 * else from using and possibly modifying it while the IO is in
 * progress.
 *
 * The function returns a pointer to the buffer_heads to be used for IO.
 *
 * We assume that the journal has already been locked in this function.
 *
 * Return value:
 *  <0: Error
 * >=0: Finished OK
 *
 * On success:
 * Bit 0 set == escape performed on the data
 * Bit 1 set == buffer copy-out performed (kfree the data after IO)
 */

static inline unsigned long virt_to_offset(void *p) 
{return ((unsigned long) p) & ~PAGE_MASK;}
					       
int journal_write_metadata_buffer(transaction_t *transaction,
				  struct journal_head  *jh_in,
				  struct journal_head **jh_out,
				  int blocknr)
{
	int need_copy_out = 0;
	int done_copy_out = 0;
	int do_escape = 0;
	char *mapped_data;
	struct buffer_head *new_bh;
	struct journal_head * new_jh;
	struct page *new_page;
	unsigned int new_offset;

	/*
	 * The buffer really shouldn't be locked: only the current committing
	 * transaction is allowed to write it, so nobody else is allowed
	 * to do any IO.
	 *
	 * akpm: except if we're journalling data, and write() output is
	 * also part of a shared mapping, and another thread has
	 * decided to launch a writepage() against this buffer.
	 */
	J_ASSERT_JH(jh_in, buffer_jdirty(jh2bh(jh_in)));

	/*
	 * If a new transaction has already done a buffer copy-out, then
	 * we use that version of the data for the commit.
	 */

	if (jh_in->b_frozen_data) {
		done_copy_out = 1;
		new_page = virt_to_page(jh_in->b_frozen_data);
		new_offset = virt_to_offset(jh_in->b_frozen_data);
	} else {
		new_page = jh2bh(jh_in)->b_page;
		new_offset = virt_to_offset(jh2bh(jh_in)->b_data);
	}

	mapped_data = ((char *) kmap(new_page)) + new_offset;

	/*
	 * Check for escaping
	 */
	if (* ((unsigned int *) mapped_data) == htonl(JFS_MAGIC_NUMBER)) {
		need_copy_out = 1;
		do_escape = 1;
	}

	/*
	 * Do we need to do a data copy?
	 */

	if (need_copy_out && !done_copy_out) {
		char *tmp;
		tmp = jbd_rep_kmalloc(jh2bh(jh_in)->b_size, GFP_NOFS);

		jh_in->b_frozen_data = tmp;
		memcpy (tmp, mapped_data, jh2bh(jh_in)->b_size);
		
		/* If we get to this path, we'll always need the new
		   address kmapped so that we can clear the escaped
		   magic number below. */
		kunmap(new_page);
		new_page = virt_to_page(tmp);
		new_offset = virt_to_offset(tmp);
		mapped_data = ((char *) kmap(new_page)) + new_offset;
		
		done_copy_out = 1;
	}

	/*
	 * Right, time to make up the new buffer_head.
	 */
	do {
		new_bh = get_unused_buffer_head(0);
		if (!new_bh) {
			printk (KERN_NOTICE "%s: ENOMEM at "
				"get_unused_buffer_head, trying again.\n",
				__FUNCTION__);
			yield();
		}
	} while (!new_bh);
	/* keep subsequent assertions sane */
	new_bh->b_prev_free = 0;
	new_bh->b_next_free = 0;
	new_bh->b_state = 0;
	init_buffer(new_bh, NULL, NULL);
	atomic_set(&new_bh->b_count, 1);
	new_jh = journal_add_journal_head(new_bh);

	set_bh_page(new_bh, new_page, new_offset);

	new_jh->b_transaction = NULL;
	new_bh->b_size = jh2bh(jh_in)->b_size;
	new_bh->b_dev = transaction->t_journal->j_dev;
	new_bh->b_blocknr = blocknr;
	new_bh->b_state |= (1 << BH_Mapped) | (1 << BH_Dirty);

	*jh_out = new_jh;

	/*
	 * Did we need to do an escaping?  Now we've done all the
	 * copying, we can finally do so.
	 */

	if (do_escape)
		* ((unsigned int *) mapped_data) = 0;
	kunmap(new_page);
	
	/*
	 * The to-be-written buffer needs to get moved to the io queue,
	 * and the original buffer whose contents we are shadowing or
	 * copying is moved to the transaction's shadow queue.
	 */
	JBUFFER_TRACE(jh_in, "file as BJ_Shadow");
	journal_file_buffer(jh_in, transaction, BJ_Shadow);
	JBUFFER_TRACE(new_jh, "file as BJ_IO");
	journal_file_buffer(new_jh, transaction, BJ_IO);

	return do_escape | (done_copy_out << 1);
}

/*
 * Allocation code for the journal file.  Manage the space left in the
 * journal, so that we can begin checkpointing when appropriate.
 */

/*
 * log_space_left: Return the number of free blocks left in the journal.
 *
 * Called with the journal already locked.
 */

int log_space_left (journal_t *journal)
{
	int left = journal->j_free;

	/* Be pessimistic here about the number of those free blocks
	 * which might be required for log descriptor control blocks. */

#define MIN_LOG_RESERVED_BLOCKS 32 /* Allow for rounding errors */

	left -= MIN_LOG_RESERVED_BLOCKS;

	if (left <= 0)
		return 0;
	left -= (left >> 3);
	return left;
}

/*
 * This function must be non-allocating for PF_MEMALLOC tasks
 */
tid_t log_start_commit (journal_t *journal, transaction_t *transaction)
{
	tid_t target = journal->j_commit_request;

	lock_kernel(); /* Protect journal->j_running_transaction */
	
	/*
	 * A NULL transaction asks us to commit the currently running
	 * transaction, if there is one.  
	 */
	if (transaction)
		target = transaction->t_tid;
	else {
		transaction = journal->j_running_transaction;
		if (!transaction)
			goto out;
		target = transaction->t_tid;
	}
		
	/*
	 * Are we already doing a recent enough commit?
	 */
	if (tid_geq(journal->j_commit_request, target))
		goto out;

	/*
	 * We want a new commit: OK, mark the request and wakup the
	 * commit thread.  We do _not_ do the commit ourselves.
	 */

	journal->j_commit_request = target;
	jbd_debug(1, "JBD: requesting commit %d/%d\n",
		  journal->j_commit_request,
		  journal->j_commit_sequence);
	wake_up(&journal->j_wait_commit);

out:
	unlock_kernel();
	return target;
}

/*
 * Wait for a specified commit to complete.
 * The caller may not hold the journal lock.
 */
void log_wait_commit (journal_t *journal, tid_t tid)
{
	lock_kernel();
#ifdef CONFIG_JBD_DEBUG
	lock_journal(journal);
	if (!tid_geq(journal->j_commit_request, tid)) {
		printk(KERN_EMERG "%s: error: j_commit_request=%d, tid=%d\n",
			__FUNCTION__, journal->j_commit_request, tid);
	}
	unlock_journal(journal);
#endif
	while (tid_gt(tid, journal->j_commit_sequence)) {
		jbd_debug(1, "JBD: want %d, j_commit_sequence=%d\n",
				  tid, journal->j_commit_sequence);
		wake_up(&journal->j_wait_commit);
		sleep_on(&journal->j_wait_done_commit);
	}
	unlock_kernel();
}

/*
 * Log buffer allocation routines:
 */

int journal_next_log_block(journal_t *journal, unsigned long *retp)
{
	unsigned long blocknr;

	J_ASSERT(journal->j_free > 1);

	blocknr = journal->j_head;
	journal->j_head++;
	journal->j_free--;
	if (journal->j_head == journal->j_last)
		journal->j_head = journal->j_first;
	return journal_bmap(journal, blocknr, retp);
}

/*
 * Conversion of logical to physical block numbers for the journal
 *
 * On external journals the journal blocks are identity-mapped, so
 * this is a no-op.  If needed, we can use j_blk_offset - everything is
 * ready.
 */
int journal_bmap(journal_t *journal, unsigned long blocknr, 
		 unsigned long *retp)
{
	int err = 0;
	unsigned long ret;

	if (journal->j_inode) {
		ret = bmap(journal->j_inode, blocknr);
		if (ret)
			*retp = ret;
		else {
			printk (KERN_ALERT "%s: journal block not found "
				"at offset %lu on %s\n", __FUNCTION__,
				blocknr, bdevname(journal->j_dev));
			err = -EIO;
			__journal_abort_soft(journal, err);
		}
	} else {
		*retp = blocknr; /* +journal->j_blk_offset */
	}
	return err;
}

/*
 * We play buffer_head aliasing tricks to write data/metadata blocks to
 * the journal without copying their contents, but for journal
 * descriptor blocks we do need to generate bona fide buffers.
 *
 * We return a jh whose bh is locked and ready to be populated.
 */

struct journal_head * journal_get_descriptor_buffer(journal_t *journal)
{
	struct buffer_head *bh;
	unsigned long blocknr;
	int err;

	err = journal_next_log_block(journal, &blocknr);

	if (err)
		return NULL;

	bh = getblk(journal->j_dev, blocknr, journal->j_blocksize);
	lock_buffer(bh);
	memset(bh->b_data, 0, journal->j_blocksize);
	BUFFER_TRACE(bh, "return this buffer");
	return journal_add_journal_head(bh);
}

/*
 * Management for journal control blocks: functions to create and
 * destroy journal_t structures, and to initialise and read existing
 * journal blocks from disk.  */

/* First: create and setup a journal_t object in memory.  We initialise
 * very few fields yet: that has to wait until we have created the
 * journal structures from from scratch, or loaded them from disk. */

static journal_t * journal_init_common (void)
{
	journal_t *journal;
	int err;

	MOD_INC_USE_COUNT;

	journal = jbd_kmalloc(sizeof(*journal), GFP_KERNEL);
	if (!journal)
		goto fail;
	memset(journal, 0, sizeof(*journal));

	init_waitqueue_head(&journal->j_wait_transaction_locked);
	init_waitqueue_head(&journal->j_wait_logspace);
	init_waitqueue_head(&journal->j_wait_done_commit);
	init_waitqueue_head(&journal->j_wait_checkpoint);
	init_waitqueue_head(&journal->j_wait_commit);
	init_waitqueue_head(&journal->j_wait_updates);
	init_MUTEX(&journal->j_barrier);
	init_MUTEX(&journal->j_checkpoint_sem);
	init_MUTEX(&journal->j_sem);

	journal->j_commit_interval = get_buffer_flushtime();

	/* The journal is marked for error until we succeed with recovery! */
	journal->j_flags = JFS_ABORT;

	/* Set up a default-sized revoke table for the new mount. */
	err = journal_init_revoke(journal, JOURNAL_REVOKE_DEFAULT_HASH);
	if (err) {
		kfree(journal);
		goto fail;
	}
	return journal;
fail:
	MOD_DEC_USE_COUNT;
	return NULL;
}

/* journal_init_dev and journal_init_inode:
 *
 * Create a journal structure assigned some fixed set of disk blocks to
 * the journal.  We don't actually touch those disk blocks yet, but we
 * need to set up all of the mapping information to tell the journaling
 * system where the journal blocks are.
 *
 */

 /**
  *  journal_t * journal_init_dev() - creates an initialises a journal structure
  *  @kdev: Block device on which to create the journal
  *  @fs_dev: Device which hold journalled filesystem for this journal.
  *  @start: Block nr Start of journal.
  *  @len:  Lenght of the journal in blocks.
  *  @blocksize: blocksize of journalling device
  *  @returns: a newly created journal_t *
  *  
  *  journal_init_dev creates a journal which maps a fixed contiguous
  *  range of blocks on an arbitrary block device.
  * 
  */
journal_t * journal_init_dev(kdev_t dev, kdev_t fs_dev,
			int start, int len, int blocksize)
{
	journal_t *journal = journal_init_common();
	struct buffer_head *bh;

	if (!journal)
		return NULL;

	journal->j_dev = dev;
	journal->j_fs_dev = fs_dev;
	journal->j_blk_offset = start;
	journal->j_maxlen = len;
	journal->j_blocksize = blocksize;

	bh = getblk(journal->j_dev, start, journal->j_blocksize);
	J_ASSERT(bh != NULL);
	journal->j_sb_buffer = bh;
	journal->j_superblock = (journal_superblock_t *)bh->b_data;

	return journal;
}
 
/** 
 *  journal_t * journal_init_inode () - creates a journal which maps to a inode.
 *  @inode: An inode to create the journal in
 *  
 * journal_init_inode creates a journal which maps an on-disk inode as
 * the journal.  The inode must exist already, must support bmap() and
 * must have all data blocks preallocated.
 */
journal_t * journal_init_inode (struct inode *inode)
{
	struct buffer_head *bh;
	journal_t *journal = journal_init_common();
	int err;
	unsigned long blocknr;

	if (!journal)
		return NULL;

	journal->j_dev = inode->i_dev;
	journal->j_fs_dev = inode->i_dev;
	journal->j_inode = inode;
	jbd_debug(1,
		  "journal %p: inode %s/%ld, size %Ld, bits %d, blksize %ld\n",
		  journal, bdevname(inode->i_dev), inode->i_ino, 
		  (long long) inode->i_size,
		  inode->i_sb->s_blocksize_bits, inode->i_sb->s_blocksize);

	journal->j_maxlen = inode->i_size >> inode->i_sb->s_blocksize_bits;
	journal->j_blocksize = inode->i_sb->s_blocksize;

	err = journal_bmap(journal, 0, &blocknr);
	/* If that failed, give up */
	if (err) {
		printk(KERN_ERR "%s: Cannnot locate journal superblock\n",
			__FUNCTION__);
		kfree(journal);
		return NULL;
	}
	
	bh = getblk(journal->j_dev, blocknr, journal->j_blocksize);
	J_ASSERT(bh != NULL);
	journal->j_sb_buffer = bh;
	journal->j_superblock = (journal_superblock_t *)bh->b_data;

	return journal;
}

/* 
 * If the journal init or create aborts, we need to mark the journal
 * superblock as being NULL to prevent the journal destroy from writing
 * back a bogus superblock. 
 */
static void journal_fail_superblock (journal_t *journal)
{
	struct buffer_head *bh = journal->j_sb_buffer;
	brelse(bh);
	journal->j_sb_buffer = NULL;
}

/*
 * Given a journal_t structure, initialise the various fields for
 * startup of a new journaling session.  We use this both when creating
 * a journal, and after recovering an old journal to reset it for
 * subsequent use.
 */

static int journal_reset (journal_t *journal)
{
	journal_superblock_t *sb = journal->j_superblock;
	unsigned int first, last;

	first = ntohl(sb->s_first);
	last = ntohl(sb->s_maxlen);

	journal->j_first = first;
	journal->j_last = last;

	journal->j_head = first;
	journal->j_tail = first;
	journal->j_free = last - first;

	journal->j_tail_sequence = journal->j_transaction_sequence;
	journal->j_commit_sequence = journal->j_transaction_sequence - 1;
	journal->j_commit_request = journal->j_commit_sequence;

	journal->j_max_transaction_buffers = journal->j_maxlen / 4;

	/* Add the dynamic fields and write it to disk. */
	journal_update_superblock(journal, 1);

	lock_journal(journal);
	journal_start_thread(journal);
	unlock_journal(journal);

	return 0;
}

/** 
 * int journal_create() - Initialise the new journal file
 * @journal: Journal to create. This structure must have been initialised
 * 
 * Given a journal_t structure which tells us which disk blocks we can
 * use, create a new journal superblock and initialise all of the
 * journal fields from scratch.  
 **/
int journal_create(journal_t *journal)
{
	unsigned long blocknr;
	struct buffer_head *bh;
	journal_superblock_t *sb;
	int i, err;

	if (journal->j_maxlen < JFS_MIN_JOURNAL_BLOCKS) {
		printk (KERN_ERR "Journal length (%d blocks) too short.\n",
			journal->j_maxlen);
		journal_fail_superblock(journal);
		return -EINVAL;
	}

	if (journal->j_inode == NULL) {
		/*
		 * We don't know what block to start at!
		 */
		printk(KERN_EMERG "%s: creation of journal on external "
			"device!\n", __FUNCTION__);
		BUG();
	}

	/* Zero out the entire journal on disk.  We cannot afford to
	   have any blocks on disk beginning with JFS_MAGIC_NUMBER. */
	jbd_debug(1, "JBD: Zeroing out journal blocks...\n");
	for (i = 0; i < journal->j_maxlen; i++) {
		err = journal_bmap(journal, i, &blocknr);
		if (err)
			return err;
		bh = getblk(journal->j_dev, blocknr, journal->j_blocksize);
		wait_on_buffer(bh);
		memset (bh->b_data, 0, journal->j_blocksize);
		BUFFER_TRACE(bh, "marking dirty");
		mark_buffer_dirty(bh);
		BUFFER_TRACE(bh, "marking uptodate");
		mark_buffer_uptodate(bh, 1);
		__brelse(bh);
	}

	fsync_no_super(journal->j_dev);
	jbd_debug(1, "JBD: journal cleared.\n");

	/* OK, fill in the initial static fields in the new superblock */
	sb = journal->j_superblock;

	sb->s_header.h_magic	 = htonl(JFS_MAGIC_NUMBER);
	sb->s_header.h_blocktype = htonl(JFS_SUPERBLOCK_V2);

	sb->s_blocksize	= htonl(journal->j_blocksize);
	sb->s_maxlen	= htonl(journal->j_maxlen);
	sb->s_first	= htonl(1);

	journal->j_transaction_sequence = 1;

	journal->j_flags &= ~JFS_ABORT;
	journal->j_format_version = 2;

	return journal_reset(journal);
}

/** 
 * void journal_update_superblock() - Update journal sb on disk.
 * @journal: The journal to update.
 * @wait: Set to '0' if you don't want to wait for IO completion.
 *
 * Update a journal's dynamic superblock fields and write it to disk,
 * optionally waiting for the IO to complete.
 */
void journal_update_superblock(journal_t *journal, int wait)
{
	journal_superblock_t *sb = journal->j_superblock;
	struct buffer_head *bh = journal->j_sb_buffer;

	jbd_debug(1,"JBD: updating superblock (start %ld, seq %d, errno %d)\n",
		  journal->j_tail, journal->j_tail_sequence, journal->j_errno);

	sb->s_sequence = htonl(journal->j_tail_sequence);
	sb->s_start    = htonl(journal->j_tail);
	sb->s_errno    = htonl(journal->j_errno);

	BUFFER_TRACE(bh, "marking dirty");
	mark_buffer_dirty(bh);
	ll_rw_block(WRITE, 1, &bh);
	if (wait)
		wait_on_buffer(bh);

	/* If we have just flushed the log (by marking s_start==0), then
	 * any future commit will have to be careful to update the
	 * superblock again to re-record the true start of the log. */

	if (sb->s_start)
		journal->j_flags &= ~JFS_FLUSHED;
	else
		journal->j_flags |= JFS_FLUSHED;
}


/*
 * Read the superblock for a given journal, performing initial
 * validation of the format.
 */

static int journal_get_superblock(journal_t *journal)
{
	struct buffer_head *bh;
	journal_superblock_t *sb;
	int err = -EIO;
	
	bh = journal->j_sb_buffer;

	J_ASSERT(bh != NULL);
	if (!buffer_uptodate(bh)) {
		ll_rw_block(READ, 1, &bh);
		wait_on_buffer(bh);
		if (!buffer_uptodate(bh)) {
			printk (KERN_ERR
				"JBD: IO error reading journal superblock\n");
			goto out;
		}
	}

	sb = journal->j_superblock;

	err = -EINVAL;
	
	if (sb->s_header.h_magic != htonl(JFS_MAGIC_NUMBER) ||
	    sb->s_blocksize != htonl(journal->j_blocksize)) {
		printk(KERN_WARNING "JBD: no valid journal superblock found\n");
		goto out;
	}

	switch(ntohl(sb->s_header.h_blocktype)) {
	case JFS_SUPERBLOCK_V1:
		journal->j_format_version = 1;
		break;
	case JFS_SUPERBLOCK_V2:
		journal->j_format_version = 2;
		break;
	default:
		printk(KERN_WARNING "JBD: unrecognised superblock format ID\n");
		goto out;
	}

	if (ntohl(sb->s_maxlen) < journal->j_maxlen)
		journal->j_maxlen = ntohl(sb->s_maxlen);
	else if (ntohl(sb->s_maxlen) > journal->j_maxlen) {
		printk (KERN_WARNING "JBD: journal file too short\n");
		goto out;
	}

	return 0;

out:
	journal_fail_superblock(journal);
	return err;
}

/*
 * Load the on-disk journal superblock and read the key fields into the
 * journal_t.
 */

static int load_superblock(journal_t *journal)
{
	int err;
	journal_superblock_t *sb;

	err = journal_get_superblock(journal);
	if (err)
		return err;

	sb = journal->j_superblock;

	journal->j_tail_sequence = ntohl(sb->s_sequence);
	journal->j_tail = ntohl(sb->s_start);
	journal->j_first = ntohl(sb->s_first);
	journal->j_last = ntohl(sb->s_maxlen);
	journal->j_errno = ntohl(sb->s_errno);

	return 0;
}


/**
 * int journal_load() - Read journal from disk.
 * @journal: Journal to act on.
 * 
 * Given a journal_t structure which tells us which disk blocks contain
 * a journal, read the journal from disk to initialise the in-memory
 * structures.
 */
int journal_load(journal_t *journal)
{
	int err;

	err = load_superblock(journal);
	if (err)
		return err;

	/* If this is a V2 superblock, then we have to check the
	 * features flags on it. */

	if (journal->j_format_version >= 2) {
		journal_superblock_t *sb = journal->j_superblock;

		if ((sb->s_feature_ro_compat &
		     ~cpu_to_be32(JFS_KNOWN_ROCOMPAT_FEATURES)) ||
		    (sb->s_feature_incompat &
		     ~cpu_to_be32(JFS_KNOWN_INCOMPAT_FEATURES))) {
			printk (KERN_WARNING
				"JBD: Unrecognised features on journal\n");
			return -EINVAL;
		}
	}

	/* Let the recovery code check whether it needs to recover any
	 * data from the journal. */
	if (journal_recover(journal))
		goto recovery_error;

	/* OK, we've finished with the dynamic journal bits:
	 * reinitialise the dynamic contents of the superblock in memory
	 * and reset them on disk. */
	if (journal_reset(journal))
		goto recovery_error;

	journal->j_flags &= ~JFS_ABORT;
	journal->j_flags |= JFS_LOADED;
	return 0;

recovery_error:
	printk (KERN_WARNING "JBD: recovery failed\n");
	return -EIO;
}

/**
 * void journal_destroy() - Release a journal_t structure.
 * @journal: Journal to act on.
* 
 * Release a journal_t structure once it is no longer in use by the
 * journaled object.
 */
void journal_destroy (journal_t *journal)
{
	/* Wait for the commit thread to wake up and die. */
	journal_kill_thread(journal);

	/* Force a final log commit */
	if (journal->j_running_transaction)
		journal_commit_transaction(journal);

	/* Force any old transactions to disk */
	lock_journal(journal);
	while (journal->j_checkpoint_transactions != NULL)
		log_do_checkpoint(journal, 1);

	J_ASSERT(journal->j_running_transaction == NULL);
	J_ASSERT(journal->j_committing_transaction == NULL);
	J_ASSERT(journal->j_checkpoint_transactions == NULL);

	/* We can now mark the journal as empty. */
	journal->j_tail = 0;
	journal->j_tail_sequence = ++journal->j_transaction_sequence;
	if (journal->j_sb_buffer) {
		journal_update_superblock(journal, 1);
		brelse(journal->j_sb_buffer);
	}

	if (journal->j_inode)
		iput(journal->j_inode);
	if (journal->j_revoke)
		journal_destroy_revoke(journal);

	unlock_journal(journal);
	kfree(journal);
	MOD_DEC_USE_COUNT;
}


/**
 *int journal_check_used_features () - Check if features specified are used.
 * 
 * Check whether the journal uses all of a given set of
 * features.  Return true (non-zero) if it does. 
 **/

int journal_check_used_features (journal_t *journal, unsigned long compat,
				 unsigned long ro, unsigned long incompat)
{
	journal_superblock_t *sb;

	if (!compat && !ro && !incompat)
		return 1;
	if (journal->j_format_version == 1)
		return 0;

	sb = journal->j_superblock;

	if (((be32_to_cpu(sb->s_feature_compat) & compat) == compat) &&
	    ((be32_to_cpu(sb->s_feature_ro_compat) & ro) == ro) &&
	    ((be32_to_cpu(sb->s_feature_incompat) & incompat) == incompat))
		return 1;

	return 0;
}

/**
 * int journal_check_available_features() - Check feature set in journalling layer
 * 
 * Check whether the journaling code supports the use of
 * all of a given set of features on this journal.  Return true
 * (non-zero) if it can. */

int journal_check_available_features (journal_t *journal, unsigned long compat,
				      unsigned long ro, unsigned long incompat)
{
	journal_superblock_t *sb;

	if (!compat && !ro && !incompat)
		return 1;

	sb = journal->j_superblock;

	/* We can support any known requested features iff the
	 * superblock is in version 2.  Otherwise we fail to support any
	 * extended sb features. */

	if (journal->j_format_version != 2)
		return 0;

	if ((compat   & JFS_KNOWN_COMPAT_FEATURES) == compat &&
	    (ro       & JFS_KNOWN_ROCOMPAT_FEATURES) == ro &&
	    (incompat & JFS_KNOWN_INCOMPAT_FEATURES) == incompat)
		return 1;

	return 0;
}

/**
 * int journal_set_features () - Mark a given journal feature in the superblock
 *
 * Mark a given journal feature as present on the
 * superblock.  Returns true if the requested features could be set. 
 *
 */

int journal_set_features (journal_t *journal, unsigned long compat,
			  unsigned long ro, unsigned long incompat)
{
	journal_superblock_t *sb;

	if (journal_check_used_features(journal, compat, ro, incompat))
		return 1;

	if (!journal_check_available_features(journal, compat, ro, incompat))
		return 0;

	jbd_debug(1, "Setting new features 0x%lx/0x%lx/0x%lx\n",
		  compat, ro, incompat);

	sb = journal->j_superblock;

	sb->s_feature_compat    |= cpu_to_be32(compat);
	sb->s_feature_ro_compat |= cpu_to_be32(ro);
	sb->s_feature_incompat  |= cpu_to_be32(incompat);

	return 1;
}


/**
 * int journal_update_format () - Update on-disk journal structure.
 *
 * Given an initialised but unloaded journal struct, poke about in the
 * on-disk structure to update it to the most recent supported version.
 */
int journal_update_format (journal_t *journal)
{
	journal_superblock_t *sb;
	int err;

	err = journal_get_superblock(journal);
	if (err)
		return err;

	sb = journal->j_superblock;

	switch (ntohl(sb->s_header.h_blocktype)) {
	case JFS_SUPERBLOCK_V2:
		return 0;
	case JFS_SUPERBLOCK_V1:
		return journal_convert_superblock_v1(journal, sb);
	default:
		break;
	}
	return -EINVAL;
}

static int journal_convert_superblock_v1(journal_t *journal,
					 journal_superblock_t *sb)
{
	int offset, blocksize;
	struct buffer_head *bh;

	printk(KERN_WARNING
		"JBD: Converting superblock from version 1 to 2.\n");

	/* Pre-initialise new fields to zero */
	offset = ((char *) &(sb->s_feature_compat)) - ((char *) sb);
	blocksize = ntohl(sb->s_blocksize);
	memset(&sb->s_feature_compat, 0, blocksize-offset);

	sb->s_nr_users = cpu_to_be32(1);
	sb->s_header.h_blocktype = cpu_to_be32(JFS_SUPERBLOCK_V2);
	journal->j_format_version = 2;

	bh = journal->j_sb_buffer;
	BUFFER_TRACE(bh, "marking dirty");
	mark_buffer_dirty(bh);
	ll_rw_block(WRITE, 1, &bh);
	wait_on_buffer(bh);
	return 0;
}


/**
 * int journal_flush () - Flush journal
 * @journal: Journal to act on.
 * 
 * Flush all data for a given journal to disk and empty the journal.
 * Filesystems can use this when remounting readonly to ensure that
 * recovery does not need to happen on remount.
 */

int journal_flush (journal_t *journal)
{
	int err = 0;
	transaction_t *transaction = NULL;
	unsigned long old_tail;

	lock_kernel();
	
	/* Force everything buffered to the log... */
	if (journal->j_running_transaction) {
		transaction = journal->j_running_transaction;
		log_start_commit(journal, transaction);
	} else if (journal->j_committing_transaction)
		transaction = journal->j_committing_transaction;

	/* Wait for the log commit to complete... */
	if (transaction)
		log_wait_commit(journal, transaction->t_tid);

	/* ...and flush everything in the log out to disk. */
	lock_journal(journal);
	while (!err && journal->j_checkpoint_transactions != NULL)
		err = log_do_checkpoint(journal, journal->j_maxlen);
	cleanup_journal_tail(journal);

	/* Finally, mark the journal as really needing no recovery.
	 * This sets s_start==0 in the underlying superblock, which is
	 * the magic code for a fully-recovered superblock.  Any future
	 * commits of data to the journal will restore the current
	 * s_start value. */
	old_tail = journal->j_tail;
	journal->j_tail = 0;
	journal_update_superblock(journal, 1);
	journal->j_tail = old_tail;

	unlock_journal(journal);

	J_ASSERT(!journal->j_running_transaction);
	J_ASSERT(!journal->j_committing_transaction);
	J_ASSERT(!journal->j_checkpoint_transactions);
	J_ASSERT(journal->j_head == journal->j_tail);
	J_ASSERT(journal->j_tail_sequence == journal->j_transaction_sequence);

	unlock_kernel();
	
	return err;
}

/**
 * int journal_wipe() - Wipe journal contents
 * @journal: Journal to act on.
 * @write: flag (see below)
 * 
 * Wipe out all of the contents of a journal, safely.  This will produce
 * a warning if the journal contains any valid recovery information.
 * Must be called between journal_init_*() and journal_load().
 *
 * If 'write' is non-zero, then we wipe out the journal on disk; otherwise
 * we merely suppress recovery.
 */

int journal_wipe (journal_t *journal, int write)
{
	journal_superblock_t *sb;
	int err = 0;

	J_ASSERT (!(journal->j_flags & JFS_LOADED));

	err = load_superblock(journal);
	if (err)
		return err;

	sb = journal->j_superblock;

	if (!journal->j_tail)
		goto no_recovery;

	printk (KERN_WARNING "JBD: %s recovery information on journal\n",
		write ? "Clearing" : "Ignoring");

	err = journal_skip_recovery(journal);
	if (write)
		journal_update_superblock(journal, 1);

 no_recovery:
	return err;
}

/*
 * journal_dev_name: format a character string to describe on what
 * device this journal is present.
 */

const char * journal_dev_name(journal_t *journal)
{
	kdev_t dev;

	if (journal->j_inode)
		dev = journal->j_inode->i_dev;
	else
		dev = journal->j_dev;

	return bdevname(dev);
}

/*
 * Journal abort has very specific semantics, which we describe
 * for journal abort. 
 *
 * Two internal function, which provide abort to te jbd layer
 * itself are here.
 */

/* Quick version for internal journal use (doesn't lock the journal).
 * Aborts hard --- we mark the abort as occurred, but do _nothing_ else,
 * and don't attempt to make any other journal updates. */
void __journal_abort_hard (journal_t *journal)
{
	transaction_t *transaction;

	if (journal->j_flags & JFS_ABORT)
		return;

	printk (KERN_ERR "Aborting journal on device %s.\n",
		journal_dev_name(journal));

	journal->j_flags |= JFS_ABORT;
	transaction = journal->j_running_transaction;
	if (transaction)
		log_start_commit(journal, transaction);
}

/* Soft abort: record the abort error status in the journal superblock,
 * but don't do any other IO. */
void __journal_abort_soft (journal_t *journal, int errno)
{
	if (journal->j_flags & JFS_ABORT)
		return;

	if (!journal->j_errno)
		journal->j_errno = errno;

	__journal_abort_hard(journal);

	if (errno)
		journal_update_superblock(journal, 1);
}

/**
 * void journal_abort () - Shutdown the journal immediately.
 * @journal: the journal to shutdown.
 * @errno:   an error number to record in the journal indicating
 *           the reason for the shutdown.
 *
 * Perform a complete, immediate shutdown of the ENTIRE
 * journal (not of a single transaction).  This operation cannot be
 * undone without closing and reopening the journal.
 *           
 * The journal_abort function is intended to support higher level error
 * recovery mechanisms such as the ext2/ext3 remount-readonly error
 * mode.
 *
 * Journal abort has very specific semantics.  Any existing dirty,
 * unjournaled buffers in the main filesystem will still be written to
 * disk by bdflush, but the journaling mechanism will be suspended
 * immediately and no further transaction commits will be honoured.
 *
 * Any dirty, journaled buffers will be written back to disk without
 * hitting the journal.  Atomicity cannot be guaranteed on an aborted
 * filesystem, but we _do_ attempt to leave as much data as possible
 * behind for fsck to use for cleanup.
 *
 * Any attempt to get a new transaction handle on a journal which is in
 * ABORT state will just result in an -EROFS error return.  A
 * journal_stop on an existing handle will return -EIO if we have
 * entered abort state during the update.
 *
 * Recursive transactions are not disturbed by journal abort until the
 * final journal_stop, which will receive the -EIO error.
 *
 * Finally, the journal_abort call allows the caller to supply an errno
 * which will be recorded (if possible) in the journal superblock.  This
 * allows a client to record failure conditions in the middle of a
 * transaction without having to complete the transaction to record the
 * failure to disk.  ext3_error, for example, now uses this
 * functionality.
 *
 * Errors which originate from within the journaling layer will NOT
 * supply an errno; a null errno implies that absolutely no further
 * writes are done to the journal (unless there are any already in
 * progress).
 * 
 */

void journal_abort (journal_t *journal, int errno)
{
	lock_journal(journal);
	__journal_abort_soft(journal, errno);
	unlock_journal(journal);
}

/** 
 * int journal_errno () - returns the journal's error state.
 * @journal: journal to examine.
 *
 * This is the errno numbet set with journal_abort(), the last
 * time the journal was mounted - if the journal was stopped
 * without calling abort this will be 0.
 *
 * If the journal has been aborted on this mount time -EROFS will
 * be returned.
 */
int journal_errno (journal_t *journal)
{
	int err;

	lock_journal(journal);
	if (journal->j_flags & JFS_ABORT)
		err = -EROFS;
	else
		err = journal->j_errno;
	unlock_journal(journal);
	return err;
}



/** 
 * int journal_clear_err () - clears the journal's error state
 *
 * An error must be cleared or Acked to take a FS out of readonly
 * mode.
 */
int journal_clear_err (journal_t *journal)
{
	int err = 0;

	lock_journal(journal);
	if (journal->j_flags & JFS_ABORT)
		err = -EROFS;
	else
		journal->j_errno = 0;
	unlock_journal(journal);
	return err;
}


/** 
 * void journal_ack_err() - Ack journal err.
 *
 * An error must be cleared or Acked to take a FS out of readonly
 * mode.
 */
void journal_ack_err (journal_t *journal)
{
	lock_journal(journal);
	if (journal->j_errno)
		journal->j_flags |= JFS_ACK_ERR;
	unlock_journal(journal);
}


/*
 * Report any unexpected dirty buffers which turn up.  Normally those
 * indicate an error, but they can occur if the user is running (say)
 * tune2fs to modify the live filesystem, so we need the option of
 * continuing as gracefully as possible.  #
 *
 * The caller should already hold the journal lock and
 * journal_datalist_lock spinlock: most callers will need those anyway
 * in order to probe the buffer's journaling state safely.
 */
void __jbd_unexpected_dirty_buffer(char *function, int line, 
				 struct journal_head *jh)
{
	struct buffer_head *bh = jh2bh(jh);
	int jlist;
	
	if (buffer_dirty(bh)) {
		printk ("%sUnexpected dirty buffer encountered at "
			"%s:%d (%s blocknr %lu)\n",
			KERN_WARNING, function, line,
			kdevname(bh->b_dev), bh->b_blocknr);
#ifdef JBD_PARANOID_WRITES
		J_ASSERT_BH (bh, !buffer_dirty(bh));
#endif	
		
		/* If this buffer is one which might reasonably be dirty
		 * --- ie. data, or not part of this journal --- then
		 * we're OK to leave it alone, but otherwise we need to
		 * move the dirty bit to the journal's own internal
		 * JBDDirty bit. */
		jlist = jh->b_jlist;
		
		if (jlist == BJ_Metadata || jlist == BJ_Reserved || 
		    jlist == BJ_Shadow || jlist == BJ_Forget) {
			if (atomic_set_buffer_clean(jh2bh(jh))) {
				set_bit(BH_JBDDirty, &jh2bh(jh)->b_state);
			}
		}
	}
}


int journal_blocks_per_page(struct inode *inode)
{
	return 1 << (PAGE_CACHE_SHIFT - inode->i_sb->s_blocksize_bits);
}

/*
 * shrink_journal_memory().
 * Called when we're under memory pressure.  Free up all the written-back
 * checkpointed metadata buffers.
 */
void shrink_journal_memory(void)
{
	struct list_head *list;

	lock_kernel();
	list_for_each(list, &all_journals) {
		journal_t *journal =
			list_entry(list, journal_t, j_all_journals);
		spin_lock(&journal_datalist_lock);
		__journal_clean_checkpoint_list(journal);
		spin_unlock(&journal_datalist_lock);
	}
	unlock_kernel();
}

/*
 * Simple support for retying memory allocations.  Introduced to help to
 * debug different VM deadlock avoidance strategies. 
 */
/*
 * Simple support for retying memory allocations.  Introduced to help to
 * debug different VM deadlock avoidance strategies. 
 */
void * __jbd_kmalloc (char *where, size_t size, int flags, int retry)
{
	void *p;
	static unsigned long last_warning;
	
	while (1) {
		p = kmalloc(size, flags);
		if (p)
			return p;
		if (!retry)
			return NULL;
		/* Log every retry for debugging.  Also log them to the
		 * syslog, but do rate-limiting on the non-debugging
		 * messages. */
		jbd_debug(1, "ENOMEM in %s, retrying.\n", where);

		if (time_after(jiffies, last_warning + 120*HZ)) {
			printk(KERN_NOTICE
			       "ENOMEM in %s, retrying.\n", where);
			last_warning = jiffies;
		}
		
		yield();
	}
}

/*
 * Journal_head storage management
 */
static kmem_cache_t *journal_head_cache;
#ifdef CONFIG_JBD_DEBUG
static atomic_t nr_journal_heads = ATOMIC_INIT(0);
#endif

static int journal_init_journal_head_cache(void)
{
	int retval;

	J_ASSERT(journal_head_cache == 0);
	journal_head_cache = kmem_cache_create("journal_head",
				sizeof(struct journal_head),
				0,		/* offset */
				0,		/* flags */
				NULL,		/* ctor */
				NULL);		/* dtor */
	retval = 0;
	if (journal_head_cache == 0) {
		retval = -ENOMEM;
		printk(KERN_EMERG "JBD: no memory for journal_head cache\n");
	}
	return retval;
}

static void journal_destroy_journal_head_cache(void)
{
	J_ASSERT(journal_head_cache != NULL);
	kmem_cache_destroy(journal_head_cache);
	journal_head_cache = 0;
}

/*
 * journal_head splicing and dicing
 */
static struct journal_head *journal_alloc_journal_head(void)
{
	struct journal_head *ret;
	static unsigned long last_warning;

#ifdef CONFIG_JBD_DEBUG
	atomic_inc(&nr_journal_heads);
#endif
	ret = kmem_cache_alloc(journal_head_cache, GFP_NOFS);
	if (ret == 0) {
		jbd_debug(1, "out of memory for journal_head\n");
		if (time_after(jiffies, last_warning + 5*HZ)) {
			printk(KERN_NOTICE "ENOMEM in %s, retrying.\n",
				__FUNCTION__);
			last_warning = jiffies;
		}
		while (ret == 0) {
			yield();
			ret = kmem_cache_alloc(journal_head_cache, GFP_NOFS);
		}
	}
	return ret;
}

static void journal_free_journal_head(struct journal_head *jh)
{
#ifdef CONFIG_JBD_DEBUG
	atomic_dec(&nr_journal_heads);
	memset(jh, 0x5b, sizeof(*jh));
#endif
	kmem_cache_free(journal_head_cache, jh);
}

/*
 * A journal_head is attached to a buffer_head whenever JBD has an
 * interest in the buffer.
 *
 * Whenever a buffer has an attached journal_head, its ->b_state:BH_JBD bit
 * is set.  This bit is tested in core kernel code where we need to take
 * JBD-specific actions.  Testing the zeroness of ->b_journal_head is not
 * reliable there.
 *
 * When a buffer has its BH_JBD bit set, its ->b_count is elevated by one.
 *
 * When a buffer has its BH_JBD bit set it is immune from being released by
 * core kernel code, mainly via ->b_count.
 *
 * A journal_head may be detached from its buffer_head when the journal_head's
 * b_transaction, b_cp_transaction and b_next_transaction pointers are NULL.
 * Various places in JBD call journal_remove_journal_head() to indicate that the
 * journal_head can be dropped if needed.
 *
 * Various places in the kernel want to attach a journal_head to a buffer_head
 * _before_ attaching the journal_head to a transaction.  To protect the
 * journal_head in this situation, journal_add_journal_head elevates the
 * journal_head's b_jcount refcount by one.  The caller must call
 * journal_unlock_journal_head() to undo this.
 *
 * So the typical usage would be:
 *
 *	(Attach a journal_head if needed.  Increments b_jcount)
 *	struct journal_head *jh = journal_add_journal_head(bh);
 *	...
 *	jh->b_transaction = xxx;
 *	journal_unlock_journal_head(jh);
 *
 * Now, the journal_head's b_jcount is zero, but it is safe from being released
 * because it has a non-zero b_transaction.
 */

/*
 * Give a buffer_head a journal_head.
 *
 * Doesn't need the journal lock.
 * May sleep.
 * Cannot be called with journal_datalist_lock held.
 */
struct journal_head *journal_add_journal_head(struct buffer_head *bh)
{
	struct journal_head *jh;

	spin_lock(&journal_datalist_lock);
	if (buffer_jbd(bh)) {
		jh = bh2jh(bh);
	} else {
		J_ASSERT_BH(bh,
			(atomic_read(&bh->b_count) > 0) ||
			(bh->b_page && bh->b_page->mapping));
		spin_unlock(&journal_datalist_lock);
		jh = journal_alloc_journal_head();
		memset(jh, 0, sizeof(*jh));
		spin_lock(&journal_datalist_lock);

		if (buffer_jbd(bh)) {
			/* Someone did it for us! */
			J_ASSERT_BH(bh, bh->b_private != NULL);
			journal_free_journal_head(jh);
			jh = bh->b_private;
		} else {
			/*
			 * We actually don't need jh_splice_lock when
			 * adding a journal_head - only on removal.
			 */
			spin_lock(&jh_splice_lock);
			set_bit(BH_JBD, &bh->b_state);
			bh->b_private = jh;
			jh->b_bh = bh;
			atomic_inc(&bh->b_count);
			spin_unlock(&jh_splice_lock);
			BUFFER_TRACE(bh, "added journal_head");
		}
	}
	jh->b_jcount++;
	spin_unlock(&journal_datalist_lock);
	return bh->b_private;
}

/*
 * journal_remove_journal_head(): if the buffer isn't attached to a transaction
 * and has a zero b_jcount then remove and release its journal_head.   If we did
 * see that the buffer is not used by any transaction we also "logically"
 * decrement ->b_count.
 *
 * We in fact take an additional increment on ->b_count as a convenience,
 * because the caller usually wants to do additional things with the bh
 * after calling here.
 * The caller of journal_remove_journal_head() *must* run __brelse(bh) at some
 * time.  Once the caller has run __brelse(), the buffer is eligible for
 * reaping by try_to_free_buffers().
 *
 * Requires journal_datalist_lock.
 */
void __journal_remove_journal_head(struct buffer_head *bh)
{
	struct journal_head *jh = bh2jh(bh);

	assert_spin_locked(&journal_datalist_lock);
	J_ASSERT_JH(jh, jh->b_jcount >= 0);
	atomic_inc(&bh->b_count);
	if (jh->b_jcount == 0) {
		if (jh->b_transaction == NULL &&
				jh->b_next_transaction == NULL &&
				jh->b_cp_transaction == NULL) {
			J_ASSERT_BH(bh, buffer_jbd(bh));
			J_ASSERT_BH(bh, jh2bh(jh) == bh);
			BUFFER_TRACE(bh, "remove journal_head");
			spin_lock(&jh_splice_lock);
			bh->b_private = NULL;
			jh->b_bh = NULL;	/* debug, really */
			clear_bit(BH_JBD, &bh->b_state);
			__brelse(bh);
			spin_unlock(&jh_splice_lock);
			journal_free_journal_head(jh);
		} else {
			BUFFER_TRACE(bh, "journal_head was locked");
		}
	}
}

void journal_unlock_journal_head(struct journal_head *jh)
{
	spin_lock(&journal_datalist_lock);
	J_ASSERT_JH(jh, jh->b_jcount > 0);
	--jh->b_jcount;
	if (!jh->b_jcount && !jh->b_transaction) {
		struct buffer_head *bh;
		bh = jh2bh(jh);
		__journal_remove_journal_head(bh);
		__brelse(bh);
	}
	
	spin_unlock(&journal_datalist_lock);
}

void journal_remove_journal_head(struct buffer_head *bh)
{
	spin_lock(&journal_datalist_lock);
	__journal_remove_journal_head(bh);
	spin_unlock(&journal_datalist_lock);
}

/*
 * /proc tunables
 */
#if defined(CONFIG_JBD_DEBUG)
int journal_enable_debug;
EXPORT_SYMBOL(journal_enable_debug);
#endif

#if defined(CONFIG_JBD_DEBUG) && defined(CONFIG_PROC_FS)

static struct proc_dir_entry *proc_jbd_debug;

int read_jbd_debug(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	int ret;

	ret = sprintf(page + off, "%d\n", journal_enable_debug);
	*eof = 1;
	return ret;
}

int write_jbd_debug(struct file *file, const char *buffer,
			   unsigned long count, void *data)
{
	char buf[32];

	if (count > ARRAY_SIZE(buf) - 1)
		count = ARRAY_SIZE(buf) - 1;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	buf[ARRAY_SIZE(buf) - 1] = '\0';
	journal_enable_debug = simple_strtoul(buf, NULL, 10);
	return count;
}

#define JBD_PROC_NAME "sys/fs/jbd-debug"

static void __init create_jbd_proc_entry(void)
{
	proc_jbd_debug = create_proc_entry(JBD_PROC_NAME, 0644, NULL);
	if (proc_jbd_debug) {
		/* Why is this so hard? */
		proc_jbd_debug->read_proc = read_jbd_debug;
		proc_jbd_debug->write_proc = write_jbd_debug;
	}
}

static void __exit remove_jbd_proc_entry(void)
{
	if (proc_jbd_debug)
		remove_proc_entry(JBD_PROC_NAME, NULL);
}

#else

#define create_jbd_proc_entry() do {} while (0)
#define remove_jbd_proc_entry() do {} while (0)

#endif

/*
 * Module startup and shutdown
 */

static int __init journal_init_caches(void)
{
	int ret;

	ret = journal_init_revoke_caches();
	if (ret == 0)
		ret = journal_init_journal_head_cache();
	return ret;
}

static void journal_destroy_caches(void)
{
	journal_destroy_revoke_caches();
	journal_destroy_journal_head_cache();
}

static int __init journal_init(void)
{
	int ret;

	printk(KERN_INFO "Journalled Block Device driver loaded\n");
	ret = journal_init_caches();
	if (ret != 0)
		journal_destroy_caches();
	create_jbd_proc_entry();
	return ret;
}

static void __exit journal_exit(void)
{
#ifdef CONFIG_JBD_DEBUG
	int n = atomic_read(&nr_journal_heads);
	if (n)
		printk(KERN_EMERG "JBD: leaked %d journal_heads!\n", n);
#endif
	remove_jbd_proc_entry();
	journal_destroy_caches();
}

MODULE_LICENSE("GPL");
module_init(journal_init);
module_exit(journal_exit);

