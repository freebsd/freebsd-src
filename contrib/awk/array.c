/*
 * array.c - routines for associative arrays.
 */

/* 
 * Copyright (C) 1986, 1988, 1989, 1991-2001 the Free Software Foundation, Inc.
 * 
 * This file is part of GAWK, the GNU implementation of the
 * AWK Programming Language.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
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

/* concat_exp --- concatenate expression list into a single string */

NODE *
concat_exp(register NODE *tree)
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
	subseplen = SUBSEP_node->var_value->stlen;
	subsep = SUBSEP_node->var_value->stptr;
	len = r->stlen + subseplen + 2;
	emalloc(str, char *, len, "concat_exp");
	memcpy(str, r->stptr, r->stlen+1);
	s = str + r->stlen;
	free_temp(r);
	for (tree = tree->rnode; tree != NULL; tree = tree->rnode) {
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
	}
	r = make_str_node(str, s - str, ALREADY_MALLOCED);
	r->flags |= TEMP;
	return r;
}

/* assoc_clear --- flush all the values in symbol[] before doing a split() */

void
assoc_clear(NODE *symbol)
{
	int i;
	NODE *bucket, *next;

	if (symbol->var_array == NULL)
		return;
	for (i = 0; i < symbol->array_size; i++) {
		for (bucket = symbol->var_array[i]; bucket != NULL; bucket = next) {
			next = bucket->ahnext;
			unref(bucket->ahname);
			unref(bucket->ahvalue);
			freenode(bucket);
		}
		symbol->var_array[i] = NULL;
	}
	free(symbol->var_array);
	symbol->var_array = NULL;
	symbol->array_size = symbol->table_size = 0;
	symbol->flags &= ~ARRAYMAXED;
}

/* hash --- calculate the hash function of the string in subs */

unsigned int
hash(register const char *s, register size_t len, unsigned long hsize)
{
	register unsigned long h = 0;

	/*
	 * This is INCREDIBLY ugly, but fast.  We break the string up into
	 * 8 byte units.  On the first time through the loop we get the
	 * "leftover bytes" (strlen % 8).  On every other iteration, we
	 * perform 8 HASHC's so we handle all 8 bytes.  Essentially, this
	 * saves us 7 cmp & branch instructions.  If this routine is
	 * heavily used enough, it's worth the ugly coding.
	 *
	 * OZ's original sdbm hash, copied from Margo Seltzers db package.
	 */

	/*
	 * Even more speed:
	 * #define HASHC   h = *s++ + 65599 * h
	 * Because 65599 = pow(2, 6) + pow(2, 16) - 1 we multiply by shifts
	 */
#define HASHC   htmp = (h << 6);  \
		h = *s++ + htmp + (htmp << 10) - h

	unsigned long htmp;

	h = 0;

#if defined(VAXC)
	/*	
	 * This was an implementation of "Duff's Device", but it has been
	 * redone, separating the switch for extra iterations from the
	 * loop. This is necessary because the DEC VAX-C compiler is
	 * STOOPID.
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
#else /* ! VAXC */
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
#endif /* ! VAXC */

	if (h >= hsize)
		h %= hsize;
	return h;
}

/* assoc_find --- locate symbol[subs] */

static NODE *				/* NULL if not found */
assoc_find(NODE *symbol, register NODE *subs, int hash1)
{
	register NODE *bucket;
	NODE *s1, *s2;

	for (bucket = symbol->var_array[hash1]; bucket != NULL;
			bucket = bucket->ahnext) {
		/*
		 * This used to use cmp_nodes() here.  That's wrong.
		 * Array indexes are strings; compare as such, always!
		 */
		s1 = bucket->ahname;
		s1 = force_string(s1);
		s2 = subs;

		if (s1->stlen == s2->stlen) {
			if (s1->stlen == 0	/* "" is a valid index */
			    || STREQN(s1->stptr, s2->stptr, s1->stlen))
				return bucket;
		}
	}
	return NULL;
}

/* in_array --- test whether the array element symbol[subs] exists or not */

int
in_array(NODE *symbol, NODE *subs)
{
	register int hash1;
	int ret;

	if (symbol->type == Node_param_list)
		symbol = stack_ptr[symbol->param_cnt];
	if (symbol->type == Node_array_ref)
		symbol = symbol->orig_array;
	if ((symbol->flags & SCALAR) != 0)
		fatal(_("attempt to use scalar `%s' as array"), symbol->vname);
	/*
	 * evaluate subscript first, it could have side effects
	 */
	subs = concat_exp(subs);	/* concat_exp returns a string node */
	if (symbol->var_array == NULL) {
		free_temp(subs);
		return 0;
	}
	hash1 = hash(subs->stptr, subs->stlen, (unsigned long) symbol->array_size);
	ret = (assoc_find(symbol, subs, hash1) != NULL);
	free_temp(subs);
	return ret;
}

/*
 * assoc_lookup:
 * Find SYMBOL[SUBS] in the assoc array.  Install it with value "" if it
 * isn't there. Returns a pointer ala get_lhs to where its value is stored.
 *
 * SYMBOL is the address of the node (or other pointer) being dereferenced.
 * SUBS is a number or string used as the subscript. 
 */

NODE **
assoc_lookup(NODE *symbol, NODE *subs, int reference)
{
	register int hash1;
	register NODE *bucket;

	assert(symbol->type == Node_var_array || symbol->type == Node_var);

	(void) force_string(subs);

	if ((symbol->flags & SCALAR) != 0)
		fatal(_("attempt to use scalar `%s' as array"), symbol->vname);

	if (symbol->var_array == NULL) {
		if (symbol->type != Node_var_array) {
			unref(symbol->var_value);
			symbol->type = Node_var_array;
		}
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

	if (do_lint && reference) {
		subs->stptr[subs->stlen] = '\0';
		lintwarn(_("reference to uninitialized element `%s[\"%s\"]'"),
		      symbol->vname, subs->stptr);
	}

	/* It's not there, install it. */
	if (do_lint && subs->stlen == 0)
		lintwarn(_("subscript of array `%s' is null string"),
			symbol->vname);

	/* first see if we would need to grow the array, before installing */
	symbol->table_size++;
	if ((symbol->flags & ARRAYMAXED) == 0
	    && (symbol->table_size / symbol->array_size) > AVG_CHAIN_MAX) {
		grow_table(symbol);
		/* have to recompute hash value for new size */
		hash1 = hash(subs->stptr, subs->stlen,
				(unsigned long) symbol->array_size);
	}

	getnode(bucket);
	bucket->type = Node_ahash;

	/*
	 * Freeze this string value --- it must never
	 * change, no matter what happens to the value
	 * that created it or to CONVFMT, etc.
	 *
	 * One day: Use an atom table to track array indices,
	 * and avoid the extra memory overhead.
	 */
	if (subs->flags & TEMP)
		bucket->ahname = dupnode(subs);
	else
		bucket->ahname = copynode(subs);

	free_temp(subs);

	/* array subscripts are strings */
	bucket->ahname->flags &= ~(NUMBER|NUM);
	bucket->ahname->flags |= (STRING|STR);
	/* ensure that this string value never changes */
	bucket->ahname->stfmt = -1;

	bucket->ahvalue = Nnull_string;
	bucket->ahnext = symbol->var_array[hash1];
	symbol->var_array[hash1] = bucket;
	return &(bucket->ahvalue);
}

/* do_delete --- perform `delete array[s]' */

void
do_delete(NODE *symbol, NODE *tree)
{
	register int hash1;
	register NODE *bucket, *last;
	NODE *subs;

	if (symbol->type == Node_param_list) {
		symbol = stack_ptr[symbol->param_cnt];
		if (symbol->type == Node_var)
			return;
	}
	if (symbol->type == Node_array_ref)
		symbol = symbol->orig_array;
	if (symbol->type == Node_var_array) {
		if (symbol->var_array == NULL)
			return;
	} else
		fatal(_("delete: illegal use of variable `%s' as array"),
			symbol->vname);

	if (tree == NULL) {	/* delete array */
		assoc_clear(symbol);
		return;
	}

	subs = concat_exp(tree);	/* concat_exp returns string node */
	hash1 = hash(subs->stptr, subs->stlen, (unsigned long) symbol->array_size);

	last = NULL;
	for (bucket = symbol->var_array[hash1]; bucket != NULL;
			last = bucket, bucket = bucket->ahnext) {
		/*
		 * This used to use cmp_nodes() here.  That's wrong.
		 * Array indexes are strings; compare as such, always!
		 */
		NODE *s1, *s2;

		s1 = bucket->ahname;
		s1 = force_string(s1);
		s2 = subs;

		if (s1->stlen == s2->stlen) {
			if (s1->stlen == 0	/* "" is a valid index */
			    || STREQN(s1->stptr, s2->stptr, s1->stlen))
				break;
		}
	}

	if (bucket == NULL) {
		if (do_lint)
			lintwarn(_("delete: index `%s' not in array `%s'"),
				subs->stptr, symbol->vname);
		free_temp(subs);
		return;
	}
	free_temp(subs);
	if (last != NULL)
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

/* do_delete_loop --- simulate ``for (iggy in foo) delete foo[iggy]'' */

/*
 * The primary hassle here is that `iggy' needs to have some arbitrary
 * array index put in it before we can clear the array, we can't
 * just replace the loop with `delete foo'.
 */

void
do_delete_loop(NODE *symbol, NODE *tree)
{
	size_t i;
	NODE **lhs;
	Func_ptr after_assign = NULL;

	if (symbol->type == Node_param_list) {
		symbol = stack_ptr[symbol->param_cnt];
		if (symbol->type == Node_var)
			return;
	}
	if (symbol->type == Node_array_ref)
		symbol = symbol->orig_array;
	if (symbol->type == Node_var_array) {
		if (symbol->var_array == NULL)
			return;
	} else
		fatal(_("delete: illegal use of variable `%s' as array"),
			symbol->vname);

	/* get first index value */
	for (i = 0; i < symbol->array_size; i++) {
		if (symbol->var_array[i] != NULL) {
			lhs = get_lhs(tree->lnode, & after_assign, FALSE);
			unref(*lhs);
			*lhs = dupnode(symbol->var_array[i]->ahname);
			break;
		}
	}

	/* blast the array in one shot */
	assoc_clear(symbol);
}

/* grow_table --- grow a hash table */

static void
grow_table(NODE *symbol)
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
	static long sizes[] = { 13, 127, 1021, 8191, 16381, 32749, 65497,
#if ! defined(MSDOS) && ! defined(OS2) && ! defined(atarist)
				131101, 262147, 524309, 1048583, 2097169,
				4194319, 8388617, 16777259, 33554467, 
				67108879, 134217757, 268435459, 536870923,
				1073741827
#endif
	};

	/* find next biggest hash size */
	newsize = oldsize = symbol->array_size;
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

/* pr_node --- print simple node info */

static void
pr_node(NODE *n)
{
	if ((n->flags & (NUM|NUMBER)) != 0)
		printf("%g", n->numbr);
	else
		printf("%.*s", (int) n->stlen, n->stptr);
}

/* assoc_dump --- dump the contents of an array */

NODE *
assoc_dump(NODE *symbol)
{
	int i;
	NODE *bucket;

	if (symbol->var_array == NULL) {
		printf(_("%s: empty (null)\n"), symbol->vname);
		return tmp_number((AWKNUM) 0);
	}

	if (symbol->table_size == 0) {
		printf(_("%s: empty (zero)\n"), symbol->vname);
		return tmp_number((AWKNUM) 0);
	}

	printf(_("%s: table_size = %d, array_size = %d\n"), symbol->vname,
			(int) symbol->table_size, (int) symbol->array_size);

	for (i = 0; i < symbol->array_size; i++) {
		for (bucket = symbol->var_array[i]; bucket != NULL;
				bucket = bucket->ahnext) {
			printf("%s: I: [(%p, %ld, %s) len %d <%.*s>] V: [",
				symbol->vname,
				bucket->ahname,
				bucket->ahname->stref,
				flags2str(bucket->ahname->flags),
				(int) bucket->ahname->stlen,
				(int) bucket->ahname->stlen,
				bucket->ahname->stptr);
			pr_node(bucket->ahvalue);
			printf("]\n");
		}
	}

	return tmp_number((AWKNUM) 0);
}

/* do_adump --- dump an array: interface to assoc_dump */

NODE *
do_adump(NODE *tree)
{
	NODE *r, *a;

	a = tree->lnode;

	if (a->type == Node_param_list) {
		printf(_("%s: is paramater\n"), a->vname);
		a = stack_ptr[a->param_cnt];
	}

	if (a->type == Node_array_ref) {
		printf(_("%s: array_ref to %s\n"), a->vname,
					a->orig_array->vname);
		a = a->orig_array;
	}

	r = assoc_dump(a);

	return r;
}

/*
 * The following functions implement the builtin
 * asort function.  Initial work by Alan J. Broder,
 * ajb@woti.com.
 */

/* dup_table --- duplicate input symbol table "symbol" */

static void
dup_table(NODE *symbol, NODE *newsymb)
{
	NODE **old, **new, *chain, *bucket;
	int i;
	unsigned long cursize;

	/* find the current hash size */
	cursize = symbol->array_size;

	new = NULL;

	/* input is a brand new hash table, so there's nothing to copy */
	if (symbol->var_array == NULL)
		newsymb->table_size = 0;
	else {
		/* old hash table there, dupnode stuff into a new table */

		/* allocate new table */
		emalloc(new, NODE **, cursize * sizeof(NODE *), "dup_table");
		memset(new, '\0', cursize * sizeof(NODE *));

		/* do the copying/dupnode'ing */
		old = symbol->var_array;
		for (i = 0; i < cursize; i++) {
			if (old[i] != NULL) {
				for (chain = old[i]; chain != NULL;
						chain = chain->ahnext) {
					/* get a node for the linked list */
					getnode(bucket);
					bucket->type = Node_ahash;

					/*
					 * copy the corresponding name and
					 * value from the original input list
					 */
					bucket->ahname = dupnode(chain->ahname);
					bucket->ahvalue = dupnode(chain->ahvalue);

					/*
					 * put the node on the corresponding
					 * linked list in the new table
					 */
					bucket->ahnext = new[i];
					new[i] = bucket;
				}
			}
		}
		newsymb->table_size = symbol->table_size;
	}

	newsymb->var_array = new;
	newsymb->array_size = cursize;
}

/* merge --- do a merge of two sorted lists */

static NODE *
merge(NODE *left, NODE *right)
{
	NODE *ans, *cur;

	if (cmp_nodes(left->ahvalue, right->ahvalue) <= 0) {
		ans = cur = left;
		left  = left->ahnext;
	} else {
		ans = cur = right;
		right = right->ahnext;
	}

	while (left != NULL && right != NULL) {
		if (cmp_nodes(left->ahvalue, right->ahvalue) <= 0) {
			cur->ahnext = left;
			cur = left;
			left  = left->ahnext;
		} else {
			cur->ahnext = right;
			cur = right;
			right = right->ahnext;
		}
	}

	cur->ahnext = (left != NULL ? left : right);

	return ans;
}

/* merge_sort --- recursively sort the left and right sides of a list */

static NODE *
merge_sort(NODE *left, int size)
{
	NODE *right, *tmp;
	int i, half;

	if (size <= 1)
		return left;

	/* walk down the list, till just one before the midpoint */
	tmp = left;
	half = size / 2;
	for (i = 0; i < half-1; i++)
		tmp = tmp->ahnext;

	/* split the list into two parts */
	right = tmp->ahnext;
	tmp->ahnext = NULL;

	/* sort the left and right parts of the list */
	left  = merge_sort(left,       half);
	right = merge_sort(right, size-half);

	/* merge the two sorted parts of the list */
	return merge(left, right);
}


/*
 * assoc_from_list -- Populate an array with the contents of a list of NODEs, 
 * using increasing integers as the key.
 */

static void
assoc_from_list(NODE *symbol, NODE *list)
{
	NODE *next;
	int i = 0;
	register int hash1;

	for (; list != NULL; list = next) {
		next = list->ahnext;

		/* make an int out of i++ */
		i++;
		list->ahname = make_number((AWKNUM) i);
		(void) force_string(list->ahname);

		/* find the bucket where it belongs */
		hash1 = hash(list->ahname->stptr, list->ahname->stlen,
				symbol->array_size);

		/* link the node into the chain at that bucket */
		list->ahnext = symbol->var_array[hash1];
		symbol->var_array[hash1] = list;
	}
}

/*
 * assoc_sort_inplace --- sort all the values in symbol[], replacing
 * the sorted values back into symbol[], indexed by integers starting with 1.
 */

static NODE *
assoc_sort_inplace(NODE *symbol)
{
	int i, num;
	NODE *bucket, *next, *list;

	if (symbol->var_array == NULL
	    || symbol->array_size <= 0
	    || symbol->table_size <= 0)
		return tmp_number((AWKNUM) 0);

	/* build a linked list out of all the entries in the table */
	list = NULL;
	num = 0;
	for (i = 0; i < symbol->array_size; i++) {
		for (bucket = symbol->var_array[i]; bucket != NULL; bucket = next) {
			next = bucket->ahnext;
			unref(bucket->ahname);
			bucket->ahnext = list;
			list = bucket;
			num++;
		}
		symbol->var_array[i] = NULL;
	}

	/*
	 * Sort the linked list of NODEs.
	 * (The especially nice thing about using a merge sort here is that
	 * we require absolutely no additional storage. This is handy if the
	 * array has grown to be very large.)
	 */
	list = merge_sort(list, num);

	/*
	 * now repopulate the original array, using increasing
	 * integers as the key
	 */
	assoc_from_list(symbol, list);

	return tmp_number((AWKNUM) num);
}

/* do_asort --- do the actual work to sort the input array */

NODE *
do_asort(NODE *tree)
{
	NODE *src, *dest;

	src = tree->lnode;
	dest = NULL;

	if (src->type == Node_param_list)
		src = stack_ptr[src->param_cnt];
	if (src->type == Node_array_ref)
		src = src->orig_array;
	if (src->type != Node_var_array)
		fatal(_("asort: first argument is not an array"));

	if (tree->rnode != NULL) {  /* 2nd optional arg */
		dest = tree->rnode->lnode;
		if (dest->type == Node_param_list)
			dest = stack_ptr[dest->param_cnt];
		if (dest->type == Node_array_ref)
			dest = dest->orig_array;
		if (dest->type != Node_var && dest->type != Node_var_array)
			fatal(_("asort: second argument is not an array"));
		dest->type = Node_var_array;
		assoc_clear(dest);
		dup_table(src, dest);
	}

	return dest != NULL ? assoc_sort_inplace(dest) : assoc_sort_inplace(src);
}
