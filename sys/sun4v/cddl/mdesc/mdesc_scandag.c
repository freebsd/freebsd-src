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
 * $FreeBSD: src/sys/sun4v/cddl/mdesc/mdesc_scandag.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/param.h>
#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <string.h>
#include <strings.h>
#endif

#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

static int
mdl_scan_dag(md_impl_t *mdp,
	int nodeidx,
	mde_str_cookie_t node_cookie,
	mde_str_cookie_t arc_cookie,
	uint8_t *dseenp,
	int *idxp,
	mde_cookie_t *stashp,
	int level);


int
md_scan_dag(md_t *ptr,
	mde_cookie_t startnode,
	mde_str_cookie_t node_name_cookie,
	mde_str_cookie_t arc_name_cookie,
	mde_cookie_t *stashp)
{
	int	res;
	int	idx;
	uint8_t *seenp;
	md_impl_t *mdp;
	int	start;

	mdp = (md_impl_t *)ptr;

	/*
	 * Possible the caller was lazy and didn't check the
	 * validitiy of either the node name or the arc name
	 * on calling ... in which case fail to find any
	 * nodes.
	 * This is distinct, from a fail (-1) since we return
	 * that nothing was found.
	 */

	if (node_name_cookie == MDE_INVAL_STR_COOKIE ||
		arc_name_cookie == MDE_INVAL_STR_COOKIE) return 0;

	/*
	 * if we want to start at the top, start at index 0
	 */

	start = (int)startnode;
	if (start == MDE_INVAL_ELEM_COOKIE) start = 0;

	/*
	 * Scan from the start point until the first node.
	 */
	while (MDE_TAG(&mdp->mdep[start]) == MDET_NULL) start++;

	/*
	 * This was a bogus start point if no node found
	 */
	if (MDE_TAG(&mdp->mdep[start]) != MDET_NODE) {
		return (-1);	/* illegal start node specified */
	}

	/*
	 * Allocate a recursion mask on the local stack fail
	 * if we can't allocate the recursion detection.
	 */
	seenp = (uint8_t *)mdp->allocp(mdp->element_count);
	if (seenp == NULL)
		return (-1);
	(void) memset(seenp, 0, mdp->element_count);

	/*
	 * Now build the list of requested nodes.
	 */
	idx = 0;
	res = mdl_scan_dag(mdp, start,
		node_name_cookie, arc_name_cookie,
		seenp, &idx, stashp, 0);

	mdp->freep(seenp, mdp->element_count);

	return (res >= 0 ? idx : res);
}





static int
mdl_scan_dag(md_impl_t *mdp,
	int nodeidx,
	mde_str_cookie_t node_name_cookie,
	mde_str_cookie_t arc_name_cookie,
	uint8_t *seenp,
	int *idxp,
	mde_cookie_t *stashp,
	int level)
{
	md_element_t *mdep;

	mdep = &(mdp->mdep[nodeidx]);

	/* see if cookie is infact a node */
	if (MDE_TAG(mdep) != MDET_NODE)
		return (-1);

	/* have we been here before ? */
	if (seenp[nodeidx])
		return (0);
	seenp[nodeidx] = 1;

	/* is this node of the type we seek ? */

#ifdef	DEBUG_LIBMDESC
	{
	int x;
	for (x = 0; x < level; x++)
		printf("-");
	printf("%d (%s)\n", nodeidx, (char *)(mdp->datap + MDE_NAME(mdep)));
	}
#endif

	if (MDE_NAME(mdep) == node_name_cookie) {
		/* record the node in the list and keep searching */
		if (stashp != NULL) {
			stashp[*idxp] = (mde_cookie_t)nodeidx;
		}
		(*idxp)++;
#ifdef	DEBUG_LIBMDESC
		printf("\t* %d\n", *idxp);
#endif
	}

	/*
	 * Simply walk the elements in the node.
	 * if we find a matching arc, then recursively call
	 * the subordinate looking for a match
	 */

	for (mdep++; MDE_TAG(mdep) != MDET_NODE_END; mdep++) {
		if (MDE_TAG(mdep) == MDET_PROP_ARC &&
			MDE_NAME(mdep) == arc_name_cookie) {
			int res;

			res = mdl_scan_dag(mdp,
			    (int)mdep->d.prop_idx,
			    node_name_cookie,
			    arc_name_cookie,
			    seenp, idxp, stashp, level+1);

			if (res == -1)
				return (res);
		}
	}

	return (0);
}
