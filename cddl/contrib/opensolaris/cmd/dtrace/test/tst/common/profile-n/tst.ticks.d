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
 * Copyright 2025 Mark Johnston <markj@FreeBSD.org>
 */

/*
 * For 10s, verify that the value of `ticks goes up by `hz each second.
 */

#pragma D option quiet

BEGIN
{
	i = 0;
}

tick-1s
{
	if (i == 0) {
		t = *(int *)&`ticks;
		i++;
	} else {
		u = *(int *)&`ticks;
		if (u - t != `hz) {
			printf("ticks: %d, expected %d\n", u - t, `hz);
			exit(1);
		}
		t = u;
		i++;
		if (i == 10) {
			exit(0);
		}
	}
}
