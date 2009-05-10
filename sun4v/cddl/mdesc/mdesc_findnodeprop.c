/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#include <sys/types.h>
#include <sys/param.h>
#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

mde_cookie_t
md_find_node_prop(md_impl_t *mdp,
	mde_cookie_t node,
	mde_str_cookie_t prop_name,
	int tag_type)
{
	md_element_t *mdep;
	int idx;

	if (node == MDE_INVAL_ELEM_COOKIE ||
	    prop_name == MDE_INVAL_STR_COOKIE) {
		return (MDE_INVAL_ELEM_COOKIE);
	}

	idx = (int)node;
	mdep = &(mdp->mdep[idx]);

		/* Skip over any empty elements */
	while (MDE_TAG(mdep) == MDET_NULL) {
		idx++;
		mdep++;
	}

		/* see if cookie is infact a node */
	if (MDE_TAG(mdep) != MDET_NODE) {
		return (MDE_INVAL_ELEM_COOKIE);
	}

	/*
	 * Simply walk the elements in the node
	 * looking for a property with a matching name.
	 */

	for (idx++, mdep++; MDE_TAG(mdep) != MDET_NODE_END; idx++, mdep++) {
		if (MDE_TAG(mdep) == tag_type) {
			if (MDE_NAME(mdep) == prop_name) {
				return ((mde_cookie_t)idx);
			}
		}
	}

	return (MDE_INVAL_ELEM_COOKIE);	/* no such property name */
}
