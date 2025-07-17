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

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

int go(long, long, long, long, long, long, long, long, long, long);

int
go(long arg0 __unused, long arg1 __unused, long arg2 __unused,
   long arg3 __unused, long arg4 __unused, long arg5 __unused,
   long arg6 __unused, long arg7 __unused, long arg8 __unused,
   long arg9 __unused)
{
	return (arg1);
}

static void
handle(int sig __unused)
{
	go(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
	exit(0);
}

int
main(void)
{
	(void) signal(SIGUSR1, handle);
	for (;;)
		getpid();
}
