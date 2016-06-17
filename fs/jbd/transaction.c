/*
 * linux/fs/transaction.c
 * 
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1998
 *
 * Copyright 1998 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Generic filesystem transaction handling code; part of the ext2fs
 * journaling system.  
 *
 * This file manages transactions (compound commits managed by the
 * journaling code) and handles (individual atomic operations by the
 * filesystem).
 */

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/locks.h>
#include <linux/timer.h>
#include <linux/smp_lock.h>
#include <linux/mm.h>

extern spinlock_t journal_datalist_lock;

/*
 * get_transaction: obtain a new transaction_t object.
 *
 * Simply allocate and initialise a new transaction.  Create it in
 * RUNNING state and add it to the current journal (which should not
 * have an existing running transaction: we only make a new transaction
 * once we have started to commit the old one).
 *
 * Preconditions:
 *	The journal MUST be locked.  We don't perform atomic mallocs on the
 *	new transaction	and we can't block without protecting against other
 *	processes trying to touch the journal while it is in transition.
 */

static transaction_t * get_transaction (journal_t * journal, int is_try)
{
	transaction_t * transaction;

	transaction = jbd_kmalloc (sizeof (transaction_t), GFP_NOFS);
	if (!transaction)
		return NULL;
	
	memset (transaction, 0, sizeof (transaction_t));
	
	transaction->t_journal = journal;
	transaction->t_state = T_RUNNING;
	transaction->t_tid = journal->j_transaction_sequence++;
	transaction->t_expires = jiffies + journal->j_commit_interval;
	INIT_LIST_HEAD(&transaction->t_jcb);

	if (journal->j_commit_interval) {
		/* Set up the commit timer for the new transaction. */
		J_ASSERT (!journal->j_commit_timer_active);
		journal->j_commit_timer_active = 1;
		journal->j_commit_timer->expires = transaction->t_expires;
		add_timer(journal->j_commit_timer);
	}
	
	J_ASSERT (journal->j_running_transaction == NULL);
	journal->j_running_transaction = transaction;

	return transaction;
}

/*
 * Handle management.
 *
 * A handle_t is an object which represents a single atomic update to a
 * filesystem, and which tracks all of the modifications which form part
 * of that one update.
 */

/*
 * start_this_handle: Given a handle, deal with any locking or stalling
 * needed to make sure that there is enough journal space for the handle
 * to begin.  Attach the handle to a transaction and set up the
 * transaction's buffer credits.  
 */

static int start_this_handle(journal_t *journal, handle_t *handle)
{
	transaction_t *transaction;
	int needed;
	int nblocks = handle->h_buffer_credits;

	if (nblocks > journal->j_max_transaction_buffers) {
		jbd_debug(1, "JBD: %s wants too many credits (%d > %d)\n",
		       current->comm, nblocks,
		       journal->j_max_transaction_buffers);
		return -ENOSPC;
	}

	jbd_debug(3, "New handle %p going live.\n", handle);

repeat:

	lock_journal(journal);

repeat_locked:

	if (is_journal_aborted(journal) ||
	    (journal->j_errno != 0 && !(journal->j_flags & JFS_ACK_ERR))) {
		unlock_journal(journal);
		return -EROFS; 
	}

	/* Wait on the journal's transaction barrier if necessary */
	if (journal->j_barrier_count) {
		unlock_journal(journal);
		sleep_on(&journal->j_wait_transaction_locked);
		goto repeat;
	}
	
	if (!journal->j_running_transaction)
		get_transaction(journal, 0);
	/* @@@ Error? */
	J_ASSERT(journal->j_running_transaction);
	
	transaction = journal->j_running_transaction;

	/* If the current transaction is locked down for commit, wait
	 * for the lock to be released. */

	if (transaction->t_state == T_LOCKED) {
		unlock_journal(journal);
		jbd_debug(3, "Handle %p stalling...\n", handle);
		sleep_on(&journal->j_wait_transaction_locked);
		goto repeat;
	}
	
	/* If there is not enough space left in the log to write all
	 * potential buffers requested by this operation, we need to
	 * stall pending a log checkpoint to free some more log
	 * space. */

	needed = transaction->t_outstanding_credits + nblocks;

	if (needed > journal->j_max_transaction_buffers) {
		/* If the current transaction is already too large, then
		 * start to commit it: we can then go back and attach
		 * this handle to a new transaction. */
		
		jbd_debug(2, "Handle %p starting new commit...\n", handle);
		log_start_commit(journal, transaction);
		unlock_journal(journal);
		sleep_on(&journal->j_wait_transaction_locked);
		lock_journal(journal);
		goto repeat_locked;
	}

	/* 
	 * The commit code assumes that it can get enough log space
	 * without forcing a checkpoint.  This is *critical* for
	 * correctness: a checkpoint of a buffer which is also
	 * associated with a committing transaction creates a deadlock,
	 * so commit simply cannot force through checkpoints.
	 *
	 * We must therefore ensure the necessary space in the journal
	 * *before* starting to dirty potentially checkpointed buffers
	 * in the new transaction. 
	 *
	 * The worst part is, any transaction currently committing can
	 * reduce the free space arbitrarily.  Be careful to account for
	 * those buffers when checkpointing.
	 */

	/*
	 * @@@ AKPM: This seems rather over-defensive.  We're giving commit
	 * a _lot_ of headroom: 1/4 of the journal plus the size of
	 * the committing transaction.  Really, we only need to give it
	 * committing_transaction->t_outstanding_credits plus "enough" for
	 * the log control blocks.
	 * Also, this test is inconsitent with the matching one in
	 * journal_extend().
	 */
	needed = journal->j_max_transaction_buffers;
	if (journal->j_committing_transaction) 
		needed += journal->j_committing_transaction->
					t_outstanding_credits;
	
	if (log_space_left(journal) < needed) {
		jbd_debug(2, "Handle %p waiting for checkpoint...\n", handle);
		log_wait_for_space(journal, needed);
		goto repeat_locked;
	}

	/* OK, account for the buffers that this operation expects to
	 * use and add the handle to the running transaction. */

	handle->h_transaction = transaction;
	transaction->t_outstanding_credits += nblocks;
	transaction->t_updates++;
	transaction->t_handle_count++;
	jbd_debug(4, "Handle %p given %d credits (total %d, free %d)\n",
		  handle, nblocks, transaction->t_outstanding_credits,
		  log_space_left(journal));

	unlock_journal(journal);
	
	return 0;
}

/* Allocate a new handle.  This should probably be in a slab... */
static handle_t *new_handle(int nblocks)
{
	handle_t *handle = jbd_kmalloc(sizeof (handle_t), GFP_NOFS);
	if (!handle)
		return NULL;
	memset(handle, 0, sizeof (handle_t));
	handle->h_buffer_credits = nblocks;
	handle->h_ref = 1;
	INIT_LIST_HEAD(&handle->h_jcb);

	return handle;
}

/**
 * handle_t *journal_start() - Obtain a new handle.  
 * @journal: Journal to start transaction on.
 * @nblocks: number of block buffer we might modify
 *
 * We make sure that the transaction can guarantee at least nblocks of
 * modified buffers in the log.  We block until the log can guarantee
 * that much space.  
 *
 * This function is visible to journal users (like ext3fs), so is not
 * called with the journal already locked.
 *
 * Return a pointer to a newly allocated handle, or NULL on failure
 */
handle_t *journal_start(journal_t *journal, int nblocks)
{
	handle_t *handle = journal_current_handle();
	int err;
	
	if (!journal)
		return ERR_PTR(-EROFS);

	if (handle) {
		J_ASSERT(handle->h_transaction->t_journal == journal);
		handle->h_ref++;
		return handle;
	}

	handle = new_handle(nblocks);
	if (!handle)
		return ERR_PTR(-ENOMEM);

	current->journal_info = handle;

	err = start_this_handle(journal, handle);
	if (err < 0) {
		kfree(handle);
		current->journal_info = NULL;
		return ERR_PTR(err);
	}

	return handle;
}

/*
 * Return zero on success
 */
static int try_start_this_handle(journal_t *journal, handle_t *handle)
{
	transaction_t *transaction;
	int needed;
	int nblocks = handle->h_buffer_credits;
	int ret = 0;

	jbd_debug(3, "New handle %p maybe going live.\n", handle);

	lock_journal(journal);

	if (is_journal_aborted(journal) ||
	    (journal->j_errno != 0 && !(journal->j_flags & JFS_ACK_ERR))) {
		ret = -EROFS;
		goto fail_unlock;
	}

	if (journal->j_barrier_count)
		goto fail_unlock;

	if (!journal->j_running_transaction && get_transaction(journal, 1) == 0)
		goto fail_unlock;
	
	transaction = journal->j_running_transaction;
	if (transaction->t_state == T_LOCKED)
		goto fail_unlock;
	
	needed = transaction->t_outstanding_credits + nblocks;
	/* We could run log_start_commit here */
	if (needed > journal->j_max_transaction_buffers)
		goto fail_unlock;

	needed = journal->j_max_transaction_buffers;
	if (journal->j_committing_transaction) 
		needed += journal->j_committing_transaction->
						t_outstanding_credits;
	
	if (log_space_left(journal) < needed)
		goto fail_unlock;

	handle->h_transaction = transaction;
	transaction->t_outstanding_credits += nblocks;
	transaction->t_updates++;
	jbd_debug(4, "Handle %p given %d credits (total %d, free %d)\n",
		  handle, nblocks, transaction->t_outstanding_credits,
		  log_space_left(journal));
	unlock_journal(journal);
	return 0;

fail_unlock:
	unlock_journal(journal);
	if (ret >= 0)
		ret = -1;
	return ret;
}

/**
 * handle_t *journal_try_start() - Don't block, but try and get a handle
 * @journal: Journal to start transaction on.
 * @nblocks: number of block buffer we might modify
 * 
 * Try to start a handle, but non-blockingly.  If we weren't able
 * to, return an ERR_PTR value.
 */
handle_t *journal_try_start(journal_t *journal, int nblocks)
{
	handle_t *handle = journal_current_handle();
	int err;
	
	if (!journal)
		return ERR_PTR(-EROFS);

	if (handle) {
		jbd_debug(4, "h_ref %d -> %d\n",
				handle->h_ref,
				handle->h_ref + 1);
		J_ASSERT(handle->h_transaction->t_journal == journal);
		if (is_handle_aborted(handle))
			return ERR_PTR(-EIO);
		handle->h_ref++;
		return handle;
	} else {
		jbd_debug(4, "no current transaction\n");
	}
	
	if (is_journal_aborted(journal))
		return ERR_PTR(-EIO);

	handle = new_handle(nblocks);
	if (!handle)
		return ERR_PTR(-ENOMEM);

	current->journal_info = handle;

	err = try_start_this_handle(journal, handle);
	if (err < 0) {
		kfree(handle);
		current->journal_info = NULL;
		return ERR_PTR(err);
	}

	return handle;
}

/**
 * int journal_extend() - extend buffer credits.
 * @handle:  handle to 'extend'
 * @nblocks: nr blocks to try to extend by.
 * 
 * Some transactions, such as large extends and truncates, can be done
 * atomically all at once or in several stages.  The operation requests
 * a credit for a number of buffer modications in advance, but can
 * extend its credit if it needs more.  
 *
 * journal_extend tries to give the running handle more buffer credits.
 * It does not guarantee that allocation - this is a best-effort only.
 * The calling process MUST be able to deal cleanly with a failure to
 * extend here.
 *
 * Return 0 on success, non-zero on failure.
 *
 * return code < 0 implies an error
 * return code > 0 implies normal transaction-full status.
 */
int journal_extend (handle_t *handle, int nblocks)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int result;
	int wanted;

	lock_journal (journal);

	result = -EIO;
	if (is_handle_aborted(handle))
		goto error_out;

	result = 1;
	       
	/* Don't extend a locked-down transaction! */
	if (handle->h_transaction->t_state != T_RUNNING) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "transaction not running\n", handle, nblocks);
		goto error_out;
	}
	
	wanted = transaction->t_outstanding_credits + nblocks;
	
	if (wanted > journal->j_max_transaction_buffers) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "transaction too large\n", handle, nblocks);
		goto error_out;
	}

	if (wanted > log_space_left(journal)) {
		jbd_debug(3, "denied handle %p %d blocks: "
			  "insufficient log space\n", handle, nblocks);
		goto error_out;
	}
	
	handle->h_buffer_credits += nblocks;
	transaction->t_outstanding_credits += nblocks;
	result = 0;

	jbd_debug(3, "extended handle %p by %d\n", handle, nblocks);
	
error_out:
	unlock_journal (journal);
	return result;
}


/**
 * int journal_restart() - restart a handle .
 * @handle:  handle to restart
 * @nblocks: nr credits requested
 * 
 * Restart a handle for a multi-transaction filesystem
 * operation.
 *
 * If the journal_extend() call above fails to grant new buffer credits
 * to a running handle, a call to journal_restart will commit the
 * handle's transaction so far and reattach the handle to a new
 * transaction capabable of guaranteeing the requested number of
 * credits.
 */

int journal_restart(handle_t *handle, int nblocks)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int ret;

	/* If we've had an abort of any type, don't even think about
	 * actually doing the restart! */
	if (is_handle_aborted(handle))
		return 0;
	
	/* First unlink the handle from its current transaction, and
	 * start the commit on that. */
	
	J_ASSERT (transaction->t_updates > 0);
	J_ASSERT (journal_current_handle() == handle);

	transaction->t_outstanding_credits -= handle->h_buffer_credits;
	transaction->t_updates--;

	if (!transaction->t_updates)
		wake_up(&journal->j_wait_updates);

	jbd_debug(2, "restarting handle %p\n", handle);
	log_start_commit(journal, transaction);

	handle->h_buffer_credits = nblocks;
	ret = start_this_handle(journal, handle);
	return ret;
}


/**
 * void journal_lock_updates () - establish a transaction barrier.
 * @journal:  Journal to establish a barrier on.
 *
 * This locks out any further updates from being started, and blocks
 * until all existing updates have completed, returning only once the
 * journal is in a quiescent state with no updates running.
 *
 * The journal lock should not be held on entry.
 */
void journal_lock_updates (journal_t *journal)
{
	lock_journal(journal);
	++journal->j_barrier_count;

	/* Wait until there are no running updates */
	while (1) {
		transaction_t *transaction = journal->j_running_transaction;
		if (!transaction)
			break;
		if (!transaction->t_updates)
			break;
		
		unlock_journal(journal);
		sleep_on(&journal->j_wait_updates);
		lock_journal(journal);
	}

	unlock_journal(journal);

	/* We have now established a barrier against other normal
	 * updates, but we also need to barrier against other
	 * journal_lock_updates() calls to make sure that we serialise
	 * special journal-locked operations too. */
	down(&journal->j_barrier);
}

/**
 * void journal_unlock_updates (journal_t* journal) - release barrier
 * @journal:  Journal to release the barrier on.
 * 
 * Release a transaction barrier obtained with journal_lock_updates().
 *
 * Should be called without the journal lock held.
 */
void journal_unlock_updates (journal_t *journal)
{
	lock_journal(journal);

	J_ASSERT (journal->j_barrier_count != 0);
	
	up(&journal->j_barrier);
	--journal->j_barrier_count;
	wake_up(&journal->j_wait_transaction_locked);
	unlock_journal(journal);
}

/*
 * if the buffer is already part of the current transaction, then there
 * is nothing we need to do.  if it is already part of a prior
 * transaction which we are still committing to disk, then we need to
 * make sure that we do not overwrite the old copy: we do copy-out to
 * preserve the copy going to disk.  we also account the buffer against
 * the handle's metadata buffer credits (unless the buffer is already
 * part of the transaction, that is).
 */
static int
do_get_write_access(handle_t *handle, struct journal_head *jh, int force_copy) 
{
	struct buffer_head *bh;
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int error;
	char *frozen_buffer = NULL;
	int need_copy = 0;
	int locked;
	
	jbd_debug(5, "buffer_head %p, force_copy %d\n", jh, force_copy);

	JBUFFER_TRACE(jh, "entry");
repeat:
	bh = jh2bh(jh);

	/* @@@ Need to check for errors here at some point. */

	/*
	 * AKPM: we have replaced all the lock_journal_bh_wait() stuff with a
	 * simple lock_journal().  This code here will care for locked buffers.
	 */
	locked = test_and_set_bit(BH_Lock, &bh->b_state);
	if (locked) {
		/* We can't reliably test the buffer state if we found
		 * it already locked, so just wait for the lock and
		 * retry. */
		unlock_journal(journal);
		__wait_on_buffer(bh);
		lock_journal(journal);
		goto repeat;
	}
	
	/* We now hold the buffer lock so it is safe to query the buffer
	 * state.  Is the buffer dirty? 
	 * 
	 * If so, there are two possibilities.  The buffer may be
	 * non-journaled, and undergoing a quite legitimate writeback.
	 * Otherwise, it is journaled, and we don't expect dirty buffers
	 * in that state (the buffers should be marked JBD_Dirty
	 * instead.)  So either the IO is being done under our own
	 * control and this is a bug, or it's a third party IO such as
	 * dump(8) (which may leave the buffer scheduled for read ---
	 * ie. locked but not dirty) or tune2fs (which may actually have
	 * the buffer dirtied, ugh.)  */

	if (buffer_dirty(bh)) {
		spin_lock(&journal_datalist_lock);
		/* First question: is this buffer already part of the
		 * current transaction or the existing committing
		 * transaction? */
		if (jh->b_transaction) {
			J_ASSERT_JH(jh, jh->b_transaction == transaction || 
				    jh->b_transaction == journal->j_committing_transaction);
			if (jh->b_next_transaction)
				J_ASSERT_JH(jh, jh->b_next_transaction == transaction);
			JBUFFER_TRACE(jh, "Unexpected dirty buffer");
			jbd_unexpected_dirty_buffer(jh);
		}
		spin_unlock(&journal_datalist_lock);
	}

	unlock_buffer(bh);

	error = -EROFS;
	if (is_handle_aborted(handle)) 
		goto out_unlocked;
	error = 0;

	spin_lock(&journal_datalist_lock);

	/* The buffer is already part of this transaction if
	 * b_transaction or b_next_transaction points to it. */

	if (jh->b_transaction == transaction ||
	    jh->b_next_transaction == transaction)
		goto done_locked;

	/* If there is already a copy-out version of this buffer, then
	 * we don't need to make another one. */

	if (jh->b_frozen_data) {
		JBUFFER_TRACE(jh, "has frozen data");
		J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
		jh->b_next_transaction = transaction;

		J_ASSERT_JH(jh, handle->h_buffer_credits > 0);
		handle->h_buffer_credits--;
		goto done_locked;
	}
	
	/* Is there data here we need to preserve? */

	if (jh->b_transaction && jh->b_transaction != transaction) {
		JBUFFER_TRACE(jh, "owned by older transaction");
		J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);

		/* There is one case we have to be very careful about.
		 * If the committing transaction is currently writing
		 * this buffer out to disk and has NOT made a copy-out,
		 * then we cannot modify the buffer contents at all
		 * right now.  The essence of copy-out is that it is the
		 * extra copy, not the primary copy, which gets
		 * journaled.  If the primary copy is already going to
		 * disk then we cannot do copy-out here. */

		if (jh->b_jlist == BJ_Shadow) {
			JBUFFER_TRACE(jh, "on shadow: sleep");
			spin_unlock(&journal_datalist_lock);
			unlock_journal(journal);
			/* commit wakes up all shadow buffers after IO */
			wait_event(jh2bh(jh)->b_wait,
						jh->b_jlist != BJ_Shadow);
			lock_journal(journal);
			goto repeat;
		}
			
		/* Only do the copy if the currently-owning transaction
		 * still needs it.  If it is on the Forget list, the
		 * committing transaction is past that stage.  The
		 * buffer had better remain locked during the kmalloc,
		 * but that should be true --- we hold the journal lock
		 * still and the buffer is already on the BUF_JOURNAL
		 * list so won't be flushed. 
		 *
		 * Subtle point, though: if this is a get_undo_access,
		 * then we will be relying on the frozen_data to contain
		 * the new value of the committed_data record after the
		 * transaction, so we HAVE to force the frozen_data copy
		 * in that case. */

		if (jh->b_jlist != BJ_Forget || force_copy) {
			JBUFFER_TRACE(jh, "generate frozen data");
			if (!frozen_buffer) {
				JBUFFER_TRACE(jh, "allocate memory for buffer");
				spin_unlock(&journal_datalist_lock);
				unlock_journal(journal);
				frozen_buffer = jbd_kmalloc(jh2bh(jh)->b_size,
							    GFP_NOFS);
				lock_journal(journal);
				if (!frozen_buffer) {
					printk(KERN_EMERG
						"%s: OOM for frozen_buffer\n",
						__FUNCTION__);
					JBUFFER_TRACE(jh, "oom!");
					error = -ENOMEM;
					spin_lock(&journal_datalist_lock);
					goto done_locked;
				}
				goto repeat;
			}

			jh->b_frozen_data = frozen_buffer;
			frozen_buffer = NULL;
			need_copy = 1;
		}
		jh->b_next_transaction = transaction;
	}

	J_ASSERT(handle->h_buffer_credits > 0);
	handle->h_buffer_credits--;

	/* Finally, if the buffer is not journaled right now, we need to
	 * make sure it doesn't get written to disk before the caller
	 * actually commits the new data. */

	if (!jh->b_transaction) {
		JBUFFER_TRACE(jh, "no transaction");
		J_ASSERT_JH(jh, !jh->b_next_transaction);
		jh->b_transaction = transaction;
		JBUFFER_TRACE(jh, "file as BJ_Reserved");
		__journal_file_buffer(jh, transaction, BJ_Reserved);
	}
	
done_locked:
	spin_unlock(&journal_datalist_lock);
	if (need_copy) {
		struct page *page;
		int offset;
		char *source;

		J_EXPECT_JH(jh, buffer_uptodate(jh2bh(jh)),
			    "Possible IO failure.\n");
		page = jh2bh(jh)->b_page;
		offset = ((unsigned long) jh2bh(jh)->b_data) & ~PAGE_MASK;
		source = kmap(page);
		memcpy(jh->b_frozen_data, source+offset, jh2bh(jh)->b_size);
		kunmap(page);
	}
	

	/* If we are about to journal a buffer, then any revoke pending
           on it is no longer valid. */
	journal_cancel_revoke(handle, jh);

out_unlocked:
	if (frozen_buffer)
		kfree(frozen_buffer);

	JBUFFER_TRACE(jh, "exit");
	return error;
}

/**
 * int journal_get_write_access() - notify intent to modify a buffer for metadata (not data) update.
 * @handle: transaction to add buffer modifications to
 * @bh:     bh to be used for metadata writes
 *
 * Returns an error code or 0 on success.
 *
 * In full data journalling mode the buffer may be of type BJ_AsyncData,
 * because we're write()ing a buffer which is also part of a shared mapping.
 */

int journal_get_write_access (handle_t *handle, struct buffer_head *bh) 
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = journal_add_journal_head(bh);
	int rc;

	/* We do not want to get caught playing with fields which the
	 * log thread also manipulates.  Make sure that the buffer
	 * completes any outstanding IO before proceeding. */
	lock_journal(journal);
	rc = do_get_write_access(handle, jh, 0);
	journal_unlock_journal_head(jh);
	unlock_journal(journal);
	return rc;
}


/*
 * When the user wants to journal a newly created buffer_head
 * (ie. getblk() returned a new buffer and we are going to populate it
 * manually rather than reading off disk), then we need to keep the
 * buffer_head locked until it has been completely filled with new
 * data.  In this case, we should be able to make the assertion that
 * the bh is not already part of an existing transaction.  
 * 
 * The buffer should already be locked by the caller by this point.
 * There is no lock ranking violation: it was a newly created,
 * unlocked buffer beforehand. */

/**
 * int journal_get_create_access () - notify intent to use newly created bh
 * @handle: ransaction to new buffer to
 * @bh: new buffer.
 *
 * Call this if you create a new bh.
 */
int journal_get_create_access (handle_t *handle, struct buffer_head *bh) 
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = journal_add_journal_head(bh);
	int err;
	
	jbd_debug(5, "journal_head %p\n", jh);
	lock_journal(journal);
	err = -EROFS;
	if (is_handle_aborted(handle))
		goto out;
	err = 0;
	
	JBUFFER_TRACE(jh, "entry");
	/* The buffer may already belong to this transaction due to
	 * pre-zeroing in the filesystem's new_block code.  It may also
	 * be on the previous, committing transaction's lists, but it
	 * HAS to be in Forget state in that case: the transaction must
	 * have deleted the buffer for it to be reused here. */
	J_ASSERT_JH(jh, (jh->b_transaction == transaction ||
			 jh->b_transaction == NULL ||
			 (jh->b_transaction == journal->j_committing_transaction &&
			  jh->b_jlist == BJ_Forget)));

	J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
	J_ASSERT_JH(jh, buffer_locked(jh2bh(jh)));

	J_ASSERT_JH(jh, handle->h_buffer_credits > 0);
	handle->h_buffer_credits--;

	spin_lock(&journal_datalist_lock);
	if (jh->b_transaction == NULL) {
		jh->b_transaction = transaction;
		JBUFFER_TRACE(jh, "file as BJ_Reserved");
		__journal_file_buffer(jh, transaction, BJ_Reserved);
		JBUFFER_TRACE(jh, "refile");
		refile_buffer(jh2bh(jh));
	} else if (jh->b_transaction == journal->j_committing_transaction) {
		JBUFFER_TRACE(jh, "set next transaction");
		jh->b_next_transaction = transaction;
	}
	spin_unlock(&journal_datalist_lock);

	/*
	 * akpm: I added this.  ext3_alloc_branch can pick up new indirect
	 * blocks which contain freed but then revoked metadata.  We need
	 * to cancel the revoke in case we end up freeing it yet again
	 * and the reallocating as data - this would cause a second revoke,
	 * which hits an assertion error.
	 */
	JBUFFER_TRACE(jh, "cancelling revoke");
	journal_cancel_revoke(handle, jh);
	journal_unlock_journal_head(jh);
out:
	unlock_journal(journal);
	return err;
}



/**
 * int journal_get_undo_access() -  Notify intent to modify metadata with non-rewindable consequences
 * @handle: transaction
 * @bh: buffer to undo
 * 
 * Sometimes there is a need to distinguish between metadata which has
 * been committed to disk and that which has not.  The ext3fs code uses
 * this for freeing and allocating space, we have to make sure that we
 * do not reuse freed space until the deallocation has been committed,
 * since if we overwrote that space we would make the delete
 * un-rewindable in case of a crash.
 * 
 * To deal with that, journal_get_undo_access requests write access to a
 * buffer for parts of non-rewindable operations such as delete
 * operations on the bitmaps.  The journaling code must keep a copy of
 * the buffer's contents prior to the undo_access call until such time
 * as we know that the buffer has definitely been committed to disk.
 * 
 * We never need to know which transaction the committed data is part
 * of, buffers touched here are guaranteed to be dirtied later and so
 * will be committed to a new transaction in due course, at which point
 * we can discard the old committed data pointer.
 *
 * Returns error number or 0 on success.  
 */
int journal_get_undo_access (handle_t *handle, struct buffer_head *bh)
{
	journal_t *journal = handle->h_transaction->t_journal;
	int err;
	struct journal_head *jh = journal_add_journal_head(bh);

	JBUFFER_TRACE(jh, "entry");
	lock_journal(journal);

	/* Do this first --- it can drop the journal lock, so we want to
	 * make sure that obtaining the committed_data is done
	 * atomically wrt. completion of any outstanding commits. */
	err = do_get_write_access (handle, jh, 1);
	if (err)
		goto out;
	
	if (!jh->b_committed_data) {
		/* Copy out the current buffer contents into the
		 * preserved, committed copy. */
		JBUFFER_TRACE(jh, "generate b_committed data");
		jh->b_committed_data = jbd_kmalloc(jh2bh(jh)->b_size, 
						   GFP_NOFS);
		if (!jh->b_committed_data) {
			printk(KERN_EMERG "%s: No memory for committed data!\n",
				__FUNCTION__);
			err = -ENOMEM;
			goto out;
		}
		
		memcpy (jh->b_committed_data, jh2bh(jh)->b_data,
				jh2bh(jh)->b_size);
	}

out:
	if (!err)
		J_ASSERT_JH(jh, jh->b_committed_data);
	journal_unlock_journal_head(jh);
	unlock_journal(journal);
	return err;
}

/** 
 * int journal_dirty_data() -  mark a buffer as containing dirty data which needs to be flushed before we can commit the current transaction.  
 * @handle: transaction
 * @bh: bufferhead to mark
 * @async: flag
 * 
 * The buffer is placed on the transaction's data list and is marked as
 * belonging to the transaction.
 *
 * If `async' is set then the writebask will be initiated by the caller
 * using submit_bh -> end_buffer_io_async.  We put the buffer onto
 * t_async_datalist.
 * 
 * Returns error number or 0 on success.  
 */
int journal_dirty_data (handle_t *handle, struct buffer_head *bh, int async)
{
/*
 * journal_dirty_data() can be called via page_launder->ext3_writepage
 * by kswapd.  So it cannot block.  Happily, there's nothing here
 * which needs lock_journal if `async' is set.
 *
 * When the buffer is on the current transaction we freely move it
 * between BJ_AsyncData and BJ_SyncData according to who tried to
 * change its state last.
 */
	journal_t *journal = handle->h_transaction->t_journal;
	int need_brelse = 0;
	int wanted_jlist = async ? BJ_AsyncData : BJ_SyncData;
	struct journal_head *jh;

	if (is_handle_aborted(handle))
		return 0;
	
	jh = journal_add_journal_head(bh);
	JBUFFER_TRACE(jh, "entry");

	/*
	 * The buffer could *already* be dirty.  Writeout can start
	 * at any time.
	 */
	jbd_debug(4, "jh: %p, tid:%d\n", jh, handle->h_transaction->t_tid);

	/*
	 * What if the buffer is already part of a running transaction?
	 * 
	 * There are two cases:
	 * 1) It is part of the current running transaction.  Refile it,
	 *    just in case we have allocated it as metadata, deallocated
	 *    it, then reallocated it as data. 
	 * 2) It is part of the previous, still-committing transaction.
	 *    If all we want to do is to guarantee that the buffer will be
	 *    written to disk before this new transaction commits, then
	 *    being sure that the *previous* transaction has this same 
	 *    property is sufficient for us!  Just leave it on its old
	 *    transaction.
	 *
	 * In case (2), the buffer must not already exist as metadata
	 * --- that would violate write ordering (a transaction is free
	 * to write its data at any point, even before the previous
	 * committing transaction has committed).  The caller must
	 * never, ever allow this to happen: there's nothing we can do
	 * about it in this layer.
	 */
	spin_lock(&journal_datalist_lock);
	if (jh->b_transaction) {
		JBUFFER_TRACE(jh, "has transaction");
		if (jh->b_transaction != handle->h_transaction) {
			JBUFFER_TRACE(jh, "belongs to older transaction");
			J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);

			/* @@@ IS THIS TRUE  ? */
			/*
			 * Not any more.  Scenario: someone does a write()
			 * in data=journal mode.  The buffer's transaction has
			 * moved into commit.  Then someone does another
			 * write() to the file.  We do the frozen data copyout
			 * and set b_next_transaction to point to j_running_t.
			 * And while we're in that state, someone does a
			 * writepage() in an attempt to pageout the same area
			 * of the file via a shared mapping.  At present that
			 * calls journal_dirty_data(), and we get right here.
			 * It may be too late to journal the data.  Simply
			 * falling through to the next test will suffice: the
			 * data will be dirty and wil be checkpointed.  The
			 * ordering comments in the next comment block still
			 * apply.
			 */
			//J_ASSERT_JH(jh, jh->b_next_transaction == NULL);

			/*
			 * If we're journalling data, and this buffer was
			 * subject to a write(), it could be metadata, forget
			 * or shadow against the committing transaction.  Now,
			 * someone has dirtied the same darn page via a mapping
			 * and it is being writepage()'d.
			 * We *could* just steal the page from commit, with some
			 * fancy locking there.  Instead, we just skip it -
			 * don't tie the page's buffers to the new transaction
			 * at all.
			 * Implication: if we crash before the writepage() data
			 * is written into the filesystem, recovery will replay
			 * the write() data.
			 */
			if (jh->b_jlist != BJ_None &&
					jh->b_jlist != BJ_SyncData &&
					jh->b_jlist != BJ_AsyncData) {
				JBUFFER_TRACE(jh, "Not stealing");
				goto no_journal;
			}

			/*
			 * This buffer may be undergoing writeout in commit.  We
			 * can't return from here and let the caller dirty it
			 * again because that can cause the write-out loop in
			 * commit to never terminate.
			 */
			if (!async && buffer_dirty(bh)) {
				atomic_inc(&bh->b_count);
				spin_unlock(&journal_datalist_lock);
				need_brelse = 1;
				ll_rw_block(WRITE, 1, &bh);
				wait_on_buffer(bh);
				spin_lock(&journal_datalist_lock);
				/* The buffer may become locked again at any
				   time if it is redirtied */
			}

			/* journal_clean_data_list() may have got there first */
			if (jh->b_transaction != NULL) {
				JBUFFER_TRACE(jh, "unfile from commit");
				__journal_unfile_buffer(jh);
				jh->b_transaction = NULL;
			}
			/* The buffer will be refiled below */

		}
		/*
		 * Special case --- the buffer might actually have been
		 * allocated and then immediately deallocated in the previous,
		 * committing transaction, so might still be left on that
		 * transaction's metadata lists.
		 */
		if (jh->b_jlist != wanted_jlist) {
			JBUFFER_TRACE(jh, "not on correct data list: unfile");
			J_ASSERT_JH(jh, jh->b_jlist != BJ_Shadow);
			__journal_unfile_buffer(jh);
			jh->b_transaction = NULL;
			JBUFFER_TRACE(jh, "file as data");
			__journal_file_buffer(jh, handle->h_transaction,
						wanted_jlist);
		}
	} else {
		JBUFFER_TRACE(jh, "not on a transaction");
		__journal_file_buffer(jh, handle->h_transaction, wanted_jlist);
	}
no_journal:
	spin_unlock(&journal_datalist_lock);
	if (need_brelse) {
		BUFFER_TRACE(bh, "brelse");
		__brelse(bh);
	}
	JBUFFER_TRACE(jh, "exit");
	journal_unlock_journal_head(jh);
	return 0;
}

/** 
 * int journal_dirty_metadata() -  mark a buffer as containing dirty metadata
 * @handle: transaction to add buffer to.
 * @bh: buffer to mark 
 * 
 * mark dirty metadata which needs to be journaled as part of the current transaction.
 *
 * The buffer is placed on the transaction's metadata list and is marked
 * as belonging to the transaction.  
 *
 * Returns error number or 0 on success.  
 */
int journal_dirty_metadata (handle_t *handle, struct buffer_head *bh)
{
/*
 * Special care needs to be taken if the buffer already belongs to the
 * current committing transaction (in which case we should have frozen
 * data present for that commit).  In that case, we don't relink the
 * buffer: that only gets done when the old transaction finally
 * completes its commit.
 * 
 */
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = bh2jh(bh);

	jbd_debug(5, "journal_head %p\n", jh);
	JBUFFER_TRACE(jh, "entry");
	lock_journal(journal);
	if (is_handle_aborted(handle))
		goto out_unlock;
	
	spin_lock(&journal_datalist_lock);
	set_bit(BH_JBDDirty, &bh->b_state);

	J_ASSERT_JH(jh, jh->b_transaction != NULL);
	
	/* 
	 * Metadata already on the current transaction list doesn't
	 * need to be filed.  Metadata on another transaction's list must
	 * be committing, and will be refiled once the commit completes:
	 * leave it alone for now. 
	 */

	if (jh->b_transaction != transaction) {
		JBUFFER_TRACE(jh, "already on other transaction");
		J_ASSERT_JH(jh, jh->b_transaction ==
					journal->j_committing_transaction);
		J_ASSERT_JH(jh, jh->b_next_transaction == transaction);
		/* And this case is illegal: we can't reuse another
		 * transaction's data buffer, ever. */
		/* FIXME: writepage() should be journalled */
		J_ASSERT_JH(jh, jh->b_jlist != BJ_SyncData);
		goto done_locked;
	}

	/* That test should have eliminated the following case: */
	J_ASSERT_JH(jh, jh->b_frozen_data == 0);

	JBUFFER_TRACE(jh, "file as BJ_Metadata");
	__journal_file_buffer(jh, handle->h_transaction, BJ_Metadata);

done_locked:
	spin_unlock(&journal_datalist_lock);
	JBUFFER_TRACE(jh, "exit");
out_unlock:
	unlock_journal(journal);
	return 0;
}

#if 0
/* 
 * journal_release_buffer: undo a get_write_access without any buffer
 * updates, if the update decided in the end that it didn't need access.
 *
 * journal_get_write_access() can block, so it is quite possible for a
 * journaling component to decide after the write access is returned
 * that global state has changed and the update is no longer required.  */

void journal_release_buffer (handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh = bh2jh(bh);

	lock_journal(journal);
	JBUFFER_TRACE(jh, "entry");

	/* If the buffer is reserved but not modified by this
	 * transaction, then it is safe to release it.  In all other
	 * cases, just leave the buffer as it is. */

	spin_lock(&journal_datalist_lock);
	if (jh->b_jlist == BJ_Reserved && jh->b_transaction == transaction &&
	    !buffer_jdirty(jh2bh(jh))) {
		JBUFFER_TRACE(jh, "unused: refiling it");
		handle->h_buffer_credits++;
		__journal_refile_buffer(jh);
	}
	spin_unlock(&journal_datalist_lock);

	JBUFFER_TRACE(jh, "exit");
	unlock_journal(journal);
}
#endif

/** 
 * void journal_forget() - bforget() for potentially-journaled buffers.
 * @handle: transaction handle
 * @bh:     bh to 'forget'
 *
 * We can only do the bforget if there are no commits pending against the
 * buffer.  If the buffer is dirty in the current running transaction we
 * can safely unlink it. 
 *
 * bh may not be a journalled buffer at all - it may be a non-JBD
 * buffer which came off the hashtable.  Check for this.
 *
 * Decrements bh->b_count by one.
 * 
 * Allow this call even if the handle has aborted --- it may be part of
 * the caller's cleanup after an abort.
 */
void journal_forget (handle_t *handle, struct buffer_head *bh)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	struct journal_head *jh;

	BUFFER_TRACE(bh, "entry");

	lock_journal(journal);
	spin_lock(&journal_datalist_lock);

	if (!buffer_jbd(bh))
		goto not_jbd;
	jh = bh2jh(bh);

	if (jh->b_transaction == handle->h_transaction) {
		J_ASSERT_JH(jh, !jh->b_frozen_data);

		/* If we are forgetting a buffer which is already part
		 * of this transaction, then we can just drop it from
		 * the transaction immediately. */
		clear_bit(BH_Dirty, &bh->b_state);
		clear_bit(BH_JBDDirty, &bh->b_state);

		JBUFFER_TRACE(jh, "belongs to current transaction: unfile");
		J_ASSERT_JH(jh, !jh->b_committed_data);

		__journal_unfile_buffer(jh);
		jh->b_transaction = 0;

		/* 
		 * We are no longer going to journal this buffer.
		 * However, the commit of this transaction is still
		 * important to the buffer: the delete that we are now
		 * processing might obsolete an old log entry, so by
		 * committing, we can satisfy the buffer's checkpoint.
		 *
		 * So, if we have a checkpoint on the buffer, we should
		 * now refile the buffer on our BJ_Forget list so that
		 * we know to remove the checkpoint after we commit. 
		 */

		if (jh->b_cp_transaction) {
			__journal_file_buffer(jh, transaction, BJ_Forget);
		} else {
			__journal_remove_journal_head(bh);
			__brelse(bh);
			if (!buffer_jbd(bh)) {
				spin_unlock(&journal_datalist_lock);
				unlock_journal(journal);
				__bforget(bh);
				return;
			}
		}
		
	} else if (jh->b_transaction) {
		J_ASSERT_JH(jh, (jh->b_transaction == 
				 journal->j_committing_transaction));
		/* However, if the buffer is still owned by a prior
		 * (committing) transaction, we can't drop it yet... */
		JBUFFER_TRACE(jh, "belongs to older transaction");
		/* ... but we CAN drop it from the new transaction if we
		 * have also modified it since the original commit. */

		if (jh->b_next_transaction) {
			J_ASSERT(jh->b_next_transaction == transaction);
			jh->b_next_transaction = NULL;
		}
	}

not_jbd:
	spin_unlock(&journal_datalist_lock);
	unlock_journal(journal);
	__brelse(bh);
	return;
}

#if 0	/* Unused */
/*
 * journal_sync_buffer: flush a potentially-journaled buffer to disk.
 *
 * Used for O_SYNC filesystem operations.  If the buffer is journaled,
 * we need to complete the O_SYNC by waiting for the transaction to
 * complete.  It is an error to call journal_sync_buffer before
 * journal_stop!
 */

void journal_sync_buffer(struct buffer_head *bh)
{
	transaction_t *transaction;
	journal_t *journal;
	long sequence;
	struct journal_head *jh;

	/* If the buffer isn't journaled, this is easy: just sync it to
	 * disk.  */
	BUFFER_TRACE(bh, "entry");

	spin_lock(&journal_datalist_lock);
	if (!buffer_jbd(bh)) {
		spin_unlock(&journal_datalist_lock);
		return;
	}
	jh = bh2jh(bh);
	if (jh->b_transaction == NULL) {
		/* If the buffer has already been journaled, then this
		 * is a noop. */
		if (jh->b_cp_transaction == NULL) {
			spin_unlock(&journal_datalist_lock);
			return;
		}
		atomic_inc(&bh->b_count);
		spin_unlock(&journal_datalist_lock);
		ll_rw_block (WRITE, 1, &bh);
		wait_on_buffer(bh);
		__brelse(bh);
		goto out;
	}
	
	/* Otherwise, just wait until the transaction is synced to disk. */
	transaction = jh->b_transaction;
	journal = transaction->t_journal;
	sequence = transaction->t_tid;
	spin_unlock(&journal_datalist_lock);

	jbd_debug(2, "requesting commit for jh %p\n", jh);
	log_start_commit (journal, transaction);
	
	while (tid_gt(sequence, journal->j_commit_sequence)) {
		wake_up(&journal->j_wait_done_commit);
		sleep_on(&journal->j_wait_done_commit);
	}
	JBUFFER_TRACE(jh, "exit");
out:
	return;
}
#endif

/*
 * Register a callback function for this handle.  The function will be
 * called when the transaction that this handle is part of has been
 * committed to disk with the original callback data struct and the
 * error status of the journal as parameters.  There is no guarantee of
 * ordering between handles within a single transaction, nor between
 * callbacks registered on the same handle.
 *
 * The caller is responsible for allocating the journal_callback struct.
 * This is to allow the caller to add as much extra data to the callback
 * as needed, but reduce the overhead of multiple allocations.  The caller
 * allocated struct must start with a struct journal_callback at offset 0,
 * and has the caller-specific data afterwards.
 */
void journal_callback_set(handle_t *handle,
			  void (*func)(struct journal_callback *jcb, int error),
			  struct journal_callback *jcb)
{
	list_add_tail(&jcb->jcb_list, &handle->h_jcb);
	jcb->jcb_func = func;
}

/**
 * int journal_stop() - complete a transaction
 * @handle: tranaction to complete.
 * 
 * All done for a particular handle.
 *
 * There is not much action needed here.  We just return any remaining
 * buffer credits to the transaction and remove the handle.  The only
 * complication is that we need to start a commit operation if the
 * filesystem is marked for synchronous update.
 *
 * journal_stop itself will not usually return an error, but it may
 * do so in unusual circumstances.  In particular, expect it to 
 * return -EIO if a journal_abort has been executed since the
 * transaction began.
 */
int journal_stop(handle_t *handle)
{
	transaction_t *transaction = handle->h_transaction;
	journal_t *journal = transaction->t_journal;
	int old_handle_count, err;
	
	if (!handle)
		return 0;

	J_ASSERT (transaction->t_updates > 0);
	J_ASSERT (journal_current_handle() == handle);
	
	if (is_handle_aborted(handle))
		err = -EIO;
	else
		err = 0;
	
	if (--handle->h_ref > 0) {
		jbd_debug(4, "h_ref %d -> %d\n", handle->h_ref + 1,
			  handle->h_ref);
		return err;
	}

	jbd_debug(4, "Handle %p going down\n", handle);

	/*
	 * Implement synchronous transaction batching.  If the handle
	 * was synchronous, don't force a commit immediately.  Let's
	 * yield and let another thread piggyback onto this transaction.
	 * Keep doing that while new threads continue to arrive.
	 * It doesn't cost much - we're about to run a commit and sleep
	 * on IO anyway.  Speeds up many-threaded, many-dir operations
	 * by 30x or more...
	 */
	if (handle->h_sync) {
		do {
			old_handle_count = transaction->t_handle_count;
			yield();
		} while (old_handle_count != transaction->t_handle_count);
	}

	current->journal_info = NULL;
	transaction->t_outstanding_credits -= handle->h_buffer_credits;
	transaction->t_updates--;
	if (!transaction->t_updates) {
		wake_up(&journal->j_wait_updates);
		if (journal->j_barrier_count)
			wake_up(&journal->j_wait_transaction_locked);
	}

	/* Move callbacks from the handle to the transaction. */
	list_splice(&handle->h_jcb, &transaction->t_jcb);

	/*
	 * If the handle is marked SYNC, we need to set another commit
	 * going!  We also want to force a commit if the current
	 * transaction is occupying too much of the log, or if the
	 * transaction is too old now.
	 */
	if (handle->h_sync ||
			transaction->t_outstanding_credits >
				journal->j_max_transaction_buffers ||
	    		(journal->j_commit_interval &&
	    		 time_after_eq(jiffies, transaction->t_expires))) {
		/* Do this even for aborted journals: an abort still
		 * completes the commit thread, it just doesn't write
		 * anything to disk. */
		tid_t tid = transaction->t_tid;
		
		jbd_debug(2, "transaction too old, requesting commit for "
					"handle %p\n", handle);
		/* This is non-blocking */
		log_start_commit(journal, transaction);
		
		/*
		 * Special case: JFS_SYNC synchronous updates require us
		 * to wait for the commit to complete.  
		 */
		if (handle->h_sync && !(current->flags & PF_MEMALLOC))
			log_wait_commit(journal, tid);
	}
	kfree(handle);
	return err;
}

/**int journal_force_commit() - force any uncommitted transactions
 * @journal: journal to force
 *
 * For synchronous operations: force any uncommitted transactions
 * to disk.  May seem kludgy, but it reuses all the handle batching
 * code in a very simple manner.
 */
int journal_force_commit(journal_t *journal)
{
	handle_t *handle;
	int ret = 0;

	lock_kernel();
	handle = journal_start(journal, 1);
	if (IS_ERR(handle)) {
		ret = PTR_ERR(handle);
		goto out;
	}
	handle->h_sync = 1;
	journal_stop(handle);
out:
	unlock_kernel();
	return ret;
}

/*
 *
 * List management code snippets: various functions for manipulating the
 * transaction buffer lists.
 *
 */

/*
 * Append a buffer to a transaction list, given the transaction's list head
 * pointer.
 * journal_datalist_lock is held.
 */

static inline void 
__blist_add_buffer(struct journal_head **list, struct journal_head *jh)
{
	if (!*list) {
		jh->b_tnext = jh->b_tprev = jh;
		*list = jh;
	} else {
		/* Insert at the tail of the list to preserve order */
		struct journal_head *first = *list, *last = first->b_tprev;
		jh->b_tprev = last;
		jh->b_tnext = first;
		last->b_tnext = first->b_tprev = jh;
	}
}

/* 
 * Remove a buffer from a transaction list, given the transaction's list
 * head pointer.
 *
 * Called with journal_datalist_lock held, and the journal may not
 * be locked.
 */

static inline void
__blist_del_buffer(struct journal_head **list, struct journal_head *jh)
{
	if (*list == jh) {
		*list = jh->b_tnext;
		if (*list == jh)
			*list = 0;
	}
	jh->b_tprev->b_tnext = jh->b_tnext;
	jh->b_tnext->b_tprev = jh->b_tprev;
}

/* 
 * Remove a buffer from the appropriate transaction list.
 *
 * Note that this function can *change* the value of
 * bh->b_transaction->t_sync_datalist, t_async_datalist, t_buffers, t_forget,
 * t_iobuf_list, t_shadow_list, t_log_list or t_reserved_list.  If the caller
 * is holding onto a copy of one of thee pointers, it could go bad.
 * Generally the caller needs to re-read the pointer from the transaction_t.
 *
 * If bh->b_jlist is BJ_SyncData or BJ_AsyncData then we may have been called
 * via journal_try_to_free_buffer() or journal_clean_data_list().  In that
 * case, journal_datalist_lock will be held, and the journal may not be locked.
 */
void __journal_unfile_buffer(struct journal_head *jh)
{
	struct journal_head **list = 0;
	transaction_t * transaction;

	assert_spin_locked(&journal_datalist_lock);
	transaction = jh->b_transaction;

	J_ASSERT_JH(jh, jh->b_jlist < BJ_Types);

	if (jh->b_jlist != BJ_None)
		J_ASSERT_JH(jh, transaction != 0);

	switch (jh->b_jlist) {
	case BJ_None:
		return;
	case BJ_SyncData:
		list = &transaction->t_sync_datalist;
		break;
	case BJ_AsyncData:
		list = &transaction->t_async_datalist;
		break;
	case BJ_Metadata:
		transaction->t_nr_buffers--;
		J_ASSERT_JH(jh, transaction->t_nr_buffers >= 0);
		list = &transaction->t_buffers;
		break;
	case BJ_Forget:
		list = &transaction->t_forget;
		break;
	case BJ_IO:
		list = &transaction->t_iobuf_list;
		break;
	case BJ_Shadow:
		list = &transaction->t_shadow_list;
		break;
	case BJ_LogCtl:
		list = &transaction->t_log_list;
		break;
	case BJ_Reserved:
		list = &transaction->t_reserved_list;
		break;
	}
	
	__blist_del_buffer(list, jh);
	jh->b_jlist = BJ_None;
	if (test_and_clear_bit(BH_JBDDirty, &jh2bh(jh)->b_state)) {
		set_bit(BH_Dirty, &jh2bh(jh)->b_state);
	}
}

void journal_unfile_buffer(struct journal_head *jh)
{
	spin_lock(&journal_datalist_lock);
	__journal_unfile_buffer(jh);
	spin_unlock(&journal_datalist_lock);
}

/*
 * Called from journal_try_to_free_buffers().  The journal is not
 * locked. lru_list_lock is not held.
 *
 * Here we see why journal_datalist_lock is global and not per-journal.
 * We cannot get back to this buffer's journal pointer without locking
 * out journal_clean_data_list() in some manner.
 *
 * One could use journal_datalist_lock to get unracy access to a
 * per-journal lock.
 *
 * Called with journal_datalist_lock held.
 *
 * Returns non-zero iff we were able to free the journal_head.
 */
static int __journal_try_to_free_buffer(struct buffer_head *bh,
					int *locked_or_dirty)
{
	struct journal_head *jh;

	assert_spin_locked(&journal_datalist_lock);

	jh = bh2jh(bh);

	if (buffer_locked(bh) || buffer_dirty(bh)) {
		*locked_or_dirty = 1;
		goto out;
	}

	if (!buffer_uptodate(bh))
		goto out;

	if (jh->b_next_transaction != 0)
		goto out;

	if (jh->b_transaction != 0 && jh->b_cp_transaction == 0) {
		if (jh->b_jlist == BJ_SyncData || jh->b_jlist==BJ_AsyncData) {
			/* A written-back ordered data buffer */
			JBUFFER_TRACE(jh, "release data");
			__journal_unfile_buffer(jh);
			jh->b_transaction = 0;
			__journal_remove_journal_head(bh);
			__brelse(bh);
		}
	}
	else if (jh->b_cp_transaction != 0 && jh->b_transaction == 0) {
		/* written-back checkpointed metadata buffer */
		if (jh->b_jlist == BJ_None) {
			JBUFFER_TRACE(jh, "remove from checkpoint list");
			__journal_remove_checkpoint(jh);
			__journal_remove_journal_head(bh);
			__brelse(bh);
		}
	}
	return !buffer_jbd(bh);

out:
	return 0;
}


/** 
 * int journal_try_to_free_buffers() - try to free page buffers.
 * @journal: journal for operation
 * @page: to try and free
 * @gfp_mask: 'IO' mode for try_to_free_buffers()
 *
 * 
 * For all the buffers on this page,
 * if they are fully written out ordered data, move them onto BUF_CLEAN
 * so try_to_free_buffers() can reap them.
 * 
 * This function returns non-zero if we wish try_to_free_buffers()
 * to be called. We do this if the page is releasable by try_to_free_buffers().
 * We also do it if the page has locked or dirty buffers and the caller wants
 * us to perform sync or async writeout.
 */
int journal_try_to_free_buffers(journal_t *journal, 
				struct page *page, int gfp_mask)
{
/*
 * journal_try_to_free_buffers().  For all the buffers on this page,
 * if they are fully written out ordered data, move them onto BUF_CLEAN
 * so try_to_free_buffers() can reap them.  Called with lru_list_lock
 * not held.  Does its own locking.
 *
 * This complicates JBD locking somewhat.  We aren't protected by the
 * BKL here.  We wish to remove the buffer from its committing or
 * running transaction's ->t_datalist via __journal_unfile_buffer.
 *
 * This may *change* the value of transaction_t->t_datalist, so anyone
 * who looks at t_datalist needs to lock against this function.
 *
 * Even worse, someone may be doing a journal_dirty_data on this
 * buffer.  So we need to lock against that.  journal_dirty_data()
 * will come out of the lock with the buffer dirty, which makes it
 * ineligible for release here.
 *
 * Who else is affected by this?  hmm...  Really the only contender
 * is do_get_write_access() - it could be looking at the buffer while
 * journal_try_to_free_buffer() is changing its state.  But that
 * cannot happen because we never reallocate freed data as metadata
 * while the data is part of a transaction.  Yes?
 *
 */
	struct buffer_head *bh;
	struct buffer_head *tmp;
	int locked_or_dirty = 0;
	int call_ttfb = 1;

	J_ASSERT(PageLocked(page));

	bh = page->buffers;
	tmp = bh;
	spin_lock(&journal_datalist_lock);
	do {
		struct buffer_head *p = tmp;

		tmp = tmp->b_this_page;
		if (buffer_jbd(p))
			if (!__journal_try_to_free_buffer(p, &locked_or_dirty))
				call_ttfb = 0;
	} while (tmp != bh);
	spin_unlock(&journal_datalist_lock);

	if (!(gfp_mask & (__GFP_IO|__GFP_WAIT)))
		goto out;
	if (!locked_or_dirty)
		goto out;
	/*
	 * The VM wants us to do writeout, or to block on IO, or both.
	 * So we allow try_to_free_buffers to be called even if the page
	 * still has journalled buffers.
	 */
	call_ttfb = 1;
out:
	return call_ttfb;
}

/*
 * This buffer is no longer needed.  If it is on an older transaction's
 * checkpoint list we need to record it on this transaction's forget list
 * to pin this buffer (and hence its checkpointing transaction) down until
 * this transaction commits.  If the buffer isn't on a checkpoint list, we
 * release it.
 * Returns non-zero if JBD no longer has an interest in the buffer.
 */
static int dispose_buffer(struct journal_head *jh,
		transaction_t *transaction)
{
	int may_free = 1;
	struct buffer_head *bh = jh2bh(jh);

	spin_lock(&journal_datalist_lock);
	__journal_unfile_buffer(jh);
	jh->b_transaction = 0;

	if (jh->b_cp_transaction) {
		JBUFFER_TRACE(jh, "on running+cp transaction");
		__journal_file_buffer(jh, transaction, BJ_Forget);
		clear_bit(BH_JBDDirty, &bh->b_state);
		may_free = 0;
	} else {
		JBUFFER_TRACE(jh, "on running transaction");
		__journal_remove_journal_head(bh);
		__brelse(bh);
	}
	spin_unlock(&journal_datalist_lock);
	return may_free;
}

/*
 * journal_flushpage 
 *
 * This code is tricky.  It has a number of cases to deal with.
 *
 * There are two invariants which this code relies on:
 *
 * i_size must be updated on disk before we start calling flushpage on the
 * data.
 * 
 *  This is done in ext3 by defining an ext3_setattr method which
 *  updates i_size before truncate gets going.  By maintaining this
 *  invariant, we can be sure that it is safe to throw away any buffers
 *  attached to the current transaction: once the transaction commits,
 *  we know that the data will not be needed.
 * 
 *  Note however that we can *not* throw away data belonging to the
 *  previous, committing transaction!  
 *
 * Any disk blocks which *are* part of the previous, committing
 * transaction (and which therefore cannot be discarded immediately) are
 * not going to be reused in the new running transaction
 *
 *  The bitmap committed_data images guarantee this: any block which is
 *  allocated in one transaction and removed in the next will be marked
 *  as in-use in the committed_data bitmap, so cannot be reused until
 *  the next transaction to delete the block commits.  This means that
 *  leaving committing buffers dirty is quite safe: the disk blocks
 *  cannot be reallocated to a different file and so buffer aliasing is
 *  not possible.
 *
 *
 * The above applies mainly to ordered data mode.  In writeback mode we
 * don't make guarantees about the order in which data hits disk --- in
 * particular we don't guarantee that new dirty data is flushed before
 * transaction commit --- so it is always safe just to discard data
 * immediately in that mode.  --sct 
 */

/*
 * The journal_unmap_buffer helper function returns zero if the buffer
 * concerned remains pinned as an anonymous buffer belonging to an older
 * transaction.
 *
 * We're outside-transaction here.  Either or both of j_running_transaction
 * and j_committing_transaction may be NULL.
 */
static int journal_unmap_buffer(journal_t *journal, struct buffer_head *bh)
{
	transaction_t *transaction;
	struct journal_head *jh;
	int may_free = 1;

	BUFFER_TRACE(bh, "entry");

	if (!buffer_mapped(bh))
		return 1;

	/* It is safe to proceed here without the
	 * journal_datalist_spinlock because the buffers cannot be
	 * stolen by try_to_free_buffers as long as we are holding the
	 * page lock. --sct */

	if (!buffer_jbd(bh))
		goto zap_buffer;

	jh = bh2jh(bh);
	transaction = jh->b_transaction;
	if (transaction == NULL) {
		/* First case: not on any transaction.  If it
		 * has no checkpoint link, then we can zap it:
		 * it's a writeback-mode buffer so we don't care
		 * if it hits disk safely. */
		if (!jh->b_cp_transaction) {
			JBUFFER_TRACE(jh, "not on any transaction: zap");
			goto zap_buffer;
		}
		
		if (!buffer_dirty(bh)) {
			/* bdflush has written it.  We can drop it now */
			goto zap_buffer;
		}

		/* OK, it must be in the journal but still not
		 * written fully to disk: it's metadata or
		 * journaled data... */

		if (journal->j_running_transaction) {
			/* ... and once the current transaction has
			 * committed, the buffer won't be needed any
			 * longer. */
			JBUFFER_TRACE(jh, "checkpointed: add to BJ_Forget");
			return dispose_buffer(jh,
					journal->j_running_transaction);
		} else {
			/* There is no currently-running transaction. So the
			 * orphan record which we wrote for this file must have
			 * passed into commit.  We must attach this buffer to
			 * the committing transaction, if it exists. */
			if (journal->j_committing_transaction) {
				JBUFFER_TRACE(jh, "give to committing trans");
				return dispose_buffer(jh,
					journal->j_committing_transaction);
			} else {
				/* The orphan record's transaction has
				 * committed.  We can cleanse this buffer */
				clear_bit(BH_JBDDirty, &bh->b_state);
				goto zap_buffer;
			}
		}
	} else if (transaction == journal->j_committing_transaction) {
		/* If it is committing, we simply cannot touch it.  We
		 * can remove it's next_transaction pointer from the
		 * running transaction if that is set, but nothing
		 * else. */
		JBUFFER_TRACE(jh, "on committing transaction");
		set_bit(BH_Freed, &bh->b_state);
		if (jh->b_next_transaction) {
			J_ASSERT(jh->b_next_transaction ==
					journal->j_running_transaction);
			jh->b_next_transaction = NULL;
		}
		return 0;
	} else {
		/* Good, the buffer belongs to the running transaction.
		 * We are writing our own transaction's data, not any
		 * previous one's, so it is safe to throw it away
		 * (remember that we expect the filesystem to have set
		 * i_size already for this truncate so recovery will not
		 * expose the disk blocks we are discarding here.) */
		J_ASSERT_JH(jh, transaction == journal->j_running_transaction);
		may_free = dispose_buffer(jh, transaction);
	}

zap_buffer:	
	if (buffer_dirty(bh))
		mark_buffer_clean(bh);
	J_ASSERT_BH(bh, !buffer_jdirty(bh));
	clear_bit(BH_Uptodate, &bh->b_state);
	clear_bit(BH_Mapped, &bh->b_state);
	clear_bit(BH_Req, &bh->b_state);
	clear_bit(BH_New, &bh->b_state);
	return may_free;
}

/** 
 * int journal_flushpage() 
 * @journal: journal to use for flush... 
 * @page:    page to flush
 * @offset:  length of page to flush.
 *
 * Reap page buffers containing data after offset in page.
 *
 * Return non-zero if the page's buffers were successfully reaped.
 */
int journal_flushpage(journal_t *journal, 
		      struct page *page, 
		      unsigned long offset)
{
	struct buffer_head *head, *bh, *next;
	unsigned int curr_off = 0;
	int may_free = 1;
		
	if (!PageLocked(page))
		BUG();
	if (!page->buffers)
		return 1;

	/* We will potentially be playing with lists other than just the
	 * data lists (especially for journaled data mode), so be
	 * cautious in our locking. */
	lock_journal(journal);

	head = bh = page->buffers;
	do {
		unsigned int next_off = curr_off + bh->b_size;
		next = bh->b_this_page;

		/* AKPM: doing lock_buffer here may be overly paranoid */
		if (offset <= curr_off) {
		 	/* This block is wholly outside the truncation point */
			lock_buffer(bh);
			may_free &= journal_unmap_buffer(journal, bh);
			unlock_buffer(bh);
		}
		curr_off = next_off;
		bh = next;

	} while (bh != head);

	unlock_journal(journal);

	if (!offset) {
		if (!may_free || !try_to_free_buffers(page, 0))
			return 0;
		J_ASSERT(page->buffers == NULL);
	}
	return 1;
}

/* 
 * File a buffer on the given transaction list. 
 */
void __journal_file_buffer(struct journal_head *jh,
			transaction_t *transaction, int jlist)
{
	struct journal_head **list = 0;
	int was_dirty = 0;

	assert_spin_locked(&journal_datalist_lock);
	
	J_ASSERT_JH(jh, jh->b_jlist < BJ_Types);
	J_ASSERT_JH(jh, jh->b_transaction == transaction ||
				jh->b_transaction == 0);

	if (jh->b_transaction && jh->b_jlist == jlist)
		return;
	
	/* The following list of buffer states needs to be consistent
	 * with __jbd_unexpected_dirty_buffer()'s handling of dirty
	 * state. */

	if (jlist == BJ_Metadata || jlist == BJ_Reserved || 
	    jlist == BJ_Shadow || jlist == BJ_Forget) {
		if (atomic_set_buffer_clean(jh2bh(jh)) ||
		    test_and_clear_bit(BH_JBDDirty, &jh2bh(jh)->b_state))
			was_dirty = 1;
	}

	if (jh->b_transaction)
		__journal_unfile_buffer(jh);
	else
		jh->b_transaction = transaction;

	switch (jlist) {
	case BJ_None:
		J_ASSERT_JH(jh, !jh->b_committed_data);
		J_ASSERT_JH(jh, !jh->b_frozen_data);
		return;
	case BJ_SyncData:
		list = &transaction->t_sync_datalist;
		break;
	case BJ_AsyncData:
		list = &transaction->t_async_datalist;
		break;
	case BJ_Metadata:
		transaction->t_nr_buffers++;
		list = &transaction->t_buffers;
		break;
	case BJ_Forget:
		list = &transaction->t_forget;
		break;
	case BJ_IO:
		list = &transaction->t_iobuf_list;
		break;
	case BJ_Shadow:
		list = &transaction->t_shadow_list;
		break;
	case BJ_LogCtl:
		list = &transaction->t_log_list;
		break;
	case BJ_Reserved:
		list = &transaction->t_reserved_list;
		break;
	}

	__blist_add_buffer(list, jh);
	jh->b_jlist = jlist;

	if (was_dirty)
		set_bit(BH_JBDDirty, &jh2bh(jh)->b_state);
}

void journal_file_buffer(struct journal_head *jh,
				transaction_t *transaction, int jlist)
{
	spin_lock(&journal_datalist_lock);
	__journal_file_buffer(jh, transaction, jlist);
	spin_unlock(&journal_datalist_lock);
}

static void jbd_refile_buffer(struct buffer_head *bh)
{
	if (buffer_dirty(bh) && (bh->b_list != BUF_DIRTY))
		set_buffer_flushtime(bh);
	refile_buffer(bh);
}

/* 
 * Remove a buffer from its current buffer list in preparation for
 * dropping it from its current transaction entirely.  If the buffer has
 * already started to be used by a subsequent transaction, refile the
 * buffer on that transaction's metadata list.
 */

void __journal_refile_buffer(struct journal_head *jh)
{
	int was_dirty = 0;

	assert_spin_locked(&journal_datalist_lock);
	/* If the buffer is now unused, just drop it. */
	if (jh->b_next_transaction == NULL) {
		__journal_unfile_buffer(jh);
		jh->b_transaction = NULL;
		/* Onto BUF_DIRTY for writeback */
		jbd_refile_buffer(jh2bh(jh));
		return;
	}
	
	/* It has been modified by a later transaction: add it to the
	 * new transaction's metadata list. */

	if (test_and_clear_bit(BH_JBDDirty, &jh2bh(jh)->b_state))
			was_dirty = 1;

	__journal_unfile_buffer(jh);
	jh->b_transaction = jh->b_next_transaction;
	jh->b_next_transaction = NULL;
	__journal_file_buffer(jh, jh->b_transaction, BJ_Metadata);
	J_ASSERT_JH(jh, jh->b_transaction->t_state == T_RUNNING);

	if (was_dirty)
		set_bit(BH_JBDDirty, &jh2bh(jh)->b_state);

}

/*
 * For the unlocked version of this call, also make sure that any
 * hanging journal_head is cleaned up if necessary.
 *
 * __journal_refile_buffer is usually called as part of a single locked
 * operation on a buffer_head, in which the caller is probably going to
 * be hooking the journal_head onto other lists.  In that case it is up
 * to the caller to remove the journal_head if necessary.  For the
 * unlocked journal_refile_buffer call, the caller isn't going to be
 * doing anything else to the buffer so we need to do the cleanup
 * ourselves to avoid a jh leak. 
 *
 * *** The journal_head may be freed by this call! ***
 */
void journal_refile_buffer(struct journal_head *jh)
{
	struct buffer_head *bh;

	spin_lock(&journal_datalist_lock);
	bh = jh2bh(jh);

	__journal_refile_buffer(jh);
	__journal_remove_journal_head(bh);

	spin_unlock(&journal_datalist_lock);
	__brelse(bh);
}
