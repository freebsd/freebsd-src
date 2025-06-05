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
 *	@(#)page.h	8.4 (Berkeley) 11/7/95
 */

#define HI_MASK 0xFFFF0000
#define LO_MASK (~HI_MASK)

#define HI(N) ((u_int16_t)(((N) & HI_MASK) >> 16))
#define LO(N) ((u_int16_t)((N) & LO_MASK))

/* Constants for big key page overhead information. */
#define NUMSHORTS	0
#define KEYLEN		1
#define DATALEN 	2
#define NEXTPAGE 	3

/*
 * Hash pages store meta-data beginning at the top of the page (offset 0)
 * and key/data values beginning at the bottom of the page (offset pagesize).
 * Fields are always accessed via macros so that we can change the page
 * format without too much pain.  The only changes that will require massive
 * code changes are if we no longer store key/data offsets next to each
 * other (since we use that fact to compute key lengths).  In the accessor
 * macros below, P means a pointer to the page, I means an index of the
 * particular entry being accessed.
 *
 * Hash base page format
 * BYTE ITEM			NBYTES 	TYPE		ACCESSOR MACRO
 * ---- ------------------	------	--------	--------------
 * 0	previous page number 	4	db_pgno_t		PREV_PGNO(P)
 * 4	next page number	4	db_pgno_t		NEXT_PGNO(P)
 * 8	# pairs on page		2	indx_t		NUM_ENT(P)
 * 10	page type		1	u_int8_t	TYPE(P)
 * 11	padding			1	u_int8_t	none
 * 12	highest free byte	2	indx_t		OFFSET(P)
 * 14	key offset 0		2	indx_t		KEY_OFF(P, I)
 * 16	data offset 0		2	indx_t		DATA_OFF(P, I)
 * 18	key  offset 1		2	indx_t		KEY_OFF(P, I)
 * 20	data offset 1		2	indx_t		DATA_OFF(P, I)
 * ...etc...
 */

/* Indices (in bytes) of the beginning of each of these entries */
#define I_PREV_PGNO	 0
#define I_NEXT_PGNO	 4
#define I_ENTRIES	 8
#define I_TYPE		10
#define I_HF_OFFSET	12

/* Overhead is everything prior to the first key/data pair. */
#define PAGE_OVERHEAD	(I_HF_OFFSET + sizeof(indx_t))

/* To allocate a pair, we need room for one key offset and one data offset. */
#define PAIR_OVERHEAD	((sizeof(indx_t) << 1))

/* Use this macro to extract a value of type T from page P at offset O. */
#define REFERENCE(P, T, O)  (((T *)(void *)((u_int8_t *)(void *)(P) + O))[0])

/*
 * Use these macros to access fields on a page; P is a PAGE16 *.
 */
#define NUM_ENT(P)	(REFERENCE((P), indx_t, I_ENTRIES))
#define PREV_PGNO(P)	(REFERENCE((P), db_pgno_t, I_PREV_PGNO))
#define NEXT_PGNO(P)	(REFERENCE((P), db_pgno_t, I_NEXT_PGNO))
#define TYPE(P)		(REFERENCE((P), u_int8_t, I_TYPE))
#define OFFSET(P)	(REFERENCE((P), indx_t, I_HF_OFFSET))
/*
 * We need to store a page's own address on each page (unlike the Btree
 * access method which needs the previous page).  We use the PREV_PGNO
 * field to store our own page number.
 */
#define ADDR(P)		(PREV_PGNO((P)))

/* Extract key/data offsets and data for a given index. */
#define DATA_OFF(P, N) \
	REFERENCE(P, indx_t, PAGE_OVERHEAD + N * PAIR_OVERHEAD + sizeof(indx_t))
#define KEY_OFF(P, N) \
	REFERENCE(P, indx_t, PAGE_OVERHEAD + N * PAIR_OVERHEAD)

#define KEY(P, N)	(((PAGE8 *)(P)) + KEY_OFF((P), (N)))
#define DATA(P, N)	(((PAGE8 *)(P)) + DATA_OFF((P), (N)))

/*
 * Macros used to compute various sizes on a page.
 */
#define	PAIRSIZE(K, D)	(PAIR_OVERHEAD + (K)->size + (D)->size)
#define BIGOVERHEAD	(4 * sizeof(u_int16_t))
#define KEYSIZE(K)	(4 * sizeof(u_int16_t) + (K)->size);
#define OVFLSIZE	(2 * sizeof(u_int16_t))
#define BIGPAGEOVERHEAD (4 * sizeof(u_int16_t))
#define BIGPAGEOFFSET   4
#define BIGPAGESIZE(P)	((P)->BSIZE - BIGPAGEOVERHEAD)

#define PAGE_META(N)	(((N) + 3) * sizeof(u_int16_t))
#define MINFILL 0.75
#define ISBIG(N, P)	(((N) > ((P)->hdr.bsize * MINFILL)) ? 1 : 0)

#define ITEMSIZE(I)    (sizeof(u_int16_t) + (I)->size)

/*
 * Big key/data pages use a different page format.  They have a single
 * key/data "pair" containing the length of the key and data instead
 * of offsets.
 */
#define BIGKEYLEN(P)	(KEY_OFF((P), 0))
#define BIGDATALEN(P)	(DATA_OFF((P), 0))
#define BIGKEY(P)	(((PAGE8 *)(P)) + PAGE_OVERHEAD + PAIR_OVERHEAD)
#define BIGDATA(P) \
	(((PAGE8 *)(P)) + PAGE_OVERHEAD + PAIR_OVERHEAD + KEY_OFF((P), 0))


#define OVFLPAGE	0
#define BIGPAIR		0
#define INVALID_PGNO	0xFFFFFFFF

typedef unsigned short PAGE16;
typedef unsigned char  PAGE8;

#define A_BUCKET	0
#define A_OVFL		1
#define A_BITMAP	2
#define A_RAW		4
#define A_HEADER	5

#define PAIRFITS(P,K,D)	((PAIRSIZE((K),(D))) <= FREESPACE((P)))
#define BIGPAIRFITS(P)	((FREESPACE((P)) >= PAIR_OVERHEAD))
/*
 * Since these are all unsigned, we need to guarantee that we never go
 * negative.  Offset values are 0-based and overheads are one based (i.e.
 * one byte of overhead is 1, not 0), so we need to convert OFFSETs to
 * 1-based counting before subtraction.
 */
#define FREESPACE(P) \
	((OFFSET((P)) + 1 - PAGE_OVERHEAD - (NUM_ENT((P)) * PAIR_OVERHEAD)))

/*
 * Overhead on header pages is just one word -- the length of the
 * header info stored on that page.
 */
#define HEADER_OVERHEAD 4

#define HASH_PAGE	2
#define HASH_BIGPAGE	3
#define HASH_OVFLPAGE	4
