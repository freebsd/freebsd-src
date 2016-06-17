/*
 * linux/fs/checkpoint.c
 * 
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1999
 *
 * Copyright 1999 Red Hat Software --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Checkpoint routines for the generic filesystem journaling code.  
 * Part of the ext2fs journaling system.  
 *
 * Checkpointing is the process of ensuring that a section of the log is
 * committed fully to disk, so that that portion of the log can be
 * reused.
 */

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/locks.h>

extern spinlock_t journal_datalist_lock;

/*
 * Unlink a buffer from a transaction. 
 *
 * Called with journal_datalist_lock held.
 */

static inline void __buffer_unlink(struct journal_head *jh)
{
	transaction_t *transaction;

	transaction = jh->b_cp_transaction;
	jh->b_cp_transaction = NULL;

	jh->b_cpnext->b_cpprev = jh->b_cpprev;
	jh->b_cpprev->b_cpnext = jh->b_cpnext;
	if (transaction->t_checkpoint_list == jh)
		transaction->t_checkpoint_list = jh->b_cpnext;
	if (transaction->t_checkpoint_list == jh)
		transaction->t_checkpoint_list = NULL;
}

/*
 * Try to release a checkpointed buffer from its transaction.
 * Returns 1 if we released it.
 * Requires journal_datalist_lock
 */
static int __try_to_free_cp_buf(struct journal_head *jh)
{
	int ret = 0;
	struct buffer_head *bh = jh2bh(jh);

	if (jh->b_jlist == BJ_None && !buffer_locked(bh) && !buffer_dirty(bh)) {
		JBUFFER_TRACE(jh, "remove from checkpoint list");
		__journal_remove_checkpoint(jh);
		__journal_remove_journal_head(bh);
		BUFFER_TRACE(bh, "release");
		/* BUF_LOCKED -> BUF_CLEAN (fwiw) */
		refile_buffer(bh);
		__brelse(bh);
		ret = 1;
	}
	return ret;
}

/*
 * log_wait_for_space: wait until there is space in the journal.
 *
 * Called with the journal already locked, but it will be unlocked if we have
 * to wait for a checkpoint to free up some space in the log.
 */

void log_wait_for_space(journal_t *journal, int nblocks)
{
	while (log_space_left(journal) < nblocks) {
		if (journal->j_flags & JFS_ABORT)
			return;
		unlock_journal(journal);
		down(&journal->j_checkpoint_sem);
		lock_journal(journal);
		
		/* Test again, another process may have checkpointed
		 * while we were waiting for the checkpoint lock */
		if (log_space_left(journal) < nblocks) {
			log_do_checkpoint(journal, nblocks);
		}
		up(&journal->j_checkpoint_sem);
	}
}

/*
 * Clean up a transaction's checkpoint list.  
 *
 * We wait for any pending IO to complete and make sure any clean
 * buffers are removed from the transaction. 
 *
 * Return 1 if we performed any actions which might have destroyed the
 * checkpoint.  (journal_remove_checkpoint() deletes the transaction when
 * the last checkpoint buffer is cleansed)
 *
 * Called with the journal locked.
 * Called with journal_datalist_lock held.
 */
static int __cleanup_transaction(journal_t *journal, transaction_t *transaction)
{
	struct journal_head *jh, *next_jh, *last_jh;
	struct buffer_head *bh;
	int ret = 0;

	assert_spin_locked(&journal_datalist_lock);
	jh = transaction->t_checkpoint_list;
	if (!jh)
		return 0;

	last_jh = jh->b_cpprev;
	next_jh = jh;
	do {
		jh = next_jh;
		bh = jh2bh(jh);
		if (buffer_locked(bh)) {
			atomic_inc(&bh->b_count);
			spin_unlock(&journal_datalist_lock);
			unlock_journal(journal);
			wait_on_buffer(bh);
			/* the journal_head may have gone by now */
			BUFFER_TRACE(bh, "brelse");
			__brelse(bh);
			goto out_return_1;
		}
		
		if (jh->b_transaction != NULL) {
			transaction_t *transaction = jh->b_transaction;
			tid_t tid = transaction->t_tid;

			spin_unlock(&journal_datalist_lock);
			log_start_commit(journal, transaction);
			unlock_journal(journal);
			log_wait_commit(journal, tid);
			goto out_return_1;
		}

		/*
		 * We used to test for (jh->b_list != BUF_CLEAN) here.
		 * But unmap_underlying_metadata() can place buffer onto
		 * BUF_CLEAN. Since refile_buffer() no longer takes buffers
		 * off checkpoint lists, we cope with it here
		 */
		/*
		 * AKPM: I think the buffer_jdirty test is redundant - it
		 * shouldn't have NULL b_transaction?
		 */
		next_jh = jh->b_cpnext;
		if (!buffer_dirty(bh) && !buffer_jdirty(bh)) {
			BUFFER_TRACE(bh, "remove from checkpoint");
			__journal_remove_checkpoint(jh);
			__journal_remove_journal_head(bh);
			refile_buffer(bh);
			__brelse(bh);
			ret = 1;
		}
		
		jh = next_jh;
	} while (jh != last_jh);

	return ret;
out_return_1:
	lock_journal(journal);
	spin_lock(&journal_datalist_lock);
	return 1;
}

#define NR_BATCH	64

static void __flush_batch(struct buffer_head **bhs, int *batch_count)
{
	int i;

	spin_unlock(&journal_datalist_lock);
	ll_rw_block(WRITE, *batch_count, bhs);
	run_task_queue(&tq_disk);
	spin_lock(&journal_datalist_lock);
	for (i = 0; i < *batch_count; i++) {
		struct buffer_head *bh = bhs[i];
		clear_bit(BH_JWrite, &bh->b_state);
		BUFFER_TRACE(bh, "brelse");
		__brelse(bh);
	}
	*batch_count = 0;
}

/*
 * Try to flush one buffer from the checkpoint list to disk.
 *
 * Return 1 if something happened which requires us to abort the current
 * scan of the checkpoint list.  
 *
 * Called with journal_datalist_lock held.
 */
static int __flush_buffer(journal_t *journal, struct journal_head *jh,
			struct buffer_head **bhs, int *batch_count,
			int *drop_count)
{
	struct buffer_head *bh = jh2bh(jh);
	int ret = 0;

	if (buffer_dirty(bh) && !buffer_locked(bh) && jh->b_jlist == BJ_None) {
		J_ASSERT_JH(jh, jh->b_transaction == NULL);
		
		/*
		 * Important: we are about to write the buffer, and
		 * possibly block, while still holding the journal lock.
		 * We cannot afford to let the transaction logic start
		 * messing around with this buffer before we write it to
		 * disk, as that would break recoverability.  
		 */
		BUFFER_TRACE(bh, "queue");
		atomic_inc(&bh->b_count);
		J_ASSERT_BH(bh, !test_bit(BH_JWrite, &bh->b_state));
		set_bit(BH_JWrite, &bh->b_state);
		bhs[*batch_count] = bh;
		(*batch_count)++;
		if (*batch_count == NR_BATCH) {
			__flush_batch(bhs, batch_count);
			ret = 1;
		}
	} else {
		int last_buffer = 0;
		if (jh->b_cpnext == jh) {
			/* We may be about to drop the transaction.  Tell the
			 * caller that the lists have changed.
			 */
			last_buffer = 1;
		}
		if (__try_to_free_cp_buf(jh)) {
			(*drop_count)++;
			ret = last_buffer;
		}
	}
	return ret;
}

	
/*
 * Perform an actual checkpoint.  We don't write out only enough to
 * satisfy the current blocked requests: rather we submit a reasonably
 * sized chunk of the outstanding data to disk at once for
 * efficiency.  log_wait_for_space() will retry if we didn't free enough.
 * 
 * However, we _do_ take into account the amount requested so that once
 * the IO has been queued, we can return as soon as enough of it has
 * completed to disk.  
 *
 * The journal should be locked before calling this function.
 */

/* @@@ `nblocks' is unused.  Should it be used? */
int log_do_checkpoint (journal_t *journal, int nblocks)
{
	transaction_t *transaction, *last_transaction, *next_transaction;
	int result;
	int target;
	int batch_count = 0;
	struct buffer_head *bhs[NR_BATCH];

	jbd_debug(1, "Start checkpoint\n");

	/* 
	 * First thing: if there are any transactions in the log which
	 * don't need checkpointing, just eliminate them from the
	 * journal straight away.  
	 */
	result = cleanup_journal_tail(journal);
	jbd_debug(1, "cleanup_journal_tail returned %d\n", result);
	if (result <= 0)
		return result;

	/*
	 * OK, we need to start writing disk blocks.  Try to free up a
	 * quarter of the log in a single checkpoint if we can.
	 */
	/*
	 * AKPM: check this code.  I had a feeling a while back that it
	 * degenerates into a busy loop at unmount time.
	 */
	target = (journal->j_last - journal->j_first) / 4;

	spin_lock(&journal_datalist_lock);
repeat:
	transaction = journal->j_checkpoint_transactions;
	if (transaction == NULL)
		goto done;
	last_transaction = transaction->t_cpprev;
	next_transaction = transaction;

	do {
		struct journal_head *jh, *last_jh, *next_jh;
		int drop_count = 0;
		int cleanup_ret, retry = 0;

		transaction = next_transaction;
		next_transaction = transaction->t_cpnext;
		jh = transaction->t_checkpoint_list;
		last_jh = jh->b_cpprev;
		next_jh = jh;
		do {
			jh = next_jh;
			next_jh = jh->b_cpnext;
			retry = __flush_buffer(journal, jh, bhs, &batch_count,
						&drop_count);
		} while (jh != last_jh && !retry);
		if (batch_count) {
			__flush_batch(bhs, &batch_count);
			goto repeat;
		}
		if (retry)
			goto repeat;
		/*
		 * We have walked the whole transaction list without
		 * finding anything to write to disk.  We had better be
		 * able to make some progress or we are in trouble. 
		 */
		cleanup_ret = __cleanup_transaction(journal, transaction);
		J_ASSERT(drop_count != 0 || cleanup_ret != 0);
		goto repeat;	/* __cleanup may have dropped lock */
	} while (transaction != last_transaction);

done:
	spin_unlock(&journal_datalist_lock);
	result = cleanup_journal_tail(journal);
	if (result < 0)
		return result;
	
	return 0;
}

/*
 * Check the list of checkpoint transactions for the journal to see if
 * we have already got rid of any since the last update of the log tail
 * in the journal superblock.  If so, we can instantly roll the
 * superblock forward to remove those transactions from the log.
 * 
 * Return <0 on error, 0 on success, 1 if there was nothing to clean up.
 * 
 * Called with the journal lock held.
 *
 * This is the only part of the journaling code which really needs to be
 * aware of transaction aborts.  Checkpointing involves writing to the
 * main filesystem area rather than to the journal, so it can proceed
 * even in abort state, but we must not update the journal superblock if
 * we have an abort error outstanding.
 */

int cleanup_journal_tail(journal_t *journal)
{
	transaction_t * transaction;
	tid_t		first_tid;
	unsigned long	blocknr, freed;

	/* OK, work out the oldest transaction remaining in the log, and
	 * the log block it starts at. 
	 * 
	 * If the log is now empty, we need to work out which is the
	 * next transaction ID we will write, and where it will
	 * start. */

	/* j_checkpoint_transactions needs locking */
	spin_lock(&journal_datalist_lock);
	transaction = journal->j_checkpoint_transactions;
	if (transaction) {
		first_tid = transaction->t_tid;
		blocknr = transaction->t_log_start;
	} else if ((transaction = journal->j_committing_transaction) != NULL) {
		first_tid = transaction->t_tid;
		blocknr = transaction->t_log_start;
	} else if ((transaction = journal->j_running_transaction) != NULL) {
		first_tid = transaction->t_tid;
		blocknr = journal->j_head;
	} else {
		first_tid = journal->j_transaction_sequence;
		blocknr = journal->j_head;
	}
	spin_unlock(&journal_datalist_lock);
	J_ASSERT (blocknr != 0);

	/* If the oldest pinned transaction is at the tail of the log
           already then there's not much we can do right now. */
	if (journal->j_tail_sequence == first_tid)
		return 1;

	/* OK, update the superblock to recover the freed space.
	 * Physical blocks come first: have we wrapped beyond the end of
	 * the log?  */
	freed = blocknr - journal->j_tail;
	if (blocknr < journal->j_tail)
		freed = freed + journal->j_last - journal->j_first;

	jbd_debug(1,
		  "Cleaning journal tail from %d to %d (offset %lu), "
		  "freeing %lu\n",
		  journal->j_tail_sequence, first_tid, blocknr, freed);

	journal->j_free += freed;
	journal->j_tail_sequence = first_tid;
	journal->j_tail = blocknr;
	if (!(journal->j_flags & JFS_ABORT))
		journal_update_superblock(journal, 1);
	return 0;
}


/* Checkpoint list management */

/*
 * journal_clean_checkpoint_list
 *
 * Find all the written-back checkpoint buffers in the journal and release them.
 *
 * Called with the journal locked.
 * Called with journal_datalist_lock held.
 * Returns number of bufers reaped (for debug)
 */

int __journal_clean_checkpoint_list(journal_t *journal)
{
	transaction_t *transaction, *last_transaction, *next_transaction;
	int ret = 0;

	transaction = journal->j_checkpoint_transactions;
	if (transaction == 0)
		goto out;

	last_transaction = transaction->t_cpprev;
	next_transaction = transaction;
	do {
		struct journal_head *jh;

		transaction = next_transaction;
		next_transaction = transaction->t_cpnext;
		jh = transaction->t_checkpoint_list;
		if (jh) {
			struct journal_head *last_jh = jh->b_cpprev;
			struct journal_head *next_jh = jh;
			do {
				jh = next_jh;
				next_jh = jh->b_cpnext;
				ret += __try_to_free_cp_buf(jh);
			} while (jh != last_jh);
		}
	} while (transaction != last_transaction);
out:
	return ret;
}

/* 
 * journal_remove_checkpoint: called after a buffer has been committed
 * to disk (either by being write-back flushed to disk, or being
 * committed to the log).
 *
 * We cannot safely clean a transaction out of the log until all of the
 * buffer updates committed in that transaction have safely been stored
 * elsewhere on disk.  To achieve this, all of the buffers in a
 * transaction need to be maintained on the transaction's checkpoint
 * list until they have been rewritten, at which point this function is
 * called to remove the buffer from the existing transaction's
 * checkpoint list.  
 *
 * This function is called with the journal locked.
 * This function is called with journal_datalist_lock held.
 */

void __journal_remove_checkpoint(struct journal_head *jh)
{
	transaction_t *transaction;
	journal_t *journal;

	JBUFFER_TRACE(jh, "entry");
	
	if ((transaction = jh->b_cp_transaction) == NULL) {
		JBUFFER_TRACE(jh, "not on transaction");
		goto out;
	}

	journal = transaction->t_journal;

	__buffer_unlink(jh);

	if (transaction->t_checkpoint_list != NULL)
		goto out;
	JBUFFER_TRACE(jh, "transaction has no more buffers");

	/* There is one special case to worry about: if we have just
           pulled the buffer off a committing transaction's forget list,
           then even if the checkpoint list is empty, the transaction
           obviously cannot be dropped! */

	if (transaction == journal->j_committing_transaction) {
		JBUFFER_TRACE(jh, "belongs to committing transaction");
		goto out;
	}

	/* OK, that was the last buffer for the transaction: we can now
	   safely remove this transaction from the log */

	__journal_drop_transaction(journal, transaction);

	/* Just in case anybody was waiting for more transactions to be
           checkpointed... */
	wake_up(&journal->j_wait_logspace);
out:
	JBUFFER_TRACE(jh, "exit");
}

void journal_remove_checkpoint(struct journal_head *jh)
{
	spin_lock(&journal_datalist_lock);
	__journal_remove_checkpoint(jh);
	spin_unlock(&journal_datalist_lock);
}

/*
 * journal_insert_checkpoint: put a committed buffer onto a checkpoint
 * list so that we know when it is safe to clean the transaction out of
 * the log.
 *
 * Called with the journal locked.
 * Called with journal_datalist_lock held.
 */
void __journal_insert_checkpoint(struct journal_head *jh, 
			       transaction_t *transaction)
{
	JBUFFER_TRACE(jh, "entry");
	J_ASSERT_JH(jh, buffer_dirty(jh2bh(jh)) || buffer_jdirty(jh2bh(jh)));
	J_ASSERT_JH(jh, jh->b_cp_transaction == NULL);

	assert_spin_locked(&journal_datalist_lock);
	jh->b_cp_transaction = transaction;

	if (!transaction->t_checkpoint_list) {
		jh->b_cpnext = jh->b_cpprev = jh;
	} else {
		jh->b_cpnext = transaction->t_checkpoint_list;
		jh->b_cpprev = transaction->t_checkpoint_list->b_cpprev;
		jh->b_cpprev->b_cpnext = jh;
		jh->b_cpnext->b_cpprev = jh;
	}
	transaction->t_checkpoint_list = jh;
}

void journal_insert_checkpoint(struct journal_head *jh, 
			       transaction_t *transaction)
{
	spin_lock(&journal_datalist_lock);
	__journal_insert_checkpoint(jh, transaction);
	spin_unlock(&journal_datalist_lock);
}

/*
 * We've finished with this transaction structure: adios...
 * 
 * The transaction must have no links except for the checkpoint by this
 * point.
 *
 * Called with the journal locked.
 * Called with journal_datalist_lock held.
 */

void __journal_drop_transaction(journal_t *journal, transaction_t *transaction)
{
	assert_spin_locked(&journal_datalist_lock);
	if (transaction->t_cpnext) {
		transaction->t_cpnext->t_cpprev = transaction->t_cpprev;
		transaction->t_cpprev->t_cpnext = transaction->t_cpnext;
		if (journal->j_checkpoint_transactions == transaction)
			journal->j_checkpoint_transactions =
				transaction->t_cpnext;
		if (journal->j_checkpoint_transactions == transaction)
			journal->j_checkpoint_transactions = NULL;
	}

	J_ASSERT (transaction->t_ilist == NULL);
	J_ASSERT (transaction->t_buffers == NULL);
	J_ASSERT (transaction->t_sync_datalist == NULL);
	J_ASSERT (transaction->t_async_datalist == NULL);
	J_ASSERT (transaction->t_forget == NULL);
	J_ASSERT (transaction->t_iobuf_list == NULL);
	J_ASSERT (transaction->t_shadow_list == NULL);
	J_ASSERT (transaction->t_log_list == NULL);
	J_ASSERT (transaction->t_checkpoint_list == NULL);
	J_ASSERT (transaction->t_updates == 0);
	J_ASSERT (list_empty(&transaction->t_jcb));

	J_ASSERT (transaction->t_journal->j_committing_transaction !=
					transaction);
	
	jbd_debug (1, "Dropping transaction %d, all done\n", 
		   transaction->t_tid);
	kfree (transaction);
}

