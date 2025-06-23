/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Mark Johnston <markj@FreeBSD.org>
 */

#include <errno.h>
#include <stdlib.h>

static void __attribute__((constructor))
f(void)
{
	errno = 42;
}

int
main(void)
{
	/* errno must be zero upon program startup. */
	if (errno != 0)
		exit(1);
	exit(0);
}
