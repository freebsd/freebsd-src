/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Robert Clausecker
 */

#include <sys/cdefs.h>

#include <string.h>

void *__memccpy(void *restrict, const void *restrict, int, size_t);

char *
strncat(char *restrict dest, const char *restrict src, size_t n)
{
	size_t len;
	char *endptr;

	len = strlen(dest);
	endptr = __memccpy(dest + len, src, '\0', n);

	/* avoid an extra branch */
	if (endptr == NULL)
		endptr = dest + len + n + 1;

	endptr[-1] = '\0';

	return (dest);
}
