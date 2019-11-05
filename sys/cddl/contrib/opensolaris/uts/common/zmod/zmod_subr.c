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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>

/*ARGSUSED*/
void *
zcalloc(void *opaque, uint_t items, uint_t size)
{
	void *ptr;

	ptr = malloc((size_t)items * size, M_SOLARIS, M_NOWAIT);
	return ptr;
}

/*ARGSUSED*/
void
zcfree(void *opaque, void *ptr)
{
	free(ptr, M_SOLARIS);
}

void
zmemcpy(void *dest, const void *source, uint_t len)
{
	bcopy(source, dest, len);
}

int
zmemcmp(const void *s1, const void *s2, uint_t len)
{
	return (bcmp(s1, s2, len));
}

void
zmemzero(void *dest, uint_t len)
{
	bzero(dest, len);
}
