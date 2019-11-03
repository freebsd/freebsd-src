#ifndef __PSPAT_MAILBOX_H
#define __PSPAT_MAILBOX_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#define PSPAT_MB_NAMSZ	32
#define PSPAT_MB_DEBUG	true

MALLOC_DECLARE(M_MB);

#define ENTRY_EMPTY(entry) \
    (((entry).entries.tqe_next == NULL) && (((entry).entries.tqe_prev) == NULL))

#define CLALIGN __aligned(CACHE_LINE_SIZE)

/* List of entries */
struct list {
	TAILQ_ENTRY(list) entries;
	void  *mb; /* Address of containing mailbox */
};

static inline void entry_init(struct list *entry) {
	entry->entries.tqe_prev = NULL;
	entry->entries.tqe_next = NULL;
}

TAILQ_HEAD(entry_list, list);

/* A single producer/ single consumer lock-free mailbox queue */
struct pspat_mailbox {
	/* Constant fields shared between producer / consumer */
	char	      name[PSPAT_MB_NAMSZ]; /* Name of this mailboox */
	unsigned long entry_mask;	    /* Mask that when applied to an index, normalizes it to be an entry */
        unsigned long entries_per_line;	    /* Number of entries that fit in one cache line */
	unsigned long line_mask;	    /* TODO */

	/* Mutable fields shared between producer / consumer */
	unsigned long backpressure;	    /* TODO(astanesc): Isn't this really a bool? */
	unsigned int  identifier;	    /* Unique identifier for this mailbox */

	/* Mutable fields written only by the producer */
	bool	      dead;		    /* Is this mailbox alive or dead? */
	unsigned long prod_write CLALIGN;  /* Index of the next entry that producer will write to */
	unsigned long prod_check;	    /* Index of the first entry we have not reserved */

	/* Mutable fields written only by the consumer */
	unsigned long cons_clear CLALIGN;   /* Index of the first entry we have *not* cleared */
	unsigned long cons_read;	    /* Index of the next entry we will read */

	/* TAILQ of mailboxes */
	struct list   entry;

	/* The actual queue */
	void *	      q[0] CLALIGN;
};


static inline size_t pspat_mb_size(unsigned long entries)
{
	return roundup(sizeof(struct pspat_mailbox) + entries * sizeof(void *), CACHE_LINE_SIZE);
}

/*
 * pspat_mb_new - create a new mailbox
 * @name: A name for the mailbox (useful for debugging)
 * @entries: The number of entries
 * @line_size: The size of a cache line in bytes
 *
 * Both entries and line_size must be powers of 2.
 */
int pspat_mb_new(const char *name,
		 unsigned long entries,
		 unsigned long line_size,
		 struct pspat_mailbox **m);

/*
 * pspat_mb_init - Initialize a pre-allocated mailbox
 * @m: The mailbox to be initialized
 * @name: An arbitrary name for the mailbox (useful for debugging)
 * @entries: the number of entries
 * @line_size: the line size in bytes
 *
 * Both Entries and line_size must be a power of 2. Returns 0 on success,
 * -errno on failure
 */
int pspat_mb_init(struct pspat_mailbox *m,
		  const char *name,
		  unsigned long entries,
		  unsigned long line_size);

/*
 * pspat_mb_delete - Deletes a mailbox
 * @m: The mailbox to be deleted
 */
void pspat_mb_delete(struct pspat_mailbox *m);

/*
 * pspat_mb_dump_state - Dumps state about the mailbox
 * @m: The mailbox to dump state about
 */
void pspat_mb_dump_state(struct pspat_mailbox *m);

/*
 * pspat_mb_insert - Enqueue a value into the mailbox
 * @m: The mailbox to enqueue into
 * @v: The value to enqueue
 *
 * Returns 0 on success, -ENOBUFS on failure.
 */
static inline int pspat_mb_insert(struct pspat_mailbox *m, void *v) {
	/* The location where we will put the value */
	void **vloc = &m->q[m->prod_write & m->entry_mask];

	/* If we've reached the end of the reserved line */
	if (m->prod_write == m->prod_check) {
		/* Reserve a new cache line to avoid thrashing */
		if (m->q[(m->prod_check + m->entries_per_line) & m->entry_mask] != NULL) {
			/* Next line hasn't been cleared! */
			return -ENOBUFS;
		}

		m->prod_check += m->entries_per_line;
		/* Prefetch the next line */
		__builtin_prefetch((char *)vloc + m->entries_per_line);
	}

	*vloc = v;
	m->prod_write++;
	return 0;
}

/*
 * pspat_mb_empty - tests to see if the mailbox is empty
 * @m: The mailbox to test
 */
static inline bool pspat_mb_empty(struct pspat_mailbox *m) {
	void *v = m->q[m->cons_read & m->entry_mask];
	return v != NULL;
}

/*
 * pspat_mb_extract - extract a value
 * @m: The mailbox to extract from
 *
 * Returns the extracted value (or NULL if the mailbox is empty).
 * This does *not* free the entry. Please use pspat_mb_clear to free entries
 */
static inline void *pspat_mb_extract(struct pspat_mailbox *m) {
	void *v = m->q[m->cons_read & m->entry_mask];
	if (v != NULL) {
		m->cons_read ++;
	}

	return v;
}

/**
 * pspat_mb_clear - Clear the previously extracted entries
 * @m: The mailbox to be cleared
 */
static inline void pspat_mb_clear(struct pspat_mailbox *m) {
	unsigned long next_clear = (m->cons_read & m->line_mask) - m->entries_per_line;

	while(m->cons_clear != next_clear) {
		m->q[m->cons_clear & m->entry_mask] = NULL;
		m->cons_clear++;
	}
}

/**
 * pspat_mb_prefetch - Prefetch the next line to read
 * @m: The mailbox to run the prefetch on
 */
static inline void pspat_mb_prefetch(struct pspat_mailbox *m) {
	__builtin_prefetch((void *)m->q[m->cons_read & m->entry_mask]);
}

#endif /* __PSPAT_MAILBOX_H */
