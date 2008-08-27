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
 */
/*
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sgs.h>

/*
 * function that will find a prime'ish number.  Usefull for
 * hashbuckets and related things.
 */
uint_t
findprime(uint_t count)
{
	uint_t	h, f;

	if (count <= 3)
		return (3);


	/*
	 * Check to see if divisible by two, if so
	 * increment.
	 */
	if ((count & 0x1) == 0)
		count++;

	for (h = count, f = 2; f * f <= h; f++)
		if ((h % f) == 0)
			h += f = 1;
	return (h);
}
