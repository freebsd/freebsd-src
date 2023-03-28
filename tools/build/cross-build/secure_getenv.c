/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Mark Johnston <markj@FreeBSD.org>
 */

#include <stdlib.h>
#include <unistd.h>

char *
secure_getenv(const char *name)
{
	if (issetugid() != 0)
		return (NULL);
	return (getenv(name));
}
