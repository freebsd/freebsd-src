/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>

static inline int
open_progdir(const char *prog)
{
	char pdir[PATH_MAX], *resolved;
	int dfd;

	resolved = realpath(prog, &pdir[0]);
	assert(resolved != NULL);

	resolved = dirname(&pdir[0]);
	assert(resolved != NULL);

	dfd = open(resolved, O_DIRECTORY);
	assert(dfd != -1);

	return (dfd);
}
