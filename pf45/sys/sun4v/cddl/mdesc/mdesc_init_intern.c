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

#include <sys/types.h>
#include <sys/param.h>
#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

md_t *
md_init_intern(uint64_t *ptr,  void *(*allocp)(size_t),
	    void (*freep)(void *, size_t))
{
	md_impl_t	*mdp;
	int		idx;
	int		count;
	int		done;
	uint64_t	gen;
	mde_str_cookie_t root_name;

	/*
	 * Very basic checkup for alignment to avoid
	 * bus error issues.
	 */
	if ((((uintptr_t)ptr) & 7) != 0)
		return (NULL);

	mdp = (md_impl_t *)allocp(sizeof (md_impl_t));

	if (mdp == NULL)
		return (NULL);

	mdp->allocp = allocp;
	mdp->freep = freep;

	mdp->caddr = (char *)ptr;

	/*
	 * setup internal structures
	 */

	mdp->headerp = (md_header_t *)mdp->caddr;

	if (mdtoh32(mdp->headerp->transport_version) != MD_TRANSPORT_VERSION) {
		goto cleanup_nohash;
	}

	mdp->node_blk_size = mdtoh32(mdp->headerp->node_blk_sz);
	mdp->name_blk_size = mdtoh32(mdp->headerp->name_blk_sz);
	mdp->data_blk_size = mdtoh32(mdp->headerp->data_blk_sz);

	mdp->size = MD_HEADER_SIZE + mdp->node_blk_size +
	    mdp->name_blk_size + mdp->data_blk_size;

	mdp->mdep = (md_element_t *)(mdp->caddr + MD_HEADER_SIZE);
	mdp->namep = (char *)(mdp->caddr + MD_HEADER_SIZE + mdp->node_blk_size);
	mdp->datap = (uint8_t *)(mdp->caddr + MD_HEADER_SIZE +
	    mdp->name_blk_size + mdp->node_blk_size);

	mdp->root_node = MDE_INVAL_ELEM_COOKIE;


	/*
	 * Should do a lot more sanity checking here.
	 */

	/*
	 * Should initialize a name hash here if we intend to use one
	 */

	/*
	 * Setup to find the root node
	 */
	root_name = md_find_name((md_t *)mdp, "root");
	if (root_name == MDE_INVAL_STR_COOKIE) {
		goto cleanup;
	}

	/*
	 * One more property we need is the count of nodes in the
	 * DAG, not just the number of elements.
	 *
	 * We try and pickup the root node along the way here.
	 */

	for (done = 0, idx = 0, count = 0; !done; ) {
		md_element_t *np;

		np = &(mdp->mdep[idx]);

		switch (MDE_TAG(np)) {
		case MDET_LIST_END:
			done = 1;
			break;

		case MDET_NODE:
			if (root_name == MDE_NAME(np)) {
				if (mdp->root_node != MDE_INVAL_ELEM_COOKIE) {
					/* Gah .. more than one root */
					goto cleanup;
				}
				mdp->root_node = (mde_cookie_t)idx;
			}
			idx = MDE_PROP_INDEX(np);
			count++;
			break;

		default:
			idx++;	/* ignore */
		}
	}

	/*
	 * Ensure there is a root node
	 */
	if (mdp->root_node == MDE_INVAL_ELEM_COOKIE) {
		goto cleanup;
	}

	/*
	 * Register the counts
	 */

	mdp->element_count = idx + 1;	/* include LIST_END */
	mdp->node_count = count;

	/*
	 * Final sanity check that everything adds up
	 */
	if (mdp->element_count != (mdp->node_blk_size / MD_ELEMENT_SIZE))
		goto cleanup;

	mdp->md_magic = LIBMD_MAGIC;

	/*
	 * Setup MD generation
	 */
	if (md_get_prop_val((md_t *)mdp, mdp->root_node,
	    "md-generation#", &gen) != 0)
		mdp->gen = MDESC_INVAL_GEN;
	else
		mdp->gen = gen;

	return ((md_t *)mdp);

cleanup:
	/*
	 * Clean up here - including a name hash if
	 * we build one.
	 */

cleanup_nohash:
	mdp->freep(mdp, sizeof (md_impl_t));
	return (NULL);
}
