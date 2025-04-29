/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Robert Clausecker
 */

#include <sys/cdefs.h>

#include <string.h>

#undef strlcat	/* _FORTIFY_SOURCE */

void *__memchr_aarch64(const void *, int, size_t);
size_t __strlcpy(char *restrict, const char *restrict, size_t);

size_t
strlcat(char *restrict dst, const char *restrict src, size_t dstsize)
{
	char *loc = __memchr_aarch64(dst, '\0', dstsize);

	if (loc != NULL) {
		size_t dstlen = (size_t)(loc - dst);

		return (dstlen + __strlcpy(loc, src, dstsize - dstlen));
	} else
		return (dstsize + strlen(src));
}
