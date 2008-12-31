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
 * $FreeBSD: src/sys/sun4v/cddl/mdesc/mdesc_fini.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)mdesc_fini.c	1.4	06/05/16 SMI"

#include <sys/types.h>
#include <sys/mdesc.h>
#include <sys/mdesc_impl.h>

/*
 * Cleanup the internal MD structure. Does not
 * deallocate the buffer holding the MD.
 */
int
md_fini(md_t *ptr)
{
	md_impl_t *mdp;

	mdp = (md_impl_t *)ptr;

	mdp->freep(mdp, sizeof (md_impl_t));

	return (0);
}
