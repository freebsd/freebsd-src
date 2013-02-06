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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2012 by Delphix. All rights reserved.
 */

#pragma D option quiet

#pragma D option switchrate=1ms
#pragma D option aggrate=1ms

tick-50ms
{
	i++;
}

tick-50ms
/!(i & 1) && i > 1/
{
	setopt("quiet", "no");
	setopt("quiet");
	setopt("quiet");
	setopt("quiet", "yes");
	@["abc"] = count();
}

tick-50ms
/(i & 1) && i > 1/
{
	printa("%@d\n", @);
}

tick-50ms
/i == 10/
{
	setopt("switchrate", "5sec");
	setopt("aggrate", "5sec");
}

tick-50ms
/i == 61/
{
	exit(0);
}
