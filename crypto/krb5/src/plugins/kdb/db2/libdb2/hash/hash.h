/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)hash.h	8.4 (Berkeley) 11/2/95
 */

#include "mpool.h"
#include "db-queue.h"

/* Operations */
typedef enum {
	HASH_GET, HASH_PUT, HASH_PUTNEW, HASH_DELETE, HASH_FIRST, HASH_NEXT
} ACTION;

/* cursor structure */
typedef struct cursor_t {
	TAILQ_ENTRY(cursor_t) queue;
	int (*get)	__P((const DB *, struct cursor_t *, DBT *, DBT *, \
			     u_int32_t));
	int (*delete) __P((const DB *, struct cursor_t *, u_int32_t));
	db_pgno_t	bucket;
	db_pgno_t	pgno;
	indx_t	ndx;
	indx_t	pgndx;
	u_int16_t *pagep;
	void *internal;
} CURSOR;


#define IS_BUCKET(X)	((X) & BUF_BUCKET)
#define IS_VALID(X)     (!((X) & BUF_INVALID))

/* Hash Table Information */
typedef struct hashhdr {	/* Disk resident portion */
	int32_t	magic;		/* Magic NO for hash tables */
	int32_t	version;	/* Version ID */
	int32_t	lorder;		/* Byte Order */
	u_int32_t	bsize;	/* Bucket/Page Size */
	int32_t	bshift;		/* Bucket shift */
	int32_t	ovfl_point;	/* Where overflow pages are being allocated */
	u_int32_t	last_freed;	/* Last overflow page freed */
	u_int32_t	max_bucket;	/* ID of Maximum bucket in use */
	u_int32_t	high_mask;	/* Mask to modulo into entire table */
	u_int32_t	low_mask;	/* Mask to modulo into lower half of table */
	u_int32_t	ffactor;	/* Fill factor */
	int32_t	nkeys;		/* Number of keys in hash table */
	u_int32_t	hdrpages;	/* Size of table header */
	u_int32_t	h_charkey;	/* value of hash(CHARKEY) */
#define NCACHED	32		/* number of bit maps and spare points */
	u_int32_t	spares[NCACHED];/* spare pages for overflow */
	u_int16_t	bitmaps[NCACHED];	/* address of overflow page bitmaps */
} HASHHDR;

typedef struct htab {		/* Memory resident data structure */
	TAILQ_HEAD(_cursor_queue, cursor_t) curs_queue;
	HASHHDR hdr;		/* Header */
	u_int32_t (*hash) __P((const void *, size_t)); /* Hash Function */
	int32_t	flags;		/* Flag values */
	int32_t	fp;		/* File pointer */
	const char *fname;        	/* File path */
	u_int8_t *bigdata_buf;	/* Temporary Buffer for BIG data */
	u_int8_t *bigkey_buf;	/* Temporary Buffer for BIG keys */
	u_int16_t  *split_buf;	/* Temporary buffer for splits */
	CURSOR	*seq_cursor;	/* Cursor used for hash_seq */
	int32_t	local_errno;	/* Error Number -- for DBM compatibility */
	int32_t	new_file;	/* Indicates if fd is backing store or no */
	int32_t	save_file;	/* Indicates whether we need to flush file at
				 * exit */
	u_int32_t *mapp[NCACHED];/* Pointers to page maps */
	int32_t	nmaps;		/* Initial number of bitmaps */
	MPOOL	*mp;		/* mpool for buffer management */
} HTAB;

/*
 * Constants
 */
#define	MAX_BSIZE		65536		/* 2^16 */
#define MIN_BUFFERS		6
#define MINHDRSIZE		512
#define DEF_CACHESIZE	65536
#define DEF_BUCKET_SHIFT	12		/* log2(BUCKET) */
#define DEF_BUCKET_SIZE		(1<<DEF_BUCKET_SHIFT)
#define DEF_SEGSIZE_SHIFT	8		/* log2(SEGSIZE)	 */
#define DEF_SEGSIZE		(1<<DEF_SEGSIZE_SHIFT)
#define DEF_DIRSIZE		256
#define DEF_FFACTOR		65536
#define MIN_FFACTOR		4
#define SPLTMAX			8
#define CHARKEY			"%$sniglet^&"
#define NUMKEY			1038583
#define BYTE_SHIFT		3
#define INT32_T_TO_BYTE		2
#define INT32_T_BYTE_SHIFT		5
#define ALL_SET			((u_int32_t)0xFFFFFFFF)
#define ALL_CLEAR		0

#define PTROF(X)	((BUFHEAD *)((ptr_t)(X)&~0x3))
#define ISMOD(X)	((ptr_t)(X)&0x1)
#define DOMOD(X)	((X) = (int8_t *)((ptr_t)(X)|0x1))
#define ISDISK(X)	((ptr_t)(X)&0x2)
#define DODISK(X)	((X) = (int8_t *)((ptr_t)(X)|0x2))

#define BITS_PER_MAP	32

/* Given the address of the beginning of a big map, clear/set the nth bit */
#define CLRBIT(A, N)	((A)[(N)/BITS_PER_MAP] &= ~(1<<((N)%BITS_PER_MAP)))
#define SETBIT(A, N)	((A)[(N)/BITS_PER_MAP] |= (1<<((N)%BITS_PER_MAP)))
#define ISSET(A, N)	((A)[(N)/BITS_PER_MAP] & (1<<((N)%BITS_PER_MAP)))

/* Overflow management */
/*
 * Overflow page numbers are allocated per split point.  At each doubling of
 * the table, we can allocate extra pages.  So, an overflow page number has
 * the top 5 bits indicate which split point and the lower 11 bits indicate
 * which page at that split point is indicated (pages within split points are
 * numberered starting with 1).
 */

#define SPLITSHIFT	11
#define SPLITMASK	0x7FF
#define SPLITNUM(N)	(((u_int32_t)(N)) >> SPLITSHIFT)
#define OPAGENUM(N)	((N) & SPLITMASK)
#define	OADDR_OF(S,O)	((u_int32_t)((u_int32_t)(S) << SPLITSHIFT) + (O))

#define BUCKET_TO_PAGE(B) \
	((B) + hashp->hdr.hdrpages + ((B) \
	    ? hashp->hdr.spares[__log2((B)+1)-1] : 0))
#define OADDR_TO_PAGE(B) 	\
	(BUCKET_TO_PAGE ( (1 << SPLITNUM((B))) -1 ) + OPAGENUM((B)))

#define POW2(N)  (1 << (N))

#define MAX_PAGES(H) (DB_OFF_T_MAX / (H)->hdr.bsize)

/* Shorthands for accessing structure */
#define METADATA_PGNO 0
#define SPLIT_PGNO 0xFFFF

typedef struct item_info {
	db_pgno_t 		pgno;
	db_pgno_t		bucket;
	indx_t		ndx;
	indx_t		pgndx;
	u_int8_t	status;
	u_int32_t	seek_size;
	db_pgno_t		seek_found_page;
	indx_t		key_off;
	indx_t		data_off;
	u_int8_t	caused_expand;
} ITEM_INFO;


#define	ITEM_ERROR	0
#define	ITEM_OK		1
#define	ITEM_NO_MORE	2

#define	ITEM_GET_FIRST	0
#define	ITEM_GET_NEXT	1
#define	ITEM_GET_RESET	2
#define	ITEM_GET_DONE	3
#define	ITEM_GET_N	4

#define	UNKNOWN		0xffffffff		/* for num_items */
#define	NO_EXPAND	0xfffffffe
