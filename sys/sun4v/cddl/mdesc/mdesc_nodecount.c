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
 * $FreeBSD: src/sys/sun4v/cddl/mdesc/mdesc_nodecount.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */


#include <sys/types.h>
#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

int
md_node_count(md_t *ptr)
{
	md_impl_t *mdp;

	mdp = (md_impl_t *)ptr;

	if (mdp->md_magic != LIBMD_MAGIC)
		return (-1);

	return (mdp->node_count);
}
