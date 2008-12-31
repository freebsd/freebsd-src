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
 * $FreeBSD: src/sys/sun4v/cddl/mdesc/mdesc_findname.c,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
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

mde_str_cookie_t
md_find_name(md_t *ptr, char *namep)
{
	int idx, len;
	md_impl_t *mdp;

	mdp = (md_impl_t *)ptr;

	/*
	 * At some point init should build a local hash table to
	 * speed these name searches ... for now use brute force
	 * because the machien descriptions are so small anyway.
	 */

	for (idx = 0; idx < mdp->name_blk_size; idx += len) {
		char *p;

		p = &(mdp->namep[idx]);

		len = strlen(p)+1;

		if (strcmp(p, namep) == 0)
			return ((mde_str_cookie_t)idx);
	}

	return (MDE_INVAL_STR_COOKIE);
}
