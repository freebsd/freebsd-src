/*
 * bsdstubs.h
 * Header for stub BSD function prototypes if unavailable on a specific platform.
 *
 * Copyright (c) 2012 William Pitcock <nenolod@dereferenced.org>.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * This software is provided 'as is' and without any warranty, express or
 * implied.  In no event shall the authors be liable for any damages arising
 * from the use of this software.
 */

#ifndef LIBPKGCONF_BSDSTUBS_H
#define LIBPKGCONF_BSDSTUBS_H

#include <libpkgconf/libpkgconf-api.h>

#ifdef __cplusplus
extern "C" {
#endif

PKGCONF_API extern size_t pkgconf_strlcpy(char *dst, const char *src, size_t siz);
PKGCONF_API extern size_t pkgconf_strlcat(char *dst, const char *src, size_t siz);
PKGCONF_API extern char *pkgconf_strndup(const char *src, size_t len);
PKGCONF_API extern void *pkgconf_reallocarray(void *ptr, size_t m, size_t n);

#ifdef __cplusplus
}
#endif

#endif
