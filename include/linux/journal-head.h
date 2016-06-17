/*
 * include/linux/journal-head.h
 *
 * buffer_head fields for JBD
 *
 * 27 May 2001 ANdrew Morton <andrewm@uow.edu.au>
 *	Created - pulled out of fs.h
 */

#ifndef JOURNAL_HEAD_H_INCLUDED
#define JOURNAL_HEAD_H_INCLUDED

typedef unsigned int		tid_t;		/* Unique transaction ID */
typedef struct transaction_s	transaction_t;	/* Compound transaction type */
struct buffer_head;

struct journal_head {
#ifndef CONFIG_JBD_UNIFIED_BUFFERS
	/* Points back to our buffer_head. */
	struct buffer_head *b_bh;
#endif

	/* Reference count - see description in journal.c */
	int b_jcount;

	/* Journaling list for this buffer */
	unsigned b_jlist;

	/* Copy of the buffer data frozen for writing to the log. */
	char * b_frozen_data;

	/* Pointer to a saved copy of the buffer containing no
           uncommitted deallocation references, so that allocations can
           avoid overwriting uncommitted deletes. */
	char * b_committed_data;

	/* Pointer to the compound transaction which owns this buffer's
           metadata: either the running transaction or the committing
           transaction (if there is one).  Only applies to buffers on a
           transaction's data or metadata journaling list. */
	/* Protected by journal_datalist_lock */
	transaction_t * b_transaction;
	
	/* Pointer to the running compound transaction which is
           currently modifying the buffer's metadata, if there was
           already a transaction committing it when the new transaction
           touched it. */
	transaction_t * b_next_transaction;
	
	/* Doubly-linked list of buffers on a transaction's data,
           metadata or forget queue. */
	/* Protected by journal_datalist_lock */
	struct journal_head *b_tnext, *b_tprev;

	/*
	 * Pointer to the compound transaction against which this buffer
	 * is checkpointed.  Only dirty buffers can be checkpointed.
	 */
	/* Protected by journal_datalist_lock */
	transaction_t * b_cp_transaction;
	
	/*
	 * Doubly-linked list of buffers still remaining to be flushed
	 * before an old transaction can be checkpointed.
	 */
	/* Protected by journal_datalist_lock */
	struct journal_head *b_cpnext, *b_cpprev;
};

#endif		/* JOURNAL_HEAD_H_INCLUDED */
