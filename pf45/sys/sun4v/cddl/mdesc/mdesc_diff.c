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
 * $FreeBSD$
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)mdesc_diff.c	1.1	06/05/16 SMI"

#include <sys/types.h>
#ifdef _KERNEL
#include <sys/systm.h>
#else /* _KERNEL */
#include <string.h>
#include <strings.h>
#endif /* _KERNEL */
#include <sys/note.h>
#include <sys/mdesc.h>
#include <sys/mdesc_impl.h>

#define	MDD_FREE_CHECK(mdp, ptr, sz)		\
	do {					\
		if (ptr) mdp->freep(ptr, sz);	\
	_NOTE(CONSTCOND) } while (0)

#define	MD_DIFF_MAGIC			0x4D445F4449464621ull	/* 'MD_DIFF!' */
#define	MD_DIFF_NOMATCH			(-1)
#define	MD_DIFF_MATCH			(1)

typedef struct {
	mde_cookie_t	*mdep;
	uint_t		nelem;
} md_diff_t;

typedef struct {
	uint64_t	mdd_magic;
	md_diff_t	added;
	md_diff_t	removed;
	md_diff_t	match1;
	md_diff_t	match2;
	void 		*(*allocp)(size_t);
	void		(*freep)(void *, size_t);
} md_diff_impl_t;

/*
 * Internal utility functions
 */
static int mdd_scan_for_nodes(md_t *mdp, mde_cookie_t start,
    char *compnodep, int *countp, mde_cookie_t **nodespp);

static boolean_t mdd_any_dup_nodes(md_impl_t *mdp, md_prop_match_t *pmp,
    int count, mde_cookie_t *nodesp);

static int mdd_node_list_match(md_impl_t *md1, md_impl_t *md2,
    md_element_t *match_nodep, mde_cookie_t *match_listp,
    uint8_t *match_seenp, int start, int end, md_prop_match_t *match_elemsp);

static int mdd_node_compare(md_impl_t *mdap, md_impl_t *mdbp,
    md_element_t *nodeap, md_element_t *nodebp, md_prop_match_t *match_elemsp);

/*
 * Given two DAGs and information about how to uniquely identify
 * the nodes of interest, determine which nodes have been added
 * to the second MD, removed from the first MD, or exist in both
 * MDs. This information is recorded and can be accessed using the
 * opaque cookie returned to the caller.
 */
md_diff_cookie_t
md_diff_init(md_t *md1p, mde_cookie_t start1, md_t *md2p, mde_cookie_t start2,
    char *compnodep, md_prop_match_t *match_fieldsp)
{
	int		idx;
	md_impl_t	*md1 = (md_impl_t *)md1p;
	md_impl_t	*md2 = (md_impl_t *)md2p;
	mde_cookie_t	*md1nodesp = NULL;
	mde_cookie_t	*md2nodesp = NULL;
	int		md1count = 0;
	int		md2count = 0;
	uint8_t		*seenp = NULL;

	/* variables used to gather results */
	md_diff_impl_t	*diff_res;
	mde_cookie_t	*mde_add_scr;
	mde_cookie_t	*mde_rem_scr;
	mde_cookie_t	*mde_match1_scr;
	mde_cookie_t	*mde_match2_scr;
	int		nadd = 0;
	int		nrem = 0;
	int		nmatch = 0;

	/* sanity check params */
	if ((md1p == NULL) || (md2p == NULL))
		return (MD_INVAL_DIFF_COOKIE);

	if ((start1 == MDE_INVAL_ELEM_COOKIE) ||
	    (start2 == MDE_INVAL_ELEM_COOKIE))
		return (MD_INVAL_DIFF_COOKIE);

	if ((compnodep == NULL) || (match_fieldsp == NULL))
		return (MD_INVAL_DIFF_COOKIE);

	/*
	 * Prepare an array of the matching nodes from the first MD.
	 */
	if (mdd_scan_for_nodes(md1p,
	    start1, compnodep, &md1count, &md1nodesp) == -1)
		return (MD_INVAL_DIFF_COOKIE);

	/* sanity check that all nodes are unique */
	if (md1nodesp &&
	    mdd_any_dup_nodes(md1, match_fieldsp, md1count, md1nodesp)) {
		MDD_FREE_CHECK(md1, md1nodesp, sizeof (mde_cookie_t) *
		    md1count);
		return (MD_INVAL_DIFF_COOKIE);
	}


	/*
	 * Prepare an array of the matching nodes from the second MD.
	 */
	if (mdd_scan_for_nodes(md2p,
	    start2, compnodep, &md2count, &md2nodesp) == -1)
		return (MD_INVAL_DIFF_COOKIE);

	/* sanity check that all nodes are unique */
	if (md2nodesp &&
	    mdd_any_dup_nodes(md2, match_fieldsp, md2count, md2nodesp)) {
		MDD_FREE_CHECK(md1, md1nodesp, sizeof (mde_cookie_t) *
		    md1count);
		MDD_FREE_CHECK(md2, md2nodesp, sizeof (mde_cookie_t) *
		    md2count);
		return (MD_INVAL_DIFF_COOKIE);
	}

	/* setup our result structure */
	diff_res = md1->allocp(sizeof (md_diff_impl_t));
	bzero(diff_res, sizeof (md_diff_impl_t));
	diff_res->allocp = md1->allocp;
	diff_res->freep = md1->freep;
	diff_res->mdd_magic = MD_DIFF_MAGIC;

	/*
	 * Special cases for empty lists
	 */
	if ((md1count == 0) && (md2count != 0)) {
		/* all the nodes found were added */
		diff_res->added.mdep = md2nodesp;
		diff_res->added.nelem = md2count;
		return ((mde_cookie_t)diff_res);
	}

	if ((md1count != 0) && (md2count == 0)) {
		/* all the nodes found were removed */
		diff_res->removed.mdep = md1nodesp;
		diff_res->removed.nelem = md1count;
		return ((mde_cookie_t)diff_res);
	}

	if ((md1count == 0) && (md2count == 0))
		/* no nodes found */
		return ((mde_cookie_t)diff_res);

	/*
	 * Both lists have some elements. Allocate some scratch
	 * buffers to sort them into our three categories, added,
	 * removed, and matched pairs.
	 */
	mde_add_scr = diff_res->allocp(sizeof (mde_cookie_t) * md2count);
	mde_rem_scr = diff_res->allocp(sizeof (mde_cookie_t) * md1count);
	mde_match1_scr = diff_res->allocp(sizeof (mde_cookie_t) * md1count);
	mde_match2_scr = diff_res->allocp(sizeof (mde_cookie_t) * md2count);

	/* array of seen flags only needed for md2 */
	seenp = (uint8_t *)diff_res->allocp(sizeof (uint8_t) * md2count);
	bzero(seenp, sizeof (uint8_t) * md2count);

	/*
	 * Make a pass through the md1 node array. Make note of
	 * any nodes not in the md2 array, indicating that they
	 * have been removed. Also keep track of the nodes that
	 * are present in both arrays for the matched pair results.
	 */
	for (idx = 0; idx < md1count; idx++) {

		md_element_t *elem = &(md1->mdep[md1nodesp[idx]]);

		int match = mdd_node_list_match(md1, md2, elem, md2nodesp,
		    seenp, 0, md2count - 1, match_fieldsp);

		if (match == MD_DIFF_NOMATCH)
			/* record deleted node */
			mde_rem_scr[nrem++] = md1nodesp[idx];
		else {
			/* record matched node pair */
			mde_match1_scr[nmatch] = md1nodesp[idx];
			mde_match2_scr[nmatch] = md2nodesp[match];
			nmatch++;

			/* mark that this match has been recorded */
			seenp[match] = 1;
		}
	}

	/*
	 * Make a pass through the md2 array. Any nodes that have
	 * not been marked as seen have been added.
	 */
	for (idx = 0; idx < md2count; idx++) {
		if (!seenp[idx])
			/* record added node */
			mde_add_scr[nadd++] = md2nodesp[idx];
	}

	/* fill in the added node list */
	if (nadd) {
		int addsz = sizeof (mde_cookie_t) * nadd;
		diff_res->added.mdep = (mde_cookie_t *)diff_res->allocp(addsz);

		bcopy(mde_add_scr, diff_res->added.mdep, addsz);

		diff_res->added.nelem = nadd;
	}

	/* fill in the removed node list */
	if (nrem) {
		int remsz = sizeof (mde_cookie_t) * nrem;
		diff_res->removed.mdep =
		    (mde_cookie_t *)diff_res->allocp(remsz);

		bcopy(mde_rem_scr, diff_res->removed.mdep, remsz);
		diff_res->removed.nelem = nrem;
	}

	/* fill in the matching node lists */
	if (nmatch) {
		int matchsz = sizeof (mde_cookie_t) * nmatch;
		diff_res->match1.mdep =
		    (mde_cookie_t *)diff_res->allocp(matchsz);
		diff_res->match2.mdep =
		    (mde_cookie_t *)diff_res->allocp(matchsz);

		bcopy(mde_match1_scr, diff_res->match1.mdep, matchsz);
		bcopy(mde_match2_scr, diff_res->match2.mdep, matchsz);
		diff_res->match1.nelem = nmatch;
		diff_res->match2.nelem = nmatch;
	}

	/* clean up */
	md1->freep(md1nodesp, sizeof (mde_cookie_t) * md1count);
	md2->freep(md2nodesp, sizeof (mde_cookie_t) * md2count);

	diff_res->freep(mde_add_scr, sizeof (mde_cookie_t) * md2count);
	diff_res->freep(mde_rem_scr, sizeof (mde_cookie_t) * md1count);
	diff_res->freep(mde_match1_scr, sizeof (mde_cookie_t) * md1count);
	diff_res->freep(mde_match2_scr, sizeof (mde_cookie_t) * md2count);

	diff_res->freep(seenp, sizeof (uint8_t) * md2count);

	return ((md_diff_cookie_t)diff_res);
}

/*
 * Returns an array of the nodes added to the second MD in a
 * previous md_diff_init() call. Returns the number of elements
 * in the returned array. If the value is zero, the pointer
 * passed back will be NULL.
 */
int
md_diff_added(md_diff_cookie_t mdd, mde_cookie_t **mde_addedp)
{
	md_diff_impl_t	*mddp = (md_diff_impl_t *)mdd;

	if ((mddp == NULL) || (mddp->mdd_magic != MD_DIFF_MAGIC))
		return (-1);

	*mde_addedp = mddp->added.mdep;

	return (mddp->added.nelem);
}

/*
 * Returns an array of the nodes removed from the first MD in a
 * previous md_diff_init() call. Returns the number of elements
 * in the returned array. If the value is zero, the pointer
 * passed back will be NULL.
 */
int
md_diff_removed(md_diff_cookie_t mdd, mde_cookie_t **mde_removedp)
{
	md_diff_impl_t	*mddp = (md_diff_impl_t *)mdd;

	if ((mddp == NULL) || (mddp->mdd_magic != MD_DIFF_MAGIC))
		return (-1);

	*mde_removedp = mddp->removed.mdep;

	return (mddp->removed.nelem);
}

/*
 * Returns a pair of parallel arrays that contain nodes that were
 * considered matching based on the match criteria passed in to
 * a previous md_diff_init() call. Returns the number of elements
 * in the arrays. If the value is zero, both pointers passed back
 * will be NULL.
 */
int
md_diff_matched(md_diff_cookie_t mdd, mde_cookie_t **mde_match1p,
    mde_cookie_t **mde_match2p)
{
	md_diff_impl_t	*mddp = (md_diff_impl_t *)mdd;

	if ((mddp == NULL) || (mddp->mdd_magic != MD_DIFF_MAGIC))
		return (-1);

	*mde_match1p = mddp->match1.mdep;
	*mde_match2p = mddp->match2.mdep;

	return (mddp->match1.nelem);
}

/*
 * Deallocate any storage used to store results of a previous
 * md_diff_init() call. Returns 0 on success and -1 on failure.
 */
int
md_diff_fini(md_diff_cookie_t mdd)
{
	md_diff_impl_t	*mddp = (md_diff_impl_t *)mdd;

	if ((mddp == NULL) || (mddp->mdd_magic != MD_DIFF_MAGIC))
		return (-1);

	mddp->mdd_magic = 0;

	MDD_FREE_CHECK(mddp, mddp->added.mdep, mddp->added.nelem *
	    sizeof (mde_cookie_t));

	MDD_FREE_CHECK(mddp, mddp->removed.mdep, mddp->removed.nelem *
	    sizeof (mde_cookie_t));

	MDD_FREE_CHECK(mddp, mddp->match1.mdep, mddp->match1.nelem *
	    sizeof (mde_cookie_t));

	MDD_FREE_CHECK(mddp, mddp->match2.mdep, mddp->match2.nelem *
	    sizeof (mde_cookie_t));

	mddp->freep(mddp, sizeof (md_diff_impl_t));

	return (0);
}

/*
 * Walk the "fwd" DAG in an MD and return an array of nodes that are
 * of the specified type. The start param is used to start the walk
 * from an arbitrary location in the DAG. Returns an array of nodes
 * as well as a count of the number of nodes in the array.  If the
 * count is zero, the node pointer will be passed back as NULL.
 *
 * Returns: 0 success; -1 failure
 */
static int
mdd_scan_for_nodes(md_t *mdp,
    mde_cookie_t start, char *compnodep, int *countp, mde_cookie_t **nodespp)
{
	mde_str_cookie_t	cname;
	mde_str_cookie_t	aname;
	md_impl_t		*mdip = (md_impl_t *)mdp;

	if (mdip == NULL)
		return (-1);

	cname = md_find_name(mdp, compnodep);
	aname = md_find_name(mdp, "fwd");

	/* get the number of nodes of interest in the DAG */
	*countp = md_scan_dag(mdp, start, cname, aname, NULL);
	if (*countp == 0) {
		*nodespp = NULL;
		return (0);
	}

	/* allocate the storage */
	*nodespp = mdip->allocp(sizeof (mde_cookie_t) * (*countp));

	/* populate our array with the matching nodes */
	(void) md_scan_dag(mdp, start, cname, aname, *nodespp);

	return (0);
}

/*
 * Walk an array of nodes and check if there are any duplicate
 * nodes. A duplicate is determined based on the specified match
 * criteria. Returns B_TRUE if there are any duplicates and B_FALSE
 * otherwise.
 */
static boolean_t
mdd_any_dup_nodes(md_impl_t *mdp, md_prop_match_t *pmp, int count,
    mde_cookie_t *nodesp)
{
	int		idx;
	int		match;
	md_element_t	*elem;

	ASSERT(count > 0 || nodesp == NULL);

	for (idx = 0; idx < count; idx++) {
		elem = &(mdp->mdep[nodesp[idx]]);

		match = mdd_node_list_match(mdp, mdp, elem, nodesp, NULL,
		    idx + 1, count - 1, pmp);

		if (match != MD_DIFF_NOMATCH)
			return (B_TRUE);
	}

	return (B_FALSE);
}

/*
 * Given a node and a array of nodes, compare the node to all elements
 * in the specified start-end range of the array. If the node matches
 * one of the nodes in the array, return the index of that node. Otherwise
 * return MD_DIFF_NOMATCH.
 *
 * The optional seen array parameter can be used to optimize repeated
 * calls to this function. If the seen array indicates that an element
 * has already been matched, the full comparison is not necessary.
 */
static int
mdd_node_list_match(md_impl_t *md1, md_impl_t *md2, md_element_t *match_nodep,
    mde_cookie_t *match_listp, uint8_t *match_seenp, int start, int end,
    md_prop_match_t *match_elemsp)
{
	int		match;
	int		idx;
	md_element_t	*elem;

	for (idx = start; idx <= end; idx++) {

		if ((match_seenp != NULL) && (match_seenp[idx]))
			continue;

		elem = &(md2->mdep[match_listp[idx]]);

		match = mdd_node_compare(md1, md2, match_nodep, elem,
		    match_elemsp);
		if (match == MD_DIFF_MATCH)
			return (idx);
	}

	return (MD_DIFF_NOMATCH);
}

/*
 * Given two nodes and a list of properties, compare the nodes.
 * A match is concluded if both nodes have all of the specified
 * properties and all the values of those properties are the
 * same. Returns MD_DIFF_NOMATCH if the nodes do not match and
 * MD_DIFF_MATCH otherwise.
 */
static int
mdd_node_compare(md_impl_t *mdap, md_impl_t *mdbp, md_element_t *nodeap,
    md_element_t *nodebp, md_prop_match_t *match_elemsp)
{
	md_element_t	*ap;
	md_element_t	*bp;
	boolean_t	nodea_interest;
	boolean_t	nodeb_interest;
	int		idx;

	/* make sure we are starting at the beginning of the nodes */
	if ((MDE_TAG(nodeap) != MDET_NODE) || (MDE_TAG(nodebp) != MDET_NODE))
		return (MD_DIFF_NOMATCH);

	for (idx = 0; match_elemsp[idx].type != MDET_LIST_END; idx++) {

		int type;

		nodea_interest = B_FALSE;
		nodeb_interest = B_FALSE;

		type = match_elemsp[idx].type;

		/*
		 * Check node A for the property of interest
		 */
		for (ap = nodeap; MDE_TAG(ap) != MDET_NODE_END; ap++) {
			char *elemname;

			if (MDE_TAG(ap) != type)
				continue;

			elemname = mdap->namep + MDE_NAME(ap);

			if (strcmp(elemname, match_elemsp[idx].namep) == 0) {
				/* found the property of interest */
				nodea_interest = B_TRUE;
				break;
			}
		}

		/* node A is not of interest */
		if (!nodea_interest)
			return (MD_DIFF_NOMATCH);

		/*
		 * Check node B for the property of interest
		 */
		for (bp = nodebp; MDE_TAG(bp) != MDET_NODE_END; bp++) {
			char *elemname;

			if (MDE_TAG(bp) != type)
				continue;

			elemname = mdbp->namep + MDE_NAME(bp);

			if (strcmp(elemname, match_elemsp[idx].namep) == 0) {
				nodeb_interest = B_TRUE;
				break;
			}
		}

		/* node B is not of interest */
		if (!nodeb_interest)
			return (MD_DIFF_NOMATCH);

		/*
		 * Both nodes have the property of interest. The
		 * nodes are not a match unless the value of that
		 * property match
		 */
		switch (type) {
		case MDET_PROP_VAL:
			if (MDE_PROP_VALUE(ap) != MDE_PROP_VALUE(bp))
				return (MD_DIFF_NOMATCH);
			break;

		case MDET_PROP_STR: {
			char *stra = (char *)(mdap->datap +
			    MDE_PROP_DATA_OFFSET(ap));
			char *strb = (char *)(mdbp->datap +
			    MDE_PROP_DATA_OFFSET(bp));

			if (strcmp(stra, strb) != 0)
				return (MD_DIFF_NOMATCH);
			break;
		}

		case MDET_PROP_DAT: {

			caddr_t dataa;
			caddr_t datab;

			if (MDE_PROP_DATA_LEN(ap) != MDE_PROP_DATA_LEN(bp))
				return (MD_DIFF_NOMATCH);

			dataa = (caddr_t)(mdap->datap +
			    MDE_PROP_DATA_OFFSET(ap));
			datab = (caddr_t)(mdbp->datap +
			    MDE_PROP_DATA_OFFSET(bp));

			if (memcmp(dataa, datab, MDE_PROP_DATA_LEN(ap)) != 0)
				return (MD_DIFF_NOMATCH);

			break;
		}

		default:
			/* unsupported prop type */
			return (MD_DIFF_NOMATCH);
		}
	}

	/*
	 * All the specified properties exist in both
	 * nodes and have the same value. The two nodes
	 * match.
	 */

	return (MD_DIFF_MATCH);
}
