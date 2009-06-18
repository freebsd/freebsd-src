/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <_string_table.h>
#include <strings.h>
#include <sgs.h>
#include <stdio.h>

/*
 * This file provides the interfaces to build a Str_tbl suitable for use by
 * either the sgsmsg message system, or a standard ELF string table (SHT_STRTAB)
 * as created by ld(1).
 *
 * There are two modes which can be used when constructing a string table:
 *
 *	st_new(0)
 *		standard string table - no compression.  This is the
 *		traditional, fast method.
 *
 *	st_new(FLG_STTAB_COMPRESS)
 *		builds a compressed string table which both eliminates
 *		duplicate strings, and permits strings with common suffixes
 *		(atexit vs. exit) to overlap in the table.  This provides space
 *		savings for many string tables.  Although more work than the
 *		traditional method, the algorithms used are designed to scale
 *		and keep any overhead at a minimum.
 *
 * These string tables are built with a common interface in a two-pass manner.
 * The first pass finds all of the strings required for the string-table and
 * calculates the size required for the final string table.
 *
 * The second pass allocates the string table, populates the strings into the
 * table and returns the offsets the strings have been assigned.
 *
 * The calling sequence to build and populate a string table is:
 *
 *		st_new();		// initialize strtab
 *
 *		st_insert(st1);		// first pass of strings ...
 *					// calculates size required for
 *					// string table
 *
 *		st_delstring(st?);	// remove string previously
 *					// inserted
 *		st_insert(stN);
 *
 *		st_getstrtab_sz();	// freezes strtab and computes
 *					// size of table.
 *
 *		st_setstrbuf();		// associates a final destination
 *					// for the string table
 *
 *		st_setstring(st1);	// populate the string table
 *		...			// offsets are based off of second
 *					// pass	through the string table
 *		st_setstring(stN);
 *
 *		st_destroy();		// tear down string table
 *					// structures.
 *
 * String Suffix Compression Algorithm:
 *
 *   Here's a quick high level overview of the Suffix String
 *   compression algorithm used.  First - the heart of the algorithm
 *   is a Hash table list which represents a dictionary of all unique
 *   strings inserted into the string table.  The hash function for
 *   this table is a standard string hash except that the hash starts
 *   at the last character in the string (&str[n - 1]) and works towards
 *   the first character in the function (&str[0]).  As we compute the
 *   HASH value for a given string, we also compute the hash values
 *   for all of the possible suffix strings for that string.
 *
 *   As we compute the hash - at each character see if the current
 *   suffix string for that hash is already present in the table.  If
 *   it is, and the string is a master string.  Then change that
 *   string to a suffix string of the new string being inserted.
 *
 *   When the final hash value is found (hash for str[0...n]), check
 *   to see if it is in the hash table - if so increment the reference
 *   count for the string.  If it is not yet in the table, insert a
 *   new hash table entry for a master string.
 *
 *   The above method will find all suffixes of a given string given
 *   that the strings are inserted from shortest to longest.  That is
 *   why this is a two phase method, we first collect all of the
 *   strings and store them based off of their length in an AVL tree.
 *   Once all of the strings have been submitted we then start the
 *   hash table build by traversing the AVL tree in order and
 *   inserting the strings from shortest to longest as described
 *   above.
 */

/* LINTLIBRARY */

static int
avl_len_compare(const void *n1, const void *n2)
{
	size_t	len1, len2;

	len1 = ((LenNode *)n1)->ln_strlen;
	len2 = ((LenNode *)n2)->ln_strlen;

	if (len1 == len2)
		return (0);
	if (len2 < len1)
		return (1);
	return (-1);
}

static int
avl_str_compare(const void *n1, const void *n2)
{
	const char	*str1, *str2;
	int		rc;

	str1 = ((StrNode *)n1)->sn_str;
	str2 = ((StrNode *)n2)->sn_str;

	rc = strcmp(str1, str2);
	if (rc > 0)
		return (1);
	if (rc < 0)
		return (-1);
	return (0);
}

/*
 * Return an initialized Str_tbl - returns NULL on failure.
 *
 * flags:
 *	FLG_STTAB_COMPRESS - build a compressed string table
 */
Str_tbl *
st_new(uint_t flags)
{
	Str_tbl	*stp;

	if ((stp = calloc(sizeof (Str_tbl), 1)) == NULL)
		return (NULL);

	/*
	 * Start with a leading '\0' - it's tradition.
	 */
	stp->st_strsize = stp->st_fullstrsize = stp->st_nextoff = 1;

	/*
	 * Do we compress this string table?
	 */
	stp->st_flags = flags;
	if ((stp->st_flags & FLG_STTAB_COMPRESS) == 0)
		return (stp);

	if ((stp->st_lentree = calloc(sizeof (avl_tree_t), 1)) == NULL)
		return (NULL);

	avl_create(stp->st_lentree, &avl_len_compare, sizeof (LenNode),
	    SGSOFFSETOF(LenNode, ln_avlnode));

	return (stp);
}

/*
 * Insert a new string into the Str_tbl.  There are two AVL trees used.
 *
 *  .	The first LenNode AVL tree maintains a tree of nodes based on string
 *	sizes.
 *  .	Each LenNode maintains a StrNode AVL tree for each string.  Large
 *	applications have been known to contribute thousands of strings of
 *	the same size.  Should strings need to be removed (-z ignore), then
 *	the string AVL tree makes this removal efficient and scalable.
 */
int
st_insert(Str_tbl *stp, const char *str)
{
	size_t		len;
	StrNode		*snp, sn = { 0 };
	LenNode		*lnp, ln = { 0 };
	avl_index_t	where;

	/*
	 * String table can't have been cooked
	 */
	assert((stp->st_flags & FLG_STTAB_COOKED) == 0);

	/*
	 * Null strings always point to the head of the string
	 * table - no reason to keep searching.
	 */
	if ((len = strlen(str)) == 0)
		return (0);

	stp->st_fullstrsize += len + 1;
	stp->st_strcnt++;

	if ((stp->st_flags & FLG_STTAB_COMPRESS) == 0)
		return (0);

	/*
	 * From the controlling string table, determine which LenNode AVL node
	 * provides for this string length.  If the node doesn't exist, insert
	 * a new node to represent this string length.
	 */
	ln.ln_strlen = len;
	if ((lnp = avl_find(stp->st_lentree, &ln, &where)) == NULL) {
		if ((lnp = calloc(sizeof (LenNode), 1)) == NULL)
			return (-1);
		lnp->ln_strlen = len;
		avl_insert(stp->st_lentree, lnp, where);

		if ((lnp->ln_strtree = calloc(sizeof (avl_tree_t), 1)) == NULL)
			return (0);

		avl_create(lnp->ln_strtree, &avl_str_compare, sizeof (StrNode),
		    SGSOFFSETOF(StrNode, sn_avlnode));
	}

	/*
	 * From the string length AVL node determine whether a StrNode AVL node
	 * provides this string.  If the node doesn't exist, insert a new node
	 * to represent this string.
	 */
	sn.sn_str = str;
	if ((snp = avl_find(lnp->ln_strtree, &sn, &where)) == NULL) {
		if ((snp = calloc(sizeof (StrNode), 1)) == NULL)
			return (-1);
		snp->sn_str = str;
		avl_insert(lnp->ln_strtree, snp, where);
	}
	snp->sn_refcnt++;

	return (0);
}

/*
 * Remove a previously inserted string from the Str_tbl.
 */
int
st_delstring(Str_tbl *stp, const char *str)
{
	size_t		len;
	LenNode		*lnp, ln = { 0 };
	StrNode		*snp, sn = { 0 };

	/*
	 * String table can't have been cooked
	 */
	assert((stp->st_flags & FLG_STTAB_COOKED) == 0);

	len = strlen(str);
	stp->st_fullstrsize -= len + 1;

	if ((stp->st_flags & FLG_STTAB_COMPRESS) == 0)
		return (0);

	/*
	 * Determine which LenNode AVL node provides for this string length.
	 */
	ln.ln_strlen = len;
	if ((lnp = avl_find(stp->st_lentree, &ln, 0)) != NULL) {
		sn.sn_str = str;
		if ((snp = avl_find(lnp->ln_strtree, &sn, 0)) != NULL) {
			/*
			 * Reduce the reference count, and if zero remove the
			 * node.
			 */
			if (--snp->sn_refcnt == 0)
				avl_remove(lnp->ln_strtree, snp);
			return (0);
		}
	}

	/*
	 * No strings of this length, or no string itself - someone goofed.
	 */
	return (-1);
}

/*
 * Tear down a String_Table structure.
 */
void
st_destroy(Str_tbl *stp)
{
	Str_hash	*sthash, *psthash;
	Str_master	*mstr, *pmstr;
	uint_t		i;

	/*
	 * cleanup the master strings
	 */
	for (mstr = stp->st_mstrlist, pmstr = 0; mstr;
	    mstr = mstr->sm_next) {
		if (pmstr)
			free(pmstr);
		pmstr = mstr;
	}
	if (pmstr)
		free(pmstr);

	if (stp->st_hashbcks) {
		for (i = 0; i < stp->st_hbckcnt; i++) {
			for (sthash = stp->st_hashbcks[i], psthash = 0;
			    sthash; sthash = sthash->hi_next) {
				if (psthash)
					free(psthash);
				psthash = sthash;
			}
			if (psthash)
				free(psthash);
		}
		free(stp->st_hashbcks);
	}
	free(stp);
}


/*
 * For a given string - copy it into the buffer associated with
 * the string table - and return the offset it has been assigned.
 *
 * If a value of '-1' is returned - the string was not found in
 * the Str_tbl.
 */
int
st_setstring(Str_tbl *stp, const char *str, size_t *stoff)
{
	size_t		stlen;
	uint_t		hashval;
	Str_hash	*sthash;
	Str_master	*mstr;
	int		i;

	/*
	 * String table *must* have been previously cooked
	 */
	assert(stp->st_strbuf);

	assert(stp->st_flags & FLG_STTAB_COOKED);
	stlen = strlen(str);
	/*
	 * Null string always points to head of string table
	 */
	if (stlen == 0) {
		*stoff = 0;
		return (0);
	}

	if ((stp->st_flags & FLG_STTAB_COMPRESS) == 0) {
		size_t		_stoff;

		stlen++;	/* count for trailing '\0' */
		_stoff = stp->st_nextoff;
		/*
		 * Have we overflowed our assigned buffer?
		 */
		if ((_stoff + stlen) > stp->st_fullstrsize)
			return (-1);
		memcpy(stp->st_strbuf + _stoff, str, stlen);
		*stoff = _stoff;
		stp->st_nextoff += stlen;
		return (0);
	}

	/*
	 * Calculate reverse hash for string.
	 */
	hashval = HASHSEED;
	for (i = stlen; i >= 0; i--) {
		hashval = ((hashval << 5) + hashval) +
		    str[i];			/* h = ((h * 33) + c) */
	}

	for (sthash = stp->st_hashbcks[hashval % stp->st_hbckcnt]; sthash;
	    sthash = sthash->hi_next) {
		const char	*hstr;

		if (sthash->hi_hashval != hashval)
			continue;

		hstr = &sthash->hi_mstr->sm_str[sthash->hi_mstr->sm_strlen -
		    sthash->hi_strlen];
		if (strcmp(str, hstr) == 0)
			break;
	}

	/*
	 * Did we find the string?
	 */
	if (sthash == 0)
		return (-1);

	/*
	 * Has this string been copied into the string table?
	 */
	mstr = sthash->hi_mstr;
	if (mstr->sm_stroff == 0) {
		size_t	mstrlen = mstr->sm_strlen + 1;

		mstr->sm_stroff = stp->st_nextoff;

		/*
		 * Have we overflowed our assigned buffer?
		 */
		if ((mstr->sm_stroff + mstrlen) > stp->st_fullstrsize)
			return (-1);

		(void) memcpy(stp->st_strbuf + mstr->sm_stroff,
		    mstr->sm_str, mstrlen);
		stp->st_nextoff += mstrlen;
	}

	/*
	 * Calculate offset of (sub)string.
	 */
	*stoff = mstr->sm_stroff + mstr->sm_strlen - sthash->hi_strlen;

	return (0);
}


static int
st_hash_insert(Str_tbl *stp, const char *str, size_t len)
{
	int		i;
	uint_t		hashval = HASHSEED;
	uint_t		bckcnt = stp->st_hbckcnt;
	Str_hash	**hashbcks = stp->st_hashbcks;
	Str_hash	*sthash;
	Str_master	*mstr = 0;

	/*
	 * We use a classic 'Bernstein k=33' hash function.  But
	 * instead of hashing from the start of the string to the
	 * end, we do it in reverse.
	 *
	 * This way - we are essentially building all of the
	 * suffix hashvalues as we go.  We can check to see if
	 * any suffixes already exist in the tree as we generate
	 * the hash.
	 */
	for (i = len; i >= 0; i--) {
		hashval = ((hashval << 5) + hashval) +
		    str[i];			/* h = ((h * 33) + c) */

		for (sthash = hashbcks[hashval % bckcnt];
		    sthash; sthash = sthash->hi_next) {
			const char	*hstr;
			Str_master	*_mstr;

			if (sthash->hi_hashval != hashval)
				continue;

			_mstr = sthash->hi_mstr;
			hstr = &_mstr->sm_str[_mstr->sm_strlen -
			    sthash->hi_strlen];

			if (strcmp(&str[i], hstr))
				continue;

			if (i == 0) {
				/*
				 * Entry already in table, increment refcnt and
				 * get out.
				 */
				sthash->hi_refcnt++;
				return (0);
			} else {
				/*
				 * If this 'suffix' is presently a 'master
				 * string, then take over it's record.
				 */
				if (sthash->hi_strlen == _mstr->sm_strlen) {
					/*
					 * we should only do this once.
					 */
					assert(mstr == 0);
					mstr = _mstr;
				}
			}
		}
	}

	/*
	 * Do we need a new master string, or can we take over
	 * one we already found in the table?
	 */
	if (mstr == 0) {
		/*
		 * allocate a new master string
		 */
		if ((mstr = calloc(sizeof (Str_hash), 1)) == 0)
			return (-1);
		mstr->sm_next = stp->st_mstrlist;
		stp->st_mstrlist = mstr;
		stp->st_strsize += len + 1;
	} else {
		/*
		 * We are taking over a existing master string, the string size
		 * only increments by the difference between the current string
		 * and the previous master.
		 */
		assert(len > mstr->sm_strlen);
		stp->st_strsize += len - mstr->sm_strlen;
	}

	if ((sthash = calloc(sizeof (Str_hash), 1)) == 0)
		return (-1);

	mstr->sm_hashval = sthash->hi_hashval = hashval;
	mstr->sm_strlen = sthash->hi_strlen = len;
	mstr->sm_str = str;
	sthash->hi_refcnt = 1;
	sthash->hi_mstr = mstr;

	/*
	 * Insert string element into head of hash list
	 */
	hashval = hashval % bckcnt;
	sthash->hi_next = hashbcks[hashval];
	hashbcks[hashval] = sthash;
	return (0);
}

/*
 * Return amount of space required for the string table.
 */
size_t
st_getstrtab_sz(Str_tbl *stp)
{
	assert(stp->st_fullstrsize > 0);

	if ((stp->st_flags & FLG_STTAB_COMPRESS) == 0) {
		stp->st_flags |= FLG_STTAB_COOKED;
		return (stp->st_fullstrsize);
	}

	if ((stp->st_flags & FLG_STTAB_COOKED) == 0) {
		LenNode		*lnp;
		void		*cookie;

		stp->st_flags |= FLG_STTAB_COOKED;
		/*
		 * allocate a hash table about the size of # of
		 * strings input.
		 */
		stp->st_hbckcnt = findprime(stp->st_strcnt);
		if ((stp->st_hashbcks =
		    calloc(sizeof (Str_hash), stp->st_hbckcnt)) == NULL)
			return (0);

		/*
		 * We now walk all of the strings in the list, from shortest to
		 * longest, and insert them into the hashtable.
		 */
		if ((lnp = avl_first(stp->st_lentree)) == NULL) {
			/*
			 * Is it possible we have an empty string table, if so,
			 * the table still contains '\0', so return the size.
			 */
			if (avl_numnodes(stp->st_lentree) == 0) {
				assert(stp->st_strsize == 1);
				return (stp->st_strsize);
			}
			return (0);
		}

		while (lnp) {
			StrNode	*snp;

			/*
			 * Walk the string lists and insert them into the hash
			 * list.  Once a string is inserted we no longer need
			 * it's entry, so the string can be freed.
			 */
			for (snp = avl_first(lnp->ln_strtree); snp;
			    snp = AVL_NEXT(lnp->ln_strtree, snp)) {
				if (st_hash_insert(stp, snp->sn_str,
				    lnp->ln_strlen) == -1)
					return (0);
			}

			/*
			 * Now that the strings have been copied, walk the
			 * StrNode tree and free all the AVL nodes.  Note,
			 * avl_destroy_nodes() beats avl_remove() as the
			 * latter balances the nodes as they are removed.
			 * We just want to tear the whole thing down fast.
			 */
			cookie = NULL;
			while ((snp = avl_destroy_nodes(lnp->ln_strtree,
			    &cookie)) != NULL)
				free(snp);
			avl_destroy(lnp->ln_strtree);
			free(lnp->ln_strtree);
			lnp->ln_strtree = NULL;

			/*
			 * Move on to the next LenNode.
			 */
			lnp = AVL_NEXT(stp->st_lentree, lnp);
		}

		/*
		 * Now that all of the strings have been freed, walk the
		 * LenNode tree and free all of the AVL nodes.  Note,
		 * avl_destroy_nodes() beats avl_remove() as the latter
		 * balances the nodes as they are removed. We just want to
		 * tear the whole thing down fast.
		 */
		cookie = NULL;
		while ((lnp = avl_destroy_nodes(stp->st_lentree,
		    &cookie)) != NULL)
			free(lnp);
		avl_destroy(stp->st_lentree);
		free(stp->st_lentree);
		stp->st_lentree = 0;
	}

	assert(stp->st_strsize > 0);
	assert(stp->st_fullstrsize >= stp->st_strsize);

	return (stp->st_strsize);
}

/*
 * Associate a buffer with a string table.
 */
const char *
st_getstrbuf(Str_tbl *stp)
{
	return (stp->st_strbuf);
}

int
st_setstrbuf(Str_tbl *stp, char *stbuf, size_t bufsize)
{
	assert(stp->st_flags & FLG_STTAB_COOKED);

	if ((stp->st_flags & FLG_STTAB_COMPRESS) == 0) {
		if (bufsize < stp->st_fullstrsize)
			return (-1);
	} else {
		if (bufsize < stp->st_strsize)
			return (-1);
	}

	stp->st_strbuf = stbuf;
#ifdef	DEBUG
	/*
	 * for debug builds - start with a stringtable filled in
	 * with '0xff'.  This makes it very easy to find wholes
	 * which we failed to fill in - in the strtab.
	 */
	memset(stbuf, 0xff, bufsize);
	stbuf[0] = '\0';
#else
	memset(stbuf, 0x0, bufsize);
#endif
	return (0);
}
