/*
 * array.c - routines for associative arrays.
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992, 1993 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Progamming Language.
 * 
 * GAWK is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * GAWK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GAWK; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Tree walks (``for (iggy in foo)'') and array deletions use expensive
 * linear searching.  So what we do is start out with small arrays and
 * grow them as needed, so that our arrays are hopefully small enough,
 * most of the time, that they're pretty full and we're not looking at
 * wasted space.
 *
 * The decision is made to grow the array if the average chain length is
 * ``too big''. This is defined as the total number of entries in the table
 * divided by the size of the array being greater than some constant.
 */

#define AVG_CHAIN_MAX	10   /* don't want to linear search more than this */

#include "awk.h"

static NODE *assoc_find P((NODE *symbol, NODE *subs, int hash1));
static void grow_table P((NODE *symbol));

NODE *
concat_exp(tree)
register NODE *tree;
{
	register NODE *r;
	char *str;
	char *s;
	size_t len;
	int offset;
	size_t subseplen;
	char *subsep;

	if (tree->type != Node_expression_list)
		return force_string(tree_eval(tree));
	r = force_string(tree_eval(tree->lnode));
	if (tree->rnode == NULL)
		return r;
	subseplen = SUBSEP_node->lnode->stlen;
	subsep = SUBSEP_node->lnode->stptr;
	len = r->stlen + subseplen + 2;
	emalloc(str, char *, len, "concat_exp");
	memcpy(str, r->stptr, r->stlen+1);
	s = str + r->stlen;
	free_temp(r);
	tree = tree->rnode;
	while (tree) {
		if (subseplen == 1)
			*s++ = *subsep;
		else {
			memcpy(s, subsep, subseplen+1);
			s += subseplen;
		}
		r = force_string(tree_eval(tree->lnode));
		len += r->stlen + subseplen;
		offset = s - str;
		erealloc(str, char *, len, "concat_exp");
		s = str + offset;
		memcpy(s, r->stptr, r->stlen+1);
		s += r->stlen;
		free_temp(r);
		tree = tree->rnode;
	}
	r = make_str_node(str, s - str, ALREADY_MALLOCED);
	r->flags |= TEMP;
	return r;
}

/* Flush all the values in symbol[] before doing a split() */
void
assoc_clear(symbol)
NODE *symbol;
{
	int i;
	NODE *bucket, *next;

	if (symbol->var_array == 0)
		return;
	for (i = 0; i < symbol->array_size; i++) {
		for (bucket = symbol->var_array[i]; bucket; bucket = next) {
			next = bucket->ahnext;
			unref(bucket->ahname);
			unref(bucket->ahvalue);
			freenode(bucket);
		}
		symbol->var_array[i] = 0;
	}
	free(symbol->var_array);
	symbol->var_array = NULL;
	symbol->array_size = symbol->table_size = 0;
	symbol->flags &= ~ARRAYMAXED;
}

/*
 * calculate the hash function of the string in subs
 */
unsigned int
hash(s, len, hsize)
register const char *s;
register size_t len;
unsigned long hsize;
{
	register unsigned long h = 0;

#ifdef this_is_really_slow

	register unsigned long g;

	while (len--) {
		h = (h << 4) + *s++;
		g = (h & 0xf0000000);
		if (g) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}

#else /* this_is_really_slow */
/*
 * This is INCREDIBLY ugly, but fast.  We break the string up into 8 byte
 * units.  On the first time through the loop we get the "leftover bytes"
 * (strlen % 8).  On every other iteration, we perform 8 HASHC's so we handle
 * all 8 bytes.  Essentially, this saves us 7 cmp & branch instructions.  If
 * this routine is heavily used enough, it's worth the ugly coding.
 *
 * OZ's original sdbm hash, copied from Margo Seltzers db package.
 *
 */

/* Even more speed: */
/* #define HASHC   h = *s++ + 65599 * h */
/* Because 65599 = pow(2,6) + pow(2,16) - 1 we multiply by shifts */
#define HASHC   htmp = (h << 6);  \
		h = *s++ + htmp + (htmp << 10) - h

	unsigned long htmp;

	h = 0;

#if defined(VAXC)
/*	
 * [This was an implementation of "Duff's Device", but it has been
 * redone, separating the switch for extra iterations from the loop.
 * This is necessary because the DEC VAX-C compiler is STOOPID.]
 */
	switch (len & (8 - 1)) {
	case 7:		HASHC;
	case 6:		HASHC;
	case 5:		HASHC;
	case 4:		HASHC;
	case 3:		HASHC;
	case 2:		HASHC;
	case 1:		HASHC;
	default:	break;
	}

	if (len > (8 - 1)) {
		register size_t loop = len >> 3;
		do {
			HASHC;
			HASHC;
			HASHC;
			HASHC;
			HASHC;
			HASHC;
			HASHC;
			HASHC;
		} while (--loop);
	}
#else /* !VAXC */
	/* "Duff's Device" for those who can handle it */
	if (len > 0) {
		register size_t loop = (len + 8 - 1) >> 3;

		switch (len & (8 - 1)) {
		case 0:
			do {	/* All fall throughs */
				HASHC;
		case 7:		HASHC;
		case 6:		HASHC;
		case 5:		HASHC;
		case 4:		HASHC;
		case 3:		HASHC;
		case 2:		HASHC;
		case 1:		HASHC;
			} while (--loop);
		}
	}
#endif /* !VAXC */
#endif /* this_is_really_slow - not */

	if (h >= hsize)
		h %= hsize;
	return h;
}

/*
 * locate symbol[subs]
 */
static NODE *				/* NULL if not found */
assoc_find(symbol, subs, hash1)
NODE *symbol;
register NODE *subs;
int hash1;
{
	register NODE *bucket, *prev = 0;

	for (bucket = symbol->var_array[hash1]; bucket; bucket = bucket->ahnext) {
		if (cmp_nodes(bucket->ahname, subs) == 0) {
#if 0
	/*
	 * Disable this code for now.  It screws things up if we have
	 * a ``for (iggy in foo)'' in progress.  Interestingly enough,
	 * this was not a problem in 2.15.3, only in 2.15.4.  I'm not
	 * sure why it works in 2.15.3.
	 */
			if (prev) {	/* move found to front of chain */
				prev->ahnext = bucket->ahnext;
				bucket->ahnext = symbol->var_array[hash1];
				symbol->var_array[hash1] = bucket;
			}
#endif
			return bucket;
		} else
			prev = bucket;	/* save previous list entry */
	}
	return NULL;
}

/*
 * test whether the array element symbol[subs] exists or not 
 */
int
in_array(symbol, subs)
NODE *symbol, *subs;
{
	register int hash1;

	if (symbol->type == Node_param_list)
		symbol = stack_ptr[symbol->param_cnt];
	if (symbol->var_array == 0)
		return 0;
	subs = concat_exp(subs);	/* concat_exp returns a string node */
	hash1 = hash(subs->stptr, subs->stlen, (unsigned long) symbol->array_size);
	if (assoc_find(symbol, subs, hash1) == NULL) {
		free_temp(subs);
		return 0;
	} else {
		free_temp(subs);
		return 1;
	}
}

/*
 * SYMBOL is the address of the node (or other pointer) being dereferenced.
 * SUBS is a number or string used as the subscript. 
 *
 * Find SYMBOL[SUBS] in the assoc array.  Install it with value "" if it
 * isn't there. Returns a pointer ala get_lhs to where its value is stored 
 */
NODE **
assoc_lookup(symbol, subs)
NODE *symbol, *subs;
{
	register int hash1;
	register NODE *bucket;

	(void) force_string(subs);

	if (symbol->var_array == 0) {
		symbol->type = Node_var_array;
		symbol->array_size = symbol->table_size = 0;	/* sanity */
		symbol->flags &= ~ARRAYMAXED;
		grow_table(symbol);
		hash1 = hash(subs->stptr, subs->stlen,
				(unsigned long) symbol->array_size);
	} else {
		hash1 = hash(subs->stptr, subs->stlen,
				(unsigned long) symbol->array_size);
		bucket = assoc_find(symbol, subs, hash1);
		if (bucket != NULL) {
			free_temp(subs);
			return &(bucket->ahvalue);
		}
	}

	/* It's not there, install it. */
	if (do_lint && subs->stlen == 0)
		warning("subscript of array `%s' is null string",
			symbol->vname);

	/* first see if we would need to grow the array, before installing */
	symbol->table_size++;
	if ((symbol->flags & ARRAYMAXED) == 0
	    && symbol->table_size/symbol->array_size > AVG_CHAIN_MAX) {
		grow_table(symbol);
		/* have to recompute hash value for new size */
		hash1 = hash(subs->stptr, subs->stlen,
				(unsigned long) symbol->array_size);
	}

	getnode(bucket);
	bucket->type = Node_ahash;
	if (subs->flags & TEMP)
		bucket->ahname = dupnode(subs);
	else {
		unsigned int saveflags = subs->flags;

		subs->flags &= ~MALLOC;
		bucket->ahname = dupnode(subs);
		subs->flags = saveflags;
	}
	free_temp(subs);

	/* array subscripts are strings */
	bucket->ahname->flags &= ~NUMBER;
	bucket->ahname->flags |= STRING;
	bucket->ahvalue = Nnull_string;
	bucket->ahnext = symbol->var_array[hash1];
	symbol->var_array[hash1] = bucket;
	return &(bucket->ahvalue);
}

void
do_delete(symbol, tree)
NODE *symbol, *tree;
{
	register int hash1;
	register NODE *bucket, *last;
	NODE *subs;

	if (symbol->type == Node_param_list)
		symbol = stack_ptr[symbol->param_cnt];
	if (symbol->var_array == 0)
		return;
	subs = concat_exp(tree);	/* concat_exp returns string node */
	hash1 = hash(subs->stptr, subs->stlen, (unsigned long) symbol->array_size);

	last = NULL;
	for (bucket = symbol->var_array[hash1]; bucket; last = bucket, bucket = bucket->ahnext)
		if (cmp_nodes(bucket->ahname, subs) == 0)
			break;
	free_temp(subs);
	if (bucket == NULL)
		return;
	if (last)
		last->ahnext = bucket->ahnext;
	else
		symbol->var_array[hash1] = bucket->ahnext;
	unref(bucket->ahname);
	unref(bucket->ahvalue);
	freenode(bucket);
	symbol->table_size--;
	if (symbol->table_size <= 0) {
		memset(symbol->var_array, '\0',
			sizeof(NODE *) * symbol->array_size);
		symbol->table_size = symbol->array_size = 0;
		symbol->flags &= ~ARRAYMAXED;
		free((char *) symbol->var_array);
		symbol->var_array = NULL;
	}
}

void
assoc_scan(symbol, lookat)
NODE *symbol;
struct search *lookat;
{
	lookat->sym = symbol;
	lookat->idx = 0;
	lookat->bucket = NULL;
	lookat->retval = NULL;
	if (symbol->var_array != NULL)
		assoc_next(lookat);
}

void
assoc_next(lookat)
struct search *lookat;
{
	register NODE *symbol = lookat->sym;
	
	if (symbol == NULL)
		fatal("null symbol in assoc_next");
	if (symbol->var_array == NULL || lookat->idx > symbol->array_size) {
		lookat->retval = NULL;
		return;
	}
	/*
	 * This is theoretically unsafe.  The element bucket might have
	 * been freed if the body of the scan did a delete on the next
	 * element of the bucket.  The only way to do that is by array
	 * reference, which is unlikely.  Basically, if the user is doing
	 * anything other than an operation on the current element of an
	 * assoc array while walking through it sequentially, all bets are
	 * off.  (The safe way is to register all search structs on an
	 * array with the array, and update all of them on a delete or
	 * insert)
	 */
	if (lookat->bucket != NULL) {
		lookat->retval = lookat->bucket->ahname;
		lookat->bucket = lookat->bucket->ahnext;
		return;
	}
	for (; lookat->idx < symbol->array_size; lookat->idx++) {
		NODE *bucket;

		if ((bucket = symbol->var_array[lookat->idx]) != NULL) {
			lookat->retval = bucket->ahname;
			lookat->bucket = bucket->ahnext;
			lookat->idx++;
			return;
		}
	}
	lookat->retval = NULL;
	lookat->bucket = NULL;
	return;
}

/* grow_table --- grow a hash table */

static void
grow_table(symbol)
NODE *symbol;
{
	NODE **old, **new, *chain, *next;
	int i, j;
	unsigned long hash1;
	unsigned long oldsize, newsize;
	/*
	 * This is an array of primes. We grow the table by an order of
	 * magnitude each time (not just doubling) so that growing is a
	 * rare operation. We expect, on average, that it won't happen
	 * more than twice.  The final size is also chosen to be small
	 * enough so that MS-DOG mallocs can handle it. When things are
	 * very large (> 8K), we just double more or less, instead of
	 * just jumping from 8K to 64K.
	 */
	static long sizes[] = { 13, 127, 1021, 8191, 16381, 32749, 65497 };

	/* find next biggest hash size */
	oldsize = symbol->array_size;
	newsize = 0;
	for (i = 0, j = sizeof(sizes)/sizeof(sizes[0]); i < j; i++) {
		if (oldsize < sizes[i]) {
			newsize = sizes[i];
			break;
		}
	}

	if (newsize == oldsize) {	/* table already at max (!) */
		symbol->flags |= ARRAYMAXED;
		return;
	}

	/* allocate new table */
	emalloc(new, NODE **, newsize * sizeof(NODE *), "grow_table");
	memset(new, '\0', newsize * sizeof(NODE *));

	/* brand new hash table, set things up and return */
	if (symbol->var_array == NULL) {
		symbol->table_size = 0;
		goto done;
	}

	/* old hash table there, move stuff to new, free old */
	old = symbol->var_array;
	for (i = 0; i < oldsize; i++) {
		if (old[i] == NULL)
			continue;

		for (chain = old[i]; chain != NULL; chain = next) {
			next = chain->ahnext;
			hash1 = hash(chain->ahname->stptr,
					chain->ahname->stlen, newsize);

			/* remove from old list, add to new */
			chain->ahnext = new[hash1];
			new[hash1] = chain;

		}
	}
	free(old);

done:
	/*
	 * note that symbol->table_size does not change if an old array,
	 * and is explicitly set to 0 if a new one.
	 */
	symbol->var_array = new;
	symbol->array_size = newsize;
}
