/*
 * array.c - routines for associative arrays.
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991, 1992 the Free Software Foundation, Inc.
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

#include "awk.h"

static NODE *assoc_find P((NODE *symbol, NODE *subs, int hash1));

NODE *
concat_exp(tree)
register NODE *tree;
{
	register NODE *r;
	char *str;
	char *s;
	unsigned len;
	int offset;
	int subseplen;
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
	for (i = 0; i < HASHSIZE; i++) {
		for (bucket = symbol->var_array[i]; bucket; bucket = next) {
			next = bucket->ahnext;
			unref(bucket->ahname);
			unref(bucket->ahvalue);
			freenode(bucket);
		}
		symbol->var_array[i] = 0;
	}
}

/*
 * calculate the hash function of the string in subs
 */
unsigned int
hash(s, len)
register char *s;
register int len;
{
	register unsigned long h = 0, g;

	while (len--) {
		h = (h << 4) + *s++;
		g = (h & 0xf0000000);
		if (g) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
	}
	if (h < HASHSIZE)
		return h;
	else
		return h%HASHSIZE;
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
			if (prev) {	/* move found to front of chain */
				prev->ahnext = bucket->ahnext;
				bucket->ahnext = symbol->var_array[hash1];
				symbol->var_array[hash1] = bucket;
			}
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
	hash1 = hash(subs->stptr, subs->stlen);
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
	hash1 = hash(subs->stptr, subs->stlen);

	if (symbol->var_array == 0) {	/* this table really should grow
					 * dynamically */
		unsigned size;

		size = sizeof(NODE *) * HASHSIZE;
		emalloc(symbol->var_array, NODE **, size, "assoc_lookup");
		memset((char *)symbol->var_array, 0, size);
		symbol->type = Node_var_array;
	} else {
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
	hash1 = hash(subs->stptr, subs->stlen);

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
}

void
assoc_scan(symbol, lookat)
NODE *symbol;
struct search *lookat;
{
	if (!symbol->var_array) {
		lookat->retval = NULL;
		return;
	}
	lookat->arr_ptr = symbol->var_array;
	lookat->arr_end = lookat->arr_ptr + HASHSIZE;	/* added */
	lookat->bucket = symbol->var_array[0];
	assoc_next(lookat);
}

void
assoc_next(lookat)
struct search *lookat;
{
	while (lookat->arr_ptr < lookat->arr_end) {
		if (lookat->bucket != 0) {
			lookat->retval = lookat->bucket->ahname;
			lookat->bucket = lookat->bucket->ahnext;
			return;
		}
		lookat->arr_ptr++;
		if (lookat->arr_ptr < lookat->arr_end)
			lookat->bucket = *(lookat->arr_ptr);
		else
			lookat->retval = NULL;
	}
	return;
}
