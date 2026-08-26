/*
 * SPDX-License-Identifier: pkgconf
 *
 * Copyright (c) 2012 Ariadne Conill <ariadne@dereferenced.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#include <libpkgconf/stdinc.h>
#include <libpkgconf/bsdstubs.h>
#include <libpkgconf/config.h>

#if HAVE_DECL_STRNDUP
# define pkgconf_strndup_impl strndup
#else
/*
 * Creates a memory buffer and copies at most 'len' characters to it.
 * If 'len' is less than the length of the source string, truncation occured.
 */
static inline char *
pkgconf_strndup_impl(const char *src, size_t len)
{
	const char *end = memchr(src, '\0', len);
	size_t n = end != NULL ? (size_t)(end - src) : len;
	char *out = malloc(n + 1);

	if (out == NULL)
		return NULL;

	memcpy(out, src, n);
	out[n] = '\0';

	return out;
}
#endif

#if HAVE_DECL_PLEDGE
# define pkgconf_pledge_impl pledge
#else
static inline int
pkgconf_pledge_impl(const char *promises, const char *execpromises)
{
	(void) promises;
	(void) execpromises;

	return 0;
}
#endif

#if HAVE_DECL_UNVEIL
# define pkgconf_unveil_impl unveil
#else
static inline int
pkgconf_unveil_impl(const char *path, const char *permissions)
{
	(void) path;
	(void) permissions;

	return 0;
}
#endif

#if HAVE_DECL_REALLOCARRAY
# define pkgconf_reallocarray_impl reallocarray
#else
static inline void *
pkgconf_reallocarray_impl(void *ptr, size_t m, size_t n)
{
	if (n && m > -1 / n)
	{
		errno = ENOMEM;
		return 0;
	}

	return realloc(ptr, m * n);
}
#endif

char *
pkgconf_strndup(const char *src, size_t len)
{
	return pkgconf_strndup_impl(src, len);
}

void *
pkgconf_reallocarray(void *ptr, size_t m, size_t n)
{
	return pkgconf_reallocarray_impl(ptr, m, n);
}

int
pkgconf_pledge(const char *promises, const char *execpromises)
{
	return pkgconf_pledge_impl(promises, execpromises);
}

int
pkgconf_unveil(const char *path, const char *permissions)
{
	return pkgconf_unveil_impl(path, permissions);
}
