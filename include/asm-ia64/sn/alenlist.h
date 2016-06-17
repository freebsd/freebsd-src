/* $Id$
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 - 1997, 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */
#ifndef _ASM_IA64_SN_ALENLIST_H
#define _ASM_IA64_SN_ALENLIST_H

#include <linux/types.h>

/* Definition of Address/Length List */

/*
 * An Address/Length List is used when setting up for an I/O DMA operation.
 * A driver creates an Address/Length List that describes to the the DMA 
 * interface where in memory the DMA should go.  The bus interface sets up 
 * mapping registers, if required, and returns a suitable list of "physical 
 * addresses" or "I/O address" to the driver.  The driver then uses these 
 * to set up an appropriate scatter/gather operation(s).
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * An Address/Length List Address.  It'll get cast to the appropriate type,
 * and must be big enough to hold the largest possible address in any
 * supported address space.
 */
typedef u64 alenaddr_t;
typedef u64 uvaddr_t;

typedef struct alenlist_s *alenlist_t;

/* 
 * For tracking progress as we walk down an address/length list.
 */
typedef struct alenlist_cursor_s *alenlist_cursor_t;

/*
 * alenlist representation that can be passed via an idl
 */
struct external_alenlist {
	alenaddr_t	addr;
	size_t		len;
};
typedef struct external_alenlist *external_alenlist_t;


/* Return codes from alenlist routines.  */
#define ALENLIST_FAILURE (-1)
#define ALENLIST_SUCCESS 0


/* Flags to alenlist routines */
#define AL_NOSLEEP	0x01		/* Do not sleep, waiting for memory */
#define AL_NOCOMPACT	0x02		/* Do not try to compact adjacent entries */
#define AL_LEAVE_CURSOR	0x04		/* Do not update cursor */


/* Create an Address/Length List, and clear it of all entries.  */
extern alenlist_t alenlist_create(unsigned flags);

/* Grow/shrink an Address/Length List and FIX its size. */
extern int alenlist_grow(alenlist_t, size_t npairs);

/* Clear an Address/Length List so that it now describes 0 pairs. */
extern void alenlist_clear(alenlist_t alenlist);

/*
 * Convenience function to create an Address/Length List and then append 
 * the specified Address/Length Pair.  Exactly the same as alenlist_create 
 * followed by alenlist_append.  Can be used when a small list (e.g. 1 pair)
 * is adequate.
 */
extern alenlist_t
alenpair_init(	alenaddr_t address, 			/* init to this address */
		size_t length);				/* init to this length */

/* 
 * Peek at the head of an Address/Length List.  This does *NOT* update
 * the internal cursor.
 */
extern int
alenpair_get(	alenlist_t alenlist,		/* in: get from this List */
		alenaddr_t *address,		/* out: address */
		size_t *length);		/* out: length */

/* Free the space consumed by an Address/Length List. */
extern void alenlist_destroy(alenlist_t alenlist);

/*
 * Indicate that we're done using an Address/Length List.
 * If we are the last user, destroy the List.
 */
extern void
alenlist_done(alenlist_t alenlist);

/* Append another Pair to a List */
extern int alenlist_append(alenlist_t alenlist, 	/* append to this list */
			alenaddr_t address,		/* address to append */
			size_t length,			/* length to append */
			unsigned flags);

/* 
 * Replace a Pair in the middle of a List, and return old values.
 * (not generally useful for drivers; used by bus providers).
 */
extern int
alenlist_replace(	alenlist_t alenlist, 		/* in: replace in this list */
			alenlist_cursor_t cursorp,	/* inout: which item to replace */
			alenaddr_t *addrp, 		/* inout: address */
			size_t *lengthp,		/* inout: length */
			unsigned flags);


/* Get the next Pair from a List */
extern int alenlist_get(alenlist_t alenlist, 		/* in: get from this list */
			alenlist_cursor_t cursorp,	/* inout: which item to get */
			size_t maxlength,		/* in: at most length */
			alenaddr_t *addr, 		/* out: address */
			size_t *length,			/* out: length */
			unsigned flags);


/* Return the number of Pairs stored in this List */
extern int alenlist_size(alenlist_t alenlist);

/* Concatenate two Lists. */
extern void alenlist_concat(	alenlist_t from, 	/* copy from this list */
				alenlist_t to);		/* to this list */

/* Create a copy of an Address/Length List */
extern alenlist_t alenlist_clone(alenlist_t old,	/* clone this list */
				 unsigned flags);


/* Allocate and initialize an Address/Length List Cursor */
extern alenlist_cursor_t alenlist_cursor_create(alenlist_t alenlist, unsigned flags);

/* Free an Address/Length List Cursor */
extern void alenlist_cursor_destroy(alenlist_cursor_t cursorp);

/*
 * Initialize an Address/Length List Cursor in order to walk thru an
 * Address/Length List from the beginning.
 */
extern int alenlist_cursor_init(alenlist_t alenlist, 
				size_t offset, 
				alenlist_cursor_t cursorp);

/* Clone an Address/Length List Cursor. */
extern int alenlist_cursor_clone(alenlist_t alenlist, 
				alenlist_cursor_t cursorp_in, 
				alenlist_cursor_t cursorp_out);

/* 
 * Return the number of bytes passed so far according to the specified
 * Address/Length List Cursor.
 */
extern size_t alenlist_cursor_offset(alenlist_t alenlist, alenlist_cursor_t cursorp);




/* Convert from a Kernel Virtual Address to a Physical Address/Length List */
extern alenlist_t kvaddr_to_alenlist(	alenlist_t alenlist, 
					caddr_t kvaddr, 
					size_t length, 
					unsigned flags);

/* Convert from a User Virtual Address to a Physical Address/Length List */
extern alenlist_t uvaddr_to_alenlist(	alenlist_t alenlist,
					uvaddr_t vaddr, 
					size_t length,
					unsigned flags);

/* Convert from a buf struct to a Physical Address/Length List */
struct buf;
extern alenlist_t buf_to_alenlist(	alenlist_t alenlist, 
					struct buf *buf, 
					unsigned flags);


/* 
 * Tracking position as we walk down an Address/Length List.
 * This structure is NOT generally for use by device drivers.
 */
struct alenlist_cursor_s {
	struct alenlist_s	*al_alenlist;	/* which list */
	size_t			al_offset;	/* total bytes passed by cursor */
	struct alenlist_chunk_s	*al_chunk;	/* which chunk in alenlist */
	unsigned int		al_index;	/* which pair in chunk */
	size_t			al_bcount;	/* offset into address/length pair */
};

#ifdef __cplusplus
}
#endif

#endif /* _ASM_IA64_SN_ALENLIST_H */
