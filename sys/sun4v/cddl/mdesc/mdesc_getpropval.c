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
 * $FreeBSD: src/sys/sun4v/cddl/mdesc/mdesc_getpropval.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#include <sys/types.h>
#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <strings.h>
#endif

#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

int
md_get_prop_val(md_t *ptr, mde_cookie_t node, char *namep, uint64_t *valp)
{
	mde_str_cookie_t prop_name;
	md_impl_t	*mdp;
	mde_cookie_t	elem;

	mdp = (md_impl_t *)ptr;

	if (node == MDE_INVAL_ELEM_COOKIE) {
		return (-1);
	}

	prop_name = md_find_name(ptr, namep);
	if (prop_name == MDE_INVAL_STR_COOKIE) {
		return (-1);
	}

	elem = md_find_node_prop(mdp, node, prop_name, MDET_PROP_VAL);

	if (elem != MDE_INVAL_ELEM_COOKIE) {
		md_element_t *mdep;
		mdep = &(mdp->mdep[(int)elem]);

		*valp = MDE_PROP_VALUE(mdep);
		return (0);
	}

	return (-1);	/* no such property name */
}
