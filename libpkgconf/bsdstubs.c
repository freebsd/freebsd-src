/*
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

#if !HAVE_DECL_STRNDUP
/*
 * Creates a memory buffer and copies at most 'len' characters to it.
 * If 'len' is less than the length of the source string, truncation occured.
 */
static inline char *
strndup(const char *src, size_t len)
{
	char *out = malloc(len + 1);
	if (out == NULL)
		return NULL;

	memcpy(out, src, len);
	out[len] = '\0';

	return out;
}
#endif

#if !HAVE_DECL_PLEDGE
static inline int
pledge(const char *promises, const char *execpromises)
{
	(void) promises;
	(void) execpromises;

	return 0;
}
#endif

#if !HAVE_DECL_UNVEIL
static inline int
unveil(const char *path, const char *permissions)
{
	(void) path;
	(void) permissions;

	return 0;
}
#endif

char *
pkgconf_strndup(const char *src, size_t len)
{
	return strndup(src, len);
}

#if !HAVE_DECL_REALLOCARRAY
static inline void *
reallocarray(void *ptr, size_t m, size_t n)
{
	if (n && m > -1 / n)
	{
		errno = ENOMEM;
		return 0;
	}

	return realloc(ptr, m * n);
}
#endif

void *
pkgconf_reallocarray(void *ptr, size_t m, size_t n)
{
	return reallocarray(ptr, m, n);
}

int
pkgconf_pledge(const char *promises, const char *execpromises)
{
	return pledge(promises, execpromises);
}

int
pkgconf_unveil(const char *path, const char *permissions)
{
	return unveil(path, permissions);
}
