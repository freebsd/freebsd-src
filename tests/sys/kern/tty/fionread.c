/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/ioctl.h>

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	int nb;

	assert(ioctl(STDIN_FILENO, FIONREAD, &nb) == 0);
	printf("%d", nb);
	return (0);
}
