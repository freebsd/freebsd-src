/*
 * linux/fs/commit.c
 *
 * Written by Stephen C. Tweedie <sct@redhat.com>, 1998
 *
 * Copyright 1998 Red Hat corp --- All Rights Reserved
 *
 * This file is part of the Linux kernel and is made available under
 * the terms of the GNU General Public License, version 2, or at your
 * option, any later version, incorporated herein by reference.
 *
 * Journal commit routines for the generic filesystem journaling code;
 * part of the ext2fs journaling system.
 */

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/jbd.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/locks.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>

extern spinlock_t journal_datalist_lock;

/*
 * Default IO end handler for temporary BJ_IO buffer_heads.
 */
void journal_end_buffer_io_sync(struct buffer_head *bh, int uptodate)
{
	BUFFER_TRACE(bh, "");
	mark_buffer_uptodate(bh, uptodate);
	unlock_buffer(bh);
}

/*
 * When an ext3-ordered file is truncated, it is possible that many pages are
 * not sucessfully freed, because they are attached to a committing transaction.
 * After the transaction commits, these pages are left on the LRU, with no
 * ->mapping, and with attached buffers.  These pages are trivially reclaimable
 * by the VM, but their apparent absence upsets the VM accounting, and it makes
 * the numbers in /proc/meminfo look odd.
 *
 * So here, we have a buffer which has just come off the forget list.  Look to
 * see if we can strip all buffers from the backing page.
 *
 * Called under lock_journal(), and possibly under journal_datalist_lock.  The
 * caller provided us with a ref against the buffer, and we drop that here.
 */
static void release_buffer_page(struct buffer_head *bh)
{
	struct page *page;

	if (buffer_dirty(bh))
		goto nope;
	if (atomic_read(&bh->b_count) != 1)
		goto nope;
	page = bh->b_page;
	if (!page)
		goto nope;
	if (page->mapping)
		goto nope;

	/* OK, it's a truncated page */
	if (TryLockPage(page))
		goto nope;

	page_cache_get(page);
	__brelse(bh);
	try_to_free_buffers(page, GFP_NOIO);
	unlock_page(page);
	page_cache_release(page);
	return;

nope:
	__brelse(bh);
}

/*
 * journal_commit_transaction
 *
 * The primary function for committing a transaction to the log.  This
 * function is called by the journal thread to begin a complete commit.
 */
void journal_commit_transaction(journal_t *journal)
{
	transaction_t *commit_transaction;
	struct journal_head *jh, *new_jh, *descriptor;
	struct journal_head *next_jh, *last_jh;
	struct buffer_head *wbuf[64];
	int bufs;
	int flags;
	int err;
	unsigned long blocknr;
	char *tagp = NULL;
	journal_header_t *header;
	journal_block_tag_t *tag = NULL;
	int space_left = 0;
	int first_tag = 0;
	int tag_flag;
	int i;

	/*
	 * First job: lock down the current transaction and wait for
	 * all outstanding updates to complete.
	 */

	lock_journal(journal); /* Protect journal->j_running_transaction */

#ifdef COMMIT_STATS
	spin_lock(&journal_datalist_lock);
	summarise_journal_usage(journal);
	spin_unlock(&journal_datalist_lock);
#endif

	lock_kernel();
	
	J_ASSERT (journal->j_running_transaction != NULL);
	J_ASSERT (journal->j_committing_transaction == NULL);

	commit_transaction = journal->j_running_transaction;
	J_ASSERT (commit_transaction->t_state == T_RUNNING);

	jbd_debug (1, "JBD: starting commit of transaction %d\n",
		   commit_transaction->t_tid);

	commit_transaction->t_state = T_LOCKED;
	while (commit_transaction->t_updates != 0) {
		unlock_journal(journal);
		sleep_on(&journal->j_wait_updates);
		lock_journal(journal);
	}

	J_ASSERT (commit_transaction->t_outstanding_credits <=
			journal->j_max_transaction_buffers);

	/* Do we need to erase the effects of a prior journal_flush? */
	if (journal->j_flags & JFS_FLUSHED) {
		jbd_debug(3, "super block updated\n");
		journal_update_superblock(journal, 1);
	} else {
		jbd_debug(3, "superblock not updated\n");
	}

	/*
	 * First thing we are allowed to do is to discard any remaining
	 * BJ_Reserved buffers.  Note, it is _not_ permissible to assume
	 * that there are no such buffers: if a large filesystem
	 * operation like a truncate needs to split itself over multiple
	 * transactions, then it may try to do a journal_restart() while
	 * there are still BJ_Reserved buffers outstanding.  These must
	 * be released cleanly from the current transaction.
	 *
	 * In this case, the filesystem must still reserve write access
	 * again before modifying the buffer in the new transaction, but
	 * we do not require it to remember exactly which old buffers it
	 * has reserved.  This is consistent with the existing behaviour
	 * that multiple journal_get_write_access() calls to the same
	 * buffer are perfectly permissable.
	 */

	while (commit_transaction->t_reserved_list) {
		jh = commit_transaction->t_reserved_list;
		JBUFFER_TRACE(jh, "reserved, unused: refile");
		journal_refile_buffer(jh);
	}

	/*
	 * Now try to drop any written-back buffers from the journal's
	 * checkpoint lists.  We do this *before* commit because it potentially
	 * frees some memory
	 */
	spin_lock(&journal_datalist_lock);
	__journal_clean_checkpoint_list(journal);
	spin_unlock(&journal_datalist_lock);

	/* First part of the commit: force the revoke list out to disk.
	 * The revoke code generates its own metadata blocks on disk for this.
	 *
	 * It is important that we do this while the transaction is
	 * still locked.  Generating the revoke records should not
	 * generate any IO stalls, so this should be quick; and doing
	 * the work while we have the transaction locked means that we
	 * only ever have to maintain the revoke list for one
	 * transaction at a time.
	 */

	jbd_debug (3, "JBD: commit phase 1\n");

	journal_write_revoke_records(journal, commit_transaction);

	/*
	 * Now that we have built the revoke records, we can start
	 * reusing the revoke list for a new running transaction.  We
	 * can now safely start committing the old transaction: time to
	 * get a new running transaction for incoming filesystem updates
	 */

	commit_transaction->t_state = T_FLUSH;

	wake_up(&journal->j_wait_transaction_locked);

	journal->j_committing_transaction = commit_transaction;
	journal->j_running_transaction = NULL;

	commit_transaction->t_log_start = journal->j_head;

	unlock_kernel();
	
	jbd_debug (3, "JBD: commit phase 2\n");

	/*
	 * Now start flushing things to disk, in the order they appear
	 * on the transaction lists.  Data blocks go first.
	 */

	/*
	 * Whenever we unlock the journal and sleep, things can get added
	 * onto ->t_datalist, so we have to keep looping back to write_out_data
	 * until we *know* that the list is empty.
	 */
write_out_data:

	/*
	 * Cleanup any flushed data buffers from the data list.  Even in
	 * abort mode, we want to flush this out as soon as possible.
	 *
	 * We take journal_datalist_lock to protect the lists from
	 * journal_try_to_free_buffers().
	 */
	spin_lock(&journal_datalist_lock);

write_out_data_locked:
	bufs = 0;
	next_jh = commit_transaction->t_sync_datalist;
	if (next_jh == NULL)
		goto sync_datalist_empty;
	last_jh = next_jh->b_tprev;

	do {
		struct buffer_head *bh;

		jh = next_jh;
		next_jh = jh->b_tnext;
		bh = jh2bh(jh);
		if (!buffer_locked(bh)) {
			if (buffer_dirty(bh)) {
				BUFFER_TRACE(bh, "start journal writeout");
				atomic_inc(&bh->b_count);
				wbuf[bufs++] = bh;
			} else {
				BUFFER_TRACE(bh, "writeout complete: unfile");
				__journal_unfile_buffer(jh);
				jh->b_transaction = NULL;
				__journal_remove_journal_head(bh);
				refile_buffer(bh);
				release_buffer_page(bh);
			}
		}
		if (bufs == ARRAY_SIZE(wbuf)) {
			/*
			 * Major speedup: start here on the next scan
			 */
			J_ASSERT(commit_transaction->t_sync_datalist != 0);
			commit_transaction->t_sync_datalist = jh;
			break;
		}
	} while (jh != last_jh);

	if (bufs || current->need_resched) {
		jbd_debug(2, "submit %d writes\n", bufs);
		spin_unlock(&journal_datalist_lock);
		unlock_journal(journal);
		if (bufs)
			ll_rw_block(WRITE, bufs, wbuf);
		if (current->need_resched)
			schedule();
		journal_brelse_array(wbuf, bufs);
		lock_journal(journal);
		spin_lock(&journal_datalist_lock);
		if (bufs)
			goto write_out_data_locked;
	}

	/*
	 * Wait for all previously submitted IO on the data list to complete.
	 */
	jh = commit_transaction->t_sync_datalist;
	if (jh == NULL)
		goto sync_datalist_empty;

	do {
		struct buffer_head *bh;
		jh = jh->b_tprev;	/* Wait on the last written */
		bh = jh2bh(jh);
		if (buffer_locked(bh)) {
			spin_unlock(&journal_datalist_lock);
			unlock_journal(journal);
			wait_on_buffer(bh);
			/* the journal_head may have been removed now */
			lock_journal(journal);
			goto write_out_data;
		} else if (buffer_dirty(bh)) {
			goto write_out_data_locked;
		}
	} while (jh != commit_transaction->t_sync_datalist);
	goto write_out_data_locked;

sync_datalist_empty:
	/*
	 * Wait for all the async writepage data.  As they become unlocked
	 * in end_buffer_io_async(), the only place where they can be
	 * reaped is in try_to_free_buffers(), and we're locked against
	 * that.
	 */
	while ((jh = commit_transaction->t_async_datalist)) {
		struct buffer_head *bh = jh2bh(jh);
		if (__buffer_state(bh, Freed)) {
			BUFFER_TRACE(bh, "Cleaning freed buffer");
			clear_bit(BH_Freed, &bh->b_state);
			clear_bit(BH_Dirty, &bh->b_state);
		}
		if (buffer_locked(bh)) {
			spin_unlock(&journal_datalist_lock);
			unlock_journal(journal);
			wait_on_buffer(bh);
			lock_journal(journal);
			spin_lock(&journal_datalist_lock);
			continue;	/* List may have changed */
		}
		if (jh->b_next_transaction) {
			/*
			 * For writepage() buffers in journalled data mode: a
			 * later transaction may want the buffer for "metadata"
			 */
			__journal_refile_buffer(jh);
		} else {
			BUFFER_TRACE(bh, "finished async writeout: unfile");
			__journal_unfile_buffer(jh);
			jh->b_transaction = NULL;
			__journal_remove_journal_head(bh);
			BUFFER_TRACE(bh, "finished async writeout: refile");
			/* It can sometimes be on BUF_LOCKED due to migration
			 * from syncdata to asyncdata */
			if (bh->b_list != BUF_CLEAN)
				refile_buffer(bh);
			__brelse(bh);
		}
	}
	spin_unlock(&journal_datalist_lock);

	/*
	 * If we found any dirty or locked buffers, then we should have
	 * looped back up to the write_out_data label.  If there weren't
	 * any then journal_clean_data_list should have wiped the list
	 * clean by now, so check that it is in fact empty.
	 */
	J_ASSERT (commit_transaction->t_sync_datalist == NULL);
	J_ASSERT (commit_transaction->t_async_datalist == NULL);

	jbd_debug (3, "JBD: commit phase 3\n");

	/*
	 * Way to go: we have now written out all of the data for a
	 * transaction!  Now comes the tricky part: we need to write out
	 * metadata.  Loop over the transaction's entire buffer list:
	 */
	commit_transaction->t_state = T_COMMIT;

	descriptor = 0;
	bufs = 0;
	while (commit_transaction->t_buffers) {

		/* Find the next buffer to be journaled... */

		jh = commit_transaction->t_buffers;

		/* If we're in abort mode, we just un-journal the buffer and
		   release it for background writing. */

		if (is_journal_aborted(journal)) {
			JBUFFER_TRACE(jh, "journal is aborting: refile");
			journal_refile_buffer(jh);
			/* If that was the last one, we need to clean up
			 * any descriptor buffers which may have been
			 * already allocated, even if we are now
			 * aborting. */
			if (!commit_transaction->t_buffers)
				goto start_journal_io;
			continue;
		}

		/* Make sure we have a descriptor block in which to
		   record the metadata buffer. */

		if (!descriptor) {
			struct buffer_head *bh;

			J_ASSERT (bufs == 0);

			jbd_debug(4, "JBD: get descriptor\n");

			descriptor = journal_get_descriptor_buffer(journal);
			if (!descriptor) {
				__journal_abort_hard(journal);
				continue;
			}
			
			bh = jh2bh(descriptor);
			jbd_debug(4, "JBD: got buffer %ld (%p)\n",
				bh->b_blocknr, bh->b_data);
			header = (journal_header_t *)&bh->b_data[0];
			header->h_magic     = htonl(JFS_MAGIC_NUMBER);
			header->h_blocktype = htonl(JFS_DESCRIPTOR_BLOCK);
			header->h_sequence  = htonl(commit_transaction->t_tid);

			tagp = &bh->b_data[sizeof(journal_header_t)];
			space_left = bh->b_size - sizeof(journal_header_t);
			first_tag = 1;
			set_bit(BH_JWrite, &bh->b_state);
			wbuf[bufs++] = bh;

			/* Record it so that we can wait for IO
                           completion later */
			BUFFER_TRACE(bh, "ph3: file as descriptor");
			journal_file_buffer(descriptor, commit_transaction,
						BJ_LogCtl);
		}

		/* Where is the buffer to be written? */

		err = journal_next_log_block(journal, &blocknr);
		/* If the block mapping failed, just abandon the buffer
		   and repeat this loop: we'll fall into the
		   refile-on-abort condition above. */
		if (err) {
			__journal_abort_hard(journal);
			continue;
		}

		/* Bump b_count to prevent truncate from stumbling over
                   the shadowed buffer!  @@@ This can go if we ever get
                   rid of the BJ_IO/BJ_Shadow pairing of buffers. */
		atomic_inc(&jh2bh(jh)->b_count);

		/* Make a temporary IO buffer with which to write it out
                   (this will requeue both the metadata buffer and the
                   temporary IO buffer). new_bh goes on BJ_IO*/

		set_bit(BH_JWrite, &jh2bh(jh)->b_state);
		/*
		 * akpm: journal_write_metadata_buffer() sets
		 * new_bh->b_transaction to commit_transaction.
		 * We need to clean this up before we release new_bh
		 * (which is of type BJ_IO)
		 */
		JBUFFER_TRACE(jh, "ph3: write metadata");
		flags = journal_write_metadata_buffer(commit_transaction,
						      jh, &new_jh, blocknr);
		set_bit(BH_JWrite, &jh2bh(new_jh)->b_state);
		set_bit(BH_Lock, &jh2bh(new_jh)->b_state);
		wbuf[bufs++] = jh2bh(new_jh);

		/* Record the new block's tag in the current descriptor
                   buffer */

		tag_flag = 0;
		if (flags & 1)
			tag_flag |= JFS_FLAG_ESCAPE;
		if (!first_tag)
			tag_flag |= JFS_FLAG_SAME_UUID;

		tag = (journal_block_tag_t *) tagp;
		tag->t_blocknr = htonl(jh2bh(jh)->b_blocknr);
		tag->t_flags = htonl(tag_flag);
		tagp += sizeof(journal_block_tag_t);
		space_left -= sizeof(journal_block_tag_t);

		if (first_tag) {
			memcpy (tagp, journal->j_uuid, 16);
			tagp += 16;
			space_left -= 16;
			first_tag = 0;
		}

		/* If there's no more to do, or if the descriptor is full,
		   let the IO rip! */

		if (bufs == ARRAY_SIZE(wbuf) ||
		    commit_transaction->t_buffers == NULL ||
		    space_left < sizeof(journal_block_tag_t) + 16) {

			jbd_debug(4, "JBD: Submit %d IOs\n", bufs);

			/* Write an end-of-descriptor marker before
                           submitting the IOs.  "tag" still points to
                           the last tag we set up. */

			tag->t_flags |= htonl(JFS_FLAG_LAST_TAG);

start_journal_io:
			unlock_journal(journal);
			for (i=0; i<bufs; i++) {
				struct buffer_head *bh = wbuf[i];
				clear_bit(BH_Dirty, &bh->b_state);
				bh->b_end_io = journal_end_buffer_io_sync;
				submit_bh(WRITE, bh);
			}
			if (current->need_resched)
				schedule();
			lock_journal(journal);

			/* Force a new descriptor to be generated next
                           time round the loop. */
			descriptor = NULL;
			bufs = 0;
		}
	}

	/* Lo and behold: we have just managed to send a transaction to
           the log.  Before we can commit it, wait for the IO so far to
           complete.  Control buffers being written are on the
           transaction's t_log_list queue, and metadata buffers are on
           the t_iobuf_list queue.

	   Wait for the buffers in reverse order.  That way we are
	   less likely to be woken up until all IOs have completed, and
	   so we incur less scheduling load.
	*/

	jbd_debug(3, "JBD: commit phase 4\n");

	/* akpm: these are BJ_IO, and journal_datalist_lock is not needed */
 wait_for_iobuf:
	while (commit_transaction->t_iobuf_list != NULL) {
		struct buffer_head *bh;
		jh = commit_transaction->t_iobuf_list->b_tprev;
		bh = jh2bh(jh);
		if (buffer_locked(bh)) {
			unlock_journal(journal);
			wait_on_buffer(bh);
			lock_journal(journal);
			goto wait_for_iobuf;
		}

		clear_bit(BH_JWrite, &jh2bh(jh)->b_state);

		JBUFFER_TRACE(jh, "ph4: unfile after journal write");
		journal_unfile_buffer(jh);

		/*
		 * akpm: don't put back a buffer_head with stale pointers
		 * dangling around.
		 */
		J_ASSERT_JH(jh, jh->b_transaction != NULL);
		jh->b_transaction = NULL;

		/*
		 * ->t_iobuf_list should contain only dummy buffer_heads
		 * which were created by journal_write_metadata_buffer().
		 */
		bh = jh2bh(jh);
		BUFFER_TRACE(bh, "dumping temporary bh");
		journal_unlock_journal_head(jh);
		__brelse(bh);
		J_ASSERT_BH(bh, atomic_read(&bh->b_count) == 0);
		put_unused_buffer_head(bh);

		/* We also have to unlock and free the corresponding
                   shadowed buffer */
		jh = commit_transaction->t_shadow_list->b_tprev;
		bh = jh2bh(jh);
		clear_bit(BH_JWrite, &bh->b_state);
		J_ASSERT_BH(bh, buffer_jdirty(bh));

		/* The metadata is now released for reuse, but we need
                   to remember it against this transaction so that when
                   we finally commit, we can do any checkpointing
                   required. */
		JBUFFER_TRACE(jh, "file as BJ_Forget");
		journal_file_buffer(jh, commit_transaction, BJ_Forget);
		/* Wake up any transactions which were waiting for this
		   IO to complete */
		wake_up(&bh->b_wait);
		JBUFFER_TRACE(jh, "brelse shadowed buffer");
		__brelse(bh);
	}

	J_ASSERT (commit_transaction->t_shadow_list == NULL);

	jbd_debug(3, "JBD: commit phase 5\n");

	/* Here we wait for the revoke record and descriptor record buffers */
 wait_for_ctlbuf:
	while (commit_transaction->t_log_list != NULL) {
		struct buffer_head *bh;

		jh = commit_transaction->t_log_list->b_tprev;
		bh = jh2bh(jh);
		if (buffer_locked(bh)) {
			unlock_journal(journal);
			wait_on_buffer(bh);
			lock_journal(journal);
			goto wait_for_ctlbuf;
		}

		BUFFER_TRACE(bh, "ph5: control buffer writeout done: unfile");
		clear_bit(BH_JWrite, &bh->b_state);
		journal_unfile_buffer(jh);
		jh->b_transaction = NULL;
		journal_unlock_journal_head(jh);
		put_bh(bh);			/* One for getblk */
	}

	jbd_debug(3, "JBD: commit phase 6\n");

	if (is_journal_aborted(journal)) {
		unlock_journal(journal);
		goto skip_commit;
	}

	/* Done it all: now write the commit record.  We should have
	 * cleaned up our previous buffers by now, so if we are in abort
	 * mode we can now just skip the rest of the journal write
	 * entirely. */

	descriptor = journal_get_descriptor_buffer(journal);
	if (!descriptor) {
		__journal_abort_hard(journal);
		unlock_journal(journal);
		goto skip_commit;
	}

	/* AKPM: buglet - add `i' to tmp! */
	for (i = 0; i < jh2bh(descriptor)->b_size; i += 512) {
		journal_header_t *tmp =
			(journal_header_t*)jh2bh(descriptor)->b_data;
		tmp->h_magic = htonl(JFS_MAGIC_NUMBER);
		tmp->h_blocktype = htonl(JFS_COMMIT_BLOCK);
		tmp->h_sequence = htonl(commit_transaction->t_tid);
	}

	unlock_journal(journal);
	JBUFFER_TRACE(descriptor, "write commit block");
	{
		struct buffer_head *bh = jh2bh(descriptor);
		clear_bit(BH_Dirty, &bh->b_state);
		bh->b_end_io = journal_end_buffer_io_sync;
		submit_bh(WRITE, bh);
		wait_on_buffer(bh);
		put_bh(bh);		/* One for getblk() */
		journal_unlock_journal_head(descriptor);
	}

	/* End of a transaction!  Finally, we can do checkpoint
           processing: any buffers committed as a result of this
           transaction can be removed from any checkpoint list it was on
           before. */

skip_commit: /* The journal should be unlocked by now. */

	/* Call any callbacks that had been registered for handles in this
	 * transaction.  It is up to the callback to free any allocated
	 * memory.
	 */
	if (!list_empty(&commit_transaction->t_jcb)) {
		struct list_head *p, *n;
		int error = is_journal_aborted(journal);

		list_for_each_safe(p, n, &commit_transaction->t_jcb) {
			struct journal_callback *jcb;

			jcb = list_entry(p, struct journal_callback, jcb_list);
			list_del(p);
			jcb->jcb_func(jcb, error);
		}
	}

	lock_journal(journal);

	jbd_debug(3, "JBD: commit phase 7\n");

	J_ASSERT(commit_transaction->t_sync_datalist == NULL);
	J_ASSERT(commit_transaction->t_async_datalist == NULL);
	J_ASSERT(commit_transaction->t_buffers == NULL);
	J_ASSERT(commit_transaction->t_checkpoint_list == NULL);
	J_ASSERT(commit_transaction->t_iobuf_list == NULL);
	J_ASSERT(commit_transaction->t_shadow_list == NULL);
	J_ASSERT(commit_transaction->t_log_list == NULL);

	while (commit_transaction->t_forget) {
		transaction_t *cp_transaction;
		struct buffer_head *bh;
		int was_freed = 0;
		
		jh = commit_transaction->t_forget;
		J_ASSERT_JH(jh,	jh->b_transaction == commit_transaction ||
			jh->b_transaction == journal->j_running_transaction);

		/*
		 * If there is undo-protected committed data against
		 * this buffer, then we can remove it now.  If it is a
		 * buffer needing such protection, the old frozen_data
		 * field now points to a committed version of the
		 * buffer, so rotate that field to the new committed
		 * data.
		 *
		 * Otherwise, we can just throw away the frozen data now.
		 */
		if (jh->b_committed_data) {
			kfree(jh->b_committed_data);
			jh->b_committed_data = NULL;
			if (jh->b_frozen_data) {
				jh->b_committed_data = jh->b_frozen_data;
				jh->b_frozen_data = NULL;
			}
		} else if (jh->b_frozen_data) {
			kfree(jh->b_frozen_data);
			jh->b_frozen_data = NULL;
		}

		spin_lock(&journal_datalist_lock);
		cp_transaction = jh->b_cp_transaction;
		if (cp_transaction) {
			JBUFFER_TRACE(jh, "remove from old cp transaction");
			J_ASSERT_JH(jh, commit_transaction != cp_transaction);
			__journal_remove_checkpoint(jh);
		}

		/* Only re-checkpoint the buffer_head if it is marked
		 * dirty.  If the buffer was added to the BJ_Forget list
		 * by journal_forget, it may no longer be dirty and
		 * there's no point in keeping a checkpoint record for
		 * it. */
		bh = jh2bh(jh);

		/* A buffer which has been freed while still being
		 * journaled by a previous transaction may end up still
		 * being dirty here, but we want to avoid writing back
		 * that buffer in the future now that the last use has
		 * been committed.  That's not only a performance gain,
		 * it also stops aliasing problems if the buffer is left
		 * behind for writeback and gets reallocated for another
		 * use in a different page. */
		if (__buffer_state(bh, Freed)) {
			was_freed = 1;
			clear_bit(BH_Freed, &bh->b_state);
			clear_bit(BH_JBDDirty, &bh->b_state);
		}
			
		if (buffer_jdirty(bh)) {
			JBUFFER_TRACE(jh, "add to new checkpointing trans");
			__journal_insert_checkpoint(jh, commit_transaction);
			JBUFFER_TRACE(jh, "refile for checkpoint writeback");
			__journal_refile_buffer(jh);
		} else {
			J_ASSERT_BH(bh, !buffer_dirty(bh));
			J_ASSERT_JH(jh, jh->b_next_transaction == NULL);
			__journal_unfile_buffer(jh);
			jh->b_transaction = 0;
			__journal_remove_journal_head(bh);
			spin_unlock(&journal_datalist_lock);
			if (was_freed)
				release_buffer_page(bh);
			else
				__brelse(bh);
			continue;
		}
		spin_unlock(&journal_datalist_lock);
	}

	/* Done with this transaction! */

	jbd_debug(3, "JBD: commit phase 8\n");

	J_ASSERT (commit_transaction->t_state == T_COMMIT);
	commit_transaction->t_state = T_FINISHED;

	J_ASSERT (commit_transaction == journal->j_committing_transaction);
	journal->j_commit_sequence = commit_transaction->t_tid;
	journal->j_committing_transaction = NULL;

	spin_lock(&journal_datalist_lock);
	if (commit_transaction->t_checkpoint_list == NULL) {
		__journal_drop_transaction(journal, commit_transaction);
	} else {
		if (journal->j_checkpoint_transactions == NULL) {
			journal->j_checkpoint_transactions = commit_transaction;
			commit_transaction->t_cpnext = commit_transaction;
			commit_transaction->t_cpprev = commit_transaction;
		} else {
			commit_transaction->t_cpnext =
				journal->j_checkpoint_transactions;
			commit_transaction->t_cpprev =
				commit_transaction->t_cpnext->t_cpprev;
			commit_transaction->t_cpnext->t_cpprev =
				commit_transaction;
			commit_transaction->t_cpprev->t_cpnext =
				commit_transaction;
		}
	}
	spin_unlock(&journal_datalist_lock);

	jbd_debug(1, "JBD: commit %d complete, head %d\n",
		  journal->j_commit_sequence, journal->j_tail_sequence);

	unlock_journal(journal);
	wake_up(&journal->j_wait_done_commit);
}
