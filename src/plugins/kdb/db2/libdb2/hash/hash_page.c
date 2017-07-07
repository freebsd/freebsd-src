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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)hash_page.c	8.11 (Berkeley) 11/7/95";
#endif /* LIBC_SCCS and not lint */

/*
 * PACKAGE:  hashing
 *
 * DESCRIPTION:
 *      Page manipulation for hashing package.
 *
 * ROUTINES:
 *
 * External
 *      __get_page
 *      __add_ovflpage
 * Internal
 *      overflow_page
 *      open_temp
 */

#include <sys/types.h>

#ifdef DEBUG
#include <assert.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "db-int.h"
#include "hash.h"
#include "page.h"
#include "extern.h"

static int32_t	 add_bigptr __P((HTAB *, ITEM_INFO *, indx_t));
static u_int32_t *fetch_bitmap __P((HTAB *, int32_t));
static u_int32_t first_free __P((u_int32_t));
static indx_t	 next_realkey __P((PAGE16 *, indx_t));
static u_int16_t overflow_page __P((HTAB *));
static void	 page_init __P((HTAB *, PAGE16 *, db_pgno_t, u_int8_t));
static indx_t	 prev_realkey __P((PAGE16 *, indx_t));
static void	 putpair __P((PAGE8 *, const DBT *, const DBT *));
static void	 swap_page_header_in __P((PAGE16 *));
static void	 swap_page_header_out __P((PAGE16 *));

#ifdef DEBUG_SLOW
static void	 account_page(HTAB *, db_pgno_t, int);
#endif

u_int32_t
__get_item(hashp, cursorp, key, val, item_info)
	HTAB *hashp;
	CURSOR *cursorp;
	DBT *key, *val;
	ITEM_INFO *item_info;
{
	db_pgno_t next_pgno;
	int32_t i;

	/* Check if we need to get a page. */
	if (!cursorp->pagep) {
		if (cursorp->pgno == INVALID_PGNO) {
			cursorp->pagep =
			    __get_page(hashp, cursorp->bucket, A_BUCKET);
			cursorp->pgno = ADDR(cursorp->pagep);
			cursorp->ndx = 0;
			cursorp->pgndx = 0;
		} else
			cursorp->pagep =
			    __get_page(hashp, cursorp->pgno, A_RAW);
		if (!cursorp->pagep) {
			item_info->status = ITEM_ERROR;
			return (-1);
		}
	}
	if (item_info->seek_size &&
	    FREESPACE(cursorp->pagep) > item_info->seek_size)
		item_info->seek_found_page = cursorp->pgno;

	if (cursorp->pgndx == NUM_ENT(cursorp->pagep)) {
		/* Fetch next page. */
		if (NEXT_PGNO(cursorp->pagep) == INVALID_PGNO) {
			item_info->status = ITEM_NO_MORE;
			return (-1);
		}
		next_pgno = NEXT_PGNO(cursorp->pagep);
		cursorp->pgndx = 0;
		__put_page(hashp, cursorp->pagep, A_RAW, 0);
		cursorp->pagep = __get_page(hashp, next_pgno, A_RAW);
		if (!cursorp->pagep) {
			item_info->status = ITEM_ERROR;
			return (-1);
		}
		cursorp->pgno = next_pgno;
	}
	if (KEY_OFF(cursorp->pagep, cursorp->pgndx) != BIGPAIR) {
		if ((i = prev_realkey(cursorp->pagep, cursorp->pgndx)) ==
		    cursorp->pgndx)
			key->size = hashp->hdr.bsize -
			    KEY_OFF(cursorp->pagep, cursorp->pgndx);
		else
			key->size = DATA_OFF(cursorp->pagep, i) -
			    KEY_OFF(cursorp->pagep, cursorp->pgndx);
	}

	/*
	 * All of this information will be set incorrectly for big keys, but
	 * it will be ignored anyway.
	 */
	val->size = KEY_OFF(cursorp->pagep, cursorp->pgndx) -
	    DATA_OFF(cursorp->pagep, cursorp->pgndx);
	key->data = KEY(cursorp->pagep, cursorp->pgndx);
	val->data = DATA(cursorp->pagep, cursorp->pgndx);
	item_info->pgno = cursorp->pgno;
	item_info->bucket = cursorp->bucket;
	item_info->ndx = cursorp->ndx;
	item_info->pgndx = cursorp->pgndx;
	item_info->key_off = KEY_OFF(cursorp->pagep, cursorp->pgndx);
	item_info->data_off = DATA_OFF(cursorp->pagep, cursorp->pgndx);
	item_info->status = ITEM_OK;

	return (0);
}

u_int32_t
__get_item_reset(hashp, cursorp)
	HTAB *hashp;
	CURSOR *cursorp;
{
	if (cursorp->pagep)
		__put_page(hashp, cursorp->pagep, A_RAW, 0);
	cursorp->pagep = NULL;
	cursorp->bucket = -1;
	cursorp->ndx = 0;
	cursorp->pgndx = 0;
	cursorp->pgno = INVALID_PGNO;
	return (0);
}

u_int32_t
__get_item_done(hashp, cursorp)
	HTAB *hashp;
	CURSOR *cursorp;
{
	if (cursorp->pagep)
		__put_page(hashp, cursorp->pagep, A_RAW, 0);
	cursorp->pagep = NULL;

	/*
	 * We don't throw out the page number since we might want to
	 * continue getting on this page.
	 */
	return (0);
}

u_int32_t
__get_item_first(hashp, cursorp, key, val, item_info)
	HTAB *hashp;
	CURSOR *cursorp;
	DBT *key, *val;
	ITEM_INFO *item_info;
{
	__get_item_reset(hashp, cursorp);
	cursorp->bucket = 0;
	return (__get_item_next(hashp, cursorp, key, val, item_info));
}

/*
 * Returns a pointer to key/data pair on a page.  In the case of bigkeys,
 * just returns the page number and index of the bigkey pointer pair.
 */
u_int32_t
__get_item_next(hashp, cursorp, key, val, item_info)
	HTAB *hashp;
	CURSOR *cursorp;
	DBT *key, *val;
	ITEM_INFO *item_info;
{
	int status;

	status = __get_item(hashp, cursorp, key, val, item_info);
	cursorp->ndx++;
	cursorp->pgndx++;
	return (status);
}

/*
 * Put a non-big pair on a page.
 */
static void
putpair(p, key, val)
	PAGE8 *p;
	const DBT *key, *val;
{
	u_int16_t *pagep, n, off;

	pagep = (PAGE16 *)(void *)p;

	/* Items on the page are 0-indexed. */
	n = NUM_ENT(pagep);
	off = OFFSET(pagep) - key->size + 1;
	memmove(p + off, key->data, key->size);
	KEY_OFF(pagep, n) = off;

	off -= val->size;
	memmove(p + off, val->data, val->size);
	DATA_OFF(pagep, n) = off;

	/* Adjust page info. */
	NUM_ENT(pagep) = n + 1;
	OFFSET(pagep) = off - 1;
}

/*
 * Returns the index of the next non-bigkey pair after n on the page.
 * Returns -1 if there are no more non-big things on the page.
 */
static indx_t
#ifdef __STDC__
next_realkey(PAGE16 * pagep, indx_t n)
#else
next_realkey(pagep, n)
	PAGE16 *pagep;
	u_int32_t n;
#endif
{
	indx_t i;

	for (i = n + 1; i < NUM_ENT(pagep); i++)
		if (KEY_OFF(pagep, i) != BIGPAIR)
			return (i);
	return (-1);
}

/*
 * Returns the index of the previous non-bigkey pair after n on the page.
 * Returns n if there are no previous non-big things on the page.
 */
static indx_t
#ifdef __STDC__
prev_realkey(PAGE16 * pagep, indx_t n)
#else
prev_realkey(pagep, n)
	PAGE16 *pagep;
	u_int32_t n;
#endif
{
	int32_t i;

	/* Need a signed value to do the compare properly. */
	for (i = n - 1; i > -1; i--)
		if (KEY_OFF(pagep, i) != BIGPAIR)
			return (i);
	return (n);
}

/*
 * Returns:
 *       0 OK
 *      -1 error
 */
extern int32_t
__delpair(hashp, cursorp, item_info)
	HTAB *hashp;
	CURSOR *cursorp;
	ITEM_INFO *item_info;
{
	PAGE16 *pagep;
	indx_t ndx;
	short check_ndx;
	int16_t delta, len, next_key;
	int32_t n;
	u_int8_t *src, *dest;

	ndx = cursorp->pgndx;
	if (!cursorp->pagep) {
		pagep = __get_page(hashp, cursorp->pgno, A_RAW);
		if (!pagep)
			return (-1);
		/*
		 * KLUGE: pgndx has gone one too far, because cursor points
		 * to the _next_ item.  Use pgndx - 1.
		 */
		--ndx;
	} else
		pagep = cursorp->pagep;
#ifdef DEBUG
	assert(ADDR(pagep) == cursorp->pgno);
#endif

	if (KEY_OFF(pagep, ndx) == BIGPAIR) {
		delta = 0;
		__big_delete(hashp, pagep, ndx);
	} else {
		/*
		 * Compute "delta", the amount we have to shift all of the
		 * offsets.  To find the delta, we need to make sure that
		 * we aren't looking at the DATA_OFF of a big/keydata pair.
		 */
		for (check_ndx = (short)(ndx - 1);
		    check_ndx >= 0 && KEY_OFF(pagep, check_ndx) == BIGPAIR;
		    check_ndx--);
		if (check_ndx < 0)
			delta = hashp->hdr.bsize - DATA_OFF(pagep, ndx);
		else
			delta =
			    DATA_OFF(pagep, check_ndx) - DATA_OFF(pagep, ndx);

		/*
		 * The hard case: we want to remove something other than
		 * the last item on the page.  We need to shift data and
		 * offsets down.
		 */
		if (ndx != NUM_ENT(pagep) - 1) {
			/*
			 * Move the data: src is the address of the last data
			 * item on the page.
			 */
			src = (u_int8_t *)pagep + OFFSET(pagep) + 1;
			/*
			 * Length is the distance between where to start
			 * deleting and end of the data on the page.
			 */
			len = DATA_OFF(pagep, ndx) - (OFFSET(pagep) + 1);
			/*
			 * Dest is the location of the to-be-deleted item
			 * occupied - length.
			 */
			if (check_ndx < 0)
				dest =
				    (u_int8_t *)pagep + hashp->hdr.bsize - len;
			else
				dest = (u_int8_t *)pagep +
				    DATA_OFF(pagep, (check_ndx)) - len;
			memmove(dest, src, len);
		}
	}

	/* Adjust the offsets. */
	for (n = ndx; n < NUM_ENT(pagep) - 1; n++)
		if (KEY_OFF(pagep, (n + 1)) != BIGPAIR) {
			next_key = next_realkey(pagep, n);
#ifdef DEBUG
			assert(next_key != -1);
#endif
			KEY_OFF(pagep, n) = KEY_OFF(pagep, (n + 1)) + delta;
			DATA_OFF(pagep, n) = DATA_OFF(pagep, (n + 1)) + delta;
		} else {
			KEY_OFF(pagep, n) = KEY_OFF(pagep, (n + 1));
			DATA_OFF(pagep, n) = DATA_OFF(pagep, (n + 1));
		}

	/* Adjust page metadata. */
	OFFSET(pagep) = OFFSET(pagep) + delta;
	NUM_ENT(pagep) = NUM_ENT(pagep) - 1;

	--hashp->hdr.nkeys;

	/* Is this page now an empty overflow page?  If so, free it. */
	if (TYPE(pagep) == HASH_OVFLPAGE && NUM_ENT(pagep) == 0) {
		PAGE16 *empty_page;
		db_pgno_t to_find, next_pgno, link_page;

		/*
		 * We need to go back to the first page in the chain and
		 * look for this page so that we can update the previous
		 * page's NEXT_PGNO field.
		 */
		to_find = ADDR(pagep);
		empty_page = pagep;
		link_page = NEXT_PGNO(empty_page);
		pagep = __get_page(hashp, item_info->bucket, A_BUCKET);
		if (!pagep)
			return (-1);
		while (NEXT_PGNO(pagep) != to_find) {
			next_pgno = NEXT_PGNO(pagep);
#ifdef DEBUG
			assert(next_pgno != INVALID_PGNO);
#endif
			__put_page(hashp, pagep, A_RAW, 0);
			pagep = __get_page(hashp, next_pgno, A_RAW);
			if (!pagep)
				return (-1);
		}

		/*
		 * At this point, pagep should point to the page before the
		 * page to be deleted.
		 */
		NEXT_PGNO(pagep) = link_page;
		if (item_info->pgno == to_find) {
			item_info->pgno = ADDR(pagep);
			item_info->pgndx = NUM_ENT(pagep);
			item_info->seek_found_page = ADDR(pagep);
		}
		__delete_page(hashp, empty_page, A_OVFL);
	}
	__put_page(hashp, pagep, A_RAW, 1);

	return (0);
}

extern int32_t
__split_page(hashp, obucket, nbucket)
	HTAB *hashp;
	u_int32_t obucket, nbucket;
{
	DBT key, val;
	ITEM_INFO old_ii, new_ii;
	PAGE16 *old_pagep, *temp_pagep;
	db_pgno_t next_pgno;
	int32_t off;
	u_int16_t n;
	int8_t base_page;

	off = hashp->hdr.bsize;
	old_pagep = __get_page(hashp, obucket, A_BUCKET);

	base_page = 1;

	temp_pagep = hashp->split_buf;
	memcpy(temp_pagep, old_pagep, hashp->hdr.bsize);

	page_init(hashp, old_pagep, ADDR(old_pagep), HASH_PAGE);
	__put_page(hashp, old_pagep, A_RAW, 1);

	old_ii.pgno = BUCKET_TO_PAGE(obucket);
	new_ii.pgno = BUCKET_TO_PAGE(nbucket);
	old_ii.bucket = obucket;
	new_ii.bucket = nbucket;
	old_ii.seek_found_page = new_ii.seek_found_page = 0;

	while (temp_pagep != 0) {
		off = hashp->hdr.bsize;
		for (n = 0; n < NUM_ENT(temp_pagep); n++) {
			if (KEY_OFF(temp_pagep, n) == BIGPAIR) {
				__get_bigkey(hashp, temp_pagep, n, &key);
				if (__call_hash(hashp,
				    key.data, key.size) == obucket)
					add_bigptr(hashp, &old_ii,
					    DATA_OFF(temp_pagep, n));
				else
					add_bigptr(hashp, &new_ii,
					    DATA_OFF(temp_pagep, n));
			} else {
				key.size = off - KEY_OFF(temp_pagep, n);
				key.data = KEY(temp_pagep, n);
				off = KEY_OFF(temp_pagep, n);
				val.size = off - DATA_OFF(temp_pagep, n);
				val.data = DATA(temp_pagep, n);
				if (__call_hash(hashp,
				    key.data, key.size) == obucket)
					__addel(hashp, &old_ii, &key, &val,
					    NO_EXPAND, 1);
				else
					__addel(hashp, &new_ii, &key, &val,
					    NO_EXPAND, 1);
				off = DATA_OFF(temp_pagep, n);
			}
		}
		next_pgno = NEXT_PGNO(temp_pagep);

		/* Clear temp_page; if it's an overflow page, free it. */
		if (!base_page)
			__delete_page(hashp, temp_pagep, A_OVFL);
		else
			base_page = 0;
		if (next_pgno != INVALID_PGNO)
			temp_pagep = __get_page(hashp, next_pgno, A_RAW);
		else
			break;
	}
	return (0);
}

/*
 * Add the given pair to the page.
 *
 *
 * Returns:
 *       0 ==> OK
 *	-1 ==> failure
 */
extern  int32_t
#ifdef __STDC__
__addel(HTAB *hashp, ITEM_INFO *item_info, const DBT *key, const DBT *val,
    u_int32_t num_items, const u_int8_t expanding)
#else
__addel(hashp, item_info, key, val, num_items, expanding)
	HTAB *hashp;
	ITEM_INFO *item_info;
	const DBT *key, *val;
	u_int32_t num_items;
	const u_int32_t expanding;
#endif
{
	PAGE16 *pagep;
	int32_t do_expand;
	db_pgno_t next_pgno;

	do_expand = 0;

	pagep = __get_page(hashp,
	    item_info->seek_found_page != 0 ?
	    item_info->seek_found_page : item_info->pgno, A_RAW);
	if (!pagep)
		return (-1);

	/* Advance to first page in chain with room for item. */
	while (NUM_ENT(pagep) && NEXT_PGNO(pagep) != INVALID_PGNO) {
		/*
		 * This may not be the end of the chain, but the pair may fit
		 * anyway.
		 */
		if (ISBIG(PAIRSIZE(key, val), hashp) && BIGPAIRFITS(pagep))
			break;
		if (PAIRFITS(pagep, key, val))
			break;
		next_pgno = NEXT_PGNO(pagep);
		__put_page(hashp, pagep, A_RAW, 0);
		pagep = (PAGE16 *)__get_page(hashp, next_pgno, A_RAW);
		if (!pagep)
			return (-1);
	}

	if ((ISBIG(PAIRSIZE(key, val), hashp) &&
	     !BIGPAIRFITS(pagep)) ||
	    (!ISBIG(PAIRSIZE(key, val), hashp) &&
	     !PAIRFITS(pagep, key, val))) {
		do_expand = 1;
		pagep = __add_ovflpage(hashp, pagep);
		if (!pagep)
			return (-1);

		if ((ISBIG(PAIRSIZE(key, val), hashp) &&
		     !BIGPAIRFITS(pagep)) ||
		    (!ISBIG(PAIRSIZE(key, val), hashp) &&
		     !PAIRFITS(pagep, key, val))) {
			__put_page(hashp, pagep, A_RAW, 0);
			return (-1);
		}
	}

	/* At this point, we know the page fits, so we just add it */

	if (ISBIG(PAIRSIZE(key, val), hashp)) {
		if (__big_insert(hashp, pagep, key, val))
			return (-1);
	} else {
		putpair((PAGE8 *)pagep, key, val);
	}

	/*
	 * For splits, we are going to update item_info's page number
	 * field, so that we can easily return to the same page the
	 * next time we come in here.  For other operations, this shouldn't
	 * matter, since adds are the last thing that happens before we
	 * return to the user program.
	 */
	item_info->pgno = ADDR(pagep);

	if (!expanding)
		hashp->hdr.nkeys++;

	/* Kludge: if this is a big page, then it's already been put. */
	if (!ISBIG(PAIRSIZE(key, val), hashp))
		__put_page(hashp, pagep, A_RAW, 1);

	if (expanding)
		item_info->caused_expand = 0;
	else
		switch (num_items) {
		case NO_EXPAND:
			item_info->caused_expand = 0;
			break;
		case UNKNOWN:
			item_info->caused_expand |=
			    (hashp->hdr.nkeys / hashp->hdr.max_bucket) >
			    hashp->hdr.ffactor ||
			    item_info->pgndx > hashp->hdr.ffactor;
			break;
		default:
			item_info->caused_expand =
			    num_items > hashp->hdr.ffactor ? 1 : do_expand;
			break;
		}
	return (0);
}

/*
 * Special __addel used in big splitting; this one just puts the pointer
 * to an already-allocated big page in the appropriate bucket.
 */
static int32_t
#ifdef __STDC__
add_bigptr(HTAB * hashp, ITEM_INFO * item_info, indx_t big_pgno)
#else
add_bigptr(hashp, item_info, big_pgno)
	HTAB *hashp;
	ITEM_INFO *item_info;
	u_int32_t big_pgno;
#endif
{
	PAGE16 *pagep;
	db_pgno_t next_pgno;

	pagep = __get_page(hashp, item_info->bucket, A_BUCKET);
	if (!pagep)
		return (-1);

	/*
	 * Note: in __addel(), we used item_info->pgno for the beginning of
	 * our search for space.  Now, we use item_info->bucket, since we
	 * know that the space required by a big pair on the base page is
	 * quite small, and we may very well find that space early in the
	 * chain.
	 */

	/* Find first page in chain that has space for a big pair. */
	while (NUM_ENT(pagep) && (NEXT_PGNO(pagep) != INVALID_PGNO)) {
		if (BIGPAIRFITS(pagep))
			break;
		next_pgno = NEXT_PGNO(pagep);
		__put_page(hashp, pagep, A_RAW, 0);
		pagep = __get_page(hashp, next_pgno, A_RAW);
		if (!pagep)
			return (-1);
	}
	if (!BIGPAIRFITS(pagep)) {
		pagep = __add_ovflpage(hashp, pagep);
		if (!pagep)
			return (-1);
#ifdef DEBUG
		assert(BIGPAIRFITS(pagep));
#endif
	}
	KEY_OFF(pagep, NUM_ENT(pagep)) = BIGPAIR;
	DATA_OFF(pagep, NUM_ENT(pagep)) = big_pgno;
	NUM_ENT(pagep) = NUM_ENT(pagep) + 1;

	__put_page(hashp, pagep, A_RAW, 1);

	return (0);
}

/*
 *
 * Returns:
 *      pointer on success
 *      NULL on error
 */
extern PAGE16 *
__add_ovflpage(hashp, pagep)
	HTAB *hashp;
	PAGE16 *pagep;
{
	PAGE16 *new_pagep;
	u_int16_t ovfl_num;

	/* Check if we are dynamically determining the fill factor. */
	if (hashp->hdr.ffactor == DEF_FFACTOR) {
		hashp->hdr.ffactor = NUM_ENT(pagep) >> 1;
		if (hashp->hdr.ffactor < MIN_FFACTOR)
			hashp->hdr.ffactor = MIN_FFACTOR;
	}
	ovfl_num = overflow_page(hashp);
	if (!ovfl_num)
		return (NULL);

	if (__new_page(hashp, (u_int32_t)ovfl_num, A_OVFL) != 0)
		return (NULL);

	if (!ovfl_num || !(new_pagep = __get_page(hashp, ovfl_num, A_OVFL)))
		return (NULL);

	NEXT_PGNO(pagep) = (db_pgno_t)OADDR_TO_PAGE(ovfl_num);
	TYPE(new_pagep) = HASH_OVFLPAGE;

	__put_page(hashp, pagep, A_RAW, 1);

#ifdef HASH_STATISTICS
	hash_overflows++;
#endif
	return (new_pagep);
}

/*
 *
 * Returns:
 *      pointer on success
 *      NULL on error
 */
extern PAGE16 *
#ifdef __STDC__
__add_bigpage(HTAB * hashp, PAGE16 * pagep, indx_t ndx, const u_int8_t
    is_basepage)
#else
__add_bigpage(hashp, pagep, ndx, is_basepage)
	HTAB *hashp;
	PAGE16 *pagep;
	u_int32_t ndx;
	const u_int32_t is_basepage;
#endif
{
	PAGE16 *new_pagep;
	u_int16_t ovfl_num;

	ovfl_num = overflow_page(hashp);
	if (!ovfl_num)
		return (NULL);

	if (__new_page(hashp, (u_int32_t)ovfl_num, A_OVFL) != 0)
		return (NULL);

	if (!ovfl_num || !(new_pagep = __get_page(hashp, ovfl_num, A_OVFL)))
		return (NULL);

	if (is_basepage) {
		KEY_OFF(pagep, ndx) = BIGPAIR;
		DATA_OFF(pagep, ndx) = (indx_t)ovfl_num;
	} else
		NEXT_PGNO(pagep) = ADDR(new_pagep);

	__put_page(hashp, pagep, A_RAW, 1);

	TYPE(new_pagep) = HASH_BIGPAGE;

#ifdef HASH_STATISTICS
	hash_bigpages++;
#endif
	return (new_pagep);
}

static void
#ifdef __STDC__
page_init(HTAB * hashp, PAGE16 * pagep, db_pgno_t pgno, u_int8_t type)
#else
page_init(hashp, pagep, pgno, type)
	HTAB *hashp;
	PAGE16 *pagep;
	db_pgno_t pgno;
	u_int32_t type;
#endif
{
	NUM_ENT(pagep) = 0;
	PREV_PGNO(pagep) = NEXT_PGNO(pagep) = INVALID_PGNO;
	TYPE(pagep) = type;
	OFFSET(pagep) = hashp->hdr.bsize - 1;
	/*
	 * Note: since in the current version ADDR(pagep) == PREV_PGNO(pagep),
	 * make sure that ADDR(pagep) is set after resetting PREV_PGNO(pagep).
	 * We reset PREV_PGNO(pagep) just in case the macros are changed.
	 */
	ADDR(pagep) = pgno;

	return;
}

int32_t
__new_page(hashp, addr, addr_type)
	HTAB *hashp;
	u_int32_t addr;
	int32_t addr_type;
{
	db_pgno_t paddr;
	PAGE16 *pagep;

	switch (addr_type) {		/* Convert page number. */
	case A_BUCKET:
		paddr = BUCKET_TO_PAGE(addr);
		break;
	case A_OVFL:
	case A_BITMAP:
		paddr = OADDR_TO_PAGE(addr);
		break;
	default:
		paddr = addr;
		break;
	}
	pagep = mpool_new(hashp->mp, &paddr, MPOOL_PAGE_REQUEST);
	if (!pagep)
		return (-1);
#if DEBUG_SLOW
	account_page(hashp, paddr, 1);
#endif

	if (addr_type != A_BITMAP)
		page_init(hashp, pagep, paddr, HASH_PAGE);

	__put_page(hashp, pagep, addr_type, 1);

	return (0);
}

int32_t
__delete_page(hashp, pagep, page_type)
	HTAB *hashp;
	PAGE16 *pagep;
	int32_t page_type;
{
	if (page_type == A_OVFL)
		__free_ovflpage(hashp, pagep);
	return (mpool_delete(hashp->mp, pagep));
}

static u_int8_t
is_bitmap_pgno(hashp, pgno)
	HTAB *hashp;
	db_pgno_t pgno;
{
	int32_t i;

	for (i = 0; i < hashp->nmaps; i++)
		if (OADDR_TO_PAGE(hashp->hdr.bitmaps[i]) == pgno)
			return (1);
	return (0);
}

void
__pgin_routine(pg_cookie, pgno, page)
	void *pg_cookie;
	db_pgno_t pgno;
	void *page;
{
	HTAB *hashp;
	PAGE16 *pagep;
	int32_t max, i;

	pagep = (PAGE16 *)page;
	hashp = (HTAB *)pg_cookie;

	/*
	 * There are the following cases for swapping:
	 * 0) New page that may be unitialized.
	 * 1) Bucket page or overflow page.  Either swap
	 *	the header or initialize the page.
	 * 2) Bitmap page.  Swap the whole page!
	 * 3) Header pages.  Not handled here; these are written directly
	 *    to the file.
	 */

	if (NUM_ENT(pagep) == 0 && NEXT_PGNO(pagep) == 0 &&
	    !is_bitmap_pgno(hashp, pgno)) {
		/* XXX check for !0 LSN */
		page_init(hashp, pagep, pgno, HASH_PAGE);
		return;
	}

	if (hashp->hdr.lorder == DB_BYTE_ORDER)
		return;
	if (is_bitmap_pgno(hashp, pgno)) {
		max = hashp->hdr.bsize >> 2;	/* divide by 4 bytes */
		for (i = 0; i < max; i++)
			M_32_SWAP(((int32_t *)(void *)pagep)[i]);
	} else
		swap_page_header_in(pagep);
}

void
__pgout_routine(pg_cookie, pgno, page)
	void *pg_cookie;
	db_pgno_t pgno;
	void *page;
{
	HTAB *hashp;
	PAGE16 *pagep;
	int32_t i, max;

	pagep = (PAGE16 *)page;
	hashp = (HTAB *)pg_cookie;

	/*
	 * There are the following cases for swapping:
	 * 1) Bucket page or overflow page.  Just swap the header.
	 * 2) Bitmap page.  Swap the whole page!
	 * 3) Header pages.  Not handled here; these are written directly
	 *    to the file.
	 */

	if (hashp->hdr.lorder == DB_BYTE_ORDER)
		return;
	if (is_bitmap_pgno(hashp, pgno)) {
		max = hashp->hdr.bsize >> 2;	/* divide by 4 bytes */
		for (i = 0; i < max; i++)
			M_32_SWAP(((int32_t *)(void *)pagep)[i]);
	} else
		swap_page_header_out(pagep);
}

/*
 *
 * Returns:
 *       0 ==> OK
 *      -1 ==>failure
 */
extern int32_t
__put_page(hashp, pagep, addr_type, is_dirty)
	HTAB *hashp;
	PAGE16 *pagep;
	int32_t addr_type, is_dirty;
{
#if DEBUG_SLOW
	account_page(hashp,
	    ((BKT *)((char *)pagep - sizeof(BKT)))->pgno, -1);
#endif

	return (mpool_put(hashp->mp, pagep, (is_dirty ? MPOOL_DIRTY : 0)));
}

/*
 * Returns:
 *       0 indicates SUCCESS
 *      -1 indicates FAILURE
 */
extern PAGE16 *
__get_page(hashp, addr, addr_type)
	HTAB *hashp;
	u_int32_t addr;
	int32_t addr_type;
{
	PAGE16 *pagep;
	db_pgno_t paddr;

	switch (addr_type) {			/* Convert page number. */
	case A_BUCKET:
		paddr = BUCKET_TO_PAGE(addr);
		break;
	case A_OVFL:
	case A_BITMAP:
		paddr = OADDR_TO_PAGE(addr);
		break;
	default:
		paddr = addr;
		break;
	}
	pagep = (PAGE16 *)mpool_get(hashp->mp, paddr, 0);

#if DEBUG_SLOW
	account_page(hashp, paddr, 1);
#endif
#ifdef DEBUG
	assert(ADDR(pagep) == paddr || ADDR(pagep) == 0 ||
	    addr_type == A_BITMAP || addr_type == A_HEADER);
#endif

	return (pagep);
}

static void
swap_page_header_in(pagep)
	PAGE16 *pagep;
{
	u_int32_t i;

	/* can leave type and filler alone, since they're 1-byte quantities */

	M_32_SWAP(PREV_PGNO(pagep));
	M_32_SWAP(NEXT_PGNO(pagep));
	M_16_SWAP(NUM_ENT(pagep));
	M_16_SWAP(OFFSET(pagep));

	for (i = 0; i < NUM_ENT(pagep); i++) {
		M_16_SWAP(KEY_OFF(pagep, i));
		M_16_SWAP(DATA_OFF(pagep, i));
	}
}

static void
swap_page_header_out(pagep)
	PAGE16 *pagep;
{
	u_int32_t i;

	for (i = 0; i < NUM_ENT(pagep); i++) {
		M_16_SWAP(KEY_OFF(pagep, i));
		M_16_SWAP(DATA_OFF(pagep, i))
	}

	/* can leave type and filler alone, since they're 1-byte quantities */

	M_32_SWAP(PREV_PGNO(pagep));
	M_32_SWAP(NEXT_PGNO(pagep));
	M_16_SWAP(NUM_ENT(pagep));
	M_16_SWAP(OFFSET(pagep));
}

#define BYTE_MASK	((1 << INT32_T_BYTE_SHIFT) -1)
/*
 * Initialize a new bitmap page.  Bitmap pages are left in memory
 * once they are read in.
 */
extern int32_t
__ibitmap(hashp, pnum, nbits, ndx)
	HTAB *hashp;
	int32_t pnum, nbits, ndx;
{
	u_int32_t *ip;
	int32_t clearbytes, clearints;

	/* make a new bitmap page */
	if (__new_page(hashp, pnum, A_BITMAP) != 0)
		return (1);
	if (!(ip = (u_int32_t *)(void *)__get_page(hashp, pnum, A_BITMAP)))
		return (1);
	hashp->nmaps++;
	clearints = ((nbits - 1) >> INT32_T_BYTE_SHIFT) + 1;
	clearbytes = clearints << INT32_T_TO_BYTE;
	(void)memset((int8_t *)ip, 0, clearbytes);
	(void)memset((int8_t *)ip + clearbytes,
	    0xFF, hashp->hdr.bsize - clearbytes);
	ip[clearints - 1] = ALL_SET << (nbits & BYTE_MASK);
	SETBIT(ip, 0);
	hashp->hdr.bitmaps[ndx] = (u_int16_t)pnum;
	hashp->mapp[ndx] = ip;
	return (0);
}

static u_int32_t
first_free(map)
	u_int32_t map;
{
	u_int32_t i, mask;

	for (mask = 0x1, i = 0; i < BITS_PER_MAP; i++) {
		if (!(mask & map))
			return (i);
		mask = mask << 1;
	}
	return (i);
}

/*
 * returns 0 on error
 */
static u_int16_t
overflow_page(hashp)
	HTAB *hashp;
{
	u_int32_t *freep;
	u_int32_t bit, first_page, free_bit, free_page, i, in_use_bits, j;
	u_int32_t max_free, offset, splitnum;
	u_int16_t addr;
#ifdef DEBUG2
	int32_t tmp1, tmp2;
#endif

	splitnum = hashp->hdr.ovfl_point;
	max_free = hashp->hdr.spares[splitnum];

	free_page = (max_free - 1) >> (hashp->hdr.bshift + BYTE_SHIFT);
	free_bit = (max_free - 1) & ((hashp->hdr.bsize << BYTE_SHIFT) - 1);

	/*
	 * Look through all the free maps to find the first free block.
 	 * The compiler under -Wall will complain that freep may be used
	 * before being set, however, this loop will ALWAYS get executed
	 * at least once, so freep is guaranteed to be set.
	 */
	freep = NULL;
	first_page = hashp->hdr.last_freed >> (hashp->hdr.bshift + BYTE_SHIFT);
	for (i = first_page; i <= free_page; i++) {
		if (!(freep = fetch_bitmap(hashp, i)))
			return (0);
		if (i == free_page)
			in_use_bits = free_bit;
		else
			in_use_bits = (hashp->hdr.bsize << BYTE_SHIFT) - 1;

		if (i == first_page) {
			bit = hashp->hdr.last_freed &
			    ((hashp->hdr.bsize << BYTE_SHIFT) - 1);
			j = bit / BITS_PER_MAP;
			bit = bit & ~(BITS_PER_MAP - 1);
		} else {
			bit = 0;
			j = 0;
		}
		for (; bit <= in_use_bits; j++, bit += BITS_PER_MAP)
			if (freep[j] != ALL_SET)
				goto found;
	}

	/* No Free Page Found */
	hashp->hdr.last_freed = hashp->hdr.spares[splitnum];
	hashp->hdr.spares[splitnum]++;
	offset = hashp->hdr.spares[splitnum] -
	    (splitnum ? hashp->hdr.spares[splitnum - 1] : 0);

#define	OVMSG	"HASH: Out of overflow pages.  Increase page size\n"

	if (offset > SPLITMASK) {
		if (++splitnum >= NCACHED) {
			(void)write(STDERR_FILENO, OVMSG, sizeof(OVMSG) - 1);
			return (0);
		}
		hashp->hdr.ovfl_point = splitnum;
		hashp->hdr.spares[splitnum] = hashp->hdr.spares[splitnum - 1];
		hashp->hdr.spares[splitnum - 1]--;
		offset = 1;
	}
	/* Check if we need to allocate a new bitmap page. */
	if (free_bit == (hashp->hdr.bsize << BYTE_SHIFT) - 1) {
		free_page++;
		if (free_page >= NCACHED) {
			(void)write(STDERR_FILENO, OVMSG, sizeof(OVMSG) - 1);
			return (0);
		}
		/*
		 * This is tricky.  The 1 indicates that you want the new page
		 * allocated with 1 clear bit.  Actually, you are going to
		 * allocate 2 pages from this map.  The first is going to be
		 * the map page, the second is the overflow page we were
		 * looking for.  The __ibitmap routine automatically, sets
		 * the first bit of itself to indicate that the bitmap itself
		 * is in use.  We would explicitly set the second bit, but
		 * don't have to if we tell __ibitmap not to leave it clear
		 * in the first place.
		 */
		if (__ibitmap(hashp,
		    (int32_t)OADDR_OF(splitnum, offset), 1, free_page))
			return (0);
		hashp->hdr.spares[splitnum]++;
#ifdef DEBUG2
		free_bit = 2;
#endif
		offset++;
		if (offset > SPLITMASK) {
			if (++splitnum >= NCACHED) {
				(void)write(STDERR_FILENO,
				    OVMSG, sizeof(OVMSG) - 1);
				return (0);
			}
			hashp->hdr.ovfl_point = splitnum;
			hashp->hdr.spares[splitnum] =
			    hashp->hdr.spares[splitnum - 1];
			hashp->hdr.spares[splitnum - 1]--;
			offset = 0;
		}
	} else {
		/*
		 * Free_bit addresses the last used bit.  Bump it to address
		 * the first available bit.
		 */
		free_bit++;
		SETBIT(freep, free_bit);
	}

	/* Calculate address of the new overflow page */
	addr = OADDR_OF(splitnum, offset);
#ifdef DEBUG2
	(void)fprintf(stderr, "OVERFLOW_PAGE: ADDR: %d BIT: %d PAGE %d\n",
	    addr, free_bit, free_page);
#endif

	if (OADDR_TO_PAGE(addr) > MAX_PAGES(hashp)) {
		(void)write(STDERR_FILENO, OVMSG, sizeof(OVMSG) - 1);
		return (0);
	}
	return (addr);

found:
	bit = bit + first_free(freep[j]);
	SETBIT(freep, bit);
#ifdef DEBUG2
	tmp1 = bit;
	tmp2 = i;
#endif
	/*
	 * Bits are addressed starting with 0, but overflow pages are addressed
	 * beginning at 1. Bit is a bit address number, so we need to increment
	 * it to convert it to a page number.
	 */
	bit = 1 + bit + (i * (hashp->hdr.bsize << BYTE_SHIFT));
	if (bit >= hashp->hdr.last_freed)
		hashp->hdr.last_freed = bit - 1;

	/* Calculate the split number for this page */
	for (i = 0; i < splitnum && (bit > hashp->hdr.spares[i]); i++);
	offset = (i ? bit - hashp->hdr.spares[i - 1] : bit);
	if (offset >= SPLITMASK)
		return (0);	/* Out of overflow pages */
	addr = OADDR_OF(i, offset);
#ifdef DEBUG2
	(void)fprintf(stderr, "OVERFLOW_PAGE: ADDR: %d BIT: %d PAGE %d\n",
	    addr, tmp1, tmp2);
#endif

	if (OADDR_TO_PAGE(addr) > MAX_PAGES(hashp)) {
		(void)write(STDERR_FILENO, OVMSG, sizeof(OVMSG) - 1);
		return (0);
	}
	/* Allocate and return the overflow page */
	return (addr);
}

#ifdef DEBUG
int
bucket_to_page(hashp, n)
	HTAB *hashp;
	int n;
{
	int ret_val;

	ret_val = n + hashp->hdr.hdrpages;
	if (n != 0)
		ret_val += hashp->hdr.spares[__log2(n + 1) - 1];
	return (ret_val);
}

int32_t
oaddr_to_page(hashp, n)
	HTAB *hashp;
	int n;
{
	int ret_val, temp;

	temp = (1 << SPLITNUM(n)) - 1;
	ret_val = bucket_to_page(hashp, temp);
	ret_val += (n & SPLITMASK);

	return (ret_val);
}
#endif /* DEBUG */

static indx_t
page_to_oaddr(hashp, pgno)
	HTAB *hashp;
	db_pgno_t pgno;
{
	int32_t sp, ret_val;

	/*
	 * To convert page number to overflow address:
	 *
	 * 1.  Find a starting split point -- use 0 since there are only
	 *     32 split points.
	 * 2.  Find the split point s.t. 2^sp + hdr.spares[sp] < pgno and
	 *     2^(sp+1) = hdr.spares[sp+1] > pgno.  The overflow address will
	 *     be located at sp.
	 * 3.  return...
	 */
	pgno -= hashp->hdr.hdrpages;
	for (sp = 0; sp < NCACHED - 1; sp++)
		if (POW2(sp) + hashp->hdr.spares[sp] < pgno &&
		    (POW2(sp + 1) + hashp->hdr.spares[sp + 1]) > pgno)
			break;

	ret_val = OADDR_OF(sp + 1,
	    pgno - ((POW2(sp + 1) - 1) + hashp->hdr.spares[sp]));
#ifdef DEBUG
	assert(OADDR_TO_PAGE(ret_val) == (pgno + hashp->hdr.hdrpages));
#endif
	return (ret_val);
}

/*
 * Mark this overflow page as free.
 */
extern void
__free_ovflpage(hashp, pagep)
	HTAB *hashp;
	PAGE16 *pagep;
{
	u_int32_t *freep;
	u_int32_t bit_address, free_page, free_bit;
	u_int16_t addr, ndx;

	addr = page_to_oaddr(hashp, ADDR(pagep));

#ifdef DEBUG2
	(void)fprintf(stderr, "Freeing %d\n", addr);
#endif
	ndx = ((u_int16_t)addr) >> SPLITSHIFT;
	bit_address =
	    (ndx ? hashp->hdr.spares[ndx - 1] : 0) + (addr & SPLITMASK) - 1;
	if (bit_address < hashp->hdr.last_freed)
		hashp->hdr.last_freed = bit_address;
	free_page = (bit_address >> (hashp->hdr.bshift + BYTE_SHIFT));
	free_bit = bit_address & ((hashp->hdr.bsize << BYTE_SHIFT) - 1);

	freep = fetch_bitmap(hashp, free_page);
#ifdef DEBUG
	/*
	 * This had better never happen.  It means we tried to read a bitmap
	 * that has already had overflow pages allocated off it, and we
	 * failed to read it from the file.
	 */
	if (!freep)
		assert(0);
#endif
	CLRBIT(freep, free_bit);
#ifdef DEBUG2
	(void)fprintf(stderr, "FREE_OVFLPAGE: ADDR: %d BIT: %d PAGE %d\n",
	    obufp->addr, free_bit, free_page);
#endif
}

static u_int32_t *
fetch_bitmap(hashp, ndx)
	HTAB *hashp;
	int32_t ndx;
{
	if (ndx >= hashp->nmaps)
		return (NULL);
	if (!hashp->mapp[ndx])
	    hashp->mapp[ndx] = (u_int32_t *)(void *)__get_page(hashp,
	        hashp->hdr.bitmaps[ndx], A_BITMAP);

	return (hashp->mapp[ndx]);
}

#ifdef DEBUG_SLOW
static void
account_page(hashp, pgno, inout)
	HTAB *hashp;
	db_pgno_t pgno;
	int inout;
{
	static struct {
		db_pgno_t pgno;
		int times;
	} list[100];
	static int last;
	int i, j;

	if (inout == -1)			/* XXX: Kluge */
		inout = 0;

	/* Find page in list. */
	for (i = 0; i < last; i++)
		if (list[i].pgno == pgno)
			break;
	/* Not found. */
	if (i == last) {
		list[last].times = inout;
		list[last].pgno = pgno;
		last++;
	}
	list[i].times = inout;
	if (list[i].times == 0) {
		for (j = i; j < last; j++)
			list[j] = list[j + 1];
		last--;
	}
	for (i = 0; i < last; i++, list[i].times++)
		if (list[i].times > 20 && !is_bitmap_pgno(hashp, list[i].pgno))
			(void)fprintf(stderr,
			    "Warning: pg %d has been out for %d times\n",
			    list[i].pgno, list[i].times);
}
#endif /* DEBUG_SLOW */
